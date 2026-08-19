#include "HardwareController.h"

#if GEMINI_ENABLE_HARDWARE

#include <Wire.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <WiFi.h>

#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif

// Список абсолютно запрещенных пинов (Flash SPI, UART0 Boot ROM)
static const uint8_t PROHIBITED_PINS[] = {6, 7, 8, 9, 10, 11, 1, 3};

HardwareController::HardwareController()
    : _enabled(true) {
}

HardwareController::~HardwareController() {
}

bool HardwareController::begin() {
    return true;
}

void HardwareController::setAllowedPins(const std::vector<uint8_t>& allowedPins) {
    _allowedPins.clear();
    for (uint8_t pin : allowedPins) {
        if (isValidGpio(pin)) {
            _allowedPins.push_back(pin);
        }
    }
}

void HardwareController::allowAllSafePins() {
    _allowedPins.clear();
}

bool HardwareController::isPinAllowed(uint8_t pin) const {
    if (!_enabled) return false;
    if (!isValidGpio(pin)) return false;
    if (_allowedPins.empty()) return true;

    for (uint8_t p : _allowedPins) {
        if (p == pin) return true;
    }
    return false;
}

bool HardwareController::isValidGpio(uint8_t pin) {
    for (size_t i = 0; i < sizeof(PROHIBITED_PINS); i++) {
        if (pin == PROHIBITED_PINS[i]) return false;
    }
    if (pin > 39) return false;
    return true;
}

bool HardwareController::setPinMode(uint8_t pin, uint8_t mode) {
    if (!isPinAllowed(pin)) return false;
    if (pin >= 34 && mode != INPUT) return false;
    pinMode(pin, mode);
    return true;
}

bool HardwareController::writePin(uint8_t pin, uint8_t value) {
    if (!isPinAllowed(pin)) return false;
    if (pin >= 34) return false;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, value ? HIGH : LOW);
    return true;
}

int HardwareController::readPin(uint8_t pin) {
    if (!isPinAllowed(pin)) return -1;
    return digitalRead(pin);
}

bool HardwareController::togglePin(uint8_t pin) {
    if (!isPinAllowed(pin)) return false;
    if (pin >= 34) return false;
    int current = digitalRead(pin);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, current ? LOW : HIGH);
    return true;
}

int HardwareController::readAnalogPin(uint8_t pin) {
    if (!isPinAllowed(pin)) return -1;
    if (pin != 32 && pin != 33 && pin != 34 && pin != 35 && pin != 36 && pin != 39) {
        return -1;
    }
    pinMode(pin, INPUT);
    return analogRead(pin);
}

DeviceTelemetry HardwareController::getTelemetry() {
    DeviceTelemetry t;
    
    // Чтение встроенного термодатчика ESP32
    #if defined(temprature_sens_read) || defined(ESP32)
    t.chipTempC = (temprature_sens_read() - 32) / 1.8f;
    #else
    t.chipTempC = 0.0f;
    #endif

    t.freeHeapBytes = ESP.getFreeHeap();
    t.minFreeHeapBytes = ESP.getMinFreeHeap();
    t.heapSizeBytes = ESP.getHeapSize();
    t.uptimeSec = millis() / 1000;
    t.wifiRssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    t.cpuFreqMHz = ESP.getCpuFreqMHz();
    t.flashSizeBytes = ESP.getFlashChipSize();

    return t;
}

String HardwareController::getTelemetrySummary() {
    DeviceTelemetry t = getTelemetry();
    char buf[256];
    snprintf(buf, sizeof(buf),
             "ESP32 [RAM: %u KB свободно / %u KB всего | CPU: %u MHz | Uptime: %u c | RSSI: %d dBm | Temp: %.1f °C]",
             t.freeHeapBytes / 1024,
             t.heapSizeBytes / 1024,
             t.cpuFreqMHz,
             t.uptimeSec,
             t.wifiRssi,
             t.chipTempC);
    return String(buf);
}

