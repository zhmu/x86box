#pragma once

#include "../interface/tickinterface.h"

struct TickProvider : TickInterface
{
    std::chrono::steady_clock::time_point initial_tp;

    TickProvider();
    ~TickProvider();
    std::chrono::nanoseconds GetTickCount() override;
};
