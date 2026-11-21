#include "Servidor.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
#include <esp_netif.h>
#elif ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
#include <tcpip_adapter.h>
#endif

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

uint32_t hashTextoNormalizado(const char* texto) {
    uint32_t h = 2166136261u;
    if (texto == nullptr) return 0;
    bool tuvoCaracter = false;
    while (*texto) {
        unsigned char c = static_cast<unsigned char>(*texto++);
        if (isspace(c)) continue;
        h ^= static_cast<uint8_t>(tolower(c));
        h *= 16777619u;
        tuvoCaracter = true;
    }
    return tuvoCaracter ? (h | 0x80000000u) : 0u;
}
} // namespace

ServidorHotspot::ServidorHotspot()
    : _apIP(192, 168, 4, 1),
      _netMsk(255, 255, 255, 0),
      _server(80) {
    snprintf(_ssid, sizeof(_ssid), "punto_de_ayuda_conecta_%s", ID_ESTACION);
}

uint32_t ServidorHotspot::hashFnv1a(const char* texto) {
    uint32_t h = 2166136261u;
    if (texto == nullptr) return 0;
    while (*texto) {
        h ^= static_cast<uint8_t>(*texto++);
        h *= 16777619u;
    }
    return h;
}

uint32_t ServidorHotspot::calcularLlaveUbicacion(uint16_t barrioId, const char* ubicacion, const char* referencia) {
    if (barrioId > 0) {
        return static_cast<uint32_t>(barrioId);
    }
    if (ubicacion != nullptr && ubicacion[0] != '\0') {
        return hashTextoNormalizado(ubicacion);
    }
    if (referencia != nullptr && referencia[0] != '\0') {
        return hashTextoNormalizado(referencia);
    }
    // Zona desconocida sin referencia: 0 indica no agrupar automaticamente
    return 0;
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

bool ServidorHotspot::excedeRateLimitIp(uint32_t ip, unsigned long ahoraMs, bool esPeligroVital) {
    // Regla humanitaria / Habeas Data: peligro vital nunca se bloquea silenciosamente por rate limit
    if (ip == 0 || esPeligroVital) return false;

    for (size_t i = 0; i < MAX_CLIENTES_IP; ++i) {
        if (_clientesIp[i].ip == ip) {
            if (ahoraMs - _clientesIp[i].inicioVentanaMs >= VENTANA_RATE_LIMIT_MS) {
                _clientesIp[i].inicioVentanaMs = ahoraMs;
                _clientesIp[i].conteoVentana = 0;
                return false;
            }
            return (_clientesIp[i].conteoVentana >= MAX_REPORTES_POR_IP);
        }
    }
    return false;
}

void ServidorHotspot::registrarEnvioIp(uint32_t ip, unsigned long ahoraMs) {
    if (ip == 0) return;

    size_t libre = MAX_CLIENTES_IP;
    size_t masViejo = 0;
    unsigned long menorTiempo = 0xFFFFFFFFUL;

    for (size_t i = 0; i < MAX_CLIENTES_IP; ++i) {
        if (_clientesIp[i].ip == ip) {
            if (ahoraMs - _clientesIp[i].inicioVentanaMs >= VENTANA_RATE_LIMIT_MS) {
                _clientesIp[i].inicioVentanaMs = ahoraMs;
                _clientesIp[i].conteoVentana = 1;
            } else {
                _clientesIp[i].conteoVentana++;
            }
            return;
        }
        if (_clientesIp[i].ip == 0 && libre == MAX_CLIENTES_IP) {
            libre = i;
        }
        if (_clientesIp[i].inicioVentanaMs < menorTiempo) {
            menorTiempo = _clientesIp[i].inicioVentanaMs;
            masViejo = i;
        }
    }

    size_t idx = (libre < MAX_CLIENTES_IP) ? libre : masViejo;
    _clientesIp[idx].ip = ip;
    _clientesIp[idx].inicioVentanaMs = ahoraMs;
    _clientesIp[idx].conteoVentana = 1;
}

ServidorHotspot::IncidenteActivo* ServidorHotspot::buscarIncidenteActivo(
    uint32_t llaveUbic,
    uint8_t categoria,
    unsigned long ahoraMs
) {
    if (llaveUbic == 0) return nullptr;

    for (size_t i = 0; i < MAX_INCIDENTES_ACTIVOS; ++i) {
        if (!_incidentes[i].activo) continue;
        if (ahoraMs - _incidentes[i].ultimaActividadMs > VENTANA_CONSENSO_MS) {
            _incidentes[i].activo = false;
            continue;
        }
        if (_incidentes[i].llaveUbicacion == llaveUbic && _incidentes[i].categoria == categoria) {
            return &_incidentes[i];
        }
    }
    return nullptr;
}

ServidorHotspot::IncidenteActivo* ServidorHotspot::crearIncidenteActivo(
    const PaqueteConecta& pkt,
    uint32_t llaveUbic,
    uint32_t ip,
    unsigned long ahoraMs
) {
    if (llaveUbic == 0) return nullptr;

    size_t libre = MAX_INCIDENTES_ACTIVOS;
    size_t masViejo = 0;
    unsigned long menorTiempo = 0xFFFFFFFFUL;

    for (size_t i = 0; i < MAX_INCIDENTES_ACTIVOS; ++i) {
        if (!_incidentes[i].activo || (ahoraMs - _incidentes[i].ultimaActividadMs > VENTANA_CONSENSO_MS)) {
            if (libre == MAX_INCIDENTES_ACTIVOS) libre = i;
        }
        if (_incidentes[i].ultimaActividadMs < menorTiempo) {
            menorTiempo = _incidentes[i].ultimaActividadMs;
            masViejo = i;
        }
    }

    size_t idx = (libre < MAX_INCIDENTES_ACTIVOS) ? libre : masViejo;
    IncidenteActivo& inc = _incidentes[idx];
    inc = {};
    inc.activo            = true;
    inc.llaveUbicacion    = llaveUbic;
    inc.categoria         = pkt.categoria;
    inc.codigo            = pkt.codigo;
    inc.confirmaciones    = 1;
    inc.urgencia          = pkt.urgencia;
    inc.afectados         = pkt.afectados;
    inc.necesidadesMask   = pkt.necesidadesMask;
    inc.ultimaActividadMs = ahoraMs;
    agregarIpIncidente(inc, ip);
    return &inc;
}

bool ServidorHotspot::ipYaVoto(const IncidenteActivo& inc, uint32_t ip) {
    if (ip == 0) return false;
    for (uint8_t i = 0; i < inc.numIps && i < MAX_IPS_POR_INCIDENTE; ++i) {
        if (inc.ipsVotantes[i] == ip) return true;
    }
    return false;
}

void ServidorHotspot::agregarIpIncidente(IncidenteActivo& inc, uint32_t ip) {
    if (ip == 0 || ipYaVoto(inc, ip)) return;
    if (inc.numIps < MAX_IPS_POR_INCIDENTE) {
        inc.ipsVotantes[inc.numIps++] = ip;
    }
}

void ServidorHotspot::iniciar(uint8_t canalWiFi) {
    _canalWiFi = canalWiFi;

    // 1. Montar sistema de archivos LittleFS (donde viven index.html y reportes.jsonl)
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

    // Verifica cada 1 s turnos de 5 min y desconexiones programadas tras enviar reporte
    unsigned long ahoraMs = millis();
    if (ahoraMs - _ultimoChequeoSesionesMs >= 1000UL) {
        _ultimoChequeoSesionesMs = ahoraMs;
        gestionarSesionesWiFi();
    }
}

void ServidorHotspot::programarDesconexionPostReporte(uint32_t ipCliente, unsigned long ahoraMs) {
    if (ipCliente == 0) return;

    // Si ya hay una desconexion pendiente para esta IP, actualiza el temporizador
    for (size_t i = 0; i < MAX_SESIONES_WIFI; ++i) {
        if (_desconexiones[i].pendiente && _desconexiones[i].ip == ipCliente) {
            _desconexiones[i].ejecutarEnMs = ahoraMs + GRACIA_POST_REPORTE_MS;
            return;
        }
    }
    for (size_t i = 0; i < MAX_SESIONES_WIFI; ++i) {
        if (!_desconexiones[i].pendiente) {
            _desconexiones[i].ip = ipCliente;
            _desconexiones[i].ejecutarEnMs = ahoraMs + GRACIA_POST_REPORTE_MS;
            _desconexiones[i].pendiente = true;
            return;
        }
    }
}

bool ServidorHotspot::obtenerMacPorIp(uint32_t ipBuscada, const wifi_sta_list_t& staList, uint8_t macSalida[6]) {
    if (staList.num <= 0) return false;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (netif != nullptr) {
        for (int i = 0; i < staList.num; ++i) {
            esp_netif_pair_mac_ip_t pair = {};
            memcpy(pair.mac, staList.sta[i].mac, 6);
            if (esp_netif_dhcps_get_clients_by_mac(netif, 1, &pair) == ESP_OK &&
                pair.ip.addr == ipBuscada) {
                memcpy(macSalida, staList.sta[i].mac, 6);
                return true;
            }
        }
    }
#elif ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    tcpip_adapter_sta_list_t adapterList = {};
    if (tcpip_adapter_get_sta_list(&staList, &adapterList) == ESP_OK) {
        for (int i = 0; i < adapterList.num; ++i) {
            if (adapterList.sta[i].ip.addr == ipBuscada) {
                memcpy(macSalida, adapterList.sta[i].mac, 6);
                return true;
            }
        }
    }
#endif

    // Respaldo seguro si solo hay 1 dispositivo conectado al Hotspot
    if (staList.num == 1) {
        memcpy(macSalida, staList.sta[0].mac, 6);
        return true;
    }
    return false;
}

void ServidorHotspot::gestionarSesionesWiFi() {
    wifi_sta_list_t staList = {};
    if (esp_wifi_ap_get_sta_list(&staList) != ESP_OK) {
        return;
    }

    const unsigned long ahoraMs = millis();

    // 0. Ejecutar desconexiones automaticas de celulares que ya terminaron su reporte
    for (size_t i = 0; i < MAX_SESIONES_WIFI; ++i) {
        if (!_desconexiones[i].pendiente) continue;
        if (static_cast<long>(ahoraMs - _desconexiones[i].ejecutarEnMs) < 0) continue;

        uint8_t macObjetivo[6] = {0};
        if (obtenerMacPorIp(_desconexiones[i].ip, staList, macObjetivo)) {
            uint16_t aid = 0;
            if (esp_wifi_ap_get_sta_aid(macObjetivo, &aid) == ESP_OK && aid > 0) {
                esp_err_t res = esp_wifi_deauth_sta(aid);
                Serial.printf(
                    "[WIFI-TURNO] Reporte completado -> desconectando %02X:%02X:%02X:%02X:%02X:%02X (aid=%u) deauth=%s\n",
                    macObjetivo[0], macObjetivo[1], macObjetivo[2],
                    macObjetivo[3], macObjetivo[4], macObjetivo[5],
                    aid,
                    (res == ESP_OK) ? "OK" : "ERROR"
                );
            }
            for (size_t k = 0; k < MAX_SESIONES_WIFI; ++k) {
                if (_sesionesWiFi[k].activo && memcmp(_sesionesWiFi[k].mac, macObjetivo, 6) == 0) {
                    _sesionesWiFi[k].activo = false;
                }
            }
        }
        _desconexiones[i].pendiente = false;
    }

    // 1. Liberar entradas de dispositivos que ya se desconectaron por su cuenta
    for (size_t i = 0; i < MAX_SESIONES_WIFI; ++i) {
        if (!_sesionesWiFi[i].activo) continue;
        bool sigueConectado = false;
        for (int j = 0; j < staList.num; ++j) {
            if (memcmp(_sesionesWiFi[i].mac, staList.sta[j].mac, 6) == 0) {
                sigueConectado = true;
                break;
            }
        }
        if (!sigueConectado) {
            _sesionesWiFi[i].activo = false;
        }
    }

    // 2. Registrar nuevos clientes y desconectar a quienes superen DURACION_SESION_WIFI_MS (5 min)
    for (int j = 0; j < staList.num; ++j) {
        const uint8_t* mac = staList.sta[j].mac;
        int idxEncontrado = -1;
        int idxLibre = -1;

        for (size_t i = 0; i < MAX_SESIONES_WIFI; ++i) {
            if (_sesionesWiFi[i].activo && memcmp(_sesionesWiFi[i].mac, mac, 6) == 0) {
                idxEncontrado = static_cast<int>(i);
                break;
            }
            if (!_sesionesWiFi[i].activo && idxLibre < 0) {
                idxLibre = static_cast<int>(i);
            }
        }

        if (idxEncontrado < 0) {
            if (idxLibre >= 0) {
                _sesionesWiFi[idxLibre].activo = true;
                memcpy(_sesionesWiFi[idxLibre].mac, mac, 6);
                _sesionesWiFi[idxLibre].conectadoEnMs = ahoraMs;
                Serial.printf(
                    "[WIFI-TURNO] Dispositivo conectado %02X:%02X:%02X:%02X:%02X:%02X (limite %lu s)\n",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                    DURACION_SESION_WIFI_MS / 1000UL
                );
            }
            continue;
        }

        if (ahoraMs - _sesionesWiFi[idxEncontrado].conectadoEnMs >= DURACION_SESION_WIFI_MS) {
            uint16_t aid = 0;
            if (esp_wifi_ap_get_sta_aid(mac, &aid) == ESP_OK && aid > 0) {
                esp_err_t res = esp_wifi_deauth_sta(aid);
                Serial.printf(
                    "[WIFI-TURNO] Turno de 5 min vencido para %02X:%02X:%02X:%02X:%02X:%02X (aid=%u) -> deauth=%s\n",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                    aid,
                    (res == ESP_OK) ? "OK" : "ERROR"
                );
            }
            _sesionesWiFi[idxEncontrado].activo = false;
        }
    }
}

void ServidorHotspot::setCallbackReporte(CallbackReporte cb) {
    _onReporte = cb;
}

void ServidorHotspot::configurarRed() {
    // Limpia cualquier rastro de WIFI_PROTOCOL_LR guardado en la memoria NVS y fija 802.11 b/g/n estandar
    WiFi.persistent(true);
    WiFi.mode(WIFI_AP_STA);
    esp_wifi_set_protocol(WIFI_IF_AP,  WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    WiFi.persistent(false);

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

    // 3. Estado y metricas KPI de Edge Computing (deduplicacion + consenso)
    _server.on("/api/estado", HTTP_GET, [this](AsyncWebServerRequest* request) {
        uint32_t filtrados = _duplicadosBloqueados + _agrupadosConsenso;
        uint32_t redundantes = (_totalRecibidos > _incidentesUnicos) ? (_totalRecibidos - _incidentesUnicos) : 0;
        float tasa = (redundantes > 0) ? (100.0f * static_cast<float>(filtrados) / static_cast<float>(redundantes)) : 100.0f;
        if (tasa > 100.0f) tasa = 100.0f;

        char resp[280];
        snprintf(
            resp,
            sizeof(resp),
            "{\"estacion\":\"%s\",\"canal\":%u,\"clientes\":%d,\"recibidos\":%lu,"
            "\"incidentesUnicos\":%lu,\"agrupadosConsenso\":%lu,\"duplicadosBloqueados\":%lu,"
            "\"bloqueadosRateLimit\":%lu,\"tasaDeteccionPct\":%.1f}",
            ID_ESTACION,
            _canalWiFi,
            WiFi.softAPgetStationNum(),
            static_cast<unsigned long>(_totalRecibidos),
            static_cast<unsigned long>(_incidentesUnicos),
            static_cast<unsigned long>(_agrupadosConsenso),
            static_cast<unsigned long>(_duplicadosBloqueados),
            static_cast<unsigned long>(_bloqueadosRateLimit),
            tasa
        );
        request->send(200, "application/json", resp);
    });

    // 4. Endpoint POST con motor Edge: Idempotencia + Rate Limit IP + Consenso (3 llaves)
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

            const unsigned long ahoraMs = millis();
            const IPAddress ipObj = (request->client() != nullptr) ? request->client()->remoteIP() : IPAddress(0, 0, 0, 0);
            const uint32_t ipCliente = static_cast<uint32_t>(ipObj);

            _totalRecibidos++;

            // A) Idempotencia por id textual, hashDispositivo o codigo de 16 bits del frontend
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
                _duplicadosBloqueados++;
                Serial.printf(
                    "[EDGE-DEDUP] Reintento duplicado bloqueado (IP=%s | codigo=MZL-%03u) | Filtrados=%lu\n",
                    ipObj.toString().c_str(),
                    codigoExistente,
                    static_cast<unsigned long>(_duplicadosBloqueados)
                );
                char dupResp[192];
                snprintf(
                    dupResp,
                    sizeof(dupResp),
                    "{\"status\":\"encolado_espnow\",\"duplicado\":true,\"estacion\":\"%s\",\"codigo\":%u,\"codigoTexto\":\"MZL-%03u\"}",
                    ID_ESTACION,
                    codigoExistente,
                    codigoExistente
                );
                request->send(200, "application/json", dupResp);
                programarDesconexionPostReporte(ipCliente, ahoraMs);
                return;
            }

            // B) Parseo del paquete candidato
            PaqueteConecta pkt = {};
            pkt.tipo            = MSG_REPORTE;
            copiarCadenaSegura(pkt.estacion, sizeof(pkt.estacion), ID_ESTACION);
            pkt.barrioId        = doc["barrioId"] | 0;
            pkt.confirmaciones  = 1;
            pkt.bateriaPct      = doc["bateriaPct"] | 100;
            pkt.saltos          = 0;
            pkt.ts              = doc["ts"] | static_cast<uint32_t>(ahoraMs / 1000);
            pkt.hashDispositivo = hashId;

            int personas = 0;
            if (!doc["afectados"].isNull()) {
                personas = doc["afectados"] | 0;
            } else if (!doc["personas_aproximadas"].isNull()) {
                personas = doc["personas_aproximadas"] | 0;
            }
            if (personas < 0) personas = 0;
            if (personas > 255) personas = 255;
            pkt.afectados = static_cast<uint8_t>(personas);

            const char* tiempoStr = doc["hace_aproximadamente"] | "";
            copiarCadenaSegura(pkt.haceTiempo, sizeof(pkt.haceTiempo), tiempoStr);

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

            const char* ubicacionStr = doc["ubicacion"] | "";
            const char* detalleStr = !doc["referencia"].isNull() ? (doc["referencia"] | "") : (doc["detalle"] | "");
            copiarCadenaSegura(pkt.ubicacion, sizeof(pkt.ubicacion), ubicacionStr);
            copiarCadenaSegura(pkt.referencia, sizeof(pkt.referencia), detalleStr);

            const bool esPeligroVital = (pkt.urgencia >= 3 || pkt.categoria == CAT_ATRAPADAS);
            const uint32_t llaveUbic = calcularLlaveUbicacion(pkt.barrioId, pkt.ubicacion, pkt.referencia);

            // C) Evaluar Regla de las 3 Llaves: mismo lugar + misma categoria + ventana < 20 min
            IncidenteActivo* existente = buscarIncidenteActivo(llaveUbic, pkt.categoria, ahoraMs);
            if (existente != nullptr) {
                // Caso 1: La MISMA IP repite el mismo incidente -> Duplicado exacto interceptado en borde
                if (ipYaVoto(*existente, ipCliente)) {
                    _duplicadosBloqueados++;
                    registrarHash(hashId, existente->codigo);
                    Serial.printf(
                        "[EDGE-DEDUP] Duplicado misma IP=%s (barrio=%u cat=%u -> MZL-%03u, conf=%u) | Bloqueados=%lu\n",
                        ipObj.toString().c_str(),
                        pkt.barrioId,
                        pkt.categoria,
                        existente->codigo,
                        existente->confirmaciones,
                        static_cast<unsigned long>(_duplicadosBloqueados)
                    );
                    char dupResp[208];
                    snprintf(
                        dupResp,
                        sizeof(dupResp),
                        "{\"status\":\"encolado_espnow\",\"duplicado\":true,\"estacion\":\"%s\",\"codigo\":%u,\"codigoTexto\":\"MZL-%03u\",\"confirmaciones\":%u}",
                        ID_ESTACION,
                        existente->codigo,
                        existente->codigo,
                        existente->confirmaciones
                    );
                    request->send(200, "application/json", dupResp);
                    programarDesconexionPostReporte(ipCliente, ahoraMs);
                    return;
                }

                // Caso 2: OTRA IP distinta confirma el mismo incidente -> Consenso + escalamiento de prioridad
                if (excedeRateLimitIp(ipCliente, ahoraMs, esPeligroVital)) {
                    _bloqueadosRateLimit++;
                    _duplicadosBloqueados++;
                    Serial.printf("[EDGE-RATELIMIT] IP=%s supero %u reportes en 5 min (HTTP 429)\n",
                                  ipObj.toString().c_str(), MAX_REPORTES_POR_IP);
                    request->send(429, "application/json",
                                  "{\"status\":\"rate_limited\",\"mensaje\":\"Limite temporal: max 2 reportes cada 5 minutos por dispositivo\"}");
                    return;
                }

                agregarIpIncidente(*existente, ipCliente);
                registrarEnvioIp(ipCliente, ahoraMs);
                if (existente->confirmaciones < 255) {
                    existente->confirmaciones++;
                }
                // Acumula personas afectadas (tope 255) y necesidades
                uint16_t sumaPers = static_cast<uint16_t>(existente->afectados) + static_cast<uint16_t>(pkt.afectados);
                existente->afectados = (sumaPers > 255) ? 255 : static_cast<uint8_t>(sumaPers);
                existente->necesidadesMask |= pkt.necesidadesMask;
                existente->ultimaActividadMs = ahoraMs;

                // Escalamiento automatico de prioridad por consenso de vecinos
                if (pkt.urgencia > existente->urgencia) {
                    existente->urgencia = pkt.urgencia;
                }
                if (existente->confirmaciones >= 3) {
                    existente->urgencia = 3; // Critico verificado por 3+ vecinos
                } else if (existente->confirmaciones >= 2 && existente->urgencia < 2) {
                    existente->urgencia = 2; // Sube al menos a Alto con 2 confirmaciones
                }

                // Reutiliza el mismo codigo de incidente para actualizar la onda en el PMU sin crear tarjeta nueva
                pkt.codigo          = existente->codigo;
                pkt.confirmaciones  = existente->confirmaciones;
                pkt.urgencia        = existente->urgencia;
                pkt.afectados       = existente->afectados;
                pkt.necesidadesMask = existente->necesidadesMask;

                bool encolado = (_onReporte != nullptr) ? _onReporte(pkt) : false;
                if (encolado) {
                    registrarHash(hashId, pkt.codigo);
                    _agrupadosConsenso++;
                }

                Serial.printf(
                    "[EDGE-CONSENSO] Incidente MZL-%03u CORROBORADO por vecino #%u (IP=%s | barrio=%u | urg=%u)\n",
                    pkt.codigo,
                    pkt.confirmaciones,
                    ipObj.toString().c_str(),
                    pkt.barrioId,
                    pkt.urgencia
                );

                char consResp[220];
                snprintf(
                    consResp,
                    sizeof(consResp),
                    "{\"status\":\"%s\",\"duplicado\":false,\"agrupado\":true,\"estacion\":\"%s\",\"codigo\":%u,\"codigoTexto\":\"MZL-%03u\",\"confirmaciones\":%u}",
                    encolado ? "encolado_espnow" : "recibido_sin_enlace",
                    pkt.estacion,
                    pkt.codigo,
                    pkt.codigo,
                    pkt.confirmaciones
                );
                request->send(encolado ? 200 : 503, "application/json", consResp);
                if (encolado) {
                    programarDesconexionPostReporte(ipCliente, ahoraMs);
                }
                return;
            }

            // D) Incidente nuevo: verificar limite de tasa por IP (max 2 reportes / 5 min, salvo peligro vital)
            if (excedeRateLimitIp(ipCliente, ahoraMs, esPeligroVital)) {
                _bloqueadosRateLimit++;
                _duplicadosBloqueados++;
                Serial.printf("[EDGE-RATELIMIT] IP=%s bloqueada temporalmente (max %u/5min | HTTP 429)\n",
                              ipObj.toString().c_str(), MAX_REPORTES_POR_IP);
                request->send(429, "application/json",
                              "{\"status\":\"rate_limited\",\"mensaje\":\"Limite temporal: max 2 reportes cada 5 minutos por dispositivo\"}");
                return;
            }

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
                registrarEnvioIp(ipCliente, ahoraMs);
                crearIncidenteActivo(pkt, llaveUbic, ipCliente, ahoraMs);
                _incidentesUnicos++;
            }

            char respuesta[208];
            snprintf(
                respuesta,
                sizeof(respuesta),
                "{\"status\":\"%s\",\"duplicado\":false,\"agrupado\":false,\"estacion\":\"%s\",\"codigo\":%u,\"codigoTexto\":\"MZL-%03u\",\"confirmaciones\":1}",
                encolado ? "encolado_espnow" : "recibido_sin_enlace",
                pkt.estacion,
                pkt.codigo,
                pkt.codigo
            );
            request->send(encolado ? 200 : 503, "application/json", respuesta);
            if (encolado) {
                programarDesconexionPostReporte(ipCliente, ahoraMs);
            }
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

    // 5. Archivos estaticos de LittleFS
    _server.serveStatic("/", LittleFS, "/");

    // 6. Cualquier otra peticion desconocida redirige al portal
    _server.onNotFound([forzarRedireccion](AsyncWebServerRequest* request) {
        if (request->host() == "192.168.4.1") {
            request->send(404, "text/plain", "Archivo no encontrado");
        } else {
            forzarRedireccion(request);
        }
    });
}
