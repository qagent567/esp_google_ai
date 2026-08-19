/*
 * =============================================================================
 *  GeminiESP32 — Пример 6: Нативный Function Calling (Tool Use)
 * =============================================================================
 *  Демонстрирует официальный стандарт Google API Function Calling:
 *  вы регистрируете C++ функции с описанием их параметров, а нейросеть
 *  САМА решает, когда и с какими аргументами вызвать вашу функцию.
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

const int RELAY_PIN = 4;
float currentRoomTemp = 24.5;

void setup() {
    Serial.begin(115200);
    pinMode(RELAY_PIN, OUTPUT);
    delay(500);

    Serial.println("\n╔════════════════════════════════════════════╗");
    Serial.println("║  GeminiESP32 — Function Calling (Tools)    ║");
    Serial.println("╚════════════════════════════════════════════╝\n");

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Подключение к Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\n✓ Подключено! IP: %s\n\n", WiFi.localIP().toString().c_str());

    ai.begin();

    // ─── 1. Регистрация функции без параметров ──────────────────────────────
    ai.registerFunction(
        "get_room_temperature",
        "Возвращает текущую температуру в помещении с датчика",
        [](JsonObjectConst args) -> String {
            Serial.println("\n[ESP32]: Вызвана функция get_room_temperature()!");
            return String(currentRoomTemp, 1) + " °C";
        }
    );

    // ─── 2. Регистрация функции с параметрами ───────────────────────────────
    std::vector<FunctionParam> relayParams = {
        {"state", "BOOLEAN", "true для включения, false для выключения", true}
    };

    ai.registerFunction(
        "set_relay_state",
        "Включает или выключает силовое реле обогревателя",
        relayParams,
        [](JsonObjectConst args) -> String {
            bool state = args["state"] | false;
            digitalWrite(RELAY_PIN, state ? HIGH : LOW);
            Serial.printf("\n[ESP32]: Реле физически переключено в %s!\n", state ? "ON" : "OFF");
            return state ? "Реле успешно включено" : "Реле успешно выключено";
        }
    );

    Serial.println("✓ Функции успешно зарегистрированы в Gemini Tool Registry.");
    Serial.println("✓ Попробуйте спросить: 'Какая температура в комнате?' или 'Включи обогреватель'.");
}

void loop() {
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        if (input.length() == 0) return;

        Serial.printf("\n[Вы]: %s\n", input.c_str());
        
        GeminiResponse resp = ai.query(input);
        if (resp.hasFunctionCall) {
            Serial.printf("🤖 [ИИ решил вызвать Tool]: %s\n", resp.functionName.c_str());
            Serial.printf("⚡ [Результат работы C++]: %s\n", resp.functionResult.c_str());
        } else {
            Serial.printf("[ИИ]: %s\n", resp.text.c_str());
        }
        Serial.println();
    }
}
