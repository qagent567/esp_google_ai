# 🚀 GeminiESP32

[![PlatformIO Registry](https://img.shields.io/badge/PlatformIO-GeminiESP32-orange.svg)](https://platformio.org/)
[![Arduino Library](https://img.shields.io/badge/Arduino-Library-blue.svg)](https://www.arduino.cc/)
[![Target MCU](https://img.shields.io/badge/MCU-ESP32-red.svg)](https://espressif.com/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

**GeminiESP32** — автономная C++ библиотека для интеграции Google Gemini AI в проекты на **ESP32**. Разработана с упором на минимальный расход памяти, полную изоляцию от другой периферии и простое встраивание в готовые прошивки.

---

## 📋 Содержание

- [Ключевые возможности](#-ключевые-возможности)
- [Установка](#-установка)
- [Получение API-ключа](#-получение-api-ключа)
- [Быстрый старт](#-быстрый-старт)
- [Настройка флагов компиляции](#-настройка-флагов-компиляции)
- [Полный справочник API](#-полный-справочник-api)
- [Физическое управление железом (Physical AI)](#-физическое-управление-железом-physical-ai)
- [Встраивание в готовую прошивку](#-встраивание-в-готовую-прошивку)
- [Защита пинов от конфликтов](#-защита-пинов-от-конфликтов)
- [Управление суточными квотами](#-управление-суточными-квотами)
- [Smart DNS — работа без VPN](#-smart-dns--работа-без-vpn)
- [Архитектура расхода Flash-памяти](#-архитектура-расхода-flash-памяти)
- [Примеры](#-примеры)

---

## 🌟 Ключевые возможности

| Возможность | Описание |
|---|---|
| ⚡ **3 строки до работы** | Минимальный код для интеграции ИИ |
| 🤖 **Physical AI** | ИИ управляет GPIO, читает ADC, сканирует I2C |
| 🛡️ **Изоляция пинов** | Белый список защищает вашу периферию |
| 🌐 **Smart DNS** | Прямой доступ к Google API без VPN |
| 📊 **Учет квот** | Автоматический трекер 1500 RPD лимита |
| 💾 **RAM-оптимизация** | Фильтрованный JSON парсинг, нет утечек кучи |
| 🔁 **Авто-повтор** | Retry при сбоях сети (500, 503, timeout) |
| 💡 **NVS кэш** | Сохранение настроек в Flash без перепрошивки |

---

## 📦 Установка

### PlatformIO (рекомендуется)

Скопируйте папку `GeminiESP32` в директорию `lib/` вашего проекта:

```
my_project/
├── lib/
│   └── GeminiESP32/     ← сюда
│       ├── library.json
│       ├── src/
│       └── examples/
├── src/
│   └── main.cpp
└── platformio.ini
```

В `platformio.ini` добавьте зависимость:

```ini
[env:esp32dev]
platform = espressif32
board    = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps =
    bblanchon/ArduinoJson @ ^7.0.4
```

### Arduino IDE

1. Скачайте папку `GeminiESP32` как ZIP-архив.
2. **Скетч → Подключить библиотеку → Добавить .ZIP библиотеку...**
3. Через менеджер библиотек установите **ArduinoJson** версии 7.x.

---

## 🔑 Получение API-ключа

1. Перейдите на [aistudio.google.com](https://aistudio.google.com/)
2. Нажмите **Get API Key** → **Create API key**
3. Скопируйте ключ (начинается с `AIza...`)
4. Бесплатный план (Free Tier): **1500 запросов/сутки**, без карты

> **Доступные модели Free Tier:**
> | Модель | Скорость | Интеллект | Рекомендуется |
> |---|---|---|---|
> | `gemini-3.5-flash-lite` | ⚡ Очень быстро | ★★★ | ESP32 ✅ |
> | `gemini-2.0-flash` | 🚀 Быстро | ★★★★ | ESP32 ✅ |
> | `gemini-1.5-flash` | 🔥 Средне | ★★★★ | — |
> | `gemini-1.5-pro` | 🐢 Медленно | ★★★★★ | — |

---

## 🚀 Быстрый старт

```cpp
#include <WiFi.h>
#include <GeminiESP32.h>

GeminiESP32 ai("ВАШ_API_КЛЮЧ", "gemini-3.5-flash-lite");

void setup() {
    Serial.begin(115200);
    WiFi.begin("SSID", "ПАРОЛЬ");
    while (WiFi.status() != WL_CONNECTED) delay(500);

    ai.begin();  // Инициализация (Smart DNS + NVS + квоты)

    String answer = ai.ask("Что такое ESP32?");
    Serial.println(answer);
}

void loop() {}
```

---

## ⚙️ Настройка флагов компиляции

Все флаги задаются в `platformio.ini` через `build_flags`. Позволяют менять поведение библиотеки **без изменения её кода**.

```ini
[env:esp32dev]
platform  = espressif32
board     = esp32dev
framework = arduino
board_build.partitions = partitions_ota.csv
lib_deps  = bblanchon/ArduinoJson @ ^7.0.4

build_flags =
    ; ─── Флаги оптимизации размера (уменьшают Flash на 5-15%) ──────────────
    -Os                    ; Оптимизация по минимальному размеру кода
    -ffunction-sections    ; Каждая функция в отдельной секции
    -fdata-sections        ; Каждая переменная в отдельной секции
    -Wl,--gc-sections      ; Линкер удаляет весь неиспользуемый код!
    
    ; ─── Уровень логирования ESP-IDF ────────────────────────────────────────
    ; 0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=VERBOSE
    -D CORE_DEBUG_LEVEL=0  ; Выключить отладку ESP-IDF (освобождает ~10 КБ RAM)
    
    ; ─── Настройки GeminiESP32 ──────────────────────────────────────────────
    ; Раскомментируйте нужные флаги:
    
    ; Отключить Smart DNS глобально (использовать DNS из Wi-Fi роутера)
    ; -D GEMINI_NO_SMART_DNS
    
    ; Максимальный размер буфера ответа в байтах (по умолчанию 8192)
    ; Уменьшите если не хватает RAM, увеличьте для длинных ответов
    ; -D GEMINI_RESPONSE_BUFFER=4096
    
    ; Максимальная длина истории сообщений (по умолчанию 20)
    ; -D GEMINI_HISTORY_LIMIT=10
    
    ; Количество авто-повторов при ошибках сети (по умолчанию 2)
    ; -D GEMINI_RETRY_COUNT=3
    
    ; Таймаут HTTP-запроса в миллисекундах (по умолчанию 15000)
    ; -D GEMINI_HTTP_TIMEOUT_MS=20000
    
    ; Отключить NVS кэш моделей (если не нужно сохранение настроек)
    ; -D GEMINI_NO_NVS_CACHE
    
    ; Суточный лимит запросов (по умолчанию 1500 для Free Tier)
    ; -D GEMINI_DAILY_LIMIT=500
```

> **Примечание по `-Wl,--gc-sections`:** Этот флаг — самый мощный инструмент экономии Flash.
> Линкер автоматически удалит функции `scanI2C()`, `listAvailableModels()` и другие,
> если они не вызываются в вашем скетче. Экономия: 15-40 КБ.

---

## 📖 Полный справочник API

### `GeminiESP32` — основной класс

#### Конструктор

```cpp
GeminiESP32 ai(
    const String& apiKey = "",            // API-ключ Google AI Studio
    const String& model  = "gemini-3.5-flash-lite"  // Модель по умолчанию
);
```

#### Инициализация

```cpp
// Инициализация с автоматическим Smart DNS (рекомендуется)
ai.begin();
ai.begin("apiKey");              // Можно передать ключ здесь
ai.begin("apiKey", true);        // true = Smart DNS включен
ai.begin("apiKey", false);       // false = использовать DNS из роутера
```

#### Настройка модели и генерации

```cpp
ai.setApiKey("AIza...");                    // Установить или сменить API-ключ
ai.setModel("gemini-2.0-flash");            // Переключить модель
ai.setSystemPrompt("Ты технический ассистент..."); // Задать роль ИИ
ai.setTemperature(0.7f);     // 0.0 = точно, 1.0 = balanced, 2.0 = творчески
ai.setMaxTokens(512);        // Макс. длина ответа в токенах (1 токен ≈ 4 символа)
ai.clearHistory();           // Очистить историю контекста диалога (по умолчанию хранит 10 сообщений)
```

#### Отправка запросов

```cpp
// Простой вызов — возвращает только текст ответа
String answer = ai.ask("Вопрос...");

// Детальный вызов — возвращает всю метаинформацию
GeminiResponse resp = ai.query("Вопрос...");
if (resp.success) {
    Serial.println(resp.text);            // Текст ответа
    Serial.println(resp.httpCode);        // HTTP 200
    Serial.println(resp.totalTokens);     // Потрачено токенов
    Serial.println(resp.durationMs);      // Время ответа в мс
}

// ⚡ Потоковый вывод (Streaming / Server-Sent Events)
// Текст выводится фрагментами по мере генерации на серверах Google!
ai.streamAsk("Расскажи сказку", [](const String& chunk, bool isLast) {
    Serial.print(chunk); // Вывод в Serial / OLED по кусочкам
});

// 🚀 Неблокирующие асинхронные запросы (FreeRTOS Background Task)
// loop() не зависает и продолжает управлять железом!
ai.askAsync("Включи свет", [](const String& answer) {
    Serial.println("[ИИ ответил]: " + answer);
});

// Проверка занятости фонового потока
if (ai.isBusy()) {
    // ИИ сейчас генерирует ответ в фоне
}

// Сохранение контекста во Flash NVS (помнит диалог после рестарта)
ai.enablePersistentHistory(true);

// Проверка доступности API (без запроса к ИИ)
bool ok = ai.ping();
```

#### Управление аппаратурой

```cpp
// Включение/выключение всего аппаратного моста
ai.enableHardwareControl(true);   // ИИ может управлять GPIO/ADC/I2C
ai.enableHardwareControl(false);  // ИИ отвечает только текстом

// Белый список пинов (только эти GPIO разрешены для ИИ)
ai.setAllowedPins({2, 4, 5});     // ИИ может трогать только GPIO 2, 4, 5
ai.allowAllSafePins();            // Сброс белого списка (все безопасные GPIO)
```

#### Управление Smart DNS

```cpp
ai.enableSmartDns(true);          // Включить Smart DNS (по умолчанию)
ai.enableSmartDns(false);         // Выключить (использовать DNS роутера)

// Задать свои DNS-серверы вручную
ai.setSmartDns("8.8.8.8", "1.1.1.1");           // Google Public DNS
ai.setSmartDns("1.1.1.1", "1.0.0.1");           // Cloudflare DNS
ai.setSmartDns("111.88.96.50", "111.88.96.51"); // Smart DNS (xbox-dns.ru)
```

#### Доступ к подсистемам

```cpp
HardwareController& hw     = ai.getHardware(); // GPIO/ADC/I2C контроллер
UsageTracker&       usage  = ai.getUsage();    // Трекер квот и токенов
ConfigManager&      config = ai.getConfig();   // NVS хранилище настроек
GeminiClient&       client = ai.getClient();   // Низкоуровневый HTTP клиент
```

---

### Структуры данных

#### `GeminiResponse`

```cpp
struct GeminiResponse {
    bool          success;         // true = запрос выполнен успешно
    int           httpCode;        // HTTP статус (200, 400, 403, 429, 500...)
    String        text;            // Текст ответа или описание ошибки
    int           promptTokens;    // Токены входящего запроса
    int           candidateTokens; // Токены в ответе модели
    int           totalTokens;     // Итого токенов за запрос
    unsigned long durationMs;      // Время выполнения запроса в мс
};
```

#### `DeviceTelemetry`

```cpp
struct DeviceTelemetry {
    float    chipTempC;        // Температура кристалла ESP32 (°C)
    uint32_t freeHeapBytes;    // Свободная RAM (байт)
    uint32_t minFreeHeapBytes; // Минимальный остаток RAM в истории
    uint32_t heapSizeBytes;    // Полный размер кучи
    uint32_t uptimeSec;        // Аптайм с момента запуска (секунды)
    int      wifiRssi;         // Уровень Wi-Fi сигнала (dBm)
    uint32_t cpuFreqMHz;       // Частота CPU (обычно 240 МГц)
    uint32_t flashSizeBytes;   // Объем Flash-памяти чипа
};
```

#### `DailyUsageStats`

```cpp
struct DailyUsageStats {
    uint32_t dailyRequestLimit;   // Лимит запросов в сутки (1500 Free Tier)
    uint32_t requestsToday;       // Запросов сделано сегодня
    uint32_t promptTokensToday;   // Входных токенов сегодня
    uint32_t responseTokensToday; // Выходных токенов сегодня
    uint32_t totalTokensToday;    // Суммарно токенов за сутки
    uint32_t lifetimeRequests;    // Всего запросов за все время
    uint64_t lifetimeTokens;      // Всего токенов за все время
};
```

---

## 🔌 Физическое управление железом (Physical AI)

ИИ может самостоятельно управлять платой, включая JSON-команды в свой ответ.

### Прямые команды из кода

```cpp
HardwareController& hw = ai.getHardware();

// Цифровые пины
hw.writePin(2, HIGH);            // Включить GPIO 2
hw.writePin(2, LOW);             // Выключить GPIO 2
hw.togglePin(2);                 // Инвертировать GPIO 2
hw.setPinMode(4, INPUT_PULLUP);  // Настроить режим пина
int level = hw.readPin(4);       // Считать логический уровень (0 или 1)

// Аналоговые входы ADC1 (только GPIO 32, 33, 34, 35, 36, 39)
int raw = hw.readAnalogPin(34);              // 0–4095
float volts = (raw / 4095.0f) * 3.3f;       // Перевод в вольты

// Телеметрия чипа
DeviceTelemetry t = hw.getTelemetry();
Serial.printf("Температура: %.1f °C, RAM: %u байт\n", t.chipTempC, t.freeHeapBytes);

// I2C сканирование (найдет BMP280, SHT21, OLED, MPU-6050 и другие)
String found = hw.scanI2C(21, 22); // SDA=GPIO21, SCL=GPIO22
Serial.println(found);

// Выполнение JSON-команды напрямую (тот же формат, что ИИ использует внутри)
hw.executeActionJson("{\"action\":\"set_pin\",\"pin\":2,\"value\":1}");
```

### Команды ИИ (Action Blocks)

Когда ИИ решает управлять железом, он включает в ответ блок вида:
````action {"action": "set_pin", "pin": 2, "value": 1}````

Библиотека автоматически находит и исполняет эти блоки. Поддерживаемые команды:

| Команда JSON | Описание |
|---|---|
| `{"action":"set_pin","pin":2,"value":1}` | Установить GPIO 2 в HIGH |
| `{"action":"read_pin","pin":4}` | Считать состояние GPIO 4 |
| `{"action":"toggle_pin","pin":2}` | Инвертировать GPIO 2 |
| `{"action":"read_analog","pin":34}` | Считать АЦП на GPIO 34 (0–4095) |
| `{"action":"get_telemetry"}` | Телеметрия чипа (температура, RAM, аптайм) |
| `{"action":"scan_i2c","sda":21,"scl":22}` | Сканирование шины I2C |
| `{"action":"restart"}` | Программная перезагрузка ESP32 |

---

## 🏗️ Встраивание в готовую прошивку

### Правильный порядок интеграции

```cpp
#include <GeminiESP32.h>

// 1. Создаем объект (не занимает заметной RAM — просто структура)
GeminiESP32 ai("API_KEY", "gemini-3.5-flash-lite");

void setup() {
    // 2. Сначала инициализируем ВАШУ периферию
    pinMode(5, OUTPUT);          // Реле
    Wire.begin(21, 22);          // I2C для вашего датчика
    SPI.begin();                 // SPI для вашего дисплея
    
    // 3. Подключаемся к Wi-Fi (нужно для библиотеки)
    WiFi.begin("SSID", "PASS");
    while (WiFi.status() != WL_CONNECTED) delay(500);

    // 4. Инициализируем ИИ — ПОСЛЕ ВАШЕЙ периферии
    ai.begin();
    
    // 5. Сразу защищаем ваши пины
    ai.setAllowedPins({2, 4});  // ИИ может трогать только GPIO 2 и 4
    
    // 6. При необходимости отключить аппаратный мост целиком:
    // ai.enableHardwareControl(false);  // ИИ = только текстовые ответы
}
```

### Ленивая инициализация (экономия времени запуска)

```cpp
GeminiESP32 ai("API_KEY");
bool aiReady = false;

void ensureAiReady() {
    if (aiReady) return;
    ai.begin();
    ai.setAllowedPins({2});
    aiReady = true;
}

void loop() {
    if (needAiResponse) {
        ensureAiReady();          // ИИ инициализируется только при необходимости
        String r = ai.ask("...");
    }
}
```

---

## 🛡️ Защита пинов от конфликтов

Библиотека **никогда** не трогает пины при инициализации `begin()`. Управление возможно только при явных вызовах.

### Три уровня защиты

```cpp
// Уровень 1: Полное отключение GPIO (только текстовые ответы)
ai.enableHardwareControl(false);

// Уровень 2: Белый список — ИИ работает только с указанными пинами
ai.setAllowedPins({2, 4});   // Только GPIO 2 и GPIO 4

// Уровень 3: Пины автоматически защищены (всегда, изменить нельзя)
// GPIO 1, 3       — UART0 TX/RX (Serial Monitor)
// GPIO 6–11       — Flash SPI (встроенная память)
// GPIO 34–39      — Input Only (запись невозможна аппаратно)
```

### Типичные примеры защиты

```cpp
// ESP32 с SD-картой на GPIO 5, 18, 19, 23
// SPI дисплей на GPIO 14, 15, 27
// Разрешаем ИИ только светодиод и реле:
ai.setAllowedPins({2, 4});

// ESP32 с DHT22 на GPIO 12 и 1-Wire на GPIO 27
// Разрешаем ИИ только индикаторы:
ai.setAllowedPins({2, 13});

// Сброс ограничений (разрешить все безопасные пины):
ai.allowAllSafePins();
```

---

## 📊 Управление суточными квотами

```cpp
UsageTracker& usage = ai.getUsage();

// Синхронизация часов по NTP (нужна для точного сброса квот в 00:00)
// Часовой пояс: 3 = Москва, 9 = Токио, 0 = Лондон/UTC
usage.syncNTP(9);

// Проверка перед отправкой запроса
if (usage.isLimitReached()) {
    Serial.println("Суточный лимит исчерпан. Попробуйте завтра.");
    return;
}

// Запрос статистики
DailyUsageStats stats = usage.getStats();
Serial.printf("Запросов: %u / %u (Осталось: %u)\n",
    stats.requestsToday,
    stats.dailyRequestLimit,
    stats.dailyRequestLimit - stats.requestsToday);
Serial.printf("Токенов сегодня: %u\n", stats.totalTokensToday);
Serial.printf("Всего запросов за все время: %u\n", stats.lifetimeRequests);

// Установить свой лимит (если у вас платный план или хотите быть экономнее)
usage.setDailyLimit(500);  // Не более 500 запросов в сутки

// Вывести читаемый отчет в Serial
usage.printQuotaReport();
```

---

## 🌐 Smart DNS — работа без VPN

Библиотека автоматически настраивает DNS-серверы, обеспечивающие прямой маршрут
к серверам `generativelanguage.googleapis.com` без региональных блокировок.

```cpp
// По умолчанию включен при вызове begin():
ai.begin();                  // Smart DNS включен автоматически
ai.begin("key", true);       // Явное включение

// Отключить (использовать DNS из вашего роутера):
ai.begin("key", false);
// или:
ai.enableSmartDns(false);

// Использовать конкретные DNS-серверы:
ai.setSmartDns("8.8.8.8", "8.8.4.4");    // Google Public DNS
ai.setSmartDns("1.1.1.1", "1.0.0.1");    // Cloudflare
ai.setSmartDns("111.88.96.50", "111.88.96.51");  // Smart DNS (по умолчанию)
```

> **Когда отключать Smart DNS:**
> - Вы используете VPN или корпоративную сеть с собственным DNS
> - API Google доступен напрямую из вашей сети без ограничений
> - Вы хотите использовать локальный DNS-сервер (например, Pi-hole)

---

## 💾 Архитектура расхода Flash-памяти

### Почему прошивка занимает ~985 КБ?

Любой проект ESP32 с HTTPS-соединением включает системные библиотеки ESP-IDF:

| Компонент | Flash | Описание |
|---|---|---|
| **mbedTLS (Крипто)** | ~430 КБ | RSA-4096, AES-256, ECC, X.509 — необходимо для TLS соединения |
| **Wi-Fi Driver + LwIP** | ~260 КБ | 802.11 b/g/n, WPA2/WPA3, TCP/IP, DHCP, DNS |
| **FreeRTOS + ESP-IDF** | ~160 КБ | Планировщик двух ядер, прерывания, Flash SPI |
| **Arduino Core + C++** | ~100 КБ | String, Serial, Wire, SPI и stdlib |
| **ArduinoJson + GeminiESP32** | **~38 КБ** | **Весь код библиотеки** |
| **Ваш скетч** | ~5-20 КБ | Бизнес-логика |

> **Итог:** Из 1.5 МБ раздела (при использовании `partitions_ota.csv`) остается
> свободными **>500 КБ** — достаточно для дальнейшего расширения и OTA-обновлений.

### Рекомендуемый `platformio.ini` для минимального размера

```ini
[env:esp32dev]
platform  = espressif32
board     = esp32dev
framework = arduino
board_build.partitions = partitions_ota.csv
monitor_speed = 115200
lib_deps  = bblanchon/ArduinoJson @ ^7.0.4
build_flags =
    -Os
    -ffunction-sections
    -fdata-sections
    -Wl,--gc-sections
    -D CORE_DEBUG_LEVEL=0
```

---

## 💡 Примеры

| Пример | Описание |
|---|---|
| [BasicChat](examples/BasicChat/BasicChat.ino) | Простой чат: вопрос → ответ через Serial Monitor |
| [HardwareControl](examples/HardwareControl/HardwareControl.ino) | ИИ управляет GPIO, читает ADC, сканирует I2C |
| [Integration](examples/Integration/Integration.ino) | Встраивание ИИ в готовую прошивку без конфликтов |
| [AsyncChat](examples/AsyncChat/AsyncChat.ino) | Неблокирующие асинхронные запросы в фоне FreeRTOS |
| [StreamingChat](examples/StreamingChat/StreamingChat.ino) | Мгновенный потоковый вывод (Server-Sent Events) |

---

## 📄 Лицензия

MIT License — свободное использование, модификация и распространение.
Разработано для IoT и embedded-сообщества.
