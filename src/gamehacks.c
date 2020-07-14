#include "shared.h"
#include "backends/sound/sound_base.h"
#include "cpuhook.h"

#define mQueue_0 0xEE32 // music
#define mQueue_1 0xEE32+1 // cmd
#define mQueue_2 0xEE32+3  // sfx ??? why +3

#define mFlags 0xEE20+1
#define mFlags_Mask_Panning     0b00000001
#define mFlags_Mask_SpeedShoes  0b00000010
#define mFlags_Mask_Underwater  0b00000100
#define mFlags_Mask_Paused      0b10000000

#define Current_Zone 0xFADA-1
#define Current_Act 0xFADB-1

#define dPlaySnd 0x12AC4A

#define id_music_start  0x0B
#define id_music_end    0x2A
#define id_music_life   0x25

int gamehacks_play_sfx(int id) {
    if (id == 0) return 0;

    char *path = (char *)malloc((100)*sizeof(char));

    if (work_ram[mFlags] & mFlags_Mask_Panning) {
        sprintf(path, "./gamehacks/sfx/%x_r.wav", id);
    } else {
        sprintf(path, "./gamehacks/sfx/%x_l.wav", id);
    }
    if (Backend_Sound_PlaySFX(path)) return 0;
    
    sprintf(path, "./gamehacks/sfx/%x.wav", id);
    if (Backend_Sound_PlaySFX(path)) return 0;
    
    return id;
}

int gamehacks_play_music_path(char *path) {
    Backend_Sound_StopMusic();
    if (!Backend_Sound_PlayMusic(path)) return 0;
    return 3;
}

int gamehacks_play_music(int id) {
    if (id == 0) return 0;

    char *path = (char *)malloc((80+1)*sizeof(char));

    sprintf(
        path,
        "./gamehacks/music/%x_%i.ogg",
        id,
        work_ram[Current_Act] + 1
    );
    if (Backend_Sound_PlayMusic(path)) return 3;
    
    sprintf(
        path,
        "./gamehacks/music/%x.ogg",
        id
    );
    if (Backend_Sound_PlayMusic(path)) return 3;
    
    return id;
}

int gamehacks_play_command(int id) {
    if (id == 0) return 0;

    if (id == 2) Backend_Sound_FadeOutMusic(750);
    return id;
}

int gamehacks_play_sound(int id) {
    if (id < id_music_start) return gamehacks_play_command(id);
    else if (id <= id_music_end) return gamehacks_play_music(id);
    else return gamehacks_play_sfx(id);
}

void gamehacks_update_sound() {
    Backend_Sound_MusicSpeed(
        (work_ram[mFlags] & mFlags_Mask_SpeedShoes) ?
        1.25 : 1
    );
    Backend_Sound_SetPause(work_ram[mFlags] & mFlags_Mask_Paused);
    Backend_Sound_MusicSetUnderwater(work_ram[mFlags] & mFlags_Mask_Underwater);
}

void gamehacks_update() {
    gamehacks_update_sound();
}

void gamehacks_deinit() { }

void gamehacks_cpuhook(hook_type_t type, int width, unsigned int address, unsigned int value) {
    if (address == dPlaySnd) {
        work_ram[mQueue_0] = gamehacks_play_sound(work_ram[mQueue_0]);
        work_ram[mQueue_1] = gamehacks_play_sound(work_ram[mQueue_1]);
        work_ram[mQueue_2] = gamehacks_play_sound(work_ram[mQueue_2]);
    }
}

void gamehacks_init() {
    set_cpu_hook(&gamehacks_cpuhook);   
}
