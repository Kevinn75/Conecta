#include "Servidor.h"

ServidorHotspot::ServidorHotspot() 
    : _apIP(192, 168, 4, 1), 
      _netMsk(255, 255, 255, 0), 
      _server(80) {}

void ServidorHotspot::iniciar() {
    // 1. Montar sistema de archivos LittleFS (donde vivirá index.html)
    if (!LittleFS.begin(true)) {
        Serial.println("[ERROR] Fallo al montar LittleFS");
    } else {
        Serial.println("[OK] LittleFS montado correctamente");
    }

    // 2. Encender Wi-Fi SoftAP + DHCP automático + DNS Hijacking
    configurarRed();

    // 3. Configurar endpoints HTTP y redirección de portal cautivo
    configurarRutas();

    // 4. Arrancar servidor web asíncrono
    _server.begin();
    Serial.println("[OK] Servidor HTTP Asincrono escuchando en http://192.168.4.1");
}

void ServidorHotspot::procesarDNS() {
    // Debe ejecutarse continuamente en el loop() para responder consultas DNS
    _dnsServer.processNextRequest();
}

void ServidorHotspot::configurarRed() {
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(_apIP, _apIP, _netMsk);
    WiFi.softAP(_ssid);

    // TTL en 0 evita que el celular guarde en caché respuestas DNS fallidas
    _dnsServer.setTTL(60);
    _dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    _dnsServer.start(_puertoDNS, "*", _apIP);

    Serial.print("[OK] Hotspot activo: ");
    Serial.println(_ssid);
    Serial.print("[OK] IP de la estacion: ");
    Serial.println(WiFi.softAPIP());
}

void ServidorHotspot::configurarRutas() {
    // 1. Página principal de la estación
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (LittleFS.exists("/index.html")) {
            request->send(LittleFS, "/index.html", "text/html");
        } else {
            request->send(200, "text/html; charset=utf-8", 
                "<!DOCTYPE html><html><head><meta charset='utf-8'>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<title>Punto de Ayuda Conecta</title></head>"
                "<body style='font-family:sans-serif;text-align:center;padding:40px;'>"
                "<h1>Punto de Ayuda Conecta</h1>"
                "<p>Portal Cautivo Activo (Modo Offline-First)</p>"
                "</body></html>");
        }
    });

    // 2. Redirección estándar 302 (el navegador del celular exige saltar hacia 192.168.4.1)
    auto forzarRedireccion = [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "");
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

    // 3. Archivos estáticos de LittleFS
    _server.serveStatic("/", LittleFS, "/");

    // 4. Endpoint en espera del Contrato JSON
    _server.on("/api/reporte", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"status\":\"en_espera\",\"mensaje\":\"Endpoint listo\"}");
    });

    // 5. Cualquier otra petición desconocida redirige al portal
    _server.onNotFound([forzarRedireccion](AsyncWebServerRequest *request) {
        // Si ya está pidiendo la raíz o un archivo local, no entrar en bucle
        if (request->host() == "192.168.4.1") {
            request->send(404, "text/plain", "Archivo no encontrado");
        } else {
            forzarRedireccion(request);
        }
    });
}