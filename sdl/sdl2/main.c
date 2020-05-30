#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_mixer.h>

#include "shared.h"
#include "sms_ntsc.h"
#include "md_ntsc.h"

#include "gamehacks.h"

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

#ifdef _WIN32
#include <windows.h>
#endif

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
static uint32_t overclock_delay;
static int overclock_enable = 1;
#endif

#define SOUND_FREQUENCY 44100
#define SOUND_SAMPLES_SIZE  2048

#define VIDEO_WIDTH  398
#define VIDEO_HEIGHT 224

#define WINDOW_SCALE 3

#ifndef max
    #define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
    #define min(a,b) ((a) < (b) ? (a) : (b))
#endif

int joynum = 0;
uint64 frametimer = 0;

int log_error   = 0;
int debug_on    = 0;
int turbo_mode  = 0;
int use_sound   = 1;
int fullscreen  = 0; /* SDL_WINDOW_FULLSCREEN */

struct {
  SDL_Window* window;
  SDL_Renderer* renderer;
  SDL_Surface* surf_screen;
  SDL_Surface* surf_bitmap;
  SDL_Texture* surf_texture;
  SDL_Rect srect;
  SDL_Rect drect;
  Uint32 frames_rendered;
  Uint16 screen_width;
  Uint16 screen_height;
} sdl_video;

/* sound */

struct {
  char* current_pos;
  char* buffer;
  int current_emulated_samples;
  Mix_Chunk chunk;
  int current_chunk;
} sdl_sound;


static uint8 brm_format[0x40] =
{
  0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x00,0x00,0x00,0x00,0x40,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x53,0x45,0x47,0x41,0x5f,0x43,0x44,0x5f,0x52,0x4f,0x4d,0x00,0x01,0x00,0x00,0x00,
  0x52,0x41,0x4d,0x5f,0x43,0x41,0x52,0x54,0x52,0x49,0x44,0x47,0x45,0x5f,0x5f,0x5f
};


static short soundframe[SOUND_SAMPLES_SIZE];

// static void sdl_sound_callback(void *userdata, Uint8 *stream, int len)
static void sdl_mixer_callback(int channel)
{
  if (channel != 1) return;

  Mix_Chunk *chunk;
  chunk = &sdl_sound.chunk;

  if(sdl_sound.current_emulated_samples < len) {
    memset(chunk->abuf, 0, chunk->alen);
  }
  else {
    memcpy(chunk->abuf, sdl_sound.buffer, chunk->alen);
    /* loop to compensate desync */
    do {
      sdl_sound.current_emulated_samples -= len;
    } while(sdl_sound.current_emulated_samples > 2 * len);
    memcpy(sdl_sound.buffer,
           sdl_sound.current_pos - sdl_sound.current_emulated_samples,
           sdl_sound.current_emulated_samples);
    sdl_sound.current_pos = sdl_sound.buffer + sdl_sound.current_emulated_samples;
  }

  Mix_PlayChannel(1, chunk, 0);
}

static int sdl_sound_init()
{
  int n;
  SDL_AudioSpec as_desired;

  if(SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "SDL Audio initialization failed", sdl_video.window);
    return 0;
  }


  if(Mix_OpenAudio(SOUND_FREQUENCY, AUDIO_S16LSB, 2, SOUND_SAMPLES_SIZE) < 0) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "SDL Audio open failed", sdl_video.window);
    return 0;
  }

  sdl_sound.current_emulated_samples = 0;
  n = SOUND_SAMPLES_SIZE * 2 * sizeof(short) * 20;

  sdl_sound.chunk.allocated = 0;
  sdl_sound.chunk.abuf = (Uint8*)malloc(n / 20);
  sdl_sound.chunk.alen = n / 20;
  sdl_sound.chunk.volume = 128;

  sdl_sound.buffer = (char*)malloc(n);
  if(!sdl_sound.buffer) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Can't allocate audio buffer", sdl_video.window);
    return 0;
  }
  memset(sdl_sound.buffer, 0, n);
  sdl_sound.current_pos = sdl_sound.buffer;

  Mix_ChannelFinished(sdl_mixer_callback);
  Mix_PlayChannel(1, &sdl_sound.chunk, 0);

  return 1;
}

