
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
DEBUG	 ?= 0

STATIC	  = 1

BACKEND_VIDEO ?= sdl2
BACKEND_INPUT ?= sdl2
BACKEND_AUDIO ?= soloud_sdl2

# =============================================================================
# Detect default platform if not explicitly specified
# =============================================================================

ifeq ($(OS),Windows_NT)
	PLATFORM ?= Windows
else
	UNAME_S := $(shell uname -s)

	ifeq ($(UNAME_S),Linux)
		PLATFORM = Linux
	endif

	ifeq ($(UNAME_S),Darwin)
		PLATFORM = macOS
	endif

endif

ifdef EMSCRIPTEN
	PLATFORM = Emscripten
endif

PLATFORM ?= Unknown

# =============================================================================
# Detect explicitly-defined platforms
# =============================================================================


ifeq ($(PLATFORM),Emscripten)
	PKGCONFIG = :
	CFLAGS += -s USE_SDL=2 -s USE_SDL_MIXER=2 -s USE_ZLIB=1 -s USE_VORBIS=1 -s TOTAL_MEMORY=41484288 -s LLD_REPORT_UNDEFINED -s ALLOW_MEMORY_GROWTH=1 --preload-file "rom.bin"
	DEFINES += -DHAVE_ALLOCA_H
	SUFFIX = .html

	CC = $(CXX)
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

ifeq ($(PLATFORM),3DS)
	include $(DEVKITARM)/3ds_rules
	INCLUDES += -I$(CTRULIB)/include

	CFLAGS += -g -Wall -O2 -mword-relocations \
			-ffunction-sections \
			-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft \
			-fno-rtti -fno-exceptions -std=gnu++11
	DEFINES += -DARM11 -D_3DS
	LIBS	+= -L$(CTRULIB)/lib -lSDL_gfx -lSDL_mixer -lSDL_image -lSDL -lmikmod -lmad -lvorbisidec -logg -lpng -ljpeg -lz -lcitro3d -lctru -lm
	#PKGCONFIG = $(DEVKITPRO)/portlibs/3ds/bin/arm-none-eabi-pkg-config
	PKGCONFIG = :
	CFLAGS +=	-D_GNU_SOURCE=1 -ffunction-sections -fdata-sections -march=armv6k \
				-mtune=mpcore -mfloat-abi=hard -mword-relocations -DARM11 -D_3DS \
				-IC:/msys64/opt/devkitpro/portlibs/3ds/include/SDL \
				-I/opt/devkitpro/libctru/include \
				-IC:/msys64/opt/devkitpro/portlibs/3ds/include \
				-LC:/msys64/opt/devkitpro/portlibs/3ds/lib \
				-L/opt/devkitpro/portlibs/3ds \
				-L/opt/devkitpro/libctru/lib \
				-L/opt/devkitpro/portlibs/3ds \
				-L/opt/devkitpro/libctru/lib \
				-LC:/msys64/opt/devkitpro/portlibs/3ds/lib \
				-specs=3dsx.specs -march=armv6k -mfloat-abi=hard \
				-march=armv6k -mfloat-abi=hard \

	SUFFIX = .3dsx
	DEFINES += -DHAVE_ALLOCA_H
endif

ifeq ($(PLATFORM),Windows)
	LIBS += -lshlwapi -mconsole
	OBJECTS	+=	$(OBJDIR)/icon.o
	SUFFIX = .exe
endif

ifeq ($(PLATFORM),Linux)
	LIBS += -lm -ldl -lpthread -lasound
endif

ifeq ($(PLATFORM),macOS)
	STATIC = 0
	LIBS += -lm -ldl -lpthread
	DEFINES = -DMACOS
endif

ifeq ($(STATIC),1)
	PKGCONFIG +=  --static
endif

ifeq ($(DEBUG),1)
	CFLAGS += -g
	DEFINES += -DDEBUG
endif

# =============================================================================
# SDL Backend
# =============================================================================

