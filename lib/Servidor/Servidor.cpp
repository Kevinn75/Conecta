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

void copiarCadenaSegura(char* destino, size_t tamDestino, const char* origen) {
    if (destino == nullptr || tamDestino == 0) return;
    if (origen == nullptr) {
        destino[0] = '\0';
        return;
    }
    strncpy(destino, origen, tamDestino - 1);
    destino[tamDestino - 1] = '\0';
}
} // namespace

ServidorHotspot::ServidorHotspot()
    : _apIP(192, 168, 4, 1),
      _netMsk(255, 255, 255, 0),
      _server(80) {}

uint32_t ServidorHotspot::hashFnv1a(const char* texto) {
    uint32_t h = 2166136261u;
    if (texto == nullptr) return 0;
    while (*texto) {
        h ^= static_cast<uint8_t>(*texto++);
        h *= 16777619u;
    }
    return h;
}

bool ServidorHotspot::buscarDuplicado(uint32_t hashId, uint16_t& codigoExistente) const {
    if (hashId == 0) return false;
    for (size_t i = 0; i < HISTORIAL_IDEMPOTENCIA; ++i) {
        if (_ultimosHashes[i] == hashId) {
            codigoExistente = _ultimosCodigos[i];
            return true;
        }
    }
    return false;
}

void ServidorHotspot::registrarHash(uint32_t hashId, uint16_t codigo) {
    if (hashId == 0) return;
    _ultimosHashes[_indiceHistorial] = hashId;
    _ultimosCodigos[_indiceHistorial] = codigo;
    _indiceHistorial = (_indiceHistorial + 1) % HISTORIAL_IDEMPOTENCIA;
}

