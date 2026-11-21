#include <Arduino.h>
#include <Servidor.h> // Cabecera REAL, sin reemplazarla ni registrar rutas.
#include <Triaje.h>
#include <cstring>

ServidorHotspot servidor; // Misma construccion que src/main.cpp.
static conecta::MotorTriaje motor;
static conecta::FuenteMemoria<2, 8> indicesDeDemostracion;
static StaticJsonDocument<16384> cuerpo;

// Datos confiables resueltos por el adaptador backend, no por IP/MAC/cliente.
struct ContextoIngresoEjemplo {
    std::uint64_t recibidoSegundos;
    const char* fechaRecepcionUtc;
    bool ubicacionValidadaContraCatalogo;
    const char* contenidoCanonico;
    const char* patronOperativo;
    const char* incidenteConfirmado;
    const char* evidenciaAsociacion;
};

// Ejemplo de llamada tras parsear; NO handler HTTP ni persistencia implementada.
// El caller conserva json hasta guardar atomicamente el original completo.
conecta::ResultadoIngreso procesarReporteEjemplo(const char* json, size_t n,
                                               const ContextoIngresoEjemplo& contexto) {
    conecta::ResultadoIngreso pendiente;
    if (!json || n > 16384) {
        pendiente.banderas = conecta::DATOS_INVALIDOS | conecta::CAPACIDAD_AGOTADA;
        pendiente.revisionAntifraude = true;
        return pendiente;
    }
    cuerpo.clear();
    const auto error = deserializeJson(cuerpo, json, n);
    if (error || !cuerpo.is<JsonObject>()) {
        pendiente.banderas = conecta::DATOS_INVALIDOS;
        if (error == DeserializationError::NoMemory) pendiente.banderas |= conecta::CAPACIDAD_AGOTADA;
        pendiente.revisionAntifraude = true;
        return pendiente;
    }
    conecta::ReporteEntrante r;
    r.idReporte = cuerpo["idReporte"] | static_cast<const char*>(nullptr);
    // idReporte es la clave estable del contrato; no inventar un campo raiz.
    r.idempotencyKey = r.idReporte;
    r.ubicacionId = contexto.ubicacionValidadaContraCatalogo
        ? (cuerpo["ubicacion"]["id"] | static_cast<const char*>(nullptr)) : nullptr;
    r.zona = cuerpo["ubicacion"]["zona"] | static_cast<const char*>(nullptr);
    r.categoria = cuerpo["categoria"] | static_cast<const char*>(nullptr);
    const char* nivel = cuerpo["triajeSugerido"]["nivel"] | "";
    if (strcmp(nivel, "critico") == 0) r.nivel = conecta::Nivel::CRITICO;
    else if (strcmp(nivel, "alto") == 0) r.nivel = conecta::Nivel::ALTO;
    else if (strcmp(nivel, "medio") == 0) r.nivel = conecta::Nivel::MEDIO;
    else {
        pendiente.banderas = conecta::DATOS_INVALIDOS;
        pendiente.revisionAntifraude = true;
        return pendiente; // No inventar un nivel; conservar alerta para backend.
    }
    r.requiereVerificacion = cuerpo["triajeSugerido"]["requiereVerificacion"] | true;
    r.datosFaltantes = !cuerpo["triajeSugerido"]["datosFaltantes"].is<JsonArray>() ||
        cuerpo["triajeSugerido"]["datosFaltantes"].size() > 0;
    const char* version = cuerpo["triajeSugerido"]["versionReglas"] | "";
    r.discrepancias = strcmp(version, conecta::VERSION_REGLAS) != 0;
    if (cuerpo["personas"].isNull()) r.personas = -1;
    else if (cuerpo["personas"].is<long>() && cuerpo["personas"].as<long>() >= 1 &&
             cuerpo["personas"].as<long>() <= conecta::TRIAJE_MAX_PERSONAS)
        r.personas = static_cast<std::int32_t>(cuerpo["personas"].as<long>());
    else r.personas = 0; // invalida: conservar reporte y no sumar personas
    const char* necesidades[] = {"Agua potable", "Alimentos", "Refugio o alojamiento",
                                "Apoyo en salud", "Otra", "No sé todavía"};
    for (JsonVariantConst valor : cuerpo["necesidades"].as<JsonArrayConst>()) {
        const char* nombre = valor.as<const char*>();
        bool encontrada = false;
        for (std::size_t i = 0; i < conecta::NUM_NECESIDADES; ++i) {
            if (nombre && strcmp(nombre, necesidades[i]) == 0) {
                r.necesidades |= static_cast<std::uint8_t>(1u << i);
                encontrada = true;
            }
        }
        if (!encontrada) r.discrepancias = true;
    }
    const auto motivos = cuerpo["triajeSugerido"]["motivos"].as<JsonArrayConst>();
    r.cantidadRazones = motivos.size(); // > MAX_RAZONES produce capacidad agotada
    for (std::size_t i = 0; i < motivos.size() && i < conecta::MAX_RAZONES; ++i)
        r.razones[i] = motivos[i].as<const char*>();
    r.recibidoSegundos = contexto.recibidoSegundos;
    r.fechaRecepcion = contexto.fechaRecepcionUtc;
    r.contenidoCanonico = contexto.contenidoCanonico;
    r.patron = contexto.patronOperativo;
    r.incidenteConfirmado = contexto.incidenteConfirmado;
    r.evidenciaAsociacion = contexto.evidenciaAsociacion;
    // Sustituir este doble por FuenteIncidentes persistente del backend.
    // aplicado=true aqui SOLO significa indices RAM; NO responder "guardado".
    return motor.evaluar(r, indicesDeDemostracion);
}

void setup() {
    Serial.begin(115200);
    delay(500);
    servidor.iniciar();
}

void loop() {
    servidor.procesarDNS();
}
