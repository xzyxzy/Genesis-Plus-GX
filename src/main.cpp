#include <sys/time.h>
#include <sys/stat.h>
#include <limits.h>

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
#include "config.h"
#include "inputact.h"

#include "argparse.h"
#include "portable-file-dialogs.h"

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
    if (overclock_delay && --overclock_delay == 0)
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

char *get_valid_filepath_jsonarray(json_t *patharr) {
  if (patharr == NULL) return NULL;
  if (!json_is_array(patharr)) return NULL;

  // Iterate over array
  struct stat statbuffer;
  for (int i = 0; i < json_array_size(patharr); i++) {
      // Ensure patharr[i] is a string
      json_t *element = json_array_get(patharr, i);
      if (!json_is_string(element)) continue;

      // Ensure patharr[i] is a valid path
      const char *path = json_string_value(element);
      int path_exists = stat(path, &statbuffer) == 0;
      // If it is, we've got our file
      if (path_exists) return (char *)path;
  }
  return NULL;
}

char *get_rom_path() {
  // if (argv[1]) return argv[1];

  // Ensure config.rom exists
  json_t *config_rom = json_object_get(config_json, "rom");
  if (config_rom == NULL) return NULL;

  json_t *config_rompaths = json_object_get(config_rom, "paths");
  return get_valid_filepath_jsonarray(config_rompaths);
}

char *get_diff_path() {
  // if (argv[2]) return argv[2];

  // Ensure config.rom exists
  json_t *config_rom = json_object_get(config_json, "rom");
  if (config_rom == NULL) return NULL;

  // Ensure config.rom.paths_patch exists
  json_t *config_patchpaths = json_object_get(config_rom, "paths_patch");
  return get_valid_filepath_jsonarray(config_patchpaths);
}

int main (int argc, const char **argv) {
  char *rom_path = NULL;
  char *diff_path = NULL;
  char *config_path = "./config.json";

  struct argparse_option options[] = {
    OPT_HELP(),
    OPT_STRING('r', "rom", &rom_path, "Path to ROM file"),
    OPT_STRING('p', "patch", &diff_path, "Path to IPS patch file"),
    OPT_STRING('c', "config", &config_path, "Path to config file"),
    OPT_END(),
  };
  struct argparse argparse;
  argparse_init(&argparse, options, 0, 0);
  argparse_describe(
    &argparse,
    "\nGPGX Widescreen",
    ""
  );
  argc = argparse_parse(&argparse, argc, argv);

  gettimeofday(&timeval_start, NULL);

  #ifdef ENABLE_NXLINK
  socketInitializeDefault();
  nxlinkStdio();
  #endif

  /* set default config */
  error_init();
  config_load(config_path);

  printf("%s\n", config_path);
  printf("%s\n", rom_path);
  printf("%s\n", diff_path);

  if (!rom_path) rom_path = get_rom_path();
  if (!diff_path) diff_path = get_diff_path();

  /* Genesis BOOT ROM support (2KB max) */
  memset(boot_rom, 0xFF, 0x800);
  FILE *fp = fopen(MD_BIOS, "rb");
  if (fp != NULL) {
    /* read BOOT ROM */
    fread(boot_rom, 1, 0x800, fp);
    fclose(fp);

    /* check BOOT ROM */
    if (!memcmp((char *)(boot_rom + 0x120),"GENESIS OS", 10))
      system_bios = SYSTEM_MD; /* mark Genesis BIOS as loaded */

    /* Byteswap ROM */
    for (int i = 0; i < 0x800; i += 2) {
      uint8 temp = boot_rom[i];
      boot_rom[i] = boot_rom[i+1];
      boot_rom[i+1] = temp;
    }
  }

  // Ensure config.rom exists
  json_t *config_rom = json_object_get(config_json, "rom");
  if (config_rom == NULL) return 0;

  // Load rom and patch, show warning messages if any issues occur
  json_t *config_warn_patch_missing = json_object_get(config_rom, "warn_patch_missing");
  if (
    (diff_path == NULL) &&
    (config_warn_patch_missing != NULL) &&
    json_boolean_value(config_warn_patch_missing)
  ) {
    pfd::message(
      "Patch Missing", 
      "You are missing the IPS patch for this game. Things will likely not work correctly. Check your config.json if you're looking for where to put the patch file.",
      pfd::choice::ok,
      pfd::icon::warning
    );
  }

  if((rom_path == NULL) || !load_rom(rom_path, diff_path)) {
    char caption[256];
    sprintf(caption, "Error loading file `%s'.", rom_path);
    pfd::message(
      "ROM Missing",
      "You're missing a ROM file. Check your config.json if you're looking for where the ROM should be placed.",
      pfd::choice::ok,
      pfd::icon::error
    );
    #ifdef ENABLE_NXLINK
    socketExit();
    #endif
    return 1;
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
  inputact_init();
  Backend_Sound_Init();

  bitmap.viewport.changed = 3;

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
   emscripten_set_main_loop(&mainloop, 0, 1);
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