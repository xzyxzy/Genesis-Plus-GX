
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

# Detect default platform if not explicitly specified
ifeq ($(OS),Windows_NT)
	PLATFORM ?= Windows
endif

# Detect explicitly-defined platforms

# Now here's the real platform-specific stuff
ifdef EMSCRIPTEN
	PLATFORM = Emscripten
	PKGCONFIG = :
	CFLAGS += -s USE_SDL=2 -s USE_SDL_MIXER=2 -s USE_ZLIB=1 -s USE_VORBIS=1 -s TOTAL_MEMORY=41484288 -s LLD_REPORT_UNDEFINED -s ALLOW_MEMORY_GROWTH=1 --preload-file "rom.bin"
	DEFINES += -DHAVE_ALLOCA_H
	SUFFIX = js
endif

ifeq ($(SWITCH), 1)
	ifeq ($(strip $(DEVKITPRO)),)
		$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
	endif

	PLATFORM = Switch

	include $(DEVKITPRO)/libnx/switch_rules
	INCLUDES += -I$(LIBNX)/include -I$(PORTLIBS)/include/SDL2 -I$(PORTLIBS)/include

	CFLAGS += -DARM -march=armv8-a -mtune=cortex-a57 -mtp=soft -DLSB_FIRST -DBYTE_ORDER=LITTLE_ENDIAN -DALIGN_LONG -DALIGN_WORD -DM68K_OVERCLOCK_SHIFT=20 -DHAVE_ZLIB -DSWITCH -fPIE -Wl,--allow-multiple-definition -specs=$(DEVKITPRO)/libnx/switch.specs  $(INCLUDE)
	LIBS += -L$(LIBNX)/lib -lnx -lm
	PKGCONFIG = $(DEVKITPRO)/portlibs/switch/bin/aarch64-none-elf-pkg-config

	ifdef ENABLE_NXLINK
		CXXFLAGS += -DENABLE_NXLINK
	endif

	SUFFIX = nro

	DEFINES += -DHAVE_ALLOCA_H
endif

PLATFORM ?= Unknown

ifeq ($(PLATFORM),Windows)
	LIBS += -lshlwapi -mconsole
	OBJECTS	+=	$(OBJDIR)/icon.o
	SUFFIX = exe
endif

CC = $(CXX)
CFLAGS += `$(PKGCONFIG) --static --cflags \
	SDL2_mixer sdl2 zlib flac vorbis vorbisfile opusfile libmpg123`
CFLAGS += -Wno-strict-aliasing -Wno-narrowing -Wno-write-strings -static
LDFLAGS   = $(CFLAGS)

DEFINES   = -DLSB_FIRST -DUSE_16BPP_RENDERING -DUSE_LIBVORBIS \
			-DMAXROMSIZE=33554432 -DHAVE_NO_SPRITE_LIMIT \
			-DM68K_OVERCLOCK_SHIFT=20



SRCDIR    = ./src

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

LIBS	  += `$(PKGCONFIG) --static --libs SDL2_mixer sdl2 zlib flac vorbis vorbisfile opusfile libmpg123`

INCLUDES += $(LIBS)

CHDLIBDIR = $(SRCDIR)/core/cd_hw/libchdr

OBJECTS	+=       $(OBJDIR)/z80.o	

OBJECTS	+=     	$(OBJDIR)/m68kcpu.o \
		$(OBJDIR)/s68kcpu.o

OBJECTS	+=     	$(OBJDIR)/genesis.o	 \
		$(OBJDIR)/vdp_ctrl.o	 \
		$(OBJDIR)/vdp_render.o   \
		$(OBJDIR)/system.o       \
		$(OBJDIR)/io_ctrl.o	 \
		$(OBJDIR)/mem68k.o	 \
		$(OBJDIR)/memz80.o	 \
		$(OBJDIR)/membnk.o	 \
		$(OBJDIR)/state.o        \
		$(OBJDIR)/loadrom.o	

OBJECTS	+=      $(OBJDIR)/input.o	  \
		$(OBJDIR)/gamepad.o	  \
		$(OBJDIR)/lightgun.o	  \
		$(OBJDIR)/mouse.o	  \
		$(OBJDIR)/activator.o	  \
		$(OBJDIR)/xe_1ap.o	  \
		$(OBJDIR)/teamplayer.o    \
		$(OBJDIR)/paddle.o	  \
		$(OBJDIR)/sportspad.o     \
		$(OBJDIR)/terebi_oekaki.o \
		$(OBJDIR)/graphic_board.o

OBJECTS	+=      $(OBJDIR)/sound.o	\
		$(OBJDIR)/psg.o         \
		$(OBJDIR)/opll.o         \
		$(OBJDIR)/ym2413.o      \
		$(OBJDIR)/ym3438.o      \
		$(OBJDIR)/ym2612.o    

OBJECTS	+=	$(OBJDIR)/blip_buf.o 

OBJECTS	+=	$(OBJDIR)/eq.o 

OBJECTS	+=      $(OBJDIR)/sram.o        \
		$(OBJDIR)/svp.o	        \
		$(OBJDIR)/ssp16.o       \
		$(OBJDIR)/ggenie.o      \
		$(OBJDIR)/areplay.o	\
		$(OBJDIR)/eeprom_93c.o  \
		$(OBJDIR)/eeprom_i2c.o  \
		$(OBJDIR)/eeprom_spi.o  \
		$(OBJDIR)/md_cart.o	\
		$(OBJDIR)/sms_cart.o	
		