static void sdl_sound_update(int enabled)
{
  int size = audio_update(soundframe) * 2;

  if (enabled)
  {
    int i;
    short *out;

    SDL_LockAudio();
    out = (short*)sdl_sound.current_pos;
    for(i = 0; i < size; i++)
    {
      *out++ = soundframe[i];
    }
    sdl_sound.current_pos = (char*)out;
    sdl_sound.current_emulated_samples += size * sizeof(short);
    SDL_UnlockAudio();
  }
}

static void sdl_sound_close()
{
  SDL_PauseAudio(1);
  Mix_CloseAudio();
  if (sdl_sound.buffer)
    free(sdl_sound.buffer);
}

static int sdl_on_resize(void* data, SDL_Event* event) {
  if (event->type == SDL_WINDOWEVENT &&
      event->window.event == SDL_WINDOWEVENT_RESIZED) {
      bitmap.viewport.changed = 1;
  }
  return 0;
}

/* video */
md_ntsc_t *md_ntsc;
sms_ntsc_t *sms_ntsc;

#if defined(USE_8BPP_RENDERING)
  #define SURFACE_FORMAT SDL_PIXELFORMAT_RGB332
#elif defined(USE_15BPP_RENDERING)
  #define SURFACE_FORMAT SDL_PIXELFORMAT_RGB555
#elif defined(USE_16BPP_RENDERING)
  #define SURFACE_FORMAT SDL_PIXELFORMAT_RGB565
#elif defined(USE_32BPP_RENDERING)
  #define SURFACE_FORMAT SDL_PIXELFORMAT_RGB888
#endif

static int sdl_video_init()
{
  if(SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "SDL Video initialization failed", sdl_video.window);
    return 0;
  }

  uint32 window_flags = fullscreen | SDL_WINDOW_RESIZABLE;

  #ifndef __EMSCRIPTEN__
  window_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
  #endif

  sdl_video.window = SDL_CreateWindow(
    "Loading...",
    SDL_WINDOWPOS_UNDEFINED,
    SDL_WINDOWPOS_UNDEFINED,
    VIDEO_WIDTH * WINDOW_SCALE,
    VIDEO_HEIGHT * WINDOW_SCALE,
    window_flags
  );
  sdl_video.renderer = SDL_CreateRenderer(sdl_video.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  sdl_video.surf_screen  = SDL_GetWindowSurface(sdl_video.window);
  sdl_video.surf_bitmap = SDL_CreateRGBSurfaceWithFormat(0, 720, 576, SDL_BITSPERPIXEL(SURFACE_FORMAT), SURFACE_FORMAT);
  sdl_video.surf_texture = SDL_CreateTextureFromSurface(sdl_video.renderer, sdl_video.surf_bitmap);
  sdl_video.frames_rendered = 0;
  SDL_ShowCursor(0);

  SDL_AddEventWatch(sdl_on_resize, sdl_video.window);
  return 1;
}

