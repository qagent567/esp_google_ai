#include "UsageTracker.h"

// Цветовые ANSI-коды
#define ANSI_RESET   ""
#define ANSI_BOLD    ""
#define ANSI_RED     ""
#define ANSI_GREEN   ""
#define ANSI_YELLOW  ""
#define ANSI_CYAN    ""

// Значение суточного лимита по умолчанию (Google AI Studio Free Tier для Flash/Lite: 1500 RPD)
static const uint32_t DEFAULT_DAILY_LIMIT = 1500;

UsageTracker::UsageTracker() : _ntpSynced(false) {
    _stats.dailyRequestLimit = DEFAULT_DAILY_LIMIT;
    _stats.requestsToday = 0;
    _stats.promptTokensToday = 0;
    _stats.responseTokensToday = 0;
    _stats.totalTokensToday = 0;
    _stats.lifetimeRequests = 0;
    _stats.lifetimeTokens = 0;
    _stats.lastDay = -1;
    _stats.lastResetMillis = 0;
}

bool UsageTracker::begin() {
    _stats.lastResetMillis = millis();
    load();
    checkDayRollover();
    return true;
}

void UsageTracker::syncNTP(int gmtOffsetHours) {
    configTime(gmtOffsetHours * 3600, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
    _ntpSynced = true;
    checkDayRollover();
}

void UsageTracker::load() {
    if (_prefs.begin("ai_usage", true)) {
        _stats.dailyRequestLimit = _prefs.getUInt("limit", DEFAULT_DAILY_LIMIT);
        _stats.requestsToday = _prefs.getUInt("req_day", 0);
        _stats.promptTokensToday = _prefs.getUInt("tok_in", 0);
        _stats.responseTokensToday = _prefs.getUInt("tok_out", 0);
        _stats.totalTokensToday = _prefs.getUInt("tok_day", 0);
        _stats.lifetimeRequests = _prefs.getUInt("req_life", 0);
        _stats.lifetimeTokens = _prefs.getULong64("tok_life", 0);
        _stats.lastDay = _prefs.getInt("last_day", -1);
        _prefs.end();
    }
}

void UsageTracker::save() {
    if (_prefs.begin("ai_usage", false)) {
        _prefs.putUInt("limit", _stats.dailyRequestLimit);
        _prefs.putUInt("req_day", _stats.requestsToday);
        _prefs.putUInt("tok_in", _stats.promptTokensToday);
        _prefs.putUInt("tok_out", _stats.responseTokensToday);
        _prefs.putUInt("tok_day", _stats.totalTokensToday);
        _prefs.putUInt("req_life", _stats.lifetimeRequests);
        _prefs.putULong64("tok_life", _stats.lifetimeTokens);
        _prefs.putInt("last_day", _stats.lastDay);
        _prefs.end();
    }
}

void UsageTracker::checkDayRollover() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    bool hasValidTime = (localtime_r(&now, &timeinfo) && timeinfo.tm_year > (2020 - 1900));

    if (hasValidTime) {
        int currentDay = timeinfo.tm_yday; // День года 0-365
        if (_stats.lastDay == -1) {
            // Первый запуск с валидным временем
            _stats.lastDay = currentDay;
            save();
        } else if (_stats.lastDay != currentDay) {
            // Наступили новые сутки (полночь)
            resetDailyUsage();
            _stats.lastDay = currentDay;
            save();
        }
    } else {
        // Резервный таймер по millis() (24 часа) если NTP недоступен
        if (millis() - _stats.lastResetMillis >= 86400000UL) {
            resetDailyUsage();
            _stats.lastResetMillis = millis();
            save();
        }
    }
}

bool UsageTracker::isLimitReached() {
    checkDayRollover();
    if (_stats.dailyRequestLimit == 0) return false; // Безлимитный режим
    return (_stats.requestsToday >= _stats.dailyRequestLimit);
}

void UsageTracker::recordRequest(int promptTokens, int responseTokens, int totalTokens) {
    checkDayRollover();
    _stats.requestsToday++;
    _stats.promptTokensToday += promptTokens;
    _stats.responseTokensToday += responseTokens;
    _stats.totalTokensToday += totalTokens;

    _stats.lifetimeRequests++;
    _stats.lifetimeTokens += totalTokens;

    save();
}

DailyUsageStats UsageTracker::getStats() {
    checkDayRollover();
    return _stats;
}

void UsageTracker::setDailyLimit(uint32_t limit) {
    _stats.dailyRequestLimit = limit;
    save();
}

void UsageTracker::resetDailyUsage() {
    _stats.requestsToday = 0;
    _stats.promptTokensToday = 0;
    _stats.responseTokensToday = 0;
    _stats.totalTokensToday = 0;
    _stats.lastResetMillis = millis();
    save();
}

