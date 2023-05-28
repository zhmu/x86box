OBJS=		main.o cpux86.o memory.o io.o vga.o hostio.o file.o dos.o \
		vectors.o
LDFLAGS+=       `sdl-config --libs`
CXXFLAGS+=  -g

x86box:		$(OBJS)
		$(CXX) -o x86box $(OBJS) $(LDFLAGS)

vga.cc:		vgafont.h

vgafont.h:	../doc/vga-rom.f08 ../doc/font2h.pl
		../doc/font2h.pl < ../doc/vga-rom.f08 > vgafont.h

clean:
		rm -f x86box $(OBJS) vgafont.h
