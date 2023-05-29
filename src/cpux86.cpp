#include "cpux86.h"
#include "io.h"
#include "memory.h"
#include "vectors.h"
#include <stdio.h>
#include <stdlib.h>
#include <bit>
#include <utility>

#define TRACE(x...) \
    if (0) fprintf(stderr, "[cpu] " x)

CPUx86::CPUx86(Memory& oMemory, IO& oIO, Vectors& oVectors)
    : m_Memory(oMemory), m_IO(oIO), m_Vectors(oVectors)
{
}

CPUx86::~CPUx86() = default;

void CPUx86::Reset()
{
    m_State.m_prefix = 0;
    m_State.m_flags = 0;
    m_State.m_cs = 0xffff;
    m_State.m_ip = 0;
    m_State.m_ds = 0;
    m_State.m_es = 0;
    m_State.m_ss = 0;

    m_State.m_ax = 0x1234;
}

namespace
{
    auto ModRm_XXX(const uint8_t modRm) { return (modRm >> 3) & 7; }

    template<unsigned int BITS>
    struct UintOfImpl;
    template<>
    struct UintOfImpl<8> {
        using type = uint8_t;
    };
    template<>
    struct UintOfImpl<16> {
        using type = uint16_t;
    };
    template<>
    struct UintOfImpl<32> {
        using type = uint32_t;
    };

    template<unsigned int BITS>
    using UintOf = UintOfImpl<BITS>::type;

    template<unsigned int BITS>
    constexpr void SetFlagsSZP(uint16_t& flags, UintOf<BITS> v);

    template<>
    constexpr void SetFlagsSZP<8>(uint16_t& flags, uint8_t n)
    {
        if (n & 0x80)
            flags |= cpu::flag::SF;
        if (n == 0)
            flags |= cpu::flag::ZF;
        const uint8_t pf = std::popcount(static_cast<uint8_t>(n & 0xff));
        if ((pf & 1) == 0) flags |= cpu::flag::PF;
    }

    template<>
    constexpr void SetFlagsSZP<16>(uint16_t& flags, uint16_t n)
    {
        if (n & 0x8000)
            flags |= cpu::flag::SF;
        if (n == 0)
            flags |= cpu::flag::ZF;
        // TODO
        uint8_t pf =
            ~((n & 0x80) ^ (n & 0x40) ^ (n & 0x20) ^ (n & 0x10) ^ (n & 0x08) ^ (n & 0x04) ^
              (n & 0x02) ^ (n & 0x01));
        if (pf & 1)
            flags |= cpu::flag::PF;
    }

    template<unsigned int BITS>
    constexpr void SetFlagsArith(uint16_t& flags, UintOf<BITS> a, UintOf<BITS> b, UintOf<2 * BITS> res);

    template<>
    constexpr void SetFlagsArith<8>(uint16_t& flags, uint8_t a, uint8_t b, uint16_t res)
    {
        flags &=
            ~(cpu::flag::OF | cpu::flag::SF | cpu::flag::ZF |
              cpu::flag::PF | cpu::flag::CF);
        SetFlagsSZP<8>(flags, res);
        if (res & 0xff00)
            flags |= cpu::flag::CF;
        // https://teaching.idallen.com/dat2343/10f/notes/040_overflow.txt
        // Overflow can only happen when adding two numbers of the same sign and
        // getting a different sign.  So, to detect overflow we don't care about
        // any bits except the sign bits.  Ignore the other bits.
        const auto sign_a = (a & 0x80) != 0;
        const auto sign_b = (b & 0x80) != 0;
        const auto sign_res = (res & 0x80) != 0;
        if (sign_a == sign_b && sign_res != sign_a)
            flags |= cpu::flag::OF;
        if ((a ^ b ^ res) & 0x10)
            flags |= cpu::flag::AF;
    }

