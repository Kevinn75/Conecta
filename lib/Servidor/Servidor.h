#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

class ServidorHotspot {
public:
    ServidorHotspot();
    void iniciar();
    void procesarDNS();

private:
    const char* _ssid = "punto_de_ayuda_conecta";
    const byte _puertoDNS = 53;
    
    IPAddress _apIP;
    IPAddress _netMsk;
    
    DNSServer _dnsServer;
    AsyncWebServer _server;

    void configurarRed();
    void configurarRutas();
};