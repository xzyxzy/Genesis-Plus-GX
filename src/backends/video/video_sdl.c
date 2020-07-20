#include "video_base.h"

#include <SDL/SDL.h>
#include "shared.h"

// #if defined(USE_8BPP_RENDERING)
//   #define SURFACE_FORMAT SDL_PIXELFORMAT_RGB332
// #elif defined(USE_15BPP_RENDERING)
//   #define SURFACE_FORMAT SDL_PIXELFORMAT_RGB555
// #elif defined(USE_16BPP_RENDERING)
//   #define SURFACE_FORMAT SDL_PIXELFORMAT_RGB565
// #elif defined(USE_32BPP_RENDERING)
//   #define SURFACE_FORMAT SDL_PIXELFORMAT_RGB888
// #endif

struct {
  SDL_Surface* surf_screen;
  SDL_Surface* surf_bitmap;
  SDL_Rect srect;
  SDL_Rect drect;
  Uint32 frames_rendered;
  Uint16 screen_width;
  Uint16 screen_height;
  int fullscreen;
} sdl_video;



int SDL_OnResize(void* data, SDL_Event* event) {
  if (event->type == SDL_VIDEORESIZE) {
      bitmap.viewport.changed = 1;
  }
  return 1;
}

int Backend_Video_Init() {
  if(SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("SDL Video initialization failed");
    return 0;
  }

  sdl_video.surf_screen = SDL_SetVideoMode(
    400,
    240,
    0,
    0
  );

    Uint32 rmask, gmask, bmask, amask;

    /* SDL interprets each pixel as a 32-bit number, so our masks must depend
       on the endianness (byte order) of the machine */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    rmask = 0xff000000;
    gmask = 0x00ff0000;
    bmask = 0x0000ff00;
    amask = 0x000000ff;
#else
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = 0xff000000;
#endif

  sdl_video.surf_bitmap = SDL_CreateRGBSurface(
    SDL_SWSURFACE, 720, 576, 16,
    rmask, gmask, bmask, amask
  );
  sdl_video.frames_rendered = 0;
  SDL_ShowCursor(0);

  return 1;
}

int Backend_Video_Update() {
  if (system_hw == SYSTEM_MCD) system_frame_scd(0);
  else if ((system_hw & SYSTEM_PBC) == SYSTEM_MD) system_frame_gen(0);
  else system_frame_sms(0);

  /* viewport size changed */
  if(bitmap.viewport.changed & 1)
  {
    sdl_video.surf_screen  = SDL_GetVideoSurface();

    // #if defined(SWITCH) || defined(MACOS)
    sdl_video.screen_width = 400;
    sdl_video.screen_height = 240;
    // #else
    // sdl_video.screen_width = sdl_video.surf_screen->w;
    // sdl_video.screen_height = sdl_video.surf_screen->h;
    // #endif

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
    //   SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    } else {
    //   SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    }

    sdl_video.drect.w = (bitmap.viewport.w / (float)bitmap.viewport.h) * sdl_video.drect.h;
    if (interlaced) sdl_video.drect.w /= 2;

    // #ifdef SWITCH
    sdl_video.drect.x = 0;
    sdl_video.drect.y = 0;
    // #else
    // sdl_video.drect.x = (sdl_video.screen_width / 2.0) - (sdl_video.drect.w / 2.0);
    // sdl_video.drect.y = (sdl_video.screen_height / 2.0) - (sdl_video.drect.h / 2.0);
    // #endif

    /* clear destination surface */
    SDL_FillRect(sdl_video.surf_screen, 0, 0);
  }

  // SDL_LockSurface(sdl_video.surf_bitmap);

  // Uint32 tex_fmt_id;
  // SDL_QueryTexture(sdl_video.surf_texture, &tex_fmt_id, NULL, NULL, NULL);

  // /* Set up a destination surface for the texture update */
  // SDL_PixelFormat *dst_fmt = SDL_AllocFormat(tex_fmt_id);
  // SDL_Surface *temp = SDL_ConvertSurface(sdl_video.surf_bitmap, dst_fmt, 0);

  // SDL_FreeFormat(dst_fmt);
  // SDL_UpdateTexture(sdl_video.surf_texture, NULL, temp->pixels, temp->pitch);
  // SDL_FreeSurface(temp);

  // if (mirrormode) {
  //   SDL_RenderCopyEx(
  //     sdl_video.renderer,
  //     sdl_video.surf_texture,
  //     &sdl_video.srect,
  //     &sdl_video.drect,
  //     0,
  //     NULL,
  //     SDL_FLIP_HORIZONTAL
  //   );
  // } else {
  //   SDL_RenderCopy(
  //     sdl_video.renderer,
  //     sdl_video.surf_texture,
  //     &sdl_video.srect,
  //     &sdl_video.drect
  //   );
  // }
  // SDL_RenderPresent(sdl_video.renderer);

  // SDL_UnlockSurface(sdl_video.surf_bitmap);
  SDL_BlitSurface(sdl_video.surf_bitmap, &sdl_video.srect, sdl_video.surf_screen, &sdl_video.drect);
  SDL_UpdateRect(sdl_video.surf_screen, 0, 0, 0, 0);

  ++sdl_video.frames_rendered;

  return 1;
}

int Backend_Video_Close() {
  SDL_FreeSurface(sdl_video.surf_bitmap);
  // SDL_DestroyWindow(sdl_video.window);
  SDL_Quit();

  return 1;
}

int Backend_Video_SetFullscreen(int arg_fullscreen) {
  sdl_video.fullscreen = arg_fullscreen;
  // SDL_SetWindowFullscreen(sdl_video.window, sdl_video.fullscreen);

  return 1;
}

int Backend_Video_ToggleFullscreen() {
  Backend_Video_SetFullscreen(!sdl_video.fullscreen);

  return 1;
}

int Backend_Video_CopyBitmap() {
  SDL_LockSurface(sdl_video.surf_bitmap);
  bitmap.data = (unsigned char *)sdl_video.surf_bitmap->pixels;
  SDL_UnlockSurface(sdl_video.surf_bitmap);

  return 1;
}

int Backend_Video_SetWindowTitle(char *caption) {
  SDL_WM_SetCaption(caption, NULL);
  return 1;
}