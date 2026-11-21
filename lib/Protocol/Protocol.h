#pragma once

#include <stdint.h>

// Tipos de mensaje alineados al Protocolo Conecta v1 (sin datos personales - Ley 1581)
enum TipoMensaje : uint8_t {
    MSG_REPORTE    = 0x01,
    MSG_TELEMETRIA = 0x02,
    MSG_ENTREGA    = 0x03,
    MSG_LATIDO     = 0x04  // Estado de torre en linea para el mapa del PMU
};

// Categorias cerradas de emergencia (alineadas con Protocolo v1 y tablero PMU)
enum CategoriaEmergencia : uint8_t {
    CAT_ATRAPADAS     = 1,
    CAT_DESLIZAMIENTO = 2,
    CAT_COLAPSO       = 3,
    CAT_HERIDOS       = 4,
    CAT_GAS_ELECTRICO = 5,
    CAT_NECESIDADES   = 6,
    CAT_TERREMOTO     = 7,
    CAT_VOLCANICA     = 8,
    CAT_DONACIONES    = 9,
    CAT_INVENTARIOS   = 10,
    CAT_ENTREGAS      = 11
};

// Estructura empaquetada para transmision directa por ESP-NOW (128 bytes <= limite 250 bytes)
struct __attribute__((packed)) PaqueteConecta {
    uint8_t  tipo;            // TipoMensaje
    char     estacion[4];     // Ej: "E01\0"
    uint16_t barrioId;        // ID numerico del catalogo (0 = usar texto en ubicacion[])
    uint8_t  categoria;       // CategoriaEmergencia
    uint8_t  urgencia;        // 1=Medio, 2=Alto, 3=Critico
    uint8_t  afectados;       // Cantidad aproximada de personas (0-255)
    uint8_t  confirmaciones;  // Conteo de reportes agrupados en la estacion
    uint8_t  necesidadesMask; // bit0=agua, bit1=alimentos, bit2=refugio, bit3=salud
    uint8_t  bateriaPct;      // 0-100% (telemetria / latido)
    uint8_t  saltos;          // 0=origen, 1+=repetido
    uint32_t ts;              // Segundos desde arranque o timestamp epoch
    uint32_t hashDispositivo; // Identificador seudonimizado / hash de idempotencia
    uint16_t codigo;          // Codigo numerico corto (ej: 142 -> MZL-142)
    char     haceTiempo[8];   // "10", "15", "30", "mas_60"
    char     ubicacion[32];   // Barrio, vereda o punto de referencia escrito
    char     referencia[64];  // Detalle corto opcional sin datos personales
};

static_assert(sizeof(PaqueteConecta) <= 250, "PaqueteConecta excede el limite de 250 bytes de ESP-NOW");