void ServidorHotspot::iniciar(uint8_t canalWiFi) {
    _canalWiFi = canalWiFi;

    // 1. Montar sistema de archivos LittleFS (donde viven index.html, styles.css, app.js y reportes.jsonl)
    if (!LittleFS.begin(true)) {
        Serial.println("[ERROR] Fallo al montar LittleFS");
    } else {
        Serial.println("[OK] LittleFS montado correctamente");
        if (LittleFS.exists("/index.html")) {
            Serial.println("[OK] Frontend ciudadano encontrado en LittleFS (/index.html)");
        } else {
            Serial.println("[AVISO] /index.html no esta en LittleFS. Ejecuta 'Upload Filesystem Image'.");
        }
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
    // 1. Pagina principal de la estacion (sirve /index.html desde LittleFS)
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (LittleFS.exists("/index.html")) {
            request->send(LittleFS, "/index.html", "text/html; charset=utf-8");
        } else {
            request->send(200, "text/html; charset=utf-8",
                "<!DOCTYPE html><html><head><meta charset='utf-8'>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<title>Punto de Ayuda Conecta</title></head>"
                "<body style='font-family:sans-serif;text-align:center;padding:40px;'>"
                "<h1>Punto de Ayuda Conecta</h1>"
                "<p>Portal Cautivo Activo. Sube la imagen LittleFS (Upload Filesystem Image) para cargar el formulario.</p>"
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

    // 3. Estado de la estacion para el frontend ciudadano
    _server.on("/api/estado", HTTP_GET, [this](AsyncWebServerRequest* request) {
        char resp[160];
        snprintf(
            resp,
            sizeof(resp),
            "{\"estacion\":\"%s\",\"canal\":%u,\"clientes\":%d,\"totalReportes\":%lu}",
            ID_ESTACION,
            _canalWiFi,
            WiFi.softAPgetStationNum(),
            static_cast<unsigned long>(_totalReportes)
        );
        request->send(200, "application/json", resp);
    });

    // 4. Endpoint POST para recibir reportes JSON del formulario ciudadano y enviarlos por ESP-NOW
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
            StaticJsonDocument<1024> doc;
#endif
            DeserializationError err = deserializeJson(doc, buf->data, buf->len);
            free(buf->data);
            free(buf);
            request->_tempObject = nullptr;

            if (err) {
                request->send(400, "application/json", "{\"status\":\"error\",\"mensaje\":\"JSON invalido\"}");
                return;
            }

            // Idempotencia: usa id textual, hashDispositivo o codigo de 16 bits del frontend
            const char* idCliente = doc["id"] | "";
            uint16_t codigoEntrada = doc["codigo"] | 0;
            uint32_t hashId = 0;
            if (idCliente[0] != '\0') {
                hashId = hashFnv1a(idCliente);
            } else if ((doc["hashDispositivo"] | 0u) != 0u) {
                hashId = doc["hashDispositivo"] | 0u;
            } else if (codigoEntrada != 0) {
                hashId = 0xC0DE0000u | static_cast<uint32_t>(codigoEntrada);
            }

            uint16_t codigoExistente = 0;
            if (buscarDuplicado(hashId, codigoExistente)) {
                char dupResp[176];
                snprintf(
                    dupResp,
                    sizeof(dupResp),
                    "{\"status\":\"encolado_espnow\",\"duplicado\":true,\"estacion\":\"%s\",\"codigo\":%u,\"codigoTexto\":\"MZL-%03u\"}",
                    ID_ESTACION,
                    codigoExistente,
                    codigoExistente
                );
                request->send(200, "application/json", dupResp);
                return;
            }

            PaqueteConecta pkt = {};
            pkt.tipo            = MSG_REPORTE;
            copiarCadenaSegura(pkt.estacion, sizeof(pkt.estacion), ID_ESTACION);
            pkt.barrioId        = doc["barrioId"] | 0;
            pkt.confirmaciones  = doc["confirmaciones"] | 1;
            pkt.bateriaPct      = doc["bateriaPct"] | 100;
            pkt.saltos          = 0;
            pkt.ts              = doc["ts"] | static_cast<uint32_t>(millis() / 1000);
            pkt.hashDispositivo = hashId;

            // Personas afectadas: 0 significa desconocido ("personas s/d" en PMU), rango 0..255
            int personas = 0;
            if (!doc["afectados"].isNull()) {
                personas = doc["afectados"] | 0;
            } else if (!doc["personas_aproximadas"].isNull()) {
                personas = doc["personas_aproximadas"] | 0;
            }
            if (personas < 0) personas = 0;
            if (personas > 255) personas = 255;
            pkt.afectados = static_cast<uint8_t>(personas);

            // Tiempo aproximado desde inicio ("10", "15", "30", "mas_60") si viene en el JSON
            const char* tiempoStr = doc["hace_aproximadamente"] | "";
            copiarCadenaSegura(pkt.haceTiempo, sizeof(pkt.haceTiempo), tiempoStr);

            // Urgencia (3=critico, 2=alto, 1=medio)
            bool peligroInmediato = doc["peligro_inmediato"] | false;
            if (!doc["urgencia"].isNull()) {
                int u = doc["urgencia"] | 2;
                if (u < 1) u = 1;
                if (u > 3) u = 3;
                pkt.urgencia = static_cast<uint8_t>(u);
            } else if (peligroInmediato) {
                pkt.urgencia = 3;
            } else if (strcmp(tiempoStr, "mas_60") == 0 || strcmp(tiempoStr, "30") == 0) {
                pkt.urgencia = 2;
            } else {
                pkt.urgencia = 1;
            }

            // Soporta formato numerico del frontend final (tipo:1 + categoria + necesidadesMask)
            // y tambien formato textual previo (tipo:"agua"|"alimentos"|...)
            if (doc["tipo"].is<const char*>()) {
                const char* tipoForm = doc["tipo"] | "";
                if (strcmp(tipoForm, "agua") == 0) {
                    pkt.categoria = CAT_NECESIDADES;
                    pkt.necesidadesMask = (1 << 0);
                } else if (strcmp(tipoForm, "alimentos") == 0) {
                    pkt.categoria = CAT_NECESIDADES;
                    pkt.necesidadesMask = (1 << 1);
                } else if (strcmp(tipoForm, "albergue") == 0) {
                    pkt.categoria = CAT_NECESIDADES;
                    pkt.necesidadesMask = (1 << 2);
                } else if (strcmp(tipoForm, "salud") == 0) {
                    pkt.categoria = CAT_NECESIDADES;
                    pkt.necesidadesMask = (1 << 3);
                } else if (strcmp(tipoForm, "rescate") == 0) {
                    pkt.categoria = CAT_ATRAPADAS;
                    pkt.urgencia = 3;
                } else {
                    pkt.categoria = peligroInmediato ? CAT_GAS_ELECTRICO : CAT_NECESIDADES;
                }
            } else {
                pkt.categoria = doc["categoria"] | CAT_NECESIDADES;
                pkt.necesidadesMask = doc["necesidadesMask"] | 0;
            }

            // Ubicacion textual opcional y referencia/detalle (sin datos personales)
            const char* ubicacionStr = doc["ubicacion"] | "";
            const char* detalleStr = !doc["referencia"].isNull() ? (doc["referencia"] | "") : (doc["detalle"] | "");
            copiarCadenaSegura(pkt.ubicacion, sizeof(pkt.ubicacion), ubicacionStr);
            copiarCadenaSegura(pkt.referencia, sizeof(pkt.referencia), detalleStr);

            // Usa el codigo de 16 bits enviado por el frontend, o asigna uno secuencial
            if (codigoEntrada != 0) {
                pkt.codigo = codigoEntrada;
            } else {
                _siguienteCodigo++;
                if (_siguienteCodigo > 999) _siguienteCodigo = 100;
                pkt.codigo = _siguienteCodigo;
            }

            bool encolado = (_onReporte != nullptr) ? _onReporte(pkt) : false;
            if (encolado) {
                registrarHash(hashId, pkt.codigo);
                _totalReportes++;
            }

            char respuesta[176];
            snprintf(
                respuesta,
                sizeof(respuesta),
                "{\"status\":\"%s\",\"duplicado\":false,\"estacion\":\"%s\",\"codigo\":%u,\"codigoTexto\":\"MZL-%03u\"}",
                encolado ? "encolado_espnow" : "recibido_sin_enlace",
                pkt.estacion,
                pkt.codigo,
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

    // 5. Archivos estaticos de LittleFS (styles.css, app.js, etc.)
    _server.serveStatic("/", LittleFS, "/");

    // 6. Cualquier otra peticion desconocida redirige al portal
    _server.onNotFound([forzarRedireccion](AsyncWebServerRequest* request) {
        // Si ya esta pidiendo la raiz o un archivo local, no entrar en bucle
        if (request->host() == "192.168.4.1") {
            request->send(404, "text/plain", "Archivo no encontrado");
        } else {
            forzarRedireccion(request);
        }
    });
}