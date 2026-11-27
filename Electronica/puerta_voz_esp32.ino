/*
 * ============================================================================
 *  PUERTA AUTOMÁTICA CONTROLADA POR VOZ, PULSADOR Y ALARMA HC-SR04 (ESP32)
 * ============================================================================
 *  Placa: ESP32 DevKit (30 pines)
 *  Entorno: Arduino IDE (librerías estándar: WiFi.h, WebServer.h, LiquidCrystal.h)
 *
 *  Formas de apertura de la puerta:
 *   1) Pulsador físico en GPIO15 (abre la puerta sin WiFi ni app)
 *   2) Comando HTTP GET /abrir desde la app web (control por voz o botón manual)
 *
 *  Sensor HC-SR04: ya NO abre la puerta; ahora funciona como ALARMA.
 *   - Si detecta a alguien a <= 15 cm en estado REPOSO, suena el buzzer y
 *     parpadea el LED rojo ("ALARMA: PERSONA" en el LCD).
 *
 *  Máquina de estados no bloqueante con millis():
 *       REPOSO -> GIRO_1 -> ESPERA -> GIRO_2 -> REPOSO
 *  Microswitches definen los límites mecánicos en ambos giros.
 *  Seguridad: detención automática si un giro supera 15 segundos.
 *  Red local WiFi con reconexión automática y CORS habilitado.
 *  Protección por token en /abrir (parámetro ?token=... o cabecera).
 * ============================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <LiquidCrystal.h>

// ============================================================================
// CONFIGURACIÓN DE RED Y SEGURIDAD (Edita estos valores según tu red local)
// ============================================================================
const char* WIFI_SSID     = "Netlife-Ximena";
const char* WIFI_PASSWORD = "Carlita2017";
const char* API_TOKEN     = "neki123";  // Token requerido en GET /abrir

// ============================================================================
// DEFINICIÓN DE PINES DEL HARDWARE (Verificados sin conflictos en ESP32 30p)
// ============================================================================
//pulsador
// NOTA: GPIO15 es pin de strapping del ESP32 (boot) y puede causar lecturas
// erráticas. Se usa GPIO34 que es de solo entrada y libre de conflictos.
const int pulsadorManual = 34;
// Driver L298N (Motor DC)
const int PIN_ENA = 27;
const int PIN_IN1 = 25;
const int PIN_IN2 = 26;

// Finales de carrera (INPUT_PULLUP -> apretado = LOW)
const int PIN_SW1 = 33;  // Límite de apertura (GIRO_1)
const int PIN_SW2 = 32;  // Límite de cierre   (GIRO_2)

// Sensor ultrasónico HC-SR04
const int PIN_TRIG = 5;
const int PIN_ECHO = 18;

// Indicadores LED
const int PIN_LED_ROJO  = 17;
const int PIN_LED_VERDE = 16;

// Módulo Buzzer (activo en BAJO: LOW = suena, HIGH = silencio)
const int PIN_BUZZER = 23;

// Pantalla LCD 16x2 en modo 4 bits: RS, E, D4, D5, D6, D7
LiquidCrystal lcd(21, 22, 19, 13, 14, 4);

// ============================================================================
// PARÁMETROS DE FUNCIONAMIENTO
// ============================================================================
const float DISTANCIA_ALARMA_CM              = 15.0f;   // HC-SR04: distancia que activa la alarma (ya no abre la puerta)
const unsigned long TIEMPO_ESPERA_MS         = 3000;    // Pausa abierta (2 s)
const unsigned long TIEMPO_MAX_MOVIMIENTO_MS = 15000;   // Seguridad: máx 15 s por giro
const unsigned long INTERVALO_SENSOR_MS      = 150;     // Lectura HC-SR04 cada 150 ms
const unsigned long INTERVALO_RECONEXION_MS  = 10000;   // Reintento WiFi cada 10 s
const unsigned long COOLDOWN_POST_CICLO_MS   = 1500;    // Pausa breve tras cerrar o error
const uint8_t LECTURAS_ALARMA_REQ            = 3;       // Lecturas consecutivas requeridas para activar alarma
const unsigned long DURACION_ALARMA_MS       = 300;     // Duración del pitido de alarma del buzzer
const unsigned long COOLDOWN_ALARMA_MS       = 2000;    // Tiempo mínimo entre alarmas consecutivas
const unsigned long DEBOUNCE_PULSADOR_MS     = 50;      // Antirrebote del pulsador físico

// ============================================================================
// MÁQUINA DE ESTADOS Y VARIABLES GLOBALES
// ============================================================================
enum EstadoPuerta {
  REPOSO,
  GIRO_1,
  ESPERA,
  GIRO_2
};

enum OrigenApertura {
  ORIGEN_NINGUNO,
  ORIGEN_PULSADOR,  // Pulsador físico GPIO15
  ORIGEN_VOZ        // Comando HTTP /abrir (app web)
};

EstadoPuerta estadoActual     = REPOSO;
OrigenApertura origenActual   = ORIGEN_NINGUNO;

unsigned long tiempoInicioEstado   = 0;
unsigned long tiempoUltimaLectura  = 0;
unsigned long tiempoCooldownHasta  = 0;
unsigned long tiempoAlertaErrorFin = 0;

// Control no bloqueante del buzzer
bool buzzerActivo                  = false;
unsigned long tiempoBuzzerApagar   = 0;

// Control no bloqueante de reconexión WiFi y LCD
unsigned long tiempoUltimoIntentoWiFi = 0;
bool wifiEstabaConectado              = false;
unsigned long tiempoMostrarIpHasta       = 0;
unsigned long tiempoUltimoCambioLcd      = 0;
unsigned long tiempoUltimaActualizLcd    = 0;
bool mostrarIpEnReposo                   = false;
String ultimaLinea0                   = "";
String ultimaLinea1                   = "";

// Datos de telemetría y estado del pulsador
unsigned long contadorAperturas   = 0;    // Número de veces que se abrió la puerta
unsigned long contadorAlarmas     = 0;    // Número de alarmas del sensor activadas
float distanciaActualCm           = -1.0f;
uint8_t lecturasAlarma            = 0;    // Contador de lecturas consecutivas del sensor
unsigned long tiempoUltimaAlarma  = 0;   // Control del cooldown entre alarmas
bool alarmaActivaLcd              = false;
unsigned long tiempoAlarmaLcdFin  = 0;

// Estado del pulsador físico (antirrebote no bloqueante tipo Elliot Williams)
bool pulsadorEstadoEstable    = HIGH; // Estado confirmado (HIGH = suelto, LOW = presionado)
bool pulsadorEstadoLeido      = HIGH; // Última lectura cruda
unsigned long tiempoUltimoCambioPulsador = 0;

// Servidor HTTP en el puerto 80
WebServer server(80);

// ============================================================================
// FUNCIONES AUXILIARES DE TEXTO Y ESTADO
// ============================================================================
const char* nombreEstado(EstadoPuerta est) {
  switch (est) {
    case REPOSO: return "REPOSO";
    case GIRO_1: return "GIRO_1";
    case ESPERA: return "ESPERA";
    case GIRO_2: return "GIRO_2";
    default:     return "DESCONOCIDO";
  }
}

const char* nombreOrigen(OrigenApertura orig) {
  switch (orig) {
    case ORIGEN_PULSADOR: return "PULSADOR";
    case ORIGEN_VOZ:      return "VOZ";
    default:              return "NINGUNO";
  }
}

String formatearLineaLcd(const String& texto) {
  String s = texto.substring(0, 16);
  while (s.length() < 16) {
    s += ' ';
  }
  return s;
}

void escribirLcdSiCambia(const String& l0, const String& l1) {
  String f0 = formatearLineaLcd(l0);
  String f1 = formatearLineaLcd(l1);
  if (f0 != ultimaLinea0 || f1 != ultimaLinea1) {
    lcd.setCursor(0, 0);
    lcd.print(f0);
    lcd.setCursor(0, 1);
    lcd.print(f1);
    ultimaLinea0 = f0;
    ultimaLinea1 = f1;
  }
}

// ============================================================================
// CONTROL DE ACTUADORES (MOTOR, LEDS, BUZZER)
// ============================================================================
void motorAbrir() {
  digitalWrite(PIN_IN1, HIGH);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_ENA, HIGH);
}

void motorCerrar() {
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, HIGH);
  digitalWrite(PIN_ENA, HIGH);
}

void motorDetener() {
  digitalWrite(PIN_ENA, LOW);
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
}

void activarBuzzer(unsigned long duracionMs) {
  digitalWrite(PIN_BUZZER, LOW);  // Activo en BAJO
  buzzerActivo = true;
  tiempoBuzzerApagar = millis() + duracionMs;
}

void actualizarBuzzer() {
  if (buzzerActivo && (long)(millis() - tiempoBuzzerApagar) >= 0) {
    digitalWrite(PIN_BUZZER, HIGH);
    buzzerActivo = false;
  }
}

void actualizarLeds() {
  unsigned long ahora = millis();

  // Si hubo un error reciente de timeout, parpadea el LED rojo
  if ((long)(tiempoAlertaErrorFin - ahora) > 0) {
    bool parpadeo = ((ahora / 200) % 2) == 0;
    digitalWrite(PIN_LED_ROJO, parpadeo ? HIGH : LOW);
    digitalWrite(PIN_LED_VERDE, LOW);
    return;
  }

  switch (estadoActual) {
    case REPOSO:
      digitalWrite(PIN_LED_ROJO, HIGH);
      digitalWrite(PIN_LED_VERDE, LOW);
      break;
    case GIRO_1:
    case ESPERA:
      digitalWrite(PIN_LED_ROJO, LOW);
      digitalWrite(PIN_LED_VERDE, HIGH);
      break;
    case GIRO_2:
      // Rojo encendido mientras cierra la puerta
      digitalWrite(PIN_LED_ROJO, HIGH);
      digitalWrite(PIN_LED_VERDE, LOW);
      break;
  }
}

// ============================================================================
// LECTURA DEL SENSOR ULTRASÓNICO HC-SR04
// ============================================================================
float medirDistanciaCm() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  // Timeout de 25 ms (~4.3 m máx) para no bloquear el loop
  unsigned long duracion = pulseIn(PIN_ECHO, HIGH, 25000UL);
  if (duracion == 0) {
    return -1.0f;  // Sin eco / fuera de rango
  }
  return (duracion * 0.0343f) / 2.0f;
}

// ============================================================================
// LÓGICA COMPARTIDA DE INICIO DE CICLO (PULSADOR Y VOZ)
// ============================================================================
bool iniciarCicloPuerta(OrigenApertura origen) {
  if (estadoActual != REPOSO) {
    return false;
  }

  estadoActual = GIRO_1;
  origenActual = origen;
  tiempoInicioEstado = millis();
  tiempoAlertaErrorFin = 0;
  lecturasAlarma = 0;
  contadorAperturas++;

  if (origen == ORIGEN_VOZ) {
    // LED verde, pitido corto y mensaje en LCD
    digitalWrite(PIN_LED_ROJO, LOW);
    digitalWrite(PIN_LED_VERDE, HIGH);
    activarBuzzer(150);
    escribirLcdSiCambia("ABIERTO POR VOZ", "Abriendo puerta");
    Serial.println(F("[PUERTA] Ciclo iniciado por VOZ (HTTP /abrir)"));
  } else {
    // ORIGEN_PULSADOR
    digitalWrite(PIN_LED_ROJO, LOW);
    digitalWrite(PIN_LED_VERDE, HIGH);
    activarBuzzer(100);
    escribirLcdSiCambia("PULSADOR MANUAL", "Abriendo puerta");
    Serial.println(F("[PUERTA] Ciclo iniciado por PULSADOR FISICO (GPIO15)"));
  }

  motorAbrir();
  return true;
}

// ============================================================================
// ACTUALIZACIÓN NO BLOQUEANTE DE LA MÁQUINA DE ESTADOS
// ============================================================================
void actualizarMaquinaEstados() {
  unsigned long ahora = millis();

  // ---- 1. PULSADOR FÍSICO GPIO34 (antirrebote no bloqueante) ----
  // Patrón: guardamos el último instante en que la lectura cruda cambió.
  // Solo actualizamos el estado estable si la lectura permaneció igual >= DEBOUNCE ms.
  bool lecturaActual = digitalRead(pulsadorManual);
  if (lecturaActual != pulsadorEstadoLeido) {
    // La señal cambió: reiniciar el timer de antirrebote
    pulsadorEstadoLeido = lecturaActual;
    tiempoUltimoCambioPulsador = ahora;
  }
  if ((ahora - tiempoUltimoCambioPulsador) >= DEBOUNCE_PULSADOR_MS) {
    // La señal lleva DEBOUNCE ms estable → confirmar estado
    if (pulsadorEstadoLeido != pulsadorEstadoEstable) {
      pulsadorEstadoEstable = pulsadorEstadoLeido;
      // Flanco de bajada: suelto (HIGH) → presionado (LOW)
      if (pulsadorEstadoEstable == LOW) {
        Serial.println(F("[PULSADOR] GPIO34 presionado -> intentando abrir puerta"));
        if (estadoActual == REPOSO) {
          iniciarCicloPuerta(ORIGEN_PULSADOR);
        } else {
          Serial.print(F("[PULSADOR] Ignorado: puerta en estado "));
          Serial.println(nombreEstado(estadoActual));
        }
      }
    }
  }

  // ---- 2. ALARMA HC-SR04 (ya no abre la puerta) ----
  if (ahora - tiempoUltimaLectura >= INTERVALO_SENSOR_MS) {
    tiempoUltimaLectura = ahora;
    distanciaActualCm = medirDistanciaCm();

    // Solo evalúa alarma si la puerta está en REPOSO y el buzzer está libre
    if (estadoActual == REPOSO && !buzzerActivo) {
      if (distanciaActualCm > 0.5f && distanciaActualCm <= DISTANCIA_ALARMA_CM) {
        lecturasAlarma++;
        if (lecturasAlarma >= LECTURAS_ALARMA_REQ &&
            (ahora - tiempoUltimaAlarma) >= COOLDOWN_ALARMA_MS) {
          // Activar alarma de presencia (buzzer + LED rojo parpadeante + LCD)
          tiempoUltimaAlarma = ahora;
          contadorAlarmas++;
          lecturasAlarma = 0;
          activarBuzzer(DURACION_ALARMA_MS);
          alarmaActivaLcd = true;
          tiempoAlarmaLcdFin = ahora + 1200;
          Serial.print(F("[ALARMA] Persona detectada a "));
          Serial.print(distanciaActualCm, 1);
          Serial.println(F(" cm"));
        }
      } else {
        lecturasAlarma = 0;
      }
    } else {
      lecturasAlarma = 0;
    }
  }

  // ---- 3. Transiciones de la máquina de estados de la puerta ----
  switch (estadoActual) {
    case REPOSO:
      break;

    case GIRO_1:
      // Microswitch 1 presionado (INPUT_PULLUP -> LOW)
      if (digitalRead(PIN_SW1) == LOW) {
        motorDetener();
        estadoActual = ESPERA;
        tiempoInicioEstado = ahora;
        Serial.println(F("[PUERTA] SW1 activado -> ESPERA (3s)"));
      } else if (ahora - tiempoInicioEstado >= TIEMPO_MAX_MOVIMIENTO_MS) {
        // Protección por tiempo máximo de 15 s
        motorDetener();
        estadoActual = REPOSO;
        origenActual = ORIGEN_NINGUNO;
        tiempoCooldownHasta = ahora + 3000;
        tiempoAlertaErrorFin = ahora + 3000;
        activarBuzzer(400);
        escribirLcdSiCambia("ERROR: TIMEOUT", "Giro 1 detenido");
        Serial.println(F("[SEGURIDAD] Timeout en GIRO_1 (>15s). Motor detenido."));
      }
      break;

    case ESPERA:
      if (ahora - tiempoInicioEstado >= TIEMPO_ESPERA_MS) {
        estadoActual = GIRO_2;
        tiempoInicioEstado = ahora;
        motorCerrar();
        Serial.println(F("[PUERTA] Fin de ESPERA -> GIRO_2 (cerrando)"));
      }
      break;

    case GIRO_2:
      // Microswitch 2 presionado (INPUT_PULLUP -> LOW)
      if (digitalRead(PIN_SW2) == LOW) {
        motorDetener();
        estadoActual = REPOSO;
        origenActual = ORIGEN_NINGUNO;
        tiempoCooldownHasta = ahora + COOLDOWN_POST_CICLO_MS;
        Serial.println(F("[PUERTA] SW2 activado -> REPOSO"));
      } else if (ahora - tiempoInicioEstado >= TIEMPO_MAX_MOVIMIENTO_MS) {
        // Protección por tiempo máximo de 15 s
        motorDetener();
        estadoActual = REPOSO;
        origenActual = ORIGEN_NINGUNO;
        tiempoCooldownHasta = ahora + 3000;
        tiempoAlertaErrorFin = ahora + 3000;
        activarBuzzer(400);
        escribirLcdSiCambia("ERROR: TIMEOUT", "Giro 2 detenido");
        Serial.println(F("[SEGURIDAD] Timeout en GIRO_2 (>15s). Motor detenido."));
      }
      break;
  }
}

// ============================================================================
// ACTUALIZACIÓN DE LA PANTALLA LCD (NO BLOQUEANTE)
// ============================================================================
void actualizarLcd() {
  unsigned long ahora = millis();

  // Mantener visible el mensaje de error si ocurrió un timeout reciente
  if ((long)(tiempoAlertaErrorFin - ahora) > 0) {
    return;
  }

  // Mostrar la IP brevemente tras conectarse al WiFi (si está en reposo)
  if (estadoActual == REPOSO && (long)(tiempoMostrarIpHasta - ahora) > 0) {
    if (WiFi.status() == WL_CONNECTED) {
      escribirLcdSiCambia("WiFi Conectado", WiFi.localIP().toString());
      return;
    }
  }

  if (ahora - tiempoUltimaActualizLcd < 250) {
    return;
  }
  tiempoUltimaActualizLcd = ahora;

  if (estadoActual == REPOSO) {
    // Alterna cada 3 s la línea inferior entre distancia/contador e IP local
    if (ahora - tiempoUltimoCambioLcd >= 3000) {
      tiempoUltimoCambioLcd = ahora;
      mostrarIpEnReposo = !mostrarIpEnReposo;
    }

    String l0 = "REPOSO   Cnt:" + String(contadorAperturas);
    String l1;
    if (alarmaActivaLcd && (long)(tiempoAlarmaLcdFin - ahora) > 0) {
      // Mostrar "ALARMA: PERSONA" mientras está activa
      l1 = "ALARMA: PERSONA!";
    } else {
      alarmaActivaLcd = false;
      if (mostrarIpEnReposo) {
        l1 = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "Sin WiFi (Local)";
      } else {
        String distTxt = (distanciaActualCm > 0) ? String(distanciaActualCm, 0) + "cm" : "---";
        String wifiTxt = (WiFi.status() == WL_CONNECTED) ? "WiFi:OK" : "WiFi:--";
        l1 = "D:" + distTxt + " " + wifiTxt;
      }
    }
    escribirLcdSiCambia(l0, l1);
  } else {
    tiempoUltimoCambioLcd = ahora;
    String origen;
    if (origenActual == ORIGEN_VOZ)      origen = "ABIERTO POR VOZ";
    else if (origenActual == ORIGEN_PULSADOR) origen = "PULSADOR MANUAL";
    else                                 origen = "PUERTA ACTIVA";
    String l0 = origen;
    String l1;
    switch (estadoActual) {
      case GIRO_1: l1 = "Abriendo puerta"; break;
      case ESPERA: l1 = "Abierta (2s)..."; break;
      case GIRO_2: l1 = "Cerrando puerta"; break;
      default:     l1 = "Procesando...";   break;
    }
    escribirLcdSiCambia(l0, l1);
  }
}

// ============================================================================
// GESTIÓN DE WIFI CON RECONEXIÓN AUTOMÁTICA NO BLOQUEANTE
// ============================================================================
void conectarWiFiInicial() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print(F("[WiFi] Conectando a "));
  Serial.println(WIFI_SSID);
  escribirLcdSiCambia("Conectando WiFi", WIFI_SSID);

  // Espera máxima de 12 s al arrancar para no bloquear el modo local si no hay router
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - inicio < 12000)) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    wifiEstabaConectado = true;
    tiempoMostrarIpHasta = millis() + 4500;
    Serial.print(F("[WiFi] Conectado! IP del ESP32: "));
    Serial.println(WiFi.localIP());
    escribirLcdSiCambia("WiFi Conectado", WiFi.localIP().toString());
  } else {
    wifiEstabaConectado = false;
    Serial.println(F("[WiFi] No se pudo conectar aun. Seguire reintentando en segundo plano."));
    escribirLcdSiCambia("WiFi pendiente", "Modo sensor OK");
  }
}

void mantenerWiFi() {
  bool conectado = (WiFi.status() == WL_CONNECTED);

  if (conectado && !wifiEstabaConectado) {
    wifiEstabaConectado = true;
    tiempoMostrarIpHasta = millis() + 4500;
    Serial.print(F("[WiFi] Reconectado! IP del ESP32: "));
    Serial.println(WiFi.localIP());
  } else if (!conectado) {
    if (wifiEstabaConectado) {
      wifiEstabaConectado = false;
      Serial.println(F("[WiFi] Conexion perdida. Intentando reconectar..."));
    }

    unsigned long ahora = millis();
    if (ahora - tiempoUltimoIntentoWiFi >= INTERVALO_RECONEXION_MS) {
      tiempoUltimoIntentoWiFi = ahora;
      Serial.println(F("[WiFi] Reintentando conexion..."));
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }
}

// ============================================================================
// SERVIDOR HTTP Y ENDPOINTS (CORS + TOKEN)
// ============================================================================
void enviarCabecerasCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type, X-Auth-Token, Authorization");
  server.sendHeader("Access-Control-Allow-Private-Network", "true");
}

void handlePreflight() {
  enviarCabecerasCORS();
  server.send(204);
}

bool verificarToken() {
  // 1. Por parámetro en URL: /abrir?token=puerta1234
  if (server.hasArg("token") && server.arg("token") == API_TOKEN) {
    return true;
  }
  // 2. Por cabecera personalizada: X-Auth-Token: puerta1234
  if (server.hasHeader("X-Auth-Token") && server.header("X-Auth-Token") == API_TOKEN) {
    return true;
  }
  // 3. Por cabecera estándar: Authorization: Bearer puerta1234
  if (server.hasHeader("Authorization")) {
    String auth = server.header("Authorization");
    String esperado = String("Bearer ") + API_TOKEN;
    if (auth == esperado) {
      return true;
    }
  }
  return false;
}

// Interfaz web embebida en memoria Flash (PROGMEM) para abrir en http://IP_ESP32/
extern const char WEB_APP_HTML[];

void handleRaiz() {
  enviarCabecerasCORS();
  server.send_P(200, "text/html; charset=utf-8", WEB_APP_HTML);
}

void handleInfo() {
  enviarCabecerasCORS();
  String json = "{";
  json += "\"dispositivo\":\"ESP32 Puerta Automatica\",";
  json += "\"estado\":\"" + String(nombreEstado(estadoActual)) + "\",";
  json += "\"endpoints\":[\"/\",\"/info\",\"/estado\",\"/abrir?token=...\"]";
  json += "}";
  server.send(200, "application/json", json);
}

void handleEstado() {
  enviarCabecerasCORS();
  String distJson = (distanciaActualCm >= 0.0f) ? String(distanciaActualCm, 1) : "null";
  String ipActual = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "0.0.0.0";

  String json = "{";
  json += "\"estado\":\"" + String(nombreEstado(estadoActual)) + "\",";
  json += "\"origen\":\"" + String(nombreOrigen(origenActual)) + "\",";
  json += "\"contador\":" + String(contadorAperturas) + ",";
  json += "\"alarmas\":" + String(contadorAlarmas) + ",";
  json += "\"distancia\":" + distJson + ",";
  json += "\"ip\":\"" + ipActual + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

void handleAbrir() {
  enviarCabecerasCORS();

  if (!verificarToken()) {
    String jsonErr = "{";
    jsonErr += "\"ok\":false,";
    jsonErr += "\"error\":\"Token invalido o faltante\",";
    jsonErr += "\"estado\":\"" + String(nombreEstado(estadoActual)) + "\"";
    jsonErr += "}";
    server.send(401, "application/json", jsonErr);
    return;
  }

  if (estadoActual != REPOSO) {
    String jsonBusy = "{";
    jsonBusy += "\"ok\":false,";
    jsonBusy += "\"error\":\"La puerta ya esta en movimiento\",";
    jsonBusy += "\"estado\":\"" + String(nombreEstado(estadoActual)) + "\"";
    jsonBusy += "}";
    server.send(409, "application/json", jsonBusy);
    return;
  }

  iniciarCicloPuerta(ORIGEN_VOZ);

  String jsonOk = "{";
  jsonOk += "\"ok\":true,";
  jsonOk += "\"mensaje\":\"Puerta abriendo por comando de voz\",";
  jsonOk += "\"estado\":\"" + String(nombreEstado(estadoActual)) + "\",";
  jsonOk += "\"contador\":" + String(contadorAperturas);
  jsonOk += "}";
  server.send(200, "application/json", jsonOk);
}

void handleNotFound() {
  enviarCabecerasCORS();
  if (server.method() == HTTP_OPTIONS) {
    server.send(204);
    return;
  }
  server.send(404, "application/json", "{\"ok\":false,\"error\":\"Ruta no encontrada\"}");
}

void configurarServidor() {
  const char* headersRequeridos[] = {"X-Auth-Token", "Authorization"};
  server.collectHeaders(headersRequeridos, 2);

  server.on("/", HTTP_GET, handleRaiz);
  server.on("/info", HTTP_GET, handleInfo);
  server.on("/estado", HTTP_GET, handleEstado);
  server.on("/estado", HTTP_OPTIONS, handlePreflight);
  server.on("/abrir", HTTP_GET, handleAbrir);
  server.on("/abrir", HTTP_OPTIONS, handlePreflight);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println(F("[HTTP] Servidor iniciado en puerto 80"));
}

// ============================================================================
// SETUP Y LOOP PRINCIPAL
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("\n=== Iniciando Puerta Automatica ESP32 ==="));
// pulsador físico GPIO34 — solo entrada, sin función de strapping, sin conflictos
  // GPIO34 NO tiene pullup interno en el ESP32: conectar resistencia 10kΩ a 3.3V externamente,
  // o bien usar INPUT (sin PULLUP) con la resistencia externa.
  // Si tu pulsador ya va a GND con una resistencia a 3.3V en la PCB, usa INPUT.
  // Si quieres la resistencia interna del ESP32, usa un pin con pullup (p.ej. GPIO36 no tiene,
  // pero GPIO34 tampoco). Usamos la resistencia EXTERNA recomendada y declaramos INPUT.
  //
  // *** CONEXIÓN DEL PULSADOR ***
  // Un terminal del pulsador -> GPIO34
  // Otro terminal            -> GND
  // Resistencia 10kΩ         -> entre GPIO34 y 3.3V (pullup externo)
  // (Si ya tienes una resistencia de pullup en el circuito, solo conecta pulsador entre GPIO34 y GND)
  //
  // GPIO34 no soporta INPUT_PULLUP por hardware; si no tienes resistencia externa,
  // cambia el pin a GPIO35, GPIO36 o cualquier GPIO con pullup interno.
  pinMode(pulsadorManual, INPUT);  // GPIO34: solo lectura, sin pullup interno
  pulsadorEstadoEstable = HIGH;
  pulsadorEstadoLeido   = HIGH;
  // Configurar buzzer primero en HIGH para evitar pitido involuntario al encender
  digitalWrite(PIN_BUZZER, HIGH);
  pinMode(PIN_BUZZER, OUTPUT);

  // Configurar motor L298N detenido al inicio
  pinMode(PIN_ENA, OUTPUT);
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  motorDetener();

  // Configurar finales de carrera
  pinMode(PIN_SW1, INPUT_PULLUP);
  pinMode(PIN_SW2, INPUT_PULLUP);

  // Configurar sensor ultrasónico
  pinMode(PIN_TRIG, OUTPUT);
  digitalWrite(PIN_TRIG, LOW);
  pinMode(PIN_ECHO, INPUT);

  // Configurar LEDs
  pinMode(PIN_LED_ROJO, OUTPUT);
  pinMode(PIN_LED_VERDE, OUTPUT);
  actualizarLeds();

  // Inicializar LCD 16x2
  lcd.begin(16, 2);
  escribirLcdSiCambia("Iniciando...", "Puerta por Voz");

  // Conectar WiFi y arrancar servidor HTTP
  conectarWiFiInicial();
  configurarServidor();
}

void loop() {
  server.handleClient();
  mantenerWiFi();
  actualizarMaquinaEstados();
  actualizarBuzzer();
  actualizarLeds();
  actualizarLcd();
}

// ============================================================================
// INTERFAZ WEB EMBEBIDA (PROGMEM)
// ============================================================================
const char WEB_APP_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Control de Puerta por Voz · ESP32</title>
  <style>
:root {
  --bg: #0b0f17;
  --bg-elevated: #121826;
  --bg-input: #0e1320;
  --border: #232d42;
  --text: #f1f5f9;
  --text-muted: #94a3b8;
  --primary: #3b82f6;
  --primary-hover: #2563eb;
  --green: #22c55e;
  --green-soft: rgba(34, 197, 94, 0.14);
  --red: #ef4444;
  --red-soft: rgba(239, 68, 68, 0.14);
  --amber: #f59e0b;
  --amber-soft: rgba(245, 158, 11, 0.14);
  --radius: 16px;
  --shadow: 0 14px 35px rgba(0, 0, 0, 0.4);
}

*,
*::before,
*::after {
  box-sizing: border-box;
}

body {
  margin: 0;
  min-height: 100vh;
  font-family: system-ui, -apple-system, "Segoe UI", Roboto, Ubuntu, Cantarell, sans-serif;
  background: radial-gradient(circle at top right, #172036 0%, var(--bg) 55%);
  color: var(--text);
  line-height: 1.45;
}

.app-shell {
  max-width: 1040px;
  margin: 0 auto;
  padding: 20px 16px 40px;
  display: flex;
  flex-direction: column;
  gap: 18px;
}

/* Topbar */
.topbar {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
}

