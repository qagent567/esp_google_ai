#include "NetworkManager.h"
#include <esp_wifi.h>
#include <lwip/dns.h>
#include <lwip/ip_addr.h>

NetworkManager::NetworkManager(ConfigManager& configMgr)
    : _configMgr(configMgr) {}

void NetworkManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
}

void NetworkManager::setAutoReconnect(bool enable) {
    _autoReconnectEnabled = enable;
}

void NetworkManager::applyCustomDNS() {
    const AppConfig& cfg = _configMgr.getConfig();
    if (cfg.dnsPrimary.isEmpty()) return;

    ip_addr_t d1, d2;
    if (ipaddr_aton(cfg.dnsPrimary.c_str(), &d1)) {
        dns_setserver(0, &d1);
        if (!cfg.dnsSecondary.isEmpty() && ipaddr_aton(cfg.dnsSecondary.c_str(), &d2)) {
            dns_setserver(1, &d2);
        }
        Serial.printf("[DNS] Применены Smart DNS серверы: %s, %s\n", 
                      cfg.dnsPrimary.c_str(), 
                      cfg.dnsSecondary.isEmpty() ? "нет" : cfg.dnsSecondary.c_str());
    } else {
        Serial.printf("[DNS] Ошибка: Неверный формат DNS IP: %s\n", cfg.dnsPrimary.c_str());
    }
}

bool NetworkManager::connect() {
    _autoReconnectEnabled = true;

    const AppConfig& cfg = _configMgr.getConfig();
    if (cfg.wifiSsid.isEmpty()) {
        Serial.println(F("[Wi-Fi] SSID не задан! Введите команду 'set ssid <ваша_сеть>'"));
        return false;
    }

    String ssid = cfg.wifiSsid;
    String pass = cfg.wifiPassword;
    ssid.replace("\r", ""); ssid.replace("\n", "");
    pass.replace("\r", ""); pass.replace("\n", "");

    Serial.printf("[Wi-Fi] Подключение к сети: '%s'...\n", ssid.c_str());
    Serial.println(F("[Wi-Fi] Нажмите 'e' для отмены поиска и подключения."));

    // Сброс и перезапуск Wi-Fi
    WiFi.disconnect(true, true);
    delay(200);

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    strncpy((char*)wifi_config.sta.ssid, ssid.c_str(), sizeof(wifi_config.sta.ssid) - 1);
    if (!pass.isEmpty()) {
        strncpy((char*)wifi_config.sta.password, pass.c_str(), sizeof(wifi_config.sta.password) - 1);
    }
    wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    // Поддержка PMF (Protected Management Frames) для совместимости с Windows Hotspot и современными роутерами
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();

    // Ожидание подключения с таймаутом (до 15 секунд) или отмена по нажатию 'e'
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        if (Serial.available()) {
            char c = (char)Serial.read();
            if (c == 'e' || c == 'E') {
                WiFi.disconnect(true);
                Serial.println(F("\n[Wi-Fi] Поиск сети остановлен пользователем (нажата клавиша 'e')."));
                _autoReconnectEnabled = false;
                return false;
            }
        }
        delay(250);
        Serial.print(F("."));
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        _wasConnected = true;
        // Применяем Smart DNS после получения сетевых настроек от DHCP
        applyCustomDNS();
        
        // Запуск синхронизации времени по протоколу NTP (UTC+3 для Московского времени)
        configTime(3 * 3600, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");

        Serial.println(F("[Wi-Fi] Успешно подключено!"));
        Serial.printf("[Wi-Fi] Получен IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[Wi-Fi] Уровень сигнала: %d dBm\n", WiFi.RSSI());
        Serial.printf("[Wi-Fi] Активный DNS 1: %s\n", WiFi.dnsIP(0).toString().c_str());
        Serial.printf("[Wi-Fi] Активный DNS 2: %s\n", WiFi.dnsIP(1).toString().c_str());
        return true;
    } else {
        Serial.printf("[Wi-Fi] Не удалось подключиться (Код статуса: %d). Проверьте имя сети и пароль.\n", WiFi.status());
        return false;
    }
}

void NetworkManager::disconnect() {
    WiFi.disconnect();
    _wasConnected = false;
    _autoReconnectEnabled = false;
    Serial.println(F("[Wi-Fi] Отключено от сети."));
}

bool NetworkManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

void NetworkManager::update() {
    bool connectedNow = isConnected();

    // Отслеживание потери связи
    if (_wasConnected && !connectedNow) {
        Serial.println(F("\n[Wi-Fi] Внимание: Соединение с Wi-Fi потеряно!"));
        _wasConnected = false;
        _lastReconnectAttempt = millis();
    }

    // Авто-переподключение, если сеть настроена, но связь пропала
    if (_autoReconnectEnabled && !connectedNow && !_configMgr.getConfig().wifiSsid.isEmpty()) {
        if (millis() - _lastReconnectAttempt >= RECONNECT_INTERVAL_MS) {
            _lastReconnectAttempt = millis();
            Serial.println(F("[Wi-Fi] Попытка авто-переподключения к сети..."));
            if (connect()) {
                _wasConnected = true;
            }
        }
    }
}

void NetworkManager::scanNetworks() {
    Serial.println(F("\n[Wi-Fi] Сканирование доступных беспроводных сетей..."));
    int n = WiFi.scanNetworks();
    if (n == 0) {
        Serial.println(F("[Wi-Fi] Сети не найдены."));
    } else {
        Serial.printf("[Wi-Fi] Найдено сетей: %d\n", n);
        for (int i = 0; i < n; ++i) {
            const char* encType = "Открытая";
            switch (WiFi.encryptionType(i)) {
                case WIFI_AUTH_WEP: encType = "WEP"; break;
                case WIFI_AUTH_WPA_PSK: encType = "WPA"; break;
                case WIFI_AUTH_WPA2_PSK: encType = "WPA2"; break;
                case WIFI_AUTH_WPA_WPA2_PSK: encType = "WPA/WPA2"; break;
                case WIFI_AUTH_WPA3_PSK: encType = "WPA3"; break;
                default: break;
            }
            Serial.printf("  %2d: %-24s (Сигнал: %3d dBm, Защита: %s)\n", 
                          i + 1, 
                          WiFi.SSID(i).c_str(), 
                          WiFi.RSSI(i), 
                          encType);
            delay(10);
        }
    }
    WiFi.scanDelete();
}

String NetworkManager::getLocalIP() const {
    return isConnected() ? WiFi.localIP().toString() : "Не подключен";
}

int NetworkManager::getRSSI() const {
    return isConnected() ? WiFi.RSSI() : 0;
}

String NetworkManager::getPrimaryDNS() const {
    return WiFi.dnsIP(0).toString();
}

String NetworkManager::getSecondaryDNS() const {
    return WiFi.dnsIP(1).toString();
}
