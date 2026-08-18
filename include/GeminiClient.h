#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "ConfigManager.h"

/**
 * @brief Результат выполнения запроса к Gemini API
 */
struct GeminiResponse {
    bool success;           // Флаг успешности
    int httpCode;           // HTTP статус-код (200, 400, 403, 500 и т.д.)
    String text;            // Текст ответа нейросети или текст ошибки
    int promptTokens;       // Токенов во входящем запросе
    int candidateTokens;    // Токенов в ответе нейросети
    int totalTokens;        // Суммарно использовано токенов
    unsigned long durationMs; // Время выполнения запроса в миллисекундах
};

class UsageTracker;

/**
 * @brief Клиент для взаимодействия с Google AI Studio (Gemini API)
 */
class GeminiClient {
public:
    GeminiClient(ConfigManager& configMgr, UsageTracker* usageTracker = nullptr);

    // Установка указателя на UsageTracker
    void setUsageTracker(UsageTracker* tracker) { _usageTracker = tracker; }

    // Определение суточного лимита модели (RPD)
    static uint32_t getModelDailyLimit(const String& modelId);

    // Отправка текстового запроса (промпта) к Gemini
    GeminiResponse ask(const String& prompt);

    // Получение и вывод списка доступных моделей через API
    bool listAvailableModels();

    // Проверка доступности хоста API (DNS резолвинг и TLS пинг)
    bool testConnection();

    // Получить описание ошибки по HTTP коду
    static String getHttpErrorDescription(int httpCode);

    // Формирование URL эндпоинта
    String buildApiUrl() const;

    // Формирование JSON полезной нагрузки
    String buildRequestBody(const String& prompt) const;

    // Парсинг JSON ответа от Google API с оптимизацией памяти
    bool parseResponse(const String& jsonPayload, GeminiResponse& response);

    // Доступ к кэшированному списку моделей
    size_t getModelCount() const { return _cachedModels.size(); }
    String getModelByIndex(size_t index) const {
        if (index > 0 && index <= _cachedModels.size()) {
            return _cachedModels[index - 1];
        }
        return "";
    }
    const std::vector<String>& getCachedModels() const { return _cachedModels; }

private:
    ConfigManager& _configMgr;
    UsageTracker* _usageTracker;
    std::vector<String> _cachedModels;
};
