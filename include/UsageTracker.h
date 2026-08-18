#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <time.h>

/**
 * @brief Структура данных для суточной статистики и лимитов использования AI
 */
struct DailyUsageStats {
    uint32_t dailyRequestLimit;    // Суточный лимит запросов (по умолчанию 1500, 0 = без ограничений)
    uint32_t requestsToday;        // Количество запросов, выполненных сегодня
    uint32_t promptTokensToday;    // Израсходовано токенов промптов сегодня
    uint32_t responseTokensToday;  // Израсходовано токенов ответов сегодня
    uint32_t totalTokensToday;     // Суммарно токенов сегодня
    
    uint32_t lifetimeRequests;     // Общее количество запросов за все время
    uint64_t lifetimeTokens;       // Общее количество токенов за все время
    
    int lastDay;                   // День года (0-365) для определения наступления полуночи
    uint32_t lastResetMillis;      // Временная метка millis() для резервного таймера суток
};

/**
 * @brief Класс учета суточных лимитов и расхода запросов/токенов
 */
class UsageTracker {
public:
    UsageTracker();

    // Инициализация NVS хранилища и загрузка сохраненной статистики
    bool begin();

    // Синхронизация реального времени по протоколу NTP (UTC+3 по умолчанию для Москвы/РФ)
    void syncNTP(int gmtOffsetHours = 3);

    // Проверка наступления новых суток (00:00) и автоматический сброс счетчиков
    void checkDayRollover();

    // Проверка, исчерпан ли суточный лимит запросов
    bool isLimitReached();

    // Регистрация выполненного запроса и сохранение в NVS
    void recordRequest(int promptTokens, int responseTokens, int totalTokens);

    // Получение текущей статистики
    DailyUsageStats getStats();

    // Установка суточного лимита запросов (0 = безлимитно)
    void setDailyLimit(uint32_t limit);

    // Ручной сброс суточных счетчиков
    void resetDailyUsage();

    // Полный сброс всей накопленной статистики
    void resetAllUsage();

    // Получение строки текущего времени (ЧЧ:ММ:СС ДД.ММ.ГГГГ)
    String getCurrentTimeString() const;

    // Расчет времени до полуночи (до сброса суточного лимита)
    String getTimeUntilMidnight() const;

    // Вывод красивого отчета по суточным лимитам и квотам
    void printQuotaReport();

private:
    Preferences _prefs;
    DailyUsageStats _stats;
    bool _ntpSynced;

    void load();
    void save();
};
