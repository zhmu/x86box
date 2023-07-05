#include "cpux86.h"
#include "io.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <bit>
#include <utility>
#include "alu.h"

#include "spdlog/spdlog.h"

namespace
{
    cpu::Flags UpdateFlagsForCPU(cpu::Flags flags)
    {
        flags |= 0xf000; // always set top nibble to indicate 8086/8088/80186
        flags |= cpu::flag::ON;
        return flags;
    }
}

CPUx86::CPUx86(Memory& oMemory, IO& oIO)
    : m_Memory(oMemory), m_IO(oIO)
{
}

CPUx86::~CPUx86() = default;

void CPUx86::Reset()
{
    m_State.m_prefix = 0;
    m_State.m_flags = UpdateFlagsForCPU(0);
    m_State.m_cs = 0xffff;
    m_State.m_ip = 0;
    m_State.m_ds = 0;
    m_State.m_es = 0;
    m_State.m_ss = 0;

    m_State.m_ax = 0x1234;
}

namespace alu = cpu::alu;

namespace
{
    template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };

    // These must be in sync with the x86 segment values (Sw)
    constexpr int SEG_ES = 0;
    constexpr int SEG_CS = 1;
    constexpr int SEG_SS = 2;
    constexpr int SEG_DS = 3;

    // Interrupt values
    constexpr unsigned int INT_DIV_BY_ZERO = 0;
    constexpr unsigned int INT_SINGLE_STEP = 1;
    constexpr unsigned int INT_NMI = 2;
    constexpr unsigned int INT_BREAKPOINT = 3;
    constexpr unsigned int INT_OVERFLOW = 4;

    constexpr void RelativeJump8(uint16_t& ip, const uint8_t n)
    {
        if (n & 0x80)
            ip -= 0x100 - n;
        else
            ip += static_cast<uint16_t>(n);
    }

    constexpr void RelativeJump16(uint16_t& ip, const uint16_t n)
    {
        if (n & 0x8000)
            ip -= 0x10000 - n;
        else
            ip += n;
    }

    uint16_t HandleSegmentOverride(cpu::State& state, uint16_t def)
    {
        if ((state.m_prefix & cpu::State::PREFIX_SEG) == 0)
            return def;
        state.m_prefix &= ~cpu::State::PREFIX_SEG;
        return state.m_seg_override;
    }

    [[noreturn]] void Unreachable()
    {
        __builtin_unreachable();
    }

    template<typename T>
    auto& GetSReg16(T& state, const int n)
        requires (std::is_same_v<std::remove_cv_t<T>, cpu::State>)
    {
        switch (n) {
            case SEG_ES:
                return state.m_es;
            case SEG_CS:
                return state.m_cs;
            case SEG_SS:
                return state.m_ss;
            case SEG_DS:
                return state.m_ds;
        }
        Unreachable();
    }

    struct Reg8
    {
        uint16_t& reg;
        const unsigned int shift;

        uint8_t Load() const {
            return (reg >> shift) & 0xff;
        }

        void Store(const uint8_t value) {
            if (shift > 0) {
                reg = (reg & 0x00ff) | (value << 8);
            } else {
                reg = (reg & 0xff00) | value;
            }
        }
    };

    Reg8 ObtainReg8(cpu::State& state, int reg)
    {
        const unsigned int shift = (reg > 3) ? 8 : 0;
        switch (reg & 3) {
            case 0:
                return { state.m_ax, shift };
            case 1:
                return { state.m_cx, shift };
            case 2:
                return { state.m_dx, shift };
            case 3:
                return { state.m_bx, shift };
            default:
                Unreachable();
        }
    }

    uint16_t& GetReg16(cpu::State& state, const int n)
    {
        switch (n) {
            case 0:
                return state.m_ax;
            case 1:
                return state.m_cx;
            case 2:
                return state.m_dx;
            case 3:
                return state.m_bx;
            case 4:
                return state.m_sp;
            case 5:
                return state.m_bp;
            case 6:
                return state.m_si;
            case 7:
                return state.m_di;
        }
        Unreachable();
    }

    struct ModRM_Register
    {
        uint8_t reg;
    };

    struct ModRM_Memory
    {
        uint16_t seg, off;
        CPUx86::addr_t disp;
    };

    using ModRM = std::variant<ModRM_Register, ModRM_Memory>;

    CPUx86::addr_t GetModRMMemoryAddress(const ModRM_Memory& mem, const cpu::State& state)
    {
        const auto seg = GetSReg16(state, mem.seg);
        return CPUx86::MakeAddr(seg, mem.off + mem.disp);
    }

    uint8_t ReadEA8(Memory& memory, cpu::State& state, const ModRM& modrm)
    {
        return std::visit(overloaded{
            [&](const ModRM_Register& reg) {
                return ObtainReg8(state, reg.reg).Load();
            },
            [&](const ModRM_Memory& mem) {
                const auto addr = GetModRMMemoryAddress(mem, state);
                return memory.ReadByte(addr);
            }
        }, modrm);
    }

    void WriteEA8(Memory& memory, cpu::State& state, const ModRM& modrm, const uint8_t val)
    {
        return std::visit(overloaded{
            [&](const ModRM_Register& reg) {
                ObtainReg8(state, reg.reg).Store(val);
            },
            [&](const ModRM_Memory& mem) {
                const auto addr = GetModRMMemoryAddress(mem, state);
                memory.WriteByte(addr, val);
            } }, modrm);
    }

    uint16_t ReadEA16(Memory& memory, cpu::State& state, const ModRM& modrm, uint16_t offset_delta = 0)
    {
        return std::visit(overloaded{
            [&](const ModRM_Register& reg) {
                return GetReg16(state, reg.reg);
            },
            [&](const ModRM_Memory& mem) {
                const auto addr = GetModRMMemoryAddress(mem, state);
                return memory.ReadWord(addr + offset_delta);
            } }, modrm);
    }

    uint16_t GetAddrEA16(cpu::State& state, const ModRM& modrm)
    {
        return std::visit(overloaded{
            [&](const ModRM_Register& reg) {
                return GetReg16(state, reg.reg);
            },
            [&](const ModRM_Memory& mem) -> uint16_t {
                return mem.off + mem.disp;
            } }, modrm);
    }

    void WriteEA16(Memory& memory, cpu::State& state, const ModRM& modrm, uint16_t value)
    {
        return std::visit(overloaded{
            [&](const ModRM_Register& reg) {
                GetReg16(state, reg.reg) = value;
            },
            [&](const ModRM_Memory& mem) {
                const auto addr = GetModRMMemoryAddress(mem, state);
                memory.WriteWord(addr, value);
            } }, modrm);
    }

    void Push16(Memory& memory, cpu::State& state, const uint16_t value)
    {
        memory.WriteWord(CPUx86::MakeAddr(state.m_ss, state.m_sp - 2), value);
        state.m_sp -= 2;
    }

    uint16_t Pop16(Memory& memory, cpu::State& state)
    {
        const auto value = memory.ReadWord(CPUx86::MakeAddr(state.m_ss, state.m_sp));
        state.m_sp += 2;
        return value;
    }

    uint8_t GetCodeImm8(Memory& memory, cpu::State& state)
    {
        return memory.ReadByte(CPUx86::MakeAddr(state.m_cs, state.m_ip++));
    }

    uint16_t GetCodeImm16(Memory& memory, cpu::State& state)
    {
        const uint16_t a = GetCodeImm8(memory, state);
        const uint16_t b = GetCodeImm8(memory, state);
        return a | (b << 8);
    }

    uint16_t ExtendSign8To16(const uint8_t v)
    {
        if (v & 0x80)
            return 0xff00 | v;
        return v;
    }

    struct ModRegRM
    {
        uint8_t mod;
        uint8_t reg;
        uint8_t rm;
    };

    struct ModOpRM
    {
        uint8_t mod;
        uint8_t op;
        uint8_t rm;
    };

    template<typename T> T DecodeModXXXRm(const uint8_t v)
    {
        return T{
            static_cast<uint8_t>((v & 0xc0) >> 6),
            static_cast<uint8_t>((v >> 3) & 7),
            static_cast<uint8_t>(v & 7),
        };
    }

    ModRegRM GetModRegRm(Memory& memory, cpu::State& state)
    {
        const auto v = GetCodeImm8(memory, state);
        return DecodeModXXXRm<ModRegRM>(v);
    }

    ModOpRM GetModOpRm(Memory& memory, cpu::State& state)
    {
        const auto v = GetCodeImm8(memory, state);
        return DecodeModXXXRm<ModOpRM>(v);
    }

    template<typename T>
    ModRM DecodeModRm(Memory& memory, cpu::State& state, const T& mr)
        requires (std::is_same_v<T, ModOpRM> || std::is_same_v<T, ModRegRM>)
    {
        if (mr.mod == 3) /* rm treated as reg field */ {
            return ModRM_Register{ .reg = mr.rm };
        }
        if (mr.mod == 0 && mr.rm == 6) /* if mod==00 and rm==110, then EA = disp-hi; disp-lo */ {
            const auto imm = GetCodeImm16(memory, state);
            return ModRM_Memory{
                .seg = HandleSegmentOverride(state, SEG_DS),
                .off = imm,
                .disp = 0
            };
        }

        CPUx86::addr_t disp = 0;
        switch (mr.mod) {
            case 0: /* DISP=0*, disp-low and disp-hi are absent */
                disp = 0;
                break;
            case 1: /* DISP=disp-low sign-extended to 16-bits, disp-hi absent */ {
                const auto imm = GetCodeImm8(memory, state);
                disp = ExtendSign8To16(imm);
                break;
            }
            case 2: /* DISP=disp-hi:disp-lo */ {
                const auto imm = GetCodeImm16(memory, state);
                disp = imm;
                break;
            }
            default:
                Unreachable();
        }

        uint16_t seg, off;
        switch (mr.rm) {
            case 0: // (bx) + (si) + disp
                seg = HandleSegmentOverride(state, SEG_DS);
                off = state.m_bx + state.m_si;
                break;
            case 1: // (bx) + (di) + disp
                seg = HandleSegmentOverride(state, SEG_DS);
                off = state.m_bx + state.m_di;
                break;
            case 2: // (bp) + (si) + disp
                seg = HandleSegmentOverride(state, SEG_SS);
                off = state.m_bp + state.m_si;
                break;
            case 3: // (bp) + (di) + disp
                seg = HandleSegmentOverride(state, SEG_SS);
                off = state.m_bp + state.m_di;
                break;
            case 4: // (si) + disp
                seg = HandleSegmentOverride(state, SEG_DS);
                off = state.m_si;
                break;
            case 5: // (di) + disp
                seg = HandleSegmentOverride(state, SEG_DS);
                off = state.m_di;
                break;
            case 6: // (bp) + disp
                seg = HandleSegmentOverride(state, SEG_SS);
                off = state.m_bp;
                break;
            case 7: // (bx) + disp
                seg = HandleSegmentOverride(state, SEG_DS);
                off = state.m_bx;
                break;
            default:
                Unreachable();
        }
        return ModRM_Memory{ .seg = seg, .off = off, .disp = disp };
    }
}

