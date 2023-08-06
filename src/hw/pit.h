#pragma once

#include <memory>
#include <optional>
#include "../interface/pitinterface.h"

struct IOInterface;
struct TickInterface;

class PIT final : public PITInterface
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    PIT(IOInterface& io, TickInterface& tick);
    ~PIT();

    void Reset();
    bool Tick();
    bool GetTimer2Output() const override;
};