    template<>
    constexpr void SetFlagsArith<16>(uint16_t& flags, uint16_t a, uint16_t b, uint32_t res)
    {
        flags &=
            ~(cpu::flag::OF | cpu::flag::SF | cpu::flag::ZF |
              cpu::flag::PF | cpu::flag::CF);
        SetFlagsSZP<16>(flags, res);
        if (res & 0xffff0000)
            flags |= cpu::flag::CF;
        if ((a ^ res) & (a ^ b) & 0x8000)
            flags |= cpu::flag::OF;
        if ((a ^ b ^ res) & 0x10)
            flags |= cpu::flag::AF;
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr auto ROL(uint16_t& flags, UintOf<BITS> v, uint8_t n)
    {
        uint8_t cnt = n % BITS;
        if (cnt > 0) {
            v = (v << cnt) | (v >> (BITS - cnt));
            cpu::SetFlag<cpu::flag::CF>(flags, v & 1);
        }
        if (n == 1)
            cpu::SetFlag<cpu::flag::OF>(
                flags, ((v & (1 << (BITS - 1))) ^ (cpu::FlagCarry(flags) ? 1 : 0)));
        return v;
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr auto ROR(uint16_t& flags, UintOf<BITS> v, uint8_t n)
    {
        const uint8_t cnt = n % BITS;
        if (cnt > 0) {
            v = (v >> cnt) | (v << (BITS - cnt));
            cpu::SetFlag<cpu::flag::CF>(flags, v & (1 << (BITS - 1)));
        }
        if (n == 1)
            cpu::SetFlag<cpu::flag::OF>(flags, (v & (1 << (BITS - 1))) ^ (v & (1 << (BITS - 2))));
        return v;
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr auto RCL(uint16_t& flags, UintOf<BITS> v, uint8_t n)
    {
        const uint8_t cnt = (n & 0x1f) % (BITS + 1);
        if (cnt > 0) {
            const uint8_t tmp =
                (v << cnt) | ((cpu::FlagCarry(flags) ? 1 : 0) << (cnt - 1)) | (v >> ((BITS + 1) - cnt));
            cpu::SetFlag<cpu::flag::CF>(flags, (v >> (BITS - cnt)) & 1);
            v = tmp;
        }
        if (n == 1)
            cpu::SetFlag<cpu::flag::OF>(
                flags, ((v & (1 << (BITS - 1))) ^ (cpu::FlagCarry(flags) ? 1 : 0)));
        return v;
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr auto RCR(uint16_t& flags, UintOf<BITS> v, uint8_t n)
    {
        if (n == 1)
            cpu::SetFlag<cpu::flag::OF>(
                flags, ((v & (1 << (BITS - 1))) ^ (cpu::FlagCarry(flags) ? 1 : 0)));
        const uint8_t cnt = (n & 0x1f) % (BITS + 1);
        if (cnt == 0)
            return v;
        const UintOf<BITS> tmp =
            (v >> cnt) | ((cpu::FlagCarry(flags) ? 1 : 0)) << (BITS - cnt) | (v << ((BITS + 1) - cnt));
        cpu::SetFlag<cpu::flag::CF>(flags, (v >> (cnt - 1) & 1));
        return tmp;
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr auto SHL(uint16_t& flags, UintOf<BITS> v, uint8_t n)
    {
        const uint8_t cnt = n & 0x1f;
        if (cnt < BITS) {
            if (cnt > 0)
                cpu::SetFlag<cpu::flag::CF>(flags, v & (1 << (BITS - cnt)));
            return v << cnt;
        } else {
            cpu::SetFlag<cpu::flag::CF>(flags, false);
            return 0;
        }
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr auto SHR(uint16_t& flags, UintOf<BITS> v, uint8_t n)
    {
        const uint8_t cnt = n & 0x1f;
        if (cnt < BITS) {
            if (cnt > 0)
                cpu::SetFlag<cpu::flag::CF>(flags, v & (1 << cnt));
            v >>= cnt;
        } else {
            v = 0;
            cpu::SetFlag<cpu::flag::CF>(flags, false);
        }
        if (n == 1)
            cpu::SetFlag<cpu::flag::OF>(flags, (v & (1 << (BITS - 1))) ^ (v & (1 << (BITS - 2))));
        SetFlagsSZP<BITS>(flags, v);
        return v;
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr auto SAR(uint16_t& flags, UintOf<BITS> v, uint8_t n)
    {
        uint8_t cnt = n & 0x1f;
        if (cnt > 0)
            cpu::SetFlag<cpu::flag::CF>(flags, v & (1 << cnt));
        if (cnt < BITS) {
            if (v & (1 << (BITS - 1))) {
                v = (v >> cnt) | (0xff << (BITS - cnt));
            } else {
                v >>= cnt;
            }
        } else /* cnt >= BITS */ {
            if (v & (1 << (BITS - 1))) {
                cpu::SetFlag<cpu::flag::CF>(flags, true);
                v = (1 << BITS) - 1;
            } else {
                cpu::SetFlag<cpu::flag::CF>(flags, false);
                v = 0;
            }
        }
        if (n == 1)
            cpu::SetFlag<cpu::flag::OF>(flags, 0);
        SetFlagsSZP<BITS>(flags, v);
        return v;
    }

    [[nodiscard]] constexpr uint8_t Add8(uint16_t& flags, uint8_t a, uint8_t b)
    {
        uint16_t res = a + b;
        SetFlagsArith<8>(flags, a, b, res);
        return res & 0xff;
    }

#if 0
    namespace testing {
        using VerifyAddTuple = std::tuple<uint8_t, uint8_t, uint16_t, uint8_t>;
        constexpr bool VerifyAdd8(const VerifyAddTuple& t)
        {
            auto [ val1, val2, expected_flags, expected_result ] = t;
            uint16_t flags = cpu::flag::ON;
            auto result = Add8(flags, val1, val2);
            return result == expected_result && flags == expected_flags;
        }
        #include "test_add8.cpp"
    }
#endif

    [[nodiscard]] constexpr uint16_t Add16(uint16_t& flags, uint16_t a, uint16_t b)
    {
        uint32_t res = a + b;
        SetFlagsArith<16>(flags, a, b, res);
        return res & 0xffff;
    }

    [[nodiscard]] constexpr uint8_t Or8(uint16_t& flags, uint8_t a, uint8_t b)
    {
        uint8_t op1 = a | b;
        flags &=
            ~(cpu::flag::OF | cpu::flag::SF | cpu::flag::ZF | cpu::flag::PF | cpu::flag::CF);
        SetFlagsSZP<8>(flags, op1);
        return op1;
    }

    [[nodiscard]] constexpr uint16_t Or16(uint16_t& flags, uint16_t a, uint16_t b)
    {
        uint16_t op1 = a | b;
        flags &=
            ~(cpu::flag::OF | cpu::flag::SF | cpu::flag::ZF | cpu::flag::PF | cpu::flag::CF);
        SetFlagsSZP<16>(flags, op1);
        return op1;
    }

    [[nodiscard]] constexpr uint8_t And8(uint16_t& flags, uint8_t a, uint8_t b)
    {
        uint8_t op1 = a & b;
        flags &=
            ~(cpu::flag::OF | cpu::flag::SF | cpu::flag::ZF | cpu::flag::PF | cpu::flag::CF);
        SetFlagsSZP<8>(flags, op1);
        return op1;
    }

    [[nodiscard]] constexpr uint16_t And16(uint16_t& flags, uint16_t a, uint16_t b)
    {
        uint16_t op1 = a & b;
        flags &=
            ~(cpu::flag::OF | cpu::flag::SF | cpu::flag::ZF | cpu::flag::PF | cpu::flag::CF);
        SetFlagsSZP<16>(flags, op1);
        return op1;
    }

    [[nodiscard]] constexpr uint8_t Xor8(uint16_t& flags, uint8_t a, uint8_t b)
    {
        uint8_t op1 = a ^ b;
        flags &=
            ~(cpu::flag::OF | cpu::flag::SF | cpu::flag::ZF | cpu::flag::PF | cpu::flag::CF);
        SetFlagsSZP<8>(flags, op1);
        return op1;
    }

    [[nodiscard]] constexpr uint16_t Xor16(uint16_t& flags, uint16_t a, uint16_t b)
    {
        uint16_t op1 = a ^ b;
        flags &=
            ~(cpu::flag::OF | cpu::flag::SF | cpu::flag::ZF | cpu::flag::PF | cpu::flag::CF);
        SetFlagsSZP<16>(flags, op1);
        return op1;
    }

    [[nodiscard]] constexpr uint8_t Adc8(uint16_t& flags, uint8_t a, uint8_t b)
    {
        uint16_t res = a + b + cpu::FlagCarry(flags) ? 1 : 0;
        SetFlagsArith<8>(flags, a, b, res);
        return res & 0xff;
    }

    [[nodiscard]] constexpr uint16_t Adc16(uint16_t& flags, uint16_t a, uint16_t b)
    {
        uint32_t res = a + b + cpu::FlagCarry(flags) ? 1 : 0;
        SetFlagsArith<16>(flags, a, b, res);
        return res & 0xffff;
    }

    [[nodiscard]] constexpr uint8_t Sub8(uint16_t& flags, uint8_t a, uint8_t b)
    {
        uint16_t res = a - b;
        SetFlagsArith<8>(flags, a, b, res);
        return res & 0xff;
    }

    [[nodiscard]] constexpr uint16_t Sub16(uint16_t& flags, uint16_t a, uint16_t b)
    {
        uint32_t res = a - b;
        SetFlagsArith<16>(flags, a, b, res);
        return res & 0xffff;
    }

    [[nodiscard]] constexpr uint8_t Sbb8(uint16_t& flags, uint8_t a, uint8_t b)
    {
        uint16_t res = a - b - cpu::FlagCarry(flags) ? 1 : 0;
        SetFlagsArith<8>(flags, a, b, res);
        return res & 0xff;
    }

    [[nodiscard]] constexpr uint16_t Sbb16(uint16_t& flags, uint16_t a, uint16_t b)
    {
        uint32_t res = a - b - cpu::FlagCarry(flags) ? 1 : 0;
        SetFlagsArith<16>(flags, a, b, res);
        return res & 0xffff;
    }

    [[nodiscard]] constexpr uint8_t Inc8(uint16_t& flags, uint8_t a)
    {
        const auto carry = cpu::FlagCarry(flags);
        uint8_t res = Add8(flags, a, 1);
        cpu::SetFlag<cpu::flag::CF>(flags, carry);
        return res;
    }

    [[nodiscard]] constexpr uint16_t Inc16(uint16_t& flags, uint16_t a)
    {
        const bool carry = cpu::FlagCarry(flags);
        uint16_t res = Add16(flags, a, 1);
        cpu::SetFlag<cpu::flag::CF>(flags, carry);
        return res;
    }

    [[nodiscard]] constexpr uint8_t Dec8(uint16_t& flags, uint8_t a)
    {
        bool carry = cpu::FlagCarry(flags);
        uint8_t res = Sub8(flags, a, 1);
        cpu::SetFlag<cpu::flag::CF>(flags, carry);
        return res;
    }

    [[nodiscard]] constexpr uint16_t Dec16(uint16_t& flags, uint16_t a)
    {
        bool carry = cpu::FlagCarry(flags);
        uint16_t res = Sub16(flags, a, 1);
        cpu::SetFlag<cpu::flag::CF>(flags, carry);
        return res;
    }

    constexpr void Mul8(uint16_t& flags, uint16_t& ax, uint8_t a)
    {
        flags &= ~(cpu::flag::CF | cpu::flag::OF);
        ax = (ax & 0xff) * (uint16_t)a;
        if (ax >= 0x100)
            flags |= (cpu::flag::CF | cpu::flag::OF);
    }

    constexpr void Mul16(uint16_t& flags, uint16_t& ax, uint16_t& dx, uint16_t a)
    {
        flags &= ~(cpu::flag::CF | cpu::flag::OF);
        dx = (ax * a) >> 16;
        ax = (ax * a) & 0xffff;
        if (dx != 0)
            flags |= (cpu::flag::CF | cpu::flag::OF);
    }

    constexpr void Imul8(uint16_t& flags, uint16_t& ax, uint8_t a)
    {
        flags &= ~(cpu::flag::CF | cpu::flag::OF);
        ax = (ax & 0xff) * (uint16_t)a;
        const uint8_t ah = (ax & 0xff00) >> 8;
        cpu::SetFlag<cpu::flag::CF | cpu::flag::OF>(flags, ah == 0 || ah == 0xff);
    }

    constexpr void Imul16(uint16_t& flags, uint16_t& ax, uint16_t& dx, uint16_t a)
    {
        flags &= ~(cpu::flag::CF | cpu::flag::OF);
        uint32_t res = static_cast<uint32_t>(ax) * static_cast<uint32_t>(a);
        dx = res >> 16;
        ax = res & 0xffff;
        cpu::SetFlag<cpu::flag::CF | cpu::flag::OF>(flags, dx == 0 || dx == 0xffff);
    }

    // Returns true to signal interrupt
    [[nodiscard]] constexpr bool Div8(uint16_t& ax, uint8_t a)
    {
        if (a == 0)
            return true;
        if ((ax / a) > 0xff)
            return true;
        ax = ((ax % a) & 0xff) << 8 | ((ax / a) & 0xff);
        // TODO This does not influence flags??
        return false;
    }

    // Returns true to signal interrupt
    [[nodiscard]] constexpr bool Div16(uint16_t& ax, uint16_t& dx, uint16_t a)
    {
        if (a == 0)
            return true;
        auto v = (static_cast<uint32_t>(dx) << 16) | static_cast<uint32_t>(ax);
        if ((v / a) > 0xffff)
            return true;
        ax = (v / a) & 0xffff;
        dx = (v % a) & 0xffff;
        // TODO This does not influence flags??
        return false;
    }

    // Returns true to signal interrupt
    [[nodiscard]] constexpr bool Idiv8(uint16_t& ax, uint16_t& dx, uint8_t a)
    {
        if (a == 0)
            return true;
        int8_t res = ax / a;
        if ((static_cast<int16_t>(ax) > 0 && res > 0x7f) || (static_cast<int16_t>(ax) < 0 && res < 0x80))
            return true;
        ax = ((ax % a) & 0xff) << 8 | ((ax / a) & 0xff);
        // TODO This does not influence flags??
        return false;
    }

    // Returns true to signal interrupt
    [[nodiscard]] constexpr bool Idiv16(uint16_t& ax, uint16_t& dx, uint16_t a)
    {
        if (a == 0)
            return true;
        int32_t v = (static_cast<uint32_t>(dx) << 16) | static_cast<uint32_t>(ax);
        if ((v > 0 && (v / a) > 0x7ffff) || (v < 0 && (v / a) < 0x80000))
            return true;
        ax = (v / a) & 0xffff;
        dx = (v % a) & 0xffff;
        // TODO This does not influence flags??
        return false;
    }

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

}

void CPUx86::RunInstruction()
{
#if 0
    VerifyAddTuple t{ 0x01, 0x0f, 0x0012, 0x10 };
    auto [ val1, val2, expected_flags, expected_result ] = t;
    uint16_t flags = cpu::flag::ON;
    auto result = Add8(flags, val1, val2);
    printf("result %x == expected_result %x\n", result, expected_result);
    printf("flags %x == expected_flags %x\n", flags, expected_flags);
    std::abort();
#endif

    auto getImm8 = [&]() { return GetNextOpcode(); };
    auto getModRm = [&]() { return GetNextOpcode(); };
    auto getImm16 = [&]() -> uint16_t {
        uint16_t a = GetNextOpcode();
        uint16_t b = GetNextOpcode();
        return a | (b << 8);
    };

    auto handleConditionalJump = [&](bool take) {
        const auto imm = getImm8();
        if (take)
            RelativeJump8(m_State.m_ip, imm);
    };

    auto todo = []() { TRACE("TODO\n"); };
    auto invalidOpcode = []() { TRACE("invalidOpcode()\n"); std::abort(); };

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
#define Op_EvGv(op)                 \
    const auto modrm = getModRm();  \
    DecodeEA(modrm, m_DecodeState); \
    WriteEA16(m_DecodeState, op##16(m_State.m_flags, ReadEA16(m_DecodeState), GetReg16(ModRm_XXX(modrm))))

    // op Gv Ev -> Gv = op(Gv, Ev)
#define Op_GvEv(op)                             \
    const auto modrm = getModRm();              \
    DecodeEA(modrm, m_DecodeState);             \
    uint16_t& reg = GetReg16(ModRm_XXX(modrm)); \
    reg = op##16(m_State.m_flags, reg, ReadEA16(m_DecodeState))

    // Op Eb Gb -> Eb = op(Eb, Gb)
#define Op_EbGb(op)                                   \
    const auto modrm = getModRm();                    \
    DecodeEA(modrm, m_DecodeState);                   \
    unsigned int shift;                               \
    uint16_t& reg = GetReg8(ModRm_XXX(modrm), shift); \
    WriteEA8(m_DecodeState, op##8(m_State.m_flags, ReadEA8(m_DecodeState), (reg >> shift) & 0xff))

    // Op Gb Eb -> Gb = op(Gb, Eb)
#define Op_GbEb(op)                                   \
    const auto modrm = getModRm();                    \
    DecodeEA(modrm, m_DecodeState);                   \
    unsigned int shift;                               \
    uint16_t& reg = GetReg8(ModRm_XXX(modrm), shift); \
    SetReg8(reg, shift, op##8(m_State.m_flags, (reg >> shift) & 0xff, ReadEA8(m_DecodeState)))


    const auto opcode = GetNextOpcode();
    TRACE("cs:ip=%04x:%04x opcode 0x%02x\n", m_State.m_cs, m_State.m_ip - 1, opcode);
    switch (opcode) {
        case 0x00: /* ADD Eb Gb */ {
            Op_EbGb(Add);
            break;
        }
        case 0x01: /* ADD Ev Gv */ {
            Op_EvGv(Add);
            break;
        }
        case 0x02: /* ADD Gb Eb */ {
            Op_GbEb(Add);
            break;
        }
        case 0x03: /* ADD Gv Ev */ {
            Op_GvEv(Add);
            break;
        }
        case 0x04: /* ADD AL Ib */ {
            const auto imm = getImm8();
            m_State.m_ax = (m_State.m_ax & 0xff00) | Add8(m_State.m_flags, m_State.m_ax & 0xff, imm);
            break;
        }
        case 0x05: /* ADD eAX Iv */ {
            const auto imm = getImm16();
            m_State.m_ax = Add16(m_State.m_flags, m_State.m_ax, imm);
            break;
        }
        case 0x06: /* PUSH ES */ {
            Push16(m_State.m_es);
            break;
        }
        case 0x07: /* POP ES */ {
            m_State.m_es = Pop16();
            break;
        }
        case 0x08: /* OR Eb Gb */ {
            Op_EbGb(Or);
            break;
        }
        case 0x09: /* OR Ev Gv */ {
            Op_EvGv(Or);
            break;
        }
        case 0x0a: /* OR Gb Eb */ {
            Op_GbEb(Or);
            break;
        }
        case 0x0b: /* OR Gv Ev */ {
            Op_GvEv(Or);
            break;
        }
        case 0x0c: /* OR AL Ib */ {
            const auto imm = getImm8();
            m_State.m_ax = (m_State.m_ax & 0xff00) | Or8(m_State.m_flags, m_State.m_ax & 0xff, imm);
            break;
        }
        case 0x0d: /* OR eAX Iv */ {
            const auto imm = getImm16();
            m_State.m_ax = Or16(m_State.m_flags, m_State.m_ax, imm);
            break;
        }
        case 0x0e: /* PUSH CS */ {
            Push16(m_State.m_cs);
            break;
        }
        case 0x0f: /* -- */ {
            Handle0FPrefix();
            break;
        }
        case 0x10: /* ADC Eb Gb */ {
            Op_EbGb(Adc);
            break;
        }
        case 0x11: /* ADC Ev Gv */ {
            Op_EvGv(Adc);
            break;
        }
        case 0x12: /* ADC Gb Eb */ {
            Op_GbEb(Adc);
            break;
        }
        case 0x13: /* ADC Gv Ev */ {
            Op_GvEv(Adc);
            break;
        }
        case 0x14: /* ADC AL Ib */ {
            const auto imm = getImm8();
            m_State.m_ax = (m_State.m_ax & 0xff00) | Adc8(m_State.m_flags, m_State.m_ax & 0xff, imm);
            break;
        }
        case 0x15: /* ADC eAX Iv */ {
            const auto imm = getImm16();
            m_State.m_ax = Adc16(m_State.m_flags, m_State.m_ax, imm);
            break;
        }
        case 0x16: /* PUSH SS */ {
            Push16(m_State.m_ss);
            break;
        }
        case 0x17: /* POP SS */ {
            m_State.m_ss = Pop16();
            break;
        }
        case 0x18: /* SBB Eb Gb */ {
            Op_EbGb(Sbb);
            break;
        }
        case 0x19: /* SBB Ev Gv */ {
            Op_EvGv(Sbb);
            break;
        }
        case 0x1a: /* SBB Gb Eb */ {
            Op_GbEb(Sbb);
            break;
        }
        case 0x1b: /* SBB Gv Ev */ {
            Op_GvEv(Sbb);
            break;
        }
        case 0x1c: /* SBB AL Ib */ {
            const auto imm = getImm8();
            m_State.m_ax = (m_State.m_ax & 0xff00) | Sbb8(m_State.m_flags, m_State.m_ax & 0xff, imm);
            break;
        }
        case 0x1d: /* SBB eAX Iv */ {
            const auto imm = getImm16();
            m_State.m_ax = Sbb16(m_State.m_flags, m_State.m_ax, imm);
            break;
        }
        case 0x1e: /* PUSH DS */ {
            Push16(m_State.m_ds);
            break;
        }
        case 0x1f: /* POP DS */ {
            m_State.m_ds = Pop16();
            break;
        }
        case 0x20: /* AND Eb Gb */ {
            Op_EbGb(And);
            break;
        }
        case 0x21: /* AND Ev Gv */ {
            Op_EvGv(And);
            break;
        }
        case 0x22: /* AND Gb Eb */ {
            Op_GbEb(And);
            break;
        }
        case 0x23: /* AND Gv Ev */ {
            Op_GvEv(And);
            break;
        }
        case 0x24: /* AND AL Ib */ {
            const auto imm = getImm8();
            m_State.m_ax = (m_State.m_ax & 0xff00) | And8(m_State.m_flags, m_State.m_ax & 0xff, imm);
            break;
        }
        case 0x25: /* AND eAX Iv */ {
            const auto imm = getImm16();
            m_State.m_ax = And16(m_State.m_flags, m_State.m_ax, imm);
            break;
        }
        case 0x26: /* ES: */ {
            m_State.m_prefix |= cpu::State::PREFIX_SEG;
            m_State.m_seg_override = SEG_ES;
            break;
        }
        case 0x27: /* DAA */ {
            todo();
            break;
        }
        case 0x28: /* SUB Eb Gb */ {
            Op_EbGb(Sub);
            break;
        }
        case 0x29: /* SUB Ev Gv */ {
            Op_EvGv(Sub);
            break;
        }
        case 0x2a: /* SUB Gb Eb */ {
            Op_GbEb(Sub);
            break;
        }
        case 0x2b: /* SUB Gv Ev */ {
            Op_GvEv(Sub);
            break;
        }
        case 0x2c: /* SUB AL Ib */ {
            const auto imm = getImm8();
            m_State.m_ax = (m_State.m_ax & 0xff00) | Sub8(m_State.m_flags, m_State.m_ax & 0xff, imm);
            break;
        }
        case 0x2d: /* SUB eAX Iv */ {
            const auto imm = getImm16();
            m_State.m_ax = Sub16(m_State.m_flags, m_State.m_ax, imm);
            break;
        }
        case 0x2e: /* CS: */ {
            m_State.m_prefix |= cpu::State::PREFIX_SEG;
            m_State.m_seg_override = SEG_CS;
            break;
        }
        case 0x2f: /* DAS */ {
            todo();
            break;
        }
        case 0x30: /* XOR Eb Gb */ {
            Op_EbGb(Xor);
            break;
        }
        case 0x31: /* XOR Ev Gv */ {
            Op_EvGv(Xor);
            break;
        }
        case 0x32: /* XOR Gb Eb */ {
            Op_GbEb(Xor);
            break;
        }
        case 0x33: /* XOR Gv Ev */ {
            Op_GvEv(Xor);
            break;
        }
        case 0x34: /* XOR AL Ib */ {
            const auto imm = getImm8();
            m_State.m_ax = (m_State.m_ax & 0xff00) | Xor8(m_State.m_flags, m_State.m_ax & 0xff, imm);
            break;
        }
        case 0x35: /* XOR eAX Iv */ {
            const auto imm = getImm16();
            m_State.m_ax = Xor16(m_State.m_flags, m_State.m_ax, imm);
            break;
        }
        case 0x36: /* SS: */ {
            m_State.m_prefix |= cpu::State::PREFIX_SEG;
            m_State.m_seg_override = SEG_SS;
            break;
        }
        case 0x37: /* AAA */ {
            todo();
            break;
        }
        case 0x38: /* CMP Eb Gb */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);
            unsigned int shift;
            uint16_t& reg = GetReg8(ModRm_XXX(modrm), shift);
            [[maybe_unused]] auto _ = Sub8(m_State.m_flags, ReadEA8(m_DecodeState), (reg >> shift) & 0xff);
            break;
        }
        case 0x39: /* CMP Ev Gv */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);
            [[maybe_unused]] auto _  = Sub16(m_State.m_flags, ReadEA16(m_DecodeState), GetReg16(ModRm_XXX(modrm)));
            break;
        }
        case 0x3a: /* CMP Gb Eb */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);
            unsigned int shift;
            uint16_t& reg = GetReg8(ModRm_XXX(modrm), shift);
            [[maybe_unused]] auto _  = Sub8(m_State.m_flags, (reg >> shift) & 0xff, ReadEA8(m_DecodeState));
            break;
        }
        case 0x3b: /* CMP Gv Ev */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);
            [[maybe_unused]] auto _  = Sub16(m_State.m_flags, GetReg16(ModRm_XXX(modrm)), ReadEA16(m_DecodeState));
            break;
        }
        case 0x3c: /* CMP AL Ib */ {
            const auto imm = getImm8();
            [[maybe_unused]] auto _  = Sub8(m_State.m_flags, m_State.m_ax & 0xff, imm);
            break;
        }
        case 0x3d: /* CMP eAX Iv */ {
            const auto imm = getImm16();
            [[maybe_unused]] auto _  = Sub16(m_State.m_flags, m_State.m_ax, imm);
            break;
        }
        case 0x3e: /* DS: */ {
            m_State.m_prefix |= cpu::State::PREFIX_SEG;
            m_State.m_seg_override = SEG_DS;
            break;
        }
        case 0x3f: /* AAS */ {
            todo();
            break;
        }
        case 0x40: /* INC eAX */ {
            m_State.m_ax = Inc16(m_State.m_flags, m_State.m_ax);
            break;
        }
        case 0x41: /* INC eCX */ {
            m_State.m_cx = Inc16(m_State.m_flags, m_State.m_cx);
            break;
        }
        case 0x42: /* INC eDX */ {
            m_State.m_dx = Inc16(m_State.m_flags, m_State.m_dx);
            break;
        }
        case 0x43: /* INC eBX */ {
            m_State.m_bx = Inc16(m_State.m_flags, m_State.m_bx);
            break;
        }
        case 0x44: /* INC eSP */ {
            m_State.m_sp = Inc16(m_State.m_flags, m_State.m_sp);
            break;
        }
        case 0x45: /* INC eBP */ {
            m_State.m_bp = Inc16(m_State.m_flags, m_State.m_bp);
            break;
        }
        case 0x46: /* INC eSI */ {
            m_State.m_si = Inc16(m_State.m_flags, m_State.m_si);
            break;
        }
        case 0x47: /* INC eDI */ {
            m_State.m_di = Inc16(m_State.m_flags, m_State.m_di);
            break;
        }
        case 0x48: /* DEC eAX */ {
            m_State.m_ax = Dec16(m_State.m_flags, m_State.m_ax);
            break;
        }
        case 0x49: /* DEC eCX */ {
            m_State.m_cx = Dec16(m_State.m_flags, m_State.m_cx);
            break;
        }
        case 0x4a: /* DEC eDX */ {
            m_State.m_dx = Dec16(m_State.m_flags, m_State.m_dx);
            break;
        }
        case 0x4b: /* DEC eBX */ {
            m_State.m_bx = Dec16(m_State.m_flags, m_State.m_bx);
            break;
        }
        case 0x4c: /* DEC eSP */ {
            m_State.m_sp = Dec16(m_State.m_flags, m_State.m_sp);
            break;
        }
        case 0x4d: /* DEC eBP */ {
            m_State.m_bp = Dec16(m_State.m_flags, m_State.m_bp);
            break;
        }
        case 0x4e: /* DEC eSI */ {
            m_State.m_si = Dec16(m_State.m_flags, m_State.m_si);
            break;
        }
        case 0x4f: /* DEC eDI */ {
            m_State.m_di = Dec16(m_State.m_flags, m_State.m_di);
            break;
        }
        case 0x50: /* PUSH eAX */ {
            Push16(m_State.m_ax);
            break;
        }
        case 0x51: /* PUSH eCX */ {
            Push16(m_State.m_cx);
            break;
        }
        case 0x52: /* PUSH eDX */ {
            Push16(m_State.m_dx);
            break;
        }
        case 0x53: /* PUSH eBX */ {
            Push16(m_State.m_bx);
            break;
        }
        case 0x54: /* PUSH eSP */ {
            Push16(m_State.m_sp);
            break;
        }
        case 0x55: /* PUSH eBP */ {
            Push16(m_State.m_bp);
            break;
        }
        case 0x56: /* PUSH eSI */ {
            Push16(m_State.m_si);
            break;
        }
        case 0x57: /* PUSH eDI */ {
            Push16(m_State.m_di);
            break;
        }
        case 0x58: /* POP eAX */ {
            m_State.m_ax = Pop16();
            break;
        }
        case 0x59: /* POP eCX */ {
            m_State.m_cx = Pop16();
            break;
        }
        case 0x5a: /* POP eDX */ {
            m_State.m_dx = Pop16();
            break;
        }
        case 0x5b: /* POP eBX */ {
            m_State.m_bx = Pop16();
            break;
        }
        case 0x5c: /* POP eSP */ {
            m_State.m_sp = Pop16();
            break;
        }
        case 0x5d: /* POP eBP */ {
            m_State.m_bp = Pop16();
            break;
        }
        case 0x5e: /* POP eSI */ {
            m_State.m_si = Pop16();
            break;
        }
        case 0x5f: /* POP eDI */ {
            m_State.m_di = Pop16();
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
            Push16(imm);
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
            Push16(imm16);
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
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);
            const auto imm = getImm8();

            uint8_t val = ReadEA8(m_DecodeState);
            unsigned int op = ModRm_XXX(modrm);
            switch (op) {
                case 0: // add
                    WriteEA8(m_DecodeState, Add8(m_State.m_flags, val, imm));
                    break;
                case 1: // or
                    WriteEA8(m_DecodeState, Or8(m_State.m_flags, val, imm));
                    break;
                case 2: // adc
                    WriteEA8(m_DecodeState, Adc8(m_State.m_flags, val, imm));
                    break;
                case 3: // sbb
                    WriteEA8(m_DecodeState, Sbb8(m_State.m_flags, val, imm));
                    break;
                case 4: // and
                    WriteEA8(m_DecodeState, And8(m_State.m_flags, val, imm));
                    break;
                case 5: // sub
                    WriteEA8(m_DecodeState, Sub8(m_State.m_flags, val, imm));
                    break;
                case 6: // xor
                    WriteEA8(m_DecodeState, Xor8(m_State.m_flags, val, imm));
                    break;
                case 7: // cmp
                    [[maybe_unused]] auto _ = Sub8(m_State.m_flags, val, imm);
                    break;
            }
            break;
        }
        case 0x81: /* GRP1 Ev Iv */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);
            const auto imm = getImm16();

            uint16_t val = ReadEA16(m_DecodeState);
            unsigned int op = ModRm_XXX(modrm);
            switch (op) {
                case 0: // add
                    WriteEA16(m_DecodeState, Add16(m_State.m_flags, val, imm));
                    break;
                case 1: // or
                    WriteEA16(m_DecodeState, Or16(m_State.m_flags, val, imm));
                    break;
                case 2: // adc
                    WriteEA16(m_DecodeState, Adc16(m_State.m_flags, val, imm));
                    break;
                case 3: // sbb
                    WriteEA16(m_DecodeState, Sbb16(m_State.m_flags, val, imm));
                    break;
                case 4: // and
                    WriteEA16(m_DecodeState, And16(m_State.m_flags, val, imm));
                    break;
                case 5: // sub
                    WriteEA16(m_DecodeState, Sub16(m_State.m_flags, val, imm));
                    break;
                case 6: // xor
                    WriteEA16(m_DecodeState, Xor16(m_State.m_flags, val, imm));
                    break;
                case 7: // cmp
                    [[maybe_unused]] auto _ = Sub16(m_State.m_flags, val, imm);
                    break;
            }
            break;
        }
        case 0x83: /* GRP1 Ev Ib */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);
            const auto imm = getImm8();

            uint16_t val = ReadEA16(m_DecodeState);
            unsigned int op = ModRm_XXX(modrm);
            switch (op) {
                case 0: // add
                    WriteEA16(m_DecodeState, Add16(m_State.m_flags, val, imm));
                    break;
                case 1: // or
                    WriteEA16(m_DecodeState, Or16(m_State.m_flags, val, imm));
                    break;
                case 2: // adc
                    WriteEA16(m_DecodeState, Adc16(m_State.m_flags, val, imm));
                    break;
                case 3: // sbb
                    WriteEA16(m_DecodeState, Sbb16(m_State.m_flags, val, imm));
                    break;
                case 4: // and
                    WriteEA16(m_DecodeState, And16(m_State.m_flags, val, imm));
                    break;
                case 5: // sub
                    WriteEA16(m_DecodeState, Sub16(m_State.m_flags, val, imm));
                    break;
                case 6: // xor
                    WriteEA16(m_DecodeState, Xor16(m_State.m_flags, val, imm));
                    break;
                case 7: // cmp
                    [[maybe_unused]] auto _ = Sub16(m_State.m_flags, val, imm);
                    break;
            }
            break;
        }
        case 0x84: /* TEST Gb Eb */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);
            unsigned int shift;
            uint16_t& reg = GetReg8(ModRm_XXX(modrm), shift);
            [[maybe_unused]] auto _ = And8(m_State.m_flags, (reg >> shift) & 0xff, ReadEA8(m_DecodeState));
            break;
        }
        case 0x85: /* TEST Gv Ev */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);
            [[maybe_unused]] auto _ = And16(m_State.m_flags, GetReg16(ModRm_XXX(modrm)), ReadEA16(m_DecodeState));
            break;
        }
        case 0x86: /* XCHG Gb Eb */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);
            unsigned int shift;
            uint16_t& reg = GetReg8(ModRm_XXX(modrm), shift);
            uint8_t prev_reg = (reg >> shift) & 0xff;
            SetReg8(reg, shift, ReadEA8(m_DecodeState));
            WriteEA8(m_DecodeState, prev_reg);
            break;
        }
        case 0x87: /* XCHG Gv Ev */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);
            uint16_t& reg = GetReg16(ModRm_XXX(modrm));
            uint16_t prev_reg = reg;
            reg = ReadEA16(m_DecodeState);
            WriteEA16(m_DecodeState, prev_reg);
            break;
        }
        case 0x88: /* MOV Eb Gb */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);
            unsigned int shift;
            uint16_t& reg = GetReg8(ModRm_XXX(modrm), shift);
            WriteEA8(m_DecodeState, (reg >> shift) & 0xff);
            break;
        }
        case 0x89: /* MOV Ev Gv */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);
            WriteEA16(m_DecodeState, GetReg16(ModRm_XXX(modrm)));
            break;
        }
        case 0x8a: /* MOV Gb Eb */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);
            unsigned int shift;
            uint16_t& reg = GetReg8(ModRm_XXX(modrm), shift);
            SetReg8(reg, shift, ReadEA8(m_DecodeState));
            break;
        }
        case 0x8b: /* MOV Gv Ev */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);
            GetReg16(ModRm_XXX(modrm)) = ReadEA16(m_DecodeState);
            break;
        }
        case 0x8c: /* MOV Ew Sw */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);
            WriteEA16(m_DecodeState, GetSReg16(ModRm_XXX(modrm)));
            break;
        }
        case 0x8d: /* LEA Gv M */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);
            GetReg16(ModRm_XXX(modrm)) = GetAddrEA16(m_DecodeState);
            break;
        }
        case 0x8e: /* MOV Sw Ew */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);
            GetSReg16(ModRm_XXX(modrm)) = ReadEA16(m_DecodeState);
            break;
        }
        case 0x8f: /* POP Ev */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);
            WriteEA16(m_DecodeState, Pop16());
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
            uint16_t& reg = GetReg16(opcode - 0x90);
            uint16_t prev_ax = m_State.m_ax;
            m_State.m_ax = reg;
            reg = prev_ax;
            break;
        }
        case 0x98: /* CBW */ {
            if (m_State.m_ax & 0x80)
                m_State.m_ax = 0xff80 | m_State.m_ax & 0x7f;
            else
                m_State.m_ax = m_State.m_ax & 0x7f;
            break;
        }
        case 0x99: /* CWD */ {
            if (m_State.m_ax & 0x8000)
                m_State.m_dx = 0xffff;
            else
                m_State.m_dx = 0xffff;
            break;
        }
        case 0x9a: /* CALL Ap */ {
            const auto ip = getImm16();
            const auto cs = getImm16();
            Push16(m_State.m_cs);
            Push16(m_State.m_ip);
            m_State.m_cs = cs;
            m_State.m_ip = ip;
            break;
        }
        case 0x9b: /* WAIT */ {
            todo(); /* XXX Do we need this? */
            break;
        }
        case 0x9c: /* PUSHF */ {
            Push16(m_State.m_flags | cpu::flag::ON);
            break;
        }
        case 0x9d: /* POPF */ {
            m_State.m_flags = Pop16() | cpu::flag::ON;
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
                (m_State.m_ax & 0xff00) | m_Memory.ReadByte(MakeAddr(GetSReg16(seg), imm));
            break;
        }
        case 0xa1: /* MOV eAX Ov */ {
            const auto imm = getImm16();
            const auto seg = HandleSegmentOverride(m_State, SEG_DS);
            m_State.m_ax = m_Memory.ReadWord(MakeAddr(GetSReg16(seg), imm));
            break;
        }
        case 0xa2: /* MOV Ob AL */ {
            const auto imm = getImm16();
            const auto seg = HandleSegmentOverride(m_State, SEG_DS);
            m_Memory.WriteByte(MakeAddr(GetSReg16(seg), imm), m_State.m_ax & 0xff);
            break;
        }
        case 0xa3: /* MOV Ov eAX */ {
            const auto imm = getImm16();
            const auto seg = HandleSegmentOverride(m_State, SEG_DS);
            m_Memory.WriteWord(MakeAddr(GetSReg16(seg), imm), m_State.m_ax);
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
                        m_Memory.ReadByte(MakeAddr(GetSReg16(seg), m_State.m_si)));
                    m_State.m_si += delta;
                    m_State.m_di += delta;
                }
                m_State.m_prefix &= ~(cpu::State::PREFIX_REPZ | cpu::State::PREFIX_REPNZ);
            } else {
                m_Memory.WriteByte(
                    MakeAddr(m_State.m_es, m_State.m_di),
                    m_Memory.ReadByte(MakeAddr(GetSReg16(seg), m_State.m_si)));
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
                        m_Memory.ReadWord(MakeAddr(GetSReg16(seg), m_State.m_si)));
                    m_State.m_si += delta;
                    m_State.m_di += delta;
                }
                m_State.m_prefix &= ~(cpu::State::PREFIX_REPZ | cpu::State::PREFIX_REPNZ);
            } else {
                m_Memory.WriteWord(
                    MakeAddr(m_State.m_es, m_State.m_di),
                    m_Memory.ReadWord(MakeAddr(GetSReg16(seg), m_State.m_si)));
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
                    [[maybe_unused]] auto _ = Sub8(m_State.m_flags,
                        m_Memory.ReadByte(MakeAddr(GetSReg16(seg), m_State.m_si)),
                        m_Memory.ReadByte(MakeAddr(m_State.m_es, m_State.m_di)));
                    m_State.m_si += delta;
                    m_State.m_di += delta;
                    if (cpu::FlagZero(m_State.m_flags) == break_on_zf)
                        break;
                }
                m_State.m_prefix &= ~(cpu::State::PREFIX_REPZ | cpu::State::PREFIX_REPNZ);
            } else {
                [[maybe_unused]] auto _ = Sub8(m_State.m_flags,
                    m_Memory.ReadByte(MakeAddr(GetSReg16(seg), m_State.m_si)),
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
                    [[maybe_unused]] auto _ = Sub16(m_State.m_flags,
                        m_Memory.ReadWord(MakeAddr(GetSReg16(seg), m_State.m_si)),
                        m_Memory.ReadWord(MakeAddr(m_State.m_es, m_State.m_di)));
                    m_State.m_si += delta;
                    m_State.m_di += delta;
                    if (cpu::FlagZero(m_State.m_flags) == break_on_zf)
                        break;
                }
                m_State.m_prefix &= ~(cpu::State::PREFIX_REPZ | cpu::State::PREFIX_REPNZ);
            } else {
                [[maybe_unused]] auto _ = Sub16(m_State.m_flags,
                    m_Memory.ReadByte(MakeAddr(GetSReg16(seg), m_State.m_si)),
                    m_Memory.ReadByte(MakeAddr(m_State.m_es, m_State.m_di)));
                m_State.m_si += delta;
                m_State.m_di += delta;
            }
            break;
        }
        case 0xa8: /* TEST AL Ib */ {
            const auto imm = getImm8();
            [[maybe_unused]] auto _ = And8(m_State.m_flags, m_State.m_ax & 0xff, imm);
            break;
        }
        case 0xa9: /* TEST eAX Iv */ {
            const auto imm = getImm16();
            [[maybe_unused]] auto _ = And16(m_State.m_flags, m_State.m_ax, imm);
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
                (m_State.m_ax & 0xff00) | m_Memory.ReadByte(MakeAddr(GetSReg16(seg), m_State.m_si));
            if (cpu::FlagDirection(m_State.m_flags))
                m_State.m_si--;
            else
                m_State.m_si++;
            break;
        }
        case 0xad: /* LODSW */ {
            const auto seg = HandleSegmentOverride(m_State, SEG_DS);
            m_State.m_ax = m_Memory.ReadWord(MakeAddr(GetSReg16(seg), m_State.m_si));
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
                    [[maybe_unused]] auto _ = Sub8(m_State.m_flags, val, m_Memory.ReadByte(MakeAddr(m_State.m_es, m_State.m_di)));
                    m_State.m_di += delta;
                    if (cpu::FlagZero(m_State.m_flags) == break_on_zf)
                        break;
                }
                m_State.m_prefix &= ~(cpu::State::PREFIX_REPZ | cpu::State::PREFIX_REPNZ);
            } else {
                [[maybe_unused]] auto _ = Sub8(m_State.m_flags, val, m_Memory.ReadByte(MakeAddr(m_State.m_es, m_State.m_di)));
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
                    [[maybe_unused]] auto _ = Sub16(m_State.m_flags, m_State.m_ax, m_Memory.ReadWord(MakeAddr(m_State.m_es, m_State.m_di)));
                    m_State.m_di += delta;
                    if (cpu::FlagZero(m_State.m_flags) == break_on_zf)
                        break;
                }
                m_State.m_prefix &= ~(cpu::State::PREFIX_REPZ | cpu::State::PREFIX_REPNZ);
            } else {
                [[maybe_unused]] auto _ = Sub16(m_State.m_flags, m_State.m_ax, m_Memory.ReadWord(MakeAddr(m_State.m_es, m_State.m_di)));
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
            m_State.m_ip = Pop16();
            m_State.m_sp += imm;
            break;
        }
        case 0xc3: /* RET */ {
            m_State.m_ip = Pop16();
            break;
        }
        case 0xc4: /* LES Gv Mp */
        case 0xc5: /* LDS Gv Mp */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);
            uint16_t& reg = GetReg16(ModRm_XXX(modrm));

            uint16_t new_off = ReadEA16(m_DecodeState);
            m_DecodeState.m_off += 2;
            uint16_t new_seg = ReadEA16(m_DecodeState);

            if (opcode == 0xc4)
                m_State.m_es = new_seg;
            else
                m_State.m_ds = new_seg;
            reg = new_off;
            break;
        }
        case 0xc6: /* MOV Eb Ib */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);
            const auto imm = getImm8();
            WriteEA8(m_DecodeState, imm);
            break;
        }
        case 0xc7: /* MOV Ev Iv */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);
            const auto imm = getImm16();
            WriteEA16(m_DecodeState, imm);
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
            m_State.m_ip = Pop16();
            m_State.m_cs = Pop16();
            m_State.m_sp += imm;
            break;
        }
        case 0xcb: /* RETF */ {
            m_State.m_ip = Pop16();
            m_State.m_cs = Pop16();
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
            m_State.m_ip = Pop16();
            m_State.m_cs = Pop16();
            m_State.m_flags = Pop16();
            break;
        }
        case 0xd0: /* GRP2 Eb 1 */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);

            unsigned int op = ModRm_XXX(modrm);
            uint8_t val = ReadEA8(m_DecodeState);
            switch (op) {
                case 0: // rol
                    val = ROL<8>(m_State.m_flags, val, 1);
                    break;
                case 1: // ror
                    val = ROR<8>(m_State.m_flags, val, 1);
                    break;
                case 2: // rcl
                    val = RCL<8>(m_State.m_flags, val, 1);
                    break;
                case 3: // rcr
                    val = RCR<8>(m_State.m_flags, val, 1);
                    break;
                case 4: // shl
                    val = SHL<8>(m_State.m_flags, val, 1);
                    break;
                case 5: // shr
                    val = SHR<8>(m_State.m_flags, val, 1);
                    break;
                case 6: // undefined
                    invalidOpcode();
                    break;
                case 7: // sar
                    val = SAR<8>(m_State.m_flags, val, 1);
                    break;
            }
            WriteEA8(m_DecodeState, val);
            break;
        }
        case 0xd1: /* GRP2 Ev 1 */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);

            unsigned int op = ModRm_XXX(modrm);
            uint16_t val = ReadEA16(m_DecodeState);
            switch (op) {
                case 0: // rol
                    val = ROL<16>(m_State.m_flags, val, 1);
                    break;
                case 1: // ror
                    val = ROR<16>(m_State.m_flags, val, 1);
                    break;
                case 2: // rcl
                    val = RCL<16>(m_State.m_flags, val, 1);
                    break;
                case 3: // rcr
                    val = RCR<16>(m_State.m_flags, val, 1);
                    break;
                case 4: // shl
                    val = SHL<16>(m_State.m_flags, val, 1);
                    break;
                case 5: // shr
                    val = SHR<16>(m_State.m_flags, val, 1);
                    break;
                case 6: // undefined
                    invalidOpcode();
                    break;
                case 7: // sar
                    val = SAR<16>(m_State.m_flags, val, 1);
                    break;
            }
            WriteEA16(m_DecodeState, val);
            break;
        }
        case 0xd2: /* GRP2 Eb CL */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);

            unsigned int op = ModRm_XXX(modrm);
            uint8_t val = ReadEA8(m_DecodeState);
            uint8_t cnt = m_State.m_cx & 0xff;
            switch (op) {
                case 0: // rol
                    val = ROL<8>(m_State.m_flags, val, cnt);
                    break;
                case 1: // ror
                    val = ROR<8>(m_State.m_flags, val, cnt);
                    break;
                case 2: // rcl
                    val = RCL<8>(m_State.m_flags, val, cnt);
                    break;
                case 3: // rcr
                    val = RCR<8>(m_State.m_flags, val, cnt);
                    break;
                case 4: // shl
                    val = SHL<8>(m_State.m_flags, val, cnt);
                    break;
                case 5: // shr
                    val = SHR<8>(m_State.m_flags, val, cnt);
                    break;
                case 6: // undefined
                    invalidOpcode();
                    break;
                case 7: // sar
                    val = SAR<8>(m_State.m_flags, val, cnt);
                    break;
            }
            WriteEA8(m_DecodeState, val);
            break;
        }
        case 0xd3: /* GRP2 Ev CL */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);

            unsigned int op = ModRm_XXX(modrm);
            uint16_t val = ReadEA16(m_DecodeState);
            uint8_t cnt = m_State.m_cx & 0xff;
            switch (op) {
                case 0: // rol
                    val = ROL<16>(m_State.m_flags, val, cnt);
                    break;
                case 1: // ror
                    val = ROR<16>(m_State.m_flags, val, cnt);
                    break;
                case 2: // rcl
                    val = RCL<16>(m_State.m_flags, val, cnt);
                    break;
                case 3: // rcr
                    val = RCR<16>(m_State.m_flags, val, cnt);
                    break;
                case 4: // shl
                    val = SHL<16>(m_State.m_flags, val, cnt);
                    break;
                case 5: // shr
                    val = SHR<16>(m_State.m_flags, val, cnt);
                    break;
                case 6: // undefined
                    invalidOpcode();
                    break;
                case 7: // sar
                    val = SAR<16>(m_State.m_flags, val, cnt);
                    break;
            }
            WriteEA16(m_DecodeState, val);
            break;
        }
        case 0xd4: /* AAM I0 */ {
            todo();
            break;
        }
        case 0xd5: /* AAD I0 */ {
            todo();
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
        case 0xd8: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0xd9: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0xda: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0xdb: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0xdc: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0xdd: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0xde: /* -- */ {
            invalidOpcode();
            break;
        }
        case 0xdf: /* -- */ {
            invalidOpcode();
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
            Push16(m_State.m_ip);
            RelativeJump16(m_State.m_ip, imm);
            break;
        }
        case 0xe9: /* JMP Jv */ {
            const auto imm = getImm16();
            RelativeJump16(m_State.m_ip, imm);
            break;
        }
        case 0xea: /* JMP Ap */ {
            m_State.m_ip = getImm16();
            m_State.m_cs = getImm16();
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
        case 0xf2: /* REPNZ */ {
            m_State.m_prefix |= cpu::State::PREFIX_REPNZ;
            break;
        }
        case 0xf3: /* REPZ */ {
            m_State.m_prefix |= cpu::State::PREFIX_REPZ;
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
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);

            unsigned int op = ModRm_XXX(modrm);
            switch (op) {
                case 0: /* TEST Eb Ib */ {
                    const auto imm = getImm8();
                    [[maybe_unused]] auto _ = And8(m_State.m_flags, ReadEA8(m_DecodeState), imm);
                    break;
                }
                case 1: /* invalid */
                    invalidOpcode();
                    break;
                case 2: /* NOT */
                    WriteEA8(m_DecodeState, 0xFF - ReadEA8(m_DecodeState));
                    break;
                case 3: /* NEG */
                    WriteEA8(m_DecodeState, Sub8(m_State.m_flags, 0, ReadEA8(m_DecodeState)));
                    break;
                case 4: /* MUL */
                    Mul8(m_State.m_flags, m_State.m_ax, ReadEA8(m_DecodeState));
                    break;
                case 5: /* IMUL */
                    Imul8(m_State.m_flags, m_State.m_ax, ReadEA8(m_DecodeState));
                    break;
                case 6: /* DIV */
                    if (Div8(m_State.m_ax, ReadEA8(m_DecodeState)))
                        SignalInterrupt(INT_DIV_BY_ZERO);
                    break;
                case 7: /* IDIV */
                    if (Idiv8(m_State.m_ax, m_State.m_dx, ReadEA8(m_DecodeState)))
                        SignalInterrupt(INT_DIV_BY_ZERO);
                    break;
            }
            break;
        }
        case 0xf7: /* GRP3b Ev */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);

            unsigned int op = ModRm_XXX(modrm);
            switch (op) {
                case 0: /* TEST Eb Iw */ {
                    const auto imm = getImm16();
                    [[maybe_unused]] auto _ = And16(m_State.m_flags, ReadEA16(m_DecodeState), imm);
                    break;
                }
                case 1: /* invalid */
                    invalidOpcode();
                    break;
                case 2: /* NOT */
                    WriteEA16(m_DecodeState, 0xFFFF - ReadEA16(m_DecodeState));
                    break;
                case 3: /* NEG */
                    WriteEA16(m_DecodeState, Sub16(m_State.m_flags, 0, ReadEA16(m_DecodeState)));
                    break;
                case 4: /* MUL */
                    Mul16(m_State.m_flags, m_State.m_ax, m_State.m_dx, ReadEA16(m_DecodeState));
                    break;
                case 5: /* IMUL */
                    Imul16(m_State.m_flags, m_State.m_ax, m_State.m_dx, ReadEA16(m_DecodeState));
                    break;
                case 6: /* DIV */
                    if (Div16(m_State.m_ax, m_State.m_dx, ReadEA16(m_DecodeState)))
                        SignalInterrupt(INT_DIV_BY_ZERO);
                    break;
                case 7: /* IDIV */
                    if (Idiv16(m_State.m_ax, m_State.m_dx, ReadEA16(m_DecodeState)))
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
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);

            uint8_t val = ReadEA8(m_DecodeState);
            unsigned int op = ModRm_XXX(modrm);
            switch (op) {
                case 0: // inc
                    WriteEA8(m_DecodeState, Inc8(m_State.m_flags, val));
                    break;
                case 1: // dec
                    WriteEA8(m_DecodeState, Dec8(m_State.m_flags, val));
                    break;
                default: // invalid
                    invalidOpcode();
                    break;
            }
            break;
        }
        case 0xff: /* GRP5 Ev */ {
            const auto modrm = getModRm();
            DecodeEA(modrm, m_DecodeState);

            uint16_t val = ReadEA16(m_DecodeState);
            unsigned int op = ModRm_XXX(modrm);
            switch (op) {
                case 0: /* INC eV */
                    WriteEA16(m_DecodeState, Inc16(m_State.m_flags, val));
                    break;
                case 1: /* DEC eV */
                    WriteEA16(m_DecodeState, Dec16(m_State.m_flags, val));
                    break;
                case 2: /* CALL Ev */
                    Push16(m_State.m_ip);
                    m_State.m_ip = val;
                    break;
                case 3: /* CALL Ep */ {
                    Push16(m_State.m_cs);
                    Push16(m_State.m_ip);
                    m_State.m_ip = val;
                    m_DecodeState.m_off += 2;
                    m_State.m_cs = ReadEA16(m_DecodeState);
                    break;
                }
                case 4: /* JMP Ev */
                    m_State.m_ip = val;
                    break;
                case 5: /* JMP Ep */ {
                    m_State.m_ip = val;
                    m_DecodeState.m_off += 2;
                    m_State.m_cs = ReadEA16(m_DecodeState);
                    break;
                }
                case 6: /* PUSH Ev */
                    Push16(val);
                    break;
                case 7: /* undefined */
                    invalidOpcode();
                    break;
            }
            break;
        }
    }
}

