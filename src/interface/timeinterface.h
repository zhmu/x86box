#pragma once

struct LocalTime
{
    int seconds; // 0..60
    int minutes; // 0..59
    int hours; // 0..23
    int week_day; // 1..7, 1: sunday
    int day; // 1..31
    int month; // 1..12
    int year; // 2000...

};

struct TimeInterface
{
    virtual ~TimeInterface() = default;

    virtual LocalTime GetLocalTime() = 0;
};