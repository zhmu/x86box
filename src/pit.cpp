#include "pit.h"
#include "io.h"

#include <array>
#include <chrono>
#include "spdlog/spdlog.h"

namespace
{
    constexpr inline auto pitFrequency = 1'193'182; // Hz

    namespace io
    {
        constexpr inline io_port Base = 0x40;
        constexpr inline io_port Data0 = Base + 0x0;
        constexpr inline io_port Data1 = Base + 0x1;
        constexpr inline io_port Data2 = Base + 0x2;
        constexpr inline io_port Mode_Command = Base + 0x3;
    }

    namespace cw
    {
        constexpr uint8_t SelectChannel(uint8_t val) { return (val >> 6) & 3; }
        constexpr uint8_t AccessMode(uint8_t val) { return (val >> 4) & 3; }
        constexpr uint8_t OperatingMode(uint8_t val) { return (val >> 1) & 7; }
        constexpr inline uint8_t BCD = (1 << 0);
    }
}

struct PIT::Impl : IOPeripheral
{
    struct Channel {
        uint16_t counter{};
        uint32_t reload{};
        uint8_t access{};
        uint8_t mode{};
        int prev_mode = -1;
        uint8_t latch{};
        bool active{};
        bool current_output{};
        // When was the count last set?
        std::chrono::steady_clock::time_point count_time{};

        enum class State {
            LoByte,
            HiByte,
            LoAndHi1,
            LoAndHi2,
        } state{State::LoByte};
    };

    std::array<Channel, 3> channel;
    uint8_t control{};

    void Out8(io_port port, uint8_t val) override;
    void Out16(io_port port, uint16_t val) override;
    uint8_t In8(io_port port) override;
    uint16_t In16(io_port port) override;

    bool TickChannel(size_t ch_num, std::chrono::steady_clock::time_point now);
};

PIT::PIT(IO& io)
    : impl(std::make_unique<Impl>())
{
    io.AddPeripheral(io::Base, 4, *impl);
}

PIT::~PIT() = default;

void PIT::Reset()
{
    impl->control = 0;
    std::fill(impl->channel.begin(), impl->channel.end(), Impl::Channel{});
}

 bool PIT::Tick()
 {
    const auto now = std::chrono::steady_clock::now();;

    bool signal_irq = false;
    for(size_t ch_num = 0; ch_num < impl->channel.size(); ++ch_num)
    {
        const auto output = impl->TickChannel(ch_num, now);
        // Only signal IRQ0 if the output from channel 0 changes
        if (ch_num == 0 && !impl->channel[ch_num].current_output && output) {
            signal_irq = output;
        }
        impl->channel[ch_num].current_output = output;
    }

    return signal_irq;
 }

void PIT::Impl::Out8(io_port port, uint8_t val)
{
    spdlog::info("pit: out8({:x}, {:x})", port, val);
    switch(port) {
        case io::Mode_Command: {
            const auto sc = cw::SelectChannel(val);
            if (sc == 0b1100'0000) {
                // Read-back
                spdlog::error("pit: read-back not supported");
            } else {
                const auto am = cw::AccessMode(val);
                const auto om = cw::OperatingMode(val);
                const auto bcd = (val & cw::BCD) != 0;
                auto& ch = channel[sc];
                ch.access = am;
                ch.mode = om;
                if (bcd) spdlog::error("pit: ch{}: bcd mode not supported", sc);
                switch(am) {
                    case 0: // Latch count value
                        ch.latch = ch.counter & 0xff;
                        break;
                    case 1: // Access mode: Low byte only
                        ch.state = Channel::State::LoByte;
                        ch.active = false;
                        break;
                    case 2: // Access mode: High byte only
                        ch.state = Channel::State::HiByte;
                        ch.active = false;
                        break;
                    case 3: // Access mode: Low byte followed by high byte
                        ch.state = Channel::State::LoAndHi1;
                        ch.active = false;
                        break;
                }
                spdlog::info("pit: ch{}: mode: am {} om {} bcd {}", sc, am, om, bcd);
            }
            break;
        }
        case io::Data0:
        case io::Data1:
        case io::Data2: {
            auto& ch = channel[port - io::Data0];
            switch(ch.state) {
                case Channel::State::HiByte:
                    ch.reload = static_cast<uint16_t>(val) << 8;
                    ch.active = true;
                    ch.count_time = std::chrono::steady_clock::now();;
                    break;
                case Channel::State::LoAndHi2:
                    ch.reload = (ch.reload & 0x00ff) | (static_cast<uint16_t>(val) << 8);
                    ch.active = true;
                    ch.count_time = std::chrono::steady_clock::now();;
                    break;
                case Channel::State::LoAndHi1:
                    ch.reload = (ch.reload & 0xff00) | val;
                    ch.state = Channel::State::LoAndHi2;
                    break;
                case Channel::State::LoByte:
                    ch.reload = val;
                    ch.active = true;
                    ch.count_time = std::chrono::steady_clock::now();;
                    ch.state = Channel::State::LoAndHi1;
                    break;
            }
            if (ch.reload == 0) ch.reload = 0x10000;
            spdlog::info("pit: ch{}: setting reload to {:x}", port - io::Data0, ch.reload);
            break;
        }
    }
}

