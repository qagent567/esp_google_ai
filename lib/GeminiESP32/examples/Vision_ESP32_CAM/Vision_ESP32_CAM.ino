/*
 * =============================================================================
 *  GeminiESP32 — Пример 8: Мультимодальный анализ фото (Gemini Vision)
 * =============================================================================
 *  Демонстрирует отправку изображения (JPEG) с камеры ESP32-CAM в Google Gemini Vision
 *  для распознавания объектов, лиц, считывания текста или анализа окружения.
 *
 *  Плата:    AI Thinker ESP32-CAM (или любая плата с модулем OV2640)
 *  Частота:  115200 baud
 * =============================================================================
 */

#include <WiFi.h>
#include <GeminiESP32.h>
#include "esp_camera.h"

// Назначение пинов камеры для модуля AI-THINKER ESP32-CAM
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* GEMINI_API_KEY = "YOUR_GEMINI_API_KEY";

GeminiESP32 ai(GEMINI_API_KEY, "gemini-3.5-flash-lite");

bool initCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_QVGA; // 320x240 (оптимально по скорости и размеру памяти)
    config.jpeg_quality = 12;          // Качество сжатия 10-63
    config.fb_count = 1;

    esp_err_t err = esp_camera_init(&config);
    return (err == ESP_OK);
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n╔════════════════════════════════════════════╗");
    Serial.println("║  GeminiESP32 — ESP32-CAM Vision Example    ║");
    Serial.println("╚════════════════════════════════════════════╝\n");

    if (!initCamera()) {
        Serial.println("❌ Ошибка инициализации модуля камеры OV2640!");
    } else {
        Serial.println("✓ Модуль камеры успешно инициализирован.");
    }

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Подключение к Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\n✓ Подключено! IP: %s\n\n", WiFi.localIP().toString().c_str());

    ai.begin();
    Serial.println("✓ Нажмите Enter или отправьте промпт в Serial для снимка и анализа:");
}

void loop() {
    if (Serial.available()) {
        String prompt = Serial.readStringUntil('\n');
        prompt.trim();
        if (prompt.length() == 0) {
            prompt = "Подробно опиши, что ты видишь на этой фотографии.";
        }

        Serial.printf("\n📸 Делаем снимок...\n");
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("❌ Не удалось захватить кадр с камеры!");
            return;
        }

        Serial.printf("✓ Снимок сделан (%u байт). Отправляем в Gemini Vision...\n", (unsigned int)fb->len);
        Serial.printf("[Вопрос]: %s\n", prompt.c_str());

        // Отправка снимка в Gemini Vision
        GeminiResponse resp = ai.queryWithImage(prompt, fb->buf, fb->len, "image/jpeg");

        // Освобождаем буфер кадра
        esp_camera_fb_return(fb);

        Serial.println("\n───────────────────────────────────────");
        Serial.println("[Ответ Gemini Vision]:");
        Serial.println(resp.text);
        Serial.printf("📊 [Статистика]: %lu мс, %d токенов\n", resp.durationMs, resp.totalTokens);
        Serial.println("───────────────────────────────────────\n");
    }
}