void CPUx86::Handle0FPrefix()
{
    auto getImm8 = [&]() { return GetNextOpcode(); };
    auto invalidOpcode = []() { TRACE("invalidOpcode 0F..\n"); std::abort(); };

    uint8_t opcode = GetNextOpcode();
    TRACE("cs:ip=%04x:%04x opcode 0x0f 0x%02x\n", m_State.m_cs, m_State.m_ip - 1, opcode);
    switch (opcode) {
        case 0x34: /* SYSENTER - (ab)used for interrupt dispatch */ {
            const auto imm = getImm8();
            m_Vectors.Invoke(*this, imm);
            break;
        }
        default: /* undefined */
            invalidOpcode();
            break;
    }
}

uint8_t CPUx86::GetNextOpcode()
{
    return m_Memory.ReadByte(MakeAddr(m_State.m_cs, m_State.m_ip++));
}

CPUx86::addr_t CPUx86::MakeAddr(uint16_t seg, uint16_t off)
{
    return ((addr_t)seg << 4) + (addr_t)off;
}

void CPUx86::DecodeEA(uint8_t modrm, DecodeState& oState)
{
    auto getImm8 = [&]() { return GetNextOpcode(); };
    auto getImm16 = [&]() -> uint16_t {
        uint16_t a = GetNextOpcode();
        uint16_t b = GetNextOpcode();
        return a | (b << 8);
    };

    const uint8_t mod = (modrm & 0xc0) >> 6;
    const uint8_t rm = modrm & 7;
    oState.m_reg = ModRm_XXX(modrm);

    switch (mod) {
        case 0: /* DISP=0*, disp-low and disp-hi are absent */ {
            if (rm == 6) {
                // except if mod==00 and rm==110, then EA = disp-hi; disp-lo
                const auto imm = getImm16();
                oState.m_type = DecodeState::T_MEM;
                oState.m_seg = HandleSegmentOverride(m_State, SEG_DS);
                oState.m_off = imm;
                oState.m_disp = 0;
                return;
            } else {
                oState.m_disp = 0;
            }
            break;
        }
        case 1: /* DISP=disp-low sign-extended to 16-bits, disp-hi absent */ {
            const auto imm = getImm8();
            oState.m_disp = (addr_t)((int16_t)imm);
            break;
        }
        case 2: /* DISP=disp-hi:disp-lo */ {
            const auto imm = getImm16();
            oState.m_disp = imm;
            break;
        }
        case 3: /* rm treated as reg field */ {
            oState.m_disp = 0;
            break;
        }
    }

    if (mod != 3) {
        oState.m_type = DecodeState::T_MEM;
        switch (rm) {
            case 0: // (bx) + (si) + disp
                oState.m_seg = HandleSegmentOverride(m_State, SEG_DS);
                oState.m_off = m_State.m_bx + m_State.m_si;
                break;
            case 1: // (bx) + (di) + disp
                oState.m_seg = HandleSegmentOverride(m_State, SEG_DS);
                oState.m_off = m_State.m_bx + m_State.m_di;
                break;
            case 2: // (bp) + (si) + disp
                oState.m_seg = HandleSegmentOverride(m_State, SEG_SS);
                oState.m_off = m_State.m_bp + m_State.m_si;
                break;
            case 3: // (bp) + (di) + disp
                oState.m_seg = HandleSegmentOverride(m_State, SEG_SS);
                oState.m_off = m_State.m_bp + m_State.m_di;
                break;
            case 4: // (si) + disp
                oState.m_seg = HandleSegmentOverride(m_State, SEG_DS);
                oState.m_off = m_State.m_si;
                break;
            case 5: // (di) + disp
                oState.m_seg = HandleSegmentOverride(m_State, SEG_DS);
                oState.m_off = m_State.m_di;
                break;
            case 6: // (bp) + disp
                oState.m_seg = HandleSegmentOverride(m_State, SEG_SS);
                oState.m_off = m_State.m_bp;
                break;
            case 7: // (bx) + disp
                oState.m_seg = HandleSegmentOverride(m_State, SEG_DS);
                oState.m_off = m_State.m_bx;
                break;
        }
    } else /* mod == 3, r/m treated as reg field */ {
        oState.m_type = DecodeState::T_REG;
        oState.m_reg = rm;
    }
}

