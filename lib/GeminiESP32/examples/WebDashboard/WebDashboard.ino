/*
 * =============================================================================
 *  GeminiESP32 — Пример 7: Встроенный Web Dashboard
 * =============================================================================
 *  Демонстрирует запуск локального веб-сервера со стильным графическим интерфейсом
 *  чата с Gemini, мониторингом памяти/квот и ручным управлением GPIO со смартфона.
 *
 *  Плата:    ESP32 DevKit
 *  Частота:  115200 baud
 * =============================================================================
 */

#include <WiFi.h>
#include <GeminiESP32.h>

const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* GEMINI_API_KEY = "YOUR_GEMINI_API_KEY";

GeminiESP32 ai(GEMINI_API_KEY, "gemini-3.5-flash-lite");

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n╔════════════════════════════════════════════╗");
    Serial.println("║  GeminiESP32 — Web Dashboard Example       ║");
    Serial.println("╚════════════════════════════════════════════╝\n");

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Подключение к Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\n✓ Подключено! IP адрес: http://%s\n\n", WiFi.localIP().toString().c_str());

    ai.begin();
    ai.enablePersistentHistory(true);

    // Запуск Web Dashboard на порту 80 в фоновом FreeRTOS потоке
    ai.startWebDashboard(80, true);

    Serial.println("🎉 ВЕБ-ИНТЕРФЕЙС ЗАПУЩЕН!");
    Serial.printf("Откройте в браузере любого устройства: http://%s\n", WiFi.localIP().toString().c_str());
}

void loop() {
    // Основной цикл loop свободен для ваших задач!
    // Веб-сервер и обработка чата крутятся в фоновом потоке на Ядре 0.
    delay(1000);
}
