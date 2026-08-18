#include "ConfigManager.h"

// Значения по умолчанию
static const char* DEFAULT_MODEL = "gemini-2.0-flash";
static const char* DEFAULT_DNS_PRIMARY = "111.88.96.50";   // xbox-dns.ru Primary IPv4
static const char* DEFAULT_DNS_SECONDARY = "111.88.96.51"; // xbox-dns.ru Secondary IPv4
static const char* DEFAULT_SYSTEM_PROMPT = "Ты полезный AI-ассистент на ESP32. Отвечай кратко, понятно и по делу.";
static const int DEFAULT_MAX_TOKENS = 1024;
static const float DEFAULT_TEMPERATURE = 0.7f;

ConfigManager::ConfigManager() {
    resetToDefaults();
}

ConfigManager::~ConfigManager() {
    _prefs.end();
}

bool ConfigManager::begin() {
    if (!_prefs.begin(NVS_NAMESPACE, false)) {
        Serial.println(F("[ОШИБКА] Не удалось инициализировать NVS для настроек!"));
        return false;
    }
    load();
    return true;
}

void ConfigManager::resetToDefaults() {
    _config.wifiSsid = "";
    _config.wifiPassword = "";
    _config.apiKey = "";
    _config.model = DEFAULT_MODEL;
    _config.dnsPrimary = DEFAULT_DNS_PRIMARY;
    _config.dnsSecondary = DEFAULT_DNS_SECONDARY;
    _config.systemPrompt = DEFAULT_SYSTEM_PROMPT;
    _config.maxTokens = DEFAULT_MAX_TOKENS;
    _config.temperature = DEFAULT_TEMPERATURE;
}

void ConfigManager::load() {
    _config.wifiSsid = _prefs.getString("ssid", "");
    _config.wifiPassword = _prefs.getString("pass", "");
    _config.apiKey = _prefs.getString("api_key", "");
    _config.model = _prefs.getString("model", DEFAULT_MODEL);
    _config.dnsPrimary = _prefs.getString("dns1", DEFAULT_DNS_PRIMARY);
    _config.dnsSecondary = _prefs.getString("dns2", DEFAULT_DNS_SECONDARY);
    _config.systemPrompt = _prefs.getString("prompt", DEFAULT_SYSTEM_PROMPT);
    _config.maxTokens = _prefs.getInt("max_tokens", DEFAULT_MAX_TOKENS);
    _config.temperature = _prefs.getFloat("temp", DEFAULT_TEMPERATURE);

    // Валидация значений по умолчанию
    if (_config.model.isEmpty()) _config.model = DEFAULT_MODEL;
    if (_config.dnsPrimary.isEmpty()) _config.dnsPrimary = DEFAULT_DNS_PRIMARY;
    if (_config.dnsSecondary.isEmpty()) _config.dnsSecondary = DEFAULT_DNS_SECONDARY;
    if (_config.maxTokens <= 0) _config.maxTokens = DEFAULT_MAX_TOKENS;
    if (_config.temperature < 0.0f || _config.temperature > 2.0f) _config.temperature = DEFAULT_TEMPERATURE;
}

bool ConfigManager::save() {
    bool ok = true;
    ok &= (_prefs.putString("ssid", _config.wifiSsid) > 0 || _config.wifiSsid.isEmpty());
    ok &= (_prefs.putString("pass", _config.wifiPassword) > 0 || _config.wifiPassword.isEmpty());
    ok &= (_prefs.putString("api_key", _config.apiKey) > 0 || _config.apiKey.isEmpty());
    ok &= (_prefs.putString("model", _config.model) > 0);
    ok &= (_prefs.putString("dns1", _config.dnsPrimary) > 0);
    ok &= (_prefs.putString("dns2", _config.dnsSecondary) > 0);
    ok &= (_prefs.putString("prompt", _config.systemPrompt) > 0);
    ok &= (_prefs.putInt("max_tokens", _config.maxTokens) > 0);
    ok &= (_prefs.putFloat("temp", _config.temperature) > 0);

    if (ok) {
        Serial.println(F("[УСПЕХ] Настройки успешно сохранены в NVS память!"));
    } else {
        Serial.println(F("[ПРЕДУПРЕЖДЕНИЕ] Некоторые параметры не удалось записать в NVS."));
    }
    return ok;
}

bool ConfigManager::isConfigured() const {
    return (!_config.wifiSsid.isEmpty() && !_config.apiKey.isEmpty());
}

void ConfigManager::setWifi(const String& ssid, const String& password) {
    _config.wifiSsid = ssid;
    _config.wifiPassword = password;
}

void ConfigManager::setApiKey(const String& apiKey) {
    _config.apiKey = apiKey;
}

void ConfigManager::setModel(const String& model) {
    _config.model = model;
}

void ConfigManager::setDns(const String& primary, const String& secondary) {
    _config.dnsPrimary = primary;
    if (!secondary.isEmpty()) {
        _config.dnsSecondary = secondary;
    }
}

void ConfigManager::setSystemPrompt(const String& prompt) {
    _config.systemPrompt = prompt;
}

void ConfigManager::setMaxTokens(int tokens) {
    if (tokens > 0 && tokens <= 8192) {
        _config.maxTokens = tokens;
    }
}

void ConfigManager::setTemperature(float temp) {
    if (temp >= 0.0f && temp <= 2.0f) {
        _config.temperature = temp;
    }
}
