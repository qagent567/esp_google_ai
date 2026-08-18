#include "CLI.h"
#include <esp_system.h>

#define ANSI_RESET   ""
#define ANSI_RED     ""
#define ANSI_GREEN   ""
#define ANSI_YELLOW  ""
#define ANSI_BLUE    ""
#define ANSI_CYAN    ""
#define ANSI_BOLD    ""

SerialCLI::SerialCLI(ConfigManager& configMgr, NetworkManager& netMgr, GeminiClient& geminiClient)
    : _configMgr(configMgr), _netMgr(netMgr), _geminiClient(geminiClient) {}

void SerialCLI::begin() {
    _inputBuffer.reserve(256);
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

    Serial.println(ANSI_CYAN "\n================ ТЕКУЩИЙ СТАТУС ================" ANSI_RESET);
    Serial.printf(" [СЕТЬ] Wi-Fi SSID       : %s\n", cfg.wifiSsid.isEmpty() ? ANSI_RED "[Не задан]" ANSI_RESET : cfg.wifiSsid.c_str());
    Serial.printf(" [СЕТЬ] Пароль (RAM)     : %s\n", maskString(cfg.wifiPassword, 1, 1).c_str());
    Serial.printf(" [СЕТЬ] Статус           : %s\n", _netMgr.isConnected() ? ANSI_GREEN "ПОДКЛЮЧЕНО" ANSI_RESET : ANSI_RED "ОТКЛЮЧЕНО" ANSI_RESET);
    Serial.printf(" [СЕТЬ] Локальный IP     : %s\n", _netMgr.getLocalIP().c_str());
    Serial.printf(" [СЕТЬ] Уровень сигнала  : %d dBm\n", _netMgr.getRSSI());
    Serial.printf(" [DNS]  Smart DNS 1      : %s (Активный: %s)\n", cfg.dnsPrimary.c_str(), _netMgr.getPrimaryDNS().c_str());
    Serial.printf(" [DNS]  Smart DNS 2      : %s (Активный: %s)\n", cfg.dnsSecondary.c_str(), _netMgr.getSecondaryDNS().c_str());
    Serial.println(ANSI_CYAN "------------------------------------------------" ANSI_RESET);
    Serial.printf(" [AI]   Модель Gemini    : %s\n", cfg.model.c_str());
    Serial.printf(" [AI]   API-Ключ         : %s\n", maskString(cfg.apiKey, 6, 4).c_str());
    Serial.printf(" [AI]   Системный промпт : %s\n", cfg.systemPrompt.c_str());
    Serial.printf(" [AI]   Макс. токенов    : %d\n", cfg.maxTokens);
    Serial.printf(" [AI]   Температура      : %.2f\n", cfg.temperature);
    Serial.println(ANSI_CYAN "------------------------------------------------" ANSI_RESET);
    Serial.printf(" [СИСТЕМА] Свободно RAM  : %u байт\n", ESP.getFreeHeap());
    Serial.printf(" [СИСТЕМА] Мин. своб. RAM: %u байт\n", ESP.getMinFreeHeap());
    Serial.printf(" [СИСТЕМА] Аптайм        : %lu сек\n", millis() / 1000);
    Serial.println(ANSI_CYAN "================================================\n" ANSI_RESET);
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
                _configMgr.getConfig().wifiSsid = input;
            }
            _wizardStep = 2;
            Serial.println("ШАГ 2/3: Введите пароль от Wi-Fi сети (или нажмите Enter, если сеть открытая):");
        } else if (_wizardStep == 2) {
            _configMgr.getConfig().wifiPassword = input;
            _wizardStep = 3;
            Serial.println("ШАГ 3/3: Введите API-ключ Google AI Studio (Gemini API Key):");
            if (!_configMgr.getConfig().apiKey.isEmpty()) Serial.println("(Ключ уже сохранен. Нажмите Enter, чтобы оставить текущий)");
        } else if (_wizardStep == 3) {
            if (input.isEmpty()) { 
                if (_configMgr.getConfig().apiKey.isEmpty()) { Serial.println(ANSI_RED "[WIZARD] API-ключ не может быть пустым. Введите API-ключ:" ANSI_RESET); return; }
            } else {
                _configMgr.getConfig().apiKey = input;
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
                _configMgr.getConfig().wifiSsid = input;
            }
            _wizardStep = 2;
            Serial.println("ШАГ 2/2: Введите пароль от Wi-Fi сети (или нажмите Enter, если сеть открытая):");
        } else if (_wizardStep == 2) {
            _configMgr.getConfig().wifiPassword = input;
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
                _configMgr.getConfig().apiKey = input;
            }
            _wizardStep = 2;
            Serial.println("ШАГ 2/2: Введите системный промпт (или нажмите Enter для текущего):");
        } else if (_wizardStep == 2) {
            if (!input.isEmpty()) { _configMgr.getConfig().systemPrompt = input; }
            _configMgr.save();
            _currentWizard = WizardType::NONE;
            _wizardStep = 0;
            Serial.println(ANSI_GREEN "\n[WIZARD] Настройки AI успешно сохранены во Flash!" ANSI_RESET);
        }
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
    addCommand("reset", "Сбросить параметры", "Система", [this](int argc, String argv[]) {
        _configMgr.resetToDefaults(); _configMgr.save(); Serial.println(ANSI_GREEN "[СБРОС] Настройки сброшены к значениям по умолчанию." ANSI_RESET);
    });
    addCommand("wizard", "Запустить мастер настройки", "Настройки", [this](int argc, String argv[]) { startWizard(WizardType::FULL); });
    addCommand("setup", "Настроить компонент (wifi, ai)", "Настройки", [this](int argc, String argv[]) {
        if (argc < 2) { Serial.println("Использование: setup <wifi|ai>"); return; }
        if (argv[1] == "wifi") startWizard(WizardType::WIFI);
        else if (argv[1] == "ai") startWizard(WizardType::AI);
        else Serial.println("Неизвестный параметр. Доступно: wifi, ai");
    });
    addCommand("scan", "Сканировать сети Wi-Fi", "Wi-Fi", [this](int argc, String argv[]) { _netMgr.scanNetworks(); });
    addCommand("connect", "Подключиться к Wi-Fi", "Wi-Fi", [this](int argc, String argv[]) { _netMgr.connect(); });
    addCommand("disconnect", "Отключиться от Wi-Fi", "Wi-Fi", [this](int argc, String argv[]) { _netMgr.disconnect(); });
    addCommand("set", "Изменить настройку (set ssid, pass, key, model, prompt, tokens, temp, dns)", "Конфигурация", [this](int argc, String argv[]) {
        if (argc < 3) { Serial.println("Использование: set <параметр> <значение>"); return; }
        String param = argv[1]; String val = argv[2];
        if (param == "ssid") { _configMgr.getConfig().wifiSsid = val; Serial.printf("[ОК] Wi-Fi SSID установлен: '%s'\n", val.c_str()); }
        else if (param == "pass") { _configMgr.getConfig().wifiPassword = val; Serial.println("[ОК] Пароль Wi-Fi установлен."); }
        else if (param == "key") { _configMgr.setApiKey(val); _configMgr.save(); Serial.println("[ОК] API-ключ сохранен."); }
        else if (param == "model") { _configMgr.setModel(val); _configMgr.save(); Serial.printf("[ОК] Модель: '%s'\n", val.c_str()); }
        else if (param == "prompt") { _configMgr.setSystemPrompt(val); _configMgr.save(); Serial.println("[ОК] Промпт обновлен."); }
        else if (param == "tokens") { _configMgr.setMaxTokens(val.toInt()); _configMgr.save(); Serial.println("[ОК] Лимит токенов изменен."); }
        else if (param == "temp") { _configMgr.setTemperature(val.toFloat()); _configMgr.save(); Serial.println("[ОК] Температура изменена."); }
        else if (param == "dns") {
            String dns1 = argv[2]; String dns2 = (argc > 3) ? argv[3] : "";
            _configMgr.setDns(dns1, dns2); _configMgr.save(); _netMgr.applyCustomDNS();
            Serial.printf("[ОК] DNS обновлен: %s, %s\n", dns1.c_str(), dns2.c_str());
        } else { Serial.println("Неизвестный параметр."); }
    });
    addCommand("models", "Показать доступные AI модели", "Google AI", [this](int argc, String argv[]) { _geminiClient.listAvailableModels(); });
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
        Serial.printf(ANSI_CYAN "\n[Gemini AI] Отправка запроса к модели %s...\n" ANSI_RESET, _configMgr.getConfig().model.c_str());
        GeminiResponse res = _geminiClient.ask(prompt);
        if (res.success) { Serial.println(res.text); Serial.printf(ANSI_YELLOW "\n[Статистика] Время: %lu мс | Токены: %d\n\n" ANSI_RESET, res.durationMs, res.totalTokens); }
        else { Serial.printf(ANSI_RED "[ОШИБКА] %s (HTTP: %d)\n\n" ANSI_RESET, res.text.c_str(), res.httpCode); }
    });
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
