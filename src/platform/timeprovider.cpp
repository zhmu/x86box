#include "timeprovider.h"
#include <ctime>

TimeProvider::~TimeProvider() = default;

LocalTime TimeProvider::GetLocalTime()
{
    const auto t = time(nullptr);
    const auto tm = localtime(&t);
    return {
        .seconds = tm->tm_sec,
        .minutes = tm->tm_min,
        .hours = tm->tm_hour,
        .week_day = tm->tm_wday + 1,
        .day = tm->tm_mday,
        .month = tm->tm_mon + 1,
        .year = tm->tm_year + 1900
    };
}