#pragma once

#include <memory>
#include <span>

struct DMATransfer
{
    virtual size_t WriteFromPeripheral(uint16_t offset, std::span<const uint8_t> data) = 0;
    virtual size_t GetTotalLength() = 0;
    virtual void Complete() = 0;
};

struct DMAInterface
{
    virtual std::unique_ptr<DMATransfer> InitiateTransfer(int ch_num) = 0;
};