uint16_t CPUx86::GetAddrEA16(const DecodeState& oState)
{
    if (oState.m_type == DecodeState::T_REG)
        return GetReg16(oState.m_reg);

    return oState.m_off + oState.m_disp;
}

uint16_t CPUx86::ReadEA16(const DecodeState& oState)
{
    if (oState.m_type == DecodeState::T_REG) {
        return GetReg16(oState.m_reg);
    }

    uint16_t seg_base = 0;
    switch (oState.m_seg) {
        case SEG_ES:
            seg_base = m_State.m_es;
            break;
        case SEG_CS:
            seg_base = m_State.m_cs;
            break;
        case SEG_DS:
            seg_base = m_State.m_ds;
            break;
        case SEG_SS:
            seg_base = m_State.m_ss;
            break;
        default:
            std::abort();
    }

    TRACE(
        "read(16) @ %x:%x -> %x\n", seg_base, oState.m_off + oState.m_disp,
        m_Memory.ReadWord(MakeAddr(seg_base, oState.m_off + oState.m_disp)));
    return m_Memory.ReadWord(MakeAddr(seg_base, oState.m_off + oState.m_disp));
}

void CPUx86::WriteEA16(const DecodeState& oState, uint16_t value)
{
    if (oState.m_type == DecodeState::T_REG) {
        GetReg16(oState.m_reg) = value;
        return;
    }

    uint16_t seg_base = 0;
    switch (oState.m_seg) {
        case SEG_ES:
            seg_base = m_State.m_es;
            break;
        case SEG_CS:
            seg_base = m_State.m_cs;
            break;
        case SEG_DS:
            seg_base = m_State.m_ds;
            break;
        case SEG_SS:
            seg_base = m_State.m_ss;
            break;
        default:
            std::abort();
    }

    TRACE("write(16) @ %x:%x val %x\n", seg_base, oState.m_off + oState.m_disp, value);
    m_Memory.WriteWord(MakeAddr(seg_base, oState.m_off + oState.m_disp), value);
}

