import java.util.Locale;
import java.util.Scanner;

public class GestionComercialNEKI {

    // Constante de capacidad maxima
    static final int MAX = 100;

    // SubProceso MostrarMenu -> metodo static void
    static void mostrarMenu() {
        System.out.println("==================================================");
        System.out.println("    GESTION COMERCIAL CON ASISTENCIA NEKI");
        System.out.println("==================================================");
        System.out.println("1. Registrar productos");
        System.out.println("2. Consultar y buscar productos");
        System.out.println("3. Registrar una compra o ingreso de mercaderia");
        System.out.println("4. Registrar una venta");
        System.out.println("5. Registrar ingresos y gastos");
        System.out.println("6. Consultar existencias de inventario");
        System.out.println("7. Consultar historial de movimientos");
        System.out.println("8. Mostrar resumen comercial");
        System.out.println("9. Simular un comando de NEKI");
        System.out.println("10. Finalizar el programa");
        System.out.println("==================================================");
    }

    // SubProceso MostrarExistencias -> metodo static void
    static void mostrarExistencias(String[] codigos, String[] nombres, double[] precios, int[] cantDisponibles, int totalProd) {
        System.out.println("--- CONSULTA DE EXISTENCIAS DE INVENTARIO ---");
        // Estructura 2: Condicional doble (if - else)
        if (totalProd == 0) {
            System.out.println("No hay productos registrados en el inventario.");
        } else {
            // Estructura 6: Ciclo Para (for) - indices en Java inician en 0
            for (int i = 0; i < totalProd; i++) {
                System.out.printf("ID: %s | Producto: %s | Precio: $%.2f | Cantidad disponible: %d%n",
                        codigos[i], nombres[i], precios[i], cantDisponibles[i]);
            }
        }
    }

    // SubProceso MostrarResumenComercial -> metodo static void
    static void mostrarResumenComercial(String[] codigos, String[] nombres, double[] precios, int[] cantDisponibles,
            int totalProd, double totalVentas, double totalCompras, double totalIngresosExtra, double totalGastosExtra) {
        System.out.println("--- RESUMEN COMERCIAL ---");
        System.out.println("1. EXISTENCIAS ACTUALES EN INVENTARIO:");
        if (totalProd == 0) {
            System.out.println("   (Sin productos registrados)");
        } else {
            for (int i = 0; i < totalProd; i++) {
                System.out.printf("   - [%s] %s: %d unidades (P.U. $%.2f)%n",
                        codigos[i], nombres[i], cantDisponibles[i], precios[i]);
            }
        }
        System.out.println("--------------------------------------------------");
        double balanceNeto = (totalVentas + totalIngresosExtra) - (totalCompras + totalGastosExtra);
        System.out.println("2. TOTALES REGISTRADOS:");
        System.out.printf("   - Total en Ventas: $%.2f%n", totalVentas);
        System.out.printf("   - Total en Compras: $%.2f%n", totalCompras);
        System.out.printf("   - Total Ingresos Extra: $%.2f%n", totalIngresosExtra);
        System.out.printf("   - Total Gastos Extra: $%.2f%n", totalGastosExtra);
        System.out.printf("   - Balance Neto Estimado: $%.2f%n", balanceNeto);
    }

    // Funcion BuscarProductoPorCodigo -> metodo static con retorno int
    // Nota: Devuelve el indice (0 a totalProd-1) si lo encuentra, o -1 si no existe
    static int buscarProductoPorCodigo(String[] codigos, int totalProd, String codBuscado) {
        int pos = -1;
        for (int i = 0; i < totalProd; i++) {
            // Estructura 1: Condicional simple (if) + comparacion de cadenas con equalsIgnoreCase
            if (codigos[i].equalsIgnoreCase(codBuscado)) {
                pos = i;
            }
        }
        return pos;
    }

    // Funcion CalcularTotalVenta -> metodo static con retorno double
    static double calcularTotalVenta(int cantidad, double precioUnitario) {
        double total = cantidad * precioUnitario;
        return total;
    }

