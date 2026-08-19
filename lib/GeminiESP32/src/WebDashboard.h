#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>

class GeminiESP32;

/**
 * @brief Встроенный Web Dashboard для GeminiESP32
 * Предоставляет локальный веб-интерфейс (чат, мониторинг квот, управление GPIO).
 */
class WebDashboard {
public:
    WebDashboard();
    ~WebDashboard();

    /**
     * @brief Запуск Web Dashboard сервера
     * @param ai Указатель на экземпляр GeminiESP32
     * @param port Порт сервера (по умолчанию 80)
     * @param inBackground Запустить сервер в отдельном FreeRTOS потоке (не требует вызова handle())
     */
    bool begin(GeminiESP32* ai, uint16_t port = 80, bool inBackground = true);

    /**
     * @brief Остановка веб-сервера
     */
    void stop();

    /**
     * @brief Ручная обработка входящих запросов (если inBackground = false)
     */
    void handle();

    /**
     * @brief Проверка, запущен ли сервер
     */
    bool isRunning() const { return _running; }

private:
    void setupRoutes();
    void handleRoot();
    void handleStatus();
    void handleChat();
    void handleGpio();

    WebServer* _server;
    GeminiESP32* _ai;
    uint16_t _port;
    bool _running;
    bool _inBackground;
    TaskHandle_t _taskHandle;
};
