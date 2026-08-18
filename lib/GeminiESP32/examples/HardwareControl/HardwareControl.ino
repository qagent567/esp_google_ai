#include <WiFi.h>
#include <GeminiESP32.h>

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* geminiApiKey = "YOUR_GEMINI_API_KEY";

GeminiESP32 ai(geminiApiKey, "gemini-3.5-flash-lite");

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n--- Пример HardwareControl (GeminiESP32) ---");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi подключен! IP: " + WiFi.localIP().toString());

    // Инициализация
    ai.begin();
    
    // Включаем аппаратное управление GPIO/ADC/I2C
    ai.enableHardwareControl(true);

    // Запрос к AI с командой включить встроенный синий светодиод на плате
    Serial.println("\nОтправляем запрос: 'Включи синий светодиод на плате и сообщи текущую температуру процессора'");
    GeminiResponse resp = ai.query("Включи синий светодиод на плате и сообщи текущую температуру процессора");

    Serial.println("\n[Результат]:");
    Serial.println(resp.text);
    Serial.printf("[Статистика]: Время: %lu мс, Токены: %d (запрос: %d, ответ: %d)\n", 
                  resp.durationMs, resp.totalTokens, resp.promptTokens, resp.candidatesTokens);
}

void loop() {
    if (Serial.available()) {
        String prompt = Serial.readStringUntil('\n');
        prompt.trim();
        if (prompt.length() > 0) {
            Serial.println("\n>>> " + prompt);
            String answer = ai.ask(prompt);
            Serial.println("<<< " + answer);
        }
    }
}
