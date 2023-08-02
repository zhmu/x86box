#pragma once

#include <cstdint>
#include <span>

enum class Image
{
    Floppy0,
    Harddisk0,
    COUNT
};

class ImageProvider
{
public:
    virtual size_t Read(const Image image, uint64_t offset, std::span<uint8_t> data) = 0;
    virtual size_t Write(const Image image, uint64_t offset, std::span<const uint8_t> data) = 0;
};
