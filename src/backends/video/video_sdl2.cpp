#include "video_base.h"

#include <cstring>
#include <string>
#include <map>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "shared.h"

#if defined(USE_8BPP_RENDERING)
  #define SURFACE_FORMAT SDL_PIXELFORMAT_RGB332
#elif defined(USE_15BPP_RENDERING)
  #define SURFACE_FORMAT SDL_PIXELFORMAT_RGB555
#elif defined(USE_16BPP_RENDERING)
  #define SURFACE_FORMAT SDL_PIXELFORMAT_RGB565
#elif defined(USE_32BPP_RENDERING)
  #define SURFACE_FORMAT SDL_PIXELFORMAT_RGB888
#endif

std::map<std::string,SDL_Texture*> tex_cache;

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
  int fullscreen;
} sdl_video;

int SDL_OnResize(void* data, SDL_Event* event) {
  if (event->type == SDL_WINDOWEVENT &&
      event->window.event == SDL_WINDOWEVENT_RESIZED) {
      bitmap.viewport.changed = 1;
  }
  return 1;
}

int Backend_Video_Init() {
  if(SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "SDL Video initialization failed", sdl_video.window);
    return 0;
  }

  IMG_Init(IMG_INIT_PNG);

  uint32 window_flags = (
      (sdl_video.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) |
      SDL_WINDOW_RESIZABLE
  );

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
  SDL_SetTextureBlendMode(sdl_video.surf_texture, SDL_BLENDMODE_BLEND);
  sdl_video.frames_rendered = 0;
  SDL_ShowCursor(0);

  SDL_AddEventWatch(SDL_OnResize, sdl_video.window);

  SDL_LockSurface(sdl_video.surf_bitmap);
  bitmap.data = (unsigned char *)sdl_video.surf_bitmap->pixels;
  SDL_UnlockSurface(sdl_video.surf_bitmap);

  return 1;
}

int Backend_Video_Clear() {
  // TODO: not do this every frame
  SDL_SetRenderDrawColor(sdl_video.renderer, 0, 0, 0, 255);
  SDL_RenderClear(sdl_video.renderer);
  return 1;
}

int Backend_Video_Update() {
  if (system_hw == SYSTEM_MCD) system_frame_scd(0);
  else if ((system_hw & SYSTEM_PBC) == SYSTEM_MD) system_frame_gen(0);
  else system_frame_sms(0);

  /* viewport size changed */
  if(bitmap.viewport.changed & 1)
  {
    sdl_video.surf_screen  = SDL_GetWindowSurface(sdl_video.window);

    #if defined(SWITCH) || defined(MACOS)
    sdl_video.screen_width = 1280;
    sdl_video.screen_height = 720;
    #elif defined(__vita__)
    sdl_video.screen_width = 960;
    sdl_video.screen_height = 544;
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
      sdl_video.drect.w = (bitmap.viewport.w / (float)bitmap.viewport.h) * sdl_video.drect.h;
      SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    } else {
      SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    }

    if (interlaced) sdl_video.drect.w /= 2;

    #if defined(SWITCH)
    sdl_video.drect.x = 0;
    sdl_video.drect.y = 0;
    #else
    sdl_video.drect.x = (sdl_video.screen_width / 2.0) - (sdl_video.drect.w / 2.0);
    sdl_video.drect.y = (sdl_video.screen_height / 2.0) - (sdl_video.drect.h / 2.0);
    #endif

    /* clear destination surface */
    SDL_FillRect(sdl_video.surf_screen, 0, 0);
  }


  Uint32 tex_fmt_id;
  SDL_QueryTexture(sdl_video.surf_texture, &tex_fmt_id, NULL, NULL, NULL);

  /* Set up a destination surface for the texture update */
  SDL_PixelFormat *dst_fmt = SDL_AllocFormat(tex_fmt_id);
  SDL_LockSurface(sdl_video.surf_bitmap);
  SDL_Surface *temp = SDL_ConvertSurface(sdl_video.surf_bitmap, dst_fmt, 0);
  SDL_UnlockSurface(sdl_video.surf_bitmap);

  Uint32 key_color = SDL_MapRGB(
    temp->format,
    0xFF, 0x00, 0xFF
  );

  for (int i = 0; i < temp->w * temp->h; i++) {
    if (((Uint32*)temp->pixels)[i] != key_color) continue;
    ((Uint32*)temp->pixels)[i] &= 0x00FFFFFF; 
  }

  SDL_FreeFormat(dst_fmt);
  SDL_UpdateTexture(sdl_video.surf_texture, NULL, temp->pixels, temp->pitch);
  SDL_FreeSurface(temp);

  if (mirrormode) {
    SDL_RenderCopyEx(
      sdl_video.renderer,
      sdl_video.surf_texture,
      &sdl_video.srect,
      &sdl_video.drect,
      0,
      NULL,
      SDL_FLIP_HORIZONTAL
    );
  } else {
    SDL_RenderCopy(
      sdl_video.renderer,
      sdl_video.surf_texture,
      &sdl_video.srect,
      &sdl_video.drect
    );
  }

  ++sdl_video.frames_rendered;
  return 1;
}

int Backend_Video_Present() {
  SDL_RenderPresent(sdl_video.renderer);
  return 1;
}

int Backend_Video_Close() {
  SDL_FreeSurface(sdl_video.surf_bitmap);
  SDL_DestroyWindow(sdl_video.window);
  SDL_Quit();

  return 1;
}

int Backend_Video_SetFullscreen(int arg_fullscreen) {
  sdl_video.fullscreen = arg_fullscreen;
  SDL_SetWindowFullscreen(sdl_video.window, sdl_video.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);

  return 1;
}

int Backend_Video_ToggleFullscreen() {
  Backend_Video_SetFullscreen(!sdl_video.fullscreen);

  return 1;
}

int Backend_Video_SetWindowTitle(char *caption) {
  SDL_SetWindowTitle(sdl_video.window, caption);
  return 1;
}

void *Backend_Video_LoadImage(char *path) {
    std::string pathstr = path;
    SDL_Texture *tex;
    std::map<std::string,SDL_Texture*>::iterator it = tex_cache.find(pathstr);
    if(it != tex_cache.end()) {
        tex = it->second;
    } else {
      SDL_Surface* tmp_surface = IMG_Load(path);
      if (!tmp_surface) return NULL;
      tex = SDL_CreateTextureFromSurface(sdl_video.renderer, tmp_surface);
      SDL_FreeSurface(tmp_surface);
      tex_cache.insert(it,std::pair<std::string,SDL_Texture*>(pathstr,tex));
    }
    return (void *)tex;
}

void *Backend_Video_GetRenderer() {
  return (void *)sdl_video.renderer;
}