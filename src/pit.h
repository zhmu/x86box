#pragma once

#include <memory>
#include <optional>

class IO;

class PIT final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    PIT(IO& io);
    ~PIT();

    void Reset();
    bool Tick();
    bool GetTimer2Output() const;
};