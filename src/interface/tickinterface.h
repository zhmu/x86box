#pragma once

#include <chrono>

struct TickInterface
{
    virtual ~TickInterface() = default;
    virtual std::chrono::nanoseconds GetTickCount() = 0;
};