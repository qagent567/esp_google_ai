#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <lwip/dns.h>
#include <lwip/ip_addr.h>

// Подключение внутренних модулей библиотеки
#include "ConfigManager.h"
#include "HardwareController.h"
#include "UsageTracker.h"
#include "GeminiClient.h"

/**
 * @brief Главный высокоуровневый класс библиотеки GeminiESP32
 * Позволяет в 3-5 строк кода подключить Gemini AI к любому проекту ESP32.
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
     * @brief Инициализация и настройка Smart DNS
     */
    void begin(const String& apiKey = "");

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
     * @brief Настройка максимального количества токенов
     */
    void setMaxTokens(int maxTokens);

    /**
     * @brief Настройка Smart DNS для обхода региональных ограничений
     */
    void setSmartDns(const char* primary = "111.88.96.50", const char* secondary = "111.88.96.51");

    /**
     * @brief Включение/выключение аппаратного управления платой (GPIO/ADC/I2C)
     */
    void enableHardwareControl(bool enable = true);

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
     * @brief Проверка доступности Google AI Studio API (Ping)
     */
    bool ping();

    /**
     * @brief Получить доступ к аппаратному контроллеру
     */
    HardwareController& getHardware() { return _hardware; }

    /**
     * @brief Получить доступ к трекеру суточных квот
     */
    UsageTracker& getUsage() { return _usage; }

    /**
     * @brief Получить доступ к конфигурации
     */
    ConfigManager& getConfig() { return _config; }

    /**
     * @brief Получить низкоуровневый клиент Gemini
     */
    GeminiClient& getClient() { return *_client; }

private:
    ConfigManager _config;
    HardwareController _hardware;
    UsageTracker _usage;
    GeminiClient* _client;
    bool _hardwareEnabled;
    bool _smartDnsConfigured;
};
