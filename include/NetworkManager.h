#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "ConfigManager.h"

/**
 * @brief Класс для управления подключением к Wi-Fi и настройкой Smart DNS
 */
class NetworkManager {
public:
    NetworkManager(ConfigManager& configMgr);

    // Инициализация сетевого интерфейса
    void begin();

    // Запуск подключения к Wi-Fi с текущими настройками
    bool connect();

    // Отключение от сети
    void disconnect();

    // Проверка статуса подключения
    bool isConnected() const;

    // Фоновое обновление и авто-переподключение (вызывать в loop)
    void update();

    // Применение пользовательских DNS (xbox-dns.ru)
    void applyCustomDNS();

    // Сканирование доступных сетей Wi-Fi с выводом в Serial
    void scanNetworks();

    // Получить текущий IP
    String getLocalIP() const;

    // Получить уровень сигнала (RSSI)
    int getRSSI() const;

    // Получить адреса текущих DNS
    String getPrimaryDNS() const;
    String getSecondaryDNS() const;

    // Включение/отключение авто-переподключения
    void setAutoReconnect(bool enable);

private:
    ConfigManager& _configMgr;
    unsigned long _lastReconnectAttempt = 0;
    const unsigned long RECONNECT_INTERVAL_MS = 10000; // Попытка переподключения каждые 10 секунд
    bool _wasConnected = false;
    bool _autoReconnectEnabled = true;
};
