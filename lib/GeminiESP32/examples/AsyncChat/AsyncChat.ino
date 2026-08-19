/*
 * =============================================================================
 *  GeminiESP32 — Пример 4: Неблокирующий асинхронный чат (AsyncChat)
 * =============================================================================
 *  Демонстрирует, как отправлять запросы к Gemini AI в фоновом потоке FreeRTOS,
 *  не блокируя выполнение основного цикла loop().
 *
 *  Пока ИИ генерирует ответ (2-8 секунд), ваш ESP32 продолжает:
 *    - Быстро мигать светодиодом / плавно менять яркость
 *    - Опрашивать кнопки и датчики без задержек
 *    - Управлять сервомоторами и реле
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

const int LED_PIN = 2; // Встроенный светодиод

GeminiESP32 ai(GEMINI_API_KEY, "gemini-3.5-flash-lite");

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    delay(500);

    Serial.println("\n╔══════════════════════════════════════╗");
    Serial.println("║  GeminiESP32 — AsyncChat Example     ║");
    Serial.println("╚══════════════════════════════════════╝\n");

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Подключение к Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\n✓ Подключено! IP: %s\n\n", WiFi.localIP().toString().c_str());

    ai.begin();

    // 1. Включаем сохранение истории диалога во Flash-память (NVS).
    //    ИИ будет помнить контекст даже после перезагрузки платы!
    ai.enablePersistentHistory(true);

    Serial.println("✓ Персистентная память истории активирована.");
    Serial.println("✓ Введите вопрос в Serial Monitor. Плата продолжит мигать светодиодом!");
}

void loop() {
    // ─── 1. ОСНОВНАЯ ЗАДАЧА (работает плавно и без пауз) ────────────────────
    // Быстрое мигание светодиодом (каждые 100 мс)
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 100) {
        lastBlink = millis();
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }

    // ─── 2. ОБРАБОТКА ВВОДА С КОНСОЛИ ───────────────────────────────────────
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        if (input.length() == 0) return;

        if (input.equalsIgnoreCase("/clear")) {
            ai.clearHistory();
            Serial.println("\n[Система]: История диалога полностью очищена из RAM и NVS!\n");
            return;
        }

        if (ai.isBusy()) {
            Serial.println("\n[!] ИИ уже думает над предыдущим вопросом. Подождите...");
            return;
        }

        Serial.printf("\n[Вы]: %s\n", input.c_str());
        Serial.println("[Система]: Запрос отправлен в фоновый поток. loop() продолжает работать!");

        // 3. Отправка НЕБЛОКИРУЮЩЕГО запроса через askAsync()
        //    Колбэк выполнится автоматически, когда Google пришлет ответ!
        bool started = ai.askAsync(input, [](const String& answer) {
            Serial.println("\n───────────────────────────────────────");
            Serial.println("[Ответ ИИ из фона]:");
            Serial.println(answer);
            Serial.println("───────────────────────────────────────\n");
        });

        if (!started) {
            Serial.println("[Ошибка]: Не удалось запустить фоновую задачу.");
        }
    }
}
