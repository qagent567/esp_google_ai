#pragma once

/**
 * =============================================================================
 *                      GeminiESP32 — Главный Фасад Библиотеки
 * =============================================================================
 * Предоставляет высокоуровневый, простой и элегантный C++ интерфейс для
 * подключения искусственного интеллекта Google Gemini к проектам на ESP32.
 *
 * Особенности:
 *  - ⚡ Запуск в 3 строки кода
 *  - 🔒 Бесконфликтная изоляция пинов и шин
 *  - 🌐 Встроенный Web Dashboard
 *  - 🧠 Постоянная память диалога (Flash NVS)
 *  - 🚀 Неблокирующая асинхронность FreeRTOS
 *  - 🌊 Потоковый SSE-стриминг текста
 *  - 👁️ Мультимодальность Gemini Vision (ESP32-CAM)
 *  - ⚙️ Нативный Function Calling (Tool Use)
 * =============================================================================
 */

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <lwip/dns.h>
#include <lwip/ip_addr.h>
#include <vector>
#include <functional>

// Модули конфигурации и ядра
#include "GeminiConfig.h"
#include "ConfigManager.h"

#if GEMINI_ENABLE_HARDWARE
#include "HardwareController.h"
#endif

#if GEMINI_ENABLE_USAGE_TRACKER
#include "UsageTracker.h"
#endif

#if GEMINI_ENABLE_FUNCTION_CALLING
#include "FunctionRegistry.h"
#endif

#if GEMINI_ENABLE_WEB_DASHBOARD
#include "WebDashboard.h"
#endif

#include "GeminiClient.h"

/**
 * @brief Главный высокоуровневый класс библиотеки GeminiESP32
 */
class GeminiESP32 {
public:
    /**
     * @brief Конструктор
     * @param apiKey API-ключ Google AI Studio (необязательно, можно передать в begin)
     * @param model Название модели Gemini (по умолчанию "gemini-3.5-flash-lite")
     */
    GeminiESP32(const String& apiKey = "", const String& model = GEMINI_DEFAULT_MODEL);
    ~GeminiESP32();

    /**
     * @brief Инициализация библиотеки и подсистем
     * @param apiKey Необязательный API-ключ Google AI Studio
     * @param autoSmartDns Автоматически применить Smart DNS для обхода региональных блокировок
     */
    void begin(const String& apiKey = "", bool autoSmartDns = true);

    // ─── Настройка параметров модели ──────────────────────────────────────────
    /**
     * @brief Установка или смена API-ключа
     */
    void setApiKey(const String& apiKey);

    /**
     * @brief Установка используемой модели (напр. "gemini-3.5-flash-lite", "gemini-2.5-flash", "gemini-2.5-pro")
     */
    void setModel(const String& model);

    /**
     * @brief Установка системного промпта (роли/личности ИИ)
     */
    void setSystemPrompt(const String& prompt);

    /**
     * @brief Настройка температуры генерации (0.0 = строго, 2.0 = максимально творчески)
     */
    void setTemperature(float temp);

    /**
     * @brief Настройка часового пояса в часах (для корректного сброса суточных квот в полночь)
     */
    void setTimezone(int tzOffsetHours);

    /**
     * @brief Настройка максимальной длины ответа в токенах (1 токен ≈ 4 символа)
     */
    void setMaxTokens(int maxTokens);

    // ─── Управление историей диалога ──────────────────────────────────────────
    /**
     * @brief Очистить контекст переписки (стереть память диалога)
     */
    void clearHistory();

#if GEMINI_ENABLE_NVS_HISTORY
    /**
     * @brief Включение/выключение сохранения истории во Flash-память NVS
     * Позволяет сохранять память диалога даже после выключения питания или перезагрузки ESP32.
     */
    void enablePersistentHistory(bool enable = true);
#endif

    // ─── Сеть и Smart DNS ─────────────────────────────────────────────────────
    /**
     * @brief Включение/выключение Smart DNS
     */
    void enableSmartDns(bool enable = true);

    /**
     * @brief Ручная настройка IP-адресов Smart DNS серверов
     */
    void setSmartDns(const char* primary = GEMINI_SMART_DNS_PRIMARY, const char* secondary = GEMINI_SMART_DNS_SECONDARY);

#if GEMINI_ENABLE_HARDWARE
    // ─── Аппаратная изоляция и безопасность ───────────────────────────────────
    /**
     * @brief Включение/выключение встроенного управления железом платы (GPIO/ADC/I2C)
     * Если выключено — библиотека никогда не трогает пины и шины вашей платы.
     */
    void enableHardwareControl(bool enable = true);

    /**
     * @brief Настройка белого списка пинов, которыми разрешено управлять ИИ
     * Защищает остальные пины вашей прошивки (SD-карты, экраны, датчики) от вмешательства.
     * @param allowedPins Список номеров GPIO (например, {2, 4, 5})
     */
    void setAllowedPins(const std::vector<uint8_t>& allowedPins);

    /**
     * @brief Разрешить управление всеми безопасными пинами (сброс белого списка)
     */
    void allowAllSafePins();
#endif

    // ─── Синхронные текстовые запросы ─────────────────────────────────────────
    /**
     * @brief Простой синхронный запрос к Gemini (возвращает только текст ответа)
     * @param prompt Вопрос или инструкция для ИИ
     * @return Текст ответа или сообщение об ошибке
     */
    String ask(const String& prompt);

