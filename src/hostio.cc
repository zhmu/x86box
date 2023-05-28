#include "hostio.h"
#include <SDL/SDL.h>
#include <assert.h>
#include "vga.h"

HostIO::HostIO()
{
	m_quitting = false;
}

bool
HostIO::Initialize()
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
		return false;

	m_screen = SDL_SetVideoMode(VGA::s_video_width, VGA::s_video_height, 32, SDL_SWSURFACE);
	if (m_screen == NULL)
		return false;
	assert(m_screen->format->BytesPerPixel == 4);

	m_display = SDL_CreateRGBSurface(SDL_SWSURFACE, VGA::s_video_width, VGA::s_video_height, 32, 0, 0, 0, 0);
	if (m_display == NULL)
		return false;

	SDL_Rect r;
	r.x = 0; r.y = 0;
	r.h = VGA::s_video_height; r.w = VGA::s_video_width;
	SDL_FillRect(m_display, &r, SDL_MapRGB(m_display->format, 100, 100, 100));

	return true;
}

void
HostIO::Cleanup()
{
	SDL_FreeSurface(m_display);
	SDL_Quit();
}

void
HostIO::Update()
{
	SDL_BlitSurface(m_display, NULL, m_screen, NULL);
	SDL_Flip(m_screen);

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch(event.type) {
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

void
HostIO::putpixel(unsigned int x, unsigned int y, uint32_t c)
{
	if (x >= (unsigned int)VGA::s_video_width || y >= (unsigned int)VGA::s_video_height)
		return;

	Uint32* p = (Uint32*)m_display->pixels + y * m_display->pitch/4 + x;
	*p = c;
}

/* vim:set ts=2 sw=2: */
