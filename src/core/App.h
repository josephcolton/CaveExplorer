#pragma once

#include "core/Assets.h"
#include "core/Audio.h"
#include "core/Input.h"
#include "core/Scene.h"
#include "core/Settings.h"
#include "game/GameData.h"

#include <SDL3/SDL.h>

#include <memory>
#include <string>
#include <vector>

class App {
public:
    static constexpr int kViewWidth = 640;   // logical resolution: 20 x 11.25 tiles
    static constexpr int kViewHeight = 360;
    static constexpr float kTickRate = 60.0f;
    static constexpr const char* kVersion = "0.2.0";

    // Optional debug launch settings (from the command line). Any of startArea/giveItems/
    // openUi(gameplay screens) skip the title screen and start a throwaway game that never saves.
    struct Options {
        std::string startArea;
        std::string startSpawn = "start";
        std::string screenshotPath;  // if set: run screenshotTicks updates, save a BMP, quit
        int screenshotTicks = 60;
        bool debug = false;          // start with debug overlay (F3)
        std::string giveItems;       // comma-separated item ids added to the backpack
        std::string openUi;          // inventory | chest | shop | pause (in game) or slots | settings | credits | about (title)
        std::string script;          // scripted input: "Right:0-120,UseRight:130" (ticks)
        float hunger = -1.0f;        // dev game: starting hunger (0-100)
        bool devGame() const {
            return !startArea.empty() || !giveItems.empty() || openUi == "inventory" || openUi == "chest" ||
                   openUi == "shop" || openUi == "pause";
        }
    };

    bool init(const Options& options);
    void run();
    void shutdown();

    void quit() { running_ = false; }
    void toggleFullscreen();
    // Replaces the current scene at the end of this frame.
    void setScene(std::unique_ptr<Scene> scene) { nextScene_ = std::move(scene); }

    SDL_Renderer* renderer() { return renderer_; }
    Input& input() { return input_; }
    Assets& assets() { return assets_; }
    Audio& audio() { return audio_; }
    Settings& settings() { return settings_; }
    const GameData& data() const { return data_; }
    const Options& options() const { return options_; }

    // Pushes settings (volume, fullscreen) to the systems that use them.
    void applySettings();

    bool debugDraw = false;

private:
    struct ScriptStep {
        Action action;
        int from;
        int to;
    };
    void processEvents();
    void parseScript();
    void applyScript();
    void saveScreenshot(const std::string& path);

    Options options_;
    std::vector<ScriptStep> script_;
    int tickCount_ = 0;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    Input input_;
    Assets assets_;
    Audio audio_;
    Settings settings_;
    GameData data_;
    std::unique_ptr<Scene> scene_;
    std::unique_ptr<Scene> nextScene_;
    bool running_ = false;
    float fps_ = 0.0f;
};