.brand {
  display: flex;
  align-items: center;
  gap: 12px;
}

.brand-icon {
  font-size: 2rem;
  width: 48px;
  height: 48px;
  display: grid;
  place-items: center;
  background: var(--bg-elevated);
  border: 1px solid var(--border);
  border-radius: 14px;
}

.brand h1 {
  margin: 0;
  font-size: 1.35rem;
  letter-spacing: -0.02em;
}

.subtitle {
  margin: 2px 0 0;
  font-size: 0.85rem;
  color: var(--text-muted);
}

.badge {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  padding: 8px 14px;
  border-radius: 999px;
  font-size: 0.88rem;
  font-weight: 600;
  border: 1px solid transparent;
}

.badge .dot {
  width: 10px;
  height: 10px;
  border-radius: 50%;
}

.badge-online {
  background: var(--green-soft);
  color: #86efac;
  border-color: rgba(34, 197, 94, 0.35);
}

.badge-online .dot {
  background: var(--green);
  box-shadow: 0 0 10px var(--green);
}

.badge-offline {
  background: var(--red-soft);
  color: #fca5a5;
  border-color: rgba(239, 68, 68, 0.35);
}

.badge-offline .dot {
  background: var(--red);
}

/* Alertas */
.alert {
  padding: 12px 16px;
  border-radius: 12px;
  font-size: 0.9rem;
  border: 1px solid transparent;
}