uint8_t CPUx86::ReadEA8(const DecodeState& oState)
{
    if (oState.m_type == DecodeState::T_REG) {
        unsigned int shift;
        uint16_t& v = GetReg8(oState.m_reg, shift);
        return (v >> shift) & 0xff;
    }

    uint16_t seg_base = 0;
    switch (oState.m_seg) {
        case SEG_ES:
            seg_base = m_State.m_es;
            break;
        case SEG_CS:
            seg_base = m_State.m_cs;
            break;
        case SEG_DS:
            seg_base = m_State.m_ds;
            break;
        case SEG_SS:
            seg_base = m_State.m_ss;
            break;
        default:
            std::abort();
    }

    TRACE("read(8) @ %x:%x\n", seg_base, oState.m_off + oState.m_disp);
    return m_Memory.ReadByte(MakeAddr(seg_base, oState.m_off + oState.m_disp));
}

void CPUx86::WriteEA8(const DecodeState& oState, uint8_t val)
{
    if (oState.m_type == DecodeState::T_REG) {
        unsigned int shift;
        uint16_t& v = GetReg8(oState.m_reg, shift);
        SetReg8(v, shift, val);
        return;
    }

    uint16_t seg_base = 0;
    switch (oState.m_seg) {
        case SEG_ES:
            seg_base = m_State.m_es;
            break;
        case SEG_CS:
            seg_base = m_State.m_cs;
            break;
        case SEG_DS:
            seg_base = m_State.m_ds;
            break;
        case SEG_SS:
            seg_base = m_State.m_ss;
            break;
        default:
            std::abort();
    }

    TRACE("write(8) @ %x:%x val %\n", seg_base, oState.m_off + oState.m_disp, val);
    m_Memory.WriteByte(MakeAddr(seg_base, oState.m_off + oState.m_disp), val);
}