    public static void main(String[] args) {
        Scanner sc = new Scanner(System.in);
        sc.useLocale(Locale.US);

        // Arreglos unidimensionales paralelos para el catalogo
        String[] codigos = new String[MAX];
        String[] nombres = new String[MAX];
        double[] precios = new double[MAX];
        int[] cantDisponibles = new int[MAX];

        // Matriz bidimensional de movimientos: [fila][0]=Tipo (1=Compra, 2=Venta), [fila][1]=Cantidad, [fila][2]=Total
        double[][] matrizMovimientos = new double[MAX][3];
        int numMovimientos = 0;
        int totalProd = 0;

        // Acumuladores financieros
        double totalVentas = 0.0;
        double totalCompras = 0.0;
        double totalIngresosExtra = 0.0;
        double totalGastosExtra = 0.0;

        int opcion = 0, pos, cantSolicitada, tipoMov;
        String codBuscado, confirmacion, comandoNEKI;
        double precioIngresado, totalPagar, montoExtra;

        // Estructura 4: Ciclo Mientras (while) para el menu principal
        while (opcion != 10) {
            mostrarMenu();
            System.out.print("Seleccione una opcion (1-10): ");
            opcion = sc.nextInt();
            sc.nextLine(); // Limpiar salto de linea

            // Estructura 3: Condicional multiple (switch)
            switch (opcion) {
                case 1:
                    System.out.println("--- REGISTRAR PRODUCTO ---");
                    if (totalProd >= MAX) {
                        System.out.println("Error: Capacidad maxima de catalogo alcanzada.");
                    } else {
                        System.out.print("Ingrese el identificador (codigo) del producto: ");
                        codBuscado = sc.nextLine();

                        pos = buscarProductoPorCodigo(codigos, totalProd, codBuscado);
                        if (pos >= 0) {
                            System.out.println("AVISO: El identificador ya existe. No se pueden duplicar codigos.");
                        }

                        if (pos == -1) {
                            codigos[totalProd] = codBuscado;

                            System.out.print("Ingrese el nombre del producto: ");
                            nombres[totalProd] = sc.nextLine();

                            // Estructura 5: Ciclo Repetir...Hasta Que (do-while) para validacion
                            do {
                                System.out.print("Ingrese el precio del producto (mayor a 0): ");
                                precioIngresado = sc.nextDouble();
                                if (precioIngresado <= 0) {
                                    System.out.println("Error: El precio debe ser un valor valido mayor a cero.");
                                }
                            } while (precioIngresado <= 0);
                            precios[totalProd] = precioIngresado;

                            do {
                                System.out.print("Ingrese la cantidad disponible (0 o mayor): ");
                                cantSolicitada = sc.nextInt();
                                if (cantSolicitada < 0) {
                                    System.out.println("Error: La cantidad no puede ser negativa.");
                                }
                            } while (cantSolicitada < 0);
                            sc.nextLine(); // Limpiar buffer
                            cantDisponibles[totalProd] = cantSolicitada;

                            totalProd++;
                            System.out.println("Producto registrado exitosamente.");
                        }
                    }
                    break;

                case 2:
                    System.out.println("--- CONSULTAR Y BUSCAR PRODUCTO ---");
                    System.out.print("Ingrese el codigo del producto a buscar: ");
                    codBuscado = sc.nextLine();

                    pos = buscarProductoPorCodigo(codigos, totalProd, codBuscado);

                    if (pos >= 0) {
                        for (int i = 0; i < totalProd; i++) {
                            if (codigos[i].equalsIgnoreCase(codBuscado)) {
                                System.out.println("Producto encontrado:");
                                System.out.println("ID: " + codigos[i]);
                                System.out.println("Nombre: " + nombres[i]);
                                System.out.printf("Precio: $%.2f%n", precios[i]);
                                System.out.println("Stock disponible: " + cantDisponibles[i]);
                            }
                        }
                    } else {
                        System.out.println("Producto no encontrado.");
                    }
                    break;

                case 3:
                    System.out.println("--- REGISTRAR COMPRA DE MERCADERIA ---");
                    System.out.print("Ingrese el codigo del producto a reabastecer: ");
                    codBuscado = sc.nextLine();
                    pos = buscarProductoPorCodigo(codigos, totalProd, codBuscado);

                    if (pos >= 0) {
                        do {
                            System.out.print("Ingrese la cantidad comprada (mayor a 0): ");
                            cantSolicitada = sc.nextInt();
                        } while (cantSolicitada <= 0);

                        do {
                            System.out.print("Ingrese el precio unitario de compra (mayor a 0): ");
                            precioIngresado = sc.nextDouble();
                        } while (precioIngresado <= 0);
                        sc.nextLine();

                        cantDisponibles[pos] = cantDisponibles[pos] + cantSolicitada;
                        totalPagar = cantSolicitada * precioIngresado;
                        totalCompras = totalCompras + totalPagar;

                        if (numMovimientos < MAX) {
                            matrizMovimientos[numMovimientos][0] = 1; // 1 = Compra
                            matrizMovimientos[numMovimientos][1] = cantSolicitada;
                            matrizMovimientos[numMovimientos][2] = totalPagar;
                            numMovimientos++;
                        }

                        System.out.println("Compra registrada. Nuevo stock de " + nombres[pos] + ": " + cantDisponibles[pos]);
                    } else {
                        System.out.println("El codigo de producto no existe en el catalogo.");
                    }
                    break;

                case 4:
                    System.out.println("--- REGISTRAR VENTA ---");
                    System.out.print("Ingrese el codigo del producto a vender: ");
                    codBuscado = sc.nextLine();
                    pos = buscarProductoPorCodigo(codigos, totalProd, codBuscado);

                    if (pos >= 0) {
                        System.out.println("Stock disponible: " + cantDisponibles[pos]);
                        System.out.print("Ingrese la cantidad a vender: ");
                        cantSolicitada = sc.nextInt();
                        sc.nextLine();

                        if (cantSolicitada > cantDisponibles[pos]) {
                            System.out.println("VENTA RECHAZADA: La cantidad solicitada supera las existencias disponibles.");
                        }

                        if (cantSolicitada <= cantDisponibles[pos] && cantSolicitada > 0) {
                            cantDisponibles[pos] = cantDisponibles[pos] - cantSolicitada;

                            totalPagar = calcularTotalVenta(cantSolicitada, precios[pos]);
                            totalVentas = totalVentas + totalPagar;

                            if (numMovimientos < MAX) {
                                matrizMovimientos[numMovimientos][0] = 2; // 2 = Venta
                                matrizMovimientos[numMovimientos][1] = cantSolicitada;
                                matrizMovimientos[numMovimientos][2] = totalPagar;
                                numMovimientos++;
                            }

                            System.out.println("Venta procesada con exito.");
                            System.out.printf("Total a pagar: $%.2f%n", totalPagar);
                            System.out.println("Stock restante: " + cantDisponibles[pos]);
                        } else if (cantSolicitada <= 0) {
                            System.out.println("Error: La cantidad a vender debe ser mayor a 0.");
                        }
                    } else {
                        System.out.println("Producto no registrado.");
                    }
                    break;

                case 5:
                    System.out.println("--- REGISTRAR INGRESOS Y GASTOS ADICIONALES ---");
                    System.out.println("1. Registrar Ingreso Extra");
                    System.out.println("2. Registrar Gasto Extra");
                    System.out.print("Seleccione tipo de movimiento: ");
                    tipoMov = sc.nextInt();

                    if (tipoMov == 1 || tipoMov == 2) {
                        System.out.print("Ingrese el monto: ");
                        montoExtra = sc.nextDouble();
                        if (montoExtra > 0) {
                            if (tipoMov == 1) {
                                totalIngresosExtra = totalIngresosExtra + montoExtra;
                                System.out.println("Ingreso registrado correctamente.");
                            } else {
                                totalGastosExtra = totalGastosExtra + montoExtra;
                                System.out.println("Gasto registrado correctamente.");
                            }
                        } else {
                            System.out.println("El monto debe ser positivo.");
                        }
                    } else {
                        System.out.println("Opcion no valida.");
                    }
                    sc.nextLine();
                    break;

                case 6:
                    mostrarExistencias(codigos, nombres, precios, cantDisponibles, totalProd);
                    break;

                case 7:
                    System.out.println("--- HISTORIAL DE MOVIMIENTOS ---");
                    if (numMovimientos == 0) {
                        System.out.println("No hay movimientos registrados.");
                    } else {
                        for (int i = 0; i < numMovimientos; i++) {
                            if (matrizMovimientos[i][0] == 1) {
                                System.out.printf("Movimiento %d [COMPRA] -> Cantidad: %.0f | Total: $%.2f%n",
                                        (i + 1), matrizMovimientos[i][1], matrizMovimientos[i][2]);
                            } else {
                                System.out.printf("Movimiento %d [VENTA]  -> Cantidad: %.0f | Total: $%.2f%n",
                                        (i + 1), matrizMovimientos[i][1], matrizMovimientos[i][2]);
                            }
                        }
                    }
                    break;

                case 8:
                    mostrarResumenComercial(codigos, nombres, precios, cantDisponibles, totalProd,
                            totalVentas, totalCompras, totalIngresosExtra, totalGastosExtra);
                    break;

                case 9:
                    System.out.println("--- SIMULADOR DE ASISTENTE DE VOZ NEKI ---");
                    System.out.println("Comandos disponibles:");
                    System.out.println("a) resumen - Ver resumen comercial");
                    System.out.println("b) existencias - Consultar existencias");
                    System.out.print("Escriba o elija el comando a ejecutar: ");
                    comandoNEKI = sc.nextLine();

                    System.out.print("Desea confirmar la ejecucion del comando '" + comandoNEKI + "'? (SI/NO): ");
                    confirmacion = sc.nextLine();

                    if (confirmacion.equalsIgnoreCase("si")) {
                        System.out.println("NEKI: Ejecutando comando...");

                        if (comandoNEKI.equalsIgnoreCase("resumen") || comandoNEKI.equalsIgnoreCase("a")) {
                            mostrarResumenComercial(codigos, nombres, precios, cantDisponibles, totalProd,
                                    totalVentas, totalCompras, totalIngresosExtra, totalGastosExtra);
                        } else {
                            if (comandoNEKI.equalsIgnoreCase("existencias") || comandoNEKI.equalsIgnoreCase("b")) {
                                mostrarExistencias(codigos, nombres, precios, cantDisponibles, totalProd);
                            } else {
                                System.out.println("NEKI: Comando no reconocido.");
                            }
                        }
                    } else {
                        System.out.println("NEKI: Operacion cancelada por el usuario.");
                    }
                    break;

                case 10:
                    System.out.println("Finalizando el programa NEKI. Hasta pronto!");
                    break;

                default:
                    System.out.println("Opcion invalida. Ingrese un numero del 1 al 10.");
                    break;
            }
            System.out.println();
        }
        sc.close();
    }
}