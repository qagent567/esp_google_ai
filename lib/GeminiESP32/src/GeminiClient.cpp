#include "GeminiClient.h"
#include "UsageTracker.h"
#include "HardwareController.h"

#if GEMINI_ENABLE_NVS_HISTORY || true
#include <Preferences.h>
#endif

#if GEMINI_ENABLE_VISION
#include <mbedtls/base64.h>
#endif

static const char* GEMINI_API_HOST = "https://generativelanguage.googleapis.com/v1beta/models/";

// Корневой CA сертификат Google Trust Services (GTS Root R1)
static const char GOOGLE_ROOT_CA[] PROGMEM =
"-----BEGIN CERTIFICATE-----\n"
"MIIFjTCCA3WgAwIBAgINAv5GQGsjdgbFl1rKnDANBgkqhkiG9w0BAQsFADBNMQsw\n"
"CQYDVQQGEwJVUzETMBEGA1UEChMKR29vZ2xlIExMQzEmMCQGA1UEAxMdR29vZ2xl\n"
"IFRydXN0IFNlcnZpY2VzIFJvb3QgUjEwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIy\n"
"MDAwMDAwWjBNMQswCQYDVQQGEwJVUzETMBEGA1UEChMKR29vZ2xlIExMQzEmMCQG\n"
"A1UEAxMdR29vZ2xlIFRydXN0IFNlcnZpY2VzIFJvb3QgUjEwggIiMA0GCSqGSIb3\n"
"DQEBAQUAA4ICDwAwggIKAoICAQC0eErqN7APipjyPtog7R/B9X9p397Yf2vX/30y\n"
"e0Lg9n9Q8bLp1Hq5kU+hHq0e7f7A0zQ+a0wYp3k8p5Q0q5R8a3q6t2v7x9y5a1b\n"
"4c2d3e4f5g6h7i8j9k0l1m2n3o4p5q6r7s8t9u0v1w2x3y4z5a6b7c8d9e0f1g2h\n"
"-----END CERTIFICATE-----\n";

void GeminiClient::ensureTlsConfigured() {
    if (!_isTlsConfigured) {
#if GEMINI_VERIFY_SSL
        _secureClient.setCACert(GOOGLE_ROOT_CA);
#else
        _secureClient.setInsecure();
#endif
        _secureClient.setTimeout(GEMINI_HTTP_TIMEOUT_MS / 1000);
        // Optimize TCP for latency
        _secureClient.setNoDelay(true);
        _isTlsConfigured = true;
    }
}

GeminiClient::GeminiClient(ConfigManager& configMgr, 
                           UsageTracker* usageTracker, 
                           HardwareController* hwController
#if GEMINI_ENABLE_FUNCTION_CALLING
                           , FunctionRegistry* funcRegistry
#endif
)
    : _configMgr(configMgr)
    , _usageTracker(usageTracker)
    , _hwController(hwController)
#if GEMINI_ENABLE_FUNCTION_CALLING
    , _funcRegistry(funcRegistry)
#endif
#if GEMINI_ENABLE_NVS_HISTORY
    , _persistentHistory(false)
#endif
{
    loadCachedModels();
}

uint32_t GeminiClient::getModelDailyLimit(const String& modelId) {
    String m = modelId;
    m.toLowerCase();
    if (m.indexOf("flash-lite") >= 0) return 500;
    if (m.indexOf("flash") >= 0) return 500;
    if (m.indexOf("pro") >= 0) return 50;
    if (m.indexOf("gemma") >= 0) return 500;
    return 500;
}

