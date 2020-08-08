#ifndef __BACKEND_VIDEO_BASE___
#define __BACKEND_VIDEO_BASE___

#define VIDEO_WIDTH  398
#define VIDEO_HEIGHT 224

#define WINDOW_SCALE 3

extern int mirrormode;

#ifdef __cplusplus
extern "C" {
#endif

int Backend_Video_Close();
int Backend_Video_Init();
int Backend_Video_SetFullscreen(int arg_fullscreen);
int Backend_Video_ToggleFullscreen();
int Backend_Video_Update();
int Backend_Video_Clear();
int Backend_Video_Present();
int Backend_Video_CopyBitmap();
int Backend_Video_SetWindowTitle(char *caption);
void *Backend_Video_LoadImage(char *path);
void *Backend_Video_GetRenderer();

#ifdef __cplusplus
}
#endif

#endif