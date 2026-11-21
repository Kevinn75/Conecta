#include <Arduino.h>
#include "Servidor.h"

ServidorHotspot servidor;
unsigned long ultimoLatido = 0;

void setup() {
    Serial.begin(115200);
    delay(500);
    servidor.iniciar();
}

void loop() {
    servidor.procesarDNS();

    if (millis() - ultimoLatido > 3000) {
        ultimoLatido = millis();
        Serial.print("[VIVO] Celulares conectados: ");
        Serial.println(WiFi.softAPgetStationNum());
    }
}