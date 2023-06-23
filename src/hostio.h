#ifndef __HOSTIO_H__
#define __HOSTIO_H__

#include <cstdint>
#include <memory>

class HostIO final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    HostIO();
    ~HostIO();

    void Update();

    bool IsQuitting() const;
    void putpixel(unsigned int x, unsigned int y, uint32_t c);

};

#endif /* __HOSTIO_H__ */
