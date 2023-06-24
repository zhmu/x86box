#pragma once

#include <memory>

class IO;
class PIT;

class PPI final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    PPI(IO&, PIT&);
    ~PPI();

    void Reset();
};
