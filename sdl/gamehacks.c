// Include the main libnx system header, for Switch development
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include "shared.h"

#define mQueue 0xEF14-1
#define mFlags 0xEF02+1
#define Current_Zone 0xFBE0-1
#define Current_Act 0xFBE1-1

Mix_Music *music;

int id_music_start = 0x0B;
int id_music_end = 0x2A;
int id_music_life = 0x25;

int gamehacks_sound_hasspeedshoes() {
    return (work_ram[mFlags] >> 1) & 1;
}

int gamehacks_sound_isunderwater() {
    return (work_ram[mFlags] >> 1) & 1;
}

int gamehacks_sound_ispaused() {
    return (work_ram[mFlags] >> 7) & 1;
}

void gamehacks_init() {
    Mix_Init(MIX_INIT_OGG);
}

// int gamehacks_play_sfx(int id) {
//     if (id == 0xa5) return id;
//     char *path = malloc((80+1)*sizeof(char));
//     sprintf(path, "./gamehacks/sfx/%x.wav", id);
//     Mix_Chunk *sfx = Mix_LoadWAV_RW(SDL_RWFromFile(path, "rb"), 1);
    
//     if (!sfx) {
//         // printf("Unable to load WAV file: %s\n", Mix_GetError());
//         return id;
//     }

//     if (Mix_PlayChannel(-1, sfx, 0) == -1) {
//         // printf("Unable to play WAV file: %s\n", Mix_GetError());
//         return id;
//     }

//     return 0;
// }

int gamehacks_play_music_path(char *path) {
    Mix_HaltMusic();
    Mix_FreeMusic(music);

    music = Mix_LoadMUS(path);
    
    if (music == NULL) {
        // printf("Unable to load Ogg file: %s\n", Mix_GetError());
        return 0;
    }

    if (Mix_PlayMusic(music, 0) == -1) {
        // printf("Unable to play Ogg file: %s\n", Mix_GetError());
        return 0;
    }

    return 3;
}

int gamehacks_music_current = 0;

int gamehacks_play_music(int id) {
    if ((id < id_music_start) || (id > id_music_end))
        return id;

    gamehacks_music_current = id;

    if (gamehacks_sound_hasspeedshoes())
        return 3;

    char *path = malloc((80+1)*sizeof(char));

    int result;
    if (work_ram[Current_Act] == 1) {
        sprintf(path, "./gamehacks/music/%x_2.ogg", id);
        result = gamehacks_play_music_path(path);
        if (result != 0) return result;
    }
    
    sprintf(path, "./gamehacks/music/%x.ogg", id);
    result = gamehacks_play_music_path(path);
    if (result != 0) return result;
    return id;
}

int gamehacks_play_sound(int id) {
    if (id == 0) return;
 
    if (id == 2) {
        Mix_FadeOutMusic(750);
        return;
    }

    // int result_sfx = gamehacks_play_sfx(id);
    // if (result_sfx != id) return result_sfx;

    int result_music = gamehacks_play_music(id);
    return result_music;
}

int gamehacks_sound_hasspeedshoes_prev = 0;
int gamehacks_sound_ispaused_prev = 0;

void gamehacks_update_sound() {
    int hasspeedshoes = gamehacks_sound_hasspeedshoes();

    if (hasspeedshoes && !gamehacks_sound_hasspeedshoes_prev) {
        gamehacks_play_music_path("./gamehacks/music/speedup.ogg");
        // work_ram[mQueue+1] = 3;
    } else if (!hasspeedshoes && gamehacks_sound_hasspeedshoes_prev) {
        work_ram[mQueue+1] = gamehacks_music_current;
    }

    int paused = gamehacks_sound_ispaused();
    if (paused && !gamehacks_sound_ispaused_prev) {
        Mix_Pause(-1);
        Mix_PauseMusic();
    } else if (!paused && gamehacks_sound_ispaused_prev) {
        Mix_Resume(-1);
        Mix_ResumeMusic();
    }

    gamehacks_sound_ispaused_prev = paused;
    gamehacks_sound_hasspeedshoes_prev = hasspeedshoes;

    work_ram[mQueue+1] = gamehacks_play_sound(work_ram[mQueue+1]);
    work_ram[mQueue+2] = gamehacks_play_sound(work_ram[mQueue+2]);
}

void gamehacks_update() {
    gamehacks_update_sound();
}

void gamehacks_deinit() {
}