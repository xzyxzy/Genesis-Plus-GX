#include "video_base.h"

int Backend_Video_Close() { return 1; }
int Backend_Video_Init() { return 1; }
int Backend_Video_SetFullscreen(bool arg_fullscreen) { return 1; }
int Backend_Video_ToggleFullscreen() { return 1; }
int Backend_Video_Update() { return 1; }
int Backend_Video_CopyBitmap() { return 1; }
int Backend_Video_SetWindowTitle(char *caption) { return 1; }