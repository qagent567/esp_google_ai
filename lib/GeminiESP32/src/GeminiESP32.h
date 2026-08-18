#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <lwip/dns.h>
#include <lwip/ip_addr.h>
#include <vector>

// Подключение внутренних модулей библиотеки
#include "ConfigManager.h"
#include "HardwareController.h"
#include "UsageTracker.h"
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
     * @brief Настройка максимального количества токенов
     */
    void setMaxTokens(int maxTokens);

    /**
     * @brief Включение/выключение Smart DNS
     */
    void enableSmartDns(bool enable = true);

    /**
     * @brief Ручная настройка Smart DNS серверов
     */
    void setSmartDns(const char* primary = "111.88.96.50", const char* secondary = "111.88.96.51");

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
     * @brief Получить прямой доступ к аппаратному контроллеру
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
    bool _smartDnsEnabled;
};
