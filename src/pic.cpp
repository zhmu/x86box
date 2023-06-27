#include "pic.h"
#include "io.h"
#include <bit>

#include "spdlog/spdlog.h"

namespace
{
    namespace io
    {
        constexpr inline io_port Base = 0x20;
        constexpr inline io_port Command = Base + 0x0;
        constexpr inline io_port Data = Base + 0x1;
    }

    namespace icw1
    {
        constexpr inline uint8_t IC4 = (1 << 0);
        constexpr inline uint8_t SNGL = (1 << 1);
        constexpr inline uint8_t ADI = (1 << 2);
        constexpr inline uint8_t LTIM = (1 << 3);
        constexpr inline uint8_t ON = (1 << 4);
    }

    namespace icw4
    {
        constexpr inline uint8_t uPM = (1 << 0);
        constexpr inline uint8_t AEOI = (1 << 1);
        constexpr inline uint8_t MS = (1 << 2);
        constexpr inline uint8_t BUF = (1 << 3);
        constexpr inline uint8_t SFNM = (1 << 4);
    }

    namespace ocw2
    {
        constexpr inline uint8_t EOI = (1 << 5);
    }
}

struct PIC::Impl : IOPeripheral
{
    int irq_base = 0;
    int init_stage = -1;
    bool expect_icw3{};
    bool expect_icw4{};

    uint8_t irr{};
    uint8_t isr{};
    uint8_t imr{0xff};

    void AssertIRQ(int num);
    std::optional<int> DequeuePendingIRQ();

    void Out8(io_port port, uint8_t val) override;
    void Out16(io_port port, uint16_t val) override;
    uint8_t In8(io_port port) override;
    uint16_t In16(io_port port) override;
};

PIC::PIC(IO& io)
    : impl(std::make_unique<Impl>())
{
    io.AddPeripheral(io::Base, 2, *impl);
}

PIC::~PIC() = default;

void PIC::Reset()
{
    impl->irq_base = 0;
    impl->init_stage = -1;
    impl->expect_icw3 = {};
    impl->expect_icw4 = {};
    impl->imr = 0xff;
}

void PIC::AssertIRQ(int no)
{
    impl->AssertIRQ(no);
}

std::optional<int> PIC::DequeuePendingIRQ()
{
    return impl->DequeuePendingIRQ();
}

void PIC::Impl::Out8(io_port port, uint8_t val)
{
    const auto handleICW4 = [&]() {
        spdlog::info("pic: icw4 {:x}", val);
        if (val & icw4::AEOI)
            spdlog::error("pic: Auto EOI not implemented");
        init_stage = -1;
    };

    spdlog::info("pic: out8({:x}, {:x})", port, val);
    switch(port) {
        case io::Command:
            if (val & icw1::ON) {
                spdlog::info("pic: initialization {:x}", val);
                expect_icw3 = (val & icw1::SNGL) == 0;
                expect_icw4 = (val & icw1::IC4) != 0;
                init_stage = 0;
            } else {
                spdlog::info("pic: command {:x}", val);
                if (val & ocw2::EOI) {
                    auto irq = std::countr_zero(isr);
                    spdlog::info("pic: eoi, current active irq {}", irq);
                    isr &= ~(1 << irq);
                }
            }
            break;
        case io::Data:
            switch(init_stage) {
                case -1: // not initializing
                    spdlog::info("pic: mask {:x}", val);
                    imr = val;
                    break;
                case 0: // ICW2
                    spdlog::info("pic: icw2 {:x}", val);
                    irq_base = val;
                    if (!expect_icw3 && !expect_icw4)
                        init_stage = -1;
                    else
                        ++init_stage;
                    break;
                case 1: // ICW3/ICW4
                    if (expect_icw3) {
                        spdlog::info("pic: icw3 {:x}", val);
                        if (expect_icw4)
                            ++init_stage;
                        else
                            init_stage = -1;
                    } else if (expect_icw4) {
                        handleICW4();
                        init_stage = -1;
                    }
                    break;
                case 2: // ICW4
                    handleICW4();
                    break;
            }
            break;
    }
}

void PIC::Impl::Out16(io_port port, uint16_t val)
{
    spdlog::info("pic: out16({:x}, {:x})", port, val);
}

uint8_t PIC::Impl::In8(io_port port)
{
    spdlog::info("pic: in16({:x})", port);
    if (port == io::Data) return imr;
    return 0;
}

uint16_t PIC::Impl::In16(io_port port)
{
    spdlog::info("pic: in16({:x})", port);
    return 0;
}

void PIC::Impl::AssertIRQ(int num)
{
    assert(num >= 0 && num < 7);
    irr |= (1 << num);
    spdlog::info("pic: assertIrq {:x} -> irr {:x}", num, irr);
}
 
 std::optional<int> PIC::Impl::DequeuePendingIRQ()
 {
    uint8_t pendingIrqs = (irr & ~isr) & ~imr;
    if (pendingIrqs == 0) return {};

    auto irq = std::countr_zero(pendingIrqs);

    spdlog::info("pic: irr {:x} imr {:x} -> irq {:x}", irr, imr, irq);

    irr &= ~(1 << irq);
    isr |= (1 << irq);
    return irq_base + irq;
 }
