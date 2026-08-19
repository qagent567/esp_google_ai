#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <lwip/dns.h>
#include <lwip/ip_addr.h>
#include <vector>
#include <functional>

// Подключение внутренних модулей библиотеки
#include "ConfigManager.h"
#include "HardwareController.h"
#include "UsageTracker.h"
#include "FunctionRegistry.h"
#include "WebDashboard.h"
#include "GeminiClient.h"

/**
 * @brief Главный высокоуровневый класс библиотеки GeminiESP32
 * Позволяет в 3-5 строк кода подключить Gemini AI к любому проекту ESP32.
 * Полностью настраиваемый, исключающий любые конфликты с периферией других прошивок.
 */
class GeminiESP32 {
public:
    /**
     * @brief Конструктор
     * @param apiKey API-ключ Google AI Studio
     * @param model Модель Gemini (по умолчанию gemini-3.5-flash-lite)
     */
    GeminiESP32(const String& apiKey = "", const String& model = "gemini-3.5-flash-lite");
    ~GeminiESP32();

    /**
     * @brief Инициализация библиотеки
     * @param apiKey Необязательный API-ключ (если не передан в конструктор)
     * @param autoSmartDns Автоматически применить Smart DNS для обхода ограничений (по умолчанию true)
     */
    void begin(const String& apiKey = "", bool autoSmartDns = true);

    /**
     * @brief Установка API ключа
     */
    void setApiKey(const String& apiKey);

    /**
     * @brief Установка модели Gemini
     */
    void setModel(const String& model);

    /**
     * @brief Установка системного промпта
     */
    void setSystemPrompt(const String& prompt);

    /**
     * @brief Настройка температуры генерации (0.0 - 2.0)
     */
    void setTemperature(float temp);

    /**
     * @brief Настройка часового пояса
     */
    void setTimezone(int tzOffsetHours);

    /**
     * @brief Настройка максимального количества токенов
     */
    void setMaxTokens(int maxTokens);

    // --- Управление историей диалога ---
    /**
     * @brief Очистить историю диалога (контекст)
     */
    void clearHistory();

    /**
     * @brief Включение/выключение сохранения истории во Flash-память NVS
     * Позволяет сохранять память диалога даже после перезагрузки платы ESP32.
     */
    void enablePersistentHistory(bool enable = true);

    // --- Управление сетью и Smart DNS ---
    /**
     * @brief Включение/выключение Smart DNS
     */
    void enableSmartDns(bool enable = true);

    /**
     * @brief Ручная настройка Smart DNS серверов
     */
    void setSmartDns(const char* primary = "111.88.96.50", const char* secondary = "111.88.96.51");

    // --- Аппаратная изоляция и безопасность ---
    /**
     * @brief Включение/выключение аппаратного управления платой (GPIO/ADC/I2C)
     * Если выключено — библиотека никогда не трогает пины и шины платы.
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

    // --- Синхронные запросы ---
    /**
     * @brief Простой синхронный запрос к Gemini (возвращает только текст ответа)
     * @param prompt Вопрос или инструкция для ИИ
     * @return Текст ответа или сообщение об ошибке
     */
    String ask(const String& prompt);

    /**
     * @brief Полный запрос к Gemini (возвращает статус, токены, время и текст)
     */
    GeminiResponse query(const String& prompt);

    /**
     * @brief Мультимодальный запрос с фото (Gemini Vision / ESP32-CAM)
     * @param prompt Вопрос по картинке (например, "Что изображено на фото?")
     * @param imageData Указатель на буфер с изображением (JPEG/PNG)
     * @param imageSize Размер изображения в байтах
     * @param mimeType MIME-тип изображения (по умолчанию "image/jpeg")
     */
    String askWithImage(const String& prompt, 
                        const uint8_t* imageData, 
                        size_t imageSize, 
                        const String& mimeType = "image/jpeg");

    /**
     * @brief Полный мультимодальный запрос с фото
     */
    GeminiResponse queryWithImage(const String& prompt, 
                                  const uint8_t* imageData, 
                                  size_t imageSize, 
                                  const String& mimeType = "image/jpeg");

    /**
     * @brief Потоковый запрос (Server-Sent Events / Streaming)
     * Вызывает callback по мере поступления новых фрагментов текста от Google.
     * @param prompt Вопрос для ИИ
     * @param onChunk Функция обратного вызова (текстовый фрагмент, флаг окончания)
     */
    GeminiResponse streamAsk(const String& prompt, GeminiStreamCallback onChunk);

    // --- Асинхронные запросы (Не блокируют loop() благодаря FreeRTOS) ---
    /**
     * @brief Проверить, выполняется ли в данный момент асинхронный запрос в фоне
     */
    bool isBusy() const { return _isBusy; }

    /**
     * @brief Неблокирующий асинхронный запрос (текстовый колбэк)
     */
    bool askAsync(const String& prompt, GeminiTextCallback onResponse);

    /**
     * @brief Неблокирующий асинхронный запрос (полный ответ со статистикой)
     */
    bool queryAsync(const String& prompt, GeminiResponseCallback onResponse);

    /**
     * @brief Неблокирующий потоковый запрос в фоне FreeRTOS
     */
    bool streamAskAsync(const String& prompt, GeminiStreamCallback onChunk, GeminiResponseCallback onComplete = nullptr);

    // --- Нативный Function Calling (Tool Use) ---
    /**
     * @brief Регистрация C++ функции для вызова моделью Gemini
     * @param name Имя функции (напр. "set_relay")
     * @param description Описание для нейросети
     * @param params Параметры функции
     * @param handler C++ лямбда/функция
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

    // --- Встроенный Web Dashboard ---
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
     * @brief Проверка доступности Google AI Studio API (Ping)
     */
    bool ping();

    // --- Доступ к подсистемам ---
    HardwareController& getHardware() { return _hardware; }
    UsageTracker& getUsage() { return _usage; }
    ConfigManager& getConfig() { return _config; }
    FunctionRegistry& getFunctions() { return _functions; }
    WebDashboard& getDashboard() { return _dashboard; }
    GeminiClient& getClient() { return *_client; }

private:
    ConfigManager _config;
    HardwareController _hardware;
    UsageTracker _usage;
    FunctionRegistry _functions;
    WebDashboard _dashboard;
    GeminiClient* _client;
    bool _hardwareEnabled;
    bool _smartDnsEnabled;
    volatile bool _isBusy;
};