void UsageTracker::resetAllUsage() {
    resetDailyUsage();
    _stats.lifetimeRequests = 0;
    _stats.lifetimeTokens = 0;
    save();
}

String UsageTracker::getCurrentTimeString() const {
    time_t now = time(nullptr);
    struct tm timeinfo;
    if (localtime_r(&now, &timeinfo) && timeinfo.tm_year > (2020 - 1900)) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d  %02d.%02d.%04d", 
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                 timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
        return String(buf);
    }
    return "Синхронизация NTP...";
}

String UsageTracker::getTimeUntilMidnight() const {
    time_t now = time(nullptr);
    struct tm timeinfo;
    if (localtime_r(&now, &timeinfo) && timeinfo.tm_year > (2020 - 1900)) {
        int hoursLeft = 23 - timeinfo.tm_hour;
        int minsLeft = 59 - timeinfo.tm_min;
        int secsLeft = 60 - timeinfo.tm_sec;
        if (secsLeft == 60) { secsLeft = 0; minsLeft++; }
        if (minsLeft == 60) { minsLeft = 0; hoursLeft++; }
        char buf[32];
        snprintf(buf, sizeof(buf), "%02d ч %02d мин %02d сек", hoursLeft, minsLeft, secsLeft);
        return String(buf);
    }
    return "в 00:00 (по UTC/NTP)";
}

void UsageTracker::printQuotaReport() {
    checkDayRollover();

    uint32_t limit = _stats.dailyRequestLimit;
    uint32_t used = _stats.requestsToday;
    int32_t remaining = (limit > 0) ? ((int32_t)limit - (int32_t)used) : -1;
    if (remaining < 0 && limit > 0) remaining = 0;

    float percentUsed = (limit > 0) ? ((float)used / (float)limit * 100.0f) : 0.0f;
    float percentRemaining = (limit > 0) ? (100.0f - percentUsed) : 100.0f;
    if (percentRemaining < 0.0f) percentRemaining = 0.0f;

    Serial.println(ANSI_CYAN "\n================== СУТОЧНЫЕ ЛИМИТЫ И РАСХОД (QUOTA) ==================" ANSI_RESET);
    Serial.printf(" [ВРЕМЯ УСТРОЙСТВА]   : %s\n", getCurrentTimeString().c_str());
    if (limit > 0) {
        Serial.printf(" [СУТОЧНЫЙ ЛИМИТ]     : %u запросов / сутки (RPD)\n", limit);
        Serial.printf(" [ИЗРАСХОДОВАНО СЕГОДНЯ]: " ANSI_BOLD "%u" ANSI_RESET " запросов (%.1f%%)\n", used, percentUsed);
        if (remaining > 0) {
            Serial.printf(" [ОСТАЛОСЬ НА СЕГОДНЯ]: " ANSI_GREEN ANSI_BOLD "%u" ANSI_RESET " запросов (%.1f%%)\n", remaining, percentRemaining);
        } else {
            Serial.printf(" [ОСТАЛОСЬ НА СЕГОДНЯ]: " ANSI_RED ANSI_BOLD "0" ANSI_RESET " (ЛИМИТ ИСЧЕРПАН!)\n");
        }
    } else {
        Serial.println(" [СУТОЧНЫЙ ЛИМИТ]     : БЕЗЛИМИТНЫЙ РЕЖИМ (0 RPD)");
        Serial.printf(" [ИЗРАСХОДОВАНО СЕГОДНЯ]: %u запросов\n", used);
    }
    Serial.println(ANSI_CYAN "----------------------------------------------------------------------" ANSI_RESET);
    Serial.printf(" [ТОКЕНЫ СЕГОДНЯ]     : Всего: %u (Запрос: %u, Ответ: %u)\n", 
                  _stats.totalTokensToday, _stats.promptTokensToday, _stats.responseTokensToday);
    Serial.printf(" [ЗА ВСЕ ВРЕМЯ]       : Всего запросов: %u | Всего токенов: %llu\n", 
                  _stats.lifetimeRequests, _stats.lifetimeTokens);
    Serial.printf(" [СБРОС СУТОК ЧЕРЕЗ]  : %s\n", getTimeUntilMidnight().c_str());
    Serial.println(ANSI_CYAN "----------------------------------------------------------------------" ANSI_RESET);
    Serial.println(ANSI_YELLOW " Команды управления квотой:" ANSI_RESET);
    Serial.println("  • set limit <число>   - изменить суточный лимит (напр. 'set limit 1500', 0 = выкл)");
    Serial.println("  • reset quota         - сбросить счетчики сегодняшнего дня");
    Serial.println("  • reset allquota      - сбросить всю статистику (включая общую за все время)");
    Serial.println(ANSI_CYAN "======================================================================\n" ANSI_RESET);
}
