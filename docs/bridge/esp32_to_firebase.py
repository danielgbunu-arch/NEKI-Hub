"""Puente de telemetría REAL ESP32 (USB) -> Firebase Realtime Database.

No genera datos de prueba ni sustituye las lecturas del prototipo.
Ejecutar en la laptop que tiene conectado el ESP32 por cable USB.

INSTALAR:
  pip install pyserial firebase-admin
EJECUTAR (ejemplo Windows):
  python esp32_to_firebase.py --port COM5 --database-url https://TU-ID-default-rtdb.firebaseio.com --credentials serviceAccountKey.json

ADVERTENCIA: otro programa no puede abrir simultáneamente el mismo puerto serie.
Si hay una aplicación de voz usando el puerto USB, integrar este publicador
Firebase a la misma aplicación en lugar de ejecutar dos lectores.
"""
import argparse
import json
import time


def main():
    parser = argparse.ArgumentParser(description='Publica datos auténticos del ESP32')
    parser.add_argument('--port', required=True, help='COM5 / /dev/ttyUSB0 / etc.')
    parser.add_argument('--baud', type=int, default=115200)
    parser.add_argument('--database-url', required=True)
    parser.add_argument('--credentials', required=True, help='Archivo privado de cuenta de servicio de Firebase')
    args = parser.parse_args()

    import serial
    import firebase_admin
    from firebase_admin import credentials, db

    cred = credentials.Certificate(args.credentials)
    firebase_admin.initialize_app(cred, {'databaseURL': args.database_url.rstrip('/')})
    live = db.reference('/neki/telemetry')
    history = db.reference('/neki/history')

    print('Conectando al ESP32 por', args.port, '/', args.baud)
    print('Solo se publican líneas JSON realmente recibidas por el puerto serie.')
    with serial.Serial(args.port, args.baud, timeout=1) as device:
        while True:
            raw = device.readline().decode('utf-8', errors='replace').strip()
            if not raw:
                continue
            try:
                payload = json.loads(raw)
            except json.JSONDecodeError:
                continue
            if not isinstance(payload, dict):
                continue
            allowed = {k: payload[k] for k in ('presence', 'door', 'entries', 'event', 'led', 'buzzer') if k in payload}
            if not allowed:
                continue
            # Timestamp de recepción de un mensaje REAL, no de una medición fabricada.
            allowed['received_at_ms'] = int(time.time() * 1000)
            live.set(allowed)
            if isinstance(allowed.get('event'), str) and allowed['event'].strip():
                history.push({'event': allowed['event'], 'received_at_ms': allowed['received_at_ms']})
            print('Lectura real publicada:', json.dumps(allowed, ensure_ascii=False))


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print('\nLectura finalizada.')
