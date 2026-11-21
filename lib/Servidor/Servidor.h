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

    uint16_t _siguienteCodigo = 100;
    uint32_t _totalReportes = 0;

    static constexpr size_t HISTORIAL_IDEMPOTENCIA = 32;
    uint32_t _ultimosHashes[HISTORIAL_IDEMPOTENCIA] = {0};
    uint16_t _ultimosCodigos[HISTORIAL_IDEMPOTENCIA] = {0};
    size_t _indiceHistorial = 0;

    static uint32_t hashFnv1a(const char* texto);
    bool buscarDuplicado(uint32_t hashId, uint16_t& codigoExistente) const;
    void registrarHash(uint32_t hashId, uint16_t codigo);

    void configurarRed();
    void configurarRutas();
};