uint16_t& CPUx86::GetReg16(int n)
{
    switch (n) {
        case 0:
            return m_State.m_ax;
        case 1:
            return m_State.m_cx;
        case 2:
            return m_State.m_dx;
        case 3:
            return m_State.m_bx;
        case 4:
            return m_State.m_sp;
        case 5:
            return m_State.m_bp;
        case 6:
            return m_State.m_si;
        case 7:
            return m_State.m_di;
    }
    std::abort();
}

uint16_t& CPUx86::GetSReg16(int n)
{
    switch (n) {
        case SEG_ES:
            return m_State.m_es;
        case SEG_CS:
            return m_State.m_cs;
        case SEG_SS:
            return m_State.m_ss;
        case SEG_DS:
            return m_State.m_ds;
    }
    std::abort();
}

uint16_t& CPUx86::GetReg8(int n, unsigned int& shift)
{
    shift = (n > 3) ? 8 : 0;
    switch (n & 3) {
        case 0:
            return m_State.m_ax;
        case 1:
            return m_State.m_cx;
        case 2:
            return m_State.m_dx;
        case 3:
            return m_State.m_bx;
    }
    std::abort();
}

void CPUx86::SetReg8(uint16_t& reg, unsigned int shift, uint8_t val)
{
    if (shift > 0) {
        reg = (reg & 0x00ff) | (val << 8);
    } else {
        reg = (reg & 0xff00) | val;
    }
}

