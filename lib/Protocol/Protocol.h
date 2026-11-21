#pragma once

#include <stdint.h>

// Tipos de mensaje alineados al Protocolo Conecta v1 (sin datos personales - Ley 1581)
enum TipoMensaje : uint8_t {
    MSG_REPORTE    = 0x01,
    MSG_TELEMETRIA = 0x02,
    MSG_ENTREGA    = 0x03,
    MSG_LATIDO     = 0x04  // Estado de torre en linea para el mapa del PMU
};

// Categorias cerradas de emergencia
enum CategoriaEmergencia : uint8_t {
    CAT_ATRAPADAS     = 1,
    CAT_DESLIZAMIENTO = 2,
    CAT_COLAPSO       = 3,
    CAT_HERIDOS       = 4,
    CAT_GAS_ELECTRICO = 5,
    CAT_NECESIDADES   = 6
};

// Estructura empaquetada para transmision directa por ESP-NOW (limite v1: 250 bytes)
struct __attribute__((packed)) PaqueteConecta {
    uint8_t  tipo;            // TipoMensaje
    char     estacion[4];     // Ej: "E01\0"
    uint16_t barrioId;        // ID del catalogo de lugares (0 = por verificar)
    uint8_t  categoria;       // CategoriaEmergencia
    uint8_t  urgencia;        // 1=Medio, 2=Alto, 3=Critico
    uint8_t  afectados;       // Cantidad aproximada de personas
    uint8_t  confirmaciones;  // Conteo de reportes agrupados en la estacion
    uint8_t  necesidadesMask; // bit0=agua, bit1=alimentos, bit2=refugio, bit3=salud
    uint8_t  bateriaPct;      // 0-100% (telemetria / latido)
    uint8_t  saltos;          // 0=origen, 1+=repetido
    uint32_t ts;              // Segundos desde arranque o timestamp epoch
    uint32_t hashDispositivo; // Identificador seudonimizado (nunca cedula/nombre)
    uint16_t codigo;          // Codigo numerico corto (ej: 42 -> MZL-042)
    char     referencia[32];  // Referencia corta opcional sin datos personales
};

static_assert(sizeof(PaqueteConecta) <= 250, "PaqueteConecta excede el limite de 250 bytes de ESP-NOW");
