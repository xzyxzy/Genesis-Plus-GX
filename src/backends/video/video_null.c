#include "video_base.h"

#include "shared.h"

int Backend_Video_Close() { return 1; }
int Backend_Video_Init() { return 1; }
int Backend_Video_SetFullscreen(int arg_fullscreen) { return 1; }
int Backend_Video_ToggleFullscreen() { return 1; }
int Backend_Video_Update() {
  if (system_hw == SYSTEM_MCD) system_frame_scd(0);
  else if ((system_hw & SYSTEM_PBC) == SYSTEM_MD) system_frame_gen(0);
  else system_frame_sms(0);

  return 1;
}
int Backend_Video_CopyBitmap() { return 1; }
int Backend_Video_SetWindowTitle(char *caption) { return 1; }