String HardwareController::scanI2C(uint8_t sda, uint8_t scl) {
    if (!_enabled) return "Аппаратный контроллер отключен.";

    Wire.begin(sda, scl);
    String result = "Сканирование I2C (SDA=" + String(sda) + ", SCL=" + String(scl) + "):\n";
    int nDevices = 0;

    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();

        if (error == 0) {
            char hexBuf[16];
            snprintf(hexBuf, sizeof(hexBuf), "0x%02X", address);
            result += " - Найден датчик/устройство по адресу: " + String(hexBuf) + "\n";
            nDevices++;
        }
    }

    if (nDevices == 0) {
        result += " Устройства I2C не обнаружены.";
    } else {
        result += "Всего устройств: " + String(nDevices);
    }

    return result;
}

String HardwareController::executeActionJson(const String& jsonAction) {
    if (!_enabled) return "Аппаратный контроллер отключен.";

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, jsonAction);
    if (err) {
        return "Ошибка парсинга действия: " + String(err.c_str());
    }

    String action = doc["action"] | "";
    if (action == "pin_write") {
        uint8_t pin = doc["pin"] | 255;
        uint8_t val = doc["value"] | 0;
        if (writePin(pin, val)) {
            return "Успешно: GPIO " + String(pin) + " установлен в " + (val ? "HIGH (1)" : "LOW (0)");
        } else {
            return "Ошибка: GPIO " + String(pin) + " запрещен или недоступен для записи";
        }
    } else if (action == "pin_read") {
        uint8_t pin = doc["pin"] | 255;
        int val = readPin(pin);
        if (val >= 0) {
            return "GPIO " + String(pin) + " значение: " + String(val);
        } else {
            return "Ошибка: GPIO " + String(pin) + " запрещен или недоступен для чтения";
        }
    } else if (action == "pin_toggle") {
        uint8_t pin = doc["pin"] | 255;
        if (togglePin(pin)) {
            return "Успешно: Состояние GPIO " + String(pin) + " переключено";
        } else {
            return "Ошибка: GPIO " + String(pin) + " запрещен";
        }
    } else if (action == "analog_read") {
        uint8_t pin = doc["pin"] | 255;
        int val = readAnalogPin(pin);
        if (val >= 0) {
            float volts = (val / 4095.0f) * 3.3f;
            char buf[64];
            snprintf(buf, sizeof(buf), "ADC GPIO %u: %d (%.2f В)", pin, val, volts);
            return String(buf);
        } else {
            return "Ошибка: Пин " + String(pin) + " не является безопасным ADC1 входом";
        }
    } else if (action == "i2c_scan") {
        uint8_t sda = doc["sda"] | 21;
        uint8_t scl = doc["scl"] | 22;
        return scanI2C(sda, scl);
    } else if (action == "get_telemetry") {
        return getTelemetrySummary();
    }

    return "Неизвестное действие: " + action;
}

String HardwareController::getHardwareCapabilitiesDescription() {
    return "ИНСТРУКЦИЯ ПО ВЗАИМОДЕЙСТВИЮ С ПЛАТОЙ ESP32:\n"
           "Ты — бортовой ИИ-ассистент микроконтроллера ESP32.\n"
           "⚠️ ПРАВИЛА:\n"
           "1. НЕ ВЫЗЫВАЙ действия на обычные приветствия и текстовые вопросы ('привет', 'как дела', 'кто ты?'). Отвечай на них ПРОСТЫМ ТЕКСТОМ!\n"
           "2. Вызывай блок ```action {...}``` ТОЛЬКО ТОГДА, КОГДА ПОЛЬЗОВАТЕЛЬ ЯВНО ПРОСИТ УПРАВЛЯТЬ ПЛАТОЙ ИЛИ СЧИТАТЬ ДАТЧИКИ.\n"
           "Доступные действия:\n"
           "- Включить/выключить GPIO: {\"action\":\"pin_write\", \"pin\": 2, \"value\": 1}\n"
           "- Переключить GPIO: {\"action\":\"pin_toggle\", \"pin\": 2}\n"
           "- Прочитать цифровой вход: {\"action\":\"pin_read\", \"pin\": 4}\n"
           "- Замерить аналоговый ADC1 вход: {\"action\":\"analog_read\", \"pin\": 34}\n"
           "- Сканировать I2C: {\"action\":\"i2c_scan\", \"sda\": 21, \"scl\": 22}\n"
           "- Телеметрия чипа: {\"action\":\"get_telemetry\"}";
}

#endif // GEMINI_ENABLE_HARDWARE
