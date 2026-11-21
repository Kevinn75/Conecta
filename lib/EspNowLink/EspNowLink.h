#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "Protocol.h"

struct MensajeRecibido {
    uint8_t macOrigen[6];
    PaqueteConecta paquete;
};

class EspNowLink {
public:
    bool iniciar(uint8_t canal, const uint8_t* macDestino = nullptr);
    bool agregarPeer(const uint8_t* mac, uint8_t canal);
    bool enviar(const PaqueteConecta& paquete, const uint8_t* macDestino = nullptr);
    bool hayMensaje(MensajeRecibido& salida);
    bool ultimoEnvioOk() const { return _ultimoEnvioOk; }

private:
    uint8_t _canal = 1;
    uint8_t _macPorDefecto[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    static QueueHandle_t _colaRx;
    static volatile bool _ultimoEnvioOk;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
    static void onDataSent(const esp_now_send_info_t* info, esp_now_send_status_t status);
#else
    static void onDataSent(const uint8_t* mac, esp_now_send_status_t status);
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    static void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len);
#else
    static void onDataRecv(const uint8_t* mac, const uint8_t* data, int len);
#endif
};
