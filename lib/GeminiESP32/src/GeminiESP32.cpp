#include "GeminiESP32.h"

#if GEMINI_ENABLE_ASYNC
struct AsyncQueryContext {
    GeminiClient* client;
    String prompt;
    GeminiResponseCallback onResponse;
    GeminiTextCallback onText;
#if GEMINI_ENABLE_STREAMING
    GeminiStreamCallback onChunk;
    bool isStream;
#endif
    bool isTextOnly;
    volatile bool* pBusyFlag;
};

static void geminiAsyncWorker(void* pvParameters) {
    AsyncQueryContext* ctx = static_cast<AsyncQueryContext*>(pvParameters);
    if (ctx && ctx->client) {
#if GEMINI_ENABLE_STREAMING
        if (ctx->isStream) {
            GeminiResponse resp = ctx->client->streamAsk(ctx->prompt, ctx->onChunk);
            if (ctx->onResponse) {
                ctx->onResponse(resp);
            }
        } else {
#endif
            GeminiResponse resp = ctx->client->ask(ctx->prompt);
            if (ctx->isTextOnly) {
                if (ctx->onText) ctx->onText(resp.text);
            } else {
                if (ctx->onResponse) ctx->onResponse(resp);
            }
#if GEMINI_ENABLE_STREAMING
        }
#endif
    }
    if (ctx && ctx->pBusyFlag) {
        *(ctx->pBusyFlag) = false;
    }
    delete ctx;
    vTaskDelete(NULL);
}
#endif

GeminiESP32::GeminiESP32(const String& apiKey, const String& model)
#if GEMINI_ENABLE_HARDWARE
    : _hardwareEnabled(true)
    , _smartDnsEnabled(true)
#else
    : _smartDnsEnabled(true)
#endif
#if GEMINI_ENABLE_ASYNC
    , _isBusy(false)
#endif
{
    _client = new GeminiClient(
        _config
#if GEMINI_ENABLE_USAGE_TRACKER
        , &_usage
#else
        , nullptr
#endif
#if GEMINI_ENABLE_HARDWARE
        , &_hardware
#else
        , nullptr
#endif
#if GEMINI_ENABLE_FUNCTION_CALLING
        , &_functions
#endif
    );
    
    if (apiKey.length() > 0) {
        _config.setApiKey(apiKey);
    }
    if (model.length() > 0) {
        _config.setModel(model);
    }
}

GeminiESP32::~GeminiESP32() {
#if GEMINI_ENABLE_WEB_DASHBOARD
    stopWebDashboard();
#endif
    if (_client) {
        delete _client;
        _client = nullptr;
    }
}

void GeminiESP32::begin(const String& apiKey, bool autoSmartDns) {
    _config.begin();
    if (apiKey.length() > 0) {
        _config.setApiKey(apiKey);
    }

#if GEMINI_ENABLE_USAGE_TRACKER
    _usage.begin();
#endif

#if GEMINI_ENABLE_HARDWARE
    _hardware.begin();
#endif
    
    _smartDnsEnabled = autoSmartDns;
    if (_smartDnsEnabled) {
        setSmartDns();
    }
}

void GeminiESP32::setApiKey(const String& apiKey) {
    _config.setApiKey(apiKey);
}

void GeminiESP32::setModel(const String& model) {
    _config.setModel(model);
}

void GeminiESP32::setSystemPrompt(const String& prompt) {
    _config.setSystemPrompt(prompt);
}

void GeminiESP32::setTemperature(float temp) {
    _config.setTemperature(temp);
}

void GeminiESP32::setTimezone(int tzOffsetHours) {
    _config.setTimezone(tzOffsetHours);
}

void GeminiESP32::setMaxTokens(int maxTokens) {
    _config.setMaxTokens(maxTokens);
}

void GeminiESP32::enableSmartDns(bool enable) {
    _smartDnsEnabled = enable;
    if (enable) {
        setSmartDns();
    }
}

void GeminiESP32::setSmartDns(const char* primary, const char* secondary) {
    ip_addr_t dns1, dns2;
    ipaddr_aton(primary, &dns1);
    ipaddr_aton(secondary, &dns2);
    dns_setserver(0, &dns1);
    dns_setserver(1, &dns2);
}

#if GEMINI_ENABLE_HARDWARE
void GeminiESP32::enableHardwareControl(bool enable) {
    _hardwareEnabled = enable;
    _hardware.setEnabled(enable);
    if (_client) {
        delete _client;
        _client = new GeminiClient(
            _config
#if GEMINI_ENABLE_USAGE_TRACKER
            , &_usage
#else
            , nullptr
#endif
            , enable ? &_hardware : nullptr
#if GEMINI_ENABLE_FUNCTION_CALLING
            , &_functions
#endif
        );
    }
}

void GeminiESP32::setAllowedPins(const std::vector<uint8_t>& allowedPins) {
    _hardware.setAllowedPins(allowedPins);
}

void GeminiESP32::allowAllSafePins() {
    _hardware.allowAllSafePins();
}
#endif

void GeminiESP32::clearHistory() {
    if (_client) _client->clearHistory();
}

#if GEMINI_ENABLE_NVS_HISTORY
void GeminiESP32::enablePersistentHistory(bool enable) {
    if (_client) _client->enablePersistentHistory(enable);
}
#endif

String GeminiESP32::ask(const String& prompt) {
    if (!_client) return "Клиент Gemini не инициализирован";
    GeminiResponse resp = _client->ask(prompt);
    return resp.text;
}

