#pragma once

#include <Arduino.h>
#include "ConfigManager.h"
#include "NetworkManager.h"
#include "GeminiClient.h"

#include <vector>
#include <functional>

/**
 * @brief Класс интерактивного интерфейса командной строки (Serial CLI)
 */
class SerialCLI {
public:
    enum class WizardType {
        NONE,
        FULL,
        WIFI,
        AI,
        PASSWORD_PROMPT
    };

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

    // Запуск мастера настройки (FULL, WIFI или AI)
    void startWizard(WizardType type = WizardType::FULL);

    // Запуск встроенного набора тестов
    void runSelfTest();

private:
    ConfigManager& _configMgr;
    NetworkManager& _netMgr;
    GeminiClient& _geminiClient;

    String _inputBuffer;
    WizardType _currentWizard = WizardType::NONE;
    int _wizardStep = 0;

    // --- Обработка VT100 и истории ---
    std::vector<String> _history;
    int _historyIndex = -1;
    bool _inEscapeSequence = false;
    String _escapeBuffer = "";
    int _cursorPos = 0;

    // --- Реестр команд ---
    struct Command {
        String name;
        String description;
        std::function<void(int argc, String argv[])> handler;
        String category;
    };
    std::vector<Command> _commands;

    // Регистрация всех команд
    void registerCommands();
    void addCommand(const String& name, const String& desc, const String& category, std::function<void(int argc, String argv[])> handler);

    // Выполнение введенной команды
    void handleCommand(String line);
    
    // Парсер аргументов
    int parseArgs(String line, String argv[], int maxArgs);

    // Обработка шагов мастера настройки
    void handleWizardStep(const String& input);

    // Вспомогательные функции
    void printPrompt();
    String maskString(const String& str, int keepStart = 4, int keepEnd = 4);
    
    // Цветовые макросы и утилиты
    void clearScreen();
};
