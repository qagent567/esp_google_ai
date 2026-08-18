#include "HardwareController.h"
#include <WiFi.h>
#include <Wire.h>
#include <esp_system.h>

#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read(); // Декларация встроенного датчика температуры ESP32
#ifdef __cplusplus
}
#endif

HardwareController::HardwareController() {}

void HardwareController::begin() {
    // Встроенный LED на ESP32 (обычно GPIO 2) настраиваем на выход в состоянии LOW
    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);
}

bool HardwareController::isValidGpio(uint8_t pin) {
    // На ESP32 безопасные для управления пользователем GPIO: 2, 4, 5, 12, 13, 14, 15, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33
    // Пины только на вход (input-only): 34, 35, 36, 39
    // Пины Flash (6-11) и UART0 (1, 3) трогать нельзя во избежание сбоя прошивки
    if (pin == 1 || pin == 3) return false; // UART0
    if (pin >= 6 && pin <= 11) return false; // SPI Flash
    if (pin > 39) return false;
    return true;
}

bool HardwareController::setPinMode(uint8_t pin, uint8_t mode) {
    if (!isValidGpio(pin)) return false;
    if ((pin >= 34 && pin <= 39) && mode == OUTPUT) {
        // Пины 34-39 на ESP32 физически работают только на вход (Input Only)
        return false;
    }
    pinMode(pin, mode);
    return true;
}

bool HardwareController::writePin(uint8_t pin, uint8_t val) {
    if (!isValidGpio(pin)) return false;
    if (pin >= 34 && pin <= 39) return false; // Input Only
    pinMode(pin, OUTPUT);
    digitalWrite(pin, (val > 0) ? HIGH : LOW);
    return true;
}

int HardwareController::readPin(uint8_t pin) {
    if (!isValidGpio(pin)) return -1;
    return digitalRead(pin);
}

int HardwareController::readAnalogPin(uint8_t pin) {
    // Аналоговые входы ADC1 (работают при включенном Wi-Fi): GPIO 32, 33, 34, 35, 36, 39
    // ADC2 пины (0, 2, 4, 12-15, 25-27) могут конфликтовать с Wi-Fi driver
    if (pin == 32 || pin == 33 || pin == 34 || pin == 35 || pin == 36 || pin == 39 ||
        pin == 2 || pin == 4 || pin == 12 || pin == 13 || pin == 14 || pin == 15 || pin == 25 || pin == 26 || pin == 27) {
        return analogRead(pin);
    }
    return -1;
}

bool HardwareController::togglePin(uint8_t pin) {
    if (!isValidGpio(pin) || (pin >= 34 && pin <= 39)) return false;
    pinMode(pin, OUTPUT);
    int current = digitalRead(pin);
    digitalWrite(pin, current == HIGH ? LOW : HIGH);
    return true;
}

float HardwareController::getChipTemperature() {
    // В ESP32 Arduino Core доступен вызов temperatureRead()
    #if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 4)
        return temperatureRead();
    #else
        // Конвертация сырого значения сенсора в градусы Цельсия
        return (temprature_sens_read() - 32) / 1.8f;
    #endif
}

DeviceTelemetry HardwareController::getTelemetry() {
    DeviceTelemetry t;
    t.chipTempC = getChipTemperature();
    t.freeHeapBytes = ESP.getFreeHeap();
    t.minFreeHeapBytes = ESP.getMinFreeHeap();
    t.uptimeSec = millis() / 1000;
    t.wifiRssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
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
    Wire.begin(sda, scl);
    String result = "Сканирование шины I2C (SDA=" + String(sda) + ", SCL=" + String(scl) + "):\n";
    int nDevices = 0;

    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();

        if (error == 0) {
            char hexBuf[16];
            snprintf(hexBuf, sizeof(hexBuf), "0x%02X", address);
            result += " - Найдено устройство по адресу " + String(hexBuf) + "\n";
            nDevices++;
        }
    }

    if (nDevices == 0) {
        result += " Устройства I2C не обнаружены.";
    } else {
        result += " Всего обнаружено устройств: " + String(nDevices);
    }
    return result;
}

String HardwareController::getHardwareCapabilitiesDescription() {
    return "Вы можете управлять аппаратной частью микроконтроллера ESP32. "
           "Если пользователь просит переключить пин, замерить датчик или получить телеметрию, "
           "включите в ответ блок вызова действия в формате JSON: ```action {\"action\":\"<имя>\", ...}```\n"
           "Доступные действия:\n"
           "1. {\"action\": \"set_pin\", \"pin\": <номер GPIO>, \"value\": <0 или 1>}\n"
           "2. {\"action\": \"read_pin\", \"pin\": <номер GPIO>}\n"
           "3. {\"action\": \"read_analog\", \"pin\": <номер ADC GPIO, напр. 34>}\n"
           "4. {\"action\": \"toggle_pin\", \"pin\": <номер GPIO>}\n"
           "5. {\"action\": \"get_telemetry\"} (температура кристалла, память RAM, Wi-Fi)\n"
           "6. {\"action\": \"scan_i2c\", \"sda\": 21, \"scl\": 22}\n"
           "7. {\"action\": \"restart\"} (перезагрузка ESP32)\n"
           "Встроенный светодиод ESP32 подключен к GPIO 2.";
}

String HardwareController::executeActionJson(const String& jsonAction) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, jsonAction);
    if (err) {
        return "Ошибка разбора команды действия JSON: " + String(err.c_str());
    }

    const char* action = doc["action"] | "";
    if (strcmp(action, "set_pin") == 0) {
        uint8_t pin = doc["pin"] | 255;
        uint8_t val = doc["value"] | 0;
        if (!isValidGpio(pin)) return "Ошибка: недопустимый номер GPIO " + String(pin);
        writePin(pin, val);
        return "GPIO " + String(pin) + " успешно установлен в " + (val ? "HIGH (1)" : "LOW (0)");
    }
    else if (strcmp(action, "read_pin") == 0) {
        uint8_t pin = doc["pin"] | 255;
        if (!isValidGpio(pin)) return "Ошибка: недопустимый номер GPIO " + String(pin);
        setPinMode(pin, INPUT);
        int val = readPin(pin);
        return "Значение на цифровом GPIO " + String(pin) + ": " + String(val);
    }
    else if (strcmp(action, "read_analog") == 0) {
        uint8_t pin = doc["pin"] | 255;
        int val = readAnalogPin(pin);
        if (val < 0) return "Ошибка чтения аналогового входа на GPIO " + String(pin);
        float voltage = (val / 4095.0f) * 3.3f;
        char buf[64];
        snprintf(buf, sizeof(buf), "ADC GPIO %u: %d (%.2f В)", pin, val, voltage);
        return String(buf);
    }
    else if (strcmp(action, "toggle_pin") == 0) {
        uint8_t pin = doc["pin"] | 2;
        if (!isValidGpio(pin)) return "Ошибка: недопустимый номер GPIO " + String(pin);
        togglePin(pin);
        int current = readPin(pin);
        return "Состояние GPIO " + String(pin) + " переключено. Текущий уровень: " + String(current);
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

    return "Неизвестное действие: " + String(action);
}
