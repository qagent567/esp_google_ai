#pragma once

/**
 * =============================================================================
 *                      GeminiESP32 — Конфигурация Библиотеки
 * =============================================================================
 * В этом файле задаются флаги включения/отключения подсистем и параметры
 * тонкой настройки производительности, памяти и сетевых таймаутов.
 *
 * Вы можете переопределить любой параметр:
 *   1. В коде вашего скетча ДО подключения #include <GeminiESP32.h>
 *   2. В файле platformio.ini через секцию build_flags = -D ИМЯ_ФЛАГА=ЗНАЧЕНИЕ
 * =============================================================================
 */

// ─── ВЕРСИЯ БИБЛИОТЕКИ ───────────────────────────────────────────────────────
#define GEMINI_ESP32_VERSION        "1.0.0"
#define GEMINI_ESP32_VERSION_MAJOR  1
#define GEMINI_ESP32_VERSION_MINOR  0
#define GEMINI_ESP32_VERSION_PATCH  0

// ─── 1. МОДУЛЬНОСТЬ (Включение / Отключение подсистем) ─────────────────────────

/**
 * @brief Встроенный веб-интерфейс (Web Dashboard)
 * 1 = Включен (предоставляет локальный веб-интерфейс чата и телеметрии)
 * 0 = Полностью вырезан из сборки (экономит ~35-50 КБ Flash и WebServer зависимости)
 */
#ifndef GEMINI_ENABLE_WEB_DASHBOARD
#define GEMINI_ENABLE_WEB_DASHBOARD 1
#endif

/**
 * @brief Аппаратный контроллер (GPIO, ADC, I2C, чтение температуры чипа)
 * 1 = Включен (ИИ может анализировать датчики и управлять пинами)
 * 0 = Полностью вырезан (библиотека работает только как чистый текстовый клиент)
 */
#ifndef GEMINI_ENABLE_HARDWARE
#define GEMINI_ENABLE_HARDWARE 1
#endif

/**
 * @brief Нативный Function Calling (Tool Use)
 * 1 = Включен (позволяет регистрировать C++ функции для вызова моделью)
 * 0 = Вырезан (экономит оперативную память на хранение реестра функций)
 */
#ifndef GEMINI_ENABLE_FUNCTION_CALLING
#define GEMINI_ENABLE_FUNCTION_CALLING 1
#endif

/**
 * @brief Мультимодальность и анализ изображений (Gemini Vision)
 * 1 = Включен (поддержка Base64 кодирования и отправки фото с ESP32-CAM)
 * 0 = Вырезан (если работа с камерой не требуется)
 */
#ifndef GEMINI_ENABLE_VISION
#define GEMINI_ENABLE_VISION 1
#endif

/**
 * @brief Потоковая генерация ответов (Server-Sent Events / SSE Streaming)
 * 1 = Включен (посимвольный вывод ответа по мере генерации токенов)
 * 0 = Вырезан (только синхронные и стандартные запросы)
 */
#ifndef GEMINI_ENABLE_STREAMING
#define GEMINI_ENABLE_STREAMING 1
#endif

/**
 * @brief Неблокирующие асинхронные запросы (FreeRTOS Tasks)
 * 1 = Включен (запросы выполняются в фоновом потоке, не блокируя loop())
 * 0 = Вырезан (только синхронный режим)
 */
#ifndef GEMINI_ENABLE_ASYNC
#define GEMINI_ENABLE_ASYNC 1
#endif

/**
 * @brief Сохранение истории диалога во Flash-память NVS (Preferences)
 * 1 = Включен (контекст диалога сохраняется между перезагрузками)
 * 0 = Вырезан (история хранится только в оперативной памяти RAM)
 */
#ifndef GEMINI_ENABLE_NVS_HISTORY
#define GEMINI_ENABLE_NVS_HISTORY 1
#endif

/**
 * @brief Трекер суточных квот и расхода токенов
 * 1 = Включен (учет суточных лимитов RPD и RPM, предотвращение блокировок API)
 * 0 = Вырезан (прямые запросы без локального подсчета статистики)
 */
#ifndef GEMINI_ENABLE_USAGE_TRACKER
#define GEMINI_ENABLE_USAGE_TRACKER 1
#endif


// ─── 2. ТОНКАЯ НАСТРОЙКА ПАМЯТИ И ЛИМИТОВ ───────────────────────────────────

/**
 * @brief Максимальное количество сообщений в истории диалога
 * Рекомендуемое значение: 6-12 сообщений (для баланса контекста и RAM).
 */
#ifndef GEMINI_HISTORY_LIMIT
#define GEMINI_HISTORY_LIMIT 10
#endif

/**
 * @brief Таймаут HTTPS запроса к Google API в миллисекундах
 */
#ifndef GEMINI_HTTP_TIMEOUT_MS
#define GEMINI_HTTP_TIMEOUT_MS 30000
#endif

/**
 * @brief Размер стека фоновой задачи FreeRTOS для асинхронных запросов (байты)
 * Требуется минимум 8192 байт для безопасной работы TLS/mbedTLS.
 */
#ifndef GEMINI_ASYNC_STACK_SIZE
#define GEMINI_ASYNC_STACK_SIZE 8192
#endif

/**
 * @brief Номер ядра CPU для выполнения асинхронных задач (0 или 1)
 * По умолчанию Ядро 0 (Core 0), оставляя Ядро 1 свободным для Arduino loop().
 */
#ifndef GEMINI_ASYNC_TASK_CORE
#define GEMINI_ASYNC_TASK_CORE 0
#endif

/**
 * @brief Приоритет фоновой задачи FreeRTOS
 */
#ifndef GEMINI_ASYNC_TASK_PRIORITY
#define GEMINI_ASYNC_TASK_PRIORITY 1
#endif

/**
 * @brief Размер стека для фонового Web Dashboard сервера (байты)
 */
#ifndef GEMINI_WEB_STACK_SIZE
#define GEMINI_WEB_STACK_SIZE 4096
#endif

/**
 * @brief Номер ядра CPU для фонового Web Dashboard сервера
 */
#ifndef GEMINI_WEB_TASK_CORE
#define GEMINI_WEB_TASK_CORE 0
#endif


// ─── 3. ЗНАЧЕНИЯ ПО УМОЛЧАНИЮ ────────────────────────────────────────────────

/**
 * @brief Модель Gemini по умолчанию (самая быстрая, легкая и бесплатная)
 */
#ifndef GEMINI_DEFAULT_MODEL
#define GEMINI_DEFAULT_MODEL "gemini-3.5-flash-lite"
#endif

/**
 * @brief Температура генерации по умолчанию (0.0 = строго, 1.0 = креативно)
 */
#ifndef GEMINI_DEFAULT_TEMPERATURE
#define GEMINI_DEFAULT_TEMPERATURE 0.7f
#endif

/**
 * @brief Максимальное количество токенов в ответе по умолчанию
 */
#ifndef GEMINI_DEFAULT_MAX_TOKENS
#define GEMINI_DEFAULT_MAX_TOKENS 1024
#endif

/**
 * @brief Первичный сервер Smart DNS для обхода региональных ограничений
 */
#ifndef GEMINI_SMART_DNS_PRIMARY
#define GEMINI_SMART_DNS_PRIMARY "111.88.96.50"
#endif

/**
 * @brief Вторичный сервер Smart DNS
 */
#ifndef GEMINI_SMART_DNS_SECONDARY
#define GEMINI_SMART_DNS_SECONDARY "111.88.96.51"
#endif