.alert-warning {
  background: var(--amber-soft);
  border-color: rgba(245, 158, 11, 0.35);
  color: #fde68a;
}

.hidden {
  display: none !important;
}

/* Cards */
.card {
  background: rgba(18, 24, 38, 0.9);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 18px;
  box-shadow: var(--shadow);
  backdrop-filter: blur(8px);
}

.card-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
  margin-bottom: 14px;
}

.card-header h2 {
  margin: 0;
  font-size: 1.05rem;
}

.hint {
  font-size: 0.8rem;
  color: var(--text-muted);
}

.pill {
  font-size: 0.78rem;
  padding: 4px 10px;
  border-radius: 999px;
  background: var(--bg-input);
  border: 1px solid var(--border);
  color: var(--text-muted);
}

/* Formulario de conexión */
.config-grid {
  display: grid;
  grid-template-columns: 1fr 1fr auto;
  gap: 12px;
  align-items: end;
}

.field {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.field > span {
  font-size: 0.82rem;
  color: var(--text-muted);
}

.field input {
  width: 100%;
  padding: 11px 13px;
  border-radius: 10px;
  border: 1px solid var(--border);
  background: var(--bg-input);
  color: var(--text);
  font-size: 0.95rem;
  outline: none;
  transition: border-color 0.2s;
}

.field input:focus {
  border-color: var(--primary);
}

.token-wrap {
  position: relative;
  display: flex;
  align-items: center;
}

.token-wrap input {
  padding-right: 42px;
}

.icon-btn {
  position: absolute;
  right: 6px;
  background: transparent;
  border: none;
  color: var(--text-muted);
  cursor: pointer;
  font-size: 1rem;
  padding: 6px;
  border-radius: 8px;
}

.icon-btn:hover {
  background: rgba(255, 255, 255, 0.06);
}

/* Botones */
.btn {
  border: none;
  border-radius: 10px;
  padding: 11px 18px;
  font-size: 0.95rem;
  font-weight: 600;
  cursor: pointer;
  transition: transform 0.1s, background-color 0.2s, opacity 0.2s;
}

.btn:active {
  transform: scale(0.98);
}

.btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.btn-primary {
  background: var(--primary);
  color: #fff;
}

.btn-primary:hover:not(:disabled) {
  background: var(--primary-hover);
}

.btn-secondary {
  background: #1e293b;
  color: var(--text);
  border: 1px solid var(--border);
}

.btn-secondary:hover:not(:disabled) {
  background: #283548;
}

/* Grid principal */
.main-grid {
  display: grid;
  grid-template-columns: 1.15fr 0.85fr;
  gap: 18px;
}

/* Micrófono */
.mic-stage {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: 18px 10px;
  gap: 14px;
}

.mic-button {
  width: 170px;
  height: 170px;
  border-radius: 50%;
  border: 2px solid rgba(59, 130, 246, 0.45);
  background: radial-gradient(circle at 30% 30%, #2563eb, #1e3a8a);
  color: #fff;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 10px;
  cursor: pointer;
  box-shadow: 0 0 0 0 rgba(59, 130, 246, 0.45), 0 12px 30px rgba(0, 0, 0, 0.45);
  transition: transform 0.2s, border-color 0.2s, background 0.2s;
}

.mic-button:hover:not(:disabled) {
  transform: translateY(-2px);
}

.mic-button:disabled {
  opacity: 0.45;
  cursor: not-allowed;
}

.mic-svg {
  width: 48px;
  height: 48px;
}

#mic-label {
  font-size: 0.9rem;
  font-weight: 600;
}

.mic-button.listening {
  border-color: rgba(34, 197, 94, 0.85);
  background: radial-gradient(circle at 30% 30%, #22c55e, #15803d);
  animation: pulse-ring 1.5s infinite;
}

@keyframes pulse-ring {
  0% {
    box-shadow: 0 0 0 0 rgba(34, 197, 94, 0.55);
  }
  70% {
    box-shadow: 0 0 0 22px rgba(34, 197, 94, 0);
  }
  100% {
    box-shadow: 0 0 0 0 rgba(34, 197, 94, 0);
  }
}

.voice-tip {
  margin: 0;
  font-size: 0.9rem;
  color: var(--text-muted);
  text-align: center;
}

.controls-row {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  margin-top: 8px;
  padding-top: 14px;
  border-top: 1px solid var(--border);
}

/* Toggle Escucha continua */
.switch-label {
  display: inline-flex;
  align-items: center;
  gap: 10px;
  cursor: pointer;
  font-size: 0.92rem;
  user-select: none;
}

.switch-label input {
  position: absolute;
  opacity: 0;
  width: 0;
  height: 0;
}

.switch-slider {
  width: 44px;
  height: 24px;
  border-radius: 999px;
  background: #273248;
  border: 1px solid var(--border);
  position: relative;
  transition: background-color 0.2s;
}

.switch-slider::after {
  content: "";
  position: absolute;
  top: 2px;
  left: 2px;
  width: 18px;
  height: 18px;
  border-radius: 50%;
  background: #cbd5e1;
  transition: transform 0.2s, background-color 0.2s;
}

.switch-label input:checked + .switch-slider {
  background: var(--green-soft);
  border-color: var(--green);
}

.switch-label input:checked + .switch-slider::after {
  transform: translateX(20px);
  background: var(--green);
}

/* Transcripción y feedback */
.transcript-box {
  margin-top: 14px;
  padding: 12px 14px;
  border-radius: 12px;
  background: var(--bg-input);
  border: 1px solid var(--border);
}

.box-label {
  display: block;
  font-size: 0.76rem;
  text-transform: uppercase;
  letter-spacing: 0.04em;
  color: var(--text-muted);
  margin-bottom: 4px;
}

.transcript-text {
  margin: 0;
  font-size: 1rem;
  min-height: 1.5em;
  word-break: break-word;
}

.transcript-text.muted {
  color: var(--text-muted);
  font-style: italic;
}

.feedback {
  margin-top: 12px;
  padding: 11px 14px;
  border-radius: 10px;
  font-size: 0.9rem;
  font-weight: 500;
}

.feedback.success {
  background: var(--green-soft);
  color: #86efac;
  border: 1px solid rgba(34, 197, 94, 0.35);
}

.feedback.error {
  background: var(--red-soft);
  color: #fca5a5;
  border: 1px solid rgba(239, 68, 68, 0.35);
}

.feedback.info {
  background: rgba(59, 130, 246, 0.14);
  color: #93c5fd;
  border: 1px solid rgba(59, 130, 246, 0.35);
}

/* Estado de puerta y métricas */
.door-state-banner {
  padding: 16px;
  border-radius: 12px;
  background: var(--bg-input);
  border: 1px solid var(--border);
  display: flex;
  flex-direction: column;
  gap: 4px;
  margin-bottom: 14px;
}

.door-state-banner.active-open {
  border-color: rgba(34, 197, 94, 0.5);
  background: rgba(34, 197, 94, 0.08);
}

.door-state-banner.active-close {
  border-color: rgba(245, 158, 11, 0.5);
  background: rgba(245, 158, 11, 0.08);
}

.state-caption {
  font-size: 0.78rem;
  color: var(--text-muted);
}

#door-state-value {
  font-size: 1.4rem;
  letter-spacing: 0.02em;
}

.origin-tag {
  font-size: 0.82rem;
  color: var(--text-muted);
}

.metrics-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 10px;
  margin-bottom: 16px;
}

