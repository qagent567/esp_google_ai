#include "CLI.h"
#include <esp_system.h>

#define ANSI_RESET   ""
#define ANSI_RED     ""
#define ANSI_GREEN   ""
#define ANSI_YELLOW  ""
#define ANSI_BLUE    ""
#define ANSI_CYAN    ""
#define ANSI_BOLD    ""

SerialCLI::SerialCLI(ConfigManager& configMgr, NetworkManager& netMgr, GeminiClient& geminiClient, UsageTracker& usageTracker)
    : _configMgr(configMgr), _netMgr(netMgr), _geminiClient(geminiClient), _usageTracker(usageTracker) {}

void SerialCLI::begin() {
    _inputBuffer.reserve(256);
    _usageTracker.begin();
    registerCommands();
    printWelcome();
    
    if (_configMgr.hasSavedConfig()) {
        Serial.printf("\n" ANSI_GREEN "[СИСТЕМА] Найдена сохраненная конфигурация (Сеть: '%s', API-ключ настроен)." ANSI_RESET "\n", 
                      _configMgr.getConfig().wifiSsid.c_str());
        Serial.println(ANSI_CYAN "[СИСТЕМА] Автоматическое подключение к Wi-Fi..." ANSI_RESET);
        _netMgr.connect();
        printPrompt();
    } else {
        Serial.println(ANSI_RED "\n[!] Устройство еще не настроено (нет сохраненной конфигурации)." ANSI_RESET);
        Serial.println(ANSI_RED "[!] Автоматический запуск мастера первоначальной настройки...\n" ANSI_RESET);
        startWizard(WizardType::FULL);
    }
}

void SerialCLI::printWelcome() {
    Serial.println(ANSI_CYAN "\n========================================================" ANSI_RESET);
    Serial.println(ANSI_BOLD "       ESP32 + Google AI Studio (Gemini) [Smart DNS]     " ANSI_RESET);
    Serial.println(ANSI_CYAN "========================================================" ANSI_RESET);
    Serial.println("Обход ограничений РФ через Smart DNS (xbox-dns.ru)");
    Serial.println("Введите 'help' для списка команд или 'wizard' для настройки.");
    Serial.println(ANSI_CYAN "--------------------------------------------------------" ANSI_RESET);
}

void SerialCLI::printPrompt() {
    if (_currentWizard != WizardType::NONE) {
        Serial.print(ANSI_YELLOW "[WIZARD] > " ANSI_RESET);
    } else {
        Serial.print(ANSI_GREEN ANSI_BOLD "root@esp32" ANSI_RESET ":" ANSI_BLUE "~$ " ANSI_RESET);
    }
}

String SerialCLI::maskString(const String& str, int keepStart, int keepEnd) {
    if (str.isEmpty()) return "[НЕ ЗАДАНО]";
    if (str.length() <= (unsigned int)(keepStart + keepEnd)) return "********";
    
    String masked = str.substring(0, keepStart);
    for (size_t i = 0; i < str.length() - keepStart - keepEnd; ++i) {
        masked += '*';
    }
    masked += str.substring(str.length() - keepEnd);
    return masked;
}

void SerialCLI::clearScreen() {
    Serial.print("\e[2J\e[H");
}

void SerialCLI::printHelp() {
    Serial.println(ANSI_CYAN "\n--- СПИСОК ДОСТУПНЫХ КОМАНД ---" ANSI_RESET);
    
    String currentCategory = "";
    for (const auto& cmd : _commands) {
        if (cmd.category != currentCategory) {
            currentCategory = cmd.category;
            Serial.printf(ANSI_YELLOW "\n %s:\n" ANSI_RESET, currentCategory.c_str());
        }
        Serial.printf("   %-24s - %s\n", cmd.name.c_str(), cmd.description.c_str());
    }
    
    Serial.println(ANSI_YELLOW "\n Отправка запросов в AI:" ANSI_RESET);
    Serial.println("   ask <вопрос>             - Отправить запрос явно");
    Serial.println("   <любой текст>            - Любой нераспознанный текст отправляется в Gemini");
    Serial.println(ANSI_CYAN "--------------------------------\n" ANSI_RESET);
}

