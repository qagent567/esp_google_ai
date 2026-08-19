#include "HardwareController.h"
#include "ConfigManager.h"
#include "NetworkManager.h"
#include "UsageTracker.h"
#include <WiFi.h>
#include <Wire.h>

#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif

// Список безопасных GPIO по умолчанию для платы ESP32 WROOM / DevKit
static const uint8_t DEFAULT_SAFE_PINS[] = {
    2, 4, 5, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33, 34, 35, 36, 39
};

HardwareController::HardwareController()
    : _enabled(true), _configMgr(nullptr), _netMgr(nullptr), _usageTracker(nullptr) {
    allowAllSafePins();
}

HardwareController::~HardwareController() {}

bool HardwareController::begin() {
    _enabled = true;
    allowAllSafePins();
    return true;
}

void HardwareController::setSystemContext(ConfigManager* cfg, NetworkManager* net, UsageTracker* usage) {
    _configMgr = cfg;
    _netMgr = net;
    _usageTracker = usage;
}

void HardwareController::allowAllSafePins() {
    _allowedPins.clear();
    for (size_t i = 0; i < sizeof(DEFAULT_SAFE_PINS) / sizeof(DEFAULT_SAFE_PINS[0]); i++) {
        _allowedPins.push_back(DEFAULT_SAFE_PINS[i]);
    }
}

void HardwareController::setAllowedPins(const std::vector<uint8_t>& allowedPins) {
    _allowedPins.clear();
    for (uint8_t pin : allowedPins) {
        if (isValidGpio(pin)) {
            _allowedPins.push_back(pin);
        }
    }
}

bool HardwareController::isPinAllowed(uint8_t pin) const {
    if (!_enabled) return false;
    if (_allowedPins.empty()) {
        return isValidGpio(pin);
    }
    for (uint8_t p : _allowedPins) {
        if (p == pin) return true;
    }
    return false;
}

bool HardwareController::isValidGpio(uint8_t pin) {
    // Защита пинов встроенной SPI Flash памяти (GPIO 6-11)
    if (pin >= 6 && pin <= 11) return false;
    // Защита UART0 (GPIO 1 TX, GPIO 3 RX)
    if (pin == 1 || pin == 3) return false;
    // Защита недопустимых номеров
    if (pin > 39) return false;
    if (pin >= 28 && pin <= 31) return false; // Не распаяны на чипе
    return true;
}

bool HardwareController::setPinMode(uint8_t pin, uint8_t mode) {
    if (!_enabled || !isPinAllowed(pin)) return false;
    if (pin >= 34 && pin <= 39 && mode == OUTPUT) return false; // Пины 34-39 только вход
    pinMode(pin, mode);
    return true;
}

bool HardwareController::writePin(uint8_t pin, uint8_t value) {
    if (!_enabled || !isPinAllowed(pin)) return false;
    if (pin >= 34 && pin <= 39) return false; // Входные пины не могут быть выходами
    pinMode(pin, OUTPUT);
    digitalWrite(pin, value ? HIGH : LOW);
    return true;
}

int HardwareController::readPin(uint8_t pin) {
    if (!_enabled || !isPinAllowed(pin)) return -1;
    return digitalRead(pin);
}

bool HardwareController::togglePin(uint8_t pin) {
    if (!_enabled || !isPinAllowed(pin)) return false;
    if (pin >= 34 && pin <= 39) return false;
    pinMode(pin, OUTPUT);
    int current = digitalRead(pin);
    digitalWrite(pin, !current);
    return true;
}

bool HardwareController::pulsePin(uint8_t pin, unsigned long durationMs) {
    if (!_enabled || !isPinAllowed(pin)) return false;
    if (pin >= 34 && pin <= 39) return false;
    if (durationMs > 2000) durationMs = 2000; // Ограничение безопасности: макс 2 сек
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    delay(durationMs);
    digitalWrite(pin, LOW);
    return true;
}

bool HardwareController::setPWM(uint8_t pin, uint32_t duty, uint32_t freq, uint8_t resolution) {
    if (!_enabled || !isPinAllowed(pin)) return false;
    if (pin >= 34 && pin <= 39) return false;
    (void)freq;
    (void)resolution;
    analogWrite(pin, duty); // Стандартный метод analogWrite ESP32 Arduino Core
    return true;
}

bool HardwareController::playTone(uint8_t pin, unsigned int freq, unsigned long durationMs) {
    if (!_enabled || !isPinAllowed(pin)) return false;
    if (pin >= 34 && pin <= 39) return false;
    if (durationMs > 3000) durationMs = 3000; // Максимум 3 секунды
    #if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 4)
    tone(pin, freq, durationMs);
    #endif
    return true;
}

