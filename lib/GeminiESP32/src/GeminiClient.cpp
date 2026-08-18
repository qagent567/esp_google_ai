#include "GeminiClient.h"
#include "UsageTracker.h"
#include "HardwareController.h"
#include <Preferences.h>

static const char* GEMINI_API_HOST = "https://generativelanguage.googleapis.com/v1beta/models/";

GeminiClient::GeminiClient(ConfigManager& configMgr, UsageTracker* usageTracker, HardwareController* hwController)
    : _configMgr(configMgr), _usageTracker(usageTracker), _hwController(hwController) {
    loadCachedModels();
}

uint32_t GeminiClient::getModelDailyLimit(const String& modelId) {
    String m = modelId;
    m.toLowerCase();
    if (m.indexOf("flash-lite") >= 0) return 1500;
    if (m.indexOf("flash") >= 0) return 1500;
    if (m.indexOf("pro") >= 0) return 50;
    if (m.indexOf("gemma") >= 0) return 1500;
    return 1500;
}

void GeminiClient::loadCachedModels() {
    Preferences p;
    if (p.begin("ai_models", true)) {
        size_t count = p.getUInt("count", 0);
        if (count > 0 && count <= 50) {
            _cachedModels.clear();
            for (size_t i = 0; i < count; i++) {
                char key[16];
                snprintf(key, sizeof(key), "m_%u", i);
                String name = p.getString(key, "");
                if (!name.isEmpty()) {
                    _cachedModels.push_back(name);
                }
            }
        }
        p.end();
    }
}

void GeminiClient::saveCachedModels() {
    if (_cachedModels.empty()) return;
    Preferences p;
    if (p.begin("ai_models", false)) {
        p.putUInt("count", _cachedModels.size());
        for (size_t i = 0; i < _cachedModels.size(); i++) {
            char key[16];
            snprintf(key, sizeof(key), "m_%u", i);
            p.putString(key, _cachedModels[i]);
        }
        p.end();
    }
}

String GeminiClient::buildApiUrl() const {
    const AppConfig& cfg = _configMgr.getConfig();
    String cleanModel = cfg.model; cleanModel.trim();
    String cleanKey = cfg.apiKey; cleanKey.trim();
    
    char urlBuf[256];
    snprintf(urlBuf, sizeof(urlBuf), "%s%s:generateContent?key=%s", 
             GEMINI_API_HOST, cleanModel.c_str(), cleanKey.c_str());
    return String(urlBuf);
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

    // Системный промпт + инструкции аппаратного управления ESP32
    String effectiveSysPrompt = cfg.systemPrompt;
    if (_hwController != nullptr) {
        if (!effectiveSysPrompt.isEmpty()) effectiveSysPrompt += "\n\n";
        effectiveSysPrompt += HardwareController::getHardwareCapabilitiesDescription();
    }

    if (!effectiveSysPrompt.isEmpty()) {
        JsonObject sysInst = doc["systemInstruction"].to<JsonObject>();
        JsonArray sysParts = sysInst["parts"].to<JsonArray>();
        JsonObject sysText = sysParts.add<JsonObject>();
        sysText["text"] = effectiveSysPrompt;
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
    JsonDocument doc;
    JsonDocument filter;
    filter["candidates"][0]["content"]["parts"][0]["text"] = true;
    filter["candidates"][0]["finishReason"] = true;
    filter["usageMetadata"]["promptTokenCount"] = true;
    filter["usageMetadata"]["candidatesTokenCount"] = true;
    filter["usageMetadata"]["totalTokenCount"] = true;
    filter["error"]["message"] = true;
    filter["error"]["status"] = true;
    filter["error"]["code"] = true;

    DeserializationError err = deserializeJson(doc, jsonPayload, DeserializationOption::Filter(filter));
    if (err) {
        err = deserializeJson(doc, jsonPayload);
        if (err) {
            response.success = false;
            char errBuf[300];
            snprintf(errBuf, sizeof(errBuf), "Ошибка парсинга ответа: %s\nСырой ответ сервера: %.200s", 
                     err.c_str(), jsonPayload.c_str());
            response.text = String(errBuf);
            return false;
        }
    }

    if (doc["error"]) {
        response.success = false;
        const char* errMsg = doc["error"]["message"] | "Неизвестная ошибка API";
        const char* errStatus = doc["error"]["status"] | "ERROR";
        int errCode = doc["error"]["code"] | 0;
        char errBuf[256];
        snprintf(errBuf, sizeof(errBuf), "[Google API: %s (%d)] %s", errStatus, errCode, errMsg);
        response.text = String(errBuf);
        return false;
    }

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
        
        const char* finishReason = cand["finishReason"];
        if (finishReason != nullptr) {
            response.success = false;
            char reasonBuf[128];
            snprintf(reasonBuf, sizeof(reasonBuf), "Генерация завершена без текста. Причина: %s", finishReason);
            response.text = String(reasonBuf);
            return false;
        }
    }

    response.success = false;
    char noTxtBuf[300];
    snprintf(noTxtBuf, sizeof(noTxtBuf), "Не удалось извлечь текст ответа. Ответ: %.200s", jsonPayload.c_str());
    response.text = String(noTxtBuf);
    return false;
}

