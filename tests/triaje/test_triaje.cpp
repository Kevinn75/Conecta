#include "Triaje.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace conecta;
#define EXIGIR(condicion) do { if (!(condicion)) { \
    std::fprintf(stderr, "Fallo linea %d: %s\n", __LINE__, #condicion); std::exit(1); \
} } while (false)

static ReporteEntrante base(const char* id) {
    ReporteEntrante r;
    r.idReporte = id;
    r.ubicacionId = "SITIO-FICTICIO";
    r.zona = "Zona ficticia";
    r.categoria = "derrumbe";
    r.nivel = Nivel::CRITICO;
    r.personas = 3;
    r.necesidades = 3;
    r.recibidoSegundos = 10000;
    r.fechaRecepcion = "2026-11-20T15:00:00.000Z";
    r.razones[0] = "Peligro inmediato declarado";
    r.cantidadRazones = 1;
    r.contenidoCanonico = "contenido-ficticio-v1";
    r.patron = "derrumbe-patron-ficticio";
    return r;
}
static IncidenteAgrupado obtener(FuenteIncidentes& fuente, const char* id) {
    IncidenteAgrupado i;
    EXIGIR(fuente.buscarIncidente(id, i) == Consulta::ENCONTRADO);
    return i;
}
static void confirmar(ReporteEntrante& r, const char* id) {
    r.incidenteConfirmado = id;
    r.evidenciaAsociacion = "Backend: mismo evento verificado en sitio ficticio";
}

static void familias_y_reintento() {
    FuenteMemoria<4, 8> fuente;
    MotorTriaje motor;
    auto r = base("FAMILIA-A");
    auto a = motor.evaluar(r, fuente);
    EXIGIR(a.aplicado && a.tipo == TipoIngreso::NUEVO);
    // Otro telefono / familia: ID diferente, conserva su propia declaracion.
    r = base("FAMILIA-B"); r.personas = 7;
    confirmar(r, a.incidenteId);
    auto b = motor.evaluar(r, fuente);
    EXIGIR(b.aplicado && b.tipo == TipoIngreso::NUEVO);
    EXIGIR(b.motivoAsociacion == MotivoAsociacion::ASOCIACION_CONFIRMADA);
    EXIGIR(std::strcmp(a.incidenteId, b.incidenteId) == 0);
    EXIGIR(fuente.cantidadIncidentes() == 1 && fuente.cantidadOperaciones() == 2);
    auto inc = obtener(fuente, a.incidenteId);
    EXIGIR(inc.cantidadReportes == 2 && inc.cantidadIds == 2);
    EXIGIR(inc.cantidadPersonasDeclaradas == 10); // declaradas, no unicas
    EXIGIR(std::strcmp(inc.reportes[0], "FAMILIA-A") == 0);
    EXIGIR(std::strcmp(inc.reportes[1], "FAMILIA-B") == 0);
    EXIGIR(inc.necesidades[0] == 2 && inc.necesidades[1] == 2);
    auto retry = motor.evaluar(r, fuente); // tambien simula corte tras aplicar
    EXIGIR(retry.tipo == TipoIngreso::REINTENTO_MISMO_ENVIO && retry.aplicado);
    EXIGIR(retry.banderas == b.banderas && retry.motivoAsociacion == b.motivoAsociacion);
    EXIGIR(std::strcmp(retry.estadoOriginal, b.estadoOriginal) == 0);
    EXIGIR(std::strcmp(retry.versionReglas, VERSION_REGLAS) == 0);
    EXIGIR(fuente.cantidadOperaciones() == 2);
    EXIGIR(obtener(fuente, a.incidenteId).cantidadPersonasDeclaradas == 10);
    r.contenidoCanonico = "contenido-modificado";
    auto conflicto = motor.evaluar(r, fuente);
    EXIGIR(!conflicto.aplicado && (conflicto.banderas & CONFLICTO_IDEMPOTENCIA));
    EXIGIR(obtener(fuente, a.incidenteId).cantidadReportes == 2);
}

