#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

// Предварительные объявления системных классов для агентного контроля
class ConfigManager;
class NetworkManager;
class UsageTracker;

/**
 * @brief Структура расширенной аппаратной телеметрии микроконтроллера ESP32
 */
struct DeviceTelemetry {
    float chipTempC;             // Температура кремниевого кристалла ESP32 в °C
    uint32_t freeHeapBytes;      // Текущая свободная память RAM
    uint32_t minFreeHeapBytes;   // Минимальный исторический остаток свободной RAM
    uint32_t heapSizeBytes;      // Полный размер кучи (heap)
    uint32_t uptimeSec;          // Время непрерывной работы с момента запуска
    int wifiRssi;                // Уровень сигнала Wi-Fi (dBm)
    uint32_t cpuFreqMHz;         // Тактовая частота CPU (обычно 80/160/240 МГц)
    uint32_t flashSizeBytes;     // Объем Flash памяти (байты)
};

/**
 * @brief Класс безопасного аппаратного и системного контроллера микроконтроллера ESP32
 * Предоставляет ИИ-агенту полный доступ к управлению платой: GPIO, ШИМ (LEDC), ADC, I2C,
 * телеметрии, самодиагностике, сканированию Wi-Fi и настройкам NVS.
 */
class HardwareController {
public:
    HardwareController();
    ~HardwareController();

    bool begin();

    // Установка системного контекста для полного контроля конфигурации и сети
    void setSystemContext(ConfigManager* cfg, NetworkManager* net = nullptr, UsageTracker* usage = nullptr);

    // Включение / выключение аппаратного управления
    void setEnabled(bool enable) { _enabled = enable; }
    bool isEnabled() const { return _enabled; }

    // Настройка белого списка разрешенных пинов
    void setAllowedPins(const std::vector<uint8_t>& allowedPins);
    void allowAllSafePins();
    bool isPinAllowed(uint8_t pin) const;
    const std::vector<uint8_t>& getAllowedPins() const { return _allowedPins; }

    // Проверка физической допустимости пина (защита Flash SPI и UART0)
    static bool isValidGpio(uint8_t pin);

    // ─── УПРАВЛЕНИЕ GPIO И ШИМ ──────────────────────────────────────────────
    bool setPinMode(uint8_t pin, uint8_t mode);
    bool writePin(uint8_t pin, uint8_t value);
    int readPin(uint8_t pin);
    bool togglePin(uint8_t pin);
    bool pulsePin(uint8_t pin, unsigned long durationMs = 100);
    bool setPWM(uint8_t pin, uint32_t duty, uint32_t freq = 5000, uint8_t resolution = 8);
    bool playTone(uint8_t pin, unsigned int freq, unsigned long durationMs = 200);

    // Аналоговые входы (ADC1: пины 32, 33, 34, 35, 36, 39)
    int readAnalogPin(uint8_t pin);
    float readAnalogVoltage(uint8_t pin);

    // Групповое чтение и управление пинами
    String readAllAllowedPins();

    // ─── ДИАГНОСТИКА И СЕНСОРЫ ─────────────────────────────────────────────
    DeviceTelemetry getTelemetry();
    String getTelemetrySummary();
    String selfDiagnose();

    // Сканирование шины I2C с определением известных чипов
    String scanI2C(uint8_t sda = 21, uint8_t scl = 22);

    // Сканирование Wi-Fi сетей в эфире
    String scanWiFi();

    // ─── АГЕНТНЫЙ ИСПОЛНИТЕЛЬ ДЕЙСТВИЙ ──────────────────────────────────────
    String executeActionJson(const String& jsonAction);

    // Системный промпт с полным описанием агентных возможностей платы
    static String getHardwareCapabilitiesDescription();

private:
    bool _enabled;
    std::vector<uint8_t> _allowedPins;
    ConfigManager* _configMgr;
    NetworkManager* _netMgr;
    UsageTracker* _usageTracker;

    // Вспомогательная функция определения имени известного I2C чипа
    static const char* getI2CDeviceName(uint8_t address);
};