void SerialCLI::printStatus() {
    const AppConfig& cfg = _configMgr.getConfig();

    Serial.println(ANSI_CYAN "\n================ ТЕКУЩИЙ СТАТУС УСТРОЙСТВА ================" ANSI_RESET);
    Serial.printf(" [СЕТЬ] Wi-Fi SSID       : %s\n", cfg.wifiSsid.isEmpty() ? ANSI_RED "[Не задан]" ANSI_RESET : cfg.wifiSsid.c_str());
    Serial.printf(" [СЕТЬ] Пароль           : %s\n", maskString(cfg.wifiPassword, 1, 1).c_str());
    Serial.printf(" [СЕТЬ] Статус           : %s\n", _netMgr.isConnected() ? ANSI_GREEN "ПОДКЛЮЧЕНО" ANSI_RESET : ANSI_RED "ОТКЛЮЧЕНО" ANSI_RESET);
    Serial.printf(" [СЕТЬ] Локальный IP     : %s\n", _netMgr.getLocalIP().c_str());
    Serial.printf(" [СЕТЬ] Уровень сигнала  : %d dBm\n", _netMgr.getRSSI());
    Serial.printf(" [DNS]  Smart DNS 1      : %s (Активный: %s)\n", cfg.dnsPrimary.c_str(), _netMgr.getPrimaryDNS().c_str());
    Serial.printf(" [DNS]  Smart DNS 2      : %s (Активный: %s)\n", cfg.dnsSecondary.c_str(), _netMgr.getSecondaryDNS().c_str());
    Serial.println(ANSI_CYAN "-----------------------------------------------------------" ANSI_RESET);
    Serial.printf(" [AI]   Модель Gemini    : " ANSI_BOLD "%s" ANSI_RESET "\n", cfg.model.c_str());
    Serial.printf(" [AI]   API-Ключ         : %s\n", maskString(cfg.apiKey, 6, 4).c_str());
    Serial.printf(" [AI]   Системный промпт : %s\n", cfg.systemPrompt.c_str());
    Serial.printf(" [AI]   Макс. токенов    : %d (температура: %.2f)\n", cfg.maxTokens, cfg.temperature);
    
    DailyUsageStats uStats = _usageTracker.getStats();
    if (uStats.dailyRequestLimit > 0) {
        int rem = (int)uStats.dailyRequestLimit - (int)uStats.requestsToday;
        if (rem < 0) rem = 0;
        Serial.printf(" [AI]   Суточный расход  : %u / %u запросов (Осталось: %d) | Токенов: %u\n", 
                      uStats.requestsToday, uStats.dailyRequestLimit, rem, uStats.totalTokensToday);
    } else {
        Serial.printf(" [AI]   Суточный расход  : Безлимитно (израсходовано: %u запросов) | Токенов: %u\n", 
                      uStats.requestsToday, uStats.totalTokensToday);
    }
    Serial.printf(" [AI]   Сброс суток      : %s (текущее время: %s)\n", 
                  _usageTracker.getTimeUntilMidnight().c_str(), _usageTracker.getCurrentTimeString().c_str());

    Serial.println(ANSI_CYAN "-----------------------------------------------------------" ANSI_RESET);
    Serial.printf(" [СИСТЕМА] Свободно RAM  : %u байт\n", ESP.getFreeHeap());
    Serial.printf(" [СИСТЕМА] Мин. своб. RAM: %u байт\n", ESP.getMinFreeHeap());
    Serial.printf(" [СИСТЕМА] Аптайм        : %lu сек\n", millis() / 1000);
    Serial.println(ANSI_CYAN "-----------------------------------------------------------" ANSI_RESET);
    Serial.println(ANSI_YELLOW " Команды быстрой настройки:" ANSI_RESET);
    Serial.println("  • quota / usage        - подробный отчет о суточных лимитах и расходе");
    Serial.println("  • model [№|id]         - переключить активную модель нейросети");
    Serial.println("  • models               - список всех доступных моделей с номерами и лимитами");
    Serial.println("  • set limit <число>    - установить суточный лимит запросов (0 = безлимит)");
    Serial.println("  • set key <ключ>       - установить API-ключ Gemini");
    Serial.println("  • set prompt <текст>   - установить системную роль/промпт");
    Serial.println("  • set temp <0.0-2.0>   - температура генерации (креативность)");
    Serial.println("  • set tokens <число>   - лимит токенов ответа");
    Serial.println("  • set dns <ip1> [ip2]  - задать Smart DNS серверы");
    Serial.println("  • setup <wifi|ai|model>- интерактивный мастер настройки");
    Serial.println(ANSI_CYAN "===========================================================\n" ANSI_RESET);
}

void SerialCLI::startWizard(WizardType type) {
    _currentWizard = type;
    _wizardStep = 1;
    _netMgr.setAutoReconnect(false);
    Serial.println(ANSI_CYAN "\n========================================================" ANSI_RESET);
    
    if (type == WizardType::FULL) {
        Serial.println(ANSI_BOLD "           МАСТЕР ПОЛНОЙ НАСТРОЙКИ             " ANSI_RESET);
        Serial.println(ANSI_CYAN "========================================================" ANSI_RESET);
        Serial.println("Для отмены введите 'cancel' в любой момент.");
        Serial.println("ШАГ 1/3: Введите SSID (имя) вашей Wi-Fi сети:");
        if (!_configMgr.getConfig().wifiSsid.isEmpty()) Serial.printf("(Текущая: %s, нажмите Enter чтобы оставить)\n", _configMgr.getConfig().wifiSsid.c_str());
    } else if (type == WizardType::WIFI) {
        Serial.println(ANSI_BOLD "           НАСТРОЙКА WI-FI             " ANSI_RESET);
        Serial.println(ANSI_CYAN "========================================================" ANSI_RESET);
        Serial.println("Для отмены введите 'cancel' в любой момент.");
        Serial.println("ШАГ 1/2: Введите SSID (имя) вашей Wi-Fi сети:");
        if (!_configMgr.getConfig().wifiSsid.isEmpty()) Serial.printf("(Текущая: %s, нажмите Enter чтобы оставить)\n", _configMgr.getConfig().wifiSsid.c_str());
    } else if (type == WizardType::AI) {
        Serial.println(ANSI_BOLD "           НАСТРОЙКА GOOGLE AI (GEMINI)             " ANSI_RESET);
        Serial.println(ANSI_CYAN "========================================================" ANSI_RESET);
        Serial.println("Для отмены введите 'cancel' в любой момент.");
        Serial.println("ШАГ 1/2: Введите API-ключ Google AI Studio (Gemini API Key):");
        if (!_configMgr.getConfig().apiKey.isEmpty()) Serial.println("(Ключ уже сохранен. Нажмите Enter, чтобы оставить текущий)");
        Serial.println("(Получить ключ бесплатно можно на https://aistudio.google.com/app/apikey)");
    } else if (type == WizardType::MODEL) {
        Serial.println(ANSI_BOLD "           ВЫБОР МОДЕЛИ GEMINI             " ANSI_RESET);
        Serial.println(ANSI_CYAN "========================================================" ANSI_RESET);
        Serial.printf("Текущая активная модель: %s\n", _configMgr.getConfig().model.c_str());
        Serial.println("Введите ID модели (например: gemini-3.5-flash-lite) или номер из команды 'models':");
    } else if (type == WizardType::PASSWORD_PROMPT) {
        Serial.println(ANSI_BOLD "           АВТОРИЗАЦИЯ СЕССИИ WI-FI             " ANSI_RESET);
        Serial.println(ANSI_CYAN "========================================================" ANSI_RESET);
        Serial.printf("Сеть: '%s'\n", _configMgr.getConfig().wifiSsid.c_str());
        Serial.println("Введите пароль от Wi-Fi (или 'e' для отмены):");
    }
}