ifeq ($(BACKEND_VIDEO),sdl)
	CFLAGS += `$(PKGCONFIG) --cflags sdl`
	LIBS += `$(PKGCONFIG) --libs sdl`
	SOURCES +=	src/backends/video/video_sdl
endif

ifeq ($(BACKEND_INPUT),sdl)
	CFLAGS += `$(PKGCONFIG) --cflags sdl`
	LIBS += `$(PKGCONFIG) --libs sdl`
	SOURCES +=	src/backends/input/input_sdl
endif

ifeq ($(BACKEND_AUDIO),sdlmixer)
	CFLAGS += `$(PKGCONFIG) --cflags SDL_mixer opusfile libmpg123 flac mad`
	LIBS += `$(PKGCONFIG) --libs SDL_mixer opusfile libmpg123 flac mad`
	SOURCES +=	src/backends/sound/sound_sdlmixer
endif

# =============================================================================
# SDL2 Backend
# =============================================================================

ifeq ($(BACKEND_VIDEO),sdl2)
	CFLAGS += `$(PKGCONFIG) --cflags sdl2`
	LIBS += `$(PKGCONFIG) --libs sdl2`
	SOURCES +=	src/backends/video/video_sdl2
endif

ifeq ($(BACKEND_INPUT),sdl2)
	CFLAGS += `$(PKGCONFIG) --cflags sdl2`
	LIBS += `$(PKGCONFIG) --libs sdl2`
	SOURCES +=	src/backends/input/input_sdl2
endif

ifeq ($(BACKEND_AUDIO),sdl2mixer)
	CFLAGS += `$(PKGCONFIG) --cflags SDL2_mixer opusfile libmpg123 flac`
	LIBS += `$(PKGCONFIG) --libs SDL2_mixer opusfile libmpg123 flac`
	SOURCES +=	src/backends/sound/sound_sdl2mixer
endif

# =============================================================================
# Null Backend
# =============================================================================

ifeq ($(BACKEND_VIDEO),null)
	SOURCES +=	src/backends/video/video_null
endif

ifeq ($(BACKEND_INPUT),null)
	SOURCES +=	src/backends/input/input_null
endif

ifeq ($(BACKEND_AUDIO),null)
	SOURCES +=	src/backends/sound/sound_null
endif

# =============================================================================
# SoLoud/SDL2 Backend
# =============================================================================

