#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <time.h>

/**
 * @brief Структура суточной статистики использования Gemini API
 */
struct DailyUsageStats {
    uint32_t dailyRequestLimit;   // Суточный лимит запросов (по умолчанию 1500 для Free Tier Gemini Flash)
    uint32_t requestsToday;       // Количество запросов, сделанных за текущие сутки
    uint32_t promptTokensToday;   // Входные токены за сегодня
    uint32_t responseTokensToday; // Выходные токены за сегодня
    uint32_t totalTokensToday;    // Суммарные токены за сегодня
    uint32_t lifetimeRequests;    // Всего запросов за все время
    uint64_t lifetimeTokens;      // Всего токенов за все время
    int lastDay;                  // День года (0-365) для отслеживания смены суток
    unsigned long lastResetMillis;// Время последнего сброса по таймеру millis() (fallback)
};

/**
 * @brief Класс учета расхода суточных квот и токенов Gemini API
 */
class UsageTracker {
public:
    UsageTracker();

    bool begin();

    // Синхронизация NTP для точного сброса в 00:00 местного времени
    void syncNTP(int gmtOffsetHours = 3);

    // Проверка наступления новых суток и сброс суточных счетчиков
    void checkDayRollover();

    // Проверка, не исчерпан ли суточный лимит
    bool isLimitReached();

    // Учет выполненного запроса
    void recordRequest(int promptTokens, int responseTokens, int totalTokens);

    // Получение текущей статистики
    DailyUsageStats getStats();

    // Установка суточного лимита
    void setDailyLimit(uint32_t limit);

    // Ручной сброс суточных счетчиков
    void resetDailyUsage();

    // Полный сброс всей статистики
    void resetAllUsage();

    // Форматированная строка времени устройства
    String getCurrentTimeString() const;

    // Время до сброса суточного лимита (в 00:00)
    String getTimeUntilMidnight() const;

    // Вывод красивого отчета о квотах в Serial
    void printQuotaReport();

private:
    void load();
    void save();

    Preferences _prefs;
    DailyUsageStats _stats;
    bool _ntpSynced;
};
