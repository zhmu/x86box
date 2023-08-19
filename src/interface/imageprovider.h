#pragma once

#include <cstdint>
#include <span>

enum class Image
{
    Floppy0,
    Harddisk0,
    Harddisk1,
    COUNT
};

using Bytes = size_t;

class ImageProvider
{
public:
    virtual ~ImageProvider() = default;

    virtual Bytes GetSize(const Image image) = 0;
    virtual size_t Read(const Image image, uint64_t offset, std::span<uint8_t> data) = 0;
    virtual size_t Write(const Image image, uint64_t offset, std::span<const uint8_t> data) = 0;
};
