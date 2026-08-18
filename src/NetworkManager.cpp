#include "NetworkManager.h"
#include <lwip/dns.h>
#include <lwip/ip_addr.h>

NetworkManager::NetworkManager(ConfigManager& configMgr)
    : _configMgr(configMgr) {}

void NetworkManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
}

void NetworkManager::applyCustomDNS() {
    const AppConfig& cfg = _configMgr.getConfig();
    if (cfg.dnsPrimary.isEmpty()) return;

    IPAddress primaryIP, secondaryIP;
    bool pOk = primaryIP.fromString(cfg.dnsPrimary);
    bool sOk = secondaryIP.fromString(cfg.dnsSecondary);

    if (pOk) {
        // Установка через Arduino API (DHCP для IP, кастомные DNS)
        if (sOk) {
            WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, primaryIP, secondaryIP);
        } else {
            WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, primaryIP);
        }

        // Прямая установка через lwIP стек для 100% гарантии маршрутизации
        ip_addr_t d1, d2;
        ipaddr_aton(cfg.dnsPrimary.c_str(), &d1);
        dns_setserver(0, &d1);

        if (sOk) {
            ipaddr_aton(cfg.dnsSecondary.c_str(), &d2);
            dns_setserver(1, &d2);
        }

        Serial.printf("[DNS] Применены Smart DNS серверы: %s, %s\n", 
                      cfg.dnsPrimary.c_str(), 
                      sOk ? cfg.dnsSecondary.c_str() : "нет");
    } else {
        Serial.printf("[DNS] Ошибка: Неверный формат DNS IP: %s\n", cfg.dnsPrimary.c_str());
    }
}

bool NetworkManager::connect() {
    const AppConfig& cfg = _configMgr.getConfig();
    if (cfg.wifiSsid.isEmpty()) {
        Serial.println(F("[Wi-Fi] SSID не задан! Введите команду 'set ssid <ваша_сеть>'"));
        return false;
    }

    Serial.printf("[Wi-Fi] Подключение к сети: '%s'...\n", cfg.wifiSsid.c_str());
    Serial.println(F("[Wi-Fi] Нажмите 'e' для отмены поиска и подключения."));
    
    // Сброс предыдущего подключения
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_STA);

    // Применяем Smart DNS
    applyCustomDNS();

    if (cfg.wifiPassword.isEmpty()) {
        WiFi.begin(cfg.wifiSsid.c_str());
    } else {
        WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPassword.c_str());
    }

    // Ожидание подключения с таймаутом (до 15 секунд) или отмена по нажатию 'e'
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        // Проверка нажатия клавиши 'e' в терминале для остановки поиска
        if (Serial.available()) {
            char c = (char)Serial.read();
            if (c == 'e' || c == 'E') {
                WiFi.disconnect(true);
                Serial.println(F("\n[Wi-Fi] Поиск сети остановлен пользователем (нажата клавиша 'e')."));
                return false;
            }
        }
        delay(250);
        Serial.print(F("."));
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        _wasConnected = true;
        // Повторно закрепляем DNS после получения адреса от DHCP роутера
        applyCustomDNS();
        
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
    if (!connectedNow && !_configMgr.getConfig().wifiSsid.isEmpty()) {
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
