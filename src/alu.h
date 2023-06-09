#pragma once

#include "state.h"

namespace cpu {
namespace alu {
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
              cpu::flag::AF | cpu::flag::PF | cpu::flag::CF);
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
        if ((!sign_a && !sign_b &&  sign_res) ||
            ( sign_a &&  sign_b && !sign_res) ||
            (!sign_a &&  sign_b &&  sign_res) ||
            ( sign_a && !sign_b && !sign_res))
            flags |= cpu::flag::OF;

#if 0
        if (sign_a == sign_b && sign_res != sign_a)
            flags |= cpu::flag::OF;
#endif
        if ((a ^ b ^ res) & 0x10)
            flags |= cpu::flag::AF;
    }

    template<>
    constexpr void SetFlagsArith<16>(uint16_t& flags, uint16_t a, uint16_t b, uint32_t res)
    {
        flags &=
            ~(cpu::flag::OF | cpu::flag::SF | cpu::flag::ZF |
              cpu::flag::AF | cpu::flag::PF | cpu::flag::CF);
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
#if 0
        return Add8(flags, a, -b);
#else
        uint16_t res = a - b;
        SetFlagsArith<8>(flags, a, b, res);
        return res & 0xff;
#endif
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
}
}