void SerialCLI::handleWizardStep(const String& input) {
    if (input.equalsIgnoreCase("cancel") || (_currentWizard == WizardType::PASSWORD_PROMPT && input.equalsIgnoreCase("e"))) {
        _currentWizard = WizardType::NONE;
        _wizardStep = 0;
        Serial.println(ANSI_YELLOW "[WIZARD] Действие отменено пользователем." ANSI_RESET);
        return;
    }

    if (_currentWizard == WizardType::PASSWORD_PROMPT) {
        _configMgr.getConfig().wifiPassword = input;
        _currentWizard = WizardType::NONE;
        _wizardStep = 0;
        Serial.println("[Wi-Fi] Попытка подключения к сети...");
        if (_netMgr.connect()) {
            Serial.println(ANSI_GREEN "[СИСТЕМА] Подключение успешно! Сессия активна до перезагрузки." ANSI_RESET);
        } else {
            Serial.println(ANSI_YELLOW "[ВНИМАНИЕ] Подключение не удалось. Введите 'connect' или 'setup wifi'." ANSI_RESET);
        }
        return;
    }

    if (_currentWizard == WizardType::FULL) {
        if (_wizardStep == 1) {
            if (input.isEmpty()) { 
                if (_configMgr.getConfig().wifiSsid.isEmpty()) { Serial.println(ANSI_RED "[WIZARD] SSID не может быть пустым. Введите SSID сети:" ANSI_RESET); return; }
            } else {
                _configMgr.setWifi(input, _configMgr.getConfig().wifiPassword);
            }
            _wizardStep = 2;
            Serial.println("ШАГ 2/3: Введите пароль от Wi-Fi сети (или нажмите Enter, если сеть открытая):");
        } else if (_wizardStep == 2) {
            _configMgr.setWifi(_configMgr.getConfig().wifiSsid, input);
            _wizardStep = 3;
            Serial.println("ШАГ 3/3: Введите API-ключ Google AI Studio (Gemini API Key):");
            if (!_configMgr.getConfig().apiKey.isEmpty()) Serial.println("(Ключ уже сохранен. Нажмите Enter, чтобы оставить текущий)");
        } else if (_wizardStep == 3) {
            if (input.isEmpty()) { 
                if (_configMgr.getConfig().apiKey.isEmpty()) { Serial.println(ANSI_RED "[WIZARD] API-ключ не может быть пустым. Введите API-ключ:" ANSI_RESET); return; }
            } else {
                _configMgr.setApiKey(input);
            }
            _currentWizard = WizardType::NONE;
            _wizardStep = 0;
            Serial.println("\n[WIZARD] Проверка подключения к Wi-Fi перед сохранением...");
            if (_netMgr.connect()) {
                _configMgr.save();
                Serial.println(ANSI_GREEN "[УСПЕХ] Конфигурация и Wi-Fi успешно сохранены в NVS Flash!" ANSI_RESET);
            } else {
                Serial.println(ANSI_YELLOW "[ВНИМАНИЕ] Не удалось подключиться к сети. Параметры НЕ сохранены во Flash." ANSI_RESET);
            }
        }
    } else if (_currentWizard == WizardType::WIFI) {
        if (_wizardStep == 1) {
            if (input.isEmpty()) { 
                if (_configMgr.getConfig().wifiSsid.isEmpty()) { Serial.println(ANSI_RED "[WIZARD] SSID не может быть пустым. Введите SSID сети:" ANSI_RESET); return; }
            } else {
                _configMgr.setWifi(input, _configMgr.getConfig().wifiPassword);
            }
            _wizardStep = 2;
            Serial.println("ШАГ 2/2: Введите пароль от Wi-Fi сети (или нажмите Enter, если сеть открытая):");
        } else if (_wizardStep == 2) {
            _configMgr.setWifi(_configMgr.getConfig().wifiSsid, input);
            _currentWizard = WizardType::NONE;
            _wizardStep = 0;
            Serial.println("\n[WIZARD] Проверка подключения к Wi-Fi перед сохранением...");
            if (_netMgr.connect()) {
                _configMgr.save();
                Serial.println(ANSI_GREEN "[УСПЕХ] Wi-Fi сеть успешно подключена и сохранена!" ANSI_RESET);
            } else {
                Serial.println(ANSI_YELLOW "[ВНИМАНИЕ] Не удалось подключиться к сети. Параметры НЕ сохранены во Flash." ANSI_RESET);
            }
        }
    } else if (_currentWizard == WizardType::AI) {
        if (_wizardStep == 1) {
            if (input.isEmpty()) { 
                if (_configMgr.getConfig().apiKey.isEmpty()) { Serial.println(ANSI_RED "[WIZARD] API-ключ не может быть пустым. Введите API-ключ:" ANSI_RESET); return; }
            } else {
                _configMgr.setApiKey(input);
            }
            _wizardStep = 2;
            Serial.println("ШАГ 2/2: Введите системный промпт (или нажмите Enter для текущего):");
        } else if (_wizardStep == 2) {
            if (!input.isEmpty()) { _configMgr.setSystemPrompt(input); }
            _configMgr.save();
            _currentWizard = WizardType::NONE;
            _wizardStep = 0;
            Serial.println(ANSI_GREEN "\n[WIZARD] Настройки AI успешно сохранены во Flash!" ANSI_RESET);
        }
    } else if (_currentWizard == WizardType::MODEL) {
        String targetModel = input;
        int num = input.toInt();
        if (num > 0) {
            String fromCache = _geminiClient.getModelByIndex(num);
            if (!fromCache.isEmpty()) {
                targetModel = fromCache;
            }
        }
        if (targetModel.isEmpty()) {
            Serial.println(ANSI_RED "[WIZARD] Имя модели не может быть пустым." ANSI_RESET);
            return;
        }
        _configMgr.setModel(targetModel);
        _configMgr.save();
        _currentWizard = WizardType::NONE;
        _wizardStep = 0;
        Serial.printf(ANSI_GREEN "\n[УСПЕХ] Активная модель Gemini установлена: '%s' и сохранена во Flash!\n" ANSI_RESET, targetModel.c_str());
    }
}

