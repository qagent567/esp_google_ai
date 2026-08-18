/**
 * @file main.cpp
 * @brief Основной файл проекта ESP32 + Google AI Studio (Gemini) с обходом блокировок РФ через Smart DNS
 * @author qagent567
 * @version 1.0.0
 */

#include <Arduino.h>
#include "ConfigManager.h"
#include "NetworkManager.h"
#include "GeminiClient.h"
#include "CLI.h"

// Глобальные объекты модулей системы
ConfigManager configManager;
NetworkManager networkManager(configManager);
GeminiClient geminiClient(configManager);
SerialCLI serialCli(configManager, networkManager, geminiClient);

void setup() {
    // Инициализация Serial интерфейса на скорости 115200 бод
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println(F("[СИСТЕМА] Запуск прошивки ESP32 Google AI Studio..."));

    // Инициализация менеджера конфигурации (NVS память)
    if (configManager.begin()) {
        Serial.println(F("[СИСТЕМА] Конфигурация NVS успешно инициализирована."));
    } else {
        Serial.println(F("[СИСТЕМА] Предупреждение: Сбой инициализации NVS!"));
    }

    // Инициализация сетевого интерфейса
    networkManager.begin();

    // Если параметры Wi-Fi уже сохранены в памяти, пробуем подключиться
    if (!configManager.getConfig().wifiSsid.isEmpty()) {
        Serial.println(F("[СИСТЕМА] Найдены сохраненные настройки сети. Подключение..."));
        networkManager.connect();
    }

    // Инициализация командной строки (CLI)
    serialCli.begin();
}

void loop() {
    // Обработка команд пользователя из Serial монитора
    serialCli.update();

    // Поддержание сетевого соединения и авто-переподключение при обрывах
    networkManager.update();

    // Уступаем квант времени планировщику FreeRTOS
    delay(5);
}
