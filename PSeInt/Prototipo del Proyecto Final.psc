SubProceso MostrarMenu
	Escribir "=================================================="
	Escribir "    GESTION COMERCIAL CON ASISTENCIA NEKI"
	Escribir "=================================================="
	Escribir "1. Registrar productos"
	Escribir "2. Consultar y buscar productos"
	Escribir "3. Registrar una compra o ingreso de mercaderia"
	Escribir "4. Registrar una venta"
	Escribir "5. Registrar ingresos y gastos"
	Escribir "6. Consultar existencias de inventario"
	Escribir "7. Consultar historial de movimientos"
	Escribir "8. Mostrar resumen comercial"
	Escribir "9. Simular un comando de NEKI"
	Escribir "10. Finalizar el programa"
	Escribir "=================================================="
FinSubProceso

SubProceso MostrarExistencias(codigos, nombres, precios, cantDisponibles, totalProd)
	Definir i Como Entero
	Escribir "--- CONSULTA DE EXISTENCIAS DE INVENTARIO ---"
	Si totalProd == 0 Entonces
		Escribir "No hay productos registrados en el inventario."
	Sino
		Para i <- 1 Hasta totalProd Con Paso 1 Hacer
			Escribir "ID: ", codigos[i], " | Producto: ", nombres[i], " | Precio: $", precios[i], " | Cantidad disponible: ", cantDisponibles[i]
		FinPara
	FinSi
FinSubProceso

SubProceso MostrarResumenComercial(codigos, nombres, precios, cantDisponibles, totalProd, totalVentas, totalCompras, totalIngresosExtra, totalGastosExtra)
	Definir i Como Entero
	Escribir "--- RESUMEN COMERCIAL ---"
	Escribir "1. EXISTENCIAS ACTUALES EN INVENTARIO:"
	Si totalProd == 0 Entonces
		Escribir "   (Sin productos registrados)"
	Sino
		Para i <- 1 Hasta totalProd Con Paso 1 Hacer
			Escribir "   - [", codigos[i], "] ", nombres[i], ": ", cantDisponibles[i], " unidades (P.U. $", precios[i], ")"
		FinPara
	FinSi
	Escribir "--------------------------------------------------"
	Escribir "2. TOTALES REGISTRADOS:"
	Escribir "   - Total en Ventas: $", totalVentas
	Escribir "   - Total en Compras: $", totalCompras
	Escribir "   - Total Ingresos Extra: $", totalIngresosExtra
	Escribir "   - Total Gastos Extra: $", totalGastosExtra
	Escribir "   - Balance Neto Estimado: $", (totalVentas + totalIngresosExtra) - (totalCompras + totalGastosExtra)
FinSubProceso

Funcion pos <- BuscarProductoPorCodigo(codigos, totalProd, codBuscado)
	Definir i, pos Como Entero
	pos <- 0
	Para i <- 1 Hasta totalProd Con Paso 1 Hacer
		Si codigos[i] == codBuscado Entonces
			pos <- i
		FinSi
	FinPara
FinFuncion

Funcion total <- CalcularTotalVenta(cantidad, precioUnitario)
	Definir total Como Real
	total <- cantidad * precioUnitario
FinFuncion