.metric {
  padding: 12px;
  border-radius: 12px;
  background: var(--bg-input);
  border: 1px solid var(--border);
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.metric-label {
  font-size: 0.78rem;
  color: var(--text-muted);
}

.metric-value {
  font-size: 1.25rem;
}

/* Historial */
.history-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 8px;
}

.history-header h3 {
  margin: 0;
  font-size: 0.95rem;
}

.link-btn {
  background: none;
  border: none;
  color: var(--primary);
  cursor: pointer;
  font-size: 0.82rem;
  padding: 2px 6px;
}

.link-btn:hover {
  text-decoration: underline;
}

.history-list {
  list-style: none;
  margin: 0;
  padding: 0;
  display: flex;
  flex-direction: column;
  gap: 8px;
  max-height: 220px;
  overflow-y: auto;
}

.history-item {
  padding: 9px 12px;
  border-radius: 10px;
  background: var(--bg-input);
  border: 1px solid var(--border);
  font-size: 0.85rem;
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
  gap: 10px;
}

.history-item-main {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.history-item-time {
  font-size: 0.75rem;
  color: var(--text-muted);
  white-space: nowrap;
}

.history-ok {
  border-left: 3px solid var(--green);
}

.history-err {
  border-left: 3px solid var(--red);
}

.history-empty {
  color: var(--text-muted);
  font-size: 0.85rem;
  font-style: italic;
}

/* Responsive móvil */
@media (max-width: 780px) {
  .config-grid {
    grid-template-columns: 1fr;
  }

  .main-grid {
    grid-template-columns: 1fr;
  }

  .mic-button {
    width: 150px;
    height: 150px;
  }
}

</style>
</head>
<body>
  <div class="app-shell">
    <!-- Encabezado y estado de conexión -->
    <header class="topbar">
      <div class="brand">
        <span class="brand-icon" aria-hidden="true">🚪</span>
        <div>
          <h1>Puerta Inteligente</h1>
          <p class="subtitle">Control por voz local · ESP32</p>
        </div>
      </div>

      <div id="connection-badge" class="badge badge-offline" role="status" aria-live="polite">
        <span class="dot"></span>
        <span id="connection-text">Desconectado</span>
      </div>
    </header>

    <!-- Banner de advertencias de entorno (file://, https://, navegador sin soporte) -->
    <div id="env-warning" class="alert alert-warning hidden" role="alert"></div>

    <!-- Panel de conexión al ESP32 -->
    <section class="card" aria-labelledby="cfg-title">
      <div class="card-header">
        <h2 id="cfg-title">Conexión local al ESP32</h2>
        <span class="hint">Consulta /estado cada 2 s</span>
      </div>

      <form id="config-form" class="config-grid">
        <label class="field">
          <span>IP del ESP32</span>
          <input
            id="esp-ip"
            type="text"
            inputmode="url"
            placeholder="192.168.1.50"
            autocomplete="off"
            required
          />
        </label>

        <label class="field">
          <span>Token de seguridad</span>
          <div class="token-wrap">
            <input
              id="esp-token"
              type="password"
              placeholder="puerta1234"
              autocomplete="off"
              required
            />
            <button type="button" id="toggle-token" class="icon-btn" title="Mostrar u ocultar token" aria-label="Mostrar u ocultar token">
              👁️
            </button>
          </div>
        </label>

        <div class="field field-action">
          <button type="submit" id="connect-btn" class="btn btn-primary">
            Conectar
          </button>
        </div>
      </form>
    </section>

    <!-- Panel principal: Micrófono y estado de la puerta -->
    <main class="main-grid">
      <!-- Control por voz -->
      <section class="card voice-card" aria-labelledby="voice-title">
        <div class="card-header">
          <h2 id="voice-title">Comando de voz</h2>
          <span id="lang-badge" class="pill">Idioma: es-EC</span>
        </div>

        <div class="mic-stage">
          <button
            type="button"
            id="mic-btn"
            class="mic-button"
            aria-pressed="false"
            aria-label="Activar micrófono para decir abrir puerta"
          >
            <svg class="mic-svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
              <path d="M12 2a3 3 0 0 0-3 3v7a3 3 0 0 0 6 0V5a3 3 0 0 0-3-3Z"/>
              <path d="M19 10v2a7 7 0 0 1-14 0v-2"/>
              <line x1="12" x2="12" y1="19" y2="22"/>
            </svg>
            <span id="mic-label">Tocar para hablar</span>
          </button>

          <p class="voice-tip">
            Di <strong>"abrir puerta"</strong> o <strong>"abre la puerta"</strong>
          </p>
        </div>

        <!-- Escucha continua y botón manual -->
        <div class="controls-row">
          <label class="switch-label">
            <input type="checkbox" id="continuous-toggle" />
            <span class="switch-slider" aria-hidden="true"></span>
            <span>Escucha continua</span>
          </label>

          <button type="button" id="manual-open-btn" class="btn btn-secondary">
            Abrir puerta (manual)
          </button>
        </div>

        <!-- Texto reconocido en tiempo real -->
        <div class="transcript-box">
          <span class="box-label">Texto reconocido en tiempo real</span>
          <p id="live-transcript" class="transcript-text muted">
            Esperando comando de voz...
          </p>
        </div>

        <!-- Mensaje de resultado o error -->
        <div id="feedback-box" class="feedback hidden" role="status" aria-live="polite"></div>
      </section>

      <!-- Estado del hardware y telemetría -->
      <section class="card status-card" aria-labelledby="door-title">
        <div class="card-header">
          <h2 id="door-title">Estado de la puerta</h2>
          <span id="last-sync" class="hint">Sin datos</span>
        </div>

        <div class="door-state-banner" id="door-state-banner">
          <span class="state-caption">Estado actual</span>
          <strong id="door-state-value">DESCONECTADO</strong>
          <span id="door-origin-value" class="origin-tag">Origen: —</span>
        </div>

        <div class="metrics-grid">
          <div class="metric">
            <span class="metric-label">Contador de personas</span>
            <strong id="metric-counter" class="metric-value">0</strong>
          </div>
          <div class="metric">
            <span class="metric-label">Sensor HC-SR04</span>
            <strong id="metric-distance" class="metric-value">— cm</strong>
          </div>
        </div>

        <div class="history-section">
          <div class="history-header">
            <h3>Últimos comandos</h3>
            <button type="button" id="clear-history-btn" class="link-btn">Limpiar</button>
          </div>
          <ul id="command-history" class="history-list">
            <li class="history-empty">Aún no se han ejecutado comandos.</li>
          </ul>
        </div>
      </section>
    </main>
  </div>

  <script>
/* ============================================================================
 * App Web de Control por Voz para Puerta Automática ESP32
 * ============================================================================ */

const STORAGE_KEYS = {
  IP: "puerta_esp32_ip",
  TOKEN: "puerta_esp32_token",
  CONTINUOUS: "puerta_voz_continua",
  HISTORY: "puerta_historial_comandos"
};

const POLL_INTERVAL_MS = 2000;
const FETCH_TIMEOUT_MS = 1800;
const MAX_HISTORY_ITEMS = 8;
const COMMAND_COOLDOWN_MS = 2500;

// Elementos del DOM
const el = {
  envWarning: document.getElementById("env-warning"),
  connectionBadge: document.getElementById("connection-badge"),
  connectionText: document.getElementById("connection-text"),
  configForm: document.getElementById("config-form"),
  espIp: document.getElementById("esp-ip"),
  espToken: document.getElementById("esp-token"),
  toggleToken: document.getElementById("toggle-token"),
  micBtn: document.getElementById("mic-btn"),
  micLabel: document.getElementById("mic-label"),
  langBadge: document.getElementById("lang-badge"),
  continuousToggle: document.getElementById("continuous-toggle"),
  manualOpenBtn: document.getElementById("manual-open-btn"),
  liveTranscript: document.getElementById("live-transcript"),
  feedbackBox: document.getElementById("feedback-box"),
  lastSync: document.getElementById("last-sync"),
  doorStateBanner: document.getElementById("door-state-banner"),
  doorStateValue: document.getElementById("door-state-value"),
  doorOriginValue: document.getElementById("door-origin-value"),
  metricCounter: document.getElementById("metric-counter"),
  metricDistance: document.getElementById("metric-distance"),
  commandHistory: document.getElementById("command-history"),
  clearHistoryBtn: document.getElementById("clear-history-btn")
};

// Estado de la aplicación
const appState = {
  connected: false,
  pollTimer: null,
  recognition: null,
  listening: false,
  shouldRestartListening: false,
  retryWithFallbackLang: false,
  langIndex: 0,
  supportedLangs: ["es-EC", "es-ES"],
  lastCommandTimestamp: 0,
  requestInFlight: false,
  history: []
};

// ============================================================================
// UTILIDADES
// ============================================================================
function limpiarIp(raw) {
  return raw
    .trim()
    .replace(/^https?:\/\//i, "")
    .replace(/\/+$/, "");
}

function normalizarTexto(texto) {
  return texto
    .toLowerCase()
    .normalize("NFD")
    .replace(/[\u0300-\u036f]/g, "") // Quita tildes/diacríticos
    .replace(/[^a-z0-9\s]/g, " ")    // Quita signos de puntuación
    .replace(/\s+/g, " ")
    .trim();
}

function esComandoAbrirPuerta(textoOriginal) {
  const limpio = normalizarTexto(textoOriginal);
  // Acepta "abrir puerta", "abrir la puerta", "abre puerta", "abre la puerta", etc.
  return /\b(abrir|abre|abrete)\s+(la\s+)?puerta\b/.test(limpio);
}

async function fetchConTimeout(url, options = {}, timeoutMs = FETCH_TIMEOUT_MS) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    return await fetch(url, {
      ...options,
      signal: controller.signal,
      cache: "no-store"
    });
  } finally {
    clearTimeout(timer);
  }
}

function mostrarFeedback(mensaje, tipo = "info") {
  el.feedbackBox.textContent = mensaje;
  el.feedbackBox.className = `feedback ${tipo}`;
}

function ocultarFeedback() {
  el.feedbackBox.className = "feedback hidden";
}

function horaActualTexto() {
  return new Date().toLocaleTimeString("es-EC", {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit"
  });
}

// ============================================================================
// HISTORIAL DE COMANDOS
// ============================================================================
function cargarHistorial() {
  try {
    const guardado = localStorage.getItem(STORAGE_KEYS.HISTORY);
    appState.history = guardado ? JSON.parse(guardado) : [];
  } catch {
    appState.history = [];
  }
  renderizarHistorial();
}

function agregarHistorial(comando, resultado, ok) {
  appState.history.unshift({
    comando,
    resultado,
    ok,
    hora: horaActualTexto()
  });
  appState.history = appState.history.slice(0, MAX_HISTORY_ITEMS);
  try {
    localStorage.setItem(STORAGE_KEYS.HISTORY, JSON.stringify(appState.history));
  } catch {
    // Ignorar si el almacenamiento está lleno
  }
  renderizarHistorial();
}

function renderizarHistorial() {
  el.commandHistory.innerHTML = "";
  if (appState.history.length === 0) {
    const li = document.createElement("li");
    li.className = "history-empty";
    li.textContent = "Aún no se han ejecutado comandos.";
    el.commandHistory.appendChild(li);
    return;
  }

  for (const item of appState.history) {
    const li = document.createElement("li");
    li.className = `history-item ${item.ok ? "history-ok" : "history-err"}`;

    const main = document.createElement("div");
    main.className = "history-item-main";

    const cmd = document.createElement("strong");
    cmd.textContent = item.comando;

    const res = document.createElement("span");
    res.textContent = item.resultado;
    res.style.color = item.ok ? "#86efac" : "#fca5a5";

    main.appendChild(cmd);
    main.appendChild(res);

    const time = document.createElement("span");
    time.className = "history-item-time";
    time.textContent = item.hora;

    li.appendChild(main);
    li.appendChild(time);
    el.commandHistory.appendChild(li);
  }
}

// ============================================================================
// COMUNICACIÓN CON EL ESP32
// ============================================================================
function obtenerConfig() {
  const ip = limpiarIp(el.espIp.value);
  const token = el.espToken.value.trim();
  return { ip, token };
}

function actualizarEstadoConexionUI(conectado, mensaje = "") {
  appState.connected = conectado;
  if (conectado) {
    el.connectionBadge.className = "badge badge-online";
    el.connectionText.textContent = "Conectado";
  } else {
    el.connectionBadge.className = "badge badge-offline";
    el.connectionText.textContent = mensaje || "Desconectado";
  }
}

function actualizarTelemetriaUI(data) {
  const estado = data.estado || "DESCONOCIDO";
  el.doorStateValue.textContent = estado;
  el.doorOriginValue.textContent = `Origen: ${data.origen || "NINGUNO"}`;
  el.metricCounter.textContent = data.contador ?? 0;
  el.metricDistance.textContent =
    data.distancia !== null && data.distancia !== undefined
      ? `${data.distancia} cm`
      : "Fuera de rango";
  el.lastSync.textContent = `Actualizado ${horaActualTexto()}`;

  el.doorStateBanner.classList.remove("active-open", "active-close");
  if (estado === "GIRO_1" || estado === "ESPERA") {
    el.doorStateBanner.classList.add("active-open");
  } else if (estado === "GIRO_2") {
    el.doorStateBanner.classList.add("active-close");
  }
}

async function consultarEstado() {
  const { ip } = obtenerConfig();
  if (!ip) {
    actualizarEstadoConexionUI(false, "Ingresa la IP");
    return;
  }

  try {
    const resp = await fetchConTimeout(`http://${ip}/estado`);
    if (!resp.ok) {
      throw new Error(`HTTP ${resp.status}`);
    }
    const data = await resp.json();
    actualizarEstadoConexionUI(true);
    actualizarTelemetriaUI(data);
  } catch {
    actualizarEstadoConexionUI(false, "Sin conexión");
    el.doorStateValue.textContent = "SIN CONEXIÓN";
    el.doorStateBanner.classList.remove("active-open", "active-close");
  }
}

function iniciarPolling() {
  if (appState.pollTimer) {
    clearInterval(appState.pollTimer);
  }
  consultarEstado();
  appState.pollTimer = setInterval(consultarEstado, POLL_INTERVAL_MS);
}

async function enviarComandoAbrir(origenTexto = "Voz: \"abrir puerta\"") {
  const { ip, token } = obtenerConfig();

  if (!ip) {
    const msg = "Configura la IP del ESP32 antes de abrir la puerta.";
    mostrarFeedback(msg, "error");
    agregarHistorial(origenTexto, msg, false);
    return;
  }

  if (!token) {
    const msg = "Ingresa el token de seguridad del ESP32.";
    mostrarFeedback(msg, "error");
    agregarHistorial(origenTexto, msg, false);
    return;
  }

  if (appState.requestInFlight) {
    return;
  }

  appState.requestInFlight = true;
  el.manualOpenBtn.disabled = true;
  mostrarFeedback("Enviando orden de apertura al ESP32...", "info");

  try {
    const url = `http://${ip}/abrir?token=${encodeURIComponent(token)}`;
    const resp = await fetchConTimeout(url, { method: "GET" }, 3000);
    let data = {};
    try {
      data = await resp.json();
    } catch {
      data = {};
    }

    if (resp.ok && data.ok) {
      const msg = data.mensaje || "Puerta abriendo correctamente.";
      mostrarFeedback(msg, "success");
      agregarHistorial(origenTexto, msg, true);
      consultarEstado();
    } else if (resp.status === 409) {
      const msg = data.error || `La puerta ya está en movimiento (${data.estado || "ocupada"}).`;
      mostrarFeedback(msg, "error");
      agregarHistorial(origenTexto, msg, false);
    } else if (resp.status === 401) {
      const msg = data.error || "Token incorrecto. Verifica el token configurado.";
      mostrarFeedback(msg, "error");
      agregarHistorial(origenTexto, msg, false);
    } else {
      const msg = data.error || `Respuesta inesperada del ESP32 (HTTP ${resp.status}).`;
      mostrarFeedback(msg, "error");
      agregarHistorial(origenTexto, msg, false);
    }
  } catch (err) {
    const msg =
      err.name === "AbortError"
        ? "Tiempo agotado: el ESP32 no responde en la red local."
        : "No se pudo conectar con el ESP32. Revisa que esté encendido y en el mismo WiFi.";
    mostrarFeedback(msg, "error");
    agregarHistorial(origenTexto, msg, false);
    actualizarEstadoConexionUI(false, "Sin conexión");
  } finally {
    appState.requestInFlight = false;
    el.manualOpenBtn.disabled = false;
  }
}

// ============================================================================
// RECONOCIMIENTO DE VOZ (Web Speech API) Y DETECCIÓN DE ENTORNO
// ============================================================================
function esHostLocal() {
  const host = window.location.hostname;
  return host === "localhost" || host === "127.0.0.1" || host === "[::1]";
}

function esOrigenEsp32() {
  return window.location.protocol === "http:" && Boolean(window.location.hostname) && !esHostLocal();
}

async function verificarEntorno() {
  const avisos = [];
  const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;

  let esBrave = false;
  try {
    if (navigator.brave && (await navigator.brave.isBrave())) {
      esBrave = true;
    }
  } catch {
    // Ignorar
  }
  const ua = navigator.userAgent || "";
  const esOperaOVivaldi = /\b(OPR|Opera|Vivaldi)\b/i.test(ua);

  if (esBrave || esOperaOVivaldi) {
    // Deshabilitar micrófono en navegadores que bloquean Web Speech API
    el.micBtn.disabled = true;
    el.micLabel.textContent = "No compatible";
    el.continuousToggle.disabled = true;
    avisos.push(
      `Estás usando ${esBrave ? "Brave" : "Opera/Vivaldi"}. Este navegador bloquea el reconocimiento de voz (Web Speech API). Para usar el micrófono, abre esta página en Google Chrome o Microsoft Edge. El botón manual sigue funcionando.`
    );
  }

  if (window.location.protocol === "file:") {
    if (!esBrave && !esOperaOVivaldi) {
      el.micBtn.disabled = true;
      el.micLabel.textContent = "Requiere localhost";
      el.continuousToggle.disabled = true;
    }
    avisos.push(
      "Modo archivo (file://): el botón manual y la conexión con el ESP32 funcionan, pero Chrome/Edge bloquean el micrófono. Para usar voz, abre la app con 'iniciar_app.bat' (http://localhost:8080)."
    );
  } else if (window.location.protocol === "http:" && !esHostLocal()) {
    avisos.push(
      `Abriste la app desde http://${window.location.host}. El botón manual funciona en cualquier dispositivo. Para el micrófono, necesitas internet (Chrome envía el audio a Google) y agregar http://${window.location.host} en chrome://flags/#unsafely-treat-insecure-origin-as-secure.`
    );
  } else if (window.location.protocol === "https:") {
    avisos.push(
      "Estás en HTTPS: el micrófono funciona (necesita internet para Chrome). Si no conecta con el ESP32, habilita 'Contenido no seguro' en la configuración del sitio o usa 'iniciar_app.bat' (http://localhost)."
    );
  } else if (esHostLocal()) {
    // localhost es el entorno ideal, solo recordar que se necesita internet para voz
    avisos.push(
      "✅ Entorno correcto (localhost). El micrófono requiere internet (Chrome/Edge envían el audio a servidores de Google). Sin internet, usa el botón manual 'Abrir puerta'."
    );
  }

  if (!SpeechRecognition && !esBrave && !esOperaOVivaldi) {
    avisos.push(
      "Este navegador no es compatible con Web Speech API. Usa Google Chrome o Microsoft Edge para el control por voz."
    );
    el.micBtn.disabled = true;
    el.micLabel.textContent = "No compatible";
    el.continuousToggle.disabled = true;
  }

  if (avisos.length > 0) {
    el.envWarning.textContent = avisos.join(" | ");
    el.envWarning.classList.remove("hidden");
  }
}

function crearReconocimiento() {
  const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
  if (!SpeechRecognition) return null;

  const rec = new SpeechRecognition();
  rec.lang = appState.supportedLangs[appState.langIndex];
  rec.continuous = false; // Reiniciamos de forma controlada para mayor estabilidad en móvil y desktop
  rec.interimResults = true;
  rec.maxAlternatives = 3;

  el.langBadge.textContent = `Idioma: ${rec.lang}`;

  rec.onstart = () => {
    appState.listening = true;
    el.micBtn.classList.add("listening");
    el.micBtn.setAttribute("aria-pressed", "true");
    el.micLabel.textContent = "Escuchando...";
  };

  rec.onresult = (event) => {
    let textoCompleto = "";
    let esFinal = false;

    for (let i = event.resultIndex; i < event.results.length; i++) {
      const res = event.results[i];
      textoCompleto += res[0].transcript + " ";
      if (res.isFinal) {
        esFinal = true;
      }
    }

    const textoLimpio = textoCompleto.trim();
    if (textoLimpio) {
      el.liveTranscript.textContent = textoLimpio;
      el.liveTranscript.classList.remove("muted");
    }

    // Revisa todas las alternativas cuando hay resultado final o coincidencia clara
    let reconocido = esComandoAbrirPuerta(textoLimpio);
    if (!reconocido) {
      const ultimo = event.results[event.results.length - 1];
      for (let j = 0; j < ultimo.length; j++) {
        if (esComandoAbrirPuerta(ultimo[j].transcript)) {
          reconocido = true;
          break;
        }
      }
    }

    if (reconocido) {
      const ahora = Date.now();
      if (ahora - appState.lastCommandTimestamp >= COMMAND_COOLDOWN_MS) {
        appState.lastCommandTimestamp = ahora;
        enviarComandoAbrir(`Voz: "${textoLimpio}"`);
      }
      if (esFinal && !el.continuousToggle.checked) {
        detenerEscucha();
      }
    } else if (esFinal && textoLimpio) {
      mostrarFeedback(`Frase no reconocida como comando: "${textoLimpio}"`, "info");
    }
  };

  rec.onerror = (event) => {
    // 'no-speech' y 'aborted' ocurren normalmente cuando no hay sonido o se detiene
    if (event.error === "language-not-supported" && appState.langIndex === 0) {
      appState.langIndex = 1; // Cambiar a es-ES automáticamente
      appState.retryWithFallbackLang = true;
      el.langBadge.textContent = `Idioma: ${appState.supportedLangs[1]}`;
      mostrarFeedback("es-EC no disponible, cambiando automáticamente a es-ES.", "info");
      return;
    }

    if (event.error === "not-allowed" || event.error === "service-not-allowed") {
      appState.shouldRestartListening = false;
      el.continuousToggle.checked = false;
      localStorage.setItem(STORAGE_KEYS.CONTINUOUS, "false");
      mostrarFeedback(
        "Permiso de micrófono denegado. Habilita el micrófono en la barra de direcciones del navegador.",
        "error"
      );
      return;
    }

    if (event.error === "audio-capture") {
      appState.shouldRestartListening = false;
      mostrarFeedback("No se detectó ningún micrófono conectado.", "error");
      return;
    }

    if (event.error === "network") {
      appState.shouldRestartListening = false;
      const esBraveSync = Boolean(navigator.brave);
      const esOpera = /\b(OPR|Opera|Vivaldi)\b/i.test(navigator.userAgent || "");
      if (esBraveSync || esOpera) {
        mostrarFeedback(
          "Brave/Opera/Vivaldi bloquean la Web Speech API. Abre esta página en Google Chrome o Microsoft Edge.",
          "error"
        );
      } else {
        // Chrome/Edge requieren internet para enviar el audio a los servidores de Google.
        // Ofrecemos reintento y recordamos el botón manual como alternativa sin internet.
        mostrarFeedback(
          "⚠️ Error de red: Chrome necesita acceso a internet para el reconocimiento de voz (envía el audio a servidores de Google). Verifica tu conexión e inténtalo de nuevo. Sin internet, usa el botón 'Abrir puerta (manual)'.",
          "error"
        );
        // Habilitar botón de reintento explícito después de 2 s
        setTimeout(() => {
          if (!appState.listening) {
            el.micLabel.textContent = "Reintentar";
          }
        }, 2000);
      }
      return;
    }

    if (event.error !== "no-speech" && event.error !== "aborted") {
      mostrarFeedback(`Error de reconocimiento de voz: ${event.error}`, "error");
    }
  };

  rec.onend = () => {
    appState.listening = false;
    el.micBtn.classList.remove("listening");
    el.micBtn.setAttribute("aria-pressed", "false");
    el.micLabel.textContent = "Tocar para hablar";

    // Reintentar una vez con es-ES si es-EC no estaba disponible, o si el modo continuo sigue activo
    if (appState.retryWithFallbackLang) {
      appState.retryWithFallbackLang = false;
      setTimeout(() => {
        if (!appState.listening) {
          iniciarEscucha();
        }
      }, 200);
      return;
    }

    if (appState.shouldRestartListening && el.continuousToggle.checked) {
      setTimeout(() => {
        if (appState.shouldRestartListening && !appState.listening) {
          iniciarEscucha();
        }
      }, 280);
    }
  };

  return rec;
}

function iniciarEscucha() {
  if (appState.listening) return;

  appState.recognition = crearReconocimiento();
  if (!appState.recognition) return;

  appState.shouldRestartListening = el.continuousToggle.checked;
  ocultarFeedback();

  try {
    appState.recognition.start();
  } catch {
    // Evita excepción si start() se llama antes de terminar el ciclo anterior
  }
}

function detenerEscucha() {
  appState.shouldRestartListening = false;
  if (appState.recognition && appState.listening) {
    try {
      appState.recognition.stop();
    } catch {
      // Ignorar si ya estaba detenido
    }
  }
}

// ============================================================================
// INICIALIZACIÓN Y EVENTOS
// ============================================================================
function inicializar() {
  const ipPorDefecto = esOrigenEsp32() ? window.location.host : "192.168.1.50";
  const ipGuardada = esOrigenEsp32()
    ? window.location.host
    : localStorage.getItem(STORAGE_KEYS.IP) || ipPorDefecto;
  const tokenGuardado = localStorage.getItem(STORAGE_KEYS.TOKEN) || "puerta1234";
  const continuaGuardada = localStorage.getItem(STORAGE_KEYS.CONTINUOUS) === "true";

  el.espIp.value = ipGuardada;
  el.espToken.value = tokenGuardado;
  el.continuousToggle.checked = continuaGuardada;

  cargarHistorial();
  verificarEntorno();
  iniciarPolling();

  // Guardar IP/Token y reconectar
  el.configForm.addEventListener("submit", (e) => {
    e.preventDefault();
    const { ip, token } = obtenerConfig();
    el.espIp.value = ip;
    localStorage.setItem(STORAGE_KEYS.IP, ip);
    localStorage.setItem(STORAGE_KEYS.TOKEN, token);
    mostrarFeedback(`Conectando con ${ip}...`, "info");
    iniciarPolling();
  });

  // Mostrar/ocultar token
  el.toggleToken.addEventListener("click", () => {
    const esPass = el.espToken.type === "password";
    el.espToken.type = esPass ? "text" : "password";
  });

  // Botón grande del micrófono
  el.micBtn.addEventListener("click", () => {
    if (appState.listening) {
      detenerEscucha();
    } else {
      iniciarEscucha();
    }
  });

  // Modo escucha continua
  el.continuousToggle.addEventListener("change", () => {
    localStorage.setItem(STORAGE_KEYS.CONTINUOUS, String(el.continuousToggle.checked));
    if (el.continuousToggle.checked) {
      iniciarEscucha();
    } else {
      appState.shouldRestartListening = false;
    }
  });

  // Botón manual de respaldo
  el.manualOpenBtn.addEventListener("click", () => {
    enviarComandoAbrir("Botón manual");
  });

  // Limpiar historial
  el.clearHistoryBtn.addEventListener("click", () => {
    appState.history = [];
    localStorage.removeItem(STORAGE_KEYS.HISTORY);
    renderizarHistorial();
  });
}

document.addEventListener("DOMContentLoaded", inicializar);

</script>
</body>
</html>

)rawliteral";
