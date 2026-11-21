#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <functional>
#include "Config.h"
#include "Protocol.h"

using CallbackReporte = std::function<bool(const PaqueteConecta&)>;

class ServidorHotspot {
public:
    ServidorHotspot();
    void iniciar(uint8_t canalWiFi = CANAL_ESPNOW);
    void procesarDNS();
    void setCallbackReporte(CallbackReporte cb);

private:
    const char* _ssid = "punto_de_ayuda_conecta";
    const byte _puertoDNS = 53;
    uint8_t _canalWiFi = CANAL_ESPNOW;

    IPAddress _apIP;
    IPAddress _netMsk;

    DNSServer _dnsServer;
    AsyncWebServer _server;
    CallbackReporte _onReporte = nullptr;

    void configurarRed();
    void configurarRutas();
};