int HardwareController::readAnalogPin(uint8_t pin) {
    if (!_enabled) return -1;
    // ADC1 пины на ESP32 (безопасны для работы с Wi-Fi)
    if (pin != 32 && pin != 33 && pin != 34 && pin != 35 && pin != 36 && pin != 39) {
        return -1;
    }
    if (!isPinAllowed(pin)) return -1;
    return analogRead(pin);
}

float HardwareController::readAnalogVoltage(uint8_t pin) {
    int raw = readAnalogPin(pin);
    if (raw < 0) return -1.0f;
    return (raw / 4095.0f) * 3.3f;
}

String HardwareController::readAllAllowedPins() {
    if (!_enabled) return "Аппаратный контроллер отключен.";
    String report = "Статус разрешенных пинов:\n";
    for (uint8_t p : _allowedPins) {
        char buf[64];
        if (p == 32 || p == 33 || p == 34 || p == 35 || p == 36 || p == 39) {
            int raw = analogRead(p);
            float v = (raw / 4095.0f) * 3.3f;
            snprintf(buf, sizeof(buf), " • GPIO %2u (ADC1) : %d (%.2f В)\n", p, raw, v);
        } else {
            int val = digitalRead(p);
            snprintf(buf, sizeof(buf), " • GPIO %2u (DIG)  : %s\n", p, val ? "HIGH (1)" : "LOW (0)");
        }
        report += buf;
    }
    return report;
}

DeviceTelemetry HardwareController::getTelemetry() {
    DeviceTelemetry t;
    #if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 4)
        t.chipTempC = temperatureRead();
    #else
        t.chipTempC = (temprature_sens_read() - 32) / 1.8f;
    #endif

    t.freeHeapBytes = ESP.getFreeHeap();
    t.minFreeHeapBytes = ESP.getMinFreeHeap();
    t.heapSizeBytes = ESP.getHeapSize();
    t.uptimeSec = millis() / 1000UL;
    t.wifiRssi = WiFi.isConnected() ? WiFi.RSSI() : 0;
    t.cpuFreqMHz = ESP.getCpuFreqMHz();
    t.flashSizeBytes = ESP.getFlashChipSize();
    return t;
}

String HardwareController::getTelemetrySummary() {
    DeviceTelemetry t = getTelemetry();
    char buf[256];
    snprintf(buf, sizeof(buf),
             "Температура чипа: %.1f °C | Свободно RAM: %u байт (Мин: %u) | Аптайм: %lu сек | Wi-Fi RSSI: %d dBm | CPU: %u МГц",
             t.chipTempC, t.freeHeapBytes, t.minFreeHeapBytes, t.uptimeSec, t.wifiRssi, t.cpuFreqMHz);
    return String(buf);
}

String HardwareController::selfDiagnose() {
    DeviceTelemetry t = getTelemetry();
    String report = "=== ПОЛНАЯ САМОДИАГНОСТИКА ESP32 ===\n";
    
    // 1. Память
    report += "1. Память RAM: " + String(t.freeHeapBytes / 1024) + " КБ свободно из " + String(t.heapSizeBytes / 1024) + " КБ";
    if (t.freeHeapBytes < 40000) {
        report += " [ПРЕДУПРЕЖДЕНИЕ: Мало памяти!]\n";
    } else {
        report += " [ОТЛИЧНО]\n";
    }

    // 2. Температура
    report += "2. Температура кристалла: " + String(t.chipTempC, 1) + " °C";
    if (t.chipTempC > 75.0f) {
        report += " [ВНИМАНИЕ: Высокая температура чипа!]\n";
    } else {
        report += " [НОРМА]\n";
    }

    // 3. Wi-Fi
    report += "3. Wi-Fi связь: ";
    if (WiFi.isConnected()) {
        report += "Подключено к '" + WiFi.SSID() + "', IP: " + WiFi.localIP().toString() + ", Сигнал: " + String(t.wifiRssi) + " dBm [OK]\n";
    } else {
        report += "Отключено [НЕТ СЕТИ]\n";
    }

    // 4. Частота CPU и Flash
    report += "4. Частота CPU: " + String(t.cpuFreqMHz) + " МГц | Flash: " + String(t.flashSizeBytes / (1024 * 1024)) + " МБ\n";

    // 5. Конфигурация NVS
    if (_configMgr) {
        report += "5. Модель Gemini: " + _configMgr->getConfig().model + " | Лимит токенов: " + String(_configMgr->getConfig().maxTokens) + "\n";
    }

    return report;
}

