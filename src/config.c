
#include "shared.h"
#include "jansson.h"

t_config config_legacy;
json_t config_json;


void set_config_defaults(void)
{
  /* sound options */
  config_legacy.psg_preamp     = 150;
  config_legacy.fm_preamp      = 100;
  config_legacy.hq_fm          = 1;
  config_legacy.hq_psg         = 1;
  config_legacy.filter         = 0;
  config_legacy.low_freq       = 200;
  config_legacy.high_freq      = 8000;
  config_legacy.lg             = 100;
  config_legacy.mg             = 100;
  config_legacy.hg             = 100;
  config_legacy.lp_range       = 0x7fff; /* 0.6 in 0.16 fixed point */
  config_legacy.ym2612         = YM2612_DISCRETE;
  config_legacy.ym2413         = 2; /* = AUTO (0 = always OFF, 1 = always ON) */
#ifdef HAVE_YM3438_CORE
   config_legacy.ym3438         = 0;
#endif
#ifdef HAVE_OPLL_CORE
   config_legacy.opll           = 0;
#endif
  config_legacy.mono           = 0;

  /* system options */
  config_legacy.system         = 0; /* = AUTO (or SYSTEM_SG, SYSTEM_MARKIII, SYSTEM_SMS, SYSTEM_SMS2, SYSTEM_GG, SYSTEM_MD) */
  config_legacy.region_detect  = 0; /* = AUTO (1 = USA, 2 = EUROPE, 3 = JAPAN/NTSC, 4 = JAPAN/PAL) */
  config_legacy.vdp_mode       = 0; /* = AUTO (1 = NTSC, 2 = PAL) */
  config_legacy.master_clock   = 0; /* = AUTO (1 = NTSC, 2 = PAL) */
  config_legacy.force_dtack    = 0;
  config_legacy.addr_error     = 1;
  config_legacy.bios           = 0;
  config_legacy.lock_on        = 0; /* = OFF (can be TYPE_SK, TYPE_GG & TYPE_AR) */
  config_legacy.ntsc           = 0;
  config_legacy.lcd            = 0; /* 0.8 fixed point */
#ifdef HAVE_OVERCLOCK
   config_legacy.overclock      = 100;
#endif
   config_legacy.no_sprite_limit = 1;

  /* display options */
  config_legacy.overscan = 0;       /* 3 = all borders (0 = no borders , 1 = vertical borders only, 2 = horizontal borders only) */
  config_legacy.gg_extra = 0;       /* 1 = show extended Game Gear screen (256x192) */
  config_legacy.render   = 1;       /* 1 = double resolution output (only when interlaced mode 2 is enabled) */

  /* controllers options */
  input.system[0]       = SYSTEM_GAMEPAD;
  input.system[1]       = SYSTEM_GAMEPAD;
  config_legacy.gun_cursor[0]  = 1;
  config_legacy.gun_cursor[1]  = 1;
  config_legacy.invert_mouse   = 0;
  for (int i=0;i<MAX_INPUTS;i++)
  {
    /* autodetected control pad type */
    config_legacy.input[i].padtype = DEVICE_PAD2B | DEVICE_PAD3B | DEVICE_PAD6B;
  }
}
