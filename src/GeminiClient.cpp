#include "GeminiClient.h"

// Хост Google AI Studio API
static const char* GEMINI_API_HOST = "https://generativelanguage.googleapis.com/v1beta/models/";

GeminiClient::GeminiClient(ConfigManager& configMgr)
    : _configMgr(configMgr) {}

String GeminiClient::buildApiUrl() const {
    const AppConfig& cfg = _configMgr.getConfig();
    String url = String(GEMINI_API_HOST) + cfg.model + ":generateContent?key=" + cfg.apiKey;
    return url;
}

String GeminiClient::buildRequestBody(const String& prompt) const {
    const AppConfig& cfg = _configMgr.getConfig();

    JsonDocument doc;

    // Массив contents (сообщение пользователя)
    JsonArray contents = doc["contents"].to<JsonArray>();
    JsonObject userMsg = contents.add<JsonObject>();
    userMsg["role"] = "user";
    JsonArray parts = userMsg["parts"].to<JsonArray>();
    JsonObject textPart = parts.add<JsonObject>();
    textPart["text"] = prompt;

    // Системный промпт (если задан)
    if (!cfg.systemPrompt.isEmpty()) {
        JsonObject sysInst = doc["systemInstruction"].to<JsonObject>();
        JsonArray sysParts = sysInst["parts"].to<JsonArray>();
        JsonObject sysText = sysParts.add<JsonObject>();
        sysText["text"] = cfg.systemPrompt;
    }

    // Параметры генерации
    JsonObject genConfig = doc["generationConfig"].to<JsonObject>();
    genConfig["temperature"] = cfg.temperature;
    genConfig["maxOutputTokens"] = cfg.maxTokens;

    String jsonOutput;
    serializeJson(doc, jsonOutput);
    return jsonOutput;
}

bool GeminiClient::parseResponse(const String& jsonPayload, GeminiResponse& response) {
    // Создаем фильтр для экономии RAM: парсим только нужные поля
    JsonDocument filter;
    filter["candidates"][0]["content"]["parts"][0]["text"] = true;
    filter["candidates"][0]["finishReason"] = true;
    filter["usageMetadata"]["totalTokenCount"] = true;
    filter["error"]["message"] = true;
    filter["error"]["status"] = true;
    filter["error"]["code"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, jsonPayload, DeserializationOption::Filter(filter));
    if (err) {
        response.success = false;
        response.text = String("Ошибка парсинга JSON ответа: ") + err.c_str();
        return false;
    }

    // Проверка на ошибку в теле ответа от Google
    if (doc["error"]) {
        response.success = false;
        const char* errMsg = doc["error"]["message"] | "Неизвестная ошибка API";
        const char* errStatus = doc["error"]["status"] | "ERROR";
        response.text = String("[Google API Error: ") + errStatus + "] " + errMsg;
        return false;
    }

    // Извлечение сгенерированного текста
    const char* generatedText = doc["candidates"][0]["content"]["parts"][0]["text"];
    if (generatedText != nullptr) {
        response.success = true;
        response.text = String(generatedText);
        response.totalTokens = doc["usageMetadata"]["totalTokenCount"] | 0;
        return true;
    }

    // Если нет текста, проверяем finishReason
    const char* finishReason = doc["candidates"][0]["finishReason"];
    if (finishReason != nullptr) {
        response.success = false;
        response.text = String("Ответ пуст. Причина завершения: ") + finishReason;
        return false;
    }

    response.success = false;
    response.text = "Не удалось извлечь текст ответа из структуры ответа Google.";
    return false;
}

String GeminiClient::getHttpErrorDescription(int httpCode) {
    switch (httpCode) {
        case 200: return "OK (Успешно)";
        case 400: return "Неверный запрос (Bad Request) - проверьте формат запроса или API ключ";
        case 403: return "Доступ запрещен (Forbidden) - возможно, блокировка по региону (проверьте Smart DNS) или недействительный API-ключ";
        case 404: return "Не найдено (Not Found) - проверьте правильность названия модели Gemini";
        case 429: return "Превышен лимит запросов (Rate Limit Exceeded) - подождите немного перед следующим запросом";
        case 500: return "Внутренняя ошибка сервера Google";
        case 503: return "Сервис Google AI временно недоступен";
        case -1:  return "Ошибка соединения / таймаут (Connection Failed / Timeout)";
        case -2:  return "Не удалось отправить запрос";
        default:  return String("HTTP Код: ") + String(httpCode);
    }
}

GeminiResponse GeminiClient::ask(const String& prompt) {
    GeminiResponse result;
    result.success = false;
    result.httpCode = 0;
    result.totalTokens = 0;
    result.durationMs = 0;

    const AppConfig& cfg = _configMgr.getConfig();

    if (cfg.apiKey.isEmpty()) {
        result.text = "Ошибка: API-ключ Gemini не настроен! Задайте его командой 'set key <api_key>'.";
        return result;
    }

    if (WiFi.status() != WL_CONNECTED) {
        result.text = "Ошибка: Нет подключения к Wi-Fi сети!";
        return result;
    }

    unsigned long startTime = millis();

    WiFiClientSecure client;
    // Отключаем проверку сертификатов для экономии оперативной памяти на ESP32
    client.setInsecure();
    // Установка таймаута сокета
    client.setTimeout(30);

    HTTPClient http;
    String url = buildApiUrl();
    
    if (!http.begin(client, url)) {
        result.text = "Ошибка: Не удалось инициализировать HTTPS соединение.";
        result.durationMs = millis() - startTime;
        return result;
    }

    http.addHeader("Content-Type", "application/json; charset=utf-8");
    http.setTimeout(30000); // 30 секунд на генерацию ответа нейросетью

    String requestBody = buildRequestBody(prompt);

    int httpResponseCode = http.POST(requestBody);
    result.httpCode = httpResponseCode;
    result.durationMs = millis() - startTime;

    if (httpResponseCode > 0) {
        String payload = http.getString();
        if (httpResponseCode == 200) {
            parseResponse(payload, result);
        } else {
            // Ошибка HTTP
            if (!parseResponse(payload, result)) {
                result.text = String("Ошибка HTTP ") + String(httpResponseCode) + ": " + getHttpErrorDescription(httpResponseCode);
            }
        }
    } else {
        result.text = String("Ошибка отправки HTTPS запроса: ") + http.errorToString(httpResponseCode) + 
                      " (" + getHttpErrorDescription(httpResponseCode) + ")";
    }

    http.end();
    return result;
}

bool GeminiClient::testConnection() {
    Serial.println(F("[Тест] Проверка связи с Google Generative Language API..."));
    GeminiResponse res = ask("Ответь одним словом: 'РАБОТАЕТ'");
    if (res.success) {
        Serial.printf("[Тест] Успешно! Ответ модели: %s (Время: %lu мс)\n", res.text.c_str(), res.durationMs);
        return true;
    } else {
        Serial.printf("[Тест] Ошибка: %s\n", res.text.c_str());
        return false;
    }
}