void PIT::Impl::Out16(io_port port, uint16_t val)
{
    spdlog::info("pit: out16({:x}, {:x})", port, val);
}

uint8_t PIT::Impl::In8(io_port port)
{
    spdlog::info("pit: in8({:x})", port);
    if (port >= io::Data0 && port <= io::Data2) {
        auto& ch = channel[port - io::Data0];
        spdlog::info("pit: reading ch{}: counter value {:x}, access {}", port - io::Data0, ch.counter, ch.access);
        switch(ch.access) {
            case 0: // Latch count value
                return ch.latch;
            case 1: // Access mode: Low byte only
            default:
                return ch.counter & 0xff;
            case 2: // Access mode: High byte only
                return ch.counter >> 8;
        }
    }
    return 0;
}

uint16_t PIT::Impl::In16(io_port port)
{
    spdlog::info("pit: in16({:x})", port);
    return 0;
}

bool PIT::Impl::TickChannel(size_t ch_num, std::chrono::steady_clock::time_point now)
{
    auto& ch = channel[ch_num];
    if (!ch.active) return false;

    // Compute current channel count - we need to go from absolute delta (in ns) to PIT counts
    const uint64_t count =
        (std::chrono::duration_cast<std::chrono::nanoseconds>(now - ch.count_time).count() * pitFrequency) / 1'000'000'000;
    bool output = false;
    switch(ch.mode)
    {
        case 0: // Interrupt On Terminal Count
            if (ch.prev_mode != ch.mode)
                spdlog::error("pit: channel {}: 'interrupt on terminal count' mode not implemented", ch_num);
            break;
        case 1: // Hardware Re-Triggerable One-shot
            if (ch.prev_mode != ch.mode)
                spdlog::error("pit: channel {}: 'hardware re-triggerable one-shot' mode not implemented", ch_num);
            break;
        case 2:
        case 6: // Rate generator
            if (ch.prev_mode != ch.mode)
                spdlog::error("pit: channel {}: 'rate generator' mode not implemented", ch_num);
            break;
        case 3: // Square Wave Generator
        case 7: {
            // Datasheet, mode 3 implementation states that for ODD counts:
            // "OUT will be high for (N + 1) / 2 counts and low for (N - 1) / 2 counts"
            // TODO: Figure out whether EVEN counts need special handling here...
            output = (count % ch.reload) < ((ch.reload + 1) / 2);
            break;
        }
        case 4: // Software Triggered Strobe
            if (ch.prev_mode != ch.mode)
                spdlog::error("pit: channel {}: 'software triggered strobe' mode not implemented", ch_num);
            break;
        case 5: // Hardware Triggered Strobe
            if (ch.prev_mode != ch.mode)
                spdlog::error("pit: channel {}: 'hardware triggered strobe' mode not implemented", ch_num);
            break;
    }
        
    ch.prev_mode = ch.mode;
    return output;
}

bool PIT::GetTimer2Output() const
{
    return impl->channel[2].current_output;
}