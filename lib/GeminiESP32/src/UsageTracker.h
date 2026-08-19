#pragma once

#include "GeminiConfig.h"

#if GEMINI_ENABLE_USAGE_TRACKER

#include <Arduino.h>
#include <Preferences.h>

/**
 * @brief Структура статистики суточного использования Gemini API
 */
struct DailyUsageStats {
    uint32_t requestsToday;      ///< Количество отправленных запросов за текущие сутки
    uint32_t promptTokensToday;  ///< Сумма входных токенов за сегодня
    uint32_t responseTokensToday;///< Сумма токенов ответа за сегодня
    uint32_t totalTokensToday;   ///< Суммарно потрачено токенов сегодня
    uint32_t dailyRequestLimit;  ///< Суточный лимит запросов (RPD)
    uint32_t requestsThisMinute; ///< Количество запросов за текущую минуту (RPM)
    uint32_t minuteLimit;        ///< Лимит запросов в минуту (RPM, обычно 15)
    int currentDayOfYear;        ///< День года (1-366) для авто-сброса в полночь
};

/**
 * @brief Класс трекера использования квот и расхода токенов Google AI Studio
 */
class UsageTracker {
public:
    UsageTracker();
    ~UsageTracker();

    bool begin();

    // Фиксация совершенного запроса
    void recordRequest(int promptTokens, int responseTokens, int totalTokens);

    // Проверка, не исчерпан ли суточный или минутный лимит
    bool canSendRequest() const;

    // Установка суточного лимита
    void setDailyLimit(uint32_t limit);

    // Установка минутного лимита (RPM)
    void setMinuteLimit(uint32_t limit);

    // Установка часового пояса (для правильного определения полуночи)
    void setTimezone(int offsetHours);

    // Получение текущей статистики
    DailyUsageStats getStats() const;

    // Сброс статистики
    void resetStats();

    // Форматированная строка статистики
    String getUsageSummary() const;

private:
    void loadFromNvs();
    void saveToNvs();
    void checkDayRollover();
    int getDayOfYear() const;

    DailyUsageStats _stats;
    int _timezoneOffset;
    unsigned long _minuteWindowStart;
};

#endif // GEMINI_ENABLE_USAGE_TRACKER