Algoritmo GestionComercialNEKI
	
	Definir MAX Como Entero
	MAX <- 100
	
	Definir codigos, nombres Como Cadena
	Definir precios Como Real
	Definir cantDisponibles Como Entero
	Dimension codigos[MAX], nombres[MAX], precios[MAX], cantDisponibles[MAX]
	
	Definir matrizMovimientos Como Real
	Dimension matrizMovimientos[MAX, 3]
	Definir numMovimientos Como Entero
	numMovimientos <- 0
	
	Definir totalProd Como Entero
	totalProd <- 0
	
	Definir totalVentas, totalCompras, totalIngresosExtra, totalGastosExtra Como Real
	totalVentas <- 0
	totalCompras <- 0
	totalIngresosExtra <- 0
	totalGastosExtra <- 0
	
	Definir opcion, i, pos, cantSolicitada, tipoMov Como Entero
	Definir codBuscado, nomIngresado, confirmacion, comandoNEKI Como Cadena
	Definir precioIngresado, totalPagar, montoExtra Como Real
	
	Mientras opcion <> 10 Hacer
		MostrarMenu()
		Escribir "Seleccione una opcion (1-10): "
		Leer opcion
		
		Segun opcion Hacer
			
			1:
				Escribir "--- REGISTRAR PRODUCTO ---"
				Si totalProd >= MAX Entonces
					Escribir "Error: Capacidad maxima de catalogo alcanzada."
				Sino
					Escribir "Ingrese el identificador (codigo) del producto:"
					Leer codBuscado
					
					pos <- BuscarProductoPorCodigo(codigos, totalProd, codBuscado)
					Si pos > 0 Entonces
						Escribir "AVISO: El identificador ya existe. No se pueden duplicar codigos."
					FinSi
					
					Si pos == 0 Entonces
						totalProd <- totalProd + 1
						codigos[totalProd] <- codBuscado
						
						Escribir "Ingrese el nombre del producto:"
						Leer nombres[totalProd]
						
						Repetir
							Escribir "Ingrese el precio del producto (mayor a 0):"
							Leer precioIngresado
							Si precioIngresado <= 0 Entonces
								Escribir "Error: El precio debe ser un valor valido mayor a cero."
							FinSi
						Hasta Que precioIngresado > 0
						precios[totalProd] <- precioIngresado
						
						Repetir
							Escribir "Ingrese la cantidad disponible (0 o mayor):"
							Leer cantSolicitada
							Si cantSolicitada < 0 Entonces
								Escribir "Error: La cantidad no puede ser negativa."
							FinSi
						Hasta Que cantSolicitada >= 0
						cantDisponibles[totalProd] <- cantSolicitada
						
						Escribir "Producto registrado exitosamente."
					FinSi
				FinSi
				
			2:
				Escribir "--- CONSULTAR Y BUSCAR PRODUCTO ---"
				Escribir "Ingrese el codigo del producto a buscar:"
				Leer codBuscado
				
				pos <- BuscarProductoPorCodigo(codigos, totalProd, codBuscado)
				
				Si pos > 0 Entonces
					Para i <- 1 Hasta totalProd Con Paso 1 Hacer
						Si codigos[i] == codBuscado Entonces
							Escribir "Producto encontrado:"
							Escribir "ID: ", codigos[i]
							Escribir "Nombre: ", nombres[i]
							Escribir "Precio: $", precios[i]
							Escribir "Stock disponible: ", cantDisponibles[i]
						FinSi
					FinPara
				Sino
					Escribir "Producto no encontrado."
				FinSi
				
			3:
				Escribir "--- REGISTRAR COMPRA DE MERCADERIA ---"
				Escribir "Ingrese el codigo del producto a reabastecer:"
				Leer codBuscado
				pos <- BuscarProductoPorCodigo(codigos, totalProd, codBuscado)
				
				Si pos > 0 Entonces
					Repetir
						Escribir "Ingrese la cantidad comprada (mayor a 0):"
						Leer cantSolicitada
					Hasta Que cantSolicitada > 0
					
					Repetir
						Escribir "Ingrese el precio unitario de compra (mayor a 0):"
						Leer precioIngresado
					Hasta Que precioIngresado > 0
					
					cantDisponibles[pos] <- cantDisponibles[pos] + cantSolicitada
					totalPagar <- cantSolicitada * precioIngresado
					totalCompras <- totalCompras + totalPagar
					
					numMovimientos <- numMovimientos + 1
					matrizMovimientos[numMovimientos, 1] <- 1 
					matrizMovimientos[numMovimientos, 2] <- cantSolicitada
					matrizMovimientos[numMovimientos, 3] <- totalPagar
					
					Escribir "Compra registrada. Nuevo stock de ", nombres[pos], ": ", cantDisponibles[pos]
				Sino
					Escribir "El codigo de producto no existe en el catalogo."
				FinSi
				
			4:
				Escribir "--- REGISTRAR VENTA ---"
				Escribir "Ingrese el codigo del producto a vender:"
				Leer codBuscado
				pos <- BuscarProductoPorCodigo(codigos, totalProd, codBuscado)
				
				Si pos > 0 Entonces
					Escribir "Stock disponible: ", cantDisponibles[pos]
					Escribir "Ingrese la cantidad a vender:"
					Leer cantSolicitada
					
					Si cantSolicitada > cantDisponibles[pos] Entonces
						Escribir "VENTA RECHAZADA: La cantidad solicitada supera las existencias disponibles."
					FinSi
					
					Si cantSolicitada <= cantDisponibles[pos] Y cantSolicitada > 0 Entonces
						cantDisponibles[pos] <- cantDisponibles[pos] - cantSolicitada
						
						totalPagar <- CalcularTotalVenta(cantSolicitada, precios[pos])
						totalVentas <- totalVentas + totalPagar
						
						numMovimientos <- numMovimientos + 1
						matrizMovimientos[numMovimientos, 1] <- 2
						matrizMovimientos[numMovimientos, 2] <- cantSolicitada
						matrizMovimientos[numMovimientos, 3] <- totalPagar
						
						Escribir "Venta procesada con exito."
						Escribir "Total a pagar: $", totalPagar
						Escribir "Stock restante: ", cantDisponibles[pos]
					FinSi
				Sino
					Escribir "Producto no registrado."
				FinSi
				
			5:
				Escribir "--- REGISTRAR INGRESOS Y GASTOS ADICIONALES ---"
				Escribir "1. Registrar Ingreso Extra"
				Escribir "2. Registrar Gasto Extra"
				Leer tipoMov
				
				Si tipoMov == 1 O tipoMov == 2 Entonces
					Escribir "Ingrese el monto:"
					Leer montoExtra
					Si montoExtra > 0 Entonces
						Si tipoMov == 1 Entonces
							totalIngresosExtra <- totalIngresosExtra + montoExtra
							Escribir "Ingreso registrado correctamente."
						Sino
							totalGastosExtra <- totalGastosExtra + montoExtra
							Escribir "Gasto registrado correctamente."
						FinSi
					Sino
						Escribir "El monto debe ser positivo."
					FinSi
				Sino
					Escribir "Opcion no valida."
				FinSi
				
			6:
				MostrarExistencias(codigos, nombres, precios, cantDisponibles, totalProd)
				
			7:
				Escribir "--- HISTORIAL DE MOVIMIENTOS ---"
				Si numMovimientos == 0 Entonces
					Escribir "No hay movimientos registrados."
				Sino
					Para i <- 1 Hasta numMovimientos Con Paso 1 Hacer
						Si matrizMovimientos[i, 1] == 1 Entonces
							Escribir "Movimiento ", i, " [COMPRA] -> Cantidad: ", matrizMovimientos[i, 2], " | Total: $", matrizMovimientos[i, 3]
						Sino
							Escribir "Movimiento ", i, " [VENTA]  -> Cantidad: ", matrizMovimientos[i, 2], " | Total: $", matrizMovimientos[i, 3]
						FinSi
					FinPara
				FinSi
				
			8:
				MostrarResumenComercial(codigos, nombres, precios, cantDisponibles, totalProd, totalVentas, totalCompras, totalIngresosExtra, totalGastosExtra)
				
			9:
				Escribir "--- SIMULADOR DE ASISTENTE DE VOZ NEKI ---"
				Escribir "Comandos disponibles:"
				Escribir "a) resumen - Ver resumen comercial"
				Escribir "b) existencias - Consultar existencias"
				Escribir "Escriba o elija el comando a ejecutar:"
				Leer comandoNEKI
				
				Escribir "Desea confirmar la ejecucion del comando ", comandoNEKI, "? (SI/NO):"
				Leer confirmacion
				
				Si Minusculas(confirmacion) == "si" Entonces
					Escribir "NEKI: Ejecutando comando..."
					
					Si Minusculas(comandoNEKI) == "resumen" O Minusculas(comandoNEKI) == "a" Entonces
						MostrarResumenComercial(codigos, nombres, precios, cantDisponibles, totalProd, totalVentas, totalCompras, totalIngresosExtra, totalGastosExtra)
					Sino
						Si Minusculas(comandoNEKI) == "existencias" O Minusculas(comandoNEKI) == "b" Entonces
							MostrarExistencias(codigos, nombres, precios, cantDisponibles, totalProd)
						Sino
							Escribir "NEKI: Comando no reconocido."
						FinSi
					FinSi
				Sino
					Escribir "NEKI: Operacion cancelada por el usuario."
				FinSi
				
			10:
				Escribir "Finalizando el programa NEKI. Hasta pronto!"
				
			De Otro Modo:
				Escribir "Opcion invalida. Ingrese un numero del 1 al 10."
				
		FinSegun
		Escribir ""
	FinMientras
	
FinAlgoritmo