void CPUx86::RunInstruction()
{
    auto getImm8 = [&]() { return GetCodeImm8(m_Memory, m_State); };
    auto getImm16 = [&]() { return GetCodeImm16(m_Memory, m_State); };

    auto handleConditionalJump = [&](bool take) {
        const auto imm = getImm8();
        if (take)
            RelativeJump8(m_State.m_ip, imm);
    };

    auto todo = []() { spdlog::warn("TODO"); };
    auto invalidOpcode = []() { spdlog::error("invalidOpcode()\n"); std::abort(); };

    /*
     * The Op_... macro's follow the 80386 manual conventions (appendix F page
     * 706)
     *
     * The first character contains the addressing method:
     *
     * - E = mod/rm follows, specified operand
     * - G = reg field of modrm selects general register
     *
     * The second character is the operand type:
     *
     * - v = word
     * - b = byte
     */

    // op Ev Gv -> Ev = op(Ev, Gv)
    auto opEvGv = [&](auto op) {
        const auto mrr = GetModRegRm(m_Memory, m_State);
        const auto modRm = DecodeModRm(m_Memory, m_State, mrr);
        WriteEA16(m_Memory, m_State, modRm, op(m_State.m_flags, ReadEA16(m_Memory, m_State, modRm), GetReg16(m_State, mrr.reg)));
    };

    // op Gv Ev -> Gv = op(Gv, Ev)
    auto opGvEv = [&](auto op) {
        const auto mrr = GetModRegRm(m_Memory, m_State);
        const auto modRm = DecodeModRm(m_Memory, m_State, mrr);
        uint16_t& reg = GetReg16(m_State, mrr.reg);
        reg = op(m_State.m_flags, reg, ReadEA16(m_Memory, m_State, modRm));
    };

    // Op Eb Gb -> Eb = op(Eb, Gb)
    auto opEbGb = [&](auto op) {
        const auto mrr = GetModRegRm(m_Memory, m_State);
        const auto modRm = DecodeModRm(m_Memory, m_State, mrr);
        auto reg = ObtainReg8(m_State, mrr.reg);
        WriteEA8(m_Memory, m_State, modRm, op(m_State.m_flags, ReadEA8(m_Memory, m_State, modRm), reg.Load()));
    };

    // Op Gb Eb -> Gb = op(Gb, Eb)
    auto opGbEb = [&](auto op) {
        const auto mrr = GetModRegRm(m_Memory, m_State);
        const auto modRm = DecodeModRm(m_Memory, m_State, mrr);
        auto reg = ObtainReg8(m_State, mrr.reg);
        reg.Store(op(m_State.m_flags, reg.Load(), ReadEA8(m_Memory, m_State, modRm)));
    };

    auto opcode = GetCodeImm8(m_Memory, m_State);
    spdlog::debug("cs:ip={:04x}:{:04x} opcode {:02x}", m_State.m_cs, m_State.m_ip - 1, opcode);

    // Handle prefixes first
    m_State.m_prefix = 0;
    m_State.m_seg_override = 0;
    while(true) {
        if (opcode == 0x26) { /* ES: */
            m_State.m_prefix |= cpu::State::PREFIX_SEG;
            m_State.m_seg_override = SEG_ES;
        } else if (opcode == 0x2e) { /* CS: */
            m_State.m_prefix |= cpu::State::PREFIX_SEG;
            m_State.m_seg_override = SEG_CS;
        } else if (opcode == 0x36) {/* SS: */
            m_State.m_prefix |= cpu::State::PREFIX_SEG;
            m_State.m_seg_override = SEG_SS;
        } else if (opcode == 0x3e) {/* DS: */
            m_State.m_prefix |= cpu::State::PREFIX_SEG;
            m_State.m_seg_override = SEG_DS;
        } else if (opcode == 0xf2) { /* REPNZ */
            m_State.m_prefix |= cpu::State::PREFIX_REPNZ;
        } else if (opcode == 0xf3) { /* REPZ */
            m_State.m_prefix |= cpu::State::PREFIX_REPZ;
        } else {
            break;
        }
        opcode = GetCodeImm8(m_Memory, m_State);
    }

    switch (opcode) {
        case 0x00: /* ADD Eb Gb */ {
            opEbGb(cpu::alu::ADD<8>);
            break;
        }
        case 0x01: /* ADD Ev Gv */ {
            opEvGv(cpu::alu::ADD<16>);
            break;
        }
        case 0x02: /* ADD Gb Eb */ {
            opGbEb(cpu::alu::ADD<8>);
            break;
        }
        case 0x03: /* ADD Gv Ev */ {
            opGvEv(cpu::alu::ADD<16>);
            break;
        }
        case 0x04: /* ADD AL Ib */ {
            const auto imm = getImm8();
            m_State.m_ax = (m_State.m_ax & 0xff00) | alu::ADD<8>(m_State.m_flags, m_State.m_ax & 0xff, imm);
            break;
        }
        case 0x05: /* ADD eAX Iv */ {
            const auto imm = getImm16();
            m_State.m_ax = alu::ADD<16>(m_State.m_flags, m_State.m_ax, imm);
            break;
        }
        case 0x06: /* PUSH ES */ {
            Push16(m_Memory, m_State, m_State.m_es);
            break;
        }
        case 0x07: /* POP ES */ {
            m_State.m_es = Pop16(m_Memory, m_State);
            break;
        }
        case 0x08: /* OR Eb Gb */ {
            opEbGb(cpu::alu::OR<8>);
            break;
        }
        case 0x09: /* OR Ev Gv */ {
            opEvGv(cpu::alu::OR<16>);
            break;
        }
        case 0x0a: /* OR Gb Eb */ {
            opGbEb(cpu::alu::OR<8>);
            break;
        }
        case 0x0b: /* OR Gv Ev */ {
            opGvEv(cpu::alu::OR<16>);
            break;
        }
        case 0x0c: /* OR AL Ib */ {
            const auto imm = getImm8();
            m_State.m_ax = (m_State.m_ax & 0xff00) | alu::OR<8>(m_State.m_flags, m_State.m_ax & 0xff, imm);
            break;
        }
        case 0x0d: /* OR eAX Iv */ {
            const auto imm = getImm16();
            m_State.m_ax = alu::OR<16>(m_State.m_flags, m_State.m_ax, imm);
            break;
        }
        case 0x0e: /* PUSH CS */ {
            Push16(m_Memory, m_State, m_State.m_cs);
            break;
        }
        case 0x0f: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0x10: /* ADC Eb Gb */ {
            opEbGb(cpu::alu::ADC<8>);
            break;
        }
        case 0x11: /* ADC Ev Gv */ {
            opEvGv(cpu::alu::ADC<16>);
            break;
        }
        case 0x12: /* ADC Gb Eb */ {
            opGbEb(cpu::alu::ADC<8>);
            break;
        }
        case 0x13: /* ADC Gv Ev */ {
            opGvEv(cpu::alu::ADC<16>);
            break;
        }
        case 0x14: /* ADC AL Ib */ {
            const auto imm = getImm8();
            m_State.m_ax = (m_State.m_ax & 0xff00) | alu::ADC<8>(m_State.m_flags, m_State.m_ax & 0xff, imm);
            break;
        }
        case 0x15: /* ADC eAX Iv */ {
            const auto imm = getImm16();
            m_State.m_ax = alu::ADC<16>(m_State.m_flags, m_State.m_ax, imm);
            break;
        }
        case 0x16: /* PUSH SS */ {
            Push16(m_Memory, m_State, m_State.m_ss);
            break;
        }
        case 0x17: /* POP SS */ {
            m_State.m_ss = Pop16(m_Memory, m_State);
            break;
        }
        case 0x18: /* SBB Eb Gb */ {
            opEbGb(cpu::alu::SBB<8>);
            break;
        }
        case 0x19: /* SBB Ev Gv */ {
            opEvGv(cpu::alu::SBB<16>);
            break;
        }
        case 0x1a: /* SBB Gb Eb */ {
            opGbEb(cpu::alu::SBB<8>);
            break;
        }
        case 0x1b: /* SBB Gv Ev */ {
            opGvEv(cpu::alu::SBB<16>);
            break;
        }
        case 0x1c: /* SBB AL Ib */ {
            const auto imm = getImm8();
            m_State.m_ax = (m_State.m_ax & 0xff00) | alu::SBB<8>(m_State.m_flags, m_State.m_ax & 0xff, imm);
            break;
        }
        case 0x1d: /* SBB eAX Iv */ {
            const auto imm = getImm16();
            m_State.m_ax = alu::SBB<16>(m_State.m_flags, m_State.m_ax, imm);
            break;
        }
        case 0x1e: /* PUSH DS */ {
            Push16(m_Memory, m_State, m_State.m_ds);
            break;
        }
        case 0x1f: /* POP DS */ {
            m_State.m_ds = Pop16(m_Memory, m_State);
            break;
        }
        case 0x20: /* AND Eb Gb */ {
            opEbGb(cpu::alu::AND<8>);
            break;
        }
        case 0x21: /* AND Ev Gv */ {
            opEvGv(cpu::alu::AND<16>);
            break;
        }
        case 0x22: /* AND Gb Eb */ {
            opGbEb(cpu::alu::AND<8>);
            break;
        }
        case 0x23: /* AND Gv Ev */ {
            opGvEv(cpu::alu::AND<16>);
            break;
        }
        case 0x24: /* AND AL Ib */ {
            const auto imm = getImm8();
            m_State.m_ax = (m_State.m_ax & 0xff00) | alu::AND<8>(m_State.m_flags, m_State.m_ax & 0xff, imm);
            break;
        }
        case 0x25: /* AND eAX Iv */ {
            const auto imm = getImm16();
            m_State.m_ax = alu::AND<16>(m_State.m_flags, m_State.m_ax, imm);
            break;
        }
        case 0x26: /* ES: */{
            Unreachable();
            break;
        }
        case 0x27: /* DAA */ {
            m_State.m_ax = (m_State.m_ax & 0xff00) | alu::DAA(m_State.m_flags, m_State.m_ax & 0xff);
            break;
        }
        case 0x28: /* SUB Eb Gb */ {
            opEbGb(cpu::alu::SUB<8>);
            break;
        }
        case 0x29: /* SUB Ev Gv */ {
            opEvGv(cpu::alu::SUB<16>);
            break;
        }
        case 0x2a: /* SUB Gb Eb */ {
            opGbEb(cpu::alu::SUB<8>);
            break;
        }
        case 0x2b: /* SUB Gv Ev */ {
            opGvEv(cpu::alu::SUB<16>);
            break;
        }
        case 0x2c: /* SUB AL Ib */ {
            const auto imm = getImm8();
            m_State.m_ax = (m_State.m_ax & 0xff00) | alu::SUB<8>(m_State.m_flags, m_State.m_ax & 0xff, imm);
            break;
        }
        case 0x2d: /* SUB eAX Iv */ {
            const auto imm = getImm16();
            m_State.m_ax = alu::SUB<16>(m_State.m_flags, m_State.m_ax, imm);
            break;
        }
        case 0x2e: /* CS: */ {
            Unreachable();
            break;
        }
        case 0x2f: /* DAS */ {
            m_State.m_ax = (m_State.m_ax & 0xff00) | alu::DAS(m_State.m_flags, m_State.m_ax & 0xff);
            break;
        }
        case 0x30: /* XOR Eb Gb */ {
            opEbGb(cpu::alu::XOR<8>);
            break;
        }
        case 0x31: /* XOR Ev Gv */ {
            opEvGv(cpu::alu::XOR<16>);
            break;
        }
        case 0x32: /* XOR Gb Eb */ {
            opGbEb(cpu::alu::XOR<8>);
            break;
        }
        case 0x33: /* XOR Gv Ev */ {
            opGvEv(cpu::alu::XOR<16>);
            break;
        }
        case 0x34: /* XOR AL Ib */ {
            const auto imm = getImm8();
            m_State.m_ax = (m_State.m_ax & 0xff00) | alu::XOR<8>(m_State.m_flags, m_State.m_ax & 0xff, imm);
            break;
        }
        case 0x35: /* XOR eAX Iv */ {
            const auto imm = getImm16();
            m_State.m_ax = alu::XOR<16>(m_State.m_flags, m_State.m_ax, imm);
            break;
        }
        case 0x36: /* SS: */ {
            Unreachable();
            break;
        }
        case 0x37: /* AAA */ {
            m_State.m_ax = alu::AAA(m_State.m_flags, m_State.m_ax);
            break;
        }
        case 0x38: /* CMP Eb Gb */ {
            const auto mrr = GetModRegRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mrr);
            auto reg = ObtainReg8(m_State, mrr.reg);
            alu::CMP<8>(m_State.m_flags, ReadEA8(m_Memory, m_State, modRm), reg.Load());
            break;
        }
        case 0x39: /* CMP Ev Gv */ {
            const auto mrr = GetModRegRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mrr);
            alu::CMP<16>(m_State.m_flags, ReadEA16(m_Memory, m_State, modRm), GetReg16(m_State, mrr.reg));
            break;
        }
        case 0x3a: /* CMP Gb Eb */ {
            const auto mrr = GetModRegRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mrr);
            auto reg = ObtainReg8(m_State, mrr.reg);
            alu::CMP<8>(m_State.m_flags, reg.Load(), ReadEA8(m_Memory, m_State, modRm));
            break;
        }
        case 0x3b: /* CMP Gv Ev */ {
            const auto mrr = GetModRegRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mrr);
            alu::CMP<16>(m_State.m_flags, GetReg16(m_State, mrr.reg), ReadEA16(m_Memory, m_State, modRm));
            break;
        }
        case 0x3c: /* CMP AL Ib */ {
            const auto imm = getImm8();
            alu::CMP<8>(m_State.m_flags, m_State.m_ax & 0xff, imm);
            break;
        }
        case 0x3d: /* CMP eAX Iv */ {
            const auto imm = getImm16();
            alu::CMP<16>(m_State.m_flags, m_State.m_ax, imm);
            break;
        }
        case 0x3e: /* DS: */ {
            Unreachable();
            break;
        }
        case 0x3f: /* AAS */ {
            m_State.m_ax = alu::AAS(m_State.m_flags, m_State.m_ax);
            break;
        }
        case 0x40: /* INC eAX */ {
            m_State.m_ax = alu::INC<16>(m_State.m_flags, m_State.m_ax);
            break;
        }
        case 0x41: /* INC eCX */ {
            m_State.m_cx = alu::INC<16>(m_State.m_flags, m_State.m_cx);
            break;
        }
        case 0x42: /* INC eDX */ {
            m_State.m_dx = alu::INC<16>(m_State.m_flags, m_State.m_dx);
            break;
        }
        case 0x43: /* INC eBX */ {
            m_State.m_bx = alu::INC<16>(m_State.m_flags, m_State.m_bx);
            break;
        }
        case 0x44: /* INC eSP */ {
            m_State.m_sp = alu::INC<16>(m_State.m_flags, m_State.m_sp);
            break;
        }
        case 0x45: /* INC eBP */ {
            m_State.m_bp = alu::INC<16>(m_State.m_flags, m_State.m_bp);
            break;
        }
        case 0x46: /* INC eSI */ {
            m_State.m_si = alu::INC<16>(m_State.m_flags, m_State.m_si);
            break;
        }
        case 0x47: /* INC eDI */ {
            m_State.m_di = alu::INC<16>(m_State.m_flags, m_State.m_di);
            break;
        }
        case 0x48: /* DEC eAX */ {
            m_State.m_ax = alu::DEC<16>(m_State.m_flags, m_State.m_ax);
            break;
        }
        case 0x49: /* DEC eCX */ {
            m_State.m_cx = alu::DEC<16>(m_State.m_flags, m_State.m_cx);
            break;
        }
        case 0x4a: /* DEC eDX */ {
            m_State.m_dx = alu::DEC<16>(m_State.m_flags, m_State.m_dx);
            break;
        }
        case 0x4b: /* DEC eBX */ {
            m_State.m_bx = alu::DEC<16>(m_State.m_flags, m_State.m_bx);
            break;
        }
        case 0x4c: /* DEC eSP */ {
            m_State.m_sp = alu::DEC<16>(m_State.m_flags, m_State.m_sp);
            break;
        }
        case 0x4d: /* DEC eBP */ {
            m_State.m_bp = alu::DEC<16>(m_State.m_flags, m_State.m_bp);
            break;
        }
        case 0x4e: /* DEC eSI */ {
            m_State.m_si = alu::DEC<16>(m_State.m_flags, m_State.m_si);
            break;
        }
        case 0x4f: /* DEC eDI */ {
            m_State.m_di = alu::DEC<16>(m_State.m_flags, m_State.m_di);
            break;
        }
        case 0x50: /* PUSH eAX */ {
            Push16(m_Memory, m_State, m_State.m_ax);
            break;
        }
        case 0x51: /* PUSH eCX */ {
            Push16(m_Memory, m_State, m_State.m_cx);
            break;
        }
        case 0x52: /* PUSH eDX */ {
            Push16(m_Memory, m_State, m_State.m_dx);
            break;
        }
        case 0x53: /* PUSH eBX */ {
            Push16(m_Memory, m_State, m_State.m_bx);
            break;
        }
        case 0x54: /* PUSH eSP */ {
            Push16(m_Memory, m_State, m_State.m_sp);
            break;
        }
        case 0x55: /* PUSH eBP */ {
            Push16(m_Memory, m_State, m_State.m_bp);
            break;
        }
        case 0x56: /* PUSH eSI */ {
            Push16(m_Memory, m_State, m_State.m_si);
            break;
        }
        case 0x57: /* PUSH eDI */ {
            Push16(m_Memory, m_State, m_State.m_di);
            break;
        }
        case 0x58: /* POP eAX */ {
            m_State.m_ax = Pop16(m_Memory, m_State);
            break;
        }
        case 0x59: /* POP eCX */ {
            m_State.m_cx = Pop16(m_Memory, m_State);
            break;
        }
        case 0x5a: /* POP eDX */ {
            m_State.m_dx = Pop16(m_Memory, m_State);
            break;
        }
        case 0x5b: /* POP eBX */ {
            m_State.m_bx = Pop16(m_Memory, m_State);
            break;
        }
        case 0x5c: /* POP eSP */ {
            m_State.m_sp = Pop16(m_Memory, m_State);
            break;
        }
        case 0x5d: /* POP eBP */ {
            m_State.m_bp = Pop16(m_Memory, m_State);
            break;
        }
        case 0x5e: /* POP eSI */ {
            m_State.m_si = Pop16(m_Memory, m_State);
            break;
        }
        case 0x5f: /* POP eDI */ {
            m_State.m_di = Pop16(m_Memory, m_State);
            break;
        }
        case 0x60: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0x61: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0x62: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0x63: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0x64: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0x65: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0x66: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0x67: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0x68: /* PUSH imm16 */ {
            const auto imm = getImm16();
            Push16(m_Memory, m_State, imm);
            break;
        }
        case 0x69: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0x6a: /* PUSH imm8 */ {
            const auto imm = getImm8();
            int16_t imm16 = (int16_t)imm;
            // TODO
            Push16(m_Memory, m_State, imm16);
            break;
        }
        case 0x6b: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0x6c: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0x6d: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0x6e: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0x6f: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0x70: /* JO Jb */ {
            handleConditionalJump(cpu::FlagOverflow(m_State.m_flags));
            break;
        }
        case 0x71: /* JNO Jb */ {
            handleConditionalJump(!cpu::FlagOverflow(m_State.m_flags));
            break;
        }
        case 0x72: /* JB Jb */ {
            handleConditionalJump(cpu::FlagCarry(m_State.m_flags));
            break;
        }
        case 0x73: /* JNB Jb */ {
            handleConditionalJump(!cpu::FlagCarry(m_State.m_flags));
            break;
        }
        case 0x74: /* JZ Jb */ {
            handleConditionalJump(cpu::FlagZero(m_State.m_flags));
            break;
        }
        case 0x75: /* JNZ Jb */ {
            handleConditionalJump(!cpu::FlagZero(m_State.m_flags));
            break;
        }
        case 0x76: /* JBE Jb */ {
            handleConditionalJump(cpu::FlagCarry(m_State.m_flags) || cpu::FlagZero(m_State.m_flags));
            break;
        }
        case 0x77: /* JA Jb */ {
            handleConditionalJump(!cpu::FlagCarry(m_State.m_flags) && !cpu::FlagZero(m_State.m_flags));
            break;
        }
        case 0x78: /* JS Jb */ {
            handleConditionalJump(cpu::FlagSign(m_State.m_flags));
            break;
        }
        case 0x79: /* JNS Jb */ {
            handleConditionalJump(!cpu::FlagSign(m_State.m_flags));
            break;
        }
        case 0x7a: /* JPE Jb */ {
            handleConditionalJump(cpu::FlagParity(m_State.m_flags));
            break;
        }
        case 0x7b: /* JPO Jb */ {
            handleConditionalJump(!cpu::FlagParity(m_State.m_flags));
            break;
        }
        case 0x7c: /* JL Jb */ {
            handleConditionalJump(cpu::FlagSign(m_State.m_flags) != cpu::FlagOverflow(m_State.m_flags));
            break;
        }
        case 0x7d: /* JGE Jb */ {
            handleConditionalJump(cpu::FlagSign(m_State.m_flags) == cpu::FlagOverflow(m_State.m_flags));
            break;
        }
        case 0x7e: /* JLE Jb */ {
            handleConditionalJump(cpu::FlagSign(m_State.m_flags) != cpu::FlagOverflow(m_State.m_flags) || cpu::FlagZero(m_State.m_flags));
            break;
        }
        case 0x7f: /* JG Jb */ {
            handleConditionalJump(!cpu::FlagZero(m_State.m_flags) && cpu::FlagSign(m_State.m_flags) == cpu::FlagOverflow(m_State.m_flags));
            break;
        }
        case 0x80:
        case 0x82: /* GRP1 Eb Ib */ {
            const auto mor = GetModOpRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mor);
            const auto imm = getImm8();

            uint8_t val = ReadEA8(m_Memory, m_State, modRm);
            switch (mor.op) {
                case 0: // add
                    WriteEA8(m_Memory, m_State, modRm, alu::ADD<8>(m_State.m_flags, val, imm));
                    break;
                case 1: // or
                    WriteEA8(m_Memory, m_State, modRm, alu::OR<8>(m_State.m_flags, val, imm));
                    break;
                case 2: // adc
                    WriteEA8(m_Memory, m_State, modRm, alu::ADC<8>(m_State.m_flags, val, imm));
                    break;
                case 3: // sbb
                    WriteEA8(m_Memory, m_State, modRm, alu::SBB<8>(m_State.m_flags, val, imm));
                    break;
                case 4: // and
                    WriteEA8(m_Memory, m_State, modRm, alu::AND<8>(m_State.m_flags, val, imm));
                    break;
                case 5: // sub
                    WriteEA8(m_Memory, m_State, modRm, alu::SUB<8>(m_State.m_flags, val, imm));
                    break;
                case 6: // xor
                    WriteEA8(m_Memory, m_State, modRm, alu::XOR<8>(m_State.m_flags, val, imm));
                    break;
                case 7: // cmp
                    alu::CMP<8>(m_State.m_flags, val, imm);
                    break;
            }
            break;
        }
        case 0x81: /* GRP1 Ev Iv */ {
            const auto mor = GetModOpRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mor);
            const auto imm = getImm16();

            uint16_t val = ReadEA16(m_Memory, m_State, modRm);
            switch (mor.op) {
                case 0: // add
                    WriteEA16(m_Memory, m_State, modRm, alu::ADD<16>(m_State.m_flags, val, imm));
                    break;
                case 1: // or
                    WriteEA16(m_Memory, m_State, modRm, alu::OR<16>(m_State.m_flags, val, imm));
                    break;
                case 2: // adc
                    WriteEA16(m_Memory, m_State, modRm, alu::ADC<16>(m_State.m_flags, val, imm));
                    break;
                case 3: // sbb
                    WriteEA16(m_Memory, m_State, modRm, alu::SBB<16>(m_State.m_flags, val, imm));
                    break;
                case 4: // and
                    WriteEA16(m_Memory, m_State, modRm, alu::AND<16>(m_State.m_flags, val, imm));
                    break;
                case 5: // sub
                    WriteEA16(m_Memory, m_State, modRm, alu::SUB<16>(m_State.m_flags, val, imm));
                    break;
                case 6: // xor
                    WriteEA16(m_Memory, m_State, modRm, alu::XOR<16>(m_State.m_flags, val, imm));
                    break;
                case 7: // cmp
                    alu::CMP<16>(m_State.m_flags, val, imm);
                    break;
            }
            break;
        }
        case 0x83: /* GRP1 Ev Ib */ {
            const auto mor = GetModOpRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mor);
            const auto imm = ExtendSign8To16(getImm8());

            uint16_t val = ReadEA16(m_Memory, m_State, modRm);
            switch (mor.op) {
                case 0: // add
                    WriteEA16(m_Memory, m_State, modRm, alu::ADD<16>(m_State.m_flags, val, imm));
                    break;
                case 1: // or
                    WriteEA16(m_Memory, m_State, modRm, alu::OR<16>(m_State.m_flags, val, imm));
                    break;
                case 2: // adc
                    WriteEA16(m_Memory, m_State, modRm, alu::ADC<16>(m_State.m_flags, val, imm));
                    break;
                case 3: // sbb
                    WriteEA16(m_Memory, m_State, modRm, alu::SBB<16>(m_State.m_flags, val, imm));
                    break;
                case 4: // and
                    WriteEA16(m_Memory, m_State, modRm, alu::AND<16>(m_State.m_flags, val, imm));
                    break;
                case 5: // sub
                    WriteEA16(m_Memory, m_State, modRm, alu::SUB<16>(m_State.m_flags, val, imm));
                    break;
                case 6: // xor
                    WriteEA16(m_Memory, m_State, modRm, alu::XOR<16>(m_State.m_flags, val, imm));
                    break;
                case 7: // cmp
                    alu::CMP<16>(m_State.m_flags, val, imm);
                    break;
            }
            break;
        }
        case 0x84: /* TEST Gb Eb */ {
            const auto mrr = GetModRegRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mrr);
            auto reg = ObtainReg8(m_State, mrr.reg);
            alu::TEST<8>(m_State.m_flags, reg.Load(), ReadEA8(m_Memory, m_State, modRm));
            break;
        }
        case 0x85: /* TEST Gv Ev */ {
            const auto mrr = GetModRegRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mrr);
            alu::TEST<16>(m_State.m_flags, GetReg16(m_State, mrr.reg), ReadEA16(m_Memory, m_State, modRm));
            break;
        }
        case 0x86: /* XCHG Gb Eb */ {
            const auto mrr = GetModRegRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mrr);
            auto reg = ObtainReg8(m_State, mrr.reg);
            const auto prev_value = reg.Load();
            reg.Store(ReadEA8(m_Memory, m_State, modRm));
            WriteEA8(m_Memory, m_State, modRm, prev_value);
            break;
        }
        case 0x87: /* XCHG Gv Ev */ {
            const auto mrr = GetModRegRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mrr);
            uint16_t& reg = GetReg16(m_State, mrr.reg);
            uint16_t prev_reg = reg;
            reg = ReadEA16(m_Memory, m_State, modRm);
            WriteEA16(m_Memory, m_State, modRm, prev_reg);
            break;
        }
        case 0x88: /* MOV Eb Gb */ {
            const auto mrr = GetModRegRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mrr);
            auto reg = ObtainReg8(m_State, mrr.reg);
            WriteEA8(m_Memory, m_State, modRm, reg.Load());
            break;
        }
        case 0x89: /* MOV Ev Gv */ {
            const auto mrr = GetModRegRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mrr);
            WriteEA16(m_Memory, m_State, modRm, GetReg16(m_State, mrr.reg));
            break;
        }
        case 0x8a: /* MOV Gb Eb */ {
            const auto mrr = GetModRegRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mrr);
            auto reg = ObtainReg8(m_State, mrr.reg);
            reg.Store(ReadEA8(m_Memory, m_State, modRm));
            break;
        }
        case 0x8b: /* MOV Gv Ev */ {
            const auto mrr = GetModRegRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mrr);
            GetReg16(m_State, mrr.reg) = ReadEA16(m_Memory, m_State, modRm);
            break;
        }
        case 0x8c: /* MOV Ew Sw */ {
            const auto mrr = GetModRegRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mrr);
            WriteEA16(m_Memory, m_State, modRm, GetSReg16(m_State, mrr.reg));
            break;
        }
        case 0x8d: /* LEA Gv M */ {
            const auto mrr = GetModRegRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mrr);
            GetReg16(m_State, mrr.reg) = GetAddrEA16(m_State, modRm);
            break;
        }
        case 0x8e: /* MOV Sw Ew */ {
            const auto mrr = GetModRegRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mrr);
            GetSReg16(m_State, mrr.reg) = ReadEA16(m_Memory, m_State, modRm);
            break;
        }
        case 0x8f: /* POP Ev */ {
            const auto mrr = GetModRegRm(m_Memory, m_State);
            // TODO Verify that mrr.reg == 0
            const auto modRm = DecodeModRm(m_Memory, m_State, mrr);
            WriteEA16(m_Memory, m_State, modRm, Pop16(m_Memory, m_State));
            break;
        }
        case 0x90: /* XCHG eAX eAX (NOP) */
        case 0x91: /* XCHG eCX eAX */
        case 0x92: /* XCHG eDX eAX */
        case 0x93: /* XCHG eBX eAX */
        case 0x94: /* XCHG eSP eAX */
        case 0x95: /* XCHG eBP eAX */
        case 0x96: /* XCHG eSI eAX */
        case 0x97: /* XCHG eDI eAX */ {
            uint16_t& reg = GetReg16(m_State, opcode - 0x90);
            uint16_t prev_ax = m_State.m_ax;
            m_State.m_ax = reg;
            reg = prev_ax;
            break;
        }
        case 0x98: /* CBW */ {
            m_State.m_ax = ExtendSign8To16(m_State.m_ax & 0xff);
            break;
        }
        case 0x99: /* CWD */ {
            if (m_State.m_ax & 0x8000)
                m_State.m_dx = 0xffff;
            else
                m_State.m_dx = 0;
            break;
        }
        case 0x9a: /* 0ALL Ap */ {
            const auto ip = getImm16();
            const auto cs = getImm16();
            Push16(m_Memory, m_State, m_State.m_cs);
            Push16(m_Memory, m_State, m_State.m_ip);
            m_State.m_cs = cs;
            m_State.m_ip = ip;
            break;
        }
        case 0x9b: /* WAIT */ {
            todo(); /* XXX Do we need this? */
            break;
        }
        case 0x9c: /* PUSHF */ {
            Push16(m_Memory, m_State, m_State.m_flags);
            break;
        }
        case 0x9d: /* POPF */ {
            auto flags = Pop16(m_Memory, m_State);
            m_State.m_flags = UpdateFlagsForCPU(flags);
            break;
        }
        case 0x9e: /* SAHF */ {
            m_State.m_flags = (m_State.m_flags & 0xff00) | (m_State.m_ax & 0xff00) >> 8;
            break;
        }
        case 0x9f: /* LAHF */ {
            m_State.m_ax = (m_State.m_ax & 0xff) | ((m_State.m_flags & 0xff) << 8);
            break;
        }
        case 0xa0: /* MOV AL Ob */ {
            const auto imm = getImm16();
            const auto seg = HandleSegmentOverride(m_State, SEG_DS);
            m_State.m_ax =
                (m_State.m_ax & 0xff00) | m_Memory.ReadByte(MakeAddr(GetSReg16(m_State, seg), imm));
            break;
        }
        case 0xa1: /* MOV eAX Ov */ {
            const auto imm = getImm16();
            const auto seg = HandleSegmentOverride(m_State, SEG_DS);
            m_State.m_ax = m_Memory.ReadWord(MakeAddr(GetSReg16(m_State, seg), imm));
            break;
        }
        case 0xa2: /* MOV Ob AL */ {
            const auto imm = getImm16();
            const auto seg = HandleSegmentOverride(m_State, SEG_DS);
            m_Memory.WriteByte(MakeAddr(GetSReg16(m_State, seg), imm), m_State.m_ax & 0xff);
            break;
        }
        case 0xa3: /* MOV Ov eAX */ {
            const auto imm = getImm16();
            const auto seg = HandleSegmentOverride(m_State, SEG_DS);
            m_Memory.WriteWord(MakeAddr(GetSReg16(m_State, seg), imm), m_State.m_ax);
            break;
        }
        case 0xa4: /* MOVSB */ {
            int delta = cpu::FlagDirection(m_State.m_flags) ? -1 : 1;
            const auto seg = HandleSegmentOverride(m_State, SEG_DS);
            if (m_State.m_prefix & (cpu::State::PREFIX_REPZ | cpu::State::PREFIX_REPNZ)) {
                while (m_State.m_cx != 0) {
                    m_State.m_cx--;
                    m_Memory.WriteByte(
                        MakeAddr(m_State.m_es, m_State.m_di),
                        m_Memory.ReadByte(MakeAddr(GetSReg16(m_State, seg), m_State.m_si)));
                    m_State.m_si += delta;
                    m_State.m_di += delta;
                }
                m_State.m_prefix &= ~(cpu::State::PREFIX_REPZ | cpu::State::PREFIX_REPNZ);
            } else {
                m_Memory.WriteByte(
                    MakeAddr(m_State.m_es, m_State.m_di),
                    m_Memory.ReadByte(MakeAddr(GetSReg16(m_State, seg), m_State.m_si)));
                m_State.m_si += delta;
                m_State.m_di += delta;
            }
            break;
        }
        case 0xa5: /* MOVSW */ {
            int delta = cpu::FlagDirection(m_State.m_flags) ? -2 : 2;
            const auto seg = HandleSegmentOverride(m_State, SEG_DS);
            if (m_State.m_prefix & (cpu::State::PREFIX_REPZ | cpu::State::PREFIX_REPNZ)) {
                while (m_State.m_cx != 0) {
                    m_State.m_cx--;
                    m_Memory.WriteWord(
                        MakeAddr(m_State.m_es, m_State.m_di),
                        m_Memory.ReadWord(MakeAddr(GetSReg16(m_State, seg), m_State.m_si)));
                    m_State.m_si += delta;
                    m_State.m_di += delta;
                }
                m_State.m_prefix &= ~(cpu::State::PREFIX_REPZ | cpu::State::PREFIX_REPNZ);
            } else {
                m_Memory.WriteWord(
                    MakeAddr(m_State.m_es, m_State.m_di),
                    m_Memory.ReadWord(MakeAddr(GetSReg16(m_State, seg), m_State.m_si)));
                m_State.m_si += delta;
                m_State.m_di += delta;
            }
            break;
        }
        case 0xa6: /* CMPSB */ {
            int delta = cpu::FlagDirection(m_State.m_flags) ? -1 : 1;
            const auto seg = HandleSegmentOverride(m_State, SEG_DS);
            if (m_State.m_prefix & (cpu::State::PREFIX_REPZ | cpu::State::PREFIX_REPNZ)) {
                bool break_on_zf = (m_State.m_prefix & cpu::State::PREFIX_REPNZ) != 0;
                while (m_State.m_cx != 0) {
                    m_State.m_cx--;
                    alu::CMP<8>(m_State.m_flags,
                        m_Memory.ReadByte(MakeAddr(GetSReg16(m_State, seg), m_State.m_si)),
                        m_Memory.ReadByte(MakeAddr(m_State.m_es, m_State.m_di)));
                    m_State.m_si += delta;
                    m_State.m_di += delta;
                    if (cpu::FlagZero(m_State.m_flags) == break_on_zf)
                        break;
                }
                m_State.m_prefix &= ~(cpu::State::PREFIX_REPZ | cpu::State::PREFIX_REPNZ);
            } else {
                alu::CMP<8>(m_State.m_flags,
                    m_Memory.ReadByte(MakeAddr(GetSReg16(m_State, seg), m_State.m_si)),
                    m_Memory.ReadByte(MakeAddr(m_State.m_es, m_State.m_di)));
                m_State.m_si += delta;
                m_State.m_di += delta;
            }
            break;
        }
        case 0xa7: /* CMPSW */ {
            int delta = cpu::FlagDirection(m_State.m_flags) ? -2 : 2;
            const auto seg = HandleSegmentOverride(m_State, SEG_DS);
            if (m_State.m_prefix & (cpu::State::PREFIX_REPZ | cpu::State::PREFIX_REPNZ)) {
                bool break_on_zf = (m_State.m_prefix & cpu::State::PREFIX_REPNZ) != 0;
                while (m_State.m_cx != 0) {
                    m_State.m_cx--;
                    alu::CMP<16>(m_State.m_flags,
                        m_Memory.ReadWord(MakeAddr(GetSReg16(m_State, seg), m_State.m_si)),
                        m_Memory.ReadWord(MakeAddr(m_State.m_es, m_State.m_di)));
                    m_State.m_si += delta;
                    m_State.m_di += delta;
                    if (cpu::FlagZero(m_State.m_flags) == break_on_zf)
                        break;
                }
                m_State.m_prefix &= ~(cpu::State::PREFIX_REPZ | cpu::State::PREFIX_REPNZ);
            } else {
                alu::CMP<16>(m_State.m_flags,
                    m_Memory.ReadByte(MakeAddr(GetSReg16(m_State, seg), m_State.m_si)),
                    m_Memory.ReadByte(MakeAddr(m_State.m_es, m_State.m_di)));
                m_State.m_si += delta;
                m_State.m_di += delta;
            }
            break;
        }
        case 0xa8: /* TEST AL Ib */ {
            const auto imm = getImm8();
            alu::TEST<8>(m_State.m_flags, m_State.m_ax & 0xff, imm);
            break;
        }
        case 0xa9: /* TEST eAX Iv */ {
            const auto imm = getImm16();
            alu::TEST<16>(m_State.m_flags, m_State.m_ax, imm);
            break;
        }
        case 0xaa: /* STOSB */ {
            int delta = cpu::FlagDirection(m_State.m_flags) ? -1 : 1;
            uint8_t value = m_State.m_ax & 0xff;
            if (m_State.m_prefix & (cpu::State::PREFIX_REPZ | cpu::State::PREFIX_REPNZ)) {
                while (m_State.m_cx != 0) {
                    m_State.m_cx--;
                    m_Memory.WriteByte(MakeAddr(m_State.m_es, m_State.m_di), value);
                    m_State.m_di += delta;
                }
                m_State.m_prefix &= ~(cpu::State::PREFIX_REPZ | cpu::State::PREFIX_REPNZ);
            } else {
                m_Memory.WriteByte(MakeAddr(m_State.m_es, m_State.m_di), value);
                m_State.m_di += delta;
            }
            break;
        }
        case 0xab: /* STOSW */ {
            int delta = cpu::FlagDirection(m_State.m_flags) ? -2 : 2;
            if (m_State.m_prefix & (cpu::State::PREFIX_REPZ | cpu::State::PREFIX_REPNZ)) {
                while (m_State.m_cx != 0) {
                    m_State.m_cx--;
                    m_Memory.WriteWord(MakeAddr(m_State.m_es, m_State.m_di), m_State.m_ax);
                    m_State.m_di += delta;
                }
                m_State.m_prefix &= ~(cpu::State::PREFIX_REPZ | cpu::State::PREFIX_REPNZ);
            } else {
                m_Memory.WriteWord(MakeAddr(m_State.m_es, m_State.m_di), m_State.m_ax);
                m_State.m_di += delta;
            }
            break;
        }
        case 0xac: /* LODSB */ {
            const auto seg = HandleSegmentOverride(m_State, SEG_DS);
            m_State.m_ax =
                (m_State.m_ax & 0xff00) | m_Memory.ReadByte(MakeAddr(GetSReg16(m_State, seg), m_State.m_si));
            if (cpu::FlagDirection(m_State.m_flags))
                m_State.m_si--;
            else
                m_State.m_si++;
            break;
        }
        case 0xad: /* LODSW */ {
            const auto seg = HandleSegmentOverride(m_State, SEG_DS);
            m_State.m_ax = m_Memory.ReadWord(MakeAddr(GetSReg16(m_State, seg), m_State.m_si));
            if (cpu::FlagDirection(m_State.m_flags))
                m_State.m_si -= 2;
            else
                m_State.m_si += 2;
            break;
        }
        case 0xae: /* SCASB */ {
            int delta = cpu::FlagDirection(m_State.m_flags) ? -1 : 1;
            uint8_t val = m_State.m_ax & 0xff;
            if (m_State.m_prefix & (cpu::State::PREFIX_REPZ | cpu::State::PREFIX_REPNZ)) {
                bool break_on_zf = (m_State.m_prefix & (cpu::State::PREFIX_REPNZ)) != 0;
                while (m_State.m_cx != 0) {
                    m_State.m_cx--;
                    alu::CMP<8>(m_State.m_flags, val, m_Memory.ReadByte(MakeAddr(m_State.m_es, m_State.m_di)));
                    m_State.m_di += delta;
                    if (cpu::FlagZero(m_State.m_flags) == break_on_zf)
                        break;
                }
                m_State.m_prefix &= ~(cpu::State::PREFIX_REPZ | cpu::State::PREFIX_REPNZ);
            } else {
                alu::CMP<8>(m_State.m_flags, val, m_Memory.ReadByte(MakeAddr(m_State.m_es, m_State.m_di)));
                m_State.m_di += delta;
            }
            break;
        }
        case 0xaf: /* SCASW */ {
            int delta = cpu::FlagDirection(m_State.m_flags) ? -2 : 2;
            if (m_State.m_prefix & (cpu::State::PREFIX_REPZ | cpu::State::PREFIX_REPNZ)) {
                bool break_on_zf = (m_State.m_prefix & (cpu::State::PREFIX_REPNZ)) != 0;
                while (m_State.m_cx != 0) {
                    m_State.m_cx--;
                    alu::CMP<16>(m_State.m_flags, m_State.m_ax, m_Memory.ReadWord(MakeAddr(m_State.m_es, m_State.m_di)));
                    m_State.m_di += delta;
                    if (cpu::FlagZero(m_State.m_flags) == break_on_zf)
                        break;
                }
                m_State.m_prefix &= ~(cpu::State::PREFIX_REPZ | cpu::State::PREFIX_REPNZ);
            } else {
                alu::CMP<16>(m_State.m_flags, m_State.m_ax, m_Memory.ReadWord(MakeAddr(m_State.m_es, m_State.m_di)));
                m_State.m_di += delta;
            }
            break;
        }
        case 0xb0: /* MOV AL Ib */ {
            const auto imm = getImm8();
            m_State.m_ax = (m_State.m_ax & 0xff00) | imm;
            break;
        }
        case 0xb1: /* MOV CL Ib */ {
            const auto imm = getImm8();
            m_State.m_cx = (m_State.m_cx & 0xff00) | imm;
            break;
        }
        case 0xb2: /* MOV DL Ib */ {
            const auto imm = getImm8();
            m_State.m_dx = (m_State.m_dx & 0xff00) | imm;
            break;
        }
        case 0xb3: /* MOV BL Ib */ {
            const auto imm = getImm8();
            m_State.m_bx = (m_State.m_bx & 0xff00) | imm;
            break;
        }
        case 0xb4: /* MOV AH Ib */ {
            const auto imm = getImm8();
            m_State.m_ax = (m_State.m_ax & 0xff) | ((uint16_t)imm << 8);
            break;
        }
        case 0xb5: /* MOV CH Ib */ {
            const auto imm = getImm8();
            m_State.m_cx = (m_State.m_cx & 0xff) | ((uint16_t)imm << 8);
            break;
        }
        case 0xb6: /* MOV DH Ib */ {
            const auto imm = getImm8();
            m_State.m_dx = (m_State.m_dx & 0xff) | ((uint16_t)imm << 8);
            break;
        }
        case 0xb7: /* MOV BH Ib */ {
            const auto imm = getImm8();
            m_State.m_bx = (m_State.m_bx & 0xff) | ((uint16_t)imm << 8);
            break;
        }
        case 0xb8: /* MOV eAX Iv */ {
            const auto imm = getImm16();
            m_State.m_ax = imm;
            break;
        }
        case 0xb9: /* MOV eCX Iv */ {
            const auto imm = getImm16();
            m_State.m_cx = imm;
            break;
        }
        case 0xba: /* MOV eDX Iv */ {
            const auto imm = getImm16();
            m_State.m_dx = imm;
            break;
        }
        case 0xbb: /* MOV eBX Iv */ {
            const auto imm = getImm16();
            m_State.m_bx = imm;
            break;
        }
        case 0xbc: /* MOV eSP Iv */ {
            const auto imm = getImm16();
            m_State.m_sp = imm;
            break;
        }
        case 0xbd: /* MOV eBP Iv */ {
            const auto imm = getImm16();
            m_State.m_bp = imm;
            break;
        }
        case 0xbe: /* MOV eSI Iv */ {
            const auto imm = getImm16();
            m_State.m_si = imm;
            break;
        }
        case 0xbf: /* MOV eDI Iv */ {
            const auto imm = getImm16();
            m_State.m_di = imm;
            break;
        }
        case 0xc0: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0xc1: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0xc2: /* RET Iw */ {
            const auto imm = getImm16();
            m_State.m_ip = Pop16(m_Memory, m_State);
            m_State.m_sp += imm;
            break;
        }
        case 0xc3: /* RET */ {
            m_State.m_ip = Pop16(m_Memory, m_State);
            break;
        }
        case 0xc4: /* LES Gv Mp */
        case 0xc5: /* LDS Gv Mp */ {
            const auto mrr = GetModRegRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mrr);
            uint16_t& reg = GetReg16(m_State, mrr.reg);

            const auto new_off = ReadEA16(m_Memory, m_State, modRm, 0);
            const auto new_seg = ReadEA16(m_Memory, m_State, modRm, 2);

            if (opcode == 0xc4)
                m_State.m_es = new_seg;
            else
                m_State.m_ds = new_seg;
            reg = new_off;
            break;
        }
        case 0xc6: /* MOV Eb Ib */ {
            const auto mrr = GetModRegRm(m_Memory, m_State);
            // TODO Verif that mrr.reg == 0
            const auto modRm = DecodeModRm(m_Memory, m_State, mrr);
            const auto imm = getImm8();
            WriteEA8(m_Memory, m_State, modRm, imm);
            break;
        }
        case 0xc7: /* MOV Ev Iv */ {
            const auto mrr = GetModRegRm(m_Memory, m_State);
            // TODO Verify that mrr.reg == 0
            const auto modRm = DecodeModRm(m_Memory, m_State, mrr);
            const auto imm = getImm16();
            WriteEA16(m_Memory, m_State, modRm, imm);
            break;
        }
        case 0xc8: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0xc9: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0xca: /* RETF Iw */ {
            const auto imm = getImm16();
            m_State.m_ip = Pop16(m_Memory, m_State);
            m_State.m_cs = Pop16(m_Memory, m_State);
            m_State.m_sp += imm;
            break;
        }
        case 0xcb: /* RETF */ {
            m_State.m_ip = Pop16(m_Memory, m_State);
            m_State.m_cs = Pop16(m_Memory, m_State);
            break;
        }
        case 0xcc: /* INT 3 */ {
            HandleInterrupt(INT_BREAKPOINT);
            break;
        }
        case 0xcd: /* INT Ib */ {
            const auto imm = getImm8();
            HandleInterrupt(imm);
            break;
        }
        case 0xce: /* INTO */ {
            if (!cpu::FlagOverflow(m_State.m_flags))
                HandleInterrupt(INT_OVERFLOW);
            break;
        }
        case 0xcf: /* IRET */ {
            m_State.m_ip = Pop16(m_Memory, m_State);
            m_State.m_cs = Pop16(m_Memory, m_State);
            m_State.m_flags = UpdateFlagsForCPU(Pop16(m_Memory, m_State));
            break;
        }
        case 0xd0: /* GRP2 Eb 1 */ {
            const auto mor = GetModOpRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mor);

            uint8_t val = ReadEA8(m_Memory, m_State, modRm);
            switch (mor.op) {
                case 0: // rol
                    val = alu::ROL<8>(m_State.m_flags, val, 1);
                    break;
                case 1: // ror
                    val = alu::ROR<8>(m_State.m_flags, val, 1);
                    break;
                case 2: // rcl
                    val = alu::RCL<8>(m_State.m_flags, val, 1);
                    break;
                case 3: // rcr
                    val = alu::RCR<8>(m_State.m_flags, val, 1);
                    break;
                case 4: // shl
                    val = alu::SHL<8>(m_State.m_flags, val, 1);
                    break;
                case 5: // shr
                    val = alu::SHR<8>(m_State.m_flags, val, 1);
                    break;
                case 6: // undefined
                    invalidOpcode();
                    break;
                case 7: // sar
                    val = alu::SAR<8>(m_State.m_flags, val, 1);
                    break;
            }
            WriteEA8(m_Memory, m_State, modRm, val);
            break;
        }
        case 0xd1: /* GRP2 Ev 1 */ {
            const auto mor = GetModOpRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mor);

            uint16_t val = ReadEA16(m_Memory, m_State, modRm);
            switch (mor.op) {
                case 0: // rol
                    val = alu::ROL<16>(m_State.m_flags, val, 1);
                    break;
                case 1: // ror
                    val = alu::ROR<16>(m_State.m_flags, val, 1);
                    break;
                case 2: // rcl
                    val = alu::RCL<16>(m_State.m_flags, val, 1);
                    break;
                case 3: // rcr
                    val = alu::RCR<16>(m_State.m_flags, val, 1);
                    break;
                case 4: // shl
                    val = alu::SHL<16>(m_State.m_flags, val, 1);
                    break;
                case 5: // shr
                    val = alu::SHR<16>(m_State.m_flags, val, 1);
                    break;
                case 6: // undefined
                    invalidOpcode();
                    break;
                case 7: // sar
                    val = alu::SAR<16>(m_State.m_flags, val, 1);
                    break;
            }
            WriteEA16(m_Memory, m_State, modRm, val);
            break;
        }
        case 0xd2: /* GRP2 Eb CL */ {
            const auto mor = GetModOpRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mor);

            uint8_t val = ReadEA8(m_Memory, m_State, modRm);
            uint8_t cnt = m_State.m_cx & 0xff;
            switch (mor.op) {
                case 0: // rol
                    val = alu::ROL<8>(m_State.m_flags, val, cnt);
                    break;
                case 1: // ror
                    val = alu::ROR<8>(m_State.m_flags, val, cnt);
                    break;
                case 2: // rcl
                    val = alu::RCL<8>(m_State.m_flags, val, cnt);
                    break;
                case 3: // rcr
                    val = alu::RCR<8>(m_State.m_flags, val, cnt);
                    break;
                case 4: // shl
                    val = alu::SHL<8>(m_State.m_flags, val, cnt);
                    break;
                case 5: // shr
                    val = alu::SHR<8>(m_State.m_flags, val, cnt);
                    break;
                case 6: // undefined
                    invalidOpcode();
                    break;
                case 7: // sar
                    val = alu::SAR<8>(m_State.m_flags, val, cnt);
                    break;
            }
            WriteEA8(m_Memory, m_State, modRm, val);
            break;
        }
        case 0xd3: /* GRP2 Ev CL */ {
            const auto mor = GetModOpRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mor);

            uint16_t val = ReadEA16(m_Memory, m_State, modRm);
            uint8_t cnt = m_State.m_cx & 0xff;
            switch (mor.op) {
                case 0: // rol
                    val = alu::ROL<16>(m_State.m_flags, val, cnt);
                    break;
                case 1: // ror
                    val = alu::ROR<16>(m_State.m_flags, val, cnt);
                    break;
                case 2: // rcl
                    val = alu::RCL<16>(m_State.m_flags, val, cnt);
                    break;
                case 3: // rcr
                    val = alu::RCR<16>(m_State.m_flags, val, cnt);
                    break;
                case 4: // shl
                    val = alu::SHL<16>(m_State.m_flags, val, cnt);
                    break;
                case 5: // shr
                    val = alu::SHR<16>(m_State.m_flags, val, cnt);
                    break;
                case 6: // undefined
                    invalidOpcode();
                    break;
                case 7: // sar
                    val = alu::SAR<16>(m_State.m_flags, val, cnt);
                    break;
            }
            WriteEA16(m_Memory, m_State, modRm, val);
            break;
        }
        case 0xd4: /* AAM I0 */ {
            const auto imm = getImm8();
            const auto result = alu::AAM(m_State.m_flags, m_State.m_ax & 0xff, imm);
            if (result) {
                m_State.m_ax = *result;
            } else {
                SignalInterrupt(INT_DIV_BY_ZERO);
            }
            break;
        }
        case 0xd5: /* AAD I0 */ {
            const auto imm = getImm8();
            m_State.m_ax = alu::AAD(m_State.m_flags, m_State.m_ax, imm);
            break;
        }
        case 0xd6: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0xd7: /* XLAT */ {
            const auto seg = HandleSegmentOverride(m_State, SEG_DS);
            m_State.m_ax = (m_State.m_ax & 0xff00) |
                           m_Memory.ReadByte(MakeAddr(seg, m_State.m_bx + m_State.m_ax & 0xff));
            break;
        }
        case 0xd8: /* ESC/0 */
        case 0xd9: /* ESC/1 */
        case 0xda: /* ESC/2 */
        case 0xdb: /* ESC/3 */
        case 0xdc: /* ESC/4 */
        case 0xdd: /* ESC/5 */
        case 0xde: /* ESC/6 */
        case 0xdf: /* ESC/7 */ {
            const auto imm = getImm16();
            spdlog::warn("ignoring unimplemented FPU instruction {:x}", imm);
            break;
        }
        case 0xe0: /* LOOPNZ Jb */ {
            m_State.m_cx--;
            handleConditionalJump(!cpu::FlagZero(m_State.m_flags) && m_State.m_cx != 0);
            break;
        }
        case 0xe1: /* LOOPZ Jb */ {
            m_State.m_cx--;
            handleConditionalJump(cpu::FlagZero(m_State.m_flags) && m_State.m_cx != 0);
            break;
        }
        case 0xe2: /* LOOP Jb */ {
            m_State.m_cx--;
            handleConditionalJump(m_State.m_cx != 0);
            break;
        }
        case 0xe3: /* JCXZ Jb */ {
            handleConditionalJump(m_State.m_cx == 0);
            break;
        }
        case 0xe4: /* IN AL Ib */ {
            const auto imm = getImm8();
            m_State.m_ax = (m_State.m_ax & 0xff00) | m_IO.In8(imm);
            break;
        }
        case 0xe5: /* IN eAX Ib */ {
            const auto imm = getImm8();
            m_State.m_ax = m_IO.In16(imm);
            break;
        }
        case 0xe6: /* OUT Ib AL */ {
            const auto imm = getImm8();
            m_IO.Out8(imm, m_State.m_ax & 0xff);
            break;
        }
        case 0xe7: /* OUT Ib eAX */ {
            const auto imm = getImm16();
            m_IO.Out16(imm, m_State.m_ax & 0xff);
            break;
        }
        case 0xe8: /* CALL Jv */ {
            const auto imm = getImm16();
            Push16(m_Memory, m_State, m_State.m_ip);
            RelativeJump16(m_State.m_ip, imm);
            break;
        }
        case 0xe9: /* JMP Jv */ {
            const auto imm = getImm16();
            RelativeJump16(m_State.m_ip, imm);
            break;
        }
        case 0xea: /* JMP Ap */ {
            const auto ip = getImm16();
            const auto cs = getImm16();
            m_State.m_ip = ip;
            m_State.m_cs = cs;
            break;
        }
        case 0xeb: /* JMP Jb */ {
            handleConditionalJump(true);
            break;
        }
        case 0xec: /* IN AL DX */ {
            m_State.m_ax = (m_State.m_ax & 0xff00) | m_IO.In8(m_State.m_dx);
            break;
        }
        case 0xed: /* IN eAX DX */ {
            m_State.m_ax = m_IO.In16(m_State.m_dx);
            break;
        }
        case 0xee: /* OUT DX AL */ {
            m_IO.Out8(m_State.m_dx, m_State.m_ax & 0xff);
            break;
        }
        case 0xef: /* OUT DX eAX */ {
            m_IO.Out16(m_State.m_dx, m_State.m_ax);
            break;
        }
        case 0xf0: /* LOCK */ {
            todo(); /* XXX Should we? */
            break;
        }
        case 0xf1: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0xf2: /* REPNZ */
        case 0xf3: /* REPZ */ {
            Unreachable();
            break;
        }
        case 0xf4: /* HLT */ {
            todo();
            break;
        }
        case 0xf5: /* CMC */ {
            m_State.m_flags ^= cpu::flag::CF;
            break;
        }
        case 0xf6: /* GRP3a Eb */ {
            const auto mor = GetModOpRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mor);

            switch (mor.op) {
                case 0: /* TEST Eb Ib */ {
                    const auto imm = getImm8();
                    alu::TEST<8>(m_State.m_flags, ReadEA8(m_Memory, m_State, modRm), imm);
                    break;
                }
                case 1: /* invalid */
                    invalidOpcode();
                    break;
                case 2: /* NOT */
                    WriteEA8(m_Memory, m_State, modRm, 0xFF - ReadEA8(m_Memory, m_State, modRm));
                    break;
                case 3: /* NEG */
                    WriteEA8(m_Memory, m_State, modRm, alu::NEG<8>(m_State.m_flags, ReadEA8(m_Memory, m_State, modRm)));
                    break;
                case 4: /* MUL */
                    alu::Mul8(m_State.m_flags, m_State.m_ax, ReadEA8(m_Memory, m_State, modRm));
                    break;
                case 5: /* IMUL */
                    alu::Imul8(m_State.m_flags, m_State.m_ax, ReadEA8(m_Memory, m_State, modRm));
                    break;
                case 6: /* DIV */
                    if (alu::Div8(m_State.m_ax, ReadEA8(m_Memory, m_State, modRm)))
                        SignalInterrupt(INT_DIV_BY_ZERO);
                    break;
                case 7: /* IDIV */
                    if (alu::Idiv8(m_State.m_ax, m_State.m_dx, ReadEA8(m_Memory, m_State, modRm)))
                        SignalInterrupt(INT_DIV_BY_ZERO);
                    break;
            }
            break;
        }
        case 0xf7: /* GRP3b Ev */ {
            const auto mor = GetModOpRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mor);

            switch (mor.op) {
                case 0: /* TEST Eb Iw */ {
                    const auto imm = getImm16();
                    alu::TEST<16>(m_State.m_flags, ReadEA16(m_Memory, m_State, modRm), imm);
                    break;
                }
                case 1: /* invalid */
                    invalidOpcode();
                    break;
                case 2: /* NOT */
                    WriteEA16(m_Memory, m_State, modRm, 0xFFFF - ReadEA16(m_Memory, m_State, modRm));
                    break;
                case 3: /* NEG */
                    WriteEA16(m_Memory, m_State, modRm, alu::NEG<16>(m_State.m_flags, ReadEA16(m_Memory, m_State, modRm)));
                    break;
                case 4: /* MUL */
                    alu::Mul16(m_State.m_flags, m_State.m_ax, m_State.m_dx, ReadEA16(m_Memory, m_State, modRm));
                    break;
                case 5: /* IMUL */
                    alu::Imul16(m_State.m_flags, m_State.m_ax, m_State.m_dx, ReadEA16(m_Memory, m_State, modRm));
                    break;
                case 6: /* DIV */
                    if (alu::Div16(m_State.m_ax, m_State.m_dx, ReadEA16(m_Memory, m_State, modRm)))
                        SignalInterrupt(INT_DIV_BY_ZERO);
                    break;
                case 7: /* IDIV */
                    if (alu::Idiv16(m_State.m_ax, m_State.m_dx, ReadEA16(m_Memory, m_State, modRm)))
                        SignalInterrupt(INT_DIV_BY_ZERO);
                    break;
            }
            break;
        }
        case 0xf8: /* CLC */ {
            m_State.m_flags &= ~cpu::flag::CF;
            break;
        }
        case 0xf9: /* STC */ {
            m_State.m_flags |= cpu::flag::CF;
            break;
        }
        case 0xfa: /* CLI */ {
            m_State.m_flags &= ~cpu::flag::IF;
            break;
        }
        case 0xfb: /* STI */ {
            m_State.m_flags |= cpu::flag::IF;
            break;
        }
        case 0xfc: /* CLD */ {
            m_State.m_flags &= ~cpu::flag::DF;
            break;
        }
        case 0xfd: /* STD */ {
            m_State.m_flags |= cpu::flag::DF;
            break;
        }
        case 0xfe: /* GRP4 Eb */ {
            const auto mor = GetModOpRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mor);

            uint8_t val = ReadEA8(m_Memory, m_State, modRm);
            switch (mor.op) {
                case 0: // inc
                    WriteEA8(m_Memory, m_State, modRm, alu::INC<8>(m_State.m_flags, val));
                    break;
                case 1: // dec
                    WriteEA8(m_Memory, m_State, modRm, alu::DEC<8>(m_State.m_flags, val));
                    break;
                default: // invalid
                    invalidOpcode();
                    break;
            }
            break;
        }
        case 0xff: /* GRP5 Ev */ {
            const auto mor = GetModOpRm(m_Memory, m_State);
            const auto modRm = DecodeModRm(m_Memory, m_State, mor);

            uint16_t val = ReadEA16(m_Memory, m_State, modRm, 0);
            switch (mor.op) {
                case 0: /* INC eV */
                    WriteEA16(m_Memory, m_State, modRm, alu::INC<16>(m_State.m_flags, val));
                    break;
                case 1: /* DEC eV */
                    WriteEA16(m_Memory, m_State, modRm, alu::DEC<16>(m_State.m_flags, val));
                    break;
                case 2: /* CALL Ev */
                    Push16(m_Memory, m_State, m_State.m_ip);
                    m_State.m_ip = val;
                    break;
                case 3: /* CALL Ep */
                    Push16(m_Memory, m_State, m_State.m_cs);
                    Push16(m_Memory, m_State, m_State.m_ip);
                    m_State.m_ip = val;
                    m_State.m_cs = ReadEA16(m_Memory, m_State, modRm, 2);
                    break;
                case 4: /* JMP Ev */
                    m_State.m_ip = val;
                    break;
                case 5: /* JMP Ep */
                    m_State.m_ip = val;
                    m_State.m_cs = ReadEA16(m_Memory, m_State, modRm, 2);
                    break;
                case 6: /* PUSH Ev */
                    Push16(m_Memory, m_State, val);
                    break;
                case 7: /* undefined */
                    invalidOpcode();
                    break;
            }
            break;
        }
    }
}

