#include "Servidor.h"
#include <stdlib.h>
#include <string.h>

namespace {
constexpr size_t MAX_BODY_JSON = 2048;

struct CuerpoHttpBuffer {
    char* data;
    size_t len;
    size_t cap;
    bool desbordado;
};
} // namespace

ServidorHotspot::ServidorHotspot()
    : _apIP(192, 168, 4, 1),
      _netMsk(255, 255, 255, 0),
      _server(80) {}

void ServidorHotspot::iniciar(uint8_t canalWiFi) {
    _canalWiFi = canalWiFi;

    // 1. Montar sistema de archivos LittleFS (donde viviran los archivos del frontend)
    if (!LittleFS.begin(true)) {
        Serial.println("[ERROR] Fallo al montar LittleFS");
    } else {
        Serial.println("[OK] LittleFS montado correctamente");
    }

    // 2. Encender Wi-Fi en modo dual AP+STA (canal fijo para ESP-NOW) + DHCP + DNS Hijacking
    configurarRed();

    // 3. Configurar endpoints HTTP y redireccion de portal cautivo
    configurarRutas();

    // 4. Arrancar servidor web asincrono
    _server.begin();
    Serial.println("[OK] Servidor HTTP Asincrono escuchando en http://192.168.4.1");
}

void ServidorHotspot::procesarDNS() {
    // Debe ejecutarse continuamente en el loop() para responder consultas DNS
    _dnsServer.processNextRequest();
}

void ServidorHotspot::setCallbackReporte(CallbackReporte cb) {
    _onReporte = cb;
}

void ServidorHotspot::configurarRed() {
    // Modo dual: SoftAP para celulares + STA en el mismo canal para ESP-NOW
    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect(false, true); // Evita escaneos STA que cambien el canal de radio
    WiFi.softAPConfig(_apIP, _apIP, _netMsk);
    WiFi.softAP(_ssid, nullptr, _canalWiFi, 0, 8);

    _dnsServer.setTTL(60);
    _dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    _dnsServer.start(_puertoDNS, "*", _apIP);

    Serial.print("[OK] Hotspot activo (AP+STA): ");
    Serial.print(_ssid);
    Serial.print(" | Canal: ");
    Serial.println(_canalWiFi);
    Serial.print("[OK] IP de la estacion: ");
    Serial.println(WiFi.softAPIP());
}