OBJECTS	+=      $(OBJDIR)/scd.o	\
		$(OBJDIR)/cdd.o	\
		$(OBJDIR)/cdc.o	\
		$(OBJDIR)/gfx.o	\
		$(OBJDIR)/pcm.o	\
		$(OBJDIR)/cd_cart.o

OBJECTS	+=	$(OBJDIR)/sms_ntsc.o	\
		$(OBJDIR)/md_ntsc.o

OBJECTS	+=	$(OBJDIR)/main.o	\
		$(OBJDIR)/config.o	\
		$(OBJDIR)/error.o	\
		$(OBJDIR)/ioapi.o       \
		$(OBJDIR)/unzip.o       \
		$(OBJDIR)/fileio.o	\
		$(OBJDIR)/gamehacks.o \
		$(OBJDIR)/input_sdl2.o \
		$(OBJDIR)/sound_sdl2mixer.o \
		$(OBJDIR)/video_sdl2.o

OUTDIR = $(BINDIR)/$(PLATFORM)
FULLNAME = $(OUTDIR)/$(NAME).$(SUFFIX)
OBJDIR = objects/$(PLATFORM)

ifeq ($(SWITCH), 1)

$(FULLNAME): $(OUTDIR)/$(NAME)
	@echo "Building nro..."
	@elf2nro $< $@ $(NROFLAGS)

$(OUTDIR)/$(NAME): $(OBJDIR) $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@

else
$(FULLNAME): $(OBJDIR) $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@
endif

$(shell mkdir -p $(OUTDIR))
$(shell mkdir -p $(OBJDIR))
all: $(FULLNAME)

$(OBJDIR) :
		@[ -d $@ ] || mkdir -p $@
		
$(OBJDIR)/%.o : $(SRCDIR)/core/%.c $(SRCDIR)/core/%.h
		$(CC) -c $(CFLAGS) $(INCLUDES) $(DEFINES) $< -o $@
	        	        
$(OBJDIR)/%.o :	$(SRCDIR)/core/sound/%.c $(SRCDIR)/core/sound/%.h	        
		$(CC) -c $(CFLAGS) $(INCLUDES) $(DEFINES) $< -o $@

$(OBJDIR)/%.o :	$(SRCDIR)/core/input_hw/%.c $(SRCDIR)/core/input_hw/%.h	        
		$(CC) -c $(CFLAGS) $(INCLUDES) $(DEFINES) $< -o $@

$(OBJDIR)/%.o :	$(SRCDIR)/core/cart_hw/%.c $(SRCDIR)/core/cart_hw/%.h	        
		$(CC) -c $(CFLAGS) $(INCLUDES) $(DEFINES) $< -o $@

$(OBJDIR)/%.o :	$(SRCDIR)/core/cart_hw/svp/%.c      
		$(CC) -c $(CFLAGS) $(INCLUDES) $(DEFINES) $< -o $@

$(OBJDIR)/%.o :	$(SRCDIR)/core/cart_hw/svp/%.c $(SRCDIR)/core/cart_hw/svp/%.h	        
		$(CC) -c $(CFLAGS) $(INCLUDES) $(DEFINES) $< -o $@

$(OBJDIR)/%.o :	$(SRCDIR)/core/cd_hw/%.c $(SRCDIR)/core/cd_hw/%.h	        
		$(CC) -c $(CFLAGS) $(INCLUDES) $(DEFINES) $< -o $@

$(OBJDIR)/%.o :	$(SRCDIR)/core/z80/%.c $(SRCDIR)/core/z80/%.h	        
		$(CC) -c $(CFLAGS) $(INCLUDES) $(DEFINES) $< -o $@

$(OBJDIR)/%.o :	$(SRCDIR)/core/m68k/%.c       
		$(CC) -c $(CFLAGS) $(INCLUDES) $(DEFINES) $< -o $@

$(OBJDIR)/%.o :	$(SRCDIR)/core/ntsc/%.c $(SRCDIR)/core/ntsc/%.h	        
		$(CC) -c $(CFLAGS) $(INCLUDES) $(DEFINES) $< -o $@

$(OBJDIR)/%.o :	$(CHDLIBDIR)/deps/lzma/%.c 	        
		$(CC) -c $(FLAGS) -I$(CHDLIBDIR)/deps/lzma -D_7ZIP_ST $< -o $@

$(OBJDIR)/%.o :	$(SRCDIR)/%.c $(SRCDIR)/%.h	        
		$(CC) -c $(CFLAGS) $(INCLUDES) $(DEFINES) $< -o $@

$(OBJDIR)/%.o :	$(SRCDIR)/backends/video/%.c    
		$(CC) -c $(CFLAGS) $(INCLUDES) $(DEFINES) $< -o $@

$(OBJDIR)/%.o :	$(SRCDIR)/backends/sound/%.c  
		$(CC) -c $(CFLAGS) $(INCLUDES) $(DEFINES) $< -o $@

$(OBJDIR)/%.o :	$(SRCDIR)/backends/input/%.c  
		$(CC) -c $(CFLAGS) $(INCLUDES) $(DEFINES) $< -o $@

$(OBJDIR)/%.o :	$(SRCDIR)/sdl2/%.c $(SRCDIR)/sdl2/%.h	        
		$(CC) -c $(CFLAGS) $(INCLUDES) $(DEFINES) $< -o $@
		

ifeq ($(PLATFORM),Windows)
$(OBJDIR)/icon.o :	
		windres $(SRCDIR)/icon.rc $@
endif

pack:
		strip $(NAME)
		upx -9 $(NAME)	        

clean:
	rm -rf $(OBJDIR)
