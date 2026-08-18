    #include "GeminiClient.h"

// Хост Google AI Studio API
static const char* GEMINI_API_HOST = "https://generativelanguage.googleapis.com/v1beta/models/";

GeminiClient::GeminiClient(ConfigManager& configMgr)
    : _configMgr(configMgr) {}

String GeminiClient::buildApiUrl() const {
    const AppConfig& cfg = _configMgr.getConfig();
    String cleanModel = cfg.model; cleanModel.trim();
    String cleanKey = cfg.apiKey; cleanKey.trim();
    String url = String(GEMINI_API_HOST) + cleanModel + ":generateContent?key=" + cleanKey;
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
    if (jsonPayload.isEmpty()) {
        response.success = false;
        response.text = "Пустой ответ от сервера.";
        return false;
    }

    JsonDocument doc;
    // Парсим с фильтром для надежности и экономии оперативной памяти ESP32
    JsonDocument filter;
    filter["candidates"][0]["content"]["parts"][0]["text"] = true;
    filter["candidates"][0]["finishReason"] = true;
    filter["usageMetadata"]["totalTokenCount"] = true;
    filter["usageMetadata"]["promptTokenCount"] = true;
    filter["usageMetadata"]["candidatesTokenCount"] = true;
    filter["error"]["message"] = true;
    filter["error"]["status"] = true;
    filter["error"]["code"] = true;

    DeserializationError err = deserializeJson(doc, jsonPayload, DeserializationOption::Filter(filter));
    if (err) {
        // Если фильтр не сработал (например, нестандартный JSON), пробуем прямой парсинг
        err = deserializeJson(doc, jsonPayload);
        if (err) {
            response.success = false;
            response.text = String("Ошибка парсинга ответа: ") + err.c_str() + 
                            "\nСырой ответ сервера: " + jsonPayload.substring(0, 250);
            return false;
        }
    }

    // Проверка на ошибку в теле ответа от Google
    if (doc["error"]) {
        response.success = false;
        const char* errMsg = doc["error"]["message"] | "Неизвестная ошибка API";
        const char* errStatus = doc["error"]["status"] | "ERROR";
        int errCode = doc["error"]["code"] | 0;
        response.text = String("[Google API: ") + errStatus + " (" + String(errCode) + ")] " + errMsg;
        return false;
    }

    // Извлечение сгенерированного текста
    if (doc["candidates"] && doc["candidates"].is<JsonArray>() && doc["candidates"].size() > 0) {
        JsonObject cand = doc["candidates"][0];
        if (cand["content"] && cand["content"]["parts"] && cand["content"]["parts"].is<JsonArray>() && cand["content"]["parts"].size() > 0) {
            const char* generatedText = cand["content"]["parts"][0]["text"];
            if (generatedText != nullptr) {
                response.success = true;
                response.text = String(generatedText);
                response.promptTokens = doc["usageMetadata"]["promptTokenCount"] | 0;
                response.candidateTokens = doc["usageMetadata"]["candidatesTokenCount"] | 0;
                response.totalTokens = doc["usageMetadata"]["totalTokenCount"] | (response.promptTokens + response.candidateTokens);
                return true;
            }
        }
        
        // Если нет текста, проверяем finishReason
        const char* finishReason = cand["finishReason"];
        if (finishReason != nullptr) {
            response.success = false;
            response.text = String("Генерация завершена без текста. Причина: ") + finishReason;
            return false;
        }
    }

    response.success = false;
    response.text = "Не удалось извлечь текст ответа. Ответ: " + jsonPayload.substring(0, 250);
    return false;
}

