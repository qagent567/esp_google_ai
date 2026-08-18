# 🤖 Библиотека GeminiESP32

Легковесная и производительная Arduino/PlatformIO библиотека для интеграции **Google Gemini AI Studio** в проекты на **ESP32**.

---

## 🌟 Возможности

- 🚀 **Быстрая интеграция**: подключение к Gemini AI всего в 3-5 строк кода.
- 🛠️ **Двустороннее управление железом**: ИИ может напрямую включать/выключать GPIO (светодиоды, реле), читать ADC (аналоговые датчики), сканировать I2C и запрашивать температуру кристалла ESP32.
- 🌐 **Встроенный Smart DNS**: гарантирует стабильное прямое соединение с Google AI API в любых сетевых условиях.
- 📊 **Контроль суточных квот**: автоматический учет лимитов Free Tier (1500 запросов/сутки), токенов и расчет сброса в 00:00 по часовому поясу.
- 💾 **Экономия памяти и Flash**: нулевые утечки кучи (heap), поддержка ESP32 Flash оптимизаций и OTA-разделов.

---

## 📦 Быстрый старт

```cpp
#include <WiFi.h>
#include <GeminiESP32.h>

GeminiESP32 ai("ВАШ_GEMINI_API_KEY", "gemini-3.5-flash-lite");

void setup() {
    Serial.begin(115200);
    WiFi.begin("SSID", "PASSWORD");
    while (WiFi.status() != WL_CONNECTED) delay(500);

    ai.begin();
    
    // Простой запрос:
    String answer = ai.ask("Как работает ШИМ на микроконтроллере?");
    Serial.println(answer);
}

void loop() {}
```

---

## ⚡ Управление физическим миром через ИИ

```cpp
// Включение аппаратного управления
ai.enableHardwareControl(true);

// ИИ сам поймет команду и подаст сигнал на GPIO 2 (встроенный светодиод)
String response = ai.ask("Включи синий светодиод на плате");
Serial.println(response);
```

---

## 📊 Доступные методы

- `ai.begin([apiKey])` — инициализация библиотеки.
- `ai.ask(prompt)` — синхронный запрос (возвращает `String`).
- `ai.query(prompt)` — детальный запрос (возвращает `GeminiResponse` с токенами и временем).
- `ai.ping()` — проверка доступности API Google.
- `ai.setModel(modelName)` — смена модели (`gemini-3.5-flash-lite`, `gemini-2.0-flash` и др.).
- `ai.setSystemPrompt(prompt)` — установка роли/поведения ИИ.
- `ai.enableHardwareControl(bool)` — вкл/выкл управления GPIO/ADC/I2C.
- `ai.setSmartDns(primary, secondary)` — настройка DNS.
- `ai.getTelemetry()` — аппаратная телеметрия ESP32 (температура чипа, память, RSSI).
