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
    int totalTokens;        // Использовано токенов (если доступно)
    unsigned long durationMs; // Время выполнения запроса в миллисекундах
};

/**
 * @brief Клиент для взаимодействия с Google AI Studio (Gemini API)
 */
class GeminiClient {
public:
    GeminiClient(ConfigManager& configMgr);

    // Отправка текстового запроса (промпта) к Gemini
    GeminiResponse ask(const String& prompt);

    // Получение и вывод списка доступных моделей через API
    bool listAvailableModels();

    // Проверка доступности хоста API (DNS резолвинг и TLS пинг)
    bool testConnection();

    // Получить описание ошибки по HTTP коду
    static String getHttpErrorDescription(int httpCode);

private:
    ConfigManager& _configMgr;

    // Формирование URL эндпоинта
    String buildApiUrl() const;

    // Формирование JSON полезной нагрузки
    String buildRequestBody(const String& prompt) const;

    // Парсинг JSON ответа от Google API с оптимизацией памяти
    bool parseResponse(const String& jsonPayload, GeminiResponse& response);
};
