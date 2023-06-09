#include "rtc.h"
#include "io.h"
#include <time.h>

#include <array>
#include "spdlog/spdlog.h"


namespace
{
    namespace io
    {
        constexpr inline io_port Base = 0x70;

        constexpr inline io_port Index = Base + 0x0;
        constexpr inline io_port Data = Base + 0x1;
    }

    namespace rtc_register
    {
        constexpr inline uint8_t Seconds = 0x0;
        constexpr inline uint8_t Minutes = 0x2;
        constexpr inline uint8_t Hours = 0x4;
        constexpr inline uint8_t DayOfWeek = 0x6;
        constexpr inline uint8_t DayOfMonth = 0x7;
        constexpr inline uint8_t Month = 0x8;
        constexpr inline uint8_t Year = 0x9;
        constexpr inline uint8_t StatusA = 0xa;
        constexpr inline uint8_t StatusB = 0xb;
        constexpr inline uint8_t StatusC = 0xc;
        constexpr inline uint8_t StatusD = 0xd;
        constexpr inline uint8_t Century = 0x32;
    };

    uint8_t ValueToBcd(const uint8_t v)
    {
        return (v / 10) * 16 + (v % 10);
    }

    uint8_t ReadRtc(const uint8_t reg)
    {
        const auto t = time(nullptr);
        const auto tm = localtime(&t);
        switch(reg)
        {
            case rtc_register::Seconds:
                return ValueToBcd(tm->tm_sec);
            case rtc_register::Minutes:
                return ValueToBcd(tm->tm_min);
            case rtc_register::Hours:
                return ValueToBcd(tm->tm_hour);
            case rtc_register::DayOfWeek:
                return ValueToBcd(tm->tm_wday + 1);
            case rtc_register::DayOfMonth:
                return ValueToBcd(tm->tm_mday);
            case rtc_register::Month:
                return ValueToBcd(tm->tm_mon + 1);
            case rtc_register::Year:
                return ValueToBcd(tm->tm_year % 100);
            case rtc_register::Century:
                return ValueToBcd((tm->tm_year + 1900) / 100);
        }
        return 0;
    }
}

struct RTC::Impl : IOPeripheral
{
    std::array<uint8_t, 0x2f> cmosData{};
    uint8_t selectedRegister{};

    void Out8(io_port port, uint8_t val) override;
    void Out16(io_port port, uint16_t val) override;
    uint8_t In8(io_port port) override;
    uint16_t In16(io_port port) override;
};

RTC::RTC(IO& io)
    : impl(std::make_unique<Impl>())
{
    io.AddPeripheral(io::Base, 10, *impl);
}

RTC::~RTC() = default;

void RTC::Reset()
{
    impl->selectedRegister = {};
    std::fill(impl->cmosData.begin(), impl->cmosData.end(), 0);

    impl->cmosData[0x10] = 0x40;
}

void RTC::Impl::Out8(io_port port, uint8_t val)
{
    spdlog::info("rtc: out8({:x}, {:x})", port, val);
    switch(port) {
        case io::Index:
            selectedRegister = val;
            break;
    }
}

void RTC::Impl::Out16(io_port port, uint16_t val)
{
    spdlog::info("rtc: out16({:x}, {:x})", port, val);
}

uint8_t RTC::Impl::In8(io_port port)
{
    spdlog::info("rtc: in8({:x})", port);
    switch(port) {
        case io::Data:
            if (selectedRegister < rtc_register::StatusA || selectedRegister == rtc_register::Century) {
                return ReadRtc(selectedRegister);
            }
            return cmosData[selectedRegister % cmosData.size()];
    }
    return 0;
}

uint16_t RTC::Impl::In16(io_port port)
{
    spdlog::info("rtc: in16({:x})", port);
    return 0;
}
