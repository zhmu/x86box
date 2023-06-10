#pragma once

#include <numeric> // popcount()
#include "state.h"

namespace cpu {
namespace alu {
    template<unsigned int BITS>
    struct UintOfImpl;
    template<>
    struct UintOfImpl<8> {
        using type = uint8_t;
        static const type MsbMask = 0x80;
        static const type Mask = 0xff;
        static const auto CarryMask = 0xffff'00;
    };
    template<>
    struct UintOfImpl<16> {
        using type = uint16_t;
        static const type MsbMask = 0x8000;
        static const type Mask = 0xffff;
        static const auto CarryMask = 0xffff'0000;
    };
    template<>
    struct UintOfImpl<32> {
        using type = uint32_t;
        static const type MsbMask = 0x8000'0000;
        static const type Mask = 0xffff'ffff;
        static const auto CarryMask = 0xffff'0000'0000;
    };

    template<unsigned int BITS>
    using UintOf = UintOfImpl<BITS>::type;
    template<unsigned int BITS>
    constexpr auto MsbMaskOf() { return UintOfImpl<BITS>::MsbMask; }
    template<unsigned int BITS>
    constexpr auto LsbMaskOf() { return UintOf<BITS>{1}; }
    template<unsigned int BITS>
    constexpr auto MaskOf() { return UintOfImpl<BITS>::Mask; }
    template<unsigned int BITS>
    constexpr auto CarryMaskOf() { return UintOfImpl<BITS>::CarryMask; }

    static constexpr inline auto MaximumShiftCount = 0x1f;

    template<unsigned int BITS>
    constexpr void SetFlagZ(cpu::Flags& flags, const UintOf<BITS> value)
    {
        cpu::SetFlag<cpu::flag::ZF>(flags, value == 0);
    }

    template<unsigned int BITS>
    constexpr void SetFlagS(cpu::Flags& flags, const UintOf<BITS> value)
    {
        cpu::SetFlag<cpu::flag::SF>(flags, (value & MsbMaskOf<BITS>()) != 0);
    }

    template<unsigned int BITS>
    constexpr void SetFlagP(cpu::Flags& flags, const UintOf<BITS> value)
    {
        const auto popcnt = std::popcount(static_cast<uint8_t>(value & 0xff));
        cpu::SetFlag<cpu::flag::PF>(flags, (popcnt & 1) == 0);
    }

    template<unsigned int BITS>
    constexpr void SetFlagsSZP(cpu::Flags& flags, const UintOf<BITS> v)
    {
        SetFlagS<BITS>(flags, v);
        SetFlagZ<BITS>(flags, v);
        SetFlagP<BITS>(flags, v);
    }

    template<unsigned int BITS>
    constexpr bool MustSetOvForAdd(UintOf<BITS> a, UintOf<BITS> b, UintOf<BITS> res)
    {
        constexpr auto msbMask = MsbMaskOf<BITS>();
        const auto sign_a = (a & msbMask) != 0;
        const auto sign_b = (b & msbMask) != 0;
        const auto sign_r = (res & msbMask) != 0;
        return
            (!sign_a && !sign_b &&  sign_r) ||
            ( sign_a &&  sign_b && !sign_r);
    }

    template<unsigned int BITS>
    constexpr bool MustSetOvForSub(UintOf<BITS> a, UintOf<BITS> b, UintOf<BITS> res)
    {
        constexpr auto msbMask = MsbMaskOf<BITS>();
        const auto sign_a = (a & msbMask) != 0;
        const auto sign_b = (b & msbMask) != 0;
        const auto sign_r = (res & msbMask) != 0;
        return
            (!sign_a &&  sign_b &&  sign_r) ||
            ( sign_a && !sign_b && !sign_r);
    }

    template<unsigned int BITS>
    constexpr void SetFlagsForAdd(cpu::Flags& flags, const UintOf<BITS> a, const UintOf<BITS> b, const UintOf<BITS> c, const UintOf<BITS> res)
    {
        SetFlagsSZP<BITS>(flags, res);
        cpu::SetFlag<cpu::flag::OF>(flags, MustSetOvForAdd<BITS>(a, b, res));
        cpu::SetFlag<cpu::flag::AF>(flags, (b & 0xf) + (a & 0xf) + c >= 0x10);
    }