void CPUx86::Push16(uint16_t value)
{
    m_Memory.WriteWord(MakeAddr(m_State.m_ss, m_State.m_sp - 2), value);
    m_State.m_sp -= 2;
}

uint16_t CPUx86::Pop16()
{
    uint16_t value = m_Memory.ReadWord(MakeAddr(m_State.m_ss, m_State.m_sp));
    m_State.m_sp += 2;
    return value;
}

namespace cpu
{

void Dump(const State& st)
{
    fprintf(
        stderr, "  ax=%04x bx=%04x cx=%04x dx=%04x si=%04x di=%04x bp=%04x flags=%04x\n", st.m_ax,
        st.m_bx, st.m_cx, st.m_dx, st.m_si, st.m_di, st.m_bp, st.m_flags);
    fprintf(
        stderr, "  cs:ip=%04x:%04x ds=%04x es=%04x ss:sp=%04x:%04x\n", st.m_cs, st.m_ip, st.m_ds, st.m_es, st.m_ss,
        st.m_sp);
}

}

void CPUx86::Dump() { cpu::Dump(m_State); }

void CPUx86::SignalInterrupt(uint8_t no)
{
    TRACE("SignalInterrupt 0x%x\n", no);
    std::abort(); // TODO
}

void CPUx86::HandleInterrupt(uint8_t no)
{
    addr_t addr = MakeAddr(0, no * 4);
    uint16_t off = m_Memory.ReadWord(addr + 0);

    // Push flags and return address
    Push16(m_State.m_flags);
    Push16(m_State.m_cs);
    Push16(m_State.m_ip);

    // Transfer control to interrupt
    m_State.m_cs = m_Memory.ReadWord(addr + 2);
    m_State.m_ip = m_Memory.ReadWord(addr + 0);

    TRACE("HandleInterrupt(): no=%x -> %04x:%04x\n", no, m_State.m_cs, m_State.m_ip);

    // XXX Kludge
    if (m_State.m_cs == 0 && m_State.m_ip == 0)
        abort();

#if 0
	/* XXX KLUDGE */
	if (no == 0x10 && m_State.m_ax >> 8 == 0xf) {
		/* Video: get video mode; we return 80x25x16 */
		m_State.m_ax = 3;
		return;
	}

	/* XXX BEUN */
	if (no == 0x20) {
		m_State.m_ip -= 2;
		return;
	}

	if (no == 0x16) {
		uint16_t ah = (m_State.m_ax & 0xff00) >> 8;
		switch(ah) {
			case 1: // check for key
				cpu::SetFlag<cpu::flag::ZF>(m_State.m_flags, true);
				break;
			default:
				TRACE("HandleInterrupt %d func %d\n", no, ah);
		}
		return;
	}
#endif
}

/* vim:set ts=4 sw=4: */
