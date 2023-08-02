#pragma once

#include <memory>
#include "state.h"

struct MemoryInterface;

class Disassembler final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

public:
    Disassembler();
    ~Disassembler();

    std::string Disassemble(MemoryInterface& memory, const cpu::State& state);
};
