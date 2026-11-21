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

// Tiempo maximo que cada celular puede permanecer conectado al Hotspot (5 minutos = 300000 ms)
//constexpr unsigned long DURACION_SESION_WIFI_MS = 5UL * 60UL * 1000UL;
constexpr unsigned long DURACION_SESION_WIFI_MS = 30UL * 1000UL;

// Tiempo de gracia tras enviar un reporte antes de desconectar el celular (5 s)
// Permite que el telefono muestre el comprobante MZL-xxx antes de liberar el cupo Wi-Fi.
constexpr unsigned long GRACIA_POST_REPORTE_MS = 10000UL;

// Red mallada ESP-NOW (inundacion controlada):
// Cada estacion reenvia paquetes de otras estaciones hasta MAX_SALTOS_MALLA saltos (2 = hasta 3x alcance).
constexpr uint8_t MAX_SALTOS_MALLA = 2;

// Direccion destino ESP-NOW: MANTENER en broadcast (FF:FF:FF:FF:FF:FF) para que la red
// mallada funcione (las estaciones vecinas escuchan y reenvian hacia el Concentrador PMU).
constexpr uint8_t MAC_CONCENTRADOR[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
