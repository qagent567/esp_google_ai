#include "GeminiESP32.h"

GeminiESP32::GeminiESP32(const String& apiKey, const String& model)
    : _hardwareEnabled(true), _smartDnsEnabled(true) {
    _client = new GeminiClient(_config, &_usage, &_hardware);
    
    if (apiKey.length() > 0) {
        _config.setApiKey(apiKey);
    }
    if (model.length() > 0) {
        _config.setModel(model);
    }
}

GeminiESP32::~GeminiESP32() {
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
    _usage.begin();
    _hardware.begin();
    
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

void GeminiESP32::enableHardwareControl(bool enable) {
    _hardwareEnabled = enable;
    _hardware.setEnabled(enable);
    if (_client) {
        delete _client;
        _client = new GeminiClient(_config, &_usage, enable ? &_hardware : nullptr);
    }
}

void GeminiESP32::setAllowedPins(const std::vector<uint8_t>& allowedPins) {
    _hardware.setAllowedPins(allowedPins);
}

void GeminiESP32::allowAllSafePins() {
    _hardware.allowAllSafePins();
}

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

bool GeminiESP32::ping() {
    if (!_client) return false;
    return _client->testConnection();
}
