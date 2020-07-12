#ifndef __BACKEND_SOUND_BASE___
#define __BACKEND_SOUND_BASE___

#define SOUND_FREQUENCY 44100
#define SOUND_SAMPLES_SIZE  2048

// This is in main.c
extern short soundframe[SOUND_SAMPLES_SIZE];

int Backend_Sound_Close();
int Backend_Sound_Init();
int Backend_Sound_Update(int size);
int Backend_Sound_Pause();

#endif