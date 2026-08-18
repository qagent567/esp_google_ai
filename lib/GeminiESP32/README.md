# 🚀 GeminiESP32 — Google AI Studio Client & Hardware Controller for ESP32

[![PlatformIO Registry](https://img.shields.io/badge/PlatformIO-GeminiESP32-orange.svg)](https://platformio.org/)
[![Framework](https://img.shields.io/badge/Framework-Arduino-blue.svg)](https://www.arduino.cc/)
[![Target MCU](https://img.shields.io/badge/MCU-ESP32-red.svg)](https://espressif.com/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

**GeminiESP32** — это легковесная, высокопроизводительная и полностью автономная C++ библиотека для интеграции нейросетей **Google Gemini (Google AI Studio)** в проекты на микроконтроллерах **ESP32**.

Библиотека разработана с акцентом на **минимальный расход памяти**, **отсутствие фрагментации кучи (heap)**, **встроенный Smart DNS** и **двустороннее управление физическим миром** (GPIO, ADC, I2C, аппаратная телеметрия).

---

## 🌟 Ключевые возможности

- ⚡ **Быстрая интеграция в 3 строки кода**: подключение Gemini к любому скетчу.
- 🤖 **Двусторонний аппаратный мост (Physical AI)**:
  - ИИ может самостоятельно включать/выключать пины GPIO, переключать реле и светодиоды.
  - Чтение показаний аналоговых входов (ADC1: пины 32-39, расчет напряжения 0-3.3 В).
  - Сканирование шины I2C (поиск датчиков температуры/давления и OLED-дисплеев).
  - Чтение внутренней телеметрии кристалла (температура чипа ESP32, свободная RAM, аптайм, Wi-Fi RSSI).
- 🌐 **Встроенный Smart DNS (обход блокировок)**: автоматический DNS-роутинг через проверенные Smart DNS серверы (`xbox-dns.ru`) для гарантированного прямого доступа к API Google без VPN и прокси.
- 📊 **Контроль суточных квот (Usage Tracker)**:
  - Автоматический учет лимита 1500 запросов/сутки (Free Tier для Flash/Lite).
  - Подсчет входных и выходных токенов.
  - Точный сброс счетчиков в 00:00 местного времени через NTP-синхронизацию.
- 🛡️ **Безопасность и оптимизация памяти**:
  - Фильтрованная десериализация JSON (экономия до 8 КБ RAM на запрос).
  - Защита системных пинов (Flash SPI и UART0 защищены от случайной перезаписи).
  - Механизм авто-повтора (retry) при временных сбоях сети или 500/503 ошибках сервера.
  - Поддержка таблиц разделов с OTA-обновлениями по воздуху.

---

## 📦 Установка

### Вариант 1: PlatformIO (Рекомендуемый)

Скопируйте папку `GeminiESP32` в директорию `lib/` вашего проекта:

```
my_project/
├── lib/
│   └── GeminiESP32/
│       ├── library.json
│       ├── src/
│       └── README.md
├── src/
│   └── main.cpp
└── platformio.ini
```

В `platformio.ini` добавьте зависимость `ArduinoJson`:
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps =
    bblanchon/ArduinoJson @ ^7.0.4
```

### Вариант 2: Arduino IDE

1. Скачайте папку `GeminiESP32` в виде ZIP-архива.
2. В Arduino IDE выберите: **Скетч -> Подключить библиотеку -> Добавить .ZIP библиотеку...**
3. Установите через менеджер библиотек зависимость **ArduinoJson** (версии 7.x).

---

## 🚀 Быстрый старт (Quick Start)

```cpp
#include <WiFi.h>
#include <GeminiESP32.h>

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* geminiKey = "YOUR_GEMINI_API_KEY";

// Создаем объект ИИ с моделью по умолчанию gemini-3.5-flash-lite
GeminiESP32 ai(geminiKey, "gemini-3.5-flash-lite");

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi подключен!");

    // Инициализация библиотеки (настройка Smart DNS, NVS и квот)
    ai.begin();

    // Отправка текстового запроса:
    String response = ai.ask("Объясни в двух предложениях принцип работы ШИМ.");
    Serial.println("\n[Ответ Gemini]:");
    Serial.println(response);
}

void loop() {}
```

---

## 🔌 Физическое управление железом через ИИ

ИИ осознает себя ядром платы ESP32 и может исполнять аппаратные команды.

```cpp
#include <WiFi.h>
#include <GeminiESP32.h>

GeminiESP32 ai("YOUR_GEMINI_API_KEY", "gemini-3.5-flash-lite");

void setup() {
    Serial.begin(115200);
    WiFi.begin("SSID", "PASSWORD");
    while (WiFi.status() != WL_CONNECTED) delay(500);

    ai.begin();

    // Включаем аппаратное управление
    ai.enableHardwareControl(true);

    // Запрос с физическим действием:
    // ИИ поймет команду, включит синий светодиод на GPIO 2 и вернет отчет о температуре кристалла
    GeminiResponse resp = ai.query("Включи синий светодиод на плате и замерь температуру кристалла");

    Serial.println(resp.text);
    Serial.printf("Запрос выполнен за %lu мс. Токенов: %d\n", resp.durationMs, resp.totalTokens);
}

void loop() {}
```

---

## 📖 Справочник API (API Reference)

### Класс `GeminiESP32`

#### Конструкторы и инициализация:
- `GeminiESP32(const String& apiKey = "", const String& model = "gemini-3.5-flash-lite")` — конструктор.
- `void begin(const String& apiKey = "")` — инициализация внутренних систем, загрузка кэша и настройка DNS.

#### Настройка параметров:
- `void setApiKey(const String& apiKey)` — установить или сменить API-ключ Google AI.
- `void setModel(const String& model)` — выбрать модель (`gemini-3.5-flash-lite`, `gemini-2.0-flash`, `gemini-1.5-pro` и др.).
- `void setSystemPrompt(const String& prompt)` — задать роль или инструкцию поведения ИИ.
- `void setTemperature(float temp)` — установить температуру креативности (от 0.0 до 2.0, по умолчанию 0.7).
- `void setMaxTokens(int maxTokens)` — ограничение длины ответа (по умолчанию 1024 токена).
- `void setSmartDns(const char* primary = "111.88.96.50", const char* secondary = "111.88.96.51")` — настройка DNS серверов.
- `void enableHardwareControl(bool enable = true)` — вкл/выкл моста управления физическими контактами платы.

#### Выполнение запросов:
- `String ask(const String& prompt)` — простой синхронный запрос, возвращает строку с текстом ответа.
- `GeminiResponse query(const String& prompt)` — детальный запрос, возвращающий структуру с метаданными.
- `bool ping()` — быстрая проверка соединения с API Google AI Studio.

#### Доступ к подсистемам:
- `HardwareController& getHardware()` — прямой доступ к контроллеру GPIO/ADC/I2C/телеметрии.
- `UsageTracker& getUsage()` — доступ к трекеру суточных лимитов и токенов.
- `ConfigManager& getConfig()` — доступ к NVS хранилищу настроек.
- `GeminiClient& getClient()` — низкоуровневый HTTPS клиент.

---

### Структуры данных

#### `GeminiResponse`
```cpp
struct GeminiResponse {
    bool success;             // true, если запрос выполнен успешно
    int httpCode;             // HTTP статус (200, 400, 403, 429, 500 и т.д.)
    String text;              // Текст ответа или подробное описание ошибки
    int promptTokens;         // Количество токенов во входном запросе
    int candidateTokens;      // Количество токенов в ответе модели
    int totalTokens;          // Суммарный расход токенов
    unsigned long durationMs; // Время генерации ответа в миллисекундах
};
```

#### `DeviceTelemetry`
```cpp
struct DeviceTelemetry {
    float chipTempC;             // Температура кремниевого кристалла ESP32 (°C)
    uint32_t freeHeapBytes;      // Свободная оперативная память RAM (байт)
    uint32_t minFreeHeapBytes;   // Минимальный остаток памяти в истории
    uint32_t heapSizeBytes;      // Общий размер кучи (heap)
    uint32_t uptimeSec;          // Аптайм работы устройства в секундах
    int wifiRssi;                // Мощность Wi-Fi сигнала (dBm)
    uint32_t cpuFreqMHz;         // Частота процессора (МГц)
    uint32_t flashSizeBytes;     // Объем Flash-памяти чипа (байт)
};
```

#### `DailyUsageStats`
```cpp
struct DailyUsageStats {
    uint32_t dailyRequestLimit;   // Суточный лимит запросов (по умолчанию 1500 RPD)
    uint32_t requestsToday;       // Запросов сделано сегодня
    uint32_t promptTokensToday;   // Входных токенов сегодня
    uint32_t responseTokensToday; // Выходных токенов сегодня
    uint32_t totalTokensToday;    // Суммарно токенов за сегодня
    uint32_t lifetimeRequests;    // Всего запросов за все время
    uint64_t lifetimeTokens;      // Всего токенов за все время
};
```

---

## 🛠️ Прямое управление железом через `HardwareController`

```cpp
HardwareController& hw = ai.getHardware();

// Управление цифровыми пинами
hw.writePin(2, HIGH);         // Включить GPIO 2
int state = hw.readPin(4);    // Прочитать логический уровень на GPIO 4
hw.togglePin(2);              // Инвертировать состояние пина

// Чтение аналоговых входов (ADC1: 32-39)
int adcRaw = hw.readAnalogPin(34); // Значение 0-4095 (0 - 3.3 В)

// Сканирование шины I2C
String i2cDevices = hw.scanI2C(21, 22); // SDA=21, SCL=22
Serial.println(i2cDevices);

// Получение телеметрии чипа
DeviceTelemetry t = hw.getTelemetry();
Serial.printf("Температура кристалла: %.1f °C, Свободно RAM: %u байт\n", t.chipTempC, t.freeHeapBytes);
```

---

## 📊 Учет суточных квот через `UsageTracker`

```cpp
UsageTracker& usage = ai.getUsage();

// Синхронизация времени по NTP (для сброса в 00:00)
usage.syncNTP(3); // UTC+3 (Москва)

// Проверка лимита перед тяжелыми операциями
if (usage.isLimitReached()) {
    Serial.println("Суточный лимит запросов исчерпан!");
}

// Красивый сводный отчет о расходе квот в Serial
usage.printQuotaReport();
```

---

## 💡 Архитектура потребления Flash памяти ESP32

### Почему HTTPS/TLS прошивка занимает ~900 КБ Flash?

Прошивка ESP32 с поддержкой защищенных HTTPS-соединений включает в себя аппаратные и системные библиотеки уровня операционной системы:

| Компонент | Примерный размер Flash | Назначение |
|---|---|---|
| **mbedTLS / Cryptography** | ~380–450 КБ | Аппаратное шифрование AES, SHA256, RSA 2048/4096, эллиптические кривые ECC, X.509 сертификаты для HTTPS. |
| **Wi-Fi & TCP/IP (LwIP)** | ~220–280 КБ | Драйвер радиоканала 802.11 b/g/n, WPA2/WPA3 PMF, сетевой стек TCP/UDP, DNS, DHCP. |
| **FreeRTOS & ESP-IDF Core** | ~140–180 КБ | Планировщик задач реального времени, переключение контекста ядер, прерывания, Flash SPI контроллер. |
| **Arduino Core & C++ Runtime** | ~90–120 КБ | `String`, `Stream`, `Print`, `HardwareSerial`, виртуальные таблицы и аллокатор памяти. |
| **ArduinoJson & Gemini Client** | ~40–60 КБ | Парсер потокового JSON, логика работы с API и контроллер железа. |

> [!NOTE]
> В микроконтроллерах Flash-память (4 МБ) — это ПЗУ (хранилище бинарного кода программы), а не оперативная память. При размере бинарника ~985 КБ в разделе 1.5 МБ остается более **650 КБ свободного места** под дальнейшее расширение функционала и OTA-обновления.

---

## 📄 Лицензия

Проект распространяется под свободной лицензией **MIT License**.
Разработано для сообщества разработчиков IoT и встроенных систем.