static void candidatos_no_duplicados() {
    FuenteMemoria<4, 8> f;
    MotorTriaje m;
    auto a = m.evaluar(base("C-A"), f);
    auto b = m.evaluar(base("C-B"), f);
    EXIGIR(b.aplicado && b.tipo == TipoIngreso::NUEVO);
    EXIGIR(b.motivoAsociacion == MotivoAsociacion::CANDIDATO);
    EXIGIR(std::strcmp(b.candidatoId, a.incidenteId) == 0);
    EXIGIR(std::strcmp(b.incidenteId, a.incidenteId) != 0);
    EXIGIR(f.cantidadOperaciones() == 2 && f.cantidadIncidentes() == 2);
    auto r = base("C-C"); r.ubicacionId = "OTRO-SITIO";
    auto c = m.evaluar(r, f); // misma zona no basta
    EXIGIR(c.aplicado && c.candidatoId[0] == '\0');
    r = base("C-D"); r.categoria = "atrapadas";
    EXIGIR(m.evaluar(r, f).candidatoId[0] == '\0');
}

static void rafaga_y_alerta_sospechosa() {
    FuenteMemoria<2, 64> f;
    MotorTriaje m;
    char id[32];
    ResultadoIngreso primero;
    for (unsigned n = 0; n < 50; ++n) {
        std::snprintf(id, sizeof(id), "RAFAGA-%u", n);
        auto r = base(id); r.categoria = "atrapadas";
        if (n) confirmar(r, primero.incidenteId);
        if (n == 49) r.discrepancias = true;
        auto resultado = m.evaluar(r, f);
        if (!n) primero = resultado;
        EXIGIR(resultado.aplicado && resultado.tipo == TipoIngreso::NUEVO);
        if (n >= 10) EXIGIR(resultado.revisionAntifraude && (resultado.banderas & RAFAGA));
        if (n >= 4) EXIGIR(resultado.banderas & PATRON_REPETITIVO);
        if (n == 9) EXIGIR(!(resultado.banderas & RAFAGA));
    }
    const auto inc = obtener(f, primero.incidenteId);
    EXIGIR(inc.cantidadReportes == 50 && inc.cantidadIds == 50);
    EXIGIR(inc.cantidadPersonasDeclaradas == 150 && f.cantidadOperaciones() == 50);
    EXIGIR(inc.prioridadSugerida == Nivel::CRITICO && inc.revisionAntifraude);
    EXIGIR(inc.banderas & DISCREPANCIAS);
    auto original = base("RAFAGA-0"); original.categoria = "atrapadas";
    auto retry = m.evaluar(original, f);
    EXIGIR(retry.tipo == TipoIngreso::REINTENTO_MISMO_ENVIO);
    EXIGIR(retry.banderas == primero.banderas && !retry.revisionAntifraude);
    EXIGIR(obtener(f, primero.incidenteId).cantidadReportes == 50);
    for (unsigned n = 0; n < 50; ++n) {
        std::snprintf(id, sizeof(id), "RAFAGA-%u", n);
        EXIGIR(std::strcmp(inc.reportes[n], id) == 0);
    }
}

static void desconocida_y_personas() {
    FuenteMemoria<8, 16> f;
    MotorTriaje m;
    auto r = base("SIN-LUGAR-1"); r.ubicacionId = nullptr;
    auto a = m.evaluar(r, f);
    r.idReporte = "SIN-LUGAR-2";
    confirmar(r, a.incidenteId); // ni una confirmacion puede forzar sitio desconocido
    auto b = m.evaluar(r, f);
    EXIGIR(a.aplicado && b.aplicado && std::strcmp(a.incidenteId, b.incidenteId) != 0);
    EXIGIR(b.motivoAsociacion == MotivoAsociacion::UBICACION_DESCONOCIDA);
    auto inc = obtener(f, b.incidenteId);
    EXIGIR(inc.prioridadSugerida == Nivel::CRITICO && inc.requiereVerificacion);
    EXIGIR(inc.estadoVerificacion == EstadoVerificacion::POR_VERIFICAR);
    EXIGIR(inc.cantidadReportes == 1);
    const std::int32_t valores[] = {-1, -2, 0, 1, 100000, 100001};
    const std::uint64_t sumas[] = {0, 0, 0, 1, 100000, 0};
    char id[32];
    for (unsigned n = 0; n < 6; ++n) {
        std::snprintf(id, sizeof(id), "PERSONAS-%u", n);
        r = base(id); r.personas = valores[n];
        auto res = m.evaluar(r, f);
        EXIGIR(res.aplicado);
        inc = obtener(f, res.incidenteId);
        EXIGIR(inc.cantidadPersonasDeclaradas == sumas[n]);
        EXIGIR(inc.prioridadSugerida == Nivel::CRITICO);
        if (valores[n] == -1) EXIGIR(res.banderas & DATOS_FALTANTES);
        else if (valores[n] < 1 || valores[n] > 100000) EXIGIR(res.banderas & DATOS_INVALIDOS);
        else EXIGIR(!(res.banderas & (DATOS_FALTANTES | DATOS_INVALIDOS)));
    }
    static_assert(TRIAJE_MAX_PERSONAS == 100000, "Limite del contrato");
}

