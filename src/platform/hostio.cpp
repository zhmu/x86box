#include "hostio.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <deque>
#include "../hw/vga.h" // for VGA:s_...

namespace
{
    uint16_t MapSDLKeycodeToScancodeSet1(const SDL_Keycode code)
    {
        switch(code)
        {
			case SDLK_ESCAPE: return 0x01;
			case SDLK_1: return 0x02;
			case SDLK_2: return 0x03;
			case SDLK_3: return 0x04;
			case SDLK_4: return 0x05;
			case SDLK_5: return 0x06;
			case SDLK_6: return 0x07;
			case SDLK_7: return 0x08;
			case SDLK_8: return 0x09;
			case SDLK_9: return 0x0a;
			case SDLK_0: return 0x0b;
			case SDLK_MINUS: return 0x0c;
			case SDLK_EQUALS: return 0x0d;
			case SDLK_BACKSPACE: return 0x0e;
			case SDLK_TAB: return 0x0f;
			case SDLK_q: return 0x10;
			case SDLK_w: return 0x11;
			case SDLK_e: return 0x12;
			case SDLK_r: return 0x13;
			case SDLK_t: return 0x14;
			case SDLK_y: return 0x15;
			case SDLK_u: return 0x16;
			case SDLK_i: return 0x17;
			case SDLK_o: return 0x18;
			case SDLK_p: return 0x19;
			case SDLK_LEFTBRACKET: return 0x1a;
			case SDLK_RIGHTBRACKET: return 0x1b;
			case SDLK_RETURN: return 0x1c;
			case SDLK_LCTRL: return 0x1d;
			case SDLK_a: return 0x1e;
			case SDLK_s: return 0x1f;
			case SDLK_d: return 0x20;
			case SDLK_f: return 0x21;
			case SDLK_g: return 0x22;
			case SDLK_h: return 0x23;
			case SDLK_j: return 0x24;
			case SDLK_k: return 0x25;
			case SDLK_l: return 0x26;
			case SDLK_SEMICOLON: return 0x27;
			case SDLK_QUOTE: return 0x28;
			case SDLK_BACKQUOTE: return 0x29;
			case SDLK_LSHIFT: return 0x2a;
			case SDLK_BACKSLASH: return 0x2b;
			case SDLK_z: return 0x2c;
			case SDLK_x: return 0x2d;
			case SDLK_c: return 0x2e;
			case SDLK_v: return 0x2f;
			case SDLK_b: return 0x30;
			case SDLK_n: return 0x31;
			case SDLK_m: return 0x32;
			case SDLK_COMMA: return 0x33;
			case SDLK_PERIOD: return 0x34;
			case SDLK_SLASH: return 0x35;
			case SDLK_RSHIFT: return 0x36;
			case SDLK_KP_MULTIPLY: return 0x37;
			case SDLK_LALT: return 0x38;
			case SDLK_SPACE: return 0x39;
			case SDLK_CAPSLOCK: return 0x3a;
			case SDLK_F1: return 0x3b;
			case SDLK_F2: return 0x3c;
			case SDLK_F3: return 0x3d;
			case SDLK_F4: return 0x3e;
			case SDLK_F5: return 0x3f;
			case SDLK_F6: return 0x40;
			case SDLK_F7: return 0x41;
			case SDLK_F8: return 0x42;
			case SDLK_F9: return 0x43;
			case SDLK_F10: return 0x44;
			case SDLK_NUMLOCKCLEAR: return 0x45; // ?
			case SDLK_SCROLLLOCK: return 0x46;
			case SDLK_KP_7: return 0x47;
			case SDLK_KP_8: return 0x48;
			case SDLK_KP_9: return 0x49;
			case SDLK_KP_MINUS: return 0x4a;
			case SDLK_KP_4: return 0x4b;
			case SDLK_KP_5: return 0x4c;
			case SDLK_KP_6: return 0x4d;
			case SDLK_KP_PLUS: return 0x4e;
			case SDLK_KP_1: return 0x4f;
			case SDLK_KP_2: return 0x50;
			case SDLK_KP_3: return 0x51;
			case SDLK_KP_0: return 0x52;
			case SDLK_KP_PERIOD: return 0x53;

			case SDLK_F11: return 0x57;
			case SDLK_F12: return 0x58;

			case SDLK_RCTRL: return 0xe01d;
			case SDLK_KP_DIVIDE: return 0xe035;
			case SDLK_RALT: return 0xe038;
			case SDLK_HOME: return 0xe047;
			case SDLK_UP: return 0xe048;
			case SDLK_PAGEUP: return 0xe049;
			case SDLK_LEFT: return 0xe04b;
			case SDLK_RIGHT: return 0xe04d;
			case SDLK_END: return 0xe04f;
			case SDLK_DOWN: return 0xe050;
			case SDLK_PAGEDOWN: return 0xe051;
			case SDLK_INSERT: return 0xe052;
			case SDLK_DELETE: return 0xe053;

			//case SDLK_PRINTSCREEN: return 0x__;
			//case SDLK_PAUSE: return 0x__;
            default:
                return 0;
        }
    }
}

struct HostIO::Impl
{
    Impl();
    ~Impl();

    std::unique_ptr<uint32_t[]> frameBuffer;
    SDL_Window* window{};
    SDL_Renderer* renderer{};
    SDL_Texture* texture{};
    std::deque<uint16_t> pendingScancodes;
    std::deque<EventType> pendingEvents;
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

void HostIO::Render()
{
    SDL_UpdateTexture(impl->texture, nullptr, impl->frameBuffer.get(), VGA::s_video_width * sizeof(uint32_t));
    SDL_RenderCopy(impl->renderer, impl->texture, nullptr, nullptr);
    SDL_RenderPresent(impl->renderer);
}

void HostIO::Update()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                impl->pendingEvents.push_back(EventType::Terminate);
                break;
			case SDL_KEYDOWN: {
				if (event.key.keysym.sym == SDLK_BACKQUOTE && (event.key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL)) ) {
					impl->pendingEvents.push_back(EventType::ChangeImageFloppy0);
					break;
				}

                const auto scancode = MapSDLKeycodeToScancodeSet1(event.key.keysym.sym);
                if (scancode != 0) {
                    impl->pendingScancodes.push_back(scancode);
                }
				break;
            }
			case SDL_KEYUP: {
                const auto scancode = MapSDLKeycodeToScancodeSet1(event.key.keysym.sym);
                if (scancode != 0) {
                    impl->pendingScancodes.push_back(scancode | 0x80);
                }
				break;
            }
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

uint16_t HostIO::GetAndClearPendingScanCode()
{
    if (impl->pendingScancodes.empty()) return 0;
    const auto scancode = impl->pendingScancodes.front();
    impl->pendingScancodes.pop_front();
    return scancode;
}

std::optional<HostIO::EventType> HostIO::GetPendingEvent()
{
    if (impl->pendingEvents.empty()) return {};
    const auto event = impl->pendingEvents.front();
    impl->pendingEvents.pop_front();
    return event;
}
