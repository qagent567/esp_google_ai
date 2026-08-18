#include <WiFi.h>
#include <GeminiESP32.h>

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* geminiApiKey = "YOUR_GEMINI_API_KEY";

GeminiESP32 ai(geminiApiKey, "gemini-3.5-flash-lite");

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n--- Пример BasicChat (GeminiESP32) ---");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi подключен! IP: " + WiFi.localIP().toString());

    // Инициализация библиотеки (автоматически настраивает Smart DNS и квоты)
    ai.begin();

    // Тест соединения
    Serial.println("Проверка связи с Google AI Studio...");
    if (ai.ping()) {
        Serial.println("Связь с Gemini API успешно установлена!");
    }

    // Отправка вопроса
    Serial.println("\nЗадаем вопрос ИИ: 'Кто ты и для чего создан?'");
    String answer = ai.ask("Кто ты и для чего создан?");
    
    Serial.println("\n[Ответ Gemini]:");
    Serial.println(answer);
}

void loop() {
    // В loop можно отправлять команды из Serial Monitor
    if (Serial.available()) {
        String prompt = Serial.readStringUntil('\n');
        prompt.trim();
        if (prompt.length() > 0) {
            Serial.println("\n>>> Вы: " + prompt);
            String answer = ai.ask(prompt);
            Serial.println("<<< Gemini: " + answer);
        }
    }
}