void ServidorHotspot::configurarRutas() {
    // 1. Pagina principal de la estacion (servira /index.html desde LittleFS cuando se cargue el frontend)
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (LittleFS.exists("/index.html")) {
            request->send(LittleFS, "/index.html", "text/html");
        } else {
            request->send(200, "text/html; charset=utf-8",
                "<!DOCTYPE html><html><head><meta charset='utf-8'>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<title>Punto de Ayuda Conecta</title></head>"
                "<body style='font-family:sans-serif;text-align:center;padding:40px;'>"
                "<h1>Punto de Ayuda Conecta</h1>"
                "<p>Portal Cautivo Activo (Modo Offline-First + ESP-NOW)</p>"
                "</body></html>");
        }
    });

    // 2. Redireccion estandar 302 (el navegador del celular exige saltar hacia 192.168.4.1)
    auto forzarRedireccion = [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse(302, "text/plain", "");
        response->addHeader("Location", "http://192.168.4.1/");
        response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
        response->addHeader("Pragma", "no-cache");
        request->send(response);
    };

    // Sondas oficiales de Android, iOS, macOS y Windows
    _server.on("/generate_204", HTTP_GET, forzarRedireccion);
    _server.on("/gen_204", HTTP_GET, forzarRedireccion);
    _server.on("/hotspot-detect.html", HTTP_GET, forzarRedireccion);
    _server.on("/library/test/success.html", HTTP_GET, forzarRedireccion);
    _server.on("/ncsi.txt", HTTP_GET, forzarRedireccion);
    _server.on("/connecttest.txt", HTTP_GET, forzarRedireccion);
    _server.on("/redirect", HTTP_GET, forzarRedireccion);

    // 3. Archivos estaticos de LittleFS (aqui entraran CSS/JS del frontend de los companeros)
    _server.serveStatic("/", LittleFS, "/");

    // 4. Endpoint POST para recibir reportes JSON y despacharlos a ESP-NOW
    _server.on(
        "/api/reporte",
        HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            if (request->_tempObject == nullptr) {
                request->send(400, "application/json", "{\"status\":\"error\",\"mensaje\":\"Cuerpo vacio\"}");
                return;
            }

            auto* buf = reinterpret_cast<CuerpoHttpBuffer*>(request->_tempObject);
            if (buf->desbordado || buf->data == nullptr) {
                free(buf->data);
                free(buf);
                request->_tempObject = nullptr;
                request->send(413, "application/json", "{\"status\":\"error\",\"mensaje\":\"Payload demasiado grande\"}");
                return;
            }

            buf->data[buf->len] = '\0';

#if ARDUINOJSON_VERSION_MAJOR >= 7
            JsonDocument doc;
#else
            StaticJsonDocument<512> doc;
#endif
            DeserializationError err = deserializeJson(doc, buf->data, buf->len);
            free(buf->data);
            free(buf);
            request->_tempObject = nullptr;

            if (err) {
                request->send(400, "application/json", "{\"status\":\"error\",\"mensaje\":\"JSON invalido\"}");
                return;
            }

            PaqueteConecta pkt = {};
            pkt.tipo            = MSG_REPORTE;
            strncpy(pkt.estacion, ID_ESTACION, sizeof(pkt.estacion) - 1);
            pkt.barrioId        = doc["barrioId"] | 0;
            pkt.categoria       = doc["categoria"] | CAT_NECESIDADES;
            pkt.urgencia        = doc["urgencia"] | 2;
            pkt.afectados       = doc["afectados"] | 1;
            pkt.confirmaciones  = doc["confirmaciones"] | 1;
            pkt.necesidadesMask = doc["necesidadesMask"] | 0;
            pkt.bateriaPct      = doc["bateriaPct"] | 100;
            pkt.saltos          = 0;
            pkt.ts              = doc["ts"] | (uint32_t)(millis() / 1000);
            pkt.hashDispositivo = doc["hashDispositivo"] | 0U;
            pkt.codigo          = doc["codigo"] | (uint16_t)((millis() % 900) + 100);

            const char* ref = doc["referencia"] | "";
            strncpy(pkt.referencia, ref, sizeof(pkt.referencia) - 1);

            bool encolado = (_onReporte != nullptr) ? _onReporte(pkt) : false;

            char respuesta[128];
            snprintf(
                respuesta,
                sizeof(respuesta),
                "{\"status\":\"%s\",\"estacion\":\"%s\",\"codigo\":%u}",
                encolado ? "encolado_espnow" : "recibido_sin_enlace",
                pkt.estacion,
                pkt.codigo
            );
            request->send(encolado ? 200 : 503, "application/json", respuesta);
        },
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) {
                auto* buf = reinterpret_cast<CuerpoHttpBuffer*>(calloc(1, sizeof(CuerpoHttpBuffer)));
                if (buf == nullptr) return;
                if (total > MAX_BODY_JSON) {
                    buf->desbordado = true;
                } else {
                    buf->cap = total;
                    buf->data = reinterpret_cast<char*>(malloc(total + 1));
                    if (buf->data == nullptr) {
                        buf->desbordado = true;
                    }
                }
                request->_tempObject = buf;
                request->onDisconnect([request]() {
                    if (request->_tempObject != nullptr) {
                        auto* b = reinterpret_cast<CuerpoHttpBuffer*>(request->_tempObject);
                        free(b->data);
                        free(b);
                        request->_tempObject = nullptr;
                    }
                });
            }

            if (request->_tempObject == nullptr) return;
            auto* buf = reinterpret_cast<CuerpoHttpBuffer*>(request->_tempObject);
            if (buf->desbordado || buf->data == nullptr) return;

            if (index + len > buf->cap) {
                buf->desbordado = true;
                return;
            }
            memcpy(buf->data + index, data, len);
            buf->len = index + len;
        }
    );

    // 5. Cualquier otra peticion desconocida redirige al portal
    _server.onNotFound([forzarRedireccion](AsyncWebServerRequest* request) {
        // Si ya esta pidiendo la raiz o un archivo local, no entrar en bucle
        if (request->host() == "192.168.4.1") {
            request->send(404, "text/plain", "Archivo no encontrado");
        } else {
            forzarRedireccion(request);
        }
    });
}