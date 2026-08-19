# 📋 Changelog — GeminiESP32

Все значимые изменения библиотеки документируются в этом файле.  
Формат основан на [Keep a Changelog](https://keepachangelog.com/ru/1.0.0/).  
Версионирование следует [Semantic Versioning](https://semver.org/lang/ru/).

---

## [1.0.0] — 2026-08-19

### 🎉 Первый публичный релиз

#### Добавлено
- **Базовый Gemini API клиент** — синхронные запросы через HTTPS/TLS к Google AI Studio
- **Потоковая генерация** (SSE Streaming) — посимвольный вывод ответа по мере генерации
- **Асинхронные запросы** (FreeRTOS) — `askAsync`, `queryAsync`, `streamAskAsync` без блокировки `loop()`
- **Gemini Vision** — отправка изображений (JPEG/PNG) с ESP32-CAM для визуального анализа
- **Нативный Function Calling** (Tool Use) — реестр `FunctionRegistry` для вызова C++ функций моделью
- **Встроенный Web Dashboard** — локальный HTTP-сервер с адаптивным чатом и мониторингом телеметрии
- **Аппаратный контроллер** — безопасное управление GPIO, ADC1, I2C, чтение температуры чипа
- **Белый список пинов** — изоляция от конфликтов с другими компонентами прошивки
- **NVS история** — сохранение контекста диалога во Flash-память между перезагрузками ESP32
- **Трекер квот** — суточный и минутный учёт запросов RPD/RPM с NVS-персистентностью
- **Smart DNS** — автоматическая настройка DNS-серверов для обхода региональных ограничений
- **GeminiConfig.h** — мастер-файл с 8 препроцессорными флагами для модульной сборки
- **8 готовых примеров**: BasicChat, HardwareControl, Integration, AsyncChat, StreamingChat, FunctionCalling, WebDashboard, Vision_ESP32_CAM

#### Поддерживаемые платы
- ESP32 DevKit v1
- ESP32-S3
- AI Thinker ESP32-CAM (OV2640)

#### Зависимости
- [ArduinoJson](https://arduinojson.org/) >= 7.0.0

---

## Планируется в следующих релизах

### [1.1.0] — (в разработке)
- Поддержка ESP32-C3 (RISC-V, без FPU)
- Сжатие HTML/CSS Dashboard с помощью gzip PROGMEM
- WebSocket-канал для реального времени в Dashboard
- MQTT интеграция для Home Assistant Auto-Discovery

### [2.0.0] — (долгосрочно)
- Multi-modal Chat History (хранение фото в SPIFFS)
- OTA-обновление прошивки через Web Dashboard
- Поддержка нескольких параллельных API-ключей (ротация при исчерпании квоты)
