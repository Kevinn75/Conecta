#include <Arduino.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "Config.h"
#include "Protocol.h"
#include "EspNowLink.h"

#if !defined(ROL_ESTACION) && !defined(ROL_CONCENTRADOR)
#define ROL_ESTACION
#endif

#ifdef ROL_ESTACION
#include <LittleFS.h>
#include "Servidor.h"

ServidorHotspot servidor;
QueueHandle_t colaTxReportes = nullptr;
unsigned long ultimoLatido = 0;

// --- Red mallada ESP-NOW (inundacion controlada con deduplicacion y backoff no bloqueante) ---
constexpr size_t MAX_HUELLAS_MALLA = 48;
constexpr size_t MAX_COLA_REENVIO  = 8;

uint32_t huellasMalla[MAX_HUELLAS_MALLA] = {0};
size_t indiceHuellaMalla = 0;

struct ReenvioPendiente {
    bool activo;
    unsigned long enviarEnMs;
    PaqueteConecta paquete;
};
ReenvioPendiente colaReenvio[MAX_COLA_REENVIO] = {};

uint32_t calcularHuellaPaquete(const PaqueteConecta& pkt) {
    // Huella FNV-1a excluyendo 'saltos' para detectar ecos del mismo paquete repetido
    uint32_t h = 2166136261u;
    auto mezclar = [&h](uint8_t b) {
        h ^= b;
        h *= 16777619u;
    };
    mezclar(pkt.tipo);
    for (size_t i = 0; i < sizeof(pkt.estacion); ++i) mezclar(static_cast<uint8_t>(pkt.estacion[i]));
    mezclar(static_cast<uint8_t>(pkt.barrioId & 0xFF));
    mezclar(static_cast<uint8_t>((pkt.barrioId >> 8) & 0xFF));
    mezclar(pkt.categoria);
    mezclar(pkt.confirmaciones);
    mezclar(static_cast<uint8_t>(pkt.codigo & 0xFF));
    mezclar(static_cast<uint8_t>((pkt.codigo >> 8) & 0xFF));
    mezclar(static_cast<uint8_t>(pkt.ts & 0xFF));
    mezclar(static_cast<uint8_t>((pkt.ts >> 8) & 0xFF));
    mezclar(static_cast<uint8_t>((pkt.ts >> 16) & 0xFF));
    mezclar(static_cast<uint8_t>((pkt.ts >> 24) & 0xFF));
    return (h == 0) ? 1u : h;
}

bool huellaYaVista(uint32_t huella) {
    for (size_t i = 0; i < MAX_HUELLAS_MALLA; ++i) {
        if (huellasMalla[i] == huella) return true;
    }
    return false;
}

void registrarHuella(uint32_t huella) {
    huellasMalla[indiceHuellaMalla] = huella;
    indiceHuellaMalla = (indiceHuellaMalla + 1) % MAX_HUELLAS_MALLA;
}

void agendarReenvioMalla(const PaqueteConecta& pkt) {
    unsigned long ahora = millis();
    // Pequeño jitter aleatorio (15-65 ms) para evitar colisiones si 2 estaciones repiten a la vez
    unsigned long jitterMs = static_cast<unsigned long>(random(15, 65));
    for (size_t i = 0; i < MAX_COLA_REENVIO; ++i) {
        if (!colaReenvio[i].activo) {
            colaReenvio[i].activo = true;
            colaReenvio[i].enviarEnMs = ahora + jitterMs;
            colaReenvio[i].paquete = pkt;
            return;
        }
    }
}

void guardarReporteEnFlash(const PaqueteConecta& pkt) {
    File f = LittleFS.open("/reportes.jsonl", FILE_APPEND);
    if (!f) return;

#if ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument doc;
#else
    StaticJsonDocument<512> doc;
#endif
    doc["estacion"]        = pkt.estacion;
    doc["codigo"]          = pkt.codigo;
    doc["categoria"]       = pkt.categoria;
    doc["urgencia"]        = pkt.urgencia;
    doc["afectados"]       = pkt.afectados;
    doc["necesidadesMask"] = pkt.necesidadesMask;
    doc["haceTiempo"]      = pkt.haceTiempo;
    doc["ubicacion"]       = pkt.ubicacion;
    doc["referencia"]      = pkt.referencia;
    doc["ts"]              = pkt.ts;

    serializeJson(doc, f);
    f.println();
    f.close();
}
#endif

