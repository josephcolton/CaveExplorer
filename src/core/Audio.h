#pragma once

#include <array>
#include <string>

struct MIX_Mixer;
struct MIX_Track;
struct MIX_Audio;

// Music (and later sound effects) via SDL3_mixer. If audio can't start, the game
// keeps running silently.
class Audio {
public:
    bool init();
    void shutdown();

    // Plays a looping track from assets/music/, crossfading from whatever is playing.
    // Asking for the track that is already playing does nothing; "" fades music out.
    void playMusic(const std::string& file);

    void setMusicVolume(float volume);  // 0..1

private:
    static constexpr int kFadeMs = 1200;

    MIX_Mixer* mixer_ = nullptr;
    // Two music tracks so the old one can fade out while the new one fades in.
    std::array<MIX_Track*, 2> tracks_{};
    std::array<MIX_Audio*, 2> music_{};
    int current_ = 0;
    std::string currentFile_;
    float musicVolume_ = 0.7f;
};
