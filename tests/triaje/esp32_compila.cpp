#include <Arduino.h>
#include <Triaje.h>

// Fixture reducido: el backend real consulta sus indices persistidos.
static conecta::FuenteMemoria<1, 2> indices;
static conecta::MotorTriaje motor;
static conecta::ResultadoIngreso resultado;

void setup() {
    conecta::ReporteEntrante reporte;
    reporte.idReporte = "COMPILACION-FICTICIA";
    reporte.categoria = "atrapadas";
    reporte.nivel = conecta::Nivel::CRITICO;
    reporte.personas = -1;
    reporte.requiereVerificacion = true;
    // Sin ubicacion: alerta propia y por verificar, nunca se reclasifica.
    resultado = motor.evaluar(reporte, indices);
}

void loop() {
    // Compilar no prueba hardware, persistencia, transporte ni recepcion.
}
