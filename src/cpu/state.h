#pragma once

#include <cstdint>
#include <optional>

namespace cpu
{
    using Flags = uint16_t;

    namespace flag
    {
        static constexpr inline Flags CF = (1 << 0);
        static constexpr inline Flags ON = (1 << 1); // Always set
        static constexpr inline Flags PF = (1 << 2);
        static constexpr inline Flags AF = (1 << 4);
        static constexpr inline Flags ZF = (1 << 6);
        static constexpr inline Flags SF = (1 << 7);
        static constexpr inline Flags TF = (1 << 8);
        static constexpr inline Flags IF = (1 << 9);
        static constexpr inline Flags DF = (1 << 10);
        static constexpr inline Flags OF = (1 << 11);
    }

    // These must be in sync with the x86 segment values (Sw)
    enum class Segment
    {
        ES = 0,
        CS = 1,
        SS = 2,
        DS = 3
    };

    //! \brief CPU state
    class State
    {
      public:
        uint16_t m_ax, m_cx, m_dx, m_bx, m_sp, m_bp, m_si, m_di, m_ip;
        uint16_t m_es, m_cs, m_ss, m_ds;
        uint16_t m_flags;
        std::optional<Segment> m_seg_override;
    };

    void Dump(const State& state);

    template<uint16_t Flag>
    constexpr void SetFlag(Flags& flags, bool set)
    {
        if (set)
            flags |= Flag;
        else
            flags &= ~Flag;
    }

    template<uint16_t Flag>
    [[nodiscard]] constexpr bool IsFlagSet(Flags flags) { return (flags & Flag) != 0; };

    [[nodiscard]] constexpr bool FlagCarry(Flags flags) { return IsFlagSet<flag::CF>(flags); }
    [[nodiscard]] constexpr bool FlagAuxiliaryCarry(Flags flags) { return IsFlagSet<flag::AF>(flags); }
    [[nodiscard]] constexpr bool FlagZero(Flags flags) { return IsFlagSet<flag::ZF>(flags); }
    [[nodiscard]] constexpr bool FlagParity(Flags flags) { return IsFlagSet<flag::PF>(flags); }
    [[nodiscard]] constexpr bool FlagSign(Flags flags) { return IsFlagSet<flag::SF>(flags); }
    [[nodiscard]] constexpr bool FlagDirection(Flags flags) { return IsFlagSet<flag::DF>(flags); }
    [[nodiscard]] constexpr bool FlagOverflow(Flags flags) { return IsFlagSet<flag::OF>(flags); }
    [[nodiscard]] constexpr bool FlagInterrupt(Flags flags) { return IsFlagSet<flag::IF>(flags); }
}
