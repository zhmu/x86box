#pragma once

#include <memory>
#include "state.h"

class Memory;

class Disassembler final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

public:
    Disassembler();
    ~Disassembler();

    std::string Disassemble(Memory& memory, const cpu::State& state);
};
