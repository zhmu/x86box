#pragma once

#include <memory>
#include "../interface/imageprovider.h"

struct ImageProvider;

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