EspNowLink enlace;

void setup() {
    Serial.begin(115200);
    delay(500);
    randomSeed(esp_random());

#ifdef ROL_ESTACION
    colaTxReportes = xQueueCreate(16, sizeof(PaqueteConecta));

    // 1. Configurar Hotspot + Portal Cautivo en modo WIFI_AP_STA con canal fijo
    servidor.iniciar(CANAL_ESPNOW);

    // 2. Conectar POST /api/reporte hacia una cola segura para persistir y enviar por ESP-NOW en loop()
    servidor.setCallbackReporte([](const PaqueteConecta& pkt) -> bool {
        if (colaTxReportes == nullptr) return false;
        return (xQueueSend(colaTxReportes, &pkt, 0) == pdTRUE);
    });

    // 3. Iniciar ESP-NOW despues del SoftAP sobre el mismo canal
    enlace.iniciar(CANAL_ESPNOW, MAC_CONCENTRADOR);
    Serial.printf(
        "[ROL] ESTACION %s activa (Hotspot + Portal + LittleFS + Malla ESP-NOW max %u saltos)\n",
        ID_ESTACION,
        MAX_SALTOS_MALLA
    );
#endif

#ifdef ROL_CONCENTRADOR
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, true);
    enlace.iniciar(CANAL_ESPNOW);
    Serial.println("[ROL] CONCENTRADOR PMU escuchando red mallada ESP-NOW...");
#endif
}