void GeminiClient::processHardwareActions(GeminiResponse& response) {
    if (!response.success || _hwController == nullptr || response.text.isEmpty()) return;

    int startPos = -1;
    int endPos = -1;

    int actIdx = response.text.indexOf("```action");
    if (actIdx >= 0) {
        startPos = response.text.indexOf('{', actIdx);
        if (startPos >= 0) {
            endPos = response.text.indexOf('}', startPos);
        }
    } else {
        int jsonIdx = response.text.indexOf("{\"action\"");
        if (jsonIdx < 0) jsonIdx = response.text.indexOf("{\"action\":");
        if (jsonIdx >= 0) {
            startPos = jsonIdx;
            endPos = response.text.indexOf('}', startPos);
        }
    }

    if (startPos >= 0 && endPos > startPos) {
        String actionJson = response.text.substring(startPos, endPos + 1);
        String actionResult = _hwController->executeActionJson(actionJson);
        response.text += "\n\n[Выполнение на ESP32]: ";
        response.text += actionResult;
    }
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
        default: {
            char buf[32];
            snprintf(buf, sizeof(buf), "HTTP Код: %d", httpCode);
            return String(buf);
        }
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
    String url = buildApiUrl();
    String requestBody = buildRequestBody(prompt);

    const int maxAttempts = 2;
    for (int attempt = 1; attempt <= maxAttempts; attempt++) {
        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(30);

        HTTPClient http;
        if (!http.begin(client, url)) {
            result.text = "Ошибка: Не удалось инициализировать HTTPS соединение.";
            result.durationMs = millis() - startTime;
            return result;
        }

        http.addHeader("Content-Type", "application/json; charset=utf-8");
        http.addHeader("x-goog-api-key", cfg.apiKey);
        http.setTimeout(30000);

        int httpResponseCode = http.POST(requestBody);
        result.httpCode = httpResponseCode;
        result.durationMs = millis() - startTime;

        if (httpResponseCode > 0) {
            String payload = http.getString();
            if (httpResponseCode == 200) {
                parseResponse(payload, result);
                http.end();
                processHardwareActions(result);
                return result;
            } else if ((httpResponseCode == 500 || httpResponseCode == 503) && attempt < maxAttempts) {
                http.end();
                delay(1500);
                continue;
            } else {
                parseResponse(payload, result);
                if (result.text.isEmpty() || result.text.startsWith("Не удалось извлечь")) {
                    char errHdr[128];
                    snprintf(errHdr, sizeof(errHdr), "Ошибка HTTP %d: %s", httpResponseCode, getHttpErrorDescription(httpResponseCode).c_str());
                    result.text = errHdr;
                    if (!payload.isEmpty()) {
                        result.text += "\nОтвет Google: ";
                        result.text += payload.substring(0, 300);
                    }
                }
                http.end();
                return result;
            }
        } else {
            if (attempt < maxAttempts) {
                http.end();
                delay(1500);
                continue;
            }
            char reqErr[200];
            snprintf(reqErr, sizeof(reqErr), "Ошибка отправки HTTPS запроса: %s (%s)", 
                     http.errorToString(httpResponseCode).c_str(), getHttpErrorDescription(httpResponseCode).c_str());
            result.text = reqErr;
            http.end();
            return result;
        }
    }

    return result;
}

static String formatTokensShort(uint32_t tokens) {
    if (tokens == 0) return "-";
    char buf[16];
    if (tokens >= 1000000) {
        if (tokens % 1000000 == 0) {
            snprintf(buf, sizeof(buf), "%uM", tokens / 1000000);
        } else {
            snprintf(buf, sizeof(buf), "%.1fM", tokens / 1000000.0f);
        }
        return String(buf);
    }
    if (tokens >= 1000) {
        if (tokens % 1000 == 0) {
            snprintf(buf, sizeof(buf), "%uK", tokens / 1000);
        } else {
            snprintf(buf, sizeof(buf), "%.1fK", tokens / 1000.0f);
        }
        return String(buf);
    }
    snprintf(buf, sizeof(buf), "%u", tokens);
    return String(buf);
}