static void sdl_video_update()
{
  if (system_hw == SYSTEM_MCD)
  {
    system_frame_scd(0);
  }
  else if ((system_hw & SYSTEM_PBC) == SYSTEM_MD)
  {
    system_frame_gen(0);
  }
  else	
  {
    system_frame_sms(0);
  }

  /* viewport size changed */
  if(bitmap.viewport.changed & 1)
  {
    sdl_video.surf_screen  = SDL_GetWindowSurface(sdl_video.window);

    #ifdef SWITCH
    sdl_video.screen_width = 1280;
    sdl_video.screen_height = 720;
    #else
    sdl_video.screen_width = sdl_video.surf_screen->w;
    sdl_video.screen_height = sdl_video.surf_screen->h;
    #endif

    bitmap.viewport.changed &= ~1;

    /* source bitmap */
    sdl_video.srect.w = bitmap.viewport.w+2*bitmap.viewport.x;
    sdl_video.srect.h = bitmap.viewport.h+2*bitmap.viewport.y;
    if (interlaced) sdl_video.srect.h += bitmap.viewport.h;

    sdl_video.srect.x = 0;
    sdl_video.srect.y = 0;
    if (sdl_video.srect.w > sdl_video.screen_width)
    {
      sdl_video.srect.x = (sdl_video.srect.w - sdl_video.screen_width) / 2;
      sdl_video.srect.w = sdl_video.screen_width;
    }
    if (sdl_video.srect.h > sdl_video.screen_height)
    {
      sdl_video.srect.y = (sdl_video.srect.h - sdl_video.screen_height) / 2;
      sdl_video.srect.h = sdl_video.screen_height;
    }

    sdl_video.drect.h = sdl_video.screen_height;

    if (1) { // Integer scaling
      sdl_video.drect.h = (sdl_video.drect.h / VIDEO_HEIGHT) * (float)VIDEO_HEIGHT;
      SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    } else {
      SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    }

    sdl_video.drect.w = (bitmap.viewport.w / (float)bitmap.viewport.h) * sdl_video.drect.h;
    if (interlaced) sdl_video.drect.w /= 2;

    #ifdef SWITCH
    sdl_video.drect.x = 0;
    sdl_video.drect.y = 0;
    #else
    sdl_video.drect.x = (sdl_video.screen_width / 2.0) - (sdl_video.drect.w / 2.0);
    sdl_video.drect.y = (sdl_video.screen_height / 2.0) - (sdl_video.drect.h / 2.0);
    #endif

    /* clear destination surface */
    SDL_FillRect(sdl_video.surf_screen, 0, 0);
  }

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
  SDL_LockSurface(sdl_video.surf_bitmap);

  Uint32 tex_fmt_id;
  SDL_QueryTexture(sdl_video.surf_texture, &tex_fmt_id, NULL, NULL, NULL);

  /* Set up a destination surface for the texture update */
  SDL_PixelFormat *dst_fmt = SDL_AllocFormat(tex_fmt_id);
  SDL_Surface *temp = SDL_ConvertSurface(sdl_video.surf_bitmap, dst_fmt, 0);

  SDL_FreeFormat(dst_fmt);
  SDL_UpdateTexture(sdl_video.surf_texture, NULL, temp->pixels, temp->pitch);
  SDL_FreeSurface(temp);

  SDL_RenderCopy(sdl_video.renderer, sdl_video.surf_texture, &sdl_video.srect, &sdl_video.drect);
  SDL_RenderPresent(sdl_video.renderer);

  SDL_UnlockSurface(sdl_video.surf_bitmap);

  ++sdl_video.frames_rendered;
}

static void sdl_video_close()
{
  SDL_FreeSurface(sdl_video.surf_bitmap);
  SDL_DestroyWindow(sdl_video.window);
}

static const uint16 vc_table[4][2] =
{
  /* NTSC, PAL */
  {0xDA , 0xF2},  /* Mode 4 (192 lines) */
  {0xEA , 0x102}, /* Mode 5 (224 lines) */
  {0xDA , 0xF2},  /* Mode 4 (192 lines) */
  {0x106, 0x10A}  /* Mode 5 (240 lines) */
};

static int sdl_control_update(SDL_Keycode keystate)
{
    switch (keystate)
    {
      case SDLK_TAB:
      {
        system_reset();
        break;
      }

      case SDLK_F11:
      {
        fullscreen = (fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP );
        SDL_SetWindowFullscreen(sdl_video.window, fullscreen);
        bitmap.viewport.changed = 1;
        break;
      }

      // case SDLK_F4:
      // {
      //   if (!turbo_mode) use_sound ^= 1;
      //   break;
      // }

      // case SDLK_F6:
      // {
      //   if (!use_sound)
      //   {
      //     turbo_mode ^=1;
      //     sdl_sync.ticks = 0;
      //   }
      //   break;
      // }

      case SDLK_F7:
      {
        FILE *f = fopen("game.gp0","rb");
        if (f)
        {
          uint8 buf[STATE_SIZE];
          fread(&buf, STATE_SIZE, 1, f);
          state_load(buf);
          fclose(f);
        }
        break;
      }

      case SDLK_F8:
      {
        FILE *f = fopen("game.gp0","wb");
        if (f)
        {
          uint8 buf[STATE_SIZE];
          int len = state_save(buf);
          fwrite(&buf, len, 1, f);
          fclose(f);
        }
        break;
      }

      case SDLK_F12:
      {
        joynum = (joynum + 1) % MAX_DEVICES;
        while (input.dev[joynum] == NO_DEVICE)
        {
          joynum = (joynum + 1) % MAX_DEVICES;
        }
        break;
      }

      case SDLK_ESCAPE:
      {
        return 0;
      }

      default:
        break;
    }

   return 1;
}

