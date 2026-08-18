#pragma once

#include <Arduino.h>
#include "ConfigManager.h"
#include "NetworkManager.h"
#include "GeminiClient.h"

/**
 * @brief Класс интерактивного интерфейса командной строки (Serial CLI)
 */
class SerialCLI {
public:
    SerialCLI(ConfigManager& configMgr, NetworkManager& netMgr, GeminiClient& geminiClient);

    // Инициализация CLI
    void begin();

    // Обработка входящих символов из Serial (вызывать в loop)
    void update();

    // Вывод приветствия и статуса
    void printWelcome();

    // Вывод справочной информации по командам
    void printHelp();

    // Вывод подробного статуса устройства
    void printStatus();

    // Пошаговый мастер первой настройки
    void startWizard();

private:
    ConfigManager& _configMgr;
    NetworkManager& _netMgr;
    GeminiClient& _geminiClient;

    String _inputBuffer;
    bool _inWizardMode = false;
    int _wizardStep = 0;

    // Выполнение введенной команды
    void handleCommand(String line);

    // Обработка шагов мастера настройки
    void handleWizardStep(const String& input);

    // Вспомогательные функции
    void printPrompt();
    String maskString(const String& str, int keepStart = 4, int keepEnd = 4);
};
