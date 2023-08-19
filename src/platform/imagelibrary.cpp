#include "imagelibrary.h"
#include <fcntl.h>
#include <unistd.h>

namespace
{
    struct ImageFile final
    {
        ~ImageFile();

        int fd = -1;
        uint64_t length = 0;

        bool Attach(const int fd);
        size_t Read(uint64_t offset, std::span<uint8_t> data);
        size_t Write(uint64_t offset, std::span<const uint8_t> data);
    };

    ImageFile::~ImageFile()
    {
        close(fd);
    }

    bool ImageFile::Attach(const int newFd)
    {
        if (fd >= 0) close(fd);
        fd = newFd;
        length = lseek(fd, 0, SEEK_END);
        return true;
    }

    size_t ImageFile::Read(uint64_t offset, std::span<uint8_t> data)
    {
        const auto result = pread(fd, data.data(), data.size(), offset);
        return std::max(static_cast<ssize_t>(0), result);
    }

    size_t ImageFile::Write(uint64_t offset, std::span<const uint8_t> data)
    {
        const auto result = pwrite(fd, data.data(), data.size(), offset);
        return std::max(static_cast<ssize_t>(0), result);
    }
}

struct ImageLibrary::Impl : ImageProvider
{
    std::array<ImageFile, static_cast<size_t>(Image::COUNT)> imageFiles;

    Bytes GetSize(const Image image) override;
    size_t Read(const Image image, uint64_t offset, std::span<uint8_t> data) override;
    size_t Write(const Image image, uint64_t offset, std::span<const uint8_t> data) override;
};

Bytes ImageLibrary::Impl::GetSize(const Image image)
{
    auto& imageFile = imageFiles[static_cast<size_t>(image)];
    return imageFile.length;
}

size_t ImageLibrary::Impl::Read(const Image image, uint64_t offset, std::span<uint8_t> data)
{
    auto& imageFile = imageFiles[static_cast<size_t>(image)];
    return imageFile.Read(offset, data);
}

size_t ImageLibrary::Impl::Write(const Image image, uint64_t offset, std::span<const uint8_t> data)
{
    auto& imageFile = imageFiles[static_cast<size_t>(image)];
    return imageFile.Write(offset, data);
}

ImageLibrary::ImageLibrary()
    : impl(std::make_unique<Impl>())
{
}

ImageLibrary::~ImageLibrary() = default;

ImageProvider& ImageLibrary::GetImageProvider()
{
    return *impl;
}

bool ImageLibrary::SetImage(const Image image, const char* path)
{
    int fd = open(path, O_RDWR);
    if (fd < 0)
        return false;

    auto& imageFile = impl->imageFiles[static_cast<size_t>(image)];
    return imageFile.Attach(fd);
}