GeminiResponse GeminiESP32::query(const String& prompt) {
    if (!_client) {
        GeminiResponse err;
        err.success = false;
        err.text = "Клиент Gemini не инициализирован";
        return err;
    }
    return _client->ask(prompt);
}

#if GEMINI_ENABLE_VISION
String GeminiESP32::askWithImage(const String& prompt, 
                                 const uint8_t* imageData, 
                                 size_t imageSize, 
                                 const String& mimeType) {
    if (!_client) return "Клиент Gemini не инициализирован";
    GeminiResponse resp = _client->askWithImage(prompt, imageData, imageSize, mimeType);
    return resp.text;
}

GeminiResponse GeminiESP32::queryWithImage(const String& prompt, 
                                           const uint8_t* imageData, 
                                           size_t imageSize, 
                                           const String& mimeType) {
    if (!_client) {
        GeminiResponse err;
        err.success = false;
        err.text = "Клиент Gemini не инициализирован";
        return err;
    }
    return _client->askWithImage(prompt, imageData, imageSize, mimeType);
}
#endif

#if GEMINI_ENABLE_STREAMING
GeminiResponse GeminiESP32::streamAsk(const String& prompt, GeminiStreamCallback onChunk) {
    if (!_client) {
        GeminiResponse err;
        err.success = false;
        err.text = "Клиент Gemini не инициализирован";
        return err;
    }
    return _client->streamAsk(prompt, onChunk);
}
#endif

#if GEMINI_ENABLE_ASYNC
bool GeminiESP32::askAsync(const String& prompt, GeminiTextCallback onResponse) {
    if (_isBusy || !_client) return false;
    _isBusy = true;

    AsyncQueryContext* ctx = new AsyncQueryContext();
    ctx->client = _client;
    ctx->prompt = prompt;
    ctx->onText = onResponse;
    ctx->onResponse = nullptr;
#if GEMINI_ENABLE_STREAMING
    ctx->onChunk = nullptr;
    ctx->isStream = false;
#endif
    ctx->isTextOnly = true;
    ctx->pBusyFlag = &_isBusy;

    BaseType_t res = xTaskCreatePinnedToCore(
        geminiAsyncWorker,
        "gemini_async",
        GEMINI_ASYNC_STACK_SIZE,
        ctx,
        GEMINI_ASYNC_TASK_PRIORITY,
        NULL,
        GEMINI_ASYNC_TASK_CORE
    );

    if (res != pdPASS) {
        _isBusy = false;
        delete ctx;
        return false;
    }
    return true;
}

bool GeminiESP32::queryAsync(const String& prompt, GeminiResponseCallback onResponse) {
    if (_isBusy || !_client) return false;
    _isBusy = true;

    AsyncQueryContext* ctx = new AsyncQueryContext();
    ctx->client = _client;
    ctx->prompt = prompt;
    ctx->onText = nullptr;
    ctx->onResponse = onResponse;
#if GEMINI_ENABLE_STREAMING
    ctx->onChunk = nullptr;
    ctx->isStream = false;
#endif
    ctx->isTextOnly = false;
    ctx->pBusyFlag = &_isBusy;

    BaseType_t res = xTaskCreatePinnedToCore(
        geminiAsyncWorker,
        "gemini_async",
        GEMINI_ASYNC_STACK_SIZE,
        ctx,
        GEMINI_ASYNC_TASK_PRIORITY,
        NULL,
        GEMINI_ASYNC_TASK_CORE
    );

    if (res != pdPASS) {
        _isBusy = false;
        delete ctx;
        return false;
    }
    return true;
}

#if GEMINI_ENABLE_STREAMING
bool GeminiESP32::streamAskAsync(const String& prompt, GeminiStreamCallback onChunk, GeminiResponseCallback onComplete) {
    if (_isBusy || !_client) return false;
    _isBusy = true;

    AsyncQueryContext* ctx = new AsyncQueryContext();
    ctx->client = _client;
    ctx->prompt = prompt;
    ctx->onText = nullptr;
    ctx->onResponse = onComplete;
    ctx->onChunk = onChunk;
    ctx->isStream = true;
    ctx->isTextOnly = false;
    ctx->pBusyFlag = &_isBusy;

    BaseType_t res = xTaskCreatePinnedToCore(
        geminiAsyncWorker,
        "gemini_stream",
        GEMINI_ASYNC_STACK_SIZE,
        ctx,
        GEMINI_ASYNC_TASK_PRIORITY,
        NULL,
        GEMINI_ASYNC_TASK_CORE
    );

    if (res != pdPASS) {
        _isBusy = false;
        delete ctx;
        return false;
    }
    return true;
}
#endif
#endif

#if GEMINI_ENABLE_FUNCTION_CALLING
void GeminiESP32::registerFunction(const String& name, 
                                  const String& description, 
                                  const std::vector<FunctionParam>& params, 
                                  FunctionHandler handler) {
    _functions.registerFunction(name, description, params, handler);
}

void GeminiESP32::registerFunction(const String& name, 
                                  const String& description, 
                                  FunctionHandler handler) {
    _functions.registerFunction(name, description, handler);
}
#endif

#if GEMINI_ENABLE_WEB_DASHBOARD
bool GeminiESP32::startWebDashboard(uint16_t port, bool inBackground) {
    return _dashboard.begin(this, port, inBackground);
}

void GeminiESP32::stopWebDashboard() {
    _dashboard.stop();
}
#endif

bool GeminiESP32::ping() {
    if (!_client) return false;
    return _client->testConnection();
}
