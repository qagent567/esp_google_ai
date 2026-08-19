#include "UsageTracker.h"

#if GEMINI_ENABLE_USAGE_TRACKER

#include <time.h>
#include <sys/time.h>

static const char* NVS_NAMESPACE = "gemini_usage";

UsageTracker::UsageTracker()
    : _timezoneOffset(3), _minuteWindowStart(0) {
    memset(&_stats, 0, sizeof(_stats));
    _stats.dailyRequestLimit = 500;
    _stats.minuteLimit = 15;
    _stats.currentDayOfYear = -1;
}

UsageTracker::~UsageTracker() {
}

bool UsageTracker::begin() {
    loadFromNvs();
    checkDayRollover();
    return true;
}

int UsageTracker::getDayOfYear() const {
    time_t now = time(nullptr);
    if (now < 100000) {
        return -1;
    }
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    return timeinfo.tm_yday;
}

void UsageTracker::checkDayRollover() {
    int day = getDayOfYear();
    if (day < 0) return;

    if (_stats.currentDayOfYear != day) {
        _stats.requestsToday = 0;
        _stats.promptTokensToday = 0;
        _stats.responseTokensToday = 0;
        _stats.totalTokensToday = 0;
        _stats.currentDayOfYear = day;
        saveToNvs();
    }
}

void UsageTracker::recordRequest(int promptTokens, int responseTokens, int totalTokens) {
    checkDayRollover();

    unsigned long now = millis();
    if (now - _minuteWindowStart >= 60000) {
        _minuteWindowStart = now;
        _stats.requestsThisMinute = 0;
    }

    _stats.requestsToday++;
    _stats.requestsThisMinute++;
    _stats.promptTokensToday += promptTokens;
    _stats.responseTokensToday += responseTokens;
    _stats.totalTokensToday += totalTokens;

    saveToNvs();
}

bool UsageTracker::canSendRequest() const {
    unsigned long now = millis();
    if (now - _minuteWindowStart < 60000) {
        if (_stats.requestsThisMinute >= _stats.minuteLimit) {
            return false;
        }
    }

    if (_stats.dailyRequestLimit > 0 && _stats.requestsToday >= _stats.dailyRequestLimit) {
        return false;
    }

    return true;
}

void UsageTracker::setDailyLimit(uint32_t limit) {
    _stats.dailyRequestLimit = limit;
    saveToNvs();
}

void UsageTracker::setMinuteLimit(uint32_t limit) {
    _stats.minuteLimit = limit;
    saveToNvs();
}

void UsageTracker::setTimezone(int offsetHours) {
    _timezoneOffset = offsetHours;
}

DailyUsageStats UsageTracker::getStats() const {
    DailyUsageStats s = _stats;
    unsigned long now = millis();
    if (now - _minuteWindowStart >= 60000) {
        s.requestsThisMinute = 0;
    }
    return s;
}

void UsageTracker::resetStats() {
    _stats.requestsToday = 0;
    _stats.promptTokensToday = 0;
    _stats.responseTokensToday = 0;
    _stats.totalTokensToday = 0;
    _stats.requestsThisMinute = 0;
    saveToNvs();
}

void UsageTracker::loadFromNvs() {
    Preferences p;
    if (p.begin(NVS_NAMESPACE, true)) {
        _stats.requestsToday = p.getUInt("req_today", 0);
        _stats.promptTokensToday = p.getUInt("p_tok_today", 0);
        _stats.responseTokensToday = p.getUInt("r_tok_today", 0);
        _stats.totalTokensToday = p.getUInt("t_tok_today", 0);
        _stats.dailyRequestLimit = p.getUInt("d_limit", 500);
        if (_stats.dailyRequestLimit == 1500) {
            _stats.dailyRequestLimit = 500;
        }
        _stats.minuteLimit = p.getUInt("m_limit", 15);
        _stats.currentDayOfYear = p.getInt("day_of_year", -1);
        p.end();
    }
}

void UsageTracker::saveToNvs() {
    Preferences p;
    if (p.begin(NVS_NAMESPACE, false)) {
        p.putUInt("req_today", _stats.requestsToday);
        p.putUInt("p_tok_today", _stats.promptTokensToday);
        p.putUInt("r_tok_today", _stats.responseTokensToday);
        p.putUInt("t_tok_today", _stats.totalTokensToday);
        p.putUInt("d_limit", _stats.dailyRequestLimit);
        p.putUInt("m_limit", _stats.minuteLimit);
        p.putInt("day_of_year", _stats.currentDayOfYear);
        p.end();
    }
}

String UsageTracker::getUsageSummary() const {
    DailyUsageStats s = getStats();
    char buf[256];
    snprintf(buf, sizeof(buf),
             "Запросы сегодня: %u / %u (RPM: %u / %u) | Токены: %u (Вход: %u, Выход: %u)",
             s.requestsToday, s.dailyRequestLimit,
             s.requestsThisMinute, s.minuteLimit,
             s.totalTokensToday, s.promptTokensToday, s.responseTokensToday);
    return String(buf);
}

#endif // GEMINI_ENABLE_USAGE_TRACKER
