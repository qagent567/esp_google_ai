#pragma once

#include <Arduino.h>
#include <Preferences.h>

/**
 * @brief Структура для хранения настроек устройства в энергонезависимой памяти (NVS)
 */
struct AppConfig {
    String wifiSsid;        // SSID точки доступа Wi-Fi (сохраняется во Flash)
    String wifiPassword;    // Пароль от Wi-Fi (хранится только в RAM до перезагрузки)
    String apiKey;          // API-ключ Google AI Studio (сохраняется во Flash)
    String model;           // Название модели Gemini (сохраняется во Flash)
    String dnsPrimary;      // Основной Smart DNS (xbox-dns.ru: 111.88.96.50)
    String dnsSecondary;    // Дополнительный Smart DNS (xbox-dns.ru: 111.88.96.51)
    String systemPrompt;    // Системный промпт (инструкция роли)
    int maxTokens;          // Максимальное количество токенов в ответе
    float temperature;      // Температура генерации (0.0 - 2.0)
    bool isConfigured;      // Флаг сохранения корректной конфигурации
};

/**
 * @brief Класс для управления загрузкой и сохранением настроек во Flash-память ESP32 через Preferences (NVS)
 */
class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();

    // Инициализация NVS и загрузка конфигурации
    bool begin();

    // Загрузка настроек из NVS
    void load();

    // Сохранение текущих настроек в NVS (вызывается только после успешного подключения)
    bool save();

    // Сброс настроек на значения по умолчанию
    void resetToDefaults();

    // Проверка, сохранены ли основные параметры (SSID и API-ключ)
    bool hasSavedConfig() const;

    // Проверка готовности к работе в текущей сессии (есть SSID, API-ключ и пароль в RAM)
    bool isConfigured() const;

    // Геттеры и сеттеры
    AppConfig& getConfig() { return _config; }
    const AppConfig& getConfig() const { return _config; }

    void setWifi(const String& ssid, const String& password);
    void setApiKey(const String& apiKey);
    void setModel(const String& model);
    void setDns(const String& primary, const String& secondary = "");
    void setSystemPrompt(const String& prompt);
    void setMaxTokens(int tokens);
    void setTemperature(float temp);

private:
    Preferences _prefs;
    AppConfig _config;

    const char* NVS_NAMESPACE = "esp_gemini";
};