String GeminiClient::getHttpErrorDescription(int httpCode) {
    switch (httpCode) {
        case 200: return "OK (Успешно)";
        case 400: return "Неверный запрос (Bad Request) - недействительный API-ключ или некорректный формат";
        case 403: return "Доступ запрещен (Forbidden) - блокировка региона или ограничения ключа (проверьте Smart DNS)";
        case 404: return "Не найдено (Not Found) - проверьте имя модели Gemini";
        case 429: return "Превышен лимит запросов (Rate Limit Exceeded) - подождите перед следующим запросом";
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
    // Отключаем проверку сертификатов для оптимизации RAM на ESP32
    client.setInsecure();
    client.setTimeout(30);

    HTTPClient http;
    String url = buildApiUrl();
    
    if (!http.begin(client, url)) {
        result.text = "Ошибка: Не удалось инициализировать HTTPS соединение.";
        result.durationMs = millis() - startTime;
        return result;
    }

    http.addHeader("Content-Type", "application/json; charset=utf-8");
    http.addHeader("x-goog-api-key", cfg.apiKey);
    http.setTimeout(30000); // 30 секунд таймаут на генерацию нейросетью

    String requestBody = buildRequestBody(prompt);

    int httpResponseCode = http.POST(requestBody);
    result.httpCode = httpResponseCode;
    result.durationMs = millis() - startTime;

    if (httpResponseCode > 0) {
        String payload = http.getString();
        if (httpResponseCode == 200) {
            parseResponse(payload, result);
        } else {
            // Ошибка HTTP: пробуем распарсить JSON ошибки от Google
            parseResponse(payload, result);
            // Если parseResponse не смог распарсить ошибку Google, формируем подробное описание
            if (result.text.isEmpty() || result.text.startsWith("Не удалось извлечь")) {
                result.text = String("Ошибка HTTP ") + String(httpResponseCode) + ": " + getHttpErrorDescription(httpResponseCode);
                if (!payload.isEmpty()) {
                    result.text += "\nОтвет Google: " + payload.substring(0, 300);
                }
            }
        }
    } else {
        result.text = String("Ошибка отправки HTTPS запроса: ") + http.errorToString(httpResponseCode) + 
                      " (" + getHttpErrorDescription(httpResponseCode) + ")";
    }

    http.end();
    return result;
}

static String formatTokensShort(uint32_t tokens) {
    if (tokens == 0) return "-";
    if (tokens >= 1000000) {
        if (tokens % 1000000 == 0) {
            return String(tokens / 1000000) + "M";
        } else {
            float m = tokens / 1000000.0f;
            char buf[16];
            snprintf(buf, sizeof(buf), "%.1fM", m);
            return String(buf);
        }
    }
    if (tokens >= 1000) {
        if (tokens % 1000 == 0) {
            return String(tokens / 1000) + "K";
        } else {
            float k = tokens / 1000.0f;
            char buf[16];
            snprintf(buf, sizeof(buf), "%.1fK", k);
            return String(buf);
        }
    }
    return String(tokens);
}

bool GeminiClient::listAvailableModels() {
    const AppConfig& cfg = _configMgr.getConfig();

    if (cfg.apiKey.isEmpty()) {
        Serial.println(F("[ОШИБКА] API-ключ Gemini не настроен! Задайте его командой 'set key <api_key>'."));
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("[ОШИБКА] Нет подключения к Wi-Fi сети!"));
        return false;
    }

    Serial.println(F("\n[AI] Запрос списка доступных моделей с Google AI Studio..."));
    
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(20);

    HTTPClient http;
    String cleanKey = cfg.apiKey; cleanKey.trim();
    String url = "https://generativelanguage.googleapis.com/v1beta/models?key=" + cleanKey;
    
    if (!http.begin(client, url)) {
        Serial.println(F("[ОШИБКА] Не удалось инициализировать HTTPS соединение."));
        return false;
    }

    http.addHeader("x-goog-api-key", cleanKey);
    http.setTimeout(20000);
    int httpCode = http.GET();

    if (httpCode == 200) {
        String payload = http.getString();
        
        // Создаем фильтр для оптимизации расхода RAM
        JsonDocument filter;
        filter["models"][0]["name"] = true;
        filter["models"][0]["displayName"] = true;
        filter["models"][0]["inputTokenLimit"] = true;
        filter["models"][0]["outputTokenLimit"] = true;
        filter["models"][0]["supportedGenerationMethods"] = true;
        filter["error"]["message"] = true;

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
        if (err) {
            Serial.printf("[ОШИБКА] Не удалось разобрать JSON списка моделей: %s\n", err.c_str());
            http.end();
            return false;
        }

        if (doc["error"]) {
            Serial.printf("[ОШИБКА API] %s\n", doc["error"]["message"] | "Неизвестная ошибка");
            http.end();
            return false;
        }

        JsonArray models = doc["models"].as<JsonArray>();
        if (models.isNull() || models.size() == 0) {
            Serial.println(F("[AI] Список моделей пуст."));
            http.end();
            return false;
        }

        _cachedModels.clear();

        Serial.println(F("\n========================================== ДОСТУПНЫЕ МОДЕЛИ GEMINI =========================================="));
        Serial.println(F("+----+----------------------------------------+-------------------------------------+-------------+-----------+"));
        Serial.println(F("|  № | ID Модели                              | Отображаемое имя                    | Лимиты(I/O) | Статус    |"));
        Serial.println(F("+----+----------------------------------------+-------------------------------------+-------------+-----------+"));

        int count = 0;
        for (JsonObject m : models) {
            const char* fullName = m["name"] | "";
            const char* dispName = m["displayName"] | "";
            uint32_t inLimit = m["inputTokenLimit"] | 0;
            uint32_t outLimit = m["outputTokenLimit"] | 0;
            
            // Проверяем, поддерживает ли модель генерацию контента
            bool supportsGen = false;
            JsonArray methods = m["supportedGenerationMethods"];
            for (const char* method : methods) {
                if (method && strcmp(method, "generateContent") == 0) {
                    supportsGen = true;
                    break;
                }
            }

            if (!supportsGen) continue;

            // Убираем префикс "models/"
            String modelId = fullName;
            if (modelId.startsWith("models/")) {
                modelId = modelId.substring(7);
            }

            _cachedModels.push_back(modelId);
            count++;

            String limitStr = "-";
            if (inLimit > 0 || outLimit > 0) {
                limitStr = formatTokensShort(inLimit) + " / " + formatTokensShort(outLimit);
            }

            bool isActive = (modelId.equalsIgnoreCase(cfg.model));
            const char* statusStr = isActive ? "[АКТИВНА]" : "         ";
            
            Serial.printf("| %2d | %-38s | %-35s | %-11s | %s |\n", 
                          count, 
                          modelId.c_str(), 
                          dispName, 
                          limitStr.c_str(),
                          statusStr);
        }
        Serial.println(F("+----+----------------------------------------+-------------------------------------+-------------+-----------+"));
        Serial.printf("Всего поддерживаемых моделей: %d\n", count);
        Serial.printf("Текущая активная модель: %s\n\n", cfg.model.c_str());
        Serial.println(F("[Подсказка] Чтобы переключить модель, введите 'model <№>' (напр. 'model 24') или 'set model <id>'.\n"));

        http.end();
        return true;
    } else {
        String payload = (httpCode > 0) ? http.getString() : "";
        Serial.printf("[ОШИБКА] Не удалось получить список моделей. HTTP Код: %d (%s)\n", 
                      httpCode, getHttpErrorDescription(httpCode).c_str());
        if (!payload.isEmpty()) {
            JsonDocument errDoc;
            if (!deserializeJson(errDoc, payload) && errDoc["error"]) {
                Serial.printf("[ОТВЕТ GOOGLE API] %s: %s\n", 
                              errDoc["error"]["status"] | "ERROR", 
                              errDoc["error"]["message"] | "Неизвестная ошибка");
            } else {
                Serial.printf("[ОТВЕТ GOOGLE API] %s\n", payload.substring(0, 300).c_str());
            }
        }
        http.end();
        return false;
    }
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