const char* HardwareController::getI2CDeviceName(uint8_t address) {
    switch (address) {
        case 0x20: case 0x21: case 0x22: case 0x23:
        case 0x24: case 0x25: case 0x26: return "PCF8574 I/O Expander";
        case 0x27: return "PCF8574T (LCD1602/2004 Дисплей)";
        case 0x38: return "AHT10/AHT20 Датчик влажности и температуры";
        case 0x3C: return "SSD1306/SH1106 OLED Дисплей (0x3C, 128x64)";
        case 0x3D: return "SSD1306 OLED Дисплей (0x3D)";
        case 0x40: return "INA219 Датчик тока и мощности";
        case 0x48: return "ADS1115 (16-bit ADC) или LM75 Термометр";
        case 0x49: case 0x4A: case 0x4B: return "ADS1115 / ADS1015 ADC";
        case 0x50: case 0x51: case 0x52: case 0x53:
        case 0x57: return "AT24C32/64 EEPROM Память";
        case 0x5A: return "MLX90614 Бесконтактный ИК-термометр";
        case 0x5C: return "AM2320 / DHT12 Датчик температуры";
        case 0x68: return "MPU6050 Гироскоп/Акселерометр или DS3231 Часы RTC";
        case 0x76: return "BMP280 / BME280 / BME680 (Давление/Температура/Влажность)";
        case 0x77: return "BME280 / BMP280 (Вторичный адрес 0x77)";
        default:   return "Неизвестное I2C устройство";
    }
}

String HardwareController::scanI2C(uint8_t sda, uint8_t scl) {
    if (!_enabled) return "Аппаратный контроллер I2C отключен.";
    Wire.begin(sda, scl);
    char buf[128];
    snprintf(buf, sizeof(buf), "Сканирование I2C (SDA=GPIO%u, SCL=GPIO%u):\n", sda, scl);
    String result = buf;
    int nDevices = 0;

    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();

        if (error == 0) {
            char hexBuf[96];
            snprintf(hexBuf, sizeof(hexBuf), " • Адрес [0x%02X] : %s\n", address, getI2CDeviceName(address));
            result += hexBuf;
            nDevices++;
        }
    }

    if (nDevices == 0) {
        result += " Устройства I2C не обнаружены на шине.";
    } else {
        snprintf(buf, sizeof(buf), "Всего обнаружено устройств: %d", nDevices);
        result += buf;
    }
    return result;
}

String HardwareController::scanWiFi() {
    String report = "=== СКАНИРОВАНИЕ WI-FI СЕТЕЙ ===\n";
    int n = WiFi.scanNetworks();
    if (n == 0) {
        report += "Сети не найдены.\n";
    } else {
        report += "Найдено сетей: " + String(n) + "\n";
        for (int i = 0; i < n; ++i) {
            char buf[128];
            const char* encStr = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Открытая" : "Защищенная";
            snprintf(buf, sizeof(buf), " %2d. %-24s | Сигнал: %3d dBm | Канал: %2d | %s\n",
                     i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i), encStr);
            report += buf;
        }
    }
    WiFi.scanDelete();
    return report;
}

