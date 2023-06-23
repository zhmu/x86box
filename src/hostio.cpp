#include "hostio.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include "vga.h"

struct HostIO::Impl
{
    Impl();
    ~Impl();

    std::unique_ptr<uint32_t[]> frameBuffer;
    SDL_Window* window{};
    SDL_Renderer* renderer{};
    SDL_Texture* texture{};
    bool quitting = false;
};

HostIO::Impl::Impl()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
        std::abort();

    window = SDL_CreateWindow(
        "x86box", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        VGA::s_video_width, VGA::s_video_height, 0);
    renderer = SDL_CreateRenderer(window, 0, 0);
    texture = SDL_CreateTexture(renderer,
            SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, VGA::s_video_width, VGA::s_video_height);

    frameBuffer = std::make_unique<uint32_t[]>(VGA::s_video_height * VGA::s_video_width);
}

HostIO::Impl::~Impl()
{
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}

HostIO::HostIO()
    : impl(std::make_unique<Impl>())
{
}

HostIO::~HostIO() = default;

void HostIO::Update()
{

    SDL_UpdateTexture(impl->texture, nullptr, impl->frameBuffer.get(), VGA::s_video_width * sizeof(uint32_t));
    SDL_RenderCopy(impl->renderer, impl->texture, nullptr, nullptr);
    SDL_RenderPresent(impl->renderer);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                impl->quitting = true;
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

    auto p = impl->frameBuffer.get() + y * VGA::s_video_width + x;
    *p = c;
}

bool HostIO::IsQuitting() const
{
    return impl->quitting;
}