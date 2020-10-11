#include <sys/time.h>
#include <limits.h>

#if defined(__vita__)
#define PATH_ROM "ux0:rom.bin"
#else
#define PATH_ROM "./rom.bin"
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifdef ENABLE_NXLINK
#ifdef __cplusplus
extern "C" {
#endif

#include "switch/runtime/devices/socket.h"
#include "switch/runtime/nxlink.h"

#ifdef __cplusplus
}
#endif
#endif

#include "shared.h"
#include "sms_ntsc.h"
#include "md_ntsc.h"

#include "backends/sound/sound_base.h"
short soundframe[SOUND_SAMPLES_SIZE];

#include "backends/video/video_base.h"
int mirrormode = 0;

#include "backends/input/input_base.h"

#include "gamehacks.h"

#define STATIC_ASSERT(name, test) typedef struct { int assert_[(test)?1:-1]; } assert_ ## name ## _
#define M68K_MAX_CYCLES 1107
#define Z80_MAX_CYCLES 345
#define OVERCLOCK_FRAME_DELAY 100

#ifdef M68K_OVERCLOCK_SHIFT
#define HAVE_OVERCLOCK
STATIC_ASSERT(m68k_overflow,
              M68K_MAX_CYCLES <= UINT_MAX >> (M68K_OVERCLOCK_SHIFT + 1));
#endif

#ifdef Z80_OVERCLOCK_SHIFT
#ifndef HAVE_OVERCLOCK
#define HAVE_OVERCLOCK
#endif
STATIC_ASSERT(z80_overflow,
              Z80_MAX_CYCLES <= UINT_MAX >> (Z80_OVERCLOCK_SHIFT + 1));
#endif

/* Some games appear to calibrate music playback speed for PAL/NTSC by
   actually counting CPU cycles per frame during startup, resulting in
   hilariously fast music.  Delay overclocking for a while as a
   workaround */
#ifdef HAVE_OVERCLOCK
static uint32 overclock_delay;
static int overclock_enable = 1;
#endif

int log_error   = 0;
int debug_on    = 0;
int turbo_mode  = 0;
int use_sound   = 1;

static uint8 brm_format[0x40] =
{
  0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x00,0x00,0x00,0x00,0x40,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x53,0x45,0x47,0x41,0x5f,0x43,0x44,0x5f,0x52,0x4f,0x4d,0x00,0x01,0x00,0x00,0x00,
  0x52,0x41,0x4d,0x5f,0x43,0x41,0x52,0x54,0x52,0x49,0x44,0x47,0x45,0x5f,0x5f,0x5f
};

/* video */
md_ntsc_t *md_ntsc;
sms_ntsc_t *sms_ntsc;

static const uint16 vc_table[4][2] =
{
  /* NTSC, PAL */
  {0xDA , 0xF2},  /* Mode 4 (192 lines) */
  {0xEA , 0x102}, /* Mode 5 (224 lines) */
  {0xDA , 0xF2},  /* Mode 4 (192 lines) */
  {0x106, 0x10A}  /* Mode 5 (240 lines) */
};

#ifdef HAVE_OVERCLOCK
static void update_overclock(void)
{
#ifdef M68K_OVERCLOCK_SHIFT
    m68k.cycle_ratio = 1 << M68K_OVERCLOCK_SHIFT;
#endif
#ifdef Z80_OVERCLOCK_SHIFT
    z80_cycle_ratio = 1 << Z80_OVERCLOCK_SHIFT;
#endif
    if (overclock_delay == 0)
    {
      /* Cycle ratios multiply per-instruction cycle counts, so use
         reciprocals */
#ifdef M68K_OVERCLOCK_SHIFT
      if ((system_hw & SYSTEM_PBC) == SYSTEM_MD)
        m68k.cycle_ratio = (100 << M68K_OVERCLOCK_SHIFT) / 200;
#endif
#ifdef Z80_OVERCLOCK_SHIFT
      if ((system_hw & SYSTEM_PBC) != SYSTEM_MD)
        z80_cycle_ratio = (100 << Z80_OVERCLOCK_SHIFT) / 200;
#endif
    }
}
#endif

int running = 1;

