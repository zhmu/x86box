#ifndef __HOSTIO_H__
#define __HOSTIO_H__

#include <SDL2/SDL.h>
#include <stdint.h>
#include <memory>
#include "vga.h"

class HostIO
{
  public:
    bool Initialize();
    void Cleanup();
    void Update();

    bool Quitting() const { return m_quitting; }
    void putpixel(unsigned int x, unsigned int y, uint32_t c);

  protected:
    std::unique_ptr<uint32_t[]> frameBuffer;
    SDL_Window* window{};
    SDL_Renderer* renderer{};
    SDL_Texture* texture{};
    bool m_quitting = false;
};

#endif /* __HOSTIO_H__ */