int SerialCLI::parseArgs(String line, String argv[], int maxArgs) {
    int argc = 0; bool inQuotes = false; String currentArg = "";
    for (size_t i = 0; i < line.length(); ++i) {
        char c = line.charAt(i);
        if (c == '"') { inQuotes = !inQuotes; }
        else if (c == ' ' && !inQuotes) {
            if (currentArg.length() > 0) { if (argc < maxArgs) argv[argc++] = currentArg; currentArg = ""; }
        } else { currentArg += c; }
    }
    if (currentArg.length() > 0 && argc < maxArgs) { argv[argc++] = currentArg; }
    return argc;
}

void SerialCLI::addCommand(const String& name, const String& desc, const String& category, std::function<void(int argc, String argv[])> handler) {
    _commands.push_back({name, desc, handler, category});
}

void SerialCLI::registerCommands() {
    addCommand("help", "Показать справку по командам", "Система", [this](int argc, String argv[]) { printHelp(); });
    addCommand("?", "Алиас для help", "Система", [this](int argc, String argv[]) { printHelp(); });
    addCommand("clear", "Очистить экран терминала", "Система", [this](int argc, String argv[]) { clearScreen(); });
    addCommand("echo", "Вывести текст на экран", "Система", [this](int argc, String argv[]) {
        for (int i = 1; i < argc; ++i) { Serial.print(argv[i]); if (i < argc - 1) Serial.print(" "); }
        Serial.println();
    });
    addCommand("history", "Показать историю введенных команд", "Система", [this](int argc, String argv[]) {
        for (size_t i = 0; i < _history.size(); ++i) { Serial.printf("%d: %s\n", i + 1, _history[i].c_str()); }
    });
    addCommand("status", "Показать статус устройства", "Система", [this](int argc, String argv[]) { printStatus(); });
    addCommand("ifconfig", "Показать информацию о сети (IP, DNS)", "Система", [this](int argc, String argv[]) {
        Serial.printf("IP: %s\nMAC: %s\nDNS1: %s\nDNS2: %s\n", 
            _netMgr.getLocalIP().c_str(), WiFi.macAddress().c_str(), 
            _netMgr.getPrimaryDNS().c_str(), _netMgr.getSecondaryDNS().c_str());
    });
    addCommand("reboot", "Перезагрузить устройство", "Система", [this](int argc, String argv[]) {
        Serial.println(ANSI_YELLOW "[СИСТЕМА] Перезагрузка ESP32..." ANSI_RESET); delay(500); ESP.restart();
    });
    addCommand("save", "Сохранить параметры в NVS", "Система", [this](int argc, String argv[]) {
        if (_netMgr.isConnected()) { _configMgr.save(); Serial.println(ANSI_GREEN "[ОК] Конфигурация сохранена." ANSI_RESET); }
        else { Serial.println(ANSI_YELLOW "[ВНИМАНИЕ] Сохранение разрешено только при активном подключении к сети!" ANSI_RESET); }
    });
    addCommand("reset", "Сбросить параметры (reset, reset quota, reset allquota)", "Система", [this](int argc, String argv[]) {
        if (argc >= 2 && (argv[1] == "quota" || argv[1] == "usage")) {
            _usageTracker.resetDailyUsage();
            Serial.println(ANSI_GREEN "[СБРОС] Суточные счетчики запросов и токенов сброшены." ANSI_RESET);
        } else if (argc >= 2 && argv[1] == "allquota") {
            _usageTracker.resetAllUsage();
            Serial.println(ANSI_GREEN "[СБРОС] Вся статистика использования (включая общую) сброшена." ANSI_RESET);
        } else {
            _configMgr.resetToDefaults(); _configMgr.save(); Serial.println(ANSI_GREEN "[СБРОС] Настройки сброшены к значениям по умолчанию." ANSI_RESET);
        }
    });
    addCommand("wizard", "Запустить мастер полной настройки (Wi-Fi + AI)", "Настройки", [this](int argc, String argv[]) { startWizard(WizardType::FULL); });
    addCommand("setup", "Интерактивная настройка (setup wifi|ai|model)", "Настройки", [this](int argc, String argv[]) {
        if (argc < 2) { 
            Serial.println(ANSI_YELLOW "Использование: setup <wifi|ai|model>" ANSI_RESET); 
            Serial.println("  • setup wifi   - пошаговая настройка сети Wi-Fi");
            Serial.println("  • setup ai     - пошаговая настройка API-ключа и промпта");
            Serial.println("  • setup model  - пошаговый выбор модели нейросети");
            return; 
        }
        if (argv[1] == "wifi") startWizard(WizardType::WIFI);
        else if (argv[1] == "ai") startWizard(WizardType::AI);
        else if (argv[1] == "model") startWizard(WizardType::MODEL);
        else Serial.println("Неизвестный параметр. Доступно: wifi, ai, model");
    });
    addCommand("scan", "Сканировать сети Wi-Fi", "Wi-Fi", [this](int argc, String argv[]) { _netMgr.scanNetworks(); });
    addCommand("connect", "Подключиться к Wi-Fi", "Wi-Fi", [this](int argc, String argv[]) { 
        if (_netMgr.connect()) {
            _configMgr.save();
        }
    });
    addCommand("disconnect", "Отключиться от Wi-Fi", "Wi-Fi", [this](int argc, String argv[]) { _netMgr.disconnect(); });
    
    // Вспомогательный обработчик установки модели (по имени или по номеру)
    auto applyModel = [this](const String& val) {
        String targetModel = val;
        int num = val.toInt();
        if (num > 0) {
            if (_geminiClient.getModelCount() == 0) {
                _geminiClient.listAvailableModels();
            }
            String fromCache = _geminiClient.getModelByIndex(num);
            if (!fromCache.isEmpty()) {
                targetModel = fromCache;
            } else {
                Serial.printf(ANSI_RED "[ОШИБКА] Неверный номер модели (%d). Посмотрите доступные: 'models'\n" ANSI_RESET, num);
                return;
            }
        }
        _configMgr.setModel(targetModel);
        _configMgr.save();
        Serial.printf(ANSI_GREEN "[ОК] Активная модель Gemini установлена: '%s'\n" ANSI_RESET, targetModel.c_str());
    };

    addCommand("config", "Показать текущую конфигурацию и подсказки по настройке", "Конфигурация", [this](int argc, String argv[]) { printStatus(); });
    addCommand("settings", "Алиас для команды config", "Конфигурация", [this](int argc, String argv[]) { printStatus(); });
    addCommand("quota", "Суточные лимиты, расход запросов и токенов", "Google AI", [this](int argc, String argv[]) { _usageTracker.printQuotaReport(); });
    addCommand("usage", "Алиас для команды quota", "Google AI", [this](int argc, String argv[]) { _usageTracker.printQuotaReport(); });
    addCommand("limits", "Алиас для команды quota", "Google AI", [this](int argc, String argv[]) { _usageTracker.printQuotaReport(); });
    addCommand("set", "Изменить параметр (set limit, model, key, prompt, temp, tokens, dns, ssid, pass)", "Конфигурация", [this, applyModel](int argc, String argv[]) {
        if (argc < 3) { 
            Serial.println(ANSI_YELLOW "Использование: set <параметр> <значение>" ANSI_RESET); 
            Serial.println("  • set limit <число>     - установить суточный лимит запросов (0 = безлимитно)");
            Serial.println("  • set model <id|№>      - переключить модель (напр: set model 24 или set model gemini-3.5-flash-lite)");
            Serial.println("  • set key <api-key>     - установить API-ключ Gemini");
            Serial.println("  • set ssid <имя_сети>   - установить имя Wi-Fi");
            Serial.println("  • set pass <пароль>     - установить пароль Wi-Fi");
            Serial.println("  • set prompt <текст>    - задать системный промпт");
            Serial.println("  • set temp <0.0-2.0>    - задать температуру ответа");
            Serial.println("  • set tokens <число>    - задать максимальное число токенов ответа");
            Serial.println("  • set dns <ip1> [ip2]   - задать Smart DNS серверы");
            return; 
        }
        String param = argv[1]; String val = argv[2];
        if (param == "limit" || param == "quota") { 
            _usageTracker.setDailyLimit(val.toInt()); 
            Serial.printf("[ОК] Суточный лимит запросов установлен: %u (0 = безлимитно)\n", (unsigned int)val.toInt()); 
        }
        else if (param == "ssid") { _configMgr.setWifi(val, _configMgr.getConfig().wifiPassword); Serial.printf("[ОК] Wi-Fi SSID установлен: '%s'\n", _configMgr.getConfig().wifiSsid.c_str()); }
        else if (param == "pass") { _configMgr.setWifi(_configMgr.getConfig().wifiSsid, val); Serial.println("[ОК] Пароль Wi-Fi установлен."); }
        else if (param == "key") { _configMgr.setApiKey(val); _configMgr.save(); Serial.println("[ОК] API-ключ сохранен."); }
        else if (param == "model") { applyModel(val); }
        else if (param == "prompt") { _configMgr.setSystemPrompt(val); _configMgr.save(); Serial.println("[ОК] Промпт обновлен."); }
        else if (param == "tokens") { _configMgr.setMaxTokens(val.toInt()); _configMgr.save(); Serial.println("[ОК] Лимит токенов изменен."); }
        else if (param == "temp") { _configMgr.setTemperature(val.toFloat()); _configMgr.save(); Serial.println("[ОК] Температура изменена."); }
        else if (param == "dns") {
            String dns1 = argv[2]; String dns2 = (argc > 3) ? argv[3] : "";
            _configMgr.setDns(dns1, dns2); _configMgr.save(); _netMgr.applyCustomDNS();
            Serial.printf("[ОК] DNS обновлен: %s, %s\n", dns1.c_str(), dns2.c_str());
        } else { Serial.println(ANSI_RED "Неизвестный параметр. Введите 'set' без параметров для справки." ANSI_RESET); }
    });
    addCommand("model", "Показать или переключить активную модель (model [№|id])", "Google AI", [this, applyModel](int argc, String argv[]) { 
        if (argc < 2) {
            Serial.printf(ANSI_CYAN "\n[AI] Текущая активная модель: " ANSI_BOLD "%s" ANSI_RESET "\n", _configMgr.getConfig().model.c_str());
            Serial.println("  • 'models'           - показать таблицу всех доступных моделей с номерами и лимитами");
            Serial.println("  • 'model <№>'        - переключить модель по номеру (например: 'model 24')");
            Serial.println("  • 'model <id>'       - переключить по имени (например: 'model gemini-3.5-flash-lite')");
            Serial.println("  • 'setup model'      - интерактивный выбор модели\n");
            return;
        }
        applyModel(argv[1]);
    });
    addCommand("models", "Таблица всех доступных моделей с номерами и лимитами", "Google AI", [this](int argc, String argv[]) { _geminiClient.listAvailableModels(); });
    addCommand("test", "Тест подключения к Gemini", "Google AI", [this](int argc, String argv[]) { _geminiClient.testConnection(); });
    addCommand("demo", "Демонстрация интеграции", "Google AI", [this](int argc, String argv[]) {
        if (argc >= 2 && argv[1] == "automation") {
            Serial.println(ANSI_CYAN "[DEMO] Демонстрация работы AI в программном коде ESP32..." ANSI_RESET);
            String prompt = "Температура в серверной сейчас 35.5 градусов Цельсия. Норма до 25. Что мне сделать? Ответь очень кратко, словно ты аварийная система ESP32.";
            Serial.println(prompt);
            GeminiResponse res = _geminiClient.ask(prompt);
            if (res.success) { Serial.println(ANSI_GREEN "\n[РЕАКЦИЯ СИСТЕМЫ]:" ANSI_RESET); Serial.println(res.text); }
            else { Serial.printf(ANSI_RED "[ОШИБКА] Не удалось получить ответ: %s" ANSI_RESET "\n", res.text.c_str()); }
        }
    });
    addCommand("ask", "Отправить вопрос в Gemini", "Google AI", [this](int argc, String argv[]) {
        if (argc < 2) { Serial.println("Использование: ask \"ваш вопрос\""); return; }
        String prompt = ""; for (int i = 1; i < argc; ++i) prompt += argv[i] + (i < argc - 1 ? " " : "");
        if (!_netMgr.isConnected()) { Serial.println(ANSI_RED "[ОШИБКА] Нет подключения к Wi-Fi!" ANSI_RESET); return; }
        if (_configMgr.getConfig().apiKey.isEmpty()) { Serial.println(ANSI_RED "[ОШИБКА] Не задан API-ключ Gemini!" ANSI_RESET); return; }
        
        if (_usageTracker.isLimitReached()) {
            DailyUsageStats st = _usageTracker.getStats();
            Serial.printf(ANSI_RED "\n[ОШИБКА] Суточный лимит запросов (%u) исчерпан!\n" ANSI_RESET, st.dailyRequestLimit);
            Serial.printf("Сброс лимита через: %s (в 00:00 по времени устройства: %s).\n", 
                          _usageTracker.getTimeUntilMidnight().c_str(), _usageTracker.getCurrentTimeString().c_str());
            Serial.println("Для изменения лимита введите: 'set limit <число>' или 'set limit 0' (безлимит).\n");
            return;
        }

        Serial.printf(ANSI_CYAN "\n[Gemini AI] Отправка запроса к модели %s...\n" ANSI_RESET, _configMgr.getConfig().model.c_str());
        GeminiResponse res = _geminiClient.ask(prompt);
        if (res.success) { 
            _usageTracker.recordRequest(res.promptTokens, res.candidateTokens, res.totalTokens);
            DailyUsageStats st = _usageTracker.getStats();
            int remaining = (st.dailyRequestLimit > 0) ? ((int)st.dailyRequestLimit - (int)st.requestsToday) : -1;
            if (remaining < 0 && st.dailyRequestLimit > 0) remaining = 0;

            Serial.println(res.text); 
            if (st.dailyRequestLimit > 0) {
                Serial.printf(ANSI_YELLOW "\n[Статистика] Время: %lu мс | Токены: %d (Запрос: %d, Ответ: %d) | Запросов сегодня: %u/%u (Осталось: %d)\n\n" ANSI_RESET, 
                              res.durationMs, res.totalTokens, res.promptTokens, res.candidateTokens, st.requestsToday, st.dailyRequestLimit, remaining); 
            } else {
                Serial.printf(ANSI_YELLOW "\n[Статистика] Время: %lu мс | Токены: %d (Запрос: %d, Ответ: %d) | Запросов сегодня: %u\n\n" ANSI_RESET, 
                              res.durationMs, res.totalTokens, res.promptTokens, res.candidateTokens, st.requestsToday); 
            }
        } else { 
            Serial.printf(ANSI_RED "[ОШИБКА] %s (HTTP: %d)\n\n" ANSI_RESET, res.text.c_str(), res.httpCode); 
        }
    });
    addCommand("selftest", "Запустить автоматический набор тестов прошивки", "Система", [this](int argc, String argv[]) { runSelfTest(); });
}

