#pragma once

#include <Arduino.h>
#include <Preferences.h>

/**
 * @brief Структура для хранения настроек устройства в энергонезависимой памяти (NVS)
 */
struct AppConfig {
    String wifiSsid;        // SSID точки доступа Wi-Fi
    String wifiPassword;    // Пароль от Wi-Fi
    String apiKey;          // API-ключ Google AI Studio (Gemini)
    String model;           // Название модели Gemini (например, gemini-2.0-flash)
    String dnsPrimary;      // Основной Smart DNS (xbox-dns.ru: 111.88.96.50)
    String dnsSecondary;    // Дополнительный Smart DNS (xbox-dns.ru: 111.88.96.51)
    String systemPrompt;    // Системный промпт (инструкция роли)
    int maxTokens;          // Максимальное количество токенов в ответе
    float temperature;      // Температура генерации (0.0 - 2.0)
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

    // Сохранение текущих настроек в NVS
    bool save();

    // Сброс настроек на значения по умолчанию
    void resetToDefaults();

    // Проверка, заданы ли минимально необходимые параметры (Wi-Fi и API ключ)
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