SDL_GameController* controller;

int sdl_input_update(void)
{
  const uint8 *keystate = SDL_GetKeyboardState(NULL);

  /* reset input */
  input.pad[joynum] = 0;

  switch (input.dev[joynum])
  {
    case DEVICE_LIGHTGUN:
    {
      /* get mouse coordinates (absolute values) */
      int x,y;
      int state = SDL_GetMouseState(&x,&y);

      /* X axis */
      input.analog[joynum][0] =  x - (sdl_video.screen_width-bitmap.viewport.w)/2;

      /* Y axis */
      input.analog[joynum][1] =  y - (sdl_video.screen_height-bitmap.viewport.h)/2;

      /* TRIGGER, B, C (Menacer only), START (Menacer & Justifier only) */
      if(state & SDL_BUTTON_LMASK) input.pad[joynum] |= INPUT_A;
      if(state & SDL_BUTTON_RMASK) input.pad[joynum] |= INPUT_B;
      if(state & SDL_BUTTON_MMASK) input.pad[joynum] |= INPUT_C;
      if(keystate[SDL_SCANCODE_F])  input.pad[joynum] |= INPUT_START;
      break;
    }

    case DEVICE_PADDLE:
    {
      /* get mouse (absolute values) */
      int x;
      int state = SDL_GetMouseState(&x, NULL);

      /* Range is [0;256], 128 being middle position */
      input.analog[joynum][0] = x * 256 /sdl_video.screen_width;

      /* Button I -> 0 0 0 0 0 0 0 I*/
      if(state & SDL_BUTTON_LMASK) input.pad[joynum] |= INPUT_B;

      break;
    }

    case DEVICE_SPORTSPAD:
    {
      /* get mouse (relative values) */
      int x,y;
      int state = SDL_GetRelativeMouseState(&x,&y);

      /* Range is [0;256] */
      input.analog[joynum][0] = (unsigned char)(-x & 0xFF);
      input.analog[joynum][1] = (unsigned char)(-y & 0xFF);

      /* Buttons I & II -> 0 0 0 0 0 0 II I*/
      if(state & SDL_BUTTON_LMASK) input.pad[joynum] |= INPUT_B;
      if(state & SDL_BUTTON_RMASK) input.pad[joynum] |= INPUT_C;

      break;
    }

    case DEVICE_MOUSE:
    {
      /* get mouse (relative values) */
      int x,y;
      int state = SDL_GetRelativeMouseState(&x,&y);

      /* Sega Mouse range is [-256;+256] */
      input.analog[joynum][0] = x * 2;
      input.analog[joynum][1] = y * 2;

      /* Vertical movement is upsidedown */
      if (!config.invert_mouse)
        input.analog[joynum][1] = 0 - input.analog[joynum][1];

      /* Start,Left,Right,Middle buttons -> 0 0 0 0 START MIDDLE RIGHT LEFT */
      if(state & SDL_BUTTON_LMASK) input.pad[joynum] |= INPUT_B;
      if(state & SDL_BUTTON_RMASK) input.pad[joynum] |= INPUT_C;
      if(state & SDL_BUTTON_MMASK) input.pad[joynum] |= INPUT_A;
      if(keystate[SDL_SCANCODE_F])  input.pad[joynum] |= INPUT_START;

      break;
    }

    case DEVICE_XE_1AP:
    {
      /* A,B,C,D,Select,START,E1,E2 buttons -> E1(?) E2(?) START SELECT(?) A B C D */
      if(keystate[SDL_SCANCODE_A])  input.pad[joynum] |= INPUT_START;
      if(keystate[SDL_SCANCODE_S])  input.pad[joynum] |= INPUT_A;
      if(keystate[SDL_SCANCODE_D])  input.pad[joynum] |= INPUT_C;
      if(keystate[SDL_SCANCODE_F])  input.pad[joynum] |= INPUT_Y;
      if(keystate[SDL_SCANCODE_Z])  input.pad[joynum] |= INPUT_B;
      if(keystate[SDL_SCANCODE_X])  input.pad[joynum] |= INPUT_X;
      if(keystate[SDL_SCANCODE_C])  input.pad[joynum] |= INPUT_MODE;
      if(keystate[SDL_SCANCODE_V])  input.pad[joynum] |= INPUT_Z;

      /* Left Analog Stick (bidirectional) */
      if(keystate[SDL_SCANCODE_UP])     input.analog[joynum][1]-=2;
      else if(keystate[SDL_SCANCODE_DOWN])   input.analog[joynum][1]+=2;
      else input.analog[joynum][1] = 128;
      if(keystate[SDL_SCANCODE_LEFT])   input.analog[joynum][0]-=2;
      else if(keystate[SDL_SCANCODE_RIGHT])  input.analog[joynum][0]+=2;
      else input.analog[joynum][0] = 128;

      /* Right Analog Stick (unidirectional) */
      if(keystate[SDL_SCANCODE_KP_8])    input.analog[joynum+1][0]-=2;
      else if(keystate[SDL_SCANCODE_KP_2])   input.analog[joynum+1][0]+=2;
      else if(keystate[SDL_SCANCODE_KP_4])   input.analog[joynum+1][0]-=2;
      else if(keystate[SDL_SCANCODE_KP_6])  input.analog[joynum+1][0]+=2;
      else input.analog[joynum+1][0] = 128;

      /* Limiters */
      if (input.analog[joynum][0] > 0xFF) input.analog[joynum][0] = 0xFF;
      else if (input.analog[joynum][0] < 0) input.analog[joynum][0] = 0;
      if (input.analog[joynum][1] > 0xFF) input.analog[joynum][1] = 0xFF;
      else if (input.analog[joynum][1] < 0) input.analog[joynum][1] = 0;
      if (input.analog[joynum+1][0] > 0xFF) input.analog[joynum+1][0] = 0xFF;
      else if (input.analog[joynum+1][0] < 0) input.analog[joynum+1][0] = 0;
      if (input.analog[joynum+1][1] > 0xFF) input.analog[joynum+1][1] = 0xFF;
      else if (input.analog[joynum+1][1] < 0) input.analog[joynum+1][1] = 0;

      break;
    }

    case DEVICE_PICO:
    {
      /* get mouse (absolute values) */
      int x,y;
      int state = SDL_GetMouseState(&x,&y);

      /* Calculate X Y axis values */
      input.analog[0][0] = 0x3c  + (x * (0x17c-0x03c+1)) / sdl_video.screen_width;
      input.analog[0][1] = 0x1fc + (y * (0x2f7-0x1fc+1)) / sdl_video.screen_height;

      /* Map mouse buttons to player #1 inputs */
      if(state & SDL_BUTTON_MMASK) pico_current = (pico_current + 1) & 7;
      if(state & SDL_BUTTON_RMASK) input.pad[0] |= INPUT_PICO_RED;
      if(state & SDL_BUTTON_LMASK) input.pad[0] |= INPUT_PICO_PEN;

      break;
    }

    case DEVICE_TEREBI:
    {
      /* get mouse (absolute values) */
      int x,y;
      int state = SDL_GetMouseState(&x,&y);

      /* Calculate X Y axis values */
      input.analog[0][0] = (x * 250) / sdl_video.screen_width;
      input.analog[0][1] = (y * 250) / sdl_video.screen_height;

      /* Map mouse buttons to player #1 inputs */
      if(state & SDL_BUTTON_RMASK) input.pad[0] |= INPUT_B;

      break;
    }

    case DEVICE_GRAPHIC_BOARD:
    {
      /* get mouse (absolute values) */
      int x,y;
      int state = SDL_GetMouseState(&x,&y);

      /* Calculate X Y axis values */
      input.analog[0][0] = (x * 255) / 1280;
      input.analog[0][1] = (y * 255) / 720;

      /* Map mouse buttons to player #1 inputs */
      if(state & SDL_BUTTON_LMASK) input.pad[0] |= INPUT_GRAPHIC_PEN;
      if(state & SDL_BUTTON_RMASK) input.pad[0] |= INPUT_GRAPHIC_MENU;
      if(state & SDL_BUTTON_MMASK) input.pad[0] |= INPUT_GRAPHIC_DO;

      break;
    }

    case DEVICE_ACTIVATOR:
    {
      if(keystate[SDL_SCANCODE_G])  input.pad[joynum] |= INPUT_ACTIVATOR_7L;
      if(keystate[SDL_SCANCODE_H])  input.pad[joynum] |= INPUT_ACTIVATOR_7U;
      if(keystate[SDL_SCANCODE_J])  input.pad[joynum] |= INPUT_ACTIVATOR_8L;
      if(keystate[SDL_SCANCODE_K])  input.pad[joynum] |= INPUT_ACTIVATOR_8U;
    }

    default:
    {
      if(keystate[SDL_SCANCODE_A])  input.pad[joynum] |= INPUT_A;
      if(keystate[SDL_SCANCODE_S])  input.pad[joynum] |= INPUT_B;
      if(keystate[SDL_SCANCODE_D])  input.pad[joynum] |= INPUT_C;
      if(keystate[SDL_SCANCODE_F])  input.pad[joynum] |= INPUT_START;
      if(keystate[SDL_SCANCODE_Z])  input.pad[joynum] |= INPUT_X;
      if(keystate[SDL_SCANCODE_X])  input.pad[joynum] |= INPUT_Y;
      if(keystate[SDL_SCANCODE_C])  input.pad[joynum] |= INPUT_Z;
      if(keystate[SDL_SCANCODE_V])  input.pad[joynum] |= INPUT_MODE;

      if(keystate[SDL_SCANCODE_UP]) input.pad[joynum] |= INPUT_UP;
      else
      if(keystate[SDL_SCANCODE_DOWN]) input.pad[joynum] |= INPUT_DOWN;
      if(keystate[SDL_SCANCODE_LEFT]) input.pad[joynum] |= INPUT_LEFT;
      else
      if(keystate[SDL_SCANCODE_RIGHT]) input.pad[joynum] |= INPUT_RIGHT;

      if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A))
        input.pad[joynum] |= INPUT_A;

      if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B))
        input.pad[joynum] |= INPUT_B;

      if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_X))
        input.pad[joynum] |= INPUT_C;

      if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START))
        input.pad[joynum] |= INPUT_START;

      if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP))
        input.pad[joynum] |= INPUT_UP;

      if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
        input.pad[joynum] |= INPUT_DOWN;

      if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT))
        input.pad[joynum] |= INPUT_LEFT;

      if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
        input.pad[joynum] |= INPUT_RIGHT;

      int axis_lx = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
      int axis_ly = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);

      double deadzone_l = 10000;
      double intensity_l = sqrt(pow(axis_lx, 2) + pow(axis_ly, 2));
      double angle_l = atan2((double)axis_ly, (double)axis_lx) + M_PI;

      // printf("%i, %i, %f, %f\n", axis_lx, axis_ly, intensity_l, angle_l);
      double angle_overlap_x = M_PI_4 / 8.0;
      double angle_overlap_y = M_PI_4 / 8.0;

      double deadzone_lx_analog_upper = 10000;
      double intensity_lx_adj = min(1, max(0,
        (abs(axis_lx) - deadzone_l) /
        (32767.0 - deadzone_lx_analog_upper - deadzone_l)
      ));

      intensity_lx_adj = ((intensity_lx_adj - 0) * (1 - 0.9)) / ((1 - 0) + 0.9);
      int allow_horiz = 1;
      if (intensity_lx_adj < 1.0) {
        double frametimer_period = 3.0;
        allow_horiz = (
          (int)(frametimer / frametimer_period) %
          (
            (int)(60.0 / frametimer_period) -
            (int)(intensity_lx_adj * (60.0 / frametimer_period))
          )
        ) == 0;
      }

      if (intensity_l > deadzone_l) {
        if (
          (
            (angle_l < M_PI_4 + angle_overlap_x) ||
            (angle_l > ((2*M_PI) - M_PI_4 - angle_overlap_x))
          ) && (allow_horiz)
        ) input.pad[joynum] |= INPUT_LEFT;

        if (
          (
            (angle_l < (M_PI + M_PI_4 + angle_overlap_x)) &&
            (angle_l > (M_PI - M_PI_4 - angle_overlap_x))
          ) && (allow_horiz)
        ) input.pad[joynum] |= INPUT_RIGHT;

        if (
          (angle_l < (M_PI_2 + M_PI_4 + angle_overlap_y)) &&
          (angle_l > (M_PI_2 - M_PI_4 - angle_overlap_y))
        ) input.pad[joynum] |= INPUT_UP;

        if (
          (angle_l < (M_PI + M_PI_2 + M_PI_4 + angle_overlap_y)) &&
          (angle_l > (M_PI + M_PI_2 - M_PI_4 - angle_overlap_y))
        ) input.pad[joynum] |= INPUT_DOWN;

      }

      break;
    }
  }
  return 1;
}

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
  SDL_Event event;
  if (SDL_PollEvent(&event))
  {
    switch(event.type)
    {
      case SDL_QUIT:
      {
        running = 0;
        break;
      }

      case SDL_KEYDOWN:
      {
        running = sdl_control_update(event.key.keysym.sym);
        break;
      }
    }
  }

  gamehacks_update();

  #ifdef HAVE_OVERCLOCK
    /* update overclock delay */
    if (overclock_enable && overclock_delay && --overclock_delay == 0)
        update_overclock();
  #endif

  sdl_video_update();
  sdl_sound_update(use_sound);

  frametimer++;
}