String HardwareController::getHardwareCapabilitiesDescription() {
    return "ИНСТРУКЦИЯ ПО АВТОНОМНОМУ УПРАВЛЕНИЮ ESP32:\n"
           "Ты — бортовой ИИ-агент микроконтроллера ESP32 (Xtensa Dual-Core 240MHz, Wi-Fi, Flash, NVS).\n"
           "Ты имеешь полный доступ к физическому управлению платой и её системным настройкам.\n"
           "Если необходимо выполнить действие или считать датчики, включи в ответ блок:\n"
           "```action {\"action\": \"<команда>\", ...}```\n\n"
           "ДОСТУПНЫЕ КОМАНДЫ ПЛАТЫ:\n"
           "1. Управление пинами и ШИМ:\n"
           "   • {\"action\": \"set_pin\", \"pin\": 2, \"value\": 1} — подать HIGH(1)/LOW(0) на GPIO (GPIO 2 - синий LED)\n"
           "   • {\"action\": \"toggle_pin\", \"pin\": 2} — переключить состояние выхода\n"
           "   • {\"action\": \"pulse_pin\", \"pin\": 2, \"duration_ms\": 500} — кратковременный импульс\n"
           "   • {\"action\": \"set_pwm\", \"pin\": 2, \"duty\": 128} — ШИМ (яркость LED / мотор, duty 0..255)\n"
           "   • {\"action\": \"play_tone\", \"pin\": 25, \"freq\": 1000, \"duration_ms\": 200} — звуковой сигнал\n"
           "   • {\"action\": \"read_pin\", \"pin\": 4} — считать цифровой вход\n"
           "   • {\"action\": \"read_analog\", \"pin\": 34} — замерить напряжение на ADC1 (GPIO 32,33,34,35,36,39, диапазон 0-3.3В)\n"
           "   • {\"action\": \"read_all_pins\"} — считать состояние всех разрешенных пинов\n\n"
           "2. Датчики и периферия:\n"
           "   • {\"action\": \"scan_i2c\", \"sda\": 21, \"scl\": 22} — сканирование шины I2C и авто-определение подключенных дисплеев/сенсоров\n"
           "   • {\"action\": \"scan_wifi\"} — поиск доступных Wi-Fi сетей вокруг платы\n\n"
           "3. Системные настройки и диагностика:\n"
           "   • {\"action\": \"get_telemetry\"} — температура процессора, свободная RAM, аптайм, RSSI\n"
           "   • {\"action\": \"self_diagnose\"} — полный тест здоровья платы\n"
           "   • {\"action\": \"set_model\", \"model\": \"gemini-3.5-flash-lite\"} — переключить активную модель\n"
           "   • {\"action\": \"set_temperature\", \"value\": 0.7} — настройка креативности (0.0-1.0)\n"
           "   • {\"action\": \"set_timezone\", \"value\": 3} — часовой пояс (UTC)\n"
           "   • {\"action\": \"save_config\"} — сохранить настройки во Flash NVS\n"
           "   • {\"action\": \"set_cpu_freq\", \"freq\": 240} — тактовая частота (80, 160, 240 МГц)\n"
           "   • {\"action\": \"restart\"} — перезагрузка ESP32\n\n"
           "Действуй автономно, технически грамотно и комментируй свои действия.";
}

