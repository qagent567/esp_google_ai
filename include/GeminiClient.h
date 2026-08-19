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
    bool success;             // Флаг успешности
    int httpCode;             // HTTP статус-код (200, 400, 403, 500 и т.д.)
    String text;              // Текст ответа нейросети или текст ошибки
    int promptTokens;         // Токенов во входящем запросе
    int candidateTokens;      // Токенов в ответе нейросети
    int totalTokens;          // Суммарно использовано токенов
    unsigned long durationMs; // Время выполнения запроса в миллисекундах
    int agentStepsExecuted;   // Количество автономных агентных шагов
};

class UsageTracker;
class HardwareController;

/**
 * @brief Клиент для взаимодействия с Google AI Studio (Gemini API) с поддержкой автономного агентного цикла
 */
class GeminiClient {
public:
    GeminiClient(ConfigManager& configMgr, UsageTracker* usageTracker = nullptr, HardwareController* hwController = nullptr);

    // Установка указателей на внешние модули
    void setUsageTracker(UsageTracker* tracker) { _usageTracker = tracker; }
    void setHardwareController(HardwareController* hw) { _hwController = hw; }

    // Определение суточного лимита модели (RPD)
    static uint32_t getModelDailyLimit(const String& modelId);

    // Отправка текстового запроса (промпта) к Gemini (с авто-повтором и автономным агентным контуром)
    GeminiResponse ask(const String& prompt);

    // Автономный многошаговый агентный запрос (ReAct: Thought -> Action -> Observation -> Final Answer)
    GeminiResponse askAgent(const String& goalPrompt, int maxSteps = 3);

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

    // Формирование JSON полезной нагрузки с историей шагов
    String buildRequestBody(const String& prompt, const String& conversationContext = "") const;

    // Парсинг JSON ответа от Google API с оптимизацией памяти
    bool parseResponse(const String& jsonPayload, GeminiResponse& response);

    // Извлечение и исполнение аппаратного действия
    bool extractAndExecuteAction(const String& responseText, String& actionResult);

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

    // Внутренний низкоуровневый HTTPS POST запрос к Gemini API
    GeminiResponse sendRawRequest(const String& requestBody);
};