int main (int argc, char **argv)
{
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

  /* initialize SDL */
  if(SDL_Init(0) < 0)
  {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "SDL initialization failed", sdl_video.window);
    return 1;
  }
  sdl_video_init();
  if (use_sound) sdl_sound_init();

  SDL_InitSubSystem(SDL_INIT_JOYSTICK);
  SDL_JoystickEventState(SDL_ENABLE);
  SDL_JoystickOpen(0);

  /* initialize Genesis virtual system */
  SDL_LockSurface(sdl_video.surf_bitmap);
  memset(&bitmap, 0, sizeof(t_bitmap));
  bitmap.width        = 720;
  bitmap.height       = 576;
#if defined(USE_8BPP_RENDERING)
  bitmap.pitch        = (bitmap.width * 1);
#elif defined(USE_15BPP_RENDERING)
  bitmap.pitch        = (bitmap.width * 2);
#elif defined(USE_16BPP_RENDERING)
  bitmap.pitch        = (bitmap.width * 2);
#elif defined(USE_32BPP_RENDERING)
  bitmap.pitch        = (bitmap.width * 4);
#endif
  bitmap.data         = (unsigned char *)sdl_video.surf_bitmap->pixels;
  SDL_UnlockSurface(sdl_video.surf_bitmap);
  bitmap.viewport.changed = 3;

  char * rom_path = argv[1];
  if (rom_path == NULL) rom_path = "./rom.bin";

  /* Load game file */
  if(!load_rom(rom_path))
  {
    char caption[256];
    sprintf(caption, "Error loading file `%s'.", rom_path);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", caption, sdl_video.window);
    #ifdef ENABLE_NXLINK
    socketExit();
    #endif
    return 1;
  }

  char caption[100];
  sprintf(caption,"%s", rominfo.international);
  SDL_SetWindowTitle(sdl_video.window, caption);

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

  if(use_sound) SDL_PauseAudio(0);

  long double framerateMilliseconds = 1000.0 / 60.0;
  unsigned int vsyncMultiple;

  controller = SDL_GameControllerOpen(0);

  gamehacks_init();

  /* emulation loop */
  #ifdef __EMSCRIPTEN__
   emscripten_set_main_loop(&mainloop, 60, 1);
  #else
    while(running) {
      mainloop();
      
      static long double timePrev;
      const uint32_t timeNow = SDL_GetTicks();
      const long double timeNext = timePrev + framerateMilliseconds;

      if (timeNow >= timePrev + 100)
      {
        timePrev = (long double)timeNow;
      }
      else
      {
        if (timeNow < timeNext)
          SDL_Delay(timeNext - timeNow);
        timePrev += framerateMilliseconds;
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

    sdl_video_close();
    sdl_sound_close();
    SDL_Quit();

    #ifdef ENABLE_NXLINK
    socketExit();
    #endif
  #endif
  
  return 0;
}

#ifdef _WIN32
int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char* lpCmdLine, int nShowCmd)
{
    SetProcessDPIAware();
    return main(__argc, __argv);
}
#endif