static void borde_ventana_y_maximo() {
    const std::uint64_t diferencias[] = {1199, 1200, 1201};
    for (auto diferencia : diferencias) {
        FuenteMemoria<3, 8> f;
        MotorTriaje m;
        auto r = base("VENTANA-A"); r.nivel = Nivel::ALTO;
        auto a = m.evaluar(r, f);
        r.idReporte = "VENTANA-B"; r.nivel = Nivel::CRITICO;
        r.recibidoSegundos += diferencia;
        confirmar(r, a.incidenteId);
        auto b = m.evaluar(r, f);
        EXIGIR(b.aplicado);
        EXIGIR((std::strcmp(a.incidenteId, b.incidenteId) == 0) == (diferencia < 1200));
        EXIGIR(obtener(f, b.incidenteId).prioridadSugerida == Nivel::CRITICO);
        if (diferencia < 1200) {
            r.idReporte = "VENTANA-C"; r.nivel = Nivel::MEDIO;
            EXIGIR(m.evaluar(r, f).aplicado);
            EXIGIR(obtener(f, a.incidenteId).prioridadSugerida == Nivel::CRITICO);
            r.idReporte = "VENTANA-D"; ++r.recibidoSegundos;
            auto d = m.evaluar(r, f); // no prolongar indefinidamente con ultima fecha
            EXIGIR(std::strcmp(d.incidenteId, a.incidenteId) != 0);
        }
    }
    Configuracion config;
    config.ventanaSegundos = 60;
    FuenteMemoria<2, 8> f;
    MotorTriaje m(config);
    auto a = m.evaluar(base("CONFIG-A"), f);
    auto r = base("CONFIG-B"); r.recibidoSegundos += 60; confirmar(r, a.incidenteId);
    auto b = m.evaluar(r, f);
    EXIGIR(std::strcmp(a.incidenteId, b.incidenteId) != 0);
    EXIGIR(b.configuracionAplicada.ventanaSegundos == 60);
}

static void capacidad() {
    // Maximo de incidentes: ningun indice existente se expulsa.
    static FuenteMemoria<> f;
    MotorTriaje m;
    char id[32];
    for (unsigned n = 0; n < MAX_INCIDENTES_ACTIVOS; ++n) {
        std::snprintf(id, sizeof(id), "LLENO-%u", n);
        auto r = base(id); r.ubicacionId = nullptr;
        EXIGIR(m.evaluar(r, f).aplicado);
    }
    auto r = base("PENDIENTE"); r.ubicacionId = nullptr;
    auto fallo = m.evaluar(r, f);
    EXIGIR(!fallo.aplicado && (fallo.banderas & CAPACIDAD_AGOTADA));
    EXIGIR(std::strcmp(r.idReporte, "PENDIENTE") == 0);
    EXIGIR(f.cantidadOperaciones() == 64 && f.cantidadIncidentes() == 64);
    r.idReporte = "LLENO-0";
    EXIGIR(m.evaluar(r, f).tipo == TipoIngreso::REINTENTO_MISMO_ENVIO);
    // Limite de IDs: no incrementar un agregado si la operacion no cabe.
    FuenteMemoria<2, 1> ids;
    auto a = m.evaluar(base("ID-A"), ids);
    r = base("ID-B"); confirmar(r, a.incidenteId);
    fallo = m.evaluar(r, ids);
    EXIGIR(!fallo.aplicado && (fallo.banderas & CAPACIDAD_AGOTADA));
    EXIGIR(obtener(ids, a.incidenteId).cantidadReportes == 1);
    EXIGIR(obtener(ids, a.incidenteId).cantidadPersonasDeclaradas == 3);
    // Limite de lista de reportes: no resumen incompleto silencioso.
    FuenteMemoria<2, 128> lista;
    a = m.evaluar(base("LISTA-0"), lista);
    for (unsigned n = 1; n <= MAX_REPORTES_INCIDENTE; ++n) {
        std::snprintf(id, sizeof(id), "LISTA-%u", n);
        r = base(id); confirmar(r, a.incidenteId);
        const auto res = m.evaluar(r, lista);
        if (n < MAX_REPORTES_INCIDENTE) EXIGIR(res.aplicado);
        else EXIGIR(!res.aplicado && (res.banderas & CAPACIDAD_AGOTADA));
    }
    EXIGIR(obtener(lista, a.incidenteId).cantidadIds == MAX_REPORTES_INCIDENTE);
    EXIGIR(lista.cantidadOperaciones() == MAX_REPORTES_INCIDENTE);
}

