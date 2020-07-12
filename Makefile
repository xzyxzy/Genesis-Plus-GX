
# Makefile for genplus SDL2
#
# (c) 1999, 2000, 2001, 2002, 2003  Charles MacDonald
# modified by Eke-Eke <eke_eke31@yahoo.fr>
#
# Defines :
# -DLSB_FIRST : for little endian systems.
# -DLOGERROR  : enable message logging
# -DLOGVDP    : enable VDP debug messages
# -DLOGSOUND  : enable AUDIO debug messages
# -DLOG_SCD   : enable SCD debug messages
# -DLOG_CDD   : enable CDD debug messages
# -DLOG_CDC   : enable CDC debug messages
# -DLOG_PCM   : enable PCM debug messages
# -DLOGSOUND  : enable AUDIO debug messages
# -D8BPP_RENDERING  - configure for 8-bit pixels (RGB332)
# -D15BPP_RENDERING - configure for 15-bit pixels (RGB555)
# -D16BPP_RENDERING - configure for 16-bit pixels (RGB565)
# -D32BPP_RENDERING - configure for 32-bit pixels (RGB888)
# -DUSE_LIBCHDR      : enable CHD file support
# -DUSE_LIBTREMOR    : enable OGG file support for CD emulation using provided TREMOR library
# -DUSE_LIBVORBIS    : enable OGG file support for CD emulation using external VORBIS library
# -DISABLE_MANY_OGG_OPEN_FILES : only have one OGG file opened at once to save RAM
# -DMAXROMSIZE       : defines maximal size of ROM/SRAM buffer (also shared with CD hardware)
# -DHAVE_YM3438_CORE : enable (configurable) support for Nuked cycle-accurate YM3438 core
# -DHOOK_CPU         : enable CPU hooks

NAME	  = gen
BINDIR	  = bin
SUFFIX	  = 
CC        = gcc
PKGCONFIG = pkg-config

STATIC	  = 1

BACKEND_VIDEO ?= sdl2
BACKEND_INPUT ?= sdl2
BACKEND_AUDIO ?= sdl2mixer

SRCDIR    = ./src

# =============================================================================
# Detect default platform if not explicitly specified
# =============================================================================

ifeq ($(OS),Windows_NT)
	PLATFORM ?= Windows
endif

ifdef EMSCRIPTEN
	PLATFORM = Emscripten
endif

PLATFORM ?= Unknown

# =============================================================================
# Detect explicitly-defined platforms
# =============================================================================

ifeq ($(PLATFORM),Emscripten)
	CC = $(CXX)
	PKGCONFIG = :
	CFLAGS += -s USE_SDL=2 -s USE_SDL_MIXER=2 -s USE_ZLIB=1 -s USE_VORBIS=1 -s TOTAL_MEMORY=41484288 -s LLD_REPORT_UNDEFINED -s ALLOW_MEMORY_GROWTH=1 --preload-file "rom.bin"
	DEFINES += -DHAVE_ALLOCA_H
	SUFFIX = .html
endif

ifeq ($(PLATFORM),Switch)
	include $(DEVKITPRO)/libnx/switch_rules
	INCLUDES += -I$(LIBNX)/include -I$(PORTLIBS)/include/SDL2 -I$(PORTLIBS)/include

	CFLAGS += -DARM -march=armv8-a -mtune=cortex-a57 -mtp=soft -DLSB_FIRST -DBYTE_ORDER=LITTLE_ENDIAN -DALIGN_LONG -DALIGN_WORD -DM68K_OVERCLOCK_SHIFT=20 -DHAVE_ZLIB -DSWITCH -fPIE -Wl,--allow-multiple-definition -specs=$(DEVKITPRO)/libnx/switch.specs
	LIBS += -L$(LIBNX)/lib -lnx -lm
	PKGCONFIG = $(DEVKITPRO)/portlibs/switch/bin/aarch64-none-elf-pkg-config

	ifdef NXLINK
		CFLAGS += -DENABLE_NXLINK
	endif

	SUFFIX = .nro

	DEFINES += -DHAVE_ALLOCA_H
endif

ifeq ($(PLATFORM),Windows)
	LIBS += -lshlwapi -mconsole
	OBJECTS	+=	$(OBJDIR)/icon.o
	SUFFIX = .exe
endif

ifeq ($(PLATFORM),Unknown)
	# Linux + static binaries = no
	STATIC = 0
	LIBS += -lm
endif

ifeq ($(STATIC),1)
	PKGCONFIG += " --static"
endif

# =============================================================================
# SDL2 Backend
# =============================================================================

ifeq ($(BACKEND_VIDEO),sdl2)
	CFLAGS += `$(PKGCONFIG) --cflags sdl2`
	LIBS += `$(PKGCONFIG) --libs sdl2`
	SOURCES +=	backends/video/video_sdl2
endif

ifeq ($(BACKEND_INPUT),sdl2)
	CFLAGS += `$(PKGCONFIG) --cflags sdl2`
	LIBS += `$(PKGCONFIG) --libs sdl2`
	SOURCES +=	backends/input/input_sdl2
endif

