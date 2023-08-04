#pragma once

struct PITInterface
{
    virtual ~PITInterface() = default;

    virtual bool GetTimer2Output() const = 0;
};