void mainloop() {
  Backend_Input_MainLoop();
  gamehacks_update();

  #ifdef HAVE_OVERCLOCK
    /* update overclock delay */
    if (gamehacks_overclock_enable && overclock_enable && overclock_delay && --overclock_delay == 0)
        update_overclock();
  #endif
  Backend_Video_Clear();
  gamehacks_render();

  if (system_hw == SYSTEM_MCD) system_frame_scd(0);
  else if ((system_hw & SYSTEM_PBC) == SYSTEM_MD) system_frame_gen(0);
  else system_frame_sms(0);

  Backend_Video_Update();
  Backend_Video_Present();
  int sound_update_size = audio_update(soundframe) * 2;
  if (use_sound) Backend_Sound_Update(sound_update_size);
}

static struct timeval timeval_start;

int main (int argc, char **argv) {
  printf("Emulator start\n");

  gettimeofday(&timeval_start, NULL);

  #ifdef ENABLE_NXLINK
  socketInitializeDefault();
  nxlinkStdio();
  #endif
  
  FILE *fp;

  /* set default config */
  error_init();
  set_config_defaults();

  /* mark all BIOS as unloaded */
  system_bios = 0;

  /* Genesis BOOT ROM support (2KB max) */
  memset(boot_rom, 0xFF, 0x800);
  fp = fopen(MD_BIOS, "rb");
  if (fp != NULL)
  {
    int i;

    /* read BOOT ROM */
    fread(boot_rom, 1, 0x800, fp);
    fclose(fp);

    /* check BOOT ROM */
    if (!memcmp((char *)(boot_rom + 0x120),"GENESIS OS", 10))
    {
      /* mark Genesis BIOS as loaded */
      system_bios = SYSTEM_MD;
    }

    /* Byteswap ROM */
    for (i=0; i<0x800; i+=2)
    {
      uint8 temp = boot_rom[i];
      boot_rom[i] = boot_rom[i+1];
      boot_rom[i+1] = temp;
    }
  }

  /* initialize Genesis virtual system */
  memset(&bitmap, 0, sizeof(t_bitmap));
  bitmap.width        = 400;
  bitmap.height       = 224;
#if defined(USE_8BPP_RENDERING)
  bitmap.pitch        = (bitmap.width * 1);
#elif defined(USE_15BPP_RENDERING)
  bitmap.pitch        = (bitmap.width * 2);
#elif defined(USE_16BPP_RENDERING)
  bitmap.pitch        = (bitmap.width * 2);
#elif defined(USE_32BPP_RENDERING)
  bitmap.pitch        = (bitmap.width * 4);
#endif
  /* initialize backends */
  Backend_Video_Init();
  Backend_Input_Init();
  Backend_Sound_Init();

  bitmap.viewport.changed = 3;

  char * rom_path = argv[1];
  char * diff_path = argv[2];
  if (rom_path == NULL) rom_path = PATH_ROM;

  /* Load game file */
  if(!load_rom(rom_path, diff_path))
  {
    char caption[256];
    sprintf(caption, "Error loading file `%s'.", rom_path);
    #ifdef ENABLE_NXLINK
    socketExit();
    #endif
    return 1;
  }

  char caption[100];
  sprintf(caption,"%s", rominfo.international);
  Backend_Video_SetWindowTitle(caption);

#ifdef HAVE_OVERCLOCK
    overclock_delay = OVERCLOCK_FRAME_DELAY;
#endif

  /* initialize system hardware */
  audio_init(SOUND_FREQUENCY, 0);
  system_init();

  /* Mega CD specific */
  if (system_hw == SYSTEM_MCD)
  {
    /* load internal backup RAM */
    fp = fopen("./scd.brm", "rb");
    if (fp!=NULL)
    {
      fread(scd.bram, 0x2000, 1, fp);
      fclose(fp);
    }

    /* check if internal backup RAM is formatted */
    if (memcmp(scd.bram + 0x2000 - 0x20, brm_format + 0x20, 0x20))
    {
      /* clear internal backup RAM */
      memset(scd.bram, 0x00, 0x200);

      /* Internal Backup RAM size fields */
      brm_format[0x10] = brm_format[0x12] = brm_format[0x14] = brm_format[0x16] = 0x00;
      brm_format[0x11] = brm_format[0x13] = brm_format[0x15] = brm_format[0x17] = (sizeof(scd.bram) / 64) - 3;

      /* format internal backup RAM */
      memcpy(scd.bram + 0x2000 - 0x40, brm_format, 0x40);
    }

    /* load cartridge backup RAM */
    if (scd.cartridge.id)
    {
      fp = fopen("./cart.brm", "rb");
      if (fp!=NULL)
      {
        fread(scd.cartridge.area, scd.cartridge.mask + 1, 1, fp);
        fclose(fp);
      }

      /* check if cartridge backup RAM is formatted */
      if (memcmp(scd.cartridge.area + scd.cartridge.mask + 1 - 0x20, brm_format + 0x20, 0x20))
      {
        /* clear cartridge backup RAM */
        memset(scd.cartridge.area, 0x00, scd.cartridge.mask + 1);

        /* Cartridge Backup RAM size fields */
        brm_format[0x10] = brm_format[0x12] = brm_format[0x14] = brm_format[0x16] = (((scd.cartridge.mask + 1) / 64) - 3) >> 8;
        brm_format[0x11] = brm_format[0x13] = brm_format[0x15] = brm_format[0x17] = (((scd.cartridge.mask + 1) / 64) - 3) & 0xff;

        /* format cartridge backup RAM */
        memcpy(scd.cartridge.area + scd.cartridge.mask + 1 - sizeof(brm_format), brm_format, sizeof(brm_format));
      }
    }
  }

  if (sram.on)
  {
    /* load SRAM */
    fp = fopen("./game.srm", "rb");
    if (fp!=NULL)
    {
      fread(sram.sram,0x10000,1, fp);
      fclose(fp);
    }
  }

  /* reset system hardware */
  system_reset();

  //if (use_sound) Backend_Sound_Pause();

  uint64 framerateMicroseconds = 1000000.0 / 60.0;

  gamehacks_init();

  /* emulation loop */
  #ifdef __EMSCRIPTEN__
   emscripten_set_main_loop(&mainloop, 60, 1);
  #else
    while(running) {
      mainloop();
      
      if (turbo_mode) continue;

      uint32 timePrev;

      struct timeval timeval_now;
      gettimeofday(&timeval_now, NULL);
      uint32 timeNow = (
        (timeval_now.tv_sec-timeval_start.tv_sec)*1000000 + 
        timeval_now.tv_usec-timeval_start.tv_usec
      );

      uint32 timeNext = timeNow + framerateMicroseconds;

      if (timeNow >= timePrev + 100) {
        timePrev = timeNow;
      } else {
        if (timeNow < timeNext) {
          #ifdef USE_NORMAL_SLEEP
            sleep((timeNext - timeNow) / 100000.0);
          #else
            usleep(timeNext - timeNow);
          #endif
        }
          
        timePrev = timePrev + framerateMicroseconds;
      }
    }

    if (system_hw == SYSTEM_MCD)
    {
      /* save internal backup RAM (if formatted) */
      if (!memcmp(scd.bram + 0x2000 - 0x20, brm_format + 0x20, 0x20))
      {
        fp = fopen("./scd.brm", "wb");
        if (fp!=NULL)
        {
          fwrite(scd.bram, 0x2000, 1, fp);
          fclose(fp);
        }
      }

      /* save cartridge backup RAM (if formatted) */
      if (scd.cartridge.id)
      {
        if (!memcmp(scd.cartridge.area + scd.cartridge.mask + 1 - 0x20, brm_format + 0x20, 0x20))
        {
          fp = fopen("./cart.brm", "wb");
          if (fp!=NULL)
          {
            fwrite(scd.cartridge.area, scd.cartridge.mask + 1, 1, fp);
            fclose(fp);
          }
        }
      }
    }

    if (sram.on)
    {
      /* save SRAM */
      fp = fopen("./game.srm", "wb");
      if (fp!=NULL)
      {
        fwrite(sram.sram,0x10000,1, fp);
        fclose(fp);
      }
    }

    gamehacks_deinit();

    audio_shutdown();
    error_shutdown();

    Backend_Input_Close();
    Backend_Sound_Close();
    Backend_Video_Close();

    #ifdef ENABLE_NXLINK
    socketExit();
    #endif
  #endif
  
  return 0;
}