#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>
#include <functional>
#include "ConfigManager.h"

#ifndef GEMINI_HISTORY_LIMIT
#define GEMINI_HISTORY_LIMIT 10
#endif

/**
 * @brief Результат выполнения запроса к Gemini API
 */
struct GeminiResponse {
    bool success;             // Флаг успешности
    int httpCode;             // HTTP статус-код (200, 400, 403, 500 и т.д.)
    String text;              // Текст ответа нейросети или текст ошибки
    int promptTokens;         // Токенов во входящем запросе
    int candidateTokens;      // Токенов в ответе нейросети
    int totalTokens;          // Суммарно использовано токенов
    unsigned long durationMs; // Время выполнения запроса в миллисекундах
};

/**
 * @brief Сообщение в истории чата
 */
struct ChatMessage {
    String role;
    String text;
};

// Типы функций обратного вызова (Callbacks)
typedef std::function<void(const GeminiResponse& response)> GeminiResponseCallback;
typedef std::function<void(const String& answer)> GeminiTextCallback;
typedef std::function<void(const String& chunk, bool isLast)> GeminiStreamCallback;

class UsageTracker;
class HardwareController;

/**
 * @brief Клиент для взаимодействия с Google AI Studio (Gemini API)
 */
class GeminiClient {
public:
    GeminiClient(ConfigManager& configMgr, UsageTracker* usageTracker = nullptr, HardwareController* hwController = nullptr);

    // Установка указателей на внешние модули
    void setUsageTracker(UsageTracker* tracker) { _usageTracker = tracker; }
    void setHardwareController(HardwareController* hw) { _hwController = hw; }

    // Определение суточного лимита модели (RPD)
    static uint32_t getModelDailyLimit(const String& modelId);

    // --- Управление историей диалога ---
    void clearHistory();
    const std::vector<ChatMessage>& getHistory() const { return _history; }
    void addHistory(const String& role, const String& text);

    // Сохранение и загрузка истории из Flash (NVS)
    void enablePersistentHistory(bool enable = true);
    bool isPersistentHistoryEnabled() const { return _persistentHistory; }
    void loadHistoryFromNvs();
    void saveHistoryToNvs();

    // --- Синхронные запросы ---
    // Отправка текстового запроса (промпта) к Gemini
    GeminiResponse ask(const String& prompt);

    // Потоковый запрос (Server-Sent Events / SSE Streaming)
    GeminiResponse streamAsk(const String& prompt, GeminiStreamCallback onChunk);

    // Получение и вывод списка доступных моделей через API
    bool listAvailableModels();

    // Загрузка и сохранение кэша моделей в NVS
    void loadCachedModels();
    void saveCachedModels();

    // Проверка доступности хоста API (DNS резолвинг и TLS пинг)
    bool testConnection();

    // Получить описание ошибки по HTTP коду
    static String getHttpErrorDescription(int httpCode);

    // Формирование URL эндпоинта
    String buildApiUrl() const;
    String buildStreamApiUrl() const;

    // Формирование JSON полезной нагрузки
    String buildRequestBody(const String& prompt) const;

    // Парсинг JSON ответа от Google API с оптимизацией памяти
    bool parseResponse(const String& jsonPayload, GeminiResponse& response);

    // Проверка и выполнение аппаратных действий (actions) в ответе модели
    void processHardwareActions(GeminiResponse& response);

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
    HardwareController* _hwController;
    std::vector<String> _cachedModels;
    std::vector<ChatMessage> _history;
    bool _persistentHistory;
};
