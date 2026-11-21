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
    Serial.println("[ROL] ESTACION activa (Hotspot + Portal Cautivo + LittleFS + ESP-NOW)");
#endif

#ifdef ROL_CONCENTRADOR
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, true);
    enlace.iniciar(CANAL_ESPNOW);
    Serial.println("[ROL] CONCENTRADOR PMU escuchando paquetes ESP-NOW...");
#endif
}

void loop() {
#ifdef ROL_ESTACION
    servidor.procesarDNS();

    // Persistir en Flash (Store-and-Forward) y despachar por ESP-NOW hacia el Concentrador PMU
    PaqueteConecta reportePendiente;
    while (colaTxReportes != nullptr && xQueueReceive(colaTxReportes, &reportePendiente, 0) == pdTRUE) {
        guardarReporteEnFlash(reportePendiente);
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

    // Enviar latido periodico al PMU y registrar estado por Serial
    if (millis() - ultimoLatido >= INTERVALO_LATIDO_MS) {
        ultimoLatido = millis();

        PaqueteConecta latido = {};
        latido.tipo       = MSG_LATIDO;
        snprintf(latido.estacion, sizeof(latido.estacion), "%s", ID_ESTACION);
        latido.bateriaPct = 100;
        latido.ts         = millis() / 1000;

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