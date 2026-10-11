#include "core/Audio.h"

#include "core/Assets.h"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <nlohmann/json.hpp>

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
    for (MIX_Track*& t : sfx_) {
        t = MIX_CreateTrack(mixer_);
        MIX_TagTrack(t, "sfx");
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
    for (auto& [name, sound] : sounds_) {
        for (MIX_Audio* a : sound.variants) MIX_DestroyAudio(a);
    }
    sounds_.clear();
    tracks_ = {};
    sfx_ = {};
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
    MIX_Audio* audio = MIX_LoadAudio(mixer_, assetPath("soundtrack/" + file).c_str(), false);
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

void Audio::loadSounds() {
    if (!mixer_) return;
    auto text = readTextFile(dataPath("sounds.json"));
    if (!text) return;
    const nlohmann::json doc = nlohmann::json::parse(*text, nullptr, false);
    const nlohmann::json list = doc.is_object() ? doc.value("sounds", nlohmann::json::object()) : nlohmann::json::object();
    for (auto& [name, def] : list.items()) {
        Sound sound;
        sound.volume = def.value("volume", 1.0f);
        for (const std::string& file : def.value("files", std::vector<std::string>{})) {
            // Effects are short: decode them up front so they start instantly.
            if (MIX_Audio* a = MIX_LoadAudio(mixer_, assetPath("sounds/" + file).c_str(), true)) {
                sound.variants.push_back(a);
            } else {
                SDL_Log("Could not load sound %s: %s", file.c_str(), SDL_GetError());
            }
        }
        if (!sound.variants.empty()) sounds_[name] = std::move(sound);
    }
    SDL_Log("Loaded %d sound effects", static_cast<int>(sounds_.size()));
}

void Audio::play(const std::string& event, float volume) {
    if (!mixer_) return;
    auto it = sounds_.find(event);
    if (it == sounds_.end()) return;
    Sound& sound = it->second;
    const Uint64 now = SDL_GetTicks();
    if (now - sound.lastPlayed < 40) return;  // the same sound twice in 40 ms just sounds louder
    sound.lastPlayed = now;

    // Use a free track if there is one, otherwise cut off the oldest.
    MIX_Track* track = nullptr;
    for (std::size_t i = 0; i < kSfxTracks && !track; ++i) {
        MIX_Track* t = sfx_[(nextSfx_ + i) % kSfxTracks];
        if (!MIX_TrackPlaying(t)) track = t;
    }
    if (!track) track = sfx_[nextSfx_];
    nextSfx_ = (nextSfx_ + 1) % kSfxTracks;

    const auto pick = std::uniform_int_distribution<std::size_t>(0, sound.variants.size() - 1)(rng_);
    MIX_StopTrack(track, 0);
    MIX_SetTrackAudio(track, sound.variants[pick]);
    MIX_SetTrackGain(track, sound.volume * volume);
    MIX_PlayTrack(track, 0);
}

void Audio::setSoundVolume(float volume) {
    if (mixer_) MIX_SetTagGain(mixer_, "sfx", SDL_clamp(volume, 0.0f, 1.0f));
}
