#pragma once

#include <cstddef>
#include <cstdint>

namespace conecta {

// Capacidades en bytes, sin el NUL. No hay asignaciones dinamicas.
constexpr std::size_t MAX_INCIDENTES_ACTIVOS = 64;
constexpr std::size_t MAX_IDS_RECIENTES = 256;
constexpr std::size_t MAX_ID = 64;
constexpr std::size_t MAX_INCIDENTE_ID = 66; // "I-" + idReporte
constexpr std::size_t MAX_ZONA = 96;
constexpr std::size_t MAX_CATEGORIA = 24;
constexpr std::size_t MAX_RAZON = 160;
constexpr std::size_t MAX_RAZONES = 8;
constexpr std::size_t MAX_REPORTES_INCIDENTE = 64;
constexpr std::size_t MAX_FECHA = 24;
constexpr std::size_t MAX_ESTADO = 31;
constexpr std::size_t NUM_NECESIDADES = 6;
constexpr std::int32_t TRIAJE_MAX_PERSONAS = 100000;
constexpr std::uint64_t VENTANA_INCIDENTE_SEGUNDOS = 20 * 60;
constexpr char VERSION_REGLAS[] = "triaje-2026-11-20.1";
constexpr char VERSION_AGRUPACION[] = "agrupacion-2026-11-20.1";

enum class Nivel : std::uint8_t { MEDIO, ALTO, CRITICO };
enum class EstadoVerificacion : std::uint8_t { SIN_VERIFICAR, POR_VERIFICAR };
enum class TipoIngreso : std::uint8_t { NUEVO, REINTENTO_MISMO_ENVIO };
enum class MotivoAsociacion : std::uint8_t {
    INCIDENTE_PROPIO, UBICACION_DESCONOCIDA, CANDIDATO,
    ASOCIACION_CONFIRMADA
};
enum Bandera : std::uint32_t {
    NINGUNA = 0, CAPACIDAD_AGOTADA = 1u << 0, DATOS_FALTANTES = 1u << 1,
    DATOS_INVALIDOS = 1u << 2, DISCREPANCIAS = 1u << 3,
    RAFAGA = 1u << 4, PATRON_REPETITIVO = 1u << 5,
    TEXTO_TRUNCADO = 1u << 6, ERROR_FUENTE = 1u << 7,
    CONFLICTO_IDEMPOTENCIA = 1u << 8
};

struct Configuracion {
    std::uint64_t ventanaSegundos = VENTANA_INCIDENTE_SEGUNDOS;
    std::uint64_t rafagaSegundos = 30;
    std::uint32_t maxReportesRafaga = 10; // mas de N, incluido el entrante
    std::uint32_t minPatronRepetitivo = 5;
};

// Punteros prestados solo durante evaluar(). Cadenas UTF-8 terminadas en NUL.
// Los tiempos/fecha de recepcion los suministra el backend; no el cliente.
// El nivel ya calculado no se reclasifica. Necesidades: bits AG,AL,RF,SA,OT,NS.
struct ReporteEntrante {
    const char* idReporte = nullptr;
    const char* idempotencyKey = nullptr; // opcional; no identidad de persona
    const char* ubicacionId = nullptr; // solo ID validado contra catalogo
    const char* zona = nullptr;
    const char* categoria = nullptr;
    Nivel nivel = Nivel::MEDIO;
    bool requiereVerificacion = false;
    std::int32_t personas = -1;
    std::uint8_t necesidades = 0;
    bool datosFaltantes = false;
    bool discrepancias = false; // coherencia comprobada por el adaptador
    std::uint64_t recibidoSegundos = 0;
    const char* fechaRecepcion = nullptr; // ISO UTC validado por backend
    const char* razones[MAX_RAZONES] = {};
    std::size_t cantidadRazones = 0;
    // Token de contenido canonico y patron operativo: calculados por backend.
    // Nunca IP/MAC, contacto ni huella de usuario. No sirven para deduplicar.
    const char* contenidoCanonico = nullptr;
    const char* patron = nullptr;
    // Evidencia adicional/revision backend. Coincidencia sola NO autoriza unir.
    const char* incidenteConfirmado = nullptr;
    const char* evidenciaAsociacion = nullptr;
};

struct ResultadoIngreso {
    TipoIngreso tipo = TipoIngreso::NUEVO;
    char reporteId[MAX_ID + 1] = {};
    char incidenteId[MAX_INCIDENTE_ID + 1] = {};
    char candidatoId[MAX_INCIDENTE_ID + 1] = {};
    MotivoAsociacion motivoAsociacion = MotivoAsociacion::INCIDENTE_PROPIO;
    char evidenciaAsociacion[MAX_RAZON + 1] = {};
    std::uint32_t banderas = 0;
    bool revisionAntifraude = false;
    bool aplicado = false; // transaccion de Fuente; NO acuse de PMU/persistencia
    char estadoOriginal[MAX_ESTADO + 1] = "sin_confirmar";
    char versionReglas[sizeof(VERSION_REGLAS)] = "triaje-2026-11-20.1";
    char versionAgrupacion[sizeof(VERSION_AGRUPACION)] = "agrupacion-2026-11-20.1";
    Configuracion configuracionAplicada;
};

struct IncidenteAgrupado {
    char incidenteId[MAX_INCIDENTE_ID + 1] = {};
    char ubicacionId[MAX_ID + 1] = {};
    char zona[MAX_ZONA + 1] = {};
    char tipo[MAX_CATEGORIA + 1] = {};
    Nivel prioridadSugerida = Nivel::MEDIO;
    bool requiereVerificacion = false;
    EstadoVerificacion estadoVerificacion = EstadoVerificacion::SIN_VERIFICAR;
    std::uint32_t cantidadReportes = 0;
    std::uint64_t cantidadPersonasDeclaradas = 0; // suma, JAMAS personas unicas
    std::uint32_t necesidades[NUM_NECESIDADES] = {}; // reportes por necesidad
    char razones[MAX_RAZONES][MAX_RAZON + 1] = {};
    std::size_t cantidadRazones = 0;
    char reportes[MAX_REPORTES_INCIDENTE][MAX_ID + 1] = {};
    std::size_t cantidadIds = 0;
    char primeraRecepcion[MAX_FECHA + 1] = {};
    char ultimaRecepcion[MAX_FECHA + 1] = {};
    std::uint64_t inicioSegundos = 0;
    std::uint64_t finSegundos = 0;
    std::uint32_t banderas = 0;
    bool revisionAntifraude = false;
};

struct RegistroOperacion {
    char idReporte[MAX_ID + 1] = {};
    char idempotencyKey[MAX_ID + 1] = {};
    char contenidoCanonico[MAX_ID + 1] = {};
    char patron[MAX_ID + 1] = {};
    char ubicacionId[MAX_ID + 1] = {};
    char categoria[MAX_CATEGORIA + 1] = {};
    std::uint64_t recibidoSegundos = 0;
    ResultadoIngreso resultado;
};

enum class Consulta { ENCONTRADO, AUSENTE, ERROR };
enum class Escritura { APLICADA, SIN_CAPACIDAD, ERROR };
struct Estadisticas { std::uint32_t mismaClave = 0; std::uint32_t mismoPatron = 0; };

// Consultas contra indices persistidos; nunca requiere cargar reportes.jsonl.
// ERROR no equivale a AUSENTE. Serializar evaluar() por estacion/transaccion.
class FuenteIncidentes {
public:
    virtual ~FuenteIncidentes() = default;
    virtual Consulta buscarOperacion(const char* id, const char* clave,
                                     RegistroOperacion& salida) const = 0;
    virtual Consulta buscarIncidente(const char* id, IncidenteAgrupado& salida) const = 0;
    virtual Consulta buscarCandidato(const ReporteEntrante&, std::uint64_t ventana,
                                    IncidenteAgrupado& salida) const = 0;
    virtual bool estadisticas(const ReporteEntrante&, std::uint64_t ventana,
                             Estadisticas& salida) const = 0;
    // Atomica: original completo + indice + resultado + agregado, o nada.
    // El estado de entrega se mantiene aparte en backend; no se infiere aqui.
    virtual Escritura guardar(const ReporteEntrante& original,
                              const RegistroOperacion&, const IncidenteAgrupado&) = 0;
};

bool esCandidato(const ReporteEntrante&, const IncidenteAgrupado&, std::uint64_t ventana);
const char* nombreNivel(Nivel);
const char* nombreMotivo(MotivoAsociacion);

class MotorTriaje {
public:
    explicit MotorTriaje(Configuracion config = {}) : config_(config) {}
    ResultadoIngreso evaluar(const ReporteEntrante&, FuenteIncidentes&);
private:
    Configuracion config_;
    IncidenteAgrupado trabajo_; // ubicar Motor en memoria estatica en ESP32
    RegistroOperacion operacion_;
};

// JSON completo o buffer vacio; requeridos incluye NUL, escritos lo excluye.
struct ResultadoJson { bool completo; std::size_t escritos; std::size_t requeridos; };
ResultadoJson serializarJson(const IncidenteAgrupado&, char* buffer, std::size_t capacidad);

// Doble acotado: conserva indices y resultados, NO originales completos.
// Sin eviction: al llenarse devuelve SIN_CAPACIDAD sin mutar nada.
// Parametros de plantilla permiten fixtures pequenos para ESP32.
template<std::size_t I = MAX_INCIDENTES_ACTIVOS, std::size_t R = MAX_IDS_RECIENTES>
class FuenteMemoria final : public FuenteIncidentes {
    static_assert(I > 0 && I <= MAX_INCIDENTES_ACTIVOS, "Capacidad de incidentes invalida");
    static_assert(R > 0 && R <= MAX_IDS_RECIENTES, "Capacidad de IDs invalida");
public:
    Consulta buscarOperacion(const char*, const char*, RegistroOperacion&) const override;
    Consulta buscarIncidente(const char*, IncidenteAgrupado&) const override;
    Consulta buscarCandidato(const ReporteEntrante&, std::uint64_t, IncidenteAgrupado&) const override;
    bool estadisticas(const ReporteEntrante&, std::uint64_t, Estadisticas&) const override;
    Escritura guardar(const ReporteEntrante&, const RegistroOperacion&, const IncidenteAgrupado&) override;
    std::size_t cantidadIncidentes() const { return nIncidentes_; }
    std::size_t cantidadOperaciones() const { return nOperaciones_; }
private:
    IncidenteAgrupado incidentes_[I] = {};
    RegistroOperacion operaciones_[R] = {};
    std::size_t nIncidentes_ = 0;
    std::size_t nOperaciones_ = 0;
};

// Implementacion de plantilla; comparaciones exactas de identidad, sin hashes.
namespace detalle {
bool iguales(const char*, const char*);
bool presente(const char*);
}
template<std::size_t I, std::size_t R>
Consulta FuenteMemoria<I,R>::buscarOperacion(const char* id, const char* clave, RegistroOperacion& out) const {
    const RegistroOperacion* encontrado = nullptr;
    for (std::size_t n = 0; n < nOperaciones_; ++n) {
        const auto& op = operaciones_[n];
        if ((detalle::presente(id) && detalle::iguales(id, op.idReporte)) ||
            (detalle::presente(clave) && detalle::iguales(clave, op.idempotencyKey))) {
            if (encontrado && encontrado != &op) return Consulta::ERROR;
            encontrado = &op;
        }
    }
    if (!encontrado) return Consulta::AUSENTE;
    out = *encontrado;
    return Consulta::ENCONTRADO;
}
template<std::size_t I, std::size_t R>
Consulta FuenteMemoria<I,R>::buscarIncidente(const char* id, IncidenteAgrupado& out) const {
    for (std::size_t n = 0; n < nIncidentes_; ++n) {
        if (detalle::iguales(id, incidentes_[n].incidenteId)) {
            out = incidentes_[n]; return Consulta::ENCONTRADO;
        }
    }
    return Consulta::AUSENTE;
}
template<std::size_t I, std::size_t R>
Consulta FuenteMemoria<I,R>::buscarCandidato(const ReporteEntrante& r, std::uint64_t v, IncidenteAgrupado& out) const {
    for (std::size_t n = 0; n < nIncidentes_; ++n) {
        if (esCandidato(r, incidentes_[n], v)) { out = incidentes_[n]; return Consulta::ENCONTRADO; }
    }
    return Consulta::AUSENTE;
}
template<std::size_t I, std::size_t R>
bool FuenteMemoria<I,R>::estadisticas(const ReporteEntrante& r, std::uint64_t ventana, Estadisticas& out) const {
    out = {};
    for (std::size_t n = 0; n < nOperaciones_; ++n) {
        const auto& op = operaciones_[n];
        if (op.recibidoSegundos > r.recibidoSegundos || r.recibidoSegundos - op.recibidoSegundos > ventana) continue;
        // Clave operativa: ubicacionId + categoria, incluso grupo desconocido.
        if (detalle::iguales(r.ubicacionId, op.ubicacionId) && detalle::iguales(r.categoria, op.categoria)) {
            ++out.mismaClave;
            if (detalle::presente(r.patron) && detalle::iguales(r.patron, op.patron)) ++out.mismoPatron;
        }
    }
    return true;
}
template<std::size_t I, std::size_t R>
Escritura FuenteMemoria<I,R>::guardar(const ReporteEntrante&, const RegistroOperacion& op, const IncidenteAgrupado& inc) {
    std::size_t pos = 0;
    while (pos < nIncidentes_ && !detalle::iguales(inc.incidenteId, incidentes_[pos].incidenteId)) ++pos;
    if (nOperaciones_ == R || (pos == nIncidentes_ && nIncidentes_ == I)) return Escritura::SIN_CAPACIDAD;
    // Defensa adicional: el motor es de un solo escritor; no sobrescribir IDs.
    for (std::size_t n = 0; n < nOperaciones_; ++n) {
        if (detalle::iguales(op.idReporte, operaciones_[n].idReporte) ||
            (detalle::presente(op.idempotencyKey) && detalle::iguales(op.idempotencyKey, operaciones_[n].idempotencyKey))) return Escritura::ERROR;
    }
    incidentes_[pos] = inc;
    operaciones_[nOperaciones_++] = op;
    if (pos == nIncidentes_) ++nIncidentes_;
    return Escritura::APLICADA;
}

} // namespace conecta
