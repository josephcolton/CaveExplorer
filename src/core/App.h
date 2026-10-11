#pragma once

#include "core/Assets.h"
#include "core/Audio.h"
#include "core/Input.h"
#include "core/Scene.h"

#include <SDL3/SDL.h>

#include <memory>
#include <string>

class App {
public:
    static constexpr int kViewWidth = 640;   // logical resolution: 20 x 11.25 tiles
    static constexpr int kViewHeight = 360;
    static constexpr float kTickRate = 60.0f;

    // Optional debug launch settings (from the command line).
    struct Options {
        std::string startArea = "beach";
        std::string startSpawn = "start";
        std::string screenshotPath;  // if set: run screenshotFrames frames, save a BMP, quit
        int screenshotFrames = 60;
        bool holdRight = false;      // simulate holding Right (for automated movement checks)
        bool debug = false;          // start with debug overlay (F3)
        std::string giveItems;       // comma-separated item ids added to the backpack
        std::string openUi;          // open a screen at start: inventory | chest | shop
    };

    bool init(const Options& options);
    void run();
    void shutdown();

    void quit() { running_ = false; }
    void toggleFullscreen();

    SDL_Renderer* renderer() { return renderer_; }
    Input& input() { return input_; }
    Assets& assets() { return assets_; }
    Audio& audio() { return audio_; }

    const Options& options() const { return options_; }
    bool debugDraw = false;

private:
    void processEvents();
    void saveScreenshot(const std::string& path);

    Options options_;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    Input input_;
    Assets assets_;
    Audio audio_;
    std::unique_ptr<Scene> scene_;
    bool running_ = false;
    bool fullscreen_ = false;
    float fps_ = 0.0f;
};
