#include "Triaje.h"

#include <cstdio>
#include <cstring>
#include <limits>

namespace conecta {
namespace detalle {
bool presente(const char* s) { return s && s[0]; }
bool iguales(const char* a, const char* b) { return std::strcmp(a ? a : "", b ? b : "") == 0; }
}
namespace {
std::size_t longitud(const char* s, std::size_t limite) {
    std::size_t n = 0;
    if (s) while (n <= limite && s[n]) ++n;
    return n;
}
template<std::size_t N>
bool copiar(char (&destino)[N], const char* texto) {
    if (!texto) { destino[0] = '\0'; return false; }
    std::size_t n = longitud(texto, N - 1);
    const bool truncado = n >= N;
    if (truncado) {
        n = N - 1;
        // No partir una secuencia UTF-8 valida. El adaptador valida UTF-8.
        while (n && (static_cast<unsigned char>(texto[n]) & 0xc0u) == 0x80u) --n;
    }
    std::memcpy(destino, texto, n);
    destino[n] = '\0';
    return truncado;
}
bool identidadValida(const char* s) {
    const auto n = longitud(s, MAX_ID);
    if (!n || n > MAX_ID) return false;
    for (std::size_t i = 0; i < n; ++i) {
        const char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    }
    return true;
}
bool revisar(std::uint32_t b) {
    return (b & (DATOS_FALTANTES | DATOS_INVALIDOS | DISCREPANCIAS |
                 RAFAGA | PATRON_REPETITIVO | CONFLICTO_IDEMPOTENCIA)) != 0;
}
ResultadoIngreso fallo(ResultadoIngreso res, std::uint32_t banderas) {
    res.banderas |= banderas;
    res.revisionAntifraude = revisar(res.banderas);
    res.aplicado = false;
    return res;
}
bool agregarRazon(IncidenteAgrupado& inc, const char* texto, std::uint32_t& flags) {
    if (!detalle::presente(texto)) return true;
    char razon[MAX_RAZON + 1] = {};
    if (copiar(razon, texto)) flags |= TEXTO_TRUNCADO;
    for (std::size_t i = 0; i < inc.cantidadRazones; ++i) {
        if (detalle::iguales(inc.razones[i], razon)) return true;
    }
    if (inc.cantidadRazones == MAX_RAZONES) return false;
    copiar(inc.razones[inc.cantidadRazones++], razon);
    return true;
}
}

const char* nombreNivel(Nivel n) {
    switch (n) {
        case Nivel::CRITICO: return "critico";
        case Nivel::ALTO: return "alto";
        case Nivel::MEDIO: return "medio";
    }
    return "invalido";
}
const char* nombreMotivo(MotivoAsociacion m) {
    switch (m) {
        case MotivoAsociacion::INCIDENTE_PROPIO: return "incidente_propio";
        case MotivoAsociacion::UBICACION_DESCONOCIDA: return "ubicacion_desconocida";
        case MotivoAsociacion::CANDIDATO: return "candidato_ubicacion_categoria_ventana";
        case MotivoAsociacion::ASOCIACION_CONFIRMADA: return "asociacion_confirmada_backend";
    }
    return "invalido";
}
bool esCandidato(const ReporteEntrante& r, const IncidenteAgrupado& inc, std::uint64_t ventana) {
    if (!detalle::presente(r.ubicacionId) || !detalle::presente(r.categoria) ||
        !detalle::iguales(r.ubicacionId, inc.ubicacionId) || !detalle::iguales(r.categoria, inc.tipo)) return false;
    const auto inicio = r.recibidoSegundos < inc.inicioSegundos ? r.recibidoSegundos : inc.inicioSegundos;
    const auto fin = r.recibidoSegundos > inc.finSegundos ? r.recibidoSegundos : inc.finSegundos;
    return fin >= inicio && fin - inicio < ventana;
}

ResultadoIngreso MotorTriaje::evaluar(const ReporteEntrante& r, FuenteIncidentes& fuente) {
    ResultadoIngreso res;
    res.configuracionAplicada = config_;
    // Nunca colapsar dos identidades por truncamiento. El original sigue en caller.
    if (!identidadValida(r.idReporte)) return fallo(res, detalle::presente(r.idReporte) ? DATOS_INVALIDOS : DATOS_FALTANTES);
    copiar(res.reporteId, r.idReporte);
    if (longitud(r.idempotencyKey, MAX_ID) > MAX_ID || longitud(r.contenidoCanonico, MAX_ID) > MAX_ID)
        return fallo(res, DATOS_INVALIDOS | TEXTO_TRUNCADO);
    auto consulta = fuente.buscarOperacion(r.idReporte, r.idempotencyKey, operacion_);
    if (consulta == Consulta::ERROR) return fallo(res, ERROR_FUENTE);
    if (consulta == Consulta::ENCONTRADO) {
        res = operacion_.resultado;
        res.tipo = TipoIngreso::REINTENTO_MISMO_ENVIO;
        if (!detalle::iguales(r.contenidoCanonico, operacion_.contenidoCanonico)) {
            // Backend resuelve 409 segun contrato; no sobrescribir el original.
            res.banderas |= CONFLICTO_IDEMPOTENCIA | DISCREPANCIAS;
            res.revisionAntifraude = true;
            res.aplicado = false;
        }
        return res;
    }
    if (r.nivel != Nivel::MEDIO && r.nivel != Nivel::ALTO && r.nivel != Nivel::CRITICO)
        return fallo(res, DATOS_INVALIDOS);
    if (r.datosFaltantes || !detalle::presente(r.categoria) || !detalle::presente(r.fechaRecepcion)) res.banderas |= DATOS_FALTANTES;
    if (r.personas == -1) res.banderas |= DATOS_FALTANTES;
    else if (r.personas < 1 || r.personas > TRIAJE_MAX_PERSONAS) res.banderas |= DATOS_INVALIDOS;
    if (r.discrepancias || ((r.necesidades & 32u) && (r.necesidades & 31u))) res.banderas |= DISCREPANCIAS;
    if (r.necesidades & 192u) res.banderas |= DATOS_INVALIDOS;
    const bool lugarValido = detalle::presente(r.ubicacionId) && longitud(r.ubicacionId, MAX_ID) <= MAX_ID;
    const bool categoriaValida = detalle::presente(r.categoria) && longitud(r.categoria, MAX_CATEGORIA) <= MAX_CATEGORIA;
    if (!lugarValido) res.banderas |= DATOS_FALTANTES;
    if (longitud(r.ubicacionId, MAX_ID) > MAX_ID || !categoriaValida) res.banderas |= DATOS_INVALIDOS;
    Estadisticas stats;
    if (!fuente.estadisticas(r, config_.rafagaSegundos, stats)) return fallo(res, ERROR_FUENTE);
    if (stats.mismaClave >= config_.maxReportesRafaga) res.banderas |= RAFAGA;
    if (detalle::presente(r.patron) && (config_.minPatronRepetitivo == 0 || stats.mismoPatron >= config_.minPatronRepetitivo - 1)) res.banderas |= PATRON_REPETITIVO;
    bool asociar = false;
    if (lugarValido && categoriaValida) {
        if (detalle::presente(r.incidenteConfirmado)) {
            consulta = fuente.buscarIncidente(r.incidenteConfirmado, trabajo_);
            if (consulta == Consulta::ERROR) return fallo(res, ERROR_FUENTE);
            asociar = consulta == Consulta::ENCONTRADO && esCandidato(r, trabajo_, config_.ventanaSegundos) && detalle::presente(r.evidenciaAsociacion);
            if (!asociar) res.banderas |= DISCREPANCIAS;
        }
        if (asociar) {
            res.motivoAsociacion = MotivoAsociacion::ASOCIACION_CONFIRMADA;
            if (copiar(res.evidenciaAsociacion, r.evidenciaAsociacion)) res.banderas |= TEXTO_TRUNCADO;
        } else {
            consulta = fuente.buscarCandidato(r, config_.ventanaSegundos, trabajo_);
            if (consulta == Consulta::ERROR) return fallo(res, ERROR_FUENTE);
            if (consulta == Consulta::ENCONTRADO && esCandidato(r, trabajo_, config_.ventanaSegundos)) {
                copiar(res.candidatoId, trabajo_.incidenteId);
                res.motivoAsociacion = MotivoAsociacion::CANDIDATO;
            }
        }
    } else {
        res.motivoAsociacion = MotivoAsociacion::UBICACION_DESCONOCIDA;
        if (detalle::presente(r.incidenteConfirmado)) res.banderas |= DISCREPANCIAS;
    }
    if (!asociar) {
        std::snprintf(res.incidenteId, sizeof(res.incidenteId), "I-%s", res.reporteId);
        consulta = fuente.buscarIncidente(res.incidenteId, trabajo_);
        if (consulta != Consulta::AUSENTE) return fallo(res, ERROR_FUENTE);
        trabajo_ = {};
        copiar(trabajo_.incidenteId, res.incidenteId);
        if (lugarValido) copiar(trabajo_.ubicacionId, r.ubicacionId);
        if (copiar(trabajo_.zona, r.zona) || !categoriaValida) res.banderas |= TEXTO_TRUNCADO;
        if (copiar(trabajo_.tipo, r.categoria)) res.banderas |= TEXTO_TRUNCADO;
        trabajo_.inicioSegundos = trabajo_.finSegundos = r.recibidoSegundos;
        if (copiar(trabajo_.primeraRecepcion, r.fechaRecepcion)) res.banderas |= TEXTO_TRUNCADO;
        copiar(trabajo_.ultimaRecepcion, trabajo_.primeraRecepcion);
    } else if (!detalle::iguales(r.zona, trabajo_.zona)) {
        res.banderas |= DISCREPANCIAS;
    }
    copiar(res.incidenteId, trabajo_.incidenteId);
    if (trabajo_.cantidadIds >= MAX_REPORTES_INCIDENTE || trabajo_.cantidadRazones > MAX_RAZONES ||
        r.cantidadRazones > MAX_RAZONES || trabajo_.cantidadReportes == std::numeric_limits<std::uint32_t>::max())
        return fallo(res, CAPACIDAD_AGOTADA);
    for (std::size_t i = 0; i < r.cantidadRazones; ++i) {
        if (!agregarRazon(trabajo_, r.razones[i], res.banderas)) return fallo(res, CAPACIDAD_AGOTADA);
    }
    if (r.personas >= 1 && r.personas <= TRIAJE_MAX_PERSONAS) {
        const auto personas = static_cast<std::uint64_t>(r.personas);
        if (trabajo_.cantidadPersonasDeclaradas > std::numeric_limits<std::uint64_t>::max() - personas) return fallo(res, CAPACIDAD_AGOTADA);
        trabajo_.cantidadPersonasDeclaradas += personas;
    }
    for (std::size_t i = 0; i < NUM_NECESIDADES; ++i) {
        if (!(r.necesidades & (1u << i))) continue;
        if (trabajo_.necesidades[i] == std::numeric_limits<std::uint32_t>::max()) return fallo(res, CAPACIDAD_AGOTADA);
        ++trabajo_.necesidades[i];
    }
    ++trabajo_.cantidadReportes;
    copiar(trabajo_.reportes[trabajo_.cantidadIds++], r.idReporte);
    if (r.nivel > trabajo_.prioridadSugerida) trabajo_.prioridadSugerida = r.nivel;
    if (r.recibidoSegundos < trabajo_.inicioSegundos) {
        trabajo_.inicioSegundos = r.recibidoSegundos;
        if (copiar(trabajo_.primeraRecepcion, r.fechaRecepcion)) res.banderas |= TEXTO_TRUNCADO;
    }
    if (r.recibidoSegundos >= trabajo_.finSegundos) {
        trabajo_.finSegundos = r.recibidoSegundos;
        if (copiar(trabajo_.ultimaRecepcion, r.fechaRecepcion)) res.banderas |= TEXTO_TRUNCADO;
    }
    operacion_ = {};
    copiar(operacion_.idReporte, r.idReporte);
    copiar(operacion_.idempotencyKey, r.idempotencyKey);
    copiar(operacion_.contenidoCanonico, r.contenidoCanonico);
    if (copiar(operacion_.patron, r.patron)) res.banderas |= TEXTO_TRUNCADO;
    if (lugarValido) copiar(operacion_.ubicacionId, r.ubicacionId);
    copiar(operacion_.categoria, r.categoria);
    operacion_.recibidoSegundos = r.recibidoSegundos;
    res.revisionAntifraude = revisar(res.banderas);
    trabajo_.requiereVerificacion = trabajo_.requiereVerificacion || r.requiereVerificacion || !lugarValido || res.revisionAntifraude || (res.banderas & TEXTO_TRUNCADO);
    trabajo_.estadoVerificacion = trabajo_.requiereVerificacion ? EstadoVerificacion::POR_VERIFICAR : EstadoVerificacion::SIN_VERIFICAR;
    trabajo_.banderas |= res.banderas;
    trabajo_.revisionAntifraude = trabajo_.revisionAntifraude || res.revisionAntifraude;
    res.aplicado = true;
    operacion_.resultado = res;
    const auto escritura = fuente.guardar(r, operacion_, trabajo_);
    if (escritura != Escritura::APLICADA) return fallo(res, escritura == Escritura::SIN_CAPACIDAD ? CAPACIDAD_AGOTADA : ERROR_FUENTE);
    return res;
}

namespace {
class Json {
public:
    Json(char* buffer, std::size_t capacidad) : b_(buffer), cap_(buffer ? capacidad : 0) {}
    void texto(const char* s) { while (*s) caracter(*s++); }
    void numero(std::uint64_t n) {
        char tmp[32];
        std::snprintf(tmp, sizeof(tmp), "%llu", static_cast<unsigned long long>(n));
        texto(tmp);
    }
    void cadena(const char* s, std::size_t maximo) {
        caracter('"');
        for (std::size_t i = 0; i < maximo && s[i]; ++i) {
            const auto c = static_cast<unsigned char>(s[i]);
            if (c == '"' || c == '\\') { caracter('\\'); caracter(static_cast<char>(c)); }
            else if (c < 0x20u) {
                char esc[7];
                std::snprintf(esc, sizeof(esc), "\\u%04x", static_cast<unsigned int>(c));
                texto(esc);
            } else caracter(static_cast<char>(c));
        }
        caracter('"');
    }
    ResultadoJson terminar() {
        const bool ok = b_ && n_ < cap_;
        if (ok) b_[n_] = '\0';
        else if (cap_) b_[0] = '\0';
        return {ok, ok ? n_ : 0, n_ + 1};
    }
private:
    void caracter(char c) { if (n_ < cap_) b_[n_] = c; ++n_; }
    char* b_;
    std::size_t cap_;
    std::size_t n_ = 0;
};
}
ResultadoJson serializarJson(const IncidenteAgrupado& inc, char* buffer, std::size_t capacidad) {
    if (inc.cantidadRazones > MAX_RAZONES || inc.cantidadIds > MAX_REPORTES_INCIDENTE) {
        if (buffer && capacidad) buffer[0] = '\0';
        return {false, 0, 0};
    }
    Json j(buffer, capacidad);
    j.texto("{\"incidenteId\":"); j.cadena(inc.incidenteId, MAX_INCIDENTE_ID);
    j.texto(",\"zona\":"); if (inc.zona[0]) j.cadena(inc.zona, MAX_ZONA); else j.texto("null");
    j.texto(",\"tipo\":"); j.cadena(inc.tipo, MAX_CATEGORIA);
    j.texto(",\"prioridadSugerida\":"); j.cadena(nombreNivel(inc.prioridadSugerida), 8);
    j.texto(",\"estadoVerificacion\":");
    j.texto((inc.requiereVerificacion || inc.estadoVerificacion == EstadoVerificacion::POR_VERIFICAR) ? "\"por_verificar\"" : "\"sin_verificar\"");
    j.texto(",\"cantidadReportes\":"); j.numero(inc.cantidadReportes);
    j.texto(",\"cantidadPersonasDeclaradas\":"); j.numero(inc.cantidadPersonasDeclaradas);
    j.texto(",\"necesidades\":{");
    const char* nombres[NUM_NECESIDADES] = {"Agua potable", "Alimentos", "Refugio o alojamiento", "Apoyo en salud", "Otra", "No sé todavía"};
    for (std::size_t i = 0; i < NUM_NECESIDADES; ++i) {
        if (i) j.texto(",");
        j.cadena(nombres[i], 32); j.texto(":"); j.numero(inc.necesidades[i]);
    }
    j.texto("},\"razones\":[");
    for (std::size_t i = 0; i < inc.cantidadRazones; ++i) {
        if (i) j.texto(",");
        j.cadena(inc.razones[i], MAX_RAZON);
    }
    j.texto("],\"reportes\":[");
    for (std::size_t i = 0; i < inc.cantidadIds; ++i) {
        if (i) j.texto(",");
        j.cadena(inc.reportes[i], MAX_ID);
    }
    j.texto("],\"primeraRecepcion\":"); j.cadena(inc.primeraRecepcion, MAX_FECHA);
    j.texto(",\"ultimaRecepcion\":"); j.cadena(inc.ultimaRecepcion, MAX_FECHA);
    j.texto(",\"revisionAntifraude\":"); j.texto(inc.revisionAntifraude ? "true" : "false");
    j.texto(",\"banderas\":"); j.numero(inc.banderas);
    j.texto(",\"versionReglas\":"); j.cadena(VERSION_REGLAS, sizeof(VERSION_REGLAS) - 1);
    j.texto(",\"versionAgrupacion\":"); j.cadena(VERSION_AGRUPACION, sizeof(VERSION_AGRUPACION) - 1);
    j.texto("}");
    return j.terminar();
}
} // namespace conecta