ifeq ($(BACKEND_AUDIO),soloud_sdl2)
	CFLAGS += `$(PKGCONFIG) --cflags opusfile libmpg123 flac vorbis`
	LIBS += `$(PKGCONFIG) --libs opusfile libmpg123 flac vorbis`

	INCLUDES +=	-I./lib/soloud/src/audiosource/wav \
				-I./lib/soloud/include

	SOURCES +=	src/backends/sound/sound_soloud \
				lib/soloud/src/audiosource/ay/chipplayer \
				lib/soloud/src/audiosource/ay/sndbuffer \
				lib/soloud/src/audiosource/ay/sndchip \
				lib/soloud/src/audiosource/ay/sndrender \
				lib/soloud/src/audiosource/ay/soloud_ay \
				lib/soloud/src/audiosource/monotone/soloud_monotone \
				lib/soloud/src/audiosource/noise/soloud_noise \
				lib/soloud/src/audiosource/openmpt/soloud_openmpt_dll \
				lib/soloud/src/audiosource/openmpt/soloud_openmpt \
				lib/soloud/src/audiosource/sfxr/soloud_sfxr \
				lib/soloud/src/audiosource/speech/darray \
				lib/soloud/src/audiosource/speech/klatt \
				lib/soloud/src/audiosource/speech/resonator \
				lib/soloud/src/audiosource/speech/soloud_speech \
				lib/soloud/src/audiosource/speech/tts \
				lib/soloud/src/audiosource/tedsid/sid \
				lib/soloud/src/audiosource/tedsid/soloud_tedsid \
				lib/soloud/src/audiosource/tedsid/ted \
				lib/soloud/src/audiosource/vic/soloud_vic \
				lib/soloud/src/audiosource/vizsn/soloud_vizsn \
				lib/soloud/src/audiosource/wav/dr_impl \
				lib/soloud/src/audiosource/wav/soloud_wav \
				lib/soloud/src/audiosource/wav/soloud_wavstream \
				lib/soloud/src/audiosource/wav/stb_vorbis \
				lib/soloud/src/core/soloud \
				lib/soloud/src/core/soloud_audiosource \
				lib/soloud/src/core/soloud_bus \
				lib/soloud/src/core/soloud_core_3d \
				lib/soloud/src/core/soloud_core_basicops \
				lib/soloud/src/core/soloud_core_faderops \
				lib/soloud/src/core/soloud_core_filterops \
				lib/soloud/src/core/soloud_core_getters \
				lib/soloud/src/core/soloud_core_setters \
				lib/soloud/src/core/soloud_core_voicegroup \
				lib/soloud/src/core/soloud_core_voiceops \
				lib/soloud/src/core/soloud_fader \
				lib/soloud/src/core/soloud_fft \
				lib/soloud/src/core/soloud_fft_lut \
				lib/soloud/src/core/soloud_file \
				lib/soloud/src/core/soloud_filter \
				lib/soloud/src/core/soloud_misc \
				lib/soloud/src/core/soloud_queue \
				lib/soloud/src/core/soloud_thread \
				lib/soloud/src/c_api/soloud_c \
				lib/soloud/src/filter/soloud_bassboostfilter \
				lib/soloud/src/filter/soloud_biquadresonantfilter \
				lib/soloud/src/filter/soloud_dcremovalfilter \
				lib/soloud/src/filter/soloud_echofilter \
				lib/soloud/src/filter/soloud_eqfilter \
				lib/soloud/src/filter/soloud_fftfilter \
				lib/soloud/src/filter/soloud_flangerfilter \
				lib/soloud/src/filter/soloud_freeverbfilter \
				lib/soloud/src/filter/soloud_lofifilter \
				lib/soloud/src/filter/soloud_robotizefilter \
				lib/soloud/src/filter/soloud_waveshaperfilter 
				#lib/soloud/src/tools/codegen/main \
				#lib/soloud/src/tools/lutgen/main \
				#lib/soloud/src/tools/resamplerlab/main \
				#lib/soloud/src/tools/sanity/sanity
	
	ifeq ($(PLATFORM),macOS)
		INCLUDES += -I./compat/macOS
	endif

	ifeq ($(STATIC),1)
		SOURCES += lib/soloud/src/backend/sdl2_static/soloud_sdl2_static
		DEFINES += -DWITH_SDL2_STATIC
	else
		SOURCES +=	lib/soloud/src/backend/sdl/soloud_sdl2 \
					lib/soloud/src/backend/sdl/soloud_sdl2_dll
		DEFINES += -DWITH_SDL2
	endif
endif

# =============================================================================

CFLAGS += `$(PKGCONFIG) --cflags zlib`
LIBS   += `$(PKGCONFIG) --libs zlib`

CFLAGS += -Wno-strict-aliasing -Wno-narrowing -Wno-write-strings

ifeq ($(STATIC),1)
	CFLAGS += -static
endif

LDFLAGS   = $(CFLAGS)

DEFINES   += -DLSB_FIRST -DUSE_16BPP_RENDERING \
			-DMAXROMSIZE=33554432 -DHAVE_NO_SPRITE_LIMIT \
			-DM68K_OVERCLOCK_SHIFT=20 -DHOOK_CPU

INCLUDES  += 	-I./src \
				-I./src/core \
				-I./src/core/z80 \
				-I./src/core/m68k \
				-I./src/core/sound \
				-I./src/core/input_hw \
				-I./src/core/cart_hw \
				-I./src/core/cart_hw/svp \
				-I./src/core/cd_hw \
				-I./src/core/debug \
				-I./src/core/ntsc \
				-I./src/backends/input \
				-I./src/backends/sound \
				-I./src/backends/video

INCLUDES += $(LIBS)

