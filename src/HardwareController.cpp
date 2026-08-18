#include "HardwareController.h"
#include <WiFi.h>
#include <Wire.h>
#include <esp_system.h>

#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif

// Список системных запрещенных пинов (Flash SPI и UART0)
static const uint8_t FORBIDDEN_GPIOS[] = {
    1, 3,       // UART0 TX, RX
    6, 7, 8, 9, 10, 11 // Встроенная Flash SPI
};

HardwareController::HardwareController() : _enabled(true) {}
HardwareController::~HardwareController() {}

bool HardwareController::begin() {
    // Безопасная инициализация: не трогаем пины без явного запроса пользователя
    return true;
}

void HardwareController::setAllowedPins(const std::vector<uint8_t>& allowedPins) {
    _allowedPins = allowedPins;
}

void HardwareController::allowAllSafePins() {
    _allowedPins.clear();
}

bool HardwareController::isPinAllowed(uint8_t pin) const {
    if (!isValidGpio(pin)) return false;
    if (_allowedPins.empty()) return true; // Если белый список не задан, разрешены все безопасные пины
    for (uint8_t p : _allowedPins) {
        if (p == pin) return true;
    }
    return false;
}

bool HardwareController::isValidGpio(uint8_t pin) {
    if (pin > 39) return false;
    for (size_t i = 0; i < sizeof(FORBIDDEN_GPIOS) / sizeof(FORBIDDEN_GPIOS[0]); i++) {
        if (pin == FORBIDDEN_GPIOS[i]) return false;
    }
    return true;
}

bool HardwareController::setPinMode(uint8_t pin, uint8_t mode) {
    if (!_enabled || !isPinAllowed(pin)) return false;
    if (pin >= 34 && pin <= 39 && mode == OUTPUT) return false;
    pinMode(pin, mode);
    return true;
}

