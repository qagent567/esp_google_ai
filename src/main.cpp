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
    delay(500);

    Serial.println();
    Serial.println(F("[СИСТЕМА] Запуск прошивки ESP32 Google AI Studio..."));

    // Инициализация менеджера конфигурации (NVS память)
    if (configManager.begin()) {
        Serial.println(F("[СИСТЕМА] NVS память успешно инициализирована."));
    } else {
        Serial.println(F("[СИСТЕМА] Предупреждение: Сбой инициализации NVS!"));
    }

    // Инициализация сетевого интерфейса
    networkManager.begin();

    // Если сохранены валидные данные настройки, подключаемся в штатном режиме
    if (configManager.isConfigured()) {
        Serial.println(F("[СИСТЕМА] Найдена сохраненная конфигурация. Штатный запуск..."));
        networkManager.connect();
    } else {
        Serial.println(F("[СИСТЕМА] Сохраненная конфигурация не найдена или не заполнена."));
    }

    // Инициализация командной строки (CLI) с авто-запуском мастера настройки, если нет данных
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
