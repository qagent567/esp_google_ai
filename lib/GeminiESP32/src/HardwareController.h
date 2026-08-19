#pragma once

#include "GeminiConfig.h"

#if GEMINI_ENABLE_HARDWARE

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

/**
 * @brief Структура аппаратной телеметрии микроконтроллера ESP32
 */
struct DeviceTelemetry {
    float chipTempC;             ///< Температура кремниевого кристалла ESP32 в °C
    uint32_t freeHeapBytes;      ///< Текущая свободная оперативная память RAM
    uint32_t minFreeHeapBytes;   ///< Минимальный исторический остаток свободной RAM
    uint32_t heapSizeBytes;      ///< Полный размер кучи (heap)
    uint32_t uptimeSec;          ///< Время непрерывной работы с момента запуска в секундах
    int wifiRssi;                ///< Уровень сигнала Wi-Fi (dBm)
    uint32_t cpuFreqMHz;         ///< Тактовая частота CPU (обычно 240 МГц)
    uint32_t flashSizeBytes;     ///< Объем Flash памяти (байты)
};

/**
 * @brief Класс безопасного аппаратного контроллера микроконтроллера ESP32
 * Предоставляет функции прямого управления GPIO, чтение ADC1, температуру и интерфейс I2C.
 * Поддерживает белые списки пинов и полное отключение управления для бесконфликтной интеграции.
 */
class HardwareController {
public:
    HardwareController();
    ~HardwareController();

    bool begin();

    // Включение / выключение аппаратного управления
    void setEnabled(bool enable) { _enabled = enable; }
    bool isEnabled() const { return _enabled; }

    // Настройка белого списка разрешенных пинов (если список пуст — разрешены все безопасные GPIO)
    void setAllowedPins(const std::vector<uint8_t>& allowedPins);
    void allowAllSafePins();
    bool isPinAllowed(uint8_t pin) const;

    // Проверка физической допустимости пина (защита Flash SPI и UART0)
    static bool isValidGpio(uint8_t pin);

    // Безопасное управление цифровыми пинами (возвращают true при успехе)
    bool setPinMode(uint8_t pin, uint8_t mode);
    bool writePin(uint8_t pin, uint8_t value);
    int readPin(uint8_t pin);
    bool togglePin(uint8_t pin);

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

private:
    bool _enabled;
    std::vector<uint8_t> _allowedPins;
};

#endif // GEMINI_ENABLE_HARDWARE
