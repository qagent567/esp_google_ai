/*
 * =============================================================================
 *  GeminiESP32 — Пример 1: Базовый чат с ИИ (BasicChat)
 * =============================================================================
 *  Демонстрирует простейший способ интегрировать Google Gemini AI в ваш проект.
 *  Все, что нужно — это Wi-Fi соединение и API-ключ из Google AI Studio.
 *
 *  Как получить бесплатный API-ключ:
 *    1. Перейдите на https://aistudio.google.com/
 *    2. Нажмите "Get API Key" → "Create API key in new project"
 *    3. Скопируйте сгенерированный ключ
 *
 *  Плата:    ESP32 DevKit (или любой ESP32)
 *  Частота:  115200 baud в Serial Monitor
 * =============================================================================
 */

#include <WiFi.h>
#include <GeminiESP32.h>

// ─── НАСТРОЙКИ (замените на свои) ────────────────────────────────────────────
const char* WIFI_SSID     = "YOUR_WIFI_SSID";       // Имя вашей Wi-Fi сети
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";   // Пароль вашей Wi-Fi сети
const char* GEMINI_API_KEY = "YOUR_GEMINI_API_KEY"; // Ключ Google AI Studio
// ─────────────────────────────────────────────────────────────────────────────

// Создаем объект ИИ.
// Первый аргумент  — API-ключ.
// Второй аргумент — название модели (можно менять в любой момент).
// 
// Рекомендуемая модель (Free Tier, 500 запросов/сутки):
//   "gemini-3.5-flash-lite"   — ⚡ Самая быстрая и экономичная (рекомендуется для ESP32)
GeminiESP32 ai(GEMINI_API_KEY, "gemini-3.5-flash-lite");

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n╔══════════════════════════════════════╗");
    Serial.println("║  GeminiESP32 — BasicChat Example     ║");
    Serial.println("╚══════════════════════════════════════╝\n");

    // 1. Подключаемся к Wi-Fi
    Serial.printf("Подключение к Wi-Fi: %s...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    // Ждем подключения (макс. 20 секунд)
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n[ОШИБКА] Не удалось подключиться к Wi-Fi!");
        Serial.println("Проверьте SSID и пароль. Перезагружаюсь...");
        delay(3000);
        ESP.restart();
    }
    
    Serial.printf("\n✓ Wi-Fi подключен! IP-адрес: %s\n\n", WiFi.localIP().toString().c_str());

    // 2. Инициализируем библиотеку.
    //    begin() выполняет следующее:
    //    - Загружает настройки из энергонезависимой памяти NVS (если были сохранены ранее)
    //    - Настраивает Smart DNS для гарантированного доступа к API Google
    //    - Инициализирует трекер суточных квот
    ai.begin();

    // 3. (Опционально) Задаем личность/роль ИИ через системный промпт.
    //    ИИ будет следовать этим инструкциям на протяжении всей сессии.
    ai.setSystemPrompt(
        "Ты полезный технический помощник. "
        "Отвечай максимально кратко и по делу. "
        "Используй примеры кода, когда это уместно."
    );
    
    // 4. (Опционально) Тонкая настройка параметров генерации:
    ai.setTemperature(0.7f);  // 0.0 = точные/детерминированные ответы, 2.0 = максимум творчества
    ai.setMaxTokens(512);     // Лимит длины ответа в токенах (≈ 1 токен ≈ 4 символа)

    // 5. Отправляем первый вопрос
    Serial.println("───────────────────────────────────────");
    Serial.println("[Вопрос]: Объясни в одном предложении, что такое ШИМ (PWM)?");
    Serial.println("───────────────────────────────────────");
    
    // ask() — простой вызов, возвращает только строку с текстом ответа
    String answer = ai.ask("Объясни в одном предложении, что такое ШИМ (PWM)?");
    
    Serial.println("[Ответ ИИ]:");
    Serial.println(answer);
    Serial.println();

    // 6. Демонстрация расширенного запроса через query().
    //    query() возвращает структуру GeminiResponse с полной метаинформацией.
    Serial.println("───────────────────────────────────────");
    Serial.println("[Вопрос]: Как подключить DHT22 к ESP32?");
    Serial.println("───────────────────────────────────────");
    
    GeminiResponse resp = ai.query("Как подключить DHT22 к ESP32? Только схема подключения.");
    
    if (resp.success) {
        Serial.println("[Ответ ИИ]:");
        Serial.println(resp.text);
        Serial.println();
        // Метаданные запроса
        Serial.printf("📊 Статистика: %d токенов, %lu мс, HTTP %d\n",
                      resp.totalTokens, resp.durationMs, resp.httpCode);
    } else {
        // При ошибке resp.text содержит подробное описание проблемы
        Serial.printf("[ОШИБКА] %s (HTTP %d)\n", resp.text.c_str(), resp.httpCode);
    }

    // 7. (Опционально) Проверка оставшейся квоты
    Serial.printf("\n📈 Запросов использовано сегодня: %u / %u\n",
                  ai.getUsage().getStats().requestsToday,
                  ai.getUsage().getStats().dailyRequestLimit);

    Serial.println("\n✅ Чат запущен! Вы можете общаться с ИИ в диалоговом режиме (он помнит контекст).");
    Serial.println("Введите /clear чтобы стереть память диалога.");
}

void loop() {
    // Пример чата через Serial Monitor.
    // Введите сообщение в монитор порта и нажмите Enter.
    if (Serial.available()) {
        String userInput = Serial.readStringUntil('\n');
        userInput.trim(); // Удаляем лишние пробелы и перенос строки
        
        if (userInput.length() == 0) return;
        
        if (userInput.equalsIgnoreCase("/clear")) {
            ai.clearHistory();
            Serial.println("\n[Система]: История диалога очищена.\n");
            return;
        }
        
        Serial.printf("\n[Вы]: %s\n", userInput.c_str());
        Serial.print("[ИИ]: ");
        
        String response = ai.ask(userInput);
        Serial.println(response);
        Serial.println();
    }
}