void SerialCLI::handleCommand(String line) {
    line.trim();
    if (line.isEmpty()) return;

    if (_currentWizard != WizardType::NONE) { handleWizardStep(line); return; }

    if (_history.empty() || _history.back() != line) {
        _history.push_back(line);
        if (_history.size() > 15) _history.erase(_history.begin());
    }
    _historyIndex = _history.size();

    String argv[20];
    int argc = parseArgs(line, argv, 20);
    if (argc == 0) return;

    for (const auto& cmd : _commands) {
        if (cmd.name.equalsIgnoreCase(argv[0])) { cmd.handler(argc, argv); return; }
    }

    if (_netMgr.isConnected() && !_configMgr.getConfig().apiKey.isEmpty()) {
        String askArgv[2] = {"ask", line};
        for (const auto& cmd : _commands) {
            if (cmd.name == "ask") { cmd.handler(2, askArgv); return; }
        }
    } else {
        Serial.printf(ANSI_RED "Команда '%s' не найдена. Введите 'help' для списка команд.\n" ANSI_RESET, argv[0].c_str());
    }
}

void SerialCLI::update() {
    while (Serial.available()) {
        char c = (char)Serial.read();

        if (_inEscapeSequence) {
            _escapeBuffer += c;
            if (c >= 'A' && c <= 'D') {
                _inEscapeSequence = false;
                if (c == 'A') { // UP
                    if (!_history.empty() && _historyIndex > 0) {
                        _historyIndex--;
                        Serial.print("\r"); for(int i=0; i<80; i++) Serial.print(" "); Serial.print("\r");
                        printPrompt();
                        _inputBuffer = _history[_historyIndex];
                        _cursorPos = _inputBuffer.length();
                        Serial.print(_inputBuffer);
                    }
                } else if (c == 'B') { // DOWN
                    if (!_history.empty() && _historyIndex < (int)_history.size() - 1) {
                        _historyIndex++;
                        Serial.print("\r"); for(int i=0; i<80; i++) Serial.print(" "); Serial.print("\r");
                        printPrompt();
                        _inputBuffer = _history[_historyIndex];
                        _cursorPos = _inputBuffer.length();
                        Serial.print(_inputBuffer);
                    } else if (_historyIndex == (int)_history.size() - 1) {
                        _historyIndex++;
                        Serial.print("\r"); for(int i=0; i<80; i++) Serial.print(" "); Serial.print("\r");
                        printPrompt();
                        _inputBuffer = "";
                        _cursorPos = 0;
                    }
                } else if (c == 'C') { // RIGHT
                    if (_cursorPos < _inputBuffer.length()) {
                        Serial.print(_inputBuffer.charAt(_cursorPos));
                        _cursorPos++;
                    }
                } else if (c == 'D') { // LEFT
                    if (_cursorPos > 0) {
                        Serial.print("\b");
                        _cursorPos--;
                    }
                }
            } else if (c == '~' || _escapeBuffer.length() > 5) { _inEscapeSequence = false; }
            continue;
        }

        if (c == 27) { _inEscapeSequence = true; _escapeBuffer = ""; continue; }

        if (c == '\r' || c == '\n') {
            if (_inputBuffer.length() > 0) {
                Serial.println(); handleCommand(_inputBuffer); _inputBuffer = ""; _cursorPos = 0; printPrompt();
            } else if (_currentWizard != WizardType::NONE) {
                Serial.println(); handleCommand(""); printPrompt();
            } else {
                Serial.println(); printPrompt();
            }
        } else if (c == '\b' || c == 0x7F) {
            if (_cursorPos > 0) {
                _inputBuffer.remove(_cursorPos - 1, 1);
                _cursorPos--;
                Serial.print("\b");
                Serial.print(_inputBuffer.substring(_cursorPos));
                Serial.print(" ");
                for (int i = 0; i < _inputBuffer.length() - _cursorPos + 1; i++) {
                    Serial.print("\b");
                }
            }
        } else {
            if (_inputBuffer.length() < 512) {
                _inputBuffer = _inputBuffer.substring(0, _cursorPos) + c + _inputBuffer.substring(_cursorPos);
                Serial.print(c);
                Serial.print(_inputBuffer.substring(_cursorPos + 1));
                for (int i = 0; i < _inputBuffer.length() - _cursorPos - 1; i++) {
                    Serial.print("\b");
                }
                _cursorPos++;
            }
        }
    }
}

