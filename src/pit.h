#pragma once

#include <memory>
#include <optional>
#include "pitinterface.h"

struct IOInterface;

class PIT final : public PITInterface
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    PIT(IOInterface& io);
    ~PIT();

    void Reset();
    bool Tick();
    bool GetTimer2Output() const override;
};