void loop() {
#ifdef ROL_ESTACION
    servidor.procesarDNS();

    // 1. Persistir en Flash (Store-and-Forward) y despachar reportes locales por ESP-NOW
    PaqueteConecta reportePendiente;
    while (colaTxReportes != nullptr && xQueueReceive(colaTxReportes, &reportePendiente, 0) == pdTRUE) {
        guardarReporteEnFlash(reportePendiente);
        registrarHuella(calcularHuellaPaquete(reportePendiente));
        bool ok = enlace.enviar(reportePendiente);
        Serial.printf(
            "[ESTACION] Reporte MZL-%03u | ubic=%s | cat=%u | urg=%u | pers=%u -> ESP-NOW=%s\n",
            reportePendiente.codigo,
            reportePendiente.ubicacion,
            reportePendiente.categoria,
            reportePendiente.urgencia,
            reportePendiente.afectados,
            ok ? "OK" : "ERROR"
        );
    }

    // 2. Escuchar paquetes de OTRAS estaciones y agendarlos para reenvio en malla (repetidor)
    MensajeRecibido recibido;
    while (enlace.hayMensaje(recibido)) {
        PaqueteConecta& pkt = recibido.paquete;
        pkt.estacion[sizeof(pkt.estacion) - 1] = '\0';

        // Nunca repetir paquetes propios ni exceder MAX_SALTOS_MALLA
        if (strncmp(pkt.estacion, ID_ESTACION, sizeof(pkt.estacion)) == 0) {
            continue;
        }
        if (pkt.saltos >= MAX_SALTOS_MALLA) {
            continue;
        }

        uint32_t huella = calcularHuellaPaquete(pkt);
        if (huellaYaVista(huella)) {
            continue;
        }
        registrarHuella(huella);

        pkt.saltos++;
        agendarReenvioMalla(pkt);
    }

    // 3. Despachar reenvios de malla cuando venza su backoff aleatorio (no bloqueante)
    unsigned long ahoraMs = millis();
    for (size_t i = 0; i < MAX_COLA_REENVIO; ++i) {
        if (!colaReenvio[i].activo) continue;
        if (static_cast<long>(ahoraMs - colaReenvio[i].enviarEnMs) < 0) continue;

        const PaqueteConecta& p = colaReenvio[i].paquete;
        bool ok = enlace.enviar(p);
        if (p.tipo == MSG_REPORTE) {
            Serial.printf(
                "[MALLA] Reenviado REPORTE MZL-%03u de %s (salto %u/%u) -> %s\n",
                p.codigo,
                p.estacion,
                p.saltos,
                MAX_SALTOS_MALLA,
                ok ? "OK" : "ERROR"
            );
        } else {
            Serial.printf(
                "[MALLA] Reenviado latido de %s (salto %u/%u) -> %s\n",
                p.estacion,
                p.saltos,
                MAX_SALTOS_MALLA,
                ok ? "OK" : "ERROR"
            );
        }
        colaReenvio[i].activo = false;
    }

    // 4. Enviar latido periodico propio al PMU (y a la malla)
    if (millis() - ultimoLatido >= INTERVALO_LATIDO_MS) {
        ultimoLatido = millis();

        PaqueteConecta latido = {};
        latido.tipo       = MSG_LATIDO;
        snprintf(latido.estacion, sizeof(latido.estacion), "%s", ID_ESTACION);
        latido.bateriaPct = 100;
        latido.saltos     = 0;
        latido.ts         = millis() / 1000;

        registrarHuella(calcularHuellaPaquete(latido));
        bool ok = enlace.enviar(latido);
        Serial.printf(
            "[VIVO] Estacion=%s | Celulares=%d | Latido ESP-NOW=%s\n",
            ID_ESTACION,
            WiFi.softAPgetStationNum(),
            ok ? "OK" : "ERROR"
        );
    }
#endif

#ifdef ROL_CONCENTRADOR
    MensajeRecibido msg;
    while (enlace.hayMensaje(msg)) {
        msg.paquete.estacion[sizeof(msg.paquete.estacion) - 1]     = '\0';
        msg.paquete.haceTiempo[sizeof(msg.paquete.haceTiempo) - 1] = '\0';
        msg.paquete.ubicacion[sizeof(msg.paquete.ubicacion) - 1]   = '\0';
        msg.paquete.referencia[sizeof(msg.paquete.referencia) - 1] = '\0';

        char macStr[18];
        snprintf(
            macStr,
            sizeof(macStr),
            "%02X:%02X:%02X:%02X:%02X:%02X",
            msg.macOrigen[0], msg.macOrigen[1], msg.macOrigen[2],
            msg.macOrigen[3], msg.macOrigen[4], msg.macOrigen[5]
        );

#if ARDUINOJSON_VERSION_MAJOR >= 7
        JsonDocument doc;
#else
        StaticJsonDocument<512> doc;
#endif
        doc["source"]          = "esp";
        doc["mac"]             = macStr;
        doc["tower"]           = msg.paquete.estacion;
        doc["tipo"]            = msg.paquete.tipo;
        doc["barrioId"]        = msg.paquete.barrioId;
        doc["categoria"]       = msg.paquete.categoria;
        doc["urgencia"]        = msg.paquete.urgencia;
        doc["afectados"]       = msg.paquete.afectados;
        doc["confirmaciones"]  = msg.paquete.confirmaciones;
        doc["necesidadesMask"] = msg.paquete.necesidadesMask;
        doc["bateriaPct"]      = msg.paquete.bateriaPct;
        doc["saltos"]          = msg.paquete.saltos;
        doc["ts"]              = msg.paquete.ts;
        doc["hashDispositivo"] = msg.paquete.hashDispositivo;
        doc["codigo"]          = msg.paquete.codigo;
        doc["haceTiempo"]      = msg.paquete.haceTiempo;
        doc["ubicacion"]       = msg.paquete.ubicacion;
        doc["referencia"]      = msg.paquete.referencia;

        serializeJson(doc, Serial);
        Serial.println();
    }
#endif
}