# Core Sources
SOURCES	+=	src/core/z80/z80 \
			src/core/m68k/m68kcpu \
			src/core/m68k/s68kcpu \
			src/core/genesis \
			src/core/vdp_ctrl \
			src/core/vdp_render \
			src/core/system \
			src/core/io_ctrl \
			src/core/mem68k \
			src/core/memz80 \
			src/core/membnk \
			src/core/state \
			src/core/loadrom	\
			src/core/input_hw/input \
			src/core/input_hw/gamepad \
			src/core/input_hw/lightgun \
			src/core/input_hw/mouse \
			src/core/input_hw/activator \
			src/core/input_hw/xe_1ap \
			src/core/input_hw/teamplayer \
			src/core/input_hw/paddle \
			src/core/input_hw/sportspad \
			src/core/input_hw/terebi_oekaki \
			src/core/input_hw/graphic_board \
			src/core/sound/sound \
			src/core/sound/psg \
			src/core/sound/opll \
			src/core/sound/ym2413 \
			src/core/sound/ym3438 \
			src/core/sound/ym2612 \
			src/core/sound/blip_buf \
			src/core/sound/eq \
			src/core/cart_hw/sram \
			src/core/cart_hw/svp/svp \
			src/core/cart_hw/svp/ssp16 \
			src/core/cart_hw/ggenie \
			src/core/cart_hw/areplay \
			src/core/cart_hw/eeprom_93c \
			src/core/cart_hw/eeprom_i2c \
			src/core/cart_hw/eeprom_spi \
			src/core/cart_hw/md_cart \
			src/core/cart_hw/sms_cart \
			src/core/cd_hw/scd \
			src/core/cd_hw/cdd \
			src/core/cd_hw/cdc \
			src/core/cd_hw/gfx \
			src/core/cd_hw/pcm \
			src/core/cd_hw/cd_cart \
			src/core/debug/cpuhook \
			src/core/ntsc/sms_ntsc \
			src/core/ntsc/md_ntsc

# Main Sources
SOURCES	+=	src/main \
			src/config \
			src/error \
			src/ioapi \
			src/unzip \
			src/fileio \
			src/gamehacks


OUTDIR = $(BINDIR)/$(PLATFORM)
FULLNAME = $(OUTDIR)/$(NAME)$(SUFFIX)
OBJDIR = objects/$(PLATFORM)

OBJECTS += $(addprefix $(OBJDIR)/, $(addsuffix .o, $(SOURCES)))

ifeq ($(PLATFORM), Switch)

$(FULLNAME): $(OUTDIR)/$(NAME)
	@echo "Building nro..."
	@elf2nro $< $@ $(NROFLAGS)

$(OUTDIR)/$(NAME): $(OBJDIR) $(OBJECTS)
	@echo -n Linking...
	@$(CXX) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@
	@echo " Done!"

else ifeq ($(PLATFORM), 3DS)

$(FULLNAME): $(OUTDIR)/$(NAME)
	@echo -n "Building 3dsx..."
	@3dsxtool $< $@
	@echo " Done!"

$(OUTDIR)/$(NAME): $(OBJDIR) $(OBJECTS)
	@echo -n Linking...
	@$(CXX) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@
	@echo " Done!"

else

$(FULLNAME): $(OBJDIR) $(OBJECTS)
	@echo -n Linking...
	$(CXX) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@
	@echo " Done!"

endif

$(shell mkdir -p $(OUTDIR))
$(shell mkdir -p $(OBJDIR))

all: $(FULLNAME)

$(OBJDIR)/%.o: %.c
	@mkdir -p $(@D)
	@echo -n Compiling $<...
	@$(CC) -c $(CFLAGS) $(INCLUDES) $(DEFINES) $< -o $@
	@echo " Done!"

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(@D)
	@echo -n Compiling $<...
	@$(CXX) -c $(CFLAGS) $(INCLUDES) $(DEFINES) $< -o $@
	@echo " Done!"

#Compile the Windows icon file into an object
$(OBJDIR)/icon.o: src/icon.rc src/md.ico
	@mkdir -p $(@D)
	@windres $< $@

clean:
	rm -rf $(OBJDIR)