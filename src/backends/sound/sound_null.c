#include "sound_base.h"

int Backend_Sound_Close() { return 1; }
int Backend_Sound_Init() { return 1; }
int Backend_Sound_Update(int size) { return 1; }
int Backend_Sound_Pause() { return 1; }

int Backend_Sound_PlayMusic(char *path) { return 1; }
int Backend_Sound_StopMusic() { return 1; }
int Backend_Sound_FadeOutMusic(int fadeTime) { return 1; }
int Backend_Sound_PauseMusic() { return 1; }
int Backend_Sound_ResumeMusic() { return 1; }