String HardwareController::executeActionJson(const String& jsonAction) {
    if (!_enabled) {
        return "Аппаратное управление отключено пользователем.";
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, jsonAction);
    char buf[160];
    if (err) {
        snprintf(buf, sizeof(buf), "Ошибка разбора JSON команды: %s", err.c_str());
        return String(buf);
    }

    const char* action = doc["action"] | "";

    // ─── 1. ЦИФРОВЫЕ И АНАЛОГОВЫЕ ВХОДЫ/ВЫХОДЫ ──────────────────────────────
    if (strcmp(action, "set_pin") == 0) {
        uint8_t pin = doc["pin"] | 255;
        uint8_t val = doc["value"] | 0;
        if (!isPinAllowed(pin)) {
            snprintf(buf, sizeof(buf), "Ошибка: пин GPIO %u не разрешен", pin);
            return String(buf);
        }
        writePin(pin, val);
        snprintf(buf, sizeof(buf), "GPIO %u установлен в %s", pin, val ? "HIGH (1)" : "LOW (0)");
        return String(buf);
    }
    else if (strcmp(action, "read_pin") == 0) {
        uint8_t pin = doc["pin"] | 255;
        if (!isPinAllowed(pin)) {
            snprintf(buf, sizeof(buf), "Ошибка: пин GPIO %u не разрешен", pin);
            return String(buf);
        }
        setPinMode(pin, INPUT);
        int val = readPin(pin);
        snprintf(buf, sizeof(buf), "Значение цифрового GPIO %u: %d", pin, val);
        return String(buf);
    }
    else if (strcmp(action, "toggle_pin") == 0) {
        uint8_t pin = doc["pin"] | 2;
        if (!isPinAllowed(pin)) {
            snprintf(buf, sizeof(buf), "Ошибка: пин GPIO %u не разрешен", pin);
            return String(buf);
        }
        togglePin(pin);
        int current = readPin(pin);
        snprintf(buf, sizeof(buf), "GPIO %u переключен. Текущий уровень: %d", pin, current);
        return String(buf);
    }
    else if (strcmp(action, "pulse_pin") == 0) {
        uint8_t pin = doc["pin"] | 2;
        unsigned long dur = doc["duration_ms"] | 200;
        if (!isPinAllowed(pin)) return "Ошибка: пин не разрешен.";
        pulsePin(pin, dur);
        snprintf(buf, sizeof(buf), "Выдан импульс длительностью %lu мс на GPIO %u", dur, pin);
        return String(buf);
    }
    else if (strcmp(action, "set_pwm") == 0 || strcmp(action, "analog_write") == 0) {
        uint8_t pin = doc["pin"] | 2;
        uint32_t duty = doc["duty"] | 128;
        if (!isPinAllowed(pin)) return "Ошибка: пин не разрешен.";
        setPWM(pin, duty);
        snprintf(buf, sizeof(buf), "ШИМ на GPIO %u установлен: скважность %u/255 (%.1f%%)", pin, duty, (duty/255.0f)*100.0f);
        return String(buf);
    }
    else if (strcmp(action, "play_tone") == 0) {
        uint8_t pin = doc["pin"] | 25;
        unsigned int freq = doc["freq"] | 1000;
        unsigned long dur = doc["duration_ms"] | 200;
        if (!isPinAllowed(pin)) return "Ошибка: пин не разрешен.";
        playTone(pin, freq, dur);
        snprintf(buf, sizeof(buf), "Воспроизведен тон %u Гц на GPIO %u (%lu мс)", freq, pin, dur);
        return String(buf);
    }
    else if (strcmp(action, "read_analog") == 0) {
        uint8_t pin = doc["pin"] | 255;
        if (!isPinAllowed(pin)) return "Ошибка: аналоговый пин не разрешен.";
        int val = readAnalogPin(pin);
        if (val < 0) return "Ошибка чтения аналогового входа.";
        float v = (val / 4095.0f) * 3.3f;
        snprintf(buf, sizeof(buf), "ADC GPIO %u: %d (%.2f В)", pin, val, v);
        return String(buf);
    }
    else if (strcmp(action, "read_all_pins") == 0) {
        return readAllAllowedPins();
    }

    // ─── 2. ДАТЧИКИ И СЕТЬ ──────────────────────────────────────────────────
    else if (strcmp(action, "scan_i2c") == 0) {
        uint8_t sda = doc["sda"] | 21;
        uint8_t scl = doc["scl"] | 22;
        return scanI2C(sda, scl);
    }
    else if (strcmp(action, "scan_wifi") == 0) {
        return scanWiFi();
    }

    // ─── 3. ДИАГНОСТИКА И СИСТЕМНЫЕ НАСТРОЙКИ ──────────────────────────────
    else if (strcmp(action, "get_telemetry") == 0) {
        return getTelemetrySummary();
    }
    else if (strcmp(action, "self_diagnose") == 0) {
        return selfDiagnose();
    }
    else if (strcmp(action, "set_model") == 0) {
        const char* model = doc["model"] | "";
        if (strlen(model) > 0 && _configMgr) {
            _configMgr->setModel(model);
            snprintf(buf, sizeof(buf), "Модель Gemini изменена на '%s'", model);
            return String(buf);
        }
        return "Ошибка: не указана модель.";
    }
    else if (strcmp(action, "set_temperature") == 0) {
        float temp = doc["value"] | 0.7f;
        if (_configMgr) {
            _configMgr->setTemperature(temp);
            snprintf(buf, sizeof(buf), "Температура генерации установлена: %.2f", temp);
            return String(buf);
        }
        return "Ошибка доступа к ConfigManager.";
    }
    else if (strcmp(action, "set_timezone") == 0) {
        int tz = doc["value"] | 3;
        if (_configMgr) {
            _configMgr->setTimezone(tz);
            snprintf(buf, sizeof(buf), "Часовой пояс установлен: UTC%+d", tz);
            return String(buf);
        }
        return "Ошибка доступа к ConfigManager.";
    }
    else if (strcmp(action, "save_config") == 0) {
        if (_configMgr && _configMgr->save()) {
            return "Настройки успешно сохранены в энергонезависимую память Flash (NVS).";
        }
        return "Не удалось сохранить настройки во Flash.";
    }
    else if (strcmp(action, "set_cpu_freq") == 0) {
        uint32_t freq = doc["freq"] | 240;
        if (freq == 80 || freq == 160 || freq == 240) {
            setCpuFrequencyMhz(freq);
            snprintf(buf, sizeof(buf), "Частота CPU установлена на %u МГц", freq);
            return String(buf);
        }
        return "Поддерживаемые частоты CPU: 80, 160, 240 МГц.";
    }
    else if (strcmp(action, "restart") == 0) {
        ESP.restart();
        return "Перезагрузка ESP32...";
    }

    snprintf(buf, sizeof(buf), "Неизвестная команда действия: %s", action);
    return String(buf);
}