static IncidenteAgrupado ejemplo_json() {
    FuenteMemoria<2, 8> f;
    MotorTriaje m;
    auto r = base("JSON-1");
    r.zona = "Zona \"A\"\\B\n\t\r\b\f\x01";
    r.razones[0] = "Revisión: \"sí\"";
    auto res = m.evaluar(r, f);
    EXIGIR(res.aplicado);
    return obtener(f, res.incidenteId);
}
static void cadenas_y_json() {
    FuenteMemoria<4, 8> f;
    MotorTriaje m;
    char largo[512]; std::memset(largo, 'x', sizeof(largo)); largo[511] = '\0';
    auto r = base("LARGO"); r.zona = largo; r.razones[0] = largo;
    auto res = m.evaluar(r, f);
    EXIGIR(res.aplicado && (res.banderas & TEXTO_TRUNCADO));
    auto inc = obtener(f, res.incidenteId);
    EXIGIR(std::strlen(inc.zona) == MAX_ZONA && std::strlen(inc.razones[0]) == MAX_RAZON);
    EXIGIR(inc.cantidadReportes == 1);
    r.idReporte = largo;
    res = m.evaluar(r, f);
    EXIGIR(!res.aplicado && (res.banderas & DATOS_INVALIDOS));
    EXIGIR(f.cantidadOperaciones() == 1); // no colision por prefijos truncados
    char utf8[MAX_ZONA + 4];
    std::memset(utf8, 'a', MAX_ZONA - 1);
    std::memcpy(utf8 + MAX_ZONA - 1, "\xc3\xb1x", 4);
    r = base("UTF8"); r.zona = utf8;
    res = m.evaluar(r, f);
    EXIGIR(res.aplicado && (res.banderas & TEXTO_TRUNCADO));
    EXIGIR(std::strlen(obtener(f, res.incidenteId).zona) == MAX_ZONA - 1);
    inc = ejemplo_json();
    char json[16000];
    auto salida = serializarJson(inc, json, sizeof(json));
    EXIGIR(salida.completo && salida.requeridos == std::strlen(json) + 1);
    EXIGIR(std::strstr(json, "\\\"A\\\"\\\\B\\u000a\\u0009\\u000d\\u0008\\u000c\\u0001"));
    EXIGIR(std::strstr(json, "\"cantidadPersonasDeclaradas\":3"));
    EXIGIR(serializarJson(inc, nullptr, 0).requeridos == salida.requeridos);
    struct Protegido { char antes; char buffer[8]; char despues; } guardia = {'A', {}, 'Z'};
    auto corto = serializarJson(inc, guardia.buffer, sizeof(guardia.buffer));
    EXIGIR(!corto.completo && guardia.buffer[0] == '\0');
    EXIGIR(guardia.antes == 'A' && guardia.despues == 'Z');
    EXIGIR(corto.requeridos == salida.requeridos);
    EXIGIR(serializarJson(inc, json, salida.requeridos).completo);
    EXIGIR(!serializarJson(inc, json, salida.requeridos - 1).completo && json[0] == '\0');
    inc.cantidadIds = MAX_REPORTES_INCIDENTE + 1;
    EXIGIR(!serializarJson(inc, json, sizeof(json)).completo);
}

