#include "EspNowLink.h"
#include <string.h>

QueueHandle_t EspNowLink::_colaRx = nullptr;
volatile bool EspNowLink::_ultimoEnvioOk = false;

bool EspNowLink::iniciar(uint8_t canal, const uint8_t* macDestino) {
    _canal = canal;

    if (_colaRx == nullptr) {
        _colaRx = xQueueCreate(16, sizeof(MensajeRecibido));
        if (_colaRx == nullptr) {
            Serial.println("[ESPNOW] Error al crear cola RX");
            return false;
        }
    }

    // Asegura que el radio Wi-Fi opere exactamente en el canal acordado
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(_canal, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESPNOW] Error en esp_now_init()");
        return false;
    }

    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);

    if (macDestino != nullptr) {
        memcpy(_macPorDefecto, macDestino, 6);
        if (!agregarPeer(_macPorDefecto, _canal)) {
            return false;
        }
    }

    Serial.print("[ESPNOW] Inicializado en canal ");
    Serial.print(_canal);
    Serial.print(" | MAC STA: ");
    Serial.println(WiFi.macAddress());
    return true;
}

bool EspNowLink::agregarPeer(const uint8_t* mac, uint8_t canal) {
    if (mac == nullptr) return false;

    if (esp_now_is_peer_exist(mac)) {
        return true;
    }

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = canal;
    peerInfo.ifidx   = WIFI_IF_STA; // Interfaz STA compatible con WIFI_AP_STA y WIFI_STA
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("[ESPNOW] Error al registrar peer");
        return false;
    }
    return true;
}

bool EspNowLink::enviar(const PaqueteConecta& paquete, const uint8_t* macDestino) {
    const uint8_t* destino = (macDestino != nullptr) ? macDestino : _macPorDefecto;

    if (!esp_now_is_peer_exist(destino)) {
        if (!agregarPeer(destino, _canal)) {
            return false;
        }
    }

    esp_err_t res = esp_now_send(
        destino,
        reinterpret_cast<const uint8_t*>(&paquete),
        sizeof(PaqueteConecta)
    );
    return (res == ESP_OK);
}

bool EspNowLink::hayMensaje(MensajeRecibido& salida) {
    if (_colaRx == nullptr) return false;
    return (xQueueReceive(_colaRx, &salida, 0) == pdTRUE);
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
void EspNowLink::onDataSent(const esp_now_send_info_t* info, esp_now_send_status_t status) {
    (void)info;
    _ultimoEnvioOk = (status == ESP_NOW_SEND_SUCCESS);
}
#else
void EspNowLink::onDataSent(const uint8_t* mac, esp_now_send_status_t status) {
    (void)mac;
    _ultimoEnvioOk = (status == ESP_NOW_SEND_SUCCESS);
}
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void EspNowLink::onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (_colaRx == nullptr || info == nullptr || data == nullptr || len != sizeof(PaqueteConecta)) {
        return;
    }
    MensajeRecibido msg = {};
    memcpy(msg.macOrigen, info->src_addr, 6);
    memcpy(&msg.paquete, data, sizeof(PaqueteConecta));
    xQueueSend(_colaRx, &msg, 0);
}
#else
void EspNowLink::onDataRecv(const uint8_t* mac, const uint8_t* data, int len) {
    if (_colaRx == nullptr || mac == nullptr || data == nullptr || len != sizeof(PaqueteConecta)) {
        return;
    }
    MensajeRecibido msg = {};
    memcpy(msg.macOrigen, mac, 6);
    memcpy(&msg.paquete, data, sizeof(PaqueteConecta));
    xQueueSend(_colaRx, &msg, 0);
}
#endif
