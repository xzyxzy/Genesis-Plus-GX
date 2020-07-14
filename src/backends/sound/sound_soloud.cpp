#include "sound_base.h"

#include <cstring>
#include <string>
#include <map>
#include "soloud.h"
#include "soloud_wav.h"
#include "soloud_wavstream.h"
#include "soloud_lofifilter.h"
#include "soloud_biquadresonantfilter.h"

using namespace SoLoud;

#define CHUNK_BUFFER_SIZE 8

Soloud soloud; // object created

WavStream music;      // One wave file
int music_handle;
Bus filter_bus;
float music_speed = 1.0;

int whichchunk = 0;
Wav chunks[CHUNK_BUFFER_SIZE];
Queue chunkqueue;

BiquadResonantFilter underwaterFilter;

std::map<std::string,Wav*> sfx_cache;


int Backend_Sound_Close() {
    soloud.deinit();
    return 1;
}

int Backend_Sound_Init() {
    soloud.init(
        Soloud::CLIP_ROUNDOFF,
        Soloud::SDL2,
        SOUND_FREQUENCY,
        SOUND_SAMPLES_SIZE,
        2
    );

    chunkqueue.setParams(SOUND_FREQUENCY * 2, 1);
    soloud.play(filter_bus);
    underwaterFilter.setParams(SoLoud::BiquadResonantFilter::LOWPASS, 1000, 2);

    return 1;
}

// 0 = filling, 1 = full, 2 = playback started
int bufferstate = 0;

int Backend_Sound_Update(int size) {
    chunks[whichchunk].loadRawWave16(
        soundframe,
        size,
        SOUND_FREQUENCY,
        1
    );

    whichchunk++;
    bufferstate |= whichchunk == CHUNK_BUFFER_SIZE - 1;
    whichchunk %= CHUNK_BUFFER_SIZE;

    chunkqueue.play(chunks[(whichchunk + (CHUNK_BUFFER_SIZE - 1)) % CHUNK_BUFFER_SIZE]);
 
    if (!bufferstate) return 1;    
    if (bufferstate == 2) return 1;
    soloud.play(chunkqueue);
    bufferstate = 2;

    return 1;
}

int Backend_Sound_PlayMusic(char *path) {
    music.stop();

    if (music.load(path) != 0) {
        printf("Unable to load Ogg file: %s\n", path);
        return 0;
    }

    music_handle = filter_bus.play(music, 0.75);
    soloud.setRelativePlaySpeed(music_handle, music_speed);
    return 1;
}

int Backend_Sound_PlaySFX(char *path) {
    std::string pathstr = path;
    Wav *sfx;
    std::map<std::string,Wav*>::iterator it = sfx_cache.find(pathstr);
    if(it != sfx_cache.end()) {
        sfx = it->second;
    } else {
        sfx = new Wav();
        if (sfx->load(path) != 0) {
            printf("Unable to load Wav file: %s\n", path);
            return 0;
        }
        sfx_cache.insert(it,std::pair<std::string,Wav*>(pathstr,sfx));
    }
    filter_bus.play(*sfx);
    return 1;
}

int Backend_Sound_StopMusic() {
    music.stop();
    return 1;
}

int Backend_Sound_FadeOutMusic(int fadeTime) {
    soloud.fadeVolume(music_handle, 0, fadeTime / 1000.0);
    soloud.scheduleStop(music_handle, fadeTime / 1000.0);
    return 1;
}

int Backend_Sound_SetPause(int paused) {
    soloud.setPauseAll(paused);
    return 1;
}

int Backend_Sound_MusicSpeed(float speed) {
    if (music_speed == speed) return 1;
    music_speed = speed;
    soloud.setRelativePlaySpeed(music_handle, music_speed);
    return 1;
}

int Backend_Sound_MusicSetUnderwater(int isUnderwater) {
    if (isUnderwater) filter_bus.setFilter(0, &underwaterFilter);
    else filter_bus.setFilter(0, NULL);
    return 1;
}