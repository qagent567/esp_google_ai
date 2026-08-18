/*
 * =============================================================================
 *  GeminiESP32 — Пример 3: Интеграция в существующую прошивку (Integration)
 * =============================================================================
 *  Показывает как встроить библиотеку GeminiESP32 в уже готовый проект,
 *  не затрагивая существующий код управления периферией.
 *
 *  Сценарий: У вас уже есть прошивка, которая управляет:
 *    - Сервомотором на GPIO 13 (ШИМ)
 *    - Реле на GPIO 5 (управление насосом)
 *    - SPI дисплеем (GPIO 18=SCK, GPIO 19=MISO, GPIO 23=MOSI, GPIO 15=CS)
 *    - DS18B20 датчиком температуры на GPIO 27 (1-Wire шина)
 *
 *  Задача: Добавить ИИ-помощника, который может:
 *    - Отвечать на вопросы о состоянии системы
 *    - Управлять ТОЛЬКО реле (GPIO 5) и встроенным светодиодом (GPIO 2)
 *    - НЕ трогать остальные пины (сервомотор, SPI, 1-Wire)
 * =============================================================================
 */

#include <WiFi.h>
#include <GeminiESP32.h>

// ─── НАСТРОЙКИ ────────────────────────────────────────────────────────────────
const char* WIFI_SSID      = "YOUR_SSID";
const char* WIFI_PASSWORD  = "YOUR_PASSWORD";
const char* GEMINI_API_KEY = "YOUR_API_KEY";

// Пины ВАШЕЙ прошивки (ИИ к ним не должен прикасаться!)
#define PIN_SERVO   13   // Сервомотор (ШИМ)
#define PIN_RELAY    5   // Реле насоса
#define PIN_SPI_CS  15   // Chip Select дисплея
#define PIN_DS18B20 27   // Температурный датчик
#define PIN_LED      2   // Встроенный светодиод

// Только ЭТИ пины разрешены для управления ИИ:
const std::vector<uint8_t> AI_ALLOWED_PINS = {
    PIN_RELAY,  // GPIO 5 — реле (ИИ может включать/выключать насос)
    PIN_LED,    // GPIO 2 — индикатор статуса
};
// ─────────────────────────────────────────────────────────────────────────────

// ─── Переменные вашей прошивки ────────────────────────────────────────────────
float waterTemperature = 22.5f;   // Температура воды (в реальной прошивке — от DS18B20)
int   servoAngle       = 90;      // Текущий угол сервомотора
bool  pumpActive       = false;   // Состояние насоса
// ─────────────────────────────────────────────────────────────────────────────

// ─── ИИ — добавляем как отдельный независимый объект ─────────────────────────
// Библиотека создает объекты только в RAM (48 байт структуры + клиент).
// Flash-код ИИ начнет расходовать память только при первом вызове ask() / query().
GeminiESP32 ai(GEMINI_API_KEY, "gemini-3.5-flash-lite");

bool aiInitialized = false; // Инициализируем ИИ лениво, только при первом запросе
// ─────────────────────────────────────────────────────────────────────────────

// Функция ленивой инициализации ИИ (чтобы не тормозить startup прошивки)
void ensureAiReady() {
    if (aiInitialized) return;

    // Явно отключаем Smart DNS, если у нас корпоративный DNS в сети
    // ai.enableSmartDns(false);
    
    // Или задаем свои DNS-серверы:
    // ai.setSmartDns("8.8.8.8", "1.1.1.1");

    ai.begin(GEMINI_API_KEY, true);

    // Запрещаем ИИ трогать пины сервомотора, SPI дисплея и 1-Wire шины!
    ai.setAllowedPins(AI_ALLOWED_PINS);
    
    // Даем ИИ контекст о вашей системе
    ai.setSystemPrompt(
        "Ты — умный ассистент системы управления аквапоникой. "
        "Текущие датчики: вода " + String(waterTemperature, 1) + " °C, "
        "насос " + String(pumpActive ? "РАБОТАЕТ" : "остановлен") + ". "
        "Управление насосом (GPIO 5) и светодиодом (GPIO 2) доступно тебе. "
        "Сервомотор, дисплей и датчик температуры — ТОЛЬКО чтение через мои переменные, "
        "не пытайся управлять ими напрямую."
    );

    // Экономичный режим: короткие ответы для встроенных систем
    ai.setTemperature(0.3f);   // Меньше творчества, больше точности
    ai.setMaxTokens(256);      // Короткие ответы экономят RAM и квоту
    
    aiInitialized = true;
    Serial.println("[AI] Ассистент инициализирован.");
}

// ─── Функции вашей основной прошивки ─────────────────────────────────────────
void setPump(bool on) {
    pumpActive = on;
    digitalWrite(PIN_RELAY, on ? HIGH : LOW);
    Serial.printf("[Прошивка] Насос: %s\n", on ? "ВКЛ" : "ВЫКЛ");
}

void setServo(int angle) {
    servoAngle = constrain(angle, 0, 180);
    // ledcWrite(servoChannel, map(servoAngle, 0, 180, 26, 122)); // реальный PWM
    Serial.printf("[Прошивка] Сервомотор: %d°\n", servoAngle);
}

float readTemperature() {
    // В реальной прошивке — чтение с DS18B20 через OneWire
    return waterTemperature;
}
// ─────────────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[Запуск] Инициализация прошивки...");

    // Инициализируем ВАШУ периферию
    pinMode(PIN_RELAY, OUTPUT);
    pinMode(PIN_LED,   OUTPUT);
    pinMode(PIN_SPI_CS, OUTPUT);
    digitalWrite(PIN_SPI_CS, HIGH);  // Деактивируем дисплей

    // Подключение к Wi-Fi (нужно и для вашей прошивки, и для ИИ)
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Wi-Fi...");
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.printf("\n[OK] IP: %s\n\n", WiFi.localIP().toString().c_str());

    // ИИ не инициализируем сразу — только по первому запросу
    Serial.println("[OK] Прошивка готова. ИИ-ассистент будет запущен при первом запросе.");
    Serial.println("Введите вопрос в Serial Monitor:");
}

void loop() {
    // ─── ВАША ОСНОВНАЯ ЛОГИКА (работает всегда, независимо от ИИ) ────────────
    static unsigned long lastSensorRead = 0;
    if (millis() - lastSensorRead > 5000) {
        lastSensorRead = millis();
        waterTemperature = readTemperature();
        
        // Автоматика: включить насос если вода холоднее 20°C
        if (waterTemperature < 20.0f && !pumpActive) {
            setPump(true);
        } else if (waterTemperature >= 25.0f && pumpActive) {
            setPump(false);
        }
    }
    // ─────────────────────────────────────────────────────────────────────────

    // ─── ИИ АССИСТЕНТ (опциональный, по запросу) ─────────────────────────────
    if (Serial.available()) {
        String userInput = Serial.readStringUntil('\n');
        userInput.trim();
        if (userInput.length() == 0) return;

        // Инициализируем ИИ только при первом обращении (ленивая инициализация)
        ensureAiReady();

        Serial.printf("\n[Вы]: %s\n", userInput.c_str());
        Serial.print("[ИИ]: ");
        
        GeminiResponse resp = ai.query(userInput);
        
        Serial.println(resp.text);
        
        if (!resp.success) {
            Serial.printf("[!] Ошибка ИИ: HTTP %d\n", resp.httpCode);
        }
    }
    // ─────────────────────────────────────────────────────────────────────────
}
