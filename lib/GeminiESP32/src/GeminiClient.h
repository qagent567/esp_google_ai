#pragma once

/**
 * =============================================================================
 *                  GeminiESP32 — Низкоуровневый клиент Gemini API
 * =============================================================================
 * Отвечает за формирование HTTPS запросов, работу с TLS, сериализацию JSON,
 * потоковое чтение Server-Sent Events, кодирование Base64 и выполнение Tools.
 * =============================================================================
 */

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>
#include <functional>

#include "GeminiConfig.h"
#include "ConfigManager.h"

#if GEMINI_ENABLE_FUNCTION_CALLING
#include "FunctionRegistry.h"
#endif

/**
 * @brief Структура детального результата выполнения запроса к Gemini API
 */
struct GeminiResponse {
    bool success;             ///< Флаг успешности выполнения сетевого запроса и парсинга
    int httpCode;             ///< HTTP статус-код сервера Google (200, 400, 403, 429, 500 и т.д.)
    String text;              ///< Текст сгенерированного ответа нейросети или сообщение об ошибке
    int promptTokens;         ///< Количество токенов во входящем запросе пользователя
    int candidateTokens;      ///< Количество токенов в сгенерированном ответе модели
    int totalTokens;          ///< Суммарное количество использованных токенов
    unsigned long durationMs; ///< Длительность сетевого запроса в миллисекундах
    int agentStepsExecuted;   ///< Количество выполненных автономных агентных шагов

#if GEMINI_ENABLE_FUNCTION_CALLING
    bool hasFunctionCall;     ///< Флаг: был ли получен запрос на вызов C++ функции (Tool Call)
    String functionName;      ///< Имя вызванной функции
    String functionResult;    ///< Результат выполнения вызванной функции на микроконтроллере
#endif
};

/**
 * @brief Элемент истории сообщений диалога
 */
struct ChatMessage {
    String role; ///< Роль отправителя ("user" или "model")
    String text; ///< Текст сообщения
};

// Типы функций обратного вызова (Callbacks)
typedef std::function<void(const GeminiResponse& response)> GeminiResponseCallback;
typedef std::function<void(const String& answer)> GeminiTextCallback;

#if GEMINI_ENABLE_STREAMING
typedef std::function<void(const String& chunk, bool isLast)> GeminiStreamCallback;
#endif

class UsageTracker;
class HardwareController;

/**
 * @brief Клиент для прямого взаимодействия с REST API Google AI Studio
 */
class GeminiClient {
public:
    /**
     * @brief Конструктор клиента
     * @param configMgr Ссылка на менеджер конфигурации
     * @param usageTracker Опциональный указатель на трекер квот
     * @param hwController Опциональный указатель на аппаратный контроллер
     * @param funcRegistry Опциональный указатель на реестр функций
     */
    GeminiClient(ConfigManager& configMgr, 
                 UsageTracker* usageTracker = nullptr, 
                 HardwareController* hwController = nullptr
#if GEMINI_ENABLE_FUNCTION_CALLING
                 , FunctionRegistry* funcRegistry = nullptr
#endif
    );

    // Установка указателей на внешние компоненты
    void setUsageTracker(UsageTracker* tracker) { _usageTracker = tracker; }
    void setHardwareController(HardwareController* hw) { _hwController = hw; }

#if GEMINI_ENABLE_FUNCTION_CALLING
    void setFunctionRegistry(FunctionRegistry* registry) { _funcRegistry = registry; }
#endif

    /**
     * @brief Определение суточного лимита запросов (RPD) по имени модели
     */
    static uint32_t getModelDailyLimit(const String& modelId);

    // ─── Управление историей диалога ──────────────────────────────────────────
    /**
     * @brief Очистка оперативной истории диалога
     */
    void clearHistory();

    /**
     * @brief Получение ссылки на текущий список сообщений в оперативной памяти
     */
    const std::vector<ChatMessage>& getHistory() const { return _history; }

    /**
     * @brief Добавление сообщения в историю с автоматическим контролем лимита GEMINI_HISTORY_LIMIT
     */
    void addHistory(const String& role, const String& text);

#if GEMINI_ENABLE_NVS_HISTORY
    /**
     * @brief Включение постоянного сохранения контекста во Flash NVS
     */
    void enablePersistentHistory(bool enable = true);

    /**
     * @brief Проверка, включено ли сохранение истории в NVS
     */
    bool isPersistentHistoryEnabled() const { return _persistentHistory; }

    /**
     * @brief Загрузка сохраненной истории из Flash-памяти
     */
    void loadHistoryFromNvs();

    /**
     * @brief Принудительное сохранение текущей истории во Flash-память
     */
    void saveHistoryToNvs();
#endif

    // ─── Сетевые запросы к Gemini API ─────────────────────────────────────────
    /**
     * @brief Отправка синхронного текстового запроса
     * @param prompt Текст запроса к ИИ
     * @return Структура GeminiResponse с ответом и метаданными
     */
    GeminiResponse ask(const String& prompt);