bool HardwareController::writePin(uint8_t pin, uint8_t value) {
    if (!_enabled || !isPinAllowed(pin)) return false;
    if (pin >= 34 && pin <= 39) return false;
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

int HardwareController::readAnalogPin(uint8_t pin) {
    if (!_enabled) return -1;
    if (pin != 32 && pin != 33 && pin != 34 && pin != 35 && pin != 36 && pin != 39) {
        return -1;
    }
    if (!isPinAllowed(pin)) return -1;
    return analogRead(pin);
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

String HardwareController::scanI2C(uint8_t sda, uint8_t scl) {
    if (!_enabled) return "Аппаратный контроллер I2C отключен.";
    Wire.begin(sda, scl);
    char buf[128];
    snprintf(buf, sizeof(buf), "Сканирование шины I2C (SDA=%u, SCL=%u):\n", sda, scl);
    String result;
    result.reserve(256);
    result = buf;
    int nDevices = 0;

    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();

        if (error == 0) {
            char hexBuf[48];
            snprintf(hexBuf, sizeof(hexBuf), " - Найдено устройство по адресу 0x%02X\n", address);
            result += hexBuf;
            nDevices++;
        }
    }

    if (nDevices == 0) {
        result += " Устройства I2C не обнаружены.";
    } else {
        snprintf(buf, sizeof(buf), " Всего обнаружено устройств: %d", nDevices);
        result += buf;
    }
    return result;
}

String HardwareController::getHardwareCapabilitiesDescription() {
    return "ИНСТРУКЦИЯ ПО АППАРАТНОМУ УПРАВЛЕНИЮ ESP32:\n"
           "Ты — встроенный бортовой интеллект микроконтроллера ESP32 (Xtensa Dual-Core 240MHz, 4MB Flash, ~300KB RAM, Wi-Fi).\n"
           "Ты физически работаешь внутри платы и имеешь прямой доступ к её контактам (GPIO), датчикам и интерфейсам.\n"
           "Если пользователь просит управлять платой, включить/выключить что-либо, замерить напряжение, считать датчики или узнать состояние платы, включи в свой ответ блок команды в формате:\n"
           "```action {\"action\": \"<имя_команды>\", ...}```\n\n"
           "Доступные физические команды платы:\n"
           "1. {\"action\": \"set_pin\", \"pin\": <номер GPIO>, \"value\": <0|1>} — подать логический 0 или 1 (GPIO 2 — синий бортовой светодиод).\n"
           "2. {\"action\": \"read_pin\", \"pin\": <номер GPIO>} — считать цифровое состояние пина (0 или 1).\n"
           "3. {\"action\": \"read_analog\", \"pin\": <32|33|34|35|36|39>} — замерить напряжение на аналоговом входе ADC1 (0-4095, 0-3.3 В).\n"
           "4. {\"action\": \"toggle_pin\", \"pin\": <номер GPIO>} — переключить состояние выхода на противоположное.\n"
           "5. {\"action\": \"get_telemetry\"} — считать температуру процессора, уровень свободной RAM и Wi-Fi RSSI.\n"
           "6. {\"action\": \"scan_i2c\", \"sda\": 21, \"scl\": 22} — просканировать шину I2C на наличие внешних датчиков/экранов.\n"
           "7. {\"action\": \"restart\"} — перезагрузить плату ESP32.\n\n"
           "Правила общения: Отвечай кратко, уверенно и технически грамотно. Если выполняешь действие, кратко прокомментируй его.";
}

String HardwareController::executeActionJson(const String& jsonAction) {
    if (!_enabled) {
        return "Аппаратное управление GPIO/I2C отключено пользователем в настройках.";
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, jsonAction);
    char buf[128];
    if (err) {
        snprintf(buf, sizeof(buf), "Ошибка разбора команды действия JSON: %s", err.c_str());
        return String(buf);
    }

    const char* action = doc["action"] | "";
    if (strcmp(action, "set_pin") == 0) {
        uint8_t pin = doc["pin"] | 255;
        uint8_t val = doc["value"] | 0;
        if (!isPinAllowed(pin)) {
            snprintf(buf, sizeof(buf), "Ошибка: пин GPIO %u не разрешен для управления", pin);
            return String(buf);
        }
        writePin(pin, val);
        snprintf(buf, sizeof(buf), "GPIO %u успешно установлен в %s", pin, val ? "HIGH (1)" : "LOW (0)");
        return String(buf);
    }
    else if (strcmp(action, "read_pin") == 0) {
        uint8_t pin = doc["pin"] | 255;
        if (!isPinAllowed(pin)) {
            snprintf(buf, sizeof(buf), "Ошибка: пин GPIO %u не разрешен для чтения", pin);
            return String(buf);
        }
        setPinMode(pin, INPUT);
        int val = readPin(pin);
        snprintf(buf, sizeof(buf), "Значение на цифровом GPIO %u: %d", pin, val);
        return String(buf);
    }
    else if (strcmp(action, "read_analog") == 0) {
        uint8_t pin = doc["pin"] | 255;
        if (!isPinAllowed(pin)) {
            snprintf(buf, sizeof(buf), "Ошибка: аналоговый пин GPIO %u не разрешен", pin);
            return String(buf);
        }
        int val = readAnalogPin(pin);
        if (val < 0) {
            snprintf(buf, sizeof(buf), "Ошибка чтения аналогового входа на GPIO %u", pin);
            return String(buf);
        }
        float voltage = (val / 4095.0f) * 3.3f;
        snprintf(buf, sizeof(buf), "ADC GPIO %u: %d (%.2f В)", pin, val, voltage);
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
        snprintf(buf, sizeof(buf), "Состояние GPIO %u переключено. Текущий уровень: %d", pin, current);
        return String(buf);
    }
    else if (strcmp(action, "get_telemetry") == 0) {
        return getTelemetrySummary();
    }
    else if (strcmp(action, "scan_i2c") == 0) {
        uint8_t sda = doc["sda"] | 21;
        uint8_t scl = doc["scl"] | 22;
        return scanI2C(sda, scl);
    }
    else if (strcmp(action, "restart") == 0) {
        ESP.restart();
        return "Перезагрузка ESP32...";
    }

    snprintf(buf, sizeof(buf), "Неизвестное действие: %s", action);
    return String(buf);
}
