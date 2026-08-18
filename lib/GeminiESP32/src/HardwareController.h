#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @brief Структура аппаратной телеметрии микроконтроллера ESP32
 */
struct DeviceTelemetry {
    float chipTempC;             // Температура кремниевого кристалла ESP32 в °C
    uint32_t freeHeapBytes;      // Текущая свободная память RAM
    uint32_t minFreeHeapBytes;   // Минимальный исторический остаток свободной RAM
    uint32_t heapSizeBytes;      // Полный размер кучи (heap)
    uint32_t uptimeSec;          // Время непрерывной работы с момента запуска
    int wifiRssi;                // Уровень сигнала Wi-Fi (dBm)
    uint32_t cpuFreqMHz;         // Тактовая частота CPU (обычно 240 МГц)
    uint32_t flashSizeBytes;     // Объем Flash памяти (байты)
};

/**
 * @brief Класс безопасного аппаратного контроллера микроконтроллера ESP32
 * Предоставляет функции прямого управления GPIO, чтение ADC1, температуру и интерфейс I2C.
 */
class HardwareController {
public:
    HardwareController();
    ~HardwareController();

    bool begin();

    // Безопасное управление цифровыми пинами (GPIO)
    bool isValidGpio(uint8_t pin) const;
    void setPinMode(uint8_t pin, uint8_t mode);
    void writePin(uint8_t pin, uint8_t value);
    int readPin(uint8_t pin);
    void togglePin(uint8_t pin);

    // Аналоговые входы (ADC1: пины 32, 33, 34, 35, 36, 39)
    int readAnalogPin(uint8_t pin);

    // Телеметрия системы и чипа
    DeviceTelemetry getTelemetry();
    String getTelemetrySummary();

    // Сканирование шины I2C
    String scanI2C(uint8_t sda = 21, uint8_t scl = 22);

    // Исполнитель структурированных JSON действий от AI
    String executeActionJson(const String& jsonAction);

    // Формирование текста аппаратных возможностей для системного промпта
    static String getHardwareCapabilitiesDescription();
};