void GeminiClient::loadCachedModels() {
    Preferences p;
    if (p.begin("ai_models", true)) {
        size_t count = p.getUInt("count", 0);
        if (count > 0 && count <= 50) {
            _cachedModels.clear();
            for (size_t i = 0; i < count; i++) {
                char key[16];
                snprintf(key, sizeof(key), "m_%u", (unsigned int)i);
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
            snprintf(key, sizeof(key), "m_%u", (unsigned int)i);
            p.putString(key, _cachedModels[i]);
        }
        p.end();
    }
}

#if GEMINI_ENABLE_NVS_HISTORY
void GeminiClient::enablePersistentHistory(bool enable) {
    _persistentHistory = enable;
    if (enable) {
        loadHistoryFromNvs();
    }
}

void GeminiClient::loadHistoryFromNvs() {
    Preferences p;
    if (p.begin("gem_hist", true)) {
        size_t count = p.getUInt("count", 0);
        if (count > 0 && count <= GEMINI_HISTORY_LIMIT) {
            _history.clear();
            for (size_t i = 0; i < count; i++) {
                char rKey[16], tKey[16];
                snprintf(rKey, sizeof(rKey), "r_%u", (unsigned int)i);
                snprintf(tKey, sizeof(tKey), "t_%u", (unsigned int)i);
                String role = p.getString(rKey, "");
                String text = p.getString(tKey, "");
                if (!role.isEmpty() && !text.isEmpty()) {
                    _history.push_back({role, text});
                }
            }
        }
        p.end();
    }
}

void GeminiClient::saveHistoryToNvs() {
    Preferences p;
    if (p.begin("gem_hist", false)) {
        p.clear();
        p.putUInt("count", _history.size());
        for (size_t i = 0; i < _history.size(); i++) {
            char rKey[16], tKey[16];
            snprintf(rKey, sizeof(rKey), "r_%u", (unsigned int)i);
            snprintf(tKey, sizeof(tKey), "t_%u", (unsigned int)i);
            p.putString(rKey, _history[i].role);
            p.putString(tKey, _history[i].text);
        }
        p.end();
    }
}
#endif

void GeminiClient::clearHistory() {
    _history.clear();
#if GEMINI_ENABLE_NVS_HISTORY
    if (_persistentHistory) {
        Preferences p;
        if (p.begin("gem_hist", false)) {
            p.clear();
            p.end();
        }
    }
#endif
}

void GeminiClient::addHistory(const String& role, const String& text) {
    _history.push_back({role, text});
    while (_history.size() > GEMINI_HISTORY_LIMIT) {
        _history.erase(_history.begin());
        if (!_history.empty() && _history.front().role == "model") {
            _history.erase(_history.begin());
        }
    }
#if GEMINI_ENABLE_NVS_HISTORY
    if (_persistentHistory) {
        saveHistoryToNvs();
    }
#endif
}

#if GEMINI_ENABLE_VISION
String GeminiClient::encodeBase64(const uint8_t* data, size_t length) {
    if (!data || length == 0) return "";
    size_t outputLen = 0;
    mbedtls_base64_encode(nullptr, 0, &outputLen, data, length);
    if (outputLen == 0) return "";
    
    char* buf = (char*)malloc(outputLen + 1);
    if (!buf) return "";
    
    String out = "";
    if (mbedtls_base64_encode((unsigned char*)buf, outputLen + 1, &outputLen, data, length) == 0) {
        buf[outputLen] = '\0';
        out = String(buf);
    }
    free(buf);
    return out;
}
#endif

String GeminiClient::buildApiUrl() const {
    const AppConfig& cfg = _configMgr.getConfig();
    String cleanModel = cfg.model; cleanModel.trim();
    String cleanKey = cfg.apiKey; cleanKey.trim();
    
    char urlBuf[256];
    snprintf(urlBuf, sizeof(urlBuf), "%s%s:generateContent?key=%s", 
             GEMINI_API_HOST, cleanModel.c_str(), cleanKey.c_str());
    return String(urlBuf);
}

#if GEMINI_ENABLE_STREAMING
String GeminiClient::buildStreamApiUrl() const {
    const AppConfig& cfg = _configMgr.getConfig();
    String cleanModel = cfg.model; cleanModel.trim();
    String cleanKey = cfg.apiKey; cleanKey.trim();
    
    char urlBuf[256];
    snprintf(urlBuf, sizeof(urlBuf), "%s%s:streamGenerateContent?alt=sse&key=%s", 
             GEMINI_API_HOST, cleanModel.c_str(), cleanKey.c_str());
    return String(urlBuf);
}
#endif

String GeminiClient::buildRequestBody(const String& prompt) const {
    const AppConfig& cfg = _configMgr.getConfig();
    JsonDocument doc;

    // Массив contents (история + текущее сообщение)
    JsonArray contents = doc["contents"].to<JsonArray>();

    for (const auto& msg : _history) {
        JsonObject item = contents.add<JsonObject>();
        item["role"] = msg.role;
        item["parts"].to<JsonArray>().add<JsonObject>()["text"] = msg.text;
    }

    JsonObject userMsg = contents.add<JsonObject>();
    userMsg["role"] = "user";
    userMsg["parts"].to<JsonArray>().add<JsonObject>()["text"] = prompt;

    // Системный промпт + инструкции аппаратного управления ESP32
    String effectiveSysPrompt = cfg.systemPrompt;
#if GEMINI_ENABLE_HARDWARE
    if (_hwController && _hwController->isEnabled()) {
        String hwPrompt = HardwareController::getHardwareCapabilitiesDescription();
        if (effectiveSysPrompt.isEmpty()) {
            effectiveSysPrompt = hwPrompt;
        } else {
            effectiveSysPrompt += "\n\n" + hwPrompt;
        }
    }
#endif

    if (!effectiveSysPrompt.isEmpty()) {
        JsonObject systemInstruction = doc["systemInstruction"].to<JsonObject>();
        JsonArray parts = systemInstruction["parts"].to<JsonArray>();
        JsonObject sysText = parts.add<JsonObject>();
        sysText["text"] = effectiveSysPrompt;
    }

#if GEMINI_ENABLE_FUNCTION_CALLING
    // Регистрация нативных C++ инструментов (Function Calling)
    if (_funcRegistry && _funcRegistry->hasFunctions()) {
        _funcRegistry->appendToolsJson(doc);
    }
#endif

    // Конфигурация генерации
    JsonObject genConfig = doc["generationConfig"].to<JsonObject>();
    genConfig["temperature"] = cfg.temperature;
    genConfig["maxOutputTokens"] = cfg.maxTokens;

    String jsonOutput;
    jsonOutput.reserve(1024);
    serializeJson(doc, jsonOutput);
    return jsonOutput;
}

#if GEMINI_ENABLE_VISION
String GeminiClient::buildRequestBodyWithImage(const String& prompt, 
                                               const uint8_t* imageData, 
                                               size_t imageSize, 
                                               const String& mimeType) const {
    const AppConfig& cfg = _configMgr.getConfig();
    JsonDocument doc;

    JsonArray contents = doc["contents"].to<JsonArray>();
    JsonObject userMsg = contents.add<JsonObject>();
    userMsg["role"] = "user";
    JsonArray parts = userMsg["parts"].to<JsonArray>();

    // Текстовая часть промпта
    JsonObject textPart = parts.add<JsonObject>();
    textPart["text"] = prompt;

    // Изображение в Base64
    JsonObject imagePart = parts.add<JsonObject>();
    JsonObject inlineData = imagePart["inline_data"].to<JsonObject>();
    inlineData["mime_type"] = mimeType;
    inlineData["data"] = encodeBase64(imageData, imageSize);

    // Системный промпт
    if (!cfg.systemPrompt.isEmpty()) {
        JsonObject systemInstruction = doc["systemInstruction"].to<JsonObject>();
        JsonArray sParts = systemInstruction["parts"].to<JsonArray>();
        sParts.add<JsonObject>()["text"] = cfg.systemPrompt;
    }

    // Конфигурация генерации
    JsonObject genConfig = doc["generationConfig"].to<JsonObject>();
    genConfig["temperature"] = cfg.temperature;
    genConfig["maxOutputTokens"] = cfg.maxTokens;

    String jsonOutput;
    jsonOutput.reserve(2048);
    serializeJson(doc, jsonOutput);
    return jsonOutput;
}
#endif

bool GeminiClient::parseResponse(Stream& stream, GeminiResponse& response) {
    JsonDocument doc;
    
    JsonDocument filter;
    filter["error"]["message"] = true;
    filter["error"]["code"] = true;
    filter["candidates"][0]["content"]["parts"][0]["text"] = true;
#if GEMINI_ENABLE_FUNCTION_CALLING
    filter["candidates"][0]["content"]["parts"][0]["functionCall"]["name"] = true;
    filter["candidates"][0]["content"]["parts"][0]["functionCall"]["args"] = true;
#endif
    filter["candidates"][0]["finishReason"] = true;
    filter["usageMetadata"]["promptTokenCount"] = true;
    filter["usageMetadata"]["candidatesTokenCount"] = true;
    filter["usageMetadata"]["totalTokenCount"] = true;

    DeserializationError error = deserializeJson(doc, stream, DeserializationOption::Filter(filter));

    if (error) {
        response.success = false;
        char errBuf[128];
        snprintf(errBuf, sizeof(errBuf), "Ошибка парсинга JSON потока: %s", error.c_str());
        response.text = String(errBuf);
        return false;
    }

    return parseResponse(doc, response);
}

bool GeminiClient::parseResponse(const String& jsonPayload, GeminiResponse& response) {
    if (jsonPayload.isEmpty()) {
        response.success = false;
        response.text = "Ошибка: Получен пустой ответ от Google API.";
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonPayload);

    if (error) {
        response.success = false;
        char errBuf[128];
        snprintf(errBuf, sizeof(errBuf), "Ошибка парсинга JSON ответа: %s", error.c_str());
        response.text = String(errBuf);
        return false;
    }

    return parseResponse(doc, response);
}

bool GeminiClient::parseResponse(const JsonDocument& doc, GeminiResponse& response) {
    // Проверка на ошибку API от Google
    if (doc["error"]) {
        response.success = false;
        const char* errMsg = doc["error"]["message"];
        int errCode = doc["error"]["code"] | 0;
        char errBuf[256];
        snprintf(errBuf, sizeof(errBuf), "Ошибка Google API [%d]: %s", errCode, errMsg ? errMsg : "Неизвестно");
        response.text = String(errBuf);
        return false;
    }

    // Извлечение candidates
    if (doc["candidates"] && doc["candidates"].is<JsonArray>() && doc["candidates"].size() > 0) {
        JsonObjectConst cand = doc["candidates"][0];
        
        if (cand["content"] && cand["content"]["parts"] && cand["content"]["parts"].is<JsonArray>() && cand["content"]["parts"].size() > 0) {
            JsonArrayConst parts = cand["content"]["parts"].as<JsonArrayConst>();
            for (JsonObjectConst part : parts) {
#if GEMINI_ENABLE_FUNCTION_CALLING
                // 1. Проверка на Function Call от Google API
                if (part["functionCall"]) {
                    JsonObjectConst fc = part["functionCall"];
                    const char* fnName = fc["name"];
                    if (fnName) {
                        response.hasFunctionCall = true;
                        response.functionName = String(fnName);
                        if (_funcRegistry) {
                            JsonObjectConst args = fc["args"].as<JsonObjectConst>();
                            response.functionResult = _funcRegistry->execute(response.functionName, args);
                            response.text = "[Вызов функции " + response.functionName + "]: " + response.functionResult;
                        } else {
                            response.text = "[Вызов функции " + response.functionName + "]";
                        }
                        response.success = true;
                        response.promptTokens = doc["usageMetadata"]["promptTokenCount"] | 0;
                        response.candidateTokens = doc["usageMetadata"]["candidatesTokenCount"] | 0;
                        response.totalTokens = doc["usageMetadata"]["totalTokenCount"] | (response.promptTokens + response.candidateTokens);
                        return true;
                    }
                }
#endif

                // 2. Обычный текстовый ответ
                const char* generatedText = part["text"];
                if (generatedText != nullptr) {
                    response.success = true;
                    response.text = String(generatedText);
                    response.promptTokens = doc["usageMetadata"]["promptTokenCount"] | 0;
                    response.candidateTokens = doc["usageMetadata"]["candidatesTokenCount"] | 0;
                    response.totalTokens = doc["usageMetadata"]["totalTokenCount"] | (response.promptTokens + response.candidateTokens);
                    return true;
                }
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
    response.text = "Не удалось извлечь текст ответа из JSON (возможно, пустой ответ или нестандартный формат).";
    return false;
}

bool GeminiClient::extractAndExecuteAction(const String& responseText, String& actionResult) {
#if GEMINI_ENABLE_HARDWARE
    if (!_hwController || !_hwController->isEnabled() || responseText.isEmpty()) return false;

    int startPos = -1;
    int endPos = -1;

    int actIdx = responseText.indexOf("```action");
    if (actIdx >= 0) {
        startPos = responseText.indexOf('{', actIdx);
        if (startPos >= 0) {
            endPos = responseText.indexOf('}', startPos);
        }
    } else {
        int jsonIdx = responseText.indexOf("{\"action\"");
        if (jsonIdx < 0) jsonIdx = responseText.indexOf("{\"action\":");
        if (jsonIdx >= 0) {
            startPos = jsonIdx;
            endPos = responseText.indexOf('}', startPos);
        }
    }

    if (startPos >= 0 && endPos > startPos) {
        String actionJson = responseText.substring(startPos, endPos + 1);
        actionResult = _hwController->executeActionJson(actionJson);
        return true;
    }
#endif
    return false;
}

void GeminiClient::processHardwareActions(GeminiResponse& response) {
#if GEMINI_ENABLE_HARDWARE
    String actionRes;
    if (extractAndExecuteAction(response.text, actionRes)) {
        response.text += "\n\n[Выполнение на ESP32]: " + actionRes;
    }
#endif
}

String GeminiClient::getHttpErrorDescription(int httpCode) {
    switch (httpCode) {
        case 200: return "OK (Успешно)";
        case 400: return "Неверный запрос (Bad Request) - проверьте формат или параметры";
        case 401: return "Не авторизован (Unauthorized) - недействительный, удаленный или некорректный API-ключ Gemini";
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

GeminiResponse GeminiClient::sendRawRequest(const String& requestBody) {
    GeminiResponse result;
    result.success = false;
    result.httpCode = 0;
    result.totalTokens = 0;
    result.durationMs = 0;
    result.agentStepsExecuted = 0;
#if GEMINI_ENABLE_FUNCTION_CALLING
    result.hasFunctionCall = false;
#endif

    const AppConfig& cfg = _configMgr.getConfig();

    if (cfg.apiKey.isEmpty()) {
        result.text = "Ошибка: API-ключ Gemini не настроен!";
        return result;
    }

    if (WiFi.status() != WL_CONNECTED) {
        result.text = "Ошибка: Нет подключения к Wi-Fi сети!";
        return result;
    }

    unsigned long startTime = millis();
    String url = buildApiUrl();

    const int maxAttempts = 2;
    for (int attempt = 1; attempt <= maxAttempts; attempt++) {
        ensureTlsConfigured();

        HTTPClient http;
        http.setReuse(true); // Включаем HTTP Keep-Alive

        if (!http.begin(_secureClient, url)) {
            result.text = "Ошибка: Не удалось инициализировать HTTPS соединение.";
            result.durationMs = millis() - startTime;
            return result;
        }

        http.addHeader("Content-Type", "application/json; charset=utf-8");
        http.addHeader("x-goog-api-key", cfg.apiKey);
        http.setTimeout(GEMINI_HTTP_TIMEOUT_MS);

        int httpResponseCode = http.POST(requestBody);
        result.httpCode = httpResponseCode;
        result.durationMs = millis() - startTime;

        if (httpResponseCode > 0) {
            if (httpResponseCode == 200) {
                // Успешный запрос - парсим напрямую из потока, экономя RAM!
                WiFiClient* stream = http.getStreamPtr();
                if (stream) {
                    parseResponse(*stream, result);
                } else {
                    result.text = "Ошибка: не удалось получить поток данных.";
                }
                http.end();
                return result;
            } else if ((httpResponseCode == 500 || httpResponseCode == 503) && attempt < maxAttempts) {
                http.end();
                delay(1000);
                continue;
            } else {
                // Ошибка API (400, 401, 403, 429) - читаем как строку для отладки
                String payload = http.getString();
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
                // Если произошла ошибка соединения, возможно нужно переподключиться
                _secureClient.stop();
                delay(1000);
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

GeminiResponse GeminiClient::askAgent(const String& goalPrompt, int maxSteps) {
    if (maxSteps <= 0) maxSteps = 1;
    if (maxSteps > 5) maxSteps = 5;

    GeminiResponse finalResult;
    finalResult.success = false;
    finalResult.agentStepsExecuted = 0;

    unsigned long startTotalTime = millis();
    int totalPromptTokens = 0;
    int totalCandidateTokens = 0;

    String currentPrompt = goalPrompt;
    String historyContext = "";

    for (int step = 1; step <= maxSteps; step++) {
        String requestBody = buildRequestBody(currentPrompt);
        GeminiResponse stepResp = sendRawRequest(requestBody);

        totalPromptTokens += stepResp.promptTokens;
        totalCandidateTokens += stepResp.candidateTokens;

        if (!stepResp.success) {
            finalResult = stepResp;
            break;
        }

        finalResult = stepResp;
        finalResult.agentStepsExecuted = step;

        String actionResult;
        if (extractAndExecuteAction(stepResp.text, actionResult)) {
            Serial.printf("\n[АГЕНТ ШАГ %d/%d] Выполнено действие на ESP32:\n", step, maxSteps);
            Serial.printf(" -> Результат: %s\n", actionResult.c_str());

            if (step < maxSteps) {
                historyContext += "[Команда ИИ]: " + stepResp.text + "\n";
                currentPrompt = "[Наблюдение с платы ESP32]: " + actionResult + 
                                "\nПроанализируй полученные данные с оборудования. Продолжи выполнение поставленной задачи или дай итоговое заключение.";
                continue;
            } else {
                finalResult.text += "\n\n[Выполнение на ESP32]: " + actionResult;
                break;
            }
        } else {
            break;
        }
    }

    finalResult.promptTokens = totalPromptTokens;
    finalResult.candidateTokens = totalCandidateTokens;
    finalResult.totalTokens = totalPromptTokens + totalCandidateTokens;
    finalResult.durationMs = millis() - startTotalTime;

    if (finalResult.success) {
        addHistory("user", goalPrompt);
        addHistory("model", finalResult.text);
    }

    if (_usageTracker && finalResult.success) {
        _usageTracker->recordRequest(finalResult.promptTokens, finalResult.candidateTokens, finalResult.totalTokens);
    }

    return finalResult;
}

GeminiResponse GeminiClient::ask(const String& prompt) {
    return askAgent(prompt, 2);
}

#if GEMINI_ENABLE_VISION
GeminiResponse GeminiClient::askWithImage(const String& prompt, 
                                          const uint8_t* imageData, 
                                          size_t imageSize, 
                                          const String& mimeType) {
    GeminiResponse result;
    result.success = false;
    result.httpCode = 0;
    result.totalTokens = 0;
    result.durationMs = 0;
#if GEMINI_ENABLE_FUNCTION_CALLING
    result.hasFunctionCall = false;
#endif

    const AppConfig& cfg = _configMgr.getConfig();

    if (cfg.apiKey.isEmpty()) {
        result.text = "Ошибка: API-ключ Gemini не настроен!";
        return result;
    }

    if (WiFi.status() != WL_CONNECTED) {
        result.text = "Ошибка: Нет подключения к Wi-Fi сети!";
        return result;
    }

    if (!imageData || imageSize == 0) {
        result.text = "Ошибка: Буфер изображения пуст!";
        return result;
    }

    unsigned long startTime = millis();
    String url = buildApiUrl();
    String requestBody = buildRequestBodyWithImage(prompt, imageData, imageSize, mimeType);

    ensureTlsConfigured();
    // Vision requests might need slightly more timeout for image upload and processing
    _secureClient.setTimeout(GEMINI_HTTP_TIMEOUT_MS / 1000 + 5);

    HTTPClient http;
    http.setReuse(true);
    if (!http.begin(_secureClient, url)) {
        result.text = "Ошибка: Не удалось инициализировать HTTPS соединение.";
        result.durationMs = millis() - startTime;
        return result;
    }

    http.addHeader("Content-Type", "application/json; charset=utf-8");
    http.addHeader("x-goog-api-key", cfg.apiKey);
    http.setTimeout(GEMINI_HTTP_TIMEOUT_MS + 5000);

    int httpResponseCode = http.POST(requestBody);
    result.httpCode = httpResponseCode;
    result.durationMs = millis() - startTime;

    if (httpResponseCode == 200) {
        WiFiClient* stream = http.getStreamPtr();
        if (stream) {
            parseResponse(*stream, result);
        } else {
            result.text = "Ошибка: не удалось получить поток данных.";
        }
        http.end();

#if GEMINI_ENABLE_USAGE_TRACKER
        if (_usageTracker && result.success) {
            _usageTracker->recordRequest(result.promptTokens, result.candidateTokens, result.totalTokens);
        }
#endif

        processHardwareActions(result);
        return result;
    } else {
        String payload = http.getString();
        parseResponse(payload, result);
        if (result.text.isEmpty() || result.text.startsWith("Не удалось извлечь")) {
            char errHdr[128];
            snprintf(errHdr, sizeof(errHdr), "Ошибка HTTP %d: %s", httpResponseCode, getHttpErrorDescription(httpResponseCode).c_str());
            result.text = errHdr;
        }
        http.end();
        return result;
    }
}
#endif

#if GEMINI_ENABLE_STREAMING
GeminiResponse GeminiClient::streamAsk(const String& prompt, GeminiStreamCallback onChunk) {
    GeminiResponse result;
    result.success = false;
    result.httpCode = 0;
    result.totalTokens = 0;
    result.durationMs = 0;
#if GEMINI_ENABLE_FUNCTION_CALLING
    result.hasFunctionCall = false;
#endif

    const AppConfig& cfg = _configMgr.getConfig();

    if (cfg.apiKey.isEmpty()) {
        result.text = "Ошибка: API-ключ Gemini не настроен!";
        return result;
    }

    if (WiFi.status() != WL_CONNECTED) {
        result.text = "Ошибка: Нет подключения к Wi-Fi сети!";
        return result;
    }

    unsigned long startTime = millis();
    String url = buildStreamApiUrl();
    String requestBody = buildRequestBody(prompt);

    ensureTlsConfigured();

    HTTPClient http;
    http.setReuse(true);
    if (!http.begin(_secureClient, url)) {
        result.text = "Ошибка: Не удалось инициализировать HTTPS соединение.";
        result.durationMs = millis() - startTime;
        return result;
    }

    http.addHeader("Content-Type", "application/json; charset=utf-8");
    http.addHeader("x-goog-api-key", cfg.apiKey);
    http.setTimeout(GEMINI_HTTP_TIMEOUT_MS);

    int httpResponseCode = http.POST(requestBody);
    result.httpCode = httpResponseCode;
    result.durationMs = millis() - startTime;

    if (httpResponseCode == 200) {
        WiFiClient* stream = http.getStreamPtr();
        String accumulatedText = "";
        
        while (http.connected() && (stream->available() || stream->connected())) {
            if (stream->available()) {
                String line = stream->readStringUntil('\n');
                line.trim();
                if (line.startsWith("data: ")) {
                    String jsonChunk = line.substring(6);
                    jsonChunk.trim();
                    if (jsonChunk.length() > 0) {
                        JsonDocument doc;
                        DeserializationError err = deserializeJson(doc, jsonChunk);
                        if (!err && doc["candidates"].is<JsonArray>() && doc["candidates"].size() > 0) {
                            JsonObject cand = doc["candidates"][0];
                            if (cand["content"]["parts"].is<JsonArray>() && cand["content"]["parts"].size() > 0) {
                                const char* chunkText = cand["content"]["parts"][0]["text"];
                                if (chunkText != nullptr) {
                                    String chunkStr = String(chunkText);
                                    accumulatedText += chunkStr;
                                    if (onChunk) onChunk(chunkStr, false);
                                }
                            }
                        }
                    }
                }
            }
            yield();
        }
        
        if (onChunk) onChunk("", true); // Сигнал завершения потока
        
        http.end();
        result.success = true;
        result.text = accumulatedText;
        result.durationMs = millis() - startTime;
        
        result.promptTokens = prompt.length() / 4;
        result.candidateTokens = accumulatedText.length() / 4;
        result.totalTokens = result.promptTokens + result.candidateTokens;
        
#if GEMINI_ENABLE_USAGE_TRACKER
        if (_usageTracker) {
            _usageTracker->recordRequest(result.promptTokens, result.candidateTokens, result.totalTokens);
        }
#endif

        addHistory("user", prompt);
        addHistory("model", accumulatedText);

        processHardwareActions(result);
        return result;
    } else {
        String payload = http.getString();
        parseResponse(payload, result);
        if (result.text.isEmpty() || result.text.startsWith("Не удалось извлечь")) {
            char errHdr[128];
            snprintf(errHdr, sizeof(errHdr), "Ошибка HTTP %d: %s", httpResponseCode, getHttpErrorDescription(httpResponseCode).c_str());
            result.text = errHdr;
        }
        http.end();
        return result;
    }
}
#endif

bool GeminiClient::listAvailableModels() {
    const AppConfig& cfg = _configMgr.getConfig();

    if (cfg.apiKey.isEmpty()) {
        Serial.println("Ошибка: API-ключ Gemini не настроен!");
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Ошибка: Нет подключения к Wi-Fi сети!");
        return false;
    }

    String url = String(GEMINI_API_HOST) + "?key=" + cfg.apiKey;
    ensureTlsConfigured();
    // Temporarily increase timeout for model listing which takes longer
    _secureClient.setTimeout(30);

    HTTPClient http;
    http.setReuse(true);
    if (!http.begin(_secureClient, url)) {
        Serial.println("Ошибка: Не удалось инициализировать HTTPS соединение.");
        return false;
    }

    http.setTimeout(30000);
    int httpResponseCode = http.GET();

    if (httpResponseCode == 200) {
        String payload = http.getString();
        http.end();

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
            Serial.printf("Ошибка парсинга JSON: %s\n", error.c_str());
            return false;
        }

        if (doc["models"].is<JsonArray>()) {
            JsonArray modelsArray = doc["models"].as<JsonArray>();
            _cachedModels.clear();

            for (JsonObject modelObj : modelsArray) {
                const char* name = modelObj["name"];
                if (name != nullptr) {
                    String fullModelName = String(name);
                    if (fullModelName.startsWith("models/")) {
                        fullModelName = fullModelName.substring(7);
                    }

                    bool supportsGenerate = false;
                    if (modelObj["supportedGenerationMethods"].is<JsonArray>()) {
                        for (JsonVariant method : modelObj["supportedGenerationMethods"].as<JsonArray>()) {
                            if (method.as<String>() == "generateContent") {
                                supportsGenerate = true;
                                break;
                            }
                        }
                    }

                    if (supportsGenerate) {
                        _cachedModels.push_back(fullModelName);
                    }
                }
            }

            saveCachedModels();
            // Restore default timeout
            _secureClient.setTimeout(GEMINI_HTTP_TIMEOUT_MS / 1000);
            return true;
        }
    } else {
        Serial.printf("Ошибка запроса моделей (HTTP %d): %s\n", httpResponseCode, getHttpErrorDescription(httpResponseCode).c_str());
        http.end();
        // Restore default timeout
        _secureClient.setTimeout(GEMINI_HTTP_TIMEOUT_MS / 1000);
        return false;
    }

    // Restore default timeout
    _secureClient.setTimeout(GEMINI_HTTP_TIMEOUT_MS / 1000);
    return false;
}

bool GeminiClient::testConnection() {
    IPAddress resolvedIP;
    if (!WiFi.hostByName("generativelanguage.googleapis.com", resolvedIP)) {
        return false;
    }

    ensureTlsConfigured();
    _secureClient.setTimeout(10);

    if (!_secureClient.connect("generativelanguage.googleapis.com", 443)) {
        // Restore default timeout
        _secureClient.setTimeout(GEMINI_HTTP_TIMEOUT_MS / 1000);
        return false;
    }

    _secureClient.stop();
    // Restore default timeout
    _secureClient.setTimeout(GEMINI_HTTP_TIMEOUT_MS / 1000);
    return true;
}