static size_t utf8VisualLength(const String& str) {
    size_t len = 0;
    const char* s = str.c_str();
    while (*s) {
        if ((*s & 0xC0) != 0x80) len++;
        s++;
    }
    return len;
}

static void printCell(const String& text, size_t width) {
    size_t vLen = utf8VisualLength(text);
    if (vLen > width) {
        size_t taken = 0;
        const char* s = text.c_str();
        while (*s && taken < width - 3) {
            Serial.print(*s);
            if ((*s & 0xC0) != 0x80) taken++;
            s++;
        }
        Serial.print("...");
    } else {
        Serial.print(text);
        for (size_t i = 0; i < width - vLen; i++) {
            Serial.print(' ');
        }
    }
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
    char urlBuf[256];
    snprintf(urlBuf, sizeof(urlBuf), "https://generativelanguage.googleapis.com/v1beta/models?key=%s", cleanKey.c_str());
    
    if (!http.begin(client, urlBuf)) {
        Serial.println(F("[ОШИБКА] Не удалось инициализировать HTTPS соединение."));
        return false;
    }

    http.addHeader("x-goog-api-key", cleanKey);
    http.setTimeout(20000);
    int httpCode = http.GET();

    if (httpCode == 200) {
        String payload = http.getString();
        
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

        Serial.println(F("\n=============================================== ДОСТУПНЫЕ МОДЕЛИ GEMINI ==============================================="));
        Serial.println(F("+----+----------------------------------------+-------------------------------------+----------------------+-----------+"));
        Serial.println(F("|  № | ID Модели                              | Отображаемое имя                    | Расход/Лимит в сут.  | Статус    |"));
        Serial.println(F("+----+----------------------------------------+-------------------------------------+----------------------+-----------+"));

        int count = 0;
        for (JsonObject m : models) {
            const char* fullName = m["name"] | "";
            const char* dispName = m["displayName"] | "";
            
            bool supportsGen = false;
            JsonArray methods = m["supportedGenerationMethods"];
            for (const char* method : methods) {
                if (method && strcmp(method, "generateContent") == 0) {
                    supportsGen = true;
                    break;
                }
            }

            if (!supportsGen) continue;

            String modelId = fullName;
            if (modelId.startsWith("models/")) {
                modelId = modelId.substring(7);
            }

            _cachedModels.push_back(modelId);
            count++;

            bool isActive = (modelId.equalsIgnoreCase(cfg.model));
            const char* statusStr = isActive ? "[АКТИВНА]" : "         ";
            
            String limitStr;
            if (isActive && _usageTracker) {
                DailyUsageStats st = _usageTracker->getStats();
                if (st.dailyRequestLimit > 0) {
                    char lBuf[48];
                    snprintf(lBuf, sizeof(lBuf), "%u / %u запр.", st.requestsToday, st.dailyRequestLimit);
                    limitStr = lBuf;
                } else {
                    char lBuf[48];
                    snprintf(lBuf, sizeof(lBuf), "%u запр. (безлим)", st.requestsToday);
                    limitStr = lBuf;
                }
            } else {
                uint32_t lim = getModelDailyLimit(modelId);
                char lBuf[32];
                snprintf(lBuf, sizeof(lBuf), "%u запр/сутки", lim);
                limitStr = lBuf;
            }

            Serial.printf("| %2d | ", count);
            printCell(modelId, 38);
            Serial.print(" | ");
            printCell(dispName, 35);
            Serial.print(" | ");
            printCell(limitStr, 20);
            Serial.printf(" | %s |\n", statusStr);
        }
        _cachedModels.shrink_to_fit();
        saveCachedModels();
        Serial.println(F("+----+----------------------------------------+-------------------------------------+----------------------+-----------+"));
        Serial.printf("Всего поддерживаемых моделей: %d\n", count);
        Serial.printf("Текущая активная модель: %s\n", cfg.model.c_str());
        if (_usageTracker) {
            DailyUsageStats st = _usageTracker->getStats();
            if (st.dailyRequestLimit > 0) {
                int rem = (int)st.dailyRequestLimit - (int)st.requestsToday;
                if (rem < 0) rem = 0;
                Serial.printf("Суточный расход текущей модели: %u из %u запросов (Осталось на сегодня: %d)\n", 
                              st.requestsToday, st.dailyRequestLimit, rem);
            } else {
                Serial.printf("Суточный расход текущей модели: %u запросов (Безлимитный режим)\n", st.requestsToday);
            }
            Serial.printf("Сброс суточных лимитов через: %s\n\n", _usageTracker->getTimeUntilMidnight().c_str());
        }
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