    /**
     * @brief Полный синхронный запрос к Gemini (возвращает статус, токены, время и текст)
     */
    GeminiResponse query(const String& prompt);

#if GEMINI_ENABLE_VISION
    // ─── Мультимодальность (Gemini Vision) ────────────────────────────────────
    /**
     * @brief Простой мультимодальный запрос с изображением (возвращает текст)
     * @param prompt Вопрос по картинке (напр. "Что изображено на фото?")
     * @param imageData Указатель на буфер с байтами изображения (JPEG/PNG)
     * @param imageSize Размер изображения в байтах
     * @param mimeType MIME-тип (по умолчанию "image/jpeg")
     */
    String askWithImage(const String& prompt, 
                        const uint8_t* imageData, 
                        size_t imageSize, 
                        const String& mimeType = "image/jpeg");

    /**
     * @brief Полный мультимодальный запрос со статистикой токенов и времени
     */
    GeminiResponse queryWithImage(const String& prompt, 
                                  const uint8_t* imageData, 
                                  size_t imageSize, 
                                  const String& mimeType = "image/jpeg");
#endif

#if GEMINI_ENABLE_STREAMING
    // ─── Потоковая генерация (SSE Streaming) ──────────────────────────────────
    /**
     * @brief Потоковый запрос (Server-Sent Events)
     * Вызывает callback по мере поступления новых фрагментов текста от Google.
     * @param prompt Вопрос для ИИ
     * @param onChunk Функция обратного вызова (текстовый фрагмент, флаг окончания)
     */
    GeminiResponse streamAsk(const String& prompt, GeminiStreamCallback onChunk);
#endif

#if GEMINI_ENABLE_ASYNC
    // ─── Асинхронные неблокирующие запросы (FreeRTOS) ─────────────────────────
    /**
     * @brief Проверить, выполняется ли в данный момент асинхронный запрос в фоне
     */
    bool isBusy() const { return _isBusy; }

    /**
     * @brief Неблокирующий асинхронный запрос (текстовый колбэк)
     * Запрос выполняется в отдельном потоке FreeRTOS, не мешая работе loop().
     * @param prompt Вопрос для ИИ
     * @param onResponse Колбэк, вызываемый при получении ответа
     * @return true если фоновая задача успешно запущена, false если система занята
     */
    bool askAsync(const String& prompt, GeminiTextCallback onResponse);

    /**
     * @brief Неблокирующий асинхронный запрос (полный ответ со статистикой)
     */
    bool queryAsync(const String& prompt, GeminiResponseCallback onResponse);

#if GEMINI_ENABLE_STREAMING
    /**
     * @brief Неблокирующий потоковый запрос в фоне FreeRTOS
     */
    bool streamAskAsync(const String& prompt, GeminiStreamCallback onChunk, GeminiResponseCallback onComplete = nullptr);
#endif
#endif

#if GEMINI_ENABLE_FUNCTION_CALLING
    // ─── Нативный Function Calling (Tool Use) ─────────────────────────────────
    /**
     * @brief Регистрация C++ функции для вызова моделью Gemini
     * @param name Уникальное имя функции (только латиница и подчеркивания, напр. "set_relay")
     * @param description Подробное описание для нейросети
     * @param params Список аргументов
     * @param handler C++ лямбда/функция-обработчик
     */
    void registerFunction(const String& name, 
                          const String& description, 
                          const std::vector<FunctionParam>& params, 
                          FunctionHandler handler);

    /**
     * @brief Регистрация простой C++ функции без параметров
     */
    void registerFunction(const String& name, 
                          const String& description, 
                          FunctionHandler handler);
#endif

#if GEMINI_ENABLE_WEB_DASHBOARD
    // ─── Встроенный Web Dashboard ─────────────────────────────────────────────
    /**
     * @brief Запуск встроенного веб-сервера со стильным интерфейсом чата и телеметрии
     * @param port Порт веб-сервера (по умолчанию 80)
     * @param inBackground Запуск в фоновом FreeRTOS потоке (не требует вызовов в loop)
     */
    bool startWebDashboard(uint16_t port = 80, bool inBackground = true);

    /**
     * @brief Остановка веб-сервера
     */
    void stopWebDashboard();

    /**
     * @brief Получить доступ к объекту Web Dashboard
     */
    WebDashboard& getDashboard() { return _dashboard; }
#endif

    /**
     * @brief Проверка сетевой доступности Google AI Studio API (Ping)
     */
    bool ping();

    // ─── Прямой доступ к подсистемам ──────────────────────────────────────────
#if GEMINI_ENABLE_HARDWARE
    HardwareController& getHardware() { return _hardware; }
#endif

#if GEMINI_ENABLE_USAGE_TRACKER
    UsageTracker& getUsage() { return _usage; }
#endif

#if GEMINI_ENABLE_FUNCTION_CALLING
    FunctionRegistry& getFunctions() { return _functions; }
#endif

    ConfigManager& getConfig() { return _config; }
    GeminiClient& getClient() { return *_client; }

private:
    ConfigManager _config;

#if GEMINI_ENABLE_HARDWARE
    HardwareController _hardware;
    bool _hardwareEnabled;
#endif

#if GEMINI_ENABLE_USAGE_TRACKER
    UsageTracker _usage;
#endif

#if GEMINI_ENABLE_FUNCTION_CALLING
    FunctionRegistry _functions;
#endif

#if GEMINI_ENABLE_WEB_DASHBOARD
    WebDashboard _dashboard;
#endif

    GeminiClient* _client;
    bool _smartDnsEnabled;

#if GEMINI_ENABLE_ASYNC
    volatile bool _isBusy;
#endif
};
