#ifndef UNIT_TEST

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
#include "UsageTracker.h"
#include "HardwareController.h"
#include "CLI.h"

// Глобальные объекты модулей системы
ConfigManager configManager;
NetworkManager networkManager(configManager);
UsageTracker usageTracker;
HardwareController hardwareController;
GeminiClient geminiClient(configManager, &usageTracker, &hardwareController);
SerialCLI serialCli(configManager, networkManager, geminiClient, usageTracker, hardwareController);

void setup() {
    // Инициализация Serial интерфейса на скорости 115200 бод
    Serial.begin(115200);
    
    // Ждем до 2.5 секунд, чтобы дать пользователю возможность открыть монитор порта,
    // либо продолжаем немедленно, если монитор порта был запущен и пользователь нажал клавишу
    unsigned long startWait = millis();
    while (millis() - startWait < 2500) {
        if (Serial.available()) {
            break;
        }
        delay(10);
    }
    // Очистить буфер ввода
    while(Serial.available()) Serial.read();

    Serial.println();
    Serial.println(F("[СИСТЕМА] Запуск прошивки ESP32 Google AI Studio..."));

    // Инициализация менеджера конфигурации (NVS память)
    if (configManager.begin()) {
        Serial.println(F("[СИСТЕМА] NVS память успешно инициализирована."));
    } else {
        Serial.println(F("[СИСТЕМА] Предупреждение: Сбой инициализации NVS!"));
    }

    // Инициализация аппаратного контроллера с системным контекстом
    hardwareController.begin();
    hardwareController.setSystemContext(&configManager, &networkManager, &usageTracker);

    // Инициализация сетевого интерфейса
    networkManager.begin();

    // Инициализация интерфейса CLI (авторизация сессии или первоначальный мастер)
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

#endif // UNIT_TEST