    template<unsigned int BITS>
    constexpr void SetFlagsForSub(cpu::Flags& flags, const UintOf<BITS> a, const UintOf<BITS> b, const UintOf<BITS> c, const UintOf<BITS> res)
    {
        SetFlagsSZP<BITS>(flags, res);
        cpu::SetFlag<cpu::flag::OF>(flags, MustSetOvForSub<BITS>(a, b, res));
        cpu::SetFlag<cpu::flag::AF>(flags, (b & 0xf) + c > (a & 0xf));
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr auto ROL(cpu::Flags& flags, UintOf<BITS> v, uint8_t n)
    {
        constexpr auto lsbMask = LsbMaskOf<BITS>();
        constexpr auto msbMask = MsbMaskOf<BITS>();
        const auto cnt = n & MaximumShiftCount;

        auto res = v;
        for(int n = 0; n < cnt; ++n) {
            auto temp_cf = (res & msbMask) ? lsbMask : 0;
            res = ((res << 1) + temp_cf) & MaskOf<BITS>();
        }

        if (cnt > 0) {
            cpu::SetFlag<cpu::flag::CF>(flags, res & lsbMask);
            // OF is undefined if the count != 1, but it is set anyway
            const auto cf = cpu::FlagCarry(flags) ? msbMask : 0;
            cpu::SetFlag<cpu::flag::OF>(flags, (res & msbMask) ^ cf);
        }
        return res;
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr auto ROR(cpu::Flags& flags, UintOf<BITS> v, uint8_t n)
    {
        constexpr auto lsbMask = LsbMaskOf<BITS>();
        constexpr auto msbMask = MsbMaskOf<BITS>();
        const auto cnt = n & MaximumShiftCount;

        auto res = v;
        for(int n = 0; n < cnt % 8; ++n) {
            auto temp_cf = (res & lsbMask) ? msbMask : 0;
            res = ((res >> 1) + temp_cf) & MaskOf<BITS>();
        }

        if (cnt > 0) {
            cpu::SetFlag<cpu::flag::CF>(flags, res & msbMask);
            // OF is undefined if the count != 1, but it is set anyway
            const auto msb_0 = (res & msbMask) ? 1 : 0;
            const auto msb_1 = (res & (msbMask >> 1)) ? 1 : 0;
            cpu::SetFlag<cpu::flag::OF>(flags, msb_0 ^ msb_1);
        }
        return res;
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr auto RCL(cpu::Flags& flags, UintOf<BITS> v, uint8_t n)
    {
        constexpr auto lsbMask = LsbMaskOf<BITS>();
        constexpr auto msbMask = MsbMaskOf<BITS>();
        const auto cnt = n & MaximumShiftCount;

        auto cf = cpu::FlagCarry(flags) ? lsbMask : 0;
        auto res = v;
        for(int n = 0; n < cnt; ++n) {
            auto temp_cf = (res & msbMask) ? lsbMask : 0;
            res = ((res << 1) + cf) & MaskOf<BITS>();
            cf = temp_cf;
        }

        if (cnt > 0) {
            // OF is undefined if the count != 1, but it is set anyway
            cpu::SetFlag<cpu::flag::OF>(flags, (v & msbMask) ^ (res & msbMask));
        }
        cpu::SetFlag<cpu::flag::CF>(flags, cf);
        return res;
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr auto RCR(cpu::Flags& flags, UintOf<BITS> v, uint8_t n)
    {
        constexpr auto lsbMask = LsbMaskOf<BITS>();
        constexpr auto msbMask = MsbMaskOf<BITS>();
        const auto cnt = n & MaximumShiftCount;

        auto cf = cpu::FlagCarry(flags) ? lsbMask : 0;
        auto res = v;
        for(int n = 0; n < cnt % 9; ++n) {
            auto temp_cf = (res & lsbMask) ? lsbMask : 0;
            res = (res >> 1) + (cf ? msbMask : 0);
            cf = temp_cf;
        }

        if (cnt > 0) {
            // OF is undefined if the count != 1, but it is set anyway
            cpu::SetFlag<cpu::flag::OF>(flags, (v & msbMask) ^ (res & msbMask));
        }
        cpu::SetFlag<cpu::flag::CF>(flags, cf);
        return res;
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr auto SHL(cpu::Flags& flags, UintOf<BITS> v, uint8_t n)
    {
        const auto cnt = n & MaximumShiftCount;
        if (cnt == 0) return v;

        cpu::SetFlag<cpu::flag::CF>(flags, false);

        constexpr auto msbMask = MsbMaskOf<BITS>();
        auto res = v;
        for (int n = 0; n < cnt; ++n) {
            cpu::SetFlag<cpu::flag::CF>(flags, (res & msbMask) != 0);
            res = (res << 1) & MaskOf<BITS>();
        }

        // Formally, OF is undefined if count > 1 - but it seems set regardless
        const auto cfMask = cpu::FlagCarry(flags) ? msbMask : 0;
        cpu::SetFlag<cpu::flag::OF>(flags, (res & msbMask) ^ cfMask);
        SetFlagsSZP<BITS>(flags, res);
        return res;
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr auto SHR(cpu::Flags& flags, UintOf<BITS> v, uint8_t n)
    {
        const auto cnt = n & MaximumShiftCount;
        if (cnt == 0) return v;

        cpu::SetFlag<cpu::flag::CF>(flags, false);

        constexpr auto lsbMask = LsbMaskOf<BITS>();
        constexpr auto msbMask = MsbMaskOf<BITS>();
        auto res = v;
        for (int n = 0; n < cnt; ++n) {
            cpu::SetFlag<cpu::flag::CF>(flags, (res & lsbMask) != 0);
            res = (res >> 1) & MaskOf<BITS>();
        }

        // Formally, OF is undefined if count > 1 - it does not seem to be set
        if (cnt == 1) {
            cpu::SetFlag<cpu::flag::OF>(flags, v & msbMask);
        }
        SetFlagsSZP<BITS>(flags, res);
        return res;
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr auto SAR(cpu::Flags& flags, UintOf<BITS> v, uint8_t n)
    {
        const auto cnt = n & MaximumShiftCount;
        if (cnt == 0) return v;

        cpu::SetFlag<cpu::flag::CF>(flags, false);

        constexpr auto lsbMask = LsbMaskOf<BITS>();
        constexpr auto msbMask = MsbMaskOf<BITS>();
        auto res = v;
        for (int n = 0; n < cnt; ++n) {
            cpu::SetFlag<cpu::flag::CF>(flags, (res & lsbMask) != 0);
            const auto expand = res & msbMask;
            res = expand | (res >> 1);
        }

        // Shifts of 1 always clear OF - otherwise OF is undefined but it always
        // seems to be cleared...
        SetFlagsSZP<BITS>(flags, res);
        return res;
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr UintOf<BITS> ADD(cpu::Flags& flags, UintOf<BITS> a, UintOf<BITS> b)
    {
        UintOf<BITS * 2> res = a + b;
        cpu::SetFlag<cpu::flag::CF>(flags, res & CarryMaskOf<BITS>());
        SetFlagsForAdd<BITS>(flags, a, b, 0, res);
        return res & MaskOf<BITS>();
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr UintOf<BITS> OR(cpu::Flags& flags, UintOf<BITS> a, UintOf<BITS> b)
    {
        UintOf<BITS> op1 = a | b;
        flags &=
            ~(cpu::flag::OF | cpu::flag::SF | cpu::flag::ZF | cpu::flag::PF | cpu::flag::CF);
        SetFlagsSZP<8>(flags, op1);
        return op1;
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr UintOf<BITS> AND(cpu::Flags& flags, UintOf<BITS> a, UintOf<BITS> b)
    {
        UintOf<BITS> res = a & b;
        flags &=
            ~(cpu::flag::OF | cpu::flag::SF | cpu::flag::ZF | cpu::flag::PF | cpu::flag::CF);
        SetFlagsSZP<BITS>(flags, res);
        return res;
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr UintOf<BITS> XOR(cpu::Flags& flags, UintOf<BITS> a, UintOf<BITS> b)
    {
        UintOf<BITS> res = a ^ b;
        flags &=
            ~(cpu::flag::OF | cpu::flag::SF | cpu::flag::ZF | cpu::flag::PF | cpu::flag::CF);
        SetFlagsSZP<BITS>(flags, res);
        return res;
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr UintOf<BITS> ADC(cpu::Flags& flags, UintOf<BITS> a, UintOf<BITS> b)
    {
        const UintOf<BITS> c = cpu::FlagCarry(flags) ? 1 : 0;
        const UintOf<2 * BITS> res = a + b + c;
        cpu::SetFlag<cpu::flag::CF>(flags, res & CarryMaskOf<BITS>());
        SetFlagsForAdd<BITS>(flags, a, b, c, res);
        return res & MaskOf<BITS>();
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr UintOf<BITS> SUB(cpu::Flags& flags, UintOf<BITS> a, UintOf<BITS> b)
    {
        UintOf<BITS * 2> res = a - b;
        cpu::SetFlag<cpu::flag::CF>(flags, res & CarryMaskOf<BITS>());
        SetFlagsForSub<BITS>(flags, a, b, 0, res);
        return res & MaskOf<BITS>();
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr UintOf<BITS> SBB(cpu::Flags& flags, UintOf<BITS> a, UintOf<BITS> b)
    {
        const UintOf<BITS> c = cpu::FlagCarry(flags) ? 1 : 0;
        const UintOf<2 * BITS> res = a - b - c;
        cpu::SetFlag<cpu::flag::CF>(flags, res & CarryMaskOf<BITS>());
        SetFlagsForSub<BITS>(flags, a, b, c, res);
        return res & MaskOf<BITS>();
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr UintOf<BITS> INC(cpu::Flags& flags, UintOf<BITS> a)
    {
        const auto carry = cpu::FlagCarry(flags);
        const auto res = ADD<BITS>(flags, a, 1);
        cpu::SetFlag<cpu::flag::CF>(flags, carry);
        return res;
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr UintOf<BITS> DEC(cpu::Flags& flags, UintOf<BITS> a)
    {
        const bool carry = cpu::FlagCarry(flags);
        const auto res = SUB<BITS>(flags, a, 1);
        cpu::SetFlag<cpu::flag::CF>(flags, carry);
        return res;
    }

    template<unsigned int BITS>
    [[nodiscard]] constexpr UintOf<BITS> NEG(cpu::Flags& flags, UintOf<BITS> a)
    {
        return SUB<BITS>(flags, 0, a);
    }

    template<unsigned int BITS>
    void CMP(cpu::Flags& flags, UintOf<BITS> a, UintOf<BITS> b)
    {
        // CMP only cares about the flag bits
        [[maybe_unused]] const auto _ = SUB<BITS>(flags, a, b);
    }

    constexpr void Mul8(cpu::Flags& flags, uint16_t& ax, uint8_t a)
    {
        flags &= ~(cpu::flag::CF | cpu::flag::OF);
        ax = (ax & 0xff) * (uint16_t)a;
        if (ax >= 0x100)
            flags |= (cpu::flag::CF | cpu::flag::OF);
    }

    constexpr void Mul16(cpu::Flags& flags, uint16_t& ax, uint16_t& dx, uint16_t a)
    {
        flags &= ~(cpu::flag::CF | cpu::flag::OF);
        dx = (ax * a) >> 16;
        ax = (ax * a) & 0xffff;
        if (dx != 0)
            flags |= (cpu::flag::CF | cpu::flag::OF);
    }

    constexpr void Imul8(cpu::Flags& flags, uint16_t& ax, uint8_t a)
    {
        flags &= ~(cpu::flag::CF | cpu::flag::OF);
        ax = (ax & 0xff) * (uint16_t)a;
        const uint8_t ah = (ax & 0xff00) >> 8;
        cpu::SetFlag<cpu::flag::CF | cpu::flag::OF>(flags, ah == 0 || ah == 0xff);
    }

    constexpr void Imul16(cpu::Flags& flags, uint16_t& ax, uint16_t& dx, uint16_t a)
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
