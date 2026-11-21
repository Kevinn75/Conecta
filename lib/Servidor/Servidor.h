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

    // --- Motor Edge Computing: Rate Limit por IP + Deduplicacion + Consenso (3 llaves) ---
    static constexpr size_t MAX_CLIENTES_IP         = 16;
    static constexpr size_t MAX_INCIDENTES_ACTIVOS  = 24;
    static constexpr size_t MAX_IPS_POR_INCIDENTE   = 8;
    static constexpr size_t HISTORIAL_IDEMPOTENCIA  = 32;

    static constexpr unsigned long VENTANA_RATE_LIMIT_MS = 5UL * 60UL * 1000UL;   // 5 min
    static constexpr unsigned long VENTANA_CONSENSO_MS   = 20UL * 60UL * 1000UL;  // 20 min
    static constexpr uint8_t MAX_REPORTES_POR_IP         = 2;

    struct RegistroIp {
        uint32_t ip;
        unsigned long inicioVentanaMs;
        uint8_t conteoVentana;
    };

    struct IncidenteActivo {
        bool activo;
        uint32_t llaveUbicacion; // barrioId (si >0) o hash de referencia/ubicacion
        uint8_t categoria;
        uint16_t codigo;
        uint8_t confirmaciones;
        uint8_t urgencia;
        uint8_t afectados;
        uint8_t necesidadesMask;
        unsigned long ultimaActividadMs;
        uint32_t ipsVotantes[MAX_IPS_POR_INCIDENTE];
        uint8_t numIps;
    };

    RegistroIp _clientesIp[MAX_CLIENTES_IP] = {};
    IncidenteActivo _incidentes[MAX_INCIDENTES_ACTIVOS] = {};

    uint16_t _siguienteCodigo      = 100;
    uint32_t _totalRecibidos       = 0;
    uint32_t _incidentesUnicos     = 0;
    uint32_t _agrupadosConsenso    = 0;
    uint32_t _duplicadosBloqueados = 0;
    uint32_t _bloqueadosRateLimit  = 0;

    uint32_t _ultimosHashes[HISTORIAL_IDEMPOTENCIA] = {0};
    uint16_t _ultimosCodigos[HISTORIAL_IDEMPOTENCIA] = {0};
    size_t _indiceHistorial = 0;

    static uint32_t hashFnv1a(const char* texto);
    static uint32_t calcularLlaveUbicacion(uint16_t barrioId, const char* ubicacion, const char* referencia);
    bool buscarDuplicado(uint32_t hashId, uint16_t& codigoExistente) const;
    void registrarHash(uint32_t hashId, uint16_t codigo);
    bool excedeRateLimitIp(uint32_t ip, unsigned long ahoraMs, bool esPeligroVital);
    void registrarEnvioIp(uint32_t ip, unsigned long ahoraMs);
    IncidenteActivo* buscarIncidenteActivo(uint32_t llaveUbic, uint8_t categoria, unsigned long ahoraMs);
    IncidenteActivo* crearIncidenteActivo(const PaqueteConecta& pkt, uint32_t llaveUbic, uint32_t ip, unsigned long ahoraMs);
    static bool ipYaVoto(const IncidenteActivo& inc, uint32_t ip);
    static void agregarIpIncidente(IncidenteActivo& inc, uint32_t ip);

    void configurarRed();
    void configurarRutas();
};