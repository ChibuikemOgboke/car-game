TARGET = carcollector
OBJS = main.o

CFLAGS = -O2 -G0 -Wall -std=gnu99
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)

# SDL2 flags from the pspdev install
CFLAGS += $(shell $(shell psp-config --psp-prefix)/bin/sdl2-config --cflags)

# Order matters for static libs: users first, their dependencies after.
# modplug/harfbuzz are C++ libs (need -lstdc++); bz2 is needed by freetype.
LIBS = -lSDL2_mixer -lSDL2_ttf -lSDL2_image \
       $(shell $(shell psp-config --psp-prefix)/bin/sdl2-config --libs) \
       -lmodplug -lharfbuzz -lfreetype -lbz2 -lpng -ljpeg -lz \
       -lmpg123 -lvorbisfile -lvorbis -logg \
       -lstdc++ -lm \
       -lpspaudio -lpspgu -lpspge -lpspdisplay -lpspctrl -lpsppower -lpspvram

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Car Collector

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
