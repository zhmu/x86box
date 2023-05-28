#ifndef __HOSTIO_H__
#define __HOSTIO_H__

#include <SDL/SDL.h>
#include <stdint.h>

class HostIO
{
  public:
    HostIO();

    bool Initialize();
    void Cleanup();
    void Update();

    bool Quitting() const { return m_quitting; }
    void putpixel(unsigned int x, unsigned int y, uint32_t c);

  protected:
    SDL_Surface* m_screen;
    SDL_Surface* m_display;
    bool m_quitting;
};

#endif /* __HOSTIO_H__ */
