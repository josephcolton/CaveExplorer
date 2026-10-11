#pragma once

#include <SDL3/SDL_stdinc.h>

#include <array>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

struct MIX_Mixer;
struct MIX_Track;
struct MIX_Audio;

// Music and sound effects via SDL3_mixer. If audio can't start, the game keeps
// running silently.
class Audio {
public:
    bool init();
    void shutdown();

    // Plays a looping track from assets/soundtrack/, crossfading from whatever is playing.
    // Asking for the track that is already playing does nothing; "" fades music out.
    void playMusic(const std::string& file);
    void setMusicVolume(float volume);  // 0..1

    // Loads data/sounds.json (event -> files in assets/sounds/).
    void loadSounds();
    // Plays a sound effect by event name ("jump", "coin"...). Unknown names are ignored.
    void play(const std::string& event, float volume = 1.0f);
    void setSoundVolume(float volume);  // 0..1

private:
    static constexpr int kFadeMs = 1200;
    static constexpr std::size_t kSfxTracks = 12;

    struct Sound {
        std::vector<MIX_Audio*> variants;
        float volume = 1.0f;
        Uint64 lastPlayed = 0;  // ticks; stops the same sound stacking up in one frame
    };

    MIX_Mixer* mixer_ = nullptr;
    // Two music tracks so the old one can fade out while the new one fades in.
    std::array<MIX_Track*, 2> tracks_{};
    std::array<MIX_Audio*, 2> music_{};
    int current_ = 0;
    std::string currentFile_;
    float musicVolume_ = 0.7f;

    std::array<MIX_Track*, kSfxTracks> sfx_{};
    std::size_t nextSfx_ = 0;
    std::unordered_map<std::string, Sound> sounds_;
    std::mt19937 rng_{12345};
};
