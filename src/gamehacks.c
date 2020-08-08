#include "shared.h"
#include "backends/sound/sound_base.h"
#include "backends/video/video_base.h"
#include "cpuhook.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

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

#define dPlaySnd 0x123C4A

#define id_music_start  0x0B
#define id_music_end    0x2A
#define id_music_life   0x25

#define Game_Mode 0xF2E0+1
#define GameModeID_SpecialStage 0x10

#define SSTrack_anim 0xDCAA+1
#define SS_Cur_Speed_Factor 0xDCB8-1

int gamehacks_overclock_enable = 1;

int gamehacks_play_sfx(int id) {
    if (id == 0) return 0;

    char *path = (char *)malloc((100)*sizeof(char));

    sprintf(path, "./gamehacks/sfx/%x_%s.wav",
        id,
        work_ram[mFlags] & mFlags_Mask_Panning ? "r" : "l"
    );

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

SDL_Texture *ss_track;
SDL_Texture *ss_bg;
SDL_Rect ss_bg_src;
int ss_track_frame = 0;
char *ss_track_path;

void gamehacks_init() {
    set_cpu_hook(&gamehacks_cpuhook);
    ss_track_path = (char *)malloc(80*sizeof(char));
    ss_bg = (SDL_Texture *)Backend_Video_LoadImage(
        "./gamehacks/specialstage/bg.png"
    );
    ss_bg_src.w = 398;
    ss_bg_src.h = 224;
}

void gamehacks_render_ss_track() {
    sprintf(
        ss_track_path,
        "./gamehacks/specialstage/track/%1i/%04i.png",
        work_ram[SSTrack_anim],
        ss_track_frame + 1
    );
    ss_track = (SDL_Texture *)Backend_Video_LoadImage(ss_track_path);
    if (ss_track == NULL) {
        if (ss_track_frame != 0) {
            ss_track_frame = 0;
            gamehacks_render_ss_track();
        }
        
        return;
    }
    ss_track_frame++;

    SDL_RenderCopy(
        Backend_Video_GetRenderer(),
        ss_track,
        NULL,
        NULL
    );
}

void gamehacks_render_ss_bg() {
    uint16 bg_xscroll      = (*(uint32 *)&vram[hscb]) >> 16;
    bg_xscroll %= 256;
    bg_xscroll = 256 - bg_xscroll;
    bg_xscroll += 184;
    ss_bg_src.x = bg_xscroll;
    uint16 bg_yscroll      = (*(uint32 *)&vsram[0])   >> 16;
    bg_yscroll  &= 0x0FF;
    bg_yscroll %= 256;
    ss_bg_src.y = bg_yscroll;

    SDL_RenderCopy(
        Backend_Video_GetRenderer(),
        ss_bg,
        &ss_bg_src,
        NULL
    );
}

void gamehacks_render() {
    int in_special_stage = work_ram[Game_Mode] == GameModeID_SpecialStage;
    // printf("%x\n", *((long *)(work_ram + SS_Cur_Speed_Factor)));
    render_bg_disable = in_special_stage;
    if (in_special_stage) {
        gamehacks_render_ss_bg();
        gamehacks_render_ss_track();
    }
}