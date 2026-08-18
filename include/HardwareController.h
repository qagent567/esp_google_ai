#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @brief Структура телеметрии состояния аппаратной части ESP32
 */
struct DeviceTelemetry {
    float chipTempC;          // Температура кристалла ESP32 в градусах Цельсия
    uint32_t freeHeapBytes;   // Свободная оперативная память (RAM)
    uint32_t minFreeHeapBytes;// Минимальный зафиксированный уровень свободной RAM
    uint32_t uptimeSec;       // Время непрерывной работы с момента включения (секунды)
    int wifiRssi;             // Уровень сигнала Wi-Fi (dBm)
    uint32_t cpuFreqMHz;      // Частота процессора Xtensa (МГц)
    uint32_t flashSizeBytes;  // Общий размер Flash-памяти
};

/**
 * @brief Класс для управления аппаратными ресурсами ESP32 и выполнения команд от нейросети
 */
class HardwareController {
public:
    HardwareController();
    void begin();

    // --- Управление GPIO ---
    bool setPinMode(uint8_t pin, uint8_t mode);
    bool writePin(uint8_t pin, uint8_t val);
    int readPin(uint8_t pin);
    int readAnalogPin(uint8_t pin);
    bool togglePin(uint8_t pin);

    // --- Телеметрия и датчики ---
    DeviceTelemetry getTelemetry();
    String getTelemetrySummary();
    float getChipTemperature();

    // --- I2C сканирование ---
    String scanI2C(uint8_t sda = 21, uint8_t scl = 22);

    // --- Выполнение структурированных команд от Gemini AI ---
    // Обрабатывает вызов действия в формате JSON и возвращает текстовый результат выполнения
    String executeActionJson(const String& jsonAction);

    // Генерация описания возможностей железа для системного промпта Gemini
    static String getHardwareCapabilitiesDescription();

    // Вспомогательная проверка валидности номера GPIO пина на ESP32
    static bool isValidGpio(uint8_t pin);
};
