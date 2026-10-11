#include "core/Audio.h"

#include "core/Assets.h"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

bool Audio::init() {
    if (!MIX_Init()) {
        SDL_Log("Audio disabled: MIX_Init failed: %s", SDL_GetError());
        return false;
    }
    mixer_ = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!mixer_) {
        SDL_Log("Audio disabled: no playback device: %s", SDL_GetError());
        return false;
    }
    for (MIX_Track*& t : tracks_) {
        t = MIX_CreateTrack(mixer_);
        MIX_SetTrackGain(t, musicVolume_);
    }
    return true;
}

void Audio::shutdown() {
    if (mixer_) {
        // Destroying the mixer also destroys its tracks.
        MIX_DestroyMixer(mixer_);
        mixer_ = nullptr;
    }
    for (MIX_Audio*& a : music_) {
        if (a) MIX_DestroyAudio(a);
        a = nullptr;
    }
    tracks_ = {};
    MIX_Quit();
}

void Audio::playMusic(const std::string& file) {
    if (!mixer_ || file == currentFile_) return;
    currentFile_ = file;

    MIX_Track* old = tracks_[static_cast<std::size_t>(current_)];
    MIX_StopTrack(old, MIX_TrackMSToFrames(old, kFadeMs));
    if (file.empty()) return;

    current_ = 1 - current_;
    MIX_Track* track = tracks_[static_cast<std::size_t>(current_)];
    MIX_Audio*& slot = music_[static_cast<std::size_t>(current_)];

    // Stream from disk (predecode = false): music files are large.
    MIX_Audio* audio = MIX_LoadAudio(mixer_, assetPath("music/" + file).c_str(), false);
    if (!audio) {
        SDL_Log("Could not load music %s: %s", file.c_str(), SDL_GetError());
        return;
    }
    MIX_StopTrack(track, 0);
    MIX_SetTrackAudio(track, audio);
    if (slot) MIX_DestroyAudio(slot);
    slot = audio;

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, -1);  // loop forever
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_FADE_IN_MILLISECONDS_NUMBER, kFadeMs);
    if (!MIX_PlayTrack(track, props)) SDL_Log("Could not play %s: %s", file.c_str(), SDL_GetError());
    SDL_DestroyProperties(props);
}

void Audio::setMusicVolume(float volume) {
    musicVolume_ = SDL_clamp(volume, 0.0f, 1.0f);
    for (MIX_Track* t : tracks_) {
        if (t) MIX_SetTrackGain(t, musicVolume_);
    }
}
