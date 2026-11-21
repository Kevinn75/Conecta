#pragma once

#include <Arduino.h>

// Canal Wi-Fi unico compartido entre el Hotspot (SoftAP) y ESP-NOW (1, 6 u 11)
constexpr uint8_t CANAL_ESPNOW = 1;

// Modo Long Range (Espressif LR a 250 kbps) para ESP-NOW manteniendo Hotspot normal para celulares.
// IMPORTANTE: Estacion y Concentrador deben compilarse con el MISMO valor (ambos 1 o ambos 0).
// 1 = Long Range activado | 0 = ESP-NOW estandar (si notas problemas, ponlo en 0)
#define USAR_ESPNOW_LR 0

// Identificador corto de la estacion (ej. "E01", "E02", "E03")
constexpr const char* ID_ESTACION = "E01";

// Intervalo de latido hacia el Concentrador PMU (ms)
constexpr unsigned long INTERVALO_LATIDO_MS = 5000;

// Direccion MAC STA del nodo Concentrador PMU.
// Dejar en broadcast (FF:FF:FF:FF:FF:FF) para pruebas rapidas, o reemplazar
// por la MAC STA que imprime el Concentrador al iniciar (WiFi.macAddress()).
constexpr uint8_t MAC_CONCENTRADOR[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
