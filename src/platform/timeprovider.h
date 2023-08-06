#pragma once

#include "../interface/timeinterface.h"

struct TimeProvider : TimeInterface
{
    ~TimeProvider();

    LocalTime GetLocalTime() override;
};