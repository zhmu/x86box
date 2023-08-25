#ifndef __HOSTIO_H__
#define __HOSTIO_H__

#include <cstdint>
#include <memory>
#include <optional>

class HostIO final
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    HostIO();
    ~HostIO();

    void Render();
    void Update();

    void putpixel(unsigned int x, unsigned int y, uint32_t c);

    uint16_t GetAndClearPendingScanCode();

    enum class EventType
    {
      Terminate,
      ChangeImageFloppy0,
    };

    std::optional<EventType> GetPendingEvent();
};

#endif /* __HOSTIO_H__ */