void SerialCLI::runSelfTest() {
    Serial.println(F("\n========================================================"));
    Serial.println(F("         ЗАПУСК ВСТРОЕННЫХ ТЕСТОВ (SELF-TEST)          "));
    Serial.println(F("========================================================"));
    
    int passed = 0;
    int failed = 0;

    auto assertTest = [&](const char* name, bool condition) {
        if (condition) {
            Serial.printf(ANSI_GREEN " [PASS] %s\n" ANSI_RESET, name);
            passed++;
        } else {
            Serial.printf(ANSI_RED " [FAIL] %s\n" ANSI_RESET, name);
            failed++;
        }
    };

    // 1. Тесты ConfigManager и санитизации
    Serial.println(ANSI_CYAN "\n--- 1. Тесты менеджера конфигурации и санитизации ---" ANSI_RESET);
    {
        ConfigManager testCfg;
        assertTest("Значения по умолчанию (gemini-3.5-flash-lite, DNS)", 
                   testCfg.getConfig().model == "gemini-3.5-flash-lite" && 
                   testCfg.getConfig().dnsPrimary == "111.88.96.50");

        testCfg.setWifi("  0266\r\n\t ", " 18888888\r\n ");
        assertTest("Санитизация Wi-Fi SSID и пароля", 
                   testCfg.getConfig().wifiSsid == "0266" && 
                   testCfg.getConfig().wifiPassword == "18888888");

        testCfg.setApiKey(" \r\n AQ.Ab8RN6LtR0RySZwOW5SYz3eyFtZaCebV6Ta-Thsq5OVhiPAg1Q \n ");
        assertTest("Санитизация API-ключа (пробелы, переносы строк)", 
                   testCfg.getConfig().apiKey == "AQ.Ab8RN6LtR0RySZwOW5SYz3eyFtZaCebV6Ta-Thsq5OVhiPAg1Q" && 
                   testCfg.hasSavedApiKey());

        testCfg.setApiKey("\"AIzaSyTest123\"");
        assertTest("Удаление кавычек из API-ключа", 
                   testCfg.getConfig().apiKey == "AIzaSyTest123");
    }

    // 2. Тесты парсинга CLI
    Serial.println(ANSI_CYAN "\n--- 2. Тесты CLI парсера аргументов ---" ANSI_RESET);
    {
        String argv[10];
        int argc = parseArgs("set ssid mywifi", argv, 10);
        assertTest("Парсинг простых аргументов", argc == 3 && argv[0] == "set" && argv[1] == "ssid" && argv[2] == "mywifi");

        argc = parseArgs("ask \"привет мир\"", argv, 10);
        assertTest("Парсинг аргументов в кавычках", argc == 2 && argv[0] == "ask" && argv[1] == "привет мир");

        argc = parseArgs("", argv, 10);
        assertTest("Парсинг пустой строки", argc == 0);
    }

    // 3. Тесты GeminiClient (JSON и структура запросов)
    Serial.println(ANSI_CYAN "\n--- 3. Тесты генератора запросов и парсера Gemini ---" ANSI_RESET);
    {
        ConfigManager testCfg;
        GeminiClient testGem(testCfg);

        testCfg.setModel("gemini-2.0-flash");
        testCfg.setApiKey("TEST_KEY_999");
        String url = testGem.buildApiUrl();
        assertTest("Формирование URL API", url.indexOf("gemini-2.0-flash:generateContent?key=TEST_KEY_999") >= 0);

        testCfg.setSystemPrompt("Роль ассистента");
        testCfg.setMaxTokens(512);
        testCfg.setTemperature(0.5f);
        String body = testGem.buildRequestBody("Тестовый вопрос");
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        assertTest("Генерация валидного JSON тела запроса", !err && doc["contents"][0]["parts"][0]["text"] == "Тестовый вопрос");
        assertTest("Наличие системного промпта в теле JSON", doc["systemInstruction"]["parts"][0]["text"] == "Роль ассистента");

        // Тест парсинга успешного ответа
        String mockSuccess = "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Ответ от нейросети\"}],\"role\":\"model\"},\"finishReason\":\"STOP\"}],\"usageMetadata\":{\"totalTokenCount\":25}}";
        GeminiResponse resp;
        bool ok = testGem.parseResponse(mockSuccess, resp);
        assertTest("Парсинг успешного ответа Google API", ok && resp.success && resp.text == "Ответ от нейросети" && resp.totalTokens == 25);

        // Тест парсинга ошибки API
        String mockErr = "{\"error\":{\"code\":400,\"message\":\"API key not valid.\",\"status\":\"INVALID_ARGUMENT\"}}";
        ok = testGem.parseResponse(mockErr, resp);
        assertTest("Парсинг ошибки Google API (INVALID_ARGUMENT)", !ok && !resp.success && resp.text.indexOf("API key not valid") >= 0);
    }

    // 4. Тесты UsageTracker (квоты и суточные лимиты)
    Serial.println(ANSI_CYAN "\n--- 4. Тесты менеджера суточных лимитов и квот (UsageTracker) ---" ANSI_RESET);
    {
        UsageTracker tracker;
        tracker.setDailyLimit(10);
        tracker.resetDailyUsage();
        DailyUsageStats s = tracker.getStats();
        assertTest("Сброс суточных счетчиков", s.requestsToday == 0 && s.totalTokensToday == 0);
        assertTest("Установка суточного лимита", s.dailyRequestLimit == 10);
        assertTest("Проверка лимита при нулевом расходе (isLimitReached == false)", !tracker.isLimitReached());

        tracker.recordRequest(15, 35, 50);
        s = tracker.getStats();
        assertTest("Регистрация расхода токенов и запроса", s.requestsToday == 1 && s.promptTokensToday == 15 && s.responseTokensToday == 35 && s.totalTokensToday == 50);

        tracker.setDailyLimit(1);
        assertTest("Срабатывание суточного лимита (isLimitReached == true)", tracker.isLimitReached());

        tracker.setDailyLimit(1500);
        assertTest("Восстановление лимита (1500 RPD)", tracker.getStats().dailyRequestLimit == 1500 && !tracker.isLimitReached());
    }

    // 5. Интеграционные тесты (сеть и реальный API)
    Serial.println(ANSI_CYAN "\n--- 5. Интеграционная проверка (Hardware & Live API) ---" ANSI_RESET);
    {
        bool wifiOk = _netMgr.isConnected();
        assertTest("Статус подключения к Wi-Fi сети", wifiOk);
        if (wifiOk) {
            Serial.printf("       IP: %s, RSSI: %d dBm, DNS: %s\n", 
                          _netMgr.getLocalIP().c_str(), _netMgr.getRSSI(), _netMgr.getPrimaryDNS().c_str());
        }

        bool keyOk = _configMgr.hasSavedApiKey();
        assertTest("Наличие настроенного API-ключа в NVS", keyOk);

        if (wifiOk && keyOk) {
            Serial.println(ANSI_CYAN " [AI]  Выполнение реального тестового запроса к Gemini..." ANSI_RESET);
            GeminiResponse liveResp = _geminiClient.ask("Ответь строго одним словом: 'РАБОТАЕТ'");
            if (liveResp.success) {
                Serial.printf(ANSI_GREEN " [PASS] Реальный запрос к Gemini успешен! Ответ: %s (Время: %lu мс, Токенов: %d)\n" ANSI_RESET, 
                              liveResp.text.c_str(), liveResp.durationMs, liveResp.totalTokens);
                passed++;
            } else {
                Serial.printf(ANSI_RED " [FAIL] Ошибка живого запроса (HTTP %d): %s\n" ANSI_RESET, liveResp.httpCode, liveResp.text.c_str());
                failed++;
            }
        } else {
            Serial.println(ANSI_YELLOW " [SKIP] Пропуск живого запроса к API (требуется подключение к Wi-Fi и API-ключ)." ANSI_RESET);
        }
    }

    Serial.println(F("\n========================================================"));
    Serial.printf("ИТОГ ТЕСТИРОВАНИЯ: Успешно: %d | Сбоев: %d\n", passed, failed);
    if (failed == 0) {
        Serial.println(ANSI_GREEN "РЕЗУЛЬТАТ: ВСЕ ТЕСТЫ ПРОЙДЕНЫ УСПЕШНО [100% OK]" ANSI_RESET);
    } else {
        Serial.println(ANSI_RED "РЕЗУЛЬТАТ: ОБНАРУЖЕНЫ ОШИБКИ В ТЕСТАХ" ANSI_RESET);
    }
    Serial.println(F("========================================================\n"));
}

