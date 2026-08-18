#include "CLI.h"
#include <esp_system.h>

SerialCLI::SerialCLI(ConfigManager& configMgr, NetworkManager& netMgr, GeminiClient& geminiClient)
    : _configMgr(configMgr), _netMgr(netMgr), _geminiClient(geminiClient) {}

void SerialCLI::begin() {
    _inputBuffer.reserve(256);
    printWelcome();
    
    if (!_configMgr.isConfigured()) {
        Serial.println(F("\n[!] Устройство не настроено. Запустите мастер настройки 'wizard' или настройте параметры вручную (см. 'help')."));
    }
    
    printPrompt();
}

void SerialCLI::printWelcome() {
    Serial.println(F("\n========================================================"));
    Serial.println(F("       ESP32 + Google AI Studio (Gemini) [Smart DNS]     "));
    Serial.println(F("========================================================"));
    Serial.println(F("Обход ограничений РФ через Smart DNS (xbox-dns.ru)"));
    Serial.println(F("Введите 'help' для списка команд или 'wizard' для настройки."));
    Serial.println(F("--------------------------------------------------------"));
}

void SerialCLI::printPrompt() {
    if (_currentWizard != WizardType::NONE) {
        Serial.print(F("[WIZARD] > "));
    } else {
        Serial.print(F("ESP32-AI> "));
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

void SerialCLI::printHelp() {
    Serial.println(F("\n--- СПИСОК ДОСТУПНЫХ КОМАНД ---"));
    Serial.println(F(" Настройки Wi-Fi:"));
    Serial.println(F("   set ssid <имя_сети>      - Установить имя Wi-Fi сети"));
    Serial.println(F("   set pass <пароль>        - Установить пароль Wi-Fi сети"));
    Serial.println(F("   scan                     - Сканировать доступные сети Wi-Fi"));
    Serial.println(F("   connect                  - Подключиться / переподключиться к Wi-Fi"));
    Serial.println(F("   disconnect               - Отключиться от Wi-Fi"));
    Serial.println(F(""));
    Serial.println(F(" Настройки Google AI (Gemini):"));
    Serial.println(F("   models                   - Сканировать и вывести список доступных моделей Gemini"));
    Serial.println(F("   set key <api_key>        - Установить API-ключ Google AI Studio"));
    Serial.println(F("   set model <имя_модели>   - Сменить модель (по умолчанию: gemini-2.0-flash)"));
    Serial.println(F("   set prompt <текст>       - Задать системный промпт (роль ассистента)"));
    Serial.println(F("   set tokens <число>       - Максимальное число токенов (1-8192)"));
    Serial.println(F("   set temp <0.0-2.0>       - Температура генерации (креативность)"));
    Serial.println(F(""));
    Serial.println(F(" Настройки DNS (обход блокировок):"));
    Serial.println(F("   set dns <dns1> [dns2]    - Установить Smart DNS (по умолчанию 111.88.96.50 111.88.96.51)"));
    Serial.println(F(""));
    Serial.println(F(" Общие команды:"));
    Serial.println(F("   ask <вопрос>             - Отправить текстовый запрос в Gemini"));
    Serial.println(F("   <любой текст>            - Если просто ввести текст, он отправится в Gemini"));
    Serial.println(F("   test                     - Тестовый пинг-запрос к Gemini API"));
    Serial.println(F("   demo automation          - Демонстрация использования AI в коде (сенсоры)"));
    Serial.println(F("   status                   - Показать текущее состояние и настройки"));
    Serial.println(F("   setup wifi               - Интерактивная настройка Wi-Fi"));
    Serial.println(F("   setup ai                 - Интерактивная настройка параметров нейросети"));
    Serial.println(F("   wizard                   - Полный пошаговый мастер первой настройки"));
    Serial.println(F("   save                     - Сохранить все параметры во Flash (NVS)"));
    Serial.println(F("   reset                    - Сбросить все настройки на значения по умолчанию"));
    Serial.println(F("   reboot                   - Перезагрузить микроконтроллер ESP32"));
    Serial.println(F("   help / ?                 - Показать это меню помощи"));
    Serial.println(F("--------------------------------\n"));
}

void SerialCLI::printStatus() {
    const AppConfig& cfg = _configMgr.getConfig();

    Serial.println(F("\n================ ТЕКУЩИЙ СТАТУС ================"));
    Serial.printf(" [СЕТЬ] Wi-Fi SSID       : %s\n", cfg.wifiSsid.isEmpty() ? "[Не задан]" : cfg.wifiSsid.c_str());
    Serial.printf(" [СЕТЬ] Пароль           : %s\n", maskString(cfg.wifiPassword, 1, 1).c_str());
    Serial.printf(" [СЕТЬ] Статус           : %s\n", _netMgr.isConnected() ? "ПОДКЛЮЧЕНО" : "ОТКЛЮЧЕНО");
    Serial.printf(" [СЕТЬ] Локальный IP     : %s\n", _netMgr.getLocalIP().c_str());
    Serial.printf(" [СЕТЬ] Уровень сигнала  : %d dBm\n", _netMgr.getRSSI());
    Serial.printf(" [DNS]  Smart DNS 1      : %s (Активный: %s)\n", cfg.dnsPrimary.c_str(), _netMgr.getPrimaryDNS().c_str());
    Serial.printf(" [DNS]  Smart DNS 2      : %s (Активный: %s)\n", cfg.dnsSecondary.c_str(), _netMgr.getSecondaryDNS().c_str());
    Serial.println(F("------------------------------------------------"));
    Serial.printf(" [AI]   Модель Gemini    : %s\n", cfg.model.c_str());
    Serial.printf(" [AI]   API-Ключ         : %s\n", maskString(cfg.apiKey, 6, 4).c_str());
    Serial.printf(" [AI]   Системный промпт : %s\n", cfg.systemPrompt.c_str());
    Serial.printf(" [AI]   Макс. токенов    : %d\n", cfg.maxTokens);
    Serial.printf(" [AI]   Температура      : %.2f\n", cfg.temperature);
    Serial.println(F("------------------------------------------------"));
    Serial.printf(" [СИСТЕМА] Свободно RAM  : %u байт\n", ESP.getFreeHeap());
    Serial.printf(" [СИСТЕМА] Мин. своб. RAM: %u байт\n", ESP.getMinFreeHeap());
    Serial.printf(" [СИСТЕМА] Аптайм        : %lu сек\n", millis() / 1000);
    Serial.println(F("================================================\n"));
}

void SerialCLI::startWizard(WizardType type) {
    _currentWizard = type;
    _wizardStep = 1;
    Serial.println(F("\n========================================================"));
    
    if (type == WizardType::FULL) {
        Serial.println(F("           МАСТЕР ПОЛНОЙ НАСТРОЙКИ             "));
        Serial.println(F("========================================================"));
        Serial.println(F("Для отмены введите 'cancel' в любой момент."));
        Serial.println(F("ШАГ 1/3: Введите SSID (имя) вашей Wi-Fi сети:"));
    } else if (type == WizardType::WIFI) {
        Serial.println(F("           НАСТРОЙКА WI-FI             "));
        Serial.println(F("========================================================"));
        Serial.println(F("Для отмены введите 'cancel' в любой момент."));
        Serial.println(F("ШАГ 1/2: Введите SSID (имя) вашей Wi-Fi сети:"));
    } else if (type == WizardType::AI) {
        Serial.println(F("           НАСТРОЙКА GOOGLE AI (GEMINI)             "));
        Serial.println(F("========================================================"));
        Serial.println(F("Для отмены введите 'cancel' в любой момент."));
        Serial.println(F("ШАГ 1/2: Введите API-ключ Google AI Studio (Gemini API Key):"));
        Serial.println(F("(Получить ключ бесплатно можно на https://aistudio.google.com/app/apikey)"));
    }
}

void SerialCLI::handleWizardStep(const String& input) {
    if (input.equalsIgnoreCase("cancel")) {
        _currentWizard = WizardType::NONE;
        _wizardStep = 0;
        Serial.println(F("[WIZARD] Настройка отменена."));
        return;
    }

    if (_currentWizard == WizardType::FULL) {
        if (_wizardStep == 1) {
            if (input.isEmpty()) { Serial.println(F("[WIZARD] SSID не может быть пустым. Введите SSID сети:")); return; }
            _configMgr.getConfig().wifiSsid = input;
            _wizardStep = 2;
            Serial.println(F("ШАГ 2/3: Введите пароль от Wi-Fi сети (или нажмите Enter, если сеть открытая):"));
        } else if (_wizardStep == 2) {
            _configMgr.getConfig().wifiPassword = input;
            _wizardStep = 3;
            Serial.println(F("ШАГ 3/3: Введите API-ключ Google AI Studio (Gemini API Key):"));
        } else if (_wizardStep == 3) {
            if (input.isEmpty()) { Serial.println(F("[WIZARD] API-ключ не может быть пустым. Введите API-ключ:")); return; }
            _configMgr.getConfig().apiKey = input;
            _configMgr.save();
            _currentWizard = WizardType::NONE;
            _wizardStep = 0;
            Serial.println(F("\n[WIZARD] Все параметры успешно настроены и сохранены во Flash!"));
            _netMgr.connect();
        }
    } else if (_currentWizard == WizardType::WIFI) {
        if (_wizardStep == 1) {
            if (input.isEmpty()) { Serial.println(F("[WIZARD] SSID не может быть пустым. Введите SSID сети:")); return; }
            _configMgr.getConfig().wifiSsid = input;
            _wizardStep = 2;
            Serial.println(F("ШАГ 2/2: Введите пароль от Wi-Fi сети (или нажмите Enter, если сеть открытая):"));
        } else if (_wizardStep == 2) {
            _configMgr.getConfig().wifiPassword = input;
            _configMgr.save();
            _currentWizard = WizardType::NONE;
            _wizardStep = 0;
            Serial.println(F("\n[WIZARD] Настройки Wi-Fi успешно сохранены!"));
            _netMgr.connect();
        }
    } else if (_currentWizard == WizardType::AI) {
        if (_wizardStep == 1) {
            if (input.isEmpty()) { Serial.println(F("[WIZARD] API-ключ не может быть пустым. Введите API-ключ:")); return; }
            _configMgr.getConfig().apiKey = input;
            _wizardStep = 2;
            Serial.println(F("ШАГ 2/2: Введите системный промпт (или нажмите Enter для текущего):"));
            Serial.printf("Текущий промпт: %s\n", _configMgr.getConfig().systemPrompt.c_str());
        } else if (_wizardStep == 2) {
            if (!input.isEmpty()) {
                _configMgr.getConfig().systemPrompt = input;
            }
            _configMgr.save();
            _currentWizard = WizardType::NONE;
            _wizardStep = 0;
            Serial.println(F("\n[WIZARD] Настройки AI успешно сохранены!"));
        }
    }
}

void SerialCLI::handleCommand(String line) {
    line.trim();
    if (line.isEmpty()) return;

    // Режим мастера настройки
    if (_currentWizard != WizardType::NONE) {
        handleWizardStep(line);
        return;
    }

    // Обработка команд
    String lower = line;
    lower.toLowerCase();

    if (lower == "help" || lower == "?") {
        printHelp();
    } else if (lower == "status" || lower == "info") {
        printStatus();
    } else if (lower == "setup wifi") {
        startWizard(WizardType::WIFI);
    } else if (lower == "setup ai") {
        startWizard(WizardType::AI);
    } else if (lower == "wizard") {
        startWizard(WizardType::FULL);
    } else if (lower == "scan") {
        _netMgr.scanNetworks();
    } else if (lower == "connect") {
        _netMgr.connect();
    } else if (lower == "disconnect") {
        _netMgr.disconnect();
    } else if (lower == "save") {
        _configMgr.save();
    } else if (lower == "reset") {
        _configMgr.resetToDefaults();
        _configMgr.save();
        Serial.println(F("[СБРОС] Настройки сброшены к значениям по умолчанию."));
    } else if (lower == "reboot" || lower == "restart") {
        Serial.println(F("[СИСТЕМА] Перезагрузка ESP32..."));
        delay(500);
        ESP.restart();
    } else if (lower == "test") {
        _geminiClient.testConnection();
    } else if (lower == "models" || lower == "scan models" || lower == "list models") {
        _geminiClient.listAvailableModels();
    } else if (lower == "demo automation") {
        Serial.println(F("[DEMO] Демонстрация работы AI в программном коде ESP32..."));
        Serial.println(F("[DEMO] Имитация чтения датчика (Температура: 35.5 C)..."));
        float fakeTemp = 35.5;
        
        // Формируем программный запрос
        String prompt = "Температура в серверной сейчас " + String(fakeTemp) + " градусов Цельсия. Норма до 25. Что мне сделать? Ответь очень кратко, словно ты аварийная система ESP32.";
        
        Serial.println(F("[DEMO] Отправляем системный промпт к Gemini:"));
        Serial.println(prompt);
        
        GeminiResponse res = _geminiClient.ask(prompt);
        if (res.success) {
            Serial.println(F("\n[РЕАКЦИЯ СИСТЕМЫ на основе ответа AI]:"));
            Serial.println(res.text);
            Serial.println(F("\n(Так вы можете использовать _geminiClient.ask() в любом месте кода!)"));
        } else {
            Serial.print(F("[ОШИБКА] Не удалось получить ответ: "));
            Serial.println(res.text);
        }
    } else if (lower.startsWith("set ssid ")) {
        String val = line.substring(9);
        val.trim();
        _configMgr.getConfig().wifiSsid = val;
        _configMgr.save();
        Serial.printf("[ОК] Wi-Fi SSID установлен: '%s' (сохранено)\n", val.c_str());
    } else if (lower.startsWith("set pass ")) {
        String val = line.substring(9);
        val.trim();
        _configMgr.getConfig().wifiPassword = val;
        _configMgr.save();
        Serial.println(F("[ОК] Пароль Wi-Fi обновлен и сохранен."));
    } else if (lower.startsWith("set key ")) {
        String val = line.substring(8);
        val.trim();
        _configMgr.setApiKey(val);
        _configMgr.save();
        Serial.println(F("[ОК] API-ключ Gemini сохранен."));
    } else if (lower.startsWith("set model ")) {
        String val = line.substring(10);
        val.trim();
        _configMgr.setModel(val);
        _configMgr.save();
        Serial.printf("[ОК] Модель Gemini изменена на: '%s' (сохранено)\n", val.c_str());
    } else if (lower.startsWith("set prompt ")) {
        String val = line.substring(11);
        val.trim();
        _configMgr.setSystemPrompt(val);
        _configMgr.save();
        Serial.printf("[ОК] Системный промпт обновлен: '%s' (сохранено)\n", val.c_str());
    } else if (lower.startsWith("set tokens ")) {
        int tokens = line.substring(11).toInt();
        if (tokens > 0 && tokens <= 8192) {
            _configMgr.setMaxTokens(tokens);
            _configMgr.save();
            Serial.printf("[ОК] Макс. токенов установлено: %d (сохранено)\n", tokens);
        } else {
            Serial.println(F("[ОШИБКА] Значение токенов должно быть от 1 до 8192."));
        }
    } else if (lower.startsWith("set temp ")) {
        float temp = line.substring(9).toFloat();
        if (temp >= 0.0f && temp <= 2.0f) {
            _configMgr.setTemperature(temp);
            _configMgr.save();
            Serial.printf("[ОК] Температура генерации установлена: %.2f (сохранено)\n", temp);
        } else {
            Serial.println(F("[ОШИБКА] Температура должна быть в диапазоне от 0.0 до 2.0."));
        }
    } else if (lower.startsWith("set dns ")) {
        String rest = line.substring(8);
        rest.trim();
        int spaceIdx = rest.indexOf(' ');
        String dns1 = (spaceIdx > 0) ? rest.substring(0, spaceIdx) : rest;
        String dns2 = (spaceIdx > 0) ? rest.substring(spaceIdx + 1) : "";
        dns1.trim();
        dns2.trim();

        _configMgr.setDns(dns1, dns2);
        _configMgr.save();
        _netMgr.applyCustomDNS();
        Serial.printf("[ОК] Smart DNS обновлен: %s, %s (сохранено)\n", dns1.c_str(), dns2.isEmpty() ? "нет" : dns2.c_str());
    } else {
        // Если это команда "ask <prompt>" или просто введен текст для Gemini
        String prompt = line;
        if (lower.startsWith("ask ")) {
            prompt = line.substring(4);
            prompt.trim();
        }

        if (!_netMgr.isConnected()) {
            Serial.println(F("[ОШИБКА] Нет подключения к Wi-Fi! Введите 'connect' или 'wizard'."));
            return;
        }

        if (_configMgr.getConfig().apiKey.isEmpty()) {
            Serial.println(F("[ОШИБКА] Не задан API-ключ Gemini! Введите 'set key <api_key>'."));
            return;
        }

        Serial.printf("\n[Gemini AI] Отправка запроса к модели %s...\n", _configMgr.getConfig().model.c_str());
        Serial.println(F("--------------------------------------------------------"));
        
        GeminiResponse res = _geminiClient.ask(prompt);
        
        if (res.success) {
            Serial.println(res.text);
            Serial.println(F("--------------------------------------------------------"));
            Serial.printf("[Статистика] Время: %lu мс | Токены: %d\n\n", res.durationMs, res.totalTokens);
        } else {
            Serial.printf("[ОШИБКА] %s (HTTP: %d, Время: %lu мс)\n\n", res.text.c_str(), res.httpCode, res.durationMs);
        }
    }
}

void SerialCLI::update() {
    while (Serial.available()) {
        char c = (char)Serial.read();

        // Обработка переноса строки / Enter
        if (c == '\r' || c == '\n') {
            if (_inputBuffer.length() > 0) {
                Serial.println();
                handleCommand(_inputBuffer);
                _inputBuffer = "";
                printPrompt();
            }
        }
        // Обработка Backspace
        else if (c == '\b' || c == 0x7F) {
            if (_inputBuffer.length() > 0) {
                _inputBuffer.remove(_inputBuffer.length() - 1);
                Serial.print(F("\b \b"));
            }
        }
        // Обычные символы
        else {
            if (_inputBuffer.length() < 512) {
                _inputBuffer += c;
                Serial.print(c); // Эхо-вывод в терминал
            }
        }
    }
}
