#include "ConfigManager.h"

// Значения по умолчанию
static const char* DEFAULT_MODEL = "gemini-3.5-flash-lite";
static const char* DEFAULT_DNS_PRIMARY = "111.88.96.50";   // xbox-dns.ru Primary IPv4
static const char* DEFAULT_DNS_SECONDARY = "111.88.96.51"; // xbox-dns.ru Secondary IPv4
static const char* DEFAULT_SYSTEM_PROMPT = "Ты полезный AI-ассистент на ESP32. Отвечай кратко, понятно и по делу.";
static const int DEFAULT_MAX_TOKENS = 1024;
static const float DEFAULT_TEMPERATURE = 0.7f;

// Вспомогательная функция санитизации строк
static String sanitize(String str) {
    str.trim();
    while (str.endsWith("\r") || str.endsWith("\n") || str.endsWith(" ") || str.endsWith("\t")) {
        str.remove(str.length() - 1);
        str.trim();
    }
    while (str.startsWith("\r") || str.startsWith("\n") || str.startsWith(" ") || str.startsWith("\t")) {
        str.remove(0, 1);
        str.trim();
    }
    // Удаление обрамляющих кавычек, если пользователь ввел значение в кавычках
    if ((str.startsWith("\"") && str.endsWith("\"")) || (str.startsWith("'") && str.endsWith("'"))) {
        if (str.length() >= 2) {
            str = str.substring(1, str.length() - 1);
            str.trim();
        }
    }
    return str;
}

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
    _config.isConfigured = false;
}

void ConfigManager::load() {
    _config.wifiSsid = sanitize(_prefs.getString("ssid", ""));
    _config.wifiPassword = sanitize(_prefs.getString("pass", ""));
    _config.apiKey = sanitize(_prefs.getString("api_key", ""));
    _config.model = sanitize(_prefs.getString("model", DEFAULT_MODEL));
    _config.dnsPrimary = sanitize(_prefs.getString("dns1", DEFAULT_DNS_PRIMARY));
    _config.dnsSecondary = sanitize(_prefs.getString("dns2", DEFAULT_DNS_SECONDARY));
    _config.systemPrompt = _prefs.getString("prompt", DEFAULT_SYSTEM_PROMPT);
    _config.systemPrompt.trim();
    _config.maxTokens = _prefs.getInt("max_tokens", DEFAULT_MAX_TOKENS);
    _config.temperature = _prefs.getFloat("temp", DEFAULT_TEMPERATURE);
    _config.isConfigured = _prefs.getBool("configured", false);

    // Валидация значений по умолчанию
    if (_config.model.isEmpty()) _config.model = DEFAULT_MODEL;
    if (_config.dnsPrimary.isEmpty()) _config.dnsPrimary = DEFAULT_DNS_PRIMARY;
    if (_config.dnsSecondary.isEmpty()) _config.dnsSecondary = DEFAULT_DNS_SECONDARY;
    if (_config.maxTokens <= 0) _config.maxTokens = DEFAULT_MAX_TOKENS;
    if (_config.temperature < 0.0f || _config.temperature > 2.0f) _config.temperature = DEFAULT_TEMPERATURE;
}

bool ConfigManager::save() {
    bool ok = true;
    _config.wifiSsid = sanitize(_config.wifiSsid);
    _config.wifiPassword = sanitize(_config.wifiPassword);
    _config.apiKey = sanitize(_config.apiKey);
    _config.model = sanitize(_config.model);
    _config.dnsPrimary = sanitize(_config.dnsPrimary);
    _config.dnsSecondary = sanitize(_config.dnsSecondary);
    _config.systemPrompt.trim();

    _config.isConfigured = (!_config.wifiSsid.isEmpty() && !_config.apiKey.isEmpty());

    ok &= (_prefs.putString("ssid", _config.wifiSsid) > 0 || _config.wifiSsid.isEmpty());
    ok &= (_prefs.putString("pass", _config.wifiPassword) > 0 || _config.wifiPassword.isEmpty());
    ok &= (_prefs.putString("api_key", _config.apiKey) > 0 || _config.apiKey.isEmpty());
    ok &= (_prefs.putString("model", _config.model) > 0);
    ok &= (_prefs.putString("dns1", _config.dnsPrimary) > 0);
    ok &= (_prefs.putString("dns2", _config.dnsSecondary) > 0);
    ok &= (_prefs.putString("prompt", _config.systemPrompt) > 0);
    ok &= (_prefs.putInt("max_tokens", _config.maxTokens) > 0);
    ok &= (_prefs.putFloat("temp", _config.temperature) > 0);
    ok &= _prefs.putBool("configured", _config.isConfigured);

    if (ok) {
        Serial.println(F("[УСПЕХ] Параметры и пароль Wi-Fi сохранены в NVS Flash."));
    } else {
        Serial.println(F("[ПРЕДУПРЕЖДЕНИЕ] Некоторые параметры не удалось записать в NVS."));
    }
    return ok;
}

bool ConfigManager::hasSavedConfig() const {
    return (_config.isConfigured && !_config.wifiSsid.isEmpty() && !_config.apiKey.isEmpty());
}

bool ConfigManager::hasSavedApiKey() const {
    return !_config.apiKey.isEmpty();
}

bool ConfigManager::isConfigured() const {
    return hasSavedConfig() && !_config.wifiPassword.isEmpty();
}

void ConfigManager::setWifi(const String& ssid, const String& password) {
    _config.wifiSsid = sanitize(ssid);
    _config.wifiPassword = sanitize(password);
}

void ConfigManager::setApiKey(const String& apiKey) {
    _config.apiKey = sanitize(apiKey);
}

void ConfigManager::setModel(const String& model) {
    _config.model = sanitize(model);
}

void ConfigManager::setDns(const String& primary, const String& secondary) {
    _config.dnsPrimary = sanitize(primary);
    if (!secondary.isEmpty()) {
        _config.dnsSecondary = sanitize(secondary);
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
