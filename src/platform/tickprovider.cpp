#include "tickprovider.h"

TickProvider::TickProvider()
    : initial_tp(std::chrono::steady_clock::now())
{
}

TickProvider::~TickProvider() = default;

std::chrono::nanoseconds TickProvider::GetTickCount()
{
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now - initial_tp);
}