#include "disassembler.h"

#include <iomanip>
#include <sstream>
#include <capstone/capstone.h>
#include "../cpu/cpux86.h"
#include "../interface/memoryinterface.h"

namespace
{
    constexpr auto max_instruction_bytes = 8;
}

struct Disassembler::Impl
{
    csh handle;

    Impl();
    ~Impl();
};

Disassembler::Impl::Impl()
{
    if (cs_open(CS_ARCH_X86, CS_MODE_16, &handle) != CS_ERR_OK)
        throw std::runtime_error("cannot open capstone handle");
}

Disassembler::Impl::~Impl()
{
    cs_close(&handle);
}

Disassembler::Disassembler()
    : impl(std::make_unique<Impl>())
{
}

Disassembler::~Disassembler() = default;

std::string Disassembler::Disassemble(MemoryInterface& memory, const cpu::State& state)
{
    const auto cs = state.m_cs;
    const auto ip = state.m_ip;

    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(4)
       << cs << ":" << ip << " ";

    auto add_hex_bytes = [&](const uint8_t* ptr, size_t num_bytes) {
        for (int n = 0; n < num_bytes; ++n) {
            ss << std::setfill('0') << std::setw(2) << static_cast<uint32_t>(ptr[n]);
        }
    };

    auto add_padding_bytes = [&](size_t current_num_bytes) {
        for (auto n = current_num_bytes; n < max_instruction_bytes; ++n) {
            ss << "  ";
        }
    };

    const auto addr = CPUx86::MakeAddr(cs, ip);
    if (auto ptr = static_cast<const uint8_t*>(memory.GetPointer(addr, max_instruction_bytes)); ptr) {
        cs_insn* insn;
        if (const auto count = cs_disasm(impl->handle, ptr, max_instruction_bytes, ip, 1, &insn); count > 0)
        {
            const auto& instr = insn[0];
            add_hex_bytes(ptr, instr.size);
            add_padding_bytes(instr.size);
            ss << " ";
            ss << instr.mnemonic << " " << instr.op_str;
            cs_free(insn, count);
        } else {
            add_hex_bytes(ptr, max_instruction_bytes);
            ss << " <unrecognized>";
        }
    } else {
        add_padding_bytes(0);
        ss << " <cannot read memory>";
    }
    return ss.str();
}
