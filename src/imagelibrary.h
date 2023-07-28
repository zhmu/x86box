#pragma once

#include <memory>
#include "imageprovider.h"

class ImageLibrary
{
    struct Impl;
    std::unique_ptr<Impl> impl;

public:
    ImageLibrary();
    ~ImageLibrary();

    bool SetImage(const Image image, const char* path);

    ImageProvider& GetImageProvider();
};