// Simula un indice recuperado de disco y un error de escritura transaccional.
// No demuestra persistencia real ni estados del servidor actual.
class FuentePersistidaDoble final : public FuenteIncidentes {
public:
    RegistroOperacion recuperado;
    bool errorLectura = false;
    bool errorEstadisticas = false;
    mutable unsigned escrituras = 0;
    Consulta buscarOperacion(const char* id, const char*, RegistroOperacion& out) const override {
        if (errorLectura) return Consulta::ERROR;
        if (std::strcmp(id, recuperado.idReporte) != 0) return Consulta::AUSENTE;
        out = recuperado; return Consulta::ENCONTRADO;
    }
    Consulta buscarIncidente(const char*, IncidenteAgrupado&) const override { return Consulta::AUSENTE; }
    Consulta buscarCandidato(const ReporteEntrante&, std::uint64_t, IncidenteAgrupado&) const override { return Consulta::AUSENTE; }
    bool estadisticas(const ReporteEntrante&, std::uint64_t, Estadisticas& s) const override { s = {}; return !errorEstadisticas; }
    Escritura guardar(const ReporteEntrante&, const RegistroOperacion&, const IncidenteAgrupado&) override {
        ++escrituras; return Escritura::ERROR;
    }
};
static void claves_y_recuperacion() {
    FuenteMemoria<2, 8> f;
    MotorTriaje m;
    auto r = base("CLAVE-A"); r.idempotencyKey = "CLAVE-ENVIO";
    auto a = m.evaluar(r, f);
    r.idReporte = "OTRO-ID"; // misma operacion por clave estable
    auto b = m.evaluar(r, f);
    EXIGIR(b.tipo == TipoIngreso::REINTENTO_MISMO_ENVIO && b.aplicado);
    EXIGIR(std::strcmp(b.reporteId, a.reporteId) == 0 && f.cantidadOperaciones() == 1);
    r = base("CLAVE-B"); r.idempotencyKey = "SEGUNDA-CLAVE";
    EXIGIR(m.evaluar(r, f).aplicado);
    r.idReporte = "CLAVE-A"; // ID y clave apuntan a operaciones diferentes.
    auto cruzado = m.evaluar(r, f);
    EXIGIR(!cruzado.aplicado && (cruzado.banderas & ERROR_FUENTE));
    EXIGIR(f.cantidadOperaciones() == 2);
    FuentePersistidaDoble disco;
    EXIGIR(f.buscarOperacion("CLAVE-A", nullptr, disco.recuperado) == Consulta::ENCONTRADO);
    std::snprintf(disco.recuperado.resultado.estadoOriginal, MAX_ESTADO + 1, "estado-ficticio-original");
    MotorTriaje reiniciado;
    r = base("CLAVE-A"); r.idempotencyKey = "CLAVE-ENVIO";
    auto retry = reiniciado.evaluar(r, disco);
    EXIGIR(retry.tipo == TipoIngreso::REINTENTO_MISMO_ENVIO && disco.escrituras == 0);
    EXIGIR(std::strcmp(retry.estadoOriginal, "estado-ficticio-original") == 0);
    EXIGIR(std::strcmp(retry.incidenteId, a.incidenteId) == 0);
    disco.errorLectura = true;
    auto error = m.evaluar(base("NUEVO"), disco);
    EXIGIR(!error.aplicado && (error.banderas & ERROR_FUENTE) && disco.escrituras == 0);
    disco.errorLectura = false; disco.errorEstadisticas = true;
    error = m.evaluar(base("NUEVO"), disco);
    EXIGIR(!error.aplicado && (error.banderas & ERROR_FUENTE) && disco.escrituras == 0);
    disco.errorEstadisticas = false;
    error = m.evaluar(base("NUEVO"), disco);
    EXIGIR(!error.aplicado && (error.banderas & ERROR_FUENTE) && disco.escrituras == 1);
}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--json") == 0) {
        const auto inc = ejemplo_json(); char json[16000];
        EXIGIR(serializarJson(inc, json, sizeof(json)).completo);
        std::puts(json); return 0;
    }
    familias_y_reintento();
    candidatos_no_duplicados();
    rafaga_y_alerta_sospechosa();
    desconocida_y_personas();
    borde_ventana_y_maximo();
    capacidad();
    cadenas_y_json();
    claves_y_recuperacion();
    std::printf("Casos de triaje completados. Bytes: Incidente=%zu Registro=%zu Motor=%zu FuenteMemoria=%zu\n",
                sizeof(IncidenteAgrupado), sizeof(RegistroOperacion), sizeof(MotorTriaje), sizeof(FuenteMemoria<>));
}