ifeq ($(BACKEND_AUDIO),sdl2mixer)
	CFLAGS += `$(PKGCONFIG) --cflags SDL2_mixer opusfile libmpg123 flac`
	LIBS += `$(PKGCONFIG) --libs SDL2_mixer opusfile libmpg123 flac`
	SOURCES +=	backends/sound/sound_sdl2mixer
endif

# =============================================================================
# Null Backend
# =============================================================================

ifeq ($(BACKEND_VIDEO),null)
	SOURCES +=	backends/video/video_null
endif

ifeq ($(BACKEND_INPUT),null)
	SOURCES +=	backends/input/input_null
endif

ifeq ($(BACKEND_AUDIO),null)
	SOURCES +=	backends/sound/sound_null
endif

# =============================================================================

CFLAGS += `$(PKGCONFIG) --cflags zlib vorbisfile`
LIBS   += `$(PKGCONFIG) --libs zlib vorbisfile`

CFLAGS += -Wno-strict-aliasing -Wno-narrowing -Wno-write-strings

ifeq ($(STATIC),1)
	CFLAGS += -static
endif

LDFLAGS   = $(CFLAGS)

DEFINES   = -DLSB_FIRST -DUSE_16BPP_RENDERING -DUSE_LIBVORBIS \
			-DMAXROMSIZE=33554432 -DHAVE_NO_SPRITE_LIMIT \
			-DM68K_OVERCLOCK_SHIFT=20

INCLUDES  += 	-I$(SRCDIR) \
				-I$(SRCDIR)/core \
				-I$(SRCDIR)/core/z80 \
				-I$(SRCDIR)/core/m68k \
				-I$(SRCDIR)/core/sound \
				-I$(SRCDIR)/core/input_hw \
				-I$(SRCDIR)/core/cart_hw \
				-I$(SRCDIR)/core/cart_hw/svp \
				-I$(SRCDIR)/core/cd_hw \
				-I$(SRCDIR)/core/ntsc \
				-I$(SRCDIR)/backends/input \
				-I$(SRCDIR)/backends/sound \
				-I$(SRCDIR)/backends/video

INCLUDES += $(LIBS)

# Core Sources
SOURCES	+=	core/z80/z80 \
			core/m68k/m68kcpu \
			core/m68k/s68kcpu \
			core/genesis \
			core/vdp_ctrl \
			core/vdp_render \
			core/system \
			core/io_ctrl \
			core/mem68k \
			core/memz80 \
			core/membnk \
			core/state \
			core/loadrom	\
			core/input_hw/input \
			core/input_hw/gamepad \
			core/input_hw/lightgun \
			core/input_hw/mouse \
			core/input_hw/activator \
			core/input_hw/xe_1ap \
			core/input_hw/teamplayer \
			core/input_hw/paddle \
			core/input_hw/sportspad \
			core/input_hw/terebi_oekaki \
			core/input_hw/graphic_board \
			core/sound/sound \
			core/sound/psg \
			core/sound/opll \
			core/sound/ym2413 \
			core/sound/ym3438 \
			core/sound/ym2612 \
			core/sound/blip_buf \
			core/sound/eq \
			core/cart_hw/sram \
			core/cart_hw/svp/svp \
			core/cart_hw/svp/ssp16 \
			core/cart_hw/ggenie \
			core/cart_hw/areplay \
			core/cart_hw/eeprom_93c \
			core/cart_hw/eeprom_i2c \
			core/cart_hw/eeprom_spi \
			core/cart_hw/md_cart \
			core/cart_hw/sms_cart \
			core/cd_hw/scd \
			core/cd_hw/cdd \
			core/cd_hw/cdc \
			core/cd_hw/gfx \
			core/cd_hw/pcm \
			core/cd_hw/cd_cart \
			core/ntsc/sms_ntsc \
			core/ntsc/md_ntsc

# Main Sources
SOURCES	+=	main \
			config \
			error \
			ioapi \
			unzip \
			fileio \
			gamehacks


OUTDIR = $(BINDIR)/$(PLATFORM)
FULLNAME = $(OUTDIR)/$(NAME)$(SUFFIX)
OBJDIR = objects/$(PLATFORM)

OBJECTS += $(addprefix $(OBJDIR)/, $(addsuffix .o, $(SOURCES)))

ifeq ($(SWITCH), 1)

$(FULLNAME): $(OUTDIR)/$(NAME)
	@echo "Building nro..."
	@elf2nro $< $@ $(NROFLAGS)

$(OUTDIR)/$(NAME): $(OBJDIR) $(OBJECTS)
	@echo -n Linking...
	@$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@
	@echo " Done!"

else

$(FULLNAME): $(OBJDIR) $(OBJECTS)
	@echo -n Linking...
	@$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@
	@echo " Done!"

endif

$(shell mkdir -p $(OUTDIR))
$(shell mkdir -p $(OBJDIR))

all: $(FULLNAME)

$(OBJDIR)/%.o: src/%.c
	@mkdir -p $(@D)
	@echo -n Compiling $<...
	@$(CC) -c $(CFLAGS) $(INCLUDES) $(DEFINES) $< -o $@
	@echo " Done!"

#Compile the Windows icon file into an object
$(OBJDIR)/icon.o: src/icon.rc src/md.ico
	@mkdir -p $(@D)
	@windres $< $@

clean:
	rm -rf $(OBJDIR)