/*
 * =============================================================================
 *  GeminiESP32 — Пример 5: Потоковый вывод ответа (StreamingChat / SSE)
 * =============================================================================
 *  Демонстрирует мгновенный посимвольный вывод ответа нейросети по мере генерации
 *  токенов серверами Google (Server-Sent Events streaming).
 *
 *  Вам больше не нужно ждать 5-10 секунд до завершения всего ответа:
 *  первые слова появляются уже через ~1 секунду!
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

    Serial.println("\n╔══════════════════════════════════════════╗");
    Serial.println("║  GeminiESP32 — StreamingChat (SSE)       ║");
    Serial.println("╚══════════════════════════════════════════╝\n");

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Подключение к Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\n✓ Подключено! IP: %s\n\n", WiFi.localIP().toString().c_str());

    ai.begin();
    ai.enablePersistentHistory(true); // Запоминать контекст между сессиями

    Serial.println("✓ Введите вопрос в Serial Monitor для потокового вывода:");
}

void loop() {
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        if (input.length() == 0) return;

        if (input.equalsIgnoreCase("/clear")) {
            ai.clearHistory();
            Serial.println("\n[Система]: Память диалога очищена.\n");
            return;
        }

        Serial.printf("\n[Вы]: %s\n", input.c_str());
        Serial.print("[ИИ]: ");

        // streamAsk вызывает лямбда-функцию для каждого поступающего фрагмента текста!
        GeminiResponse resp = ai.streamAsk(input, [](const String& chunk, bool isLast) {
            if (!isLast) {
                // Выводим фрагмент сразу в Serial (или на OLED-дисплей)
                Serial.print(chunk);
            }
        });

        Serial.println("\n");
        Serial.printf("📊 [Итого]: %lu мс, ~%d токенов\n\n", resp.durationMs, resp.totalTokens);
    }
}
