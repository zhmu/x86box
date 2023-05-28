#include "hostio.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include "vga.h"

bool HostIO::Initialize()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
        return false;

    window = SDL_CreateWindow(
        "x86box", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        VGA::s_video_width, VGA::s_video_height, 0);
    renderer = SDL_CreateRenderer(window, 0, 0);
    texture = SDL_CreateTexture(renderer,
            SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, VGA::s_video_width, VGA::s_video_height);

    frameBuffer = std::make_unique<uint32_t[]>(VGA::s_video_height * VGA::s_video_width);

    return true;
}

void HostIO::Cleanup()
{
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void HostIO::Update()
{
    SDL_UpdateTexture(texture, nullptr, frameBuffer.get(), VGA::s_video_width * sizeof(uint32_t));
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                m_quitting = true;
                break;
#if 0
			case SDL_KEYDOWN:
				HandleKey(event.key.keysym.sym, true);
				break;
			case SDL_KEYUP:
				HandleKey(event.key.keysym.sym, false);
				break;
#endif
        }
    }
}

void HostIO::putpixel(unsigned int x, unsigned int y, uint32_t c)
{
    if (x >= (unsigned int)VGA::s_video_width || y >= (unsigned int)VGA::s_video_height)
        return;

    uint32_t* p = frameBuffer.get() + y * VGA::s_video_width + x;
    *p = c;
}
