#ifndef __IO_H__
#define __IO_H__

#include <stdint.h>

class IO
{
  public:
    IO();
    ~IO();

    typedef uint16_t port_t;

    void Reset();

    void Out8(port_t port, uint8_t val);
    void Out16(port_t port, uint16_t val);

    uint8_t In8(port_t port);
    uint16_t In16(port_t port);
};

#endif /* __IO_H__ */