    /**
     * @brief Автономный многошаговый агентный запрос (ReAct: Thought -> Action -> Observation -> Final Answer)
     * @param goalPrompt Задача или цель для ИИ-агента
     * @param maxSteps Максимальное количество итераций обратной связи (по умолчанию 3)
     */
    GeminiResponse askAgent(const String& goalPrompt, int maxSteps = 3);

#if GEMINI_ENABLE_VISION
    /**
     * @brief Отправка мультимодального запроса с изображением (Gemini Vision)
     * @param prompt Текстовый вопрос по изображению
     * @param imageData Указатель на буфер байтов картинки (JPEG/PNG)
     * @param imageSize Размер картинки в байтах
     * @param mimeType MIME-тип изображения (по умолчанию "image/jpeg")
     */
    GeminiResponse askWithImage(const String& prompt, 
                                const uint8_t* imageData, 
                                size_t imageSize, 
                                const String& mimeType = "image/jpeg");
#endif

#if GEMINI_ENABLE_STREAMING
    /**
     * @brief Потоковый запрос (Server-Sent Events / SSE Streaming)
     * @param prompt Текст запроса к ИИ
     * @param onChunk Функция обратного вызова для каждого поступающего фрагмента
     */
    GeminiResponse streamAsk(const String& prompt, GeminiStreamCallback onChunk);
#endif

    /**
     * @brief Запрос и кэширование списка доступных моделей Google AI
     */
    bool listAvailableModels();

    /**
     * @brief Загрузка кэша моделей из NVS
     */
    void loadCachedModels();

    /**
     * @brief Сохранение списка моделей в NVS
     */
    void saveCachedModels();

    /**
     * @brief Проверка сетевой доступности серверов Google AI
     */
    bool testConnection();

    /**
     * @brief Получение понятного описания ошибки по HTTP статус-коду
     */
    static String getHttpErrorDescription(int httpCode);

#if GEMINI_ENABLE_VISION
    /**
     * @brief Быстрое кодирование бинарного буфера в строку Base64
     */
    static String encodeBase64(const uint8_t* data, size_t length);
#endif

    /**
     * @brief Формирование полного URL эндпоинта генерации
     */
    String buildApiUrl() const;

#if GEMINI_ENABLE_STREAMING
    /**
     * @brief Формирование URL эндпоинта потоковой генерации
     */
    String buildStreamApiUrl() const;
#endif

    /**
     * @brief Построение JSON полезной нагрузки для текстового запроса
     */
    String buildRequestBody(const String& prompt) const;

#if GEMINI_ENABLE_VISION
    /**
     * @brief Построение JSON полезной нагрузки для запроса с изображением
     */
    String buildRequestBodyWithImage(const String& prompt, 
                                     const uint8_t* imageData, 
                                     size_t imageSize, 
                                     const String& mimeType) const;
#endif

    /**
     * @brief Парсинг JSON ответа от Google API из потока (Stream)
     */
    bool parseResponse(Stream& stream, GeminiResponse& response);

    /**
     * @brief Парсинг JSON ответа от Google API (из строки)
     */
    bool parseResponse(const String& jsonPayload, GeminiResponse& response);

    /**
     * @brief Внутренняя реализация парсинга уже разобранного JSON-документа
     */
    bool parseResponse(const JsonDocument& doc, GeminiResponse& response);

    /**
     * @brief Извлечение и исполнение аппаратного действия из ответа ИИ
     */
    bool extractAndExecuteAction(const String& responseText, String& actionResult);

    /**
     * @brief Проверка и выполнение аппаратных действий в ответе модели
     */
    void processHardwareActions(GeminiResponse& response);

    // Доступ к кэшированным моделям
    size_t getModelCount() const { return _cachedModels.size(); }
    String getModelByIndex(size_t index) const {
        if (index > 0 && index <= _cachedModels.size()) {
            return _cachedModels[index - 1];
        }
        return "";
    }
    const std::vector<String>& getCachedModels() const { return _cachedModels; }

private:
    // Низкоуровневый HTTP POST запрос к Gemini
    GeminiResponse sendRawRequest(const String& requestBody);

    ConfigManager& _configMgr;
    UsageTracker* _usageTracker;
    HardwareController* _hwController;

#if GEMINI_ENABLE_FUNCTION_CALLING
    FunctionRegistry* _funcRegistry;
#endif

    std::vector<String> _cachedModels;
    std::vector<ChatMessage> _history;

#if GEMINI_ENABLE_NVS_HISTORY
    bool _persistentHistory;
#endif
    
    // Persistent TLS client for HTTP Keep-Alive
    WiFiClientSecure _secureClient;
    bool _isTlsConfigured = false;
    void ensureTlsConfigured();
};