CPUx86::addr_t CPUx86::MakeAddr(uint16_t seg, uint16_t off)
{
    return ((addr_t)seg << 4) + (addr_t)off;
}

namespace cpu
{

void Dump(const State& st)
{
    spdlog::debug( "  ax={:04x} bx={:04x} cx={:04x} dx={:04x} si={:04x} di={:04x} bp={:04x} flags={:04x}", st.m_ax,
        st.m_bx, st.m_cx, st.m_dx, st.m_si, st.m_di, st.m_bp, st.m_flags);
    spdlog::debug("  cs:ip={:04x}:{:04x} ds={:04x} es={:04x} ss:sp={:04x}:{:04x}", st.m_cs, st.m_ip, st.m_ds, st.m_es, st.m_ss,
        st.m_sp);
}

}

void CPUx86::Dump() { cpu::Dump(m_State); }

void CPUx86::SignalInterrupt(uint8_t no)
{
    spdlog::error("SignalInterrupt(): no {:x}", no);
    std::abort(); // TODO
}

void CPUx86::HandleInterrupt(uint8_t no)
{
    // Push flags and return address
    Push16(m_Memory, m_State, m_State.m_flags);
    Push16(m_Memory, m_State, m_State.m_cs);
    Push16(m_Memory, m_State, m_State.m_ip);

    // Transfer control to interrupt
    const auto vectorAddress = MakeAddr(0, no * 4);
    m_State.m_ip = m_Memory.ReadWord(vectorAddress + 0);
    m_State.m_cs = m_Memory.ReadWord(vectorAddress + 2);
}

/* vim:set ts=4 sw=4: */
