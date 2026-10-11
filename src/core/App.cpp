#include "core/App.h"

#include "game/GameplayScene.h"
#include "game/TitleScene.h"

#include <algorithm>
#include <sstream>

bool App::init(const Options& options) {
    options_ = options;
    debugDraw = options.debug;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    window_ = SDL_CreateWindow("Cave Explorer", kViewWidth * 2, kViewHeight * 2, SDL_WINDOW_RESIZABLE);
    if (!window_) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!renderer_) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderVSync(renderer_, 1);
    SDL_SetRenderLogicalPresentation(renderer_, kViewWidth, kViewHeight, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

    input_.init();
    assets_.init(renderer_);
    audio_.init();
    settings_.load();
    applySettings();
    data_.load(assets_);
    parseScript();

    if (options_.devGame()) {
        scene_ = std::make_unique<GameplayScene>(*this, GameplayScene::kDevSlot, true);
    } else {
        scene_ = std::make_unique<TitleScene>(*this);
    }
    return true;
}

void App::shutdown() {
    nextScene_.reset();
    scene_.reset();  // gameplay saves its slot on destruction
    audio_.shutdown();
    assets_.clear();
    input_.shutdown();
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

void App::applySettings() {
    audio_.setMusicVolume(static_cast<float>(settings_.musicVolume) / 10.0f);
    if (window_) SDL_SetWindowFullscreen(window_, settings_.fullscreen);
}

void App::toggleFullscreen() {
    settings_.fullscreen = !settings_.fullscreen;
    applySettings();
    settings_.save();
}

void App::processEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) running_ = false;
        if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
            if (e.key.scancode == SDL_SCANCODE_F11 ||
                (e.key.scancode == SDL_SCANCODE_RETURN && (e.key.mod & SDL_KMOD_ALT))) {
                toggleFullscreen();
                continue;
            }
            if (e.key.scancode == SDL_SCANCODE_F3) debugDraw = !debugDraw;
        }
        input_.handleEvent(e);
    }
}

void App::parseScript() {
    // Format: "Action:from-to,Action:tick" with action names as in Input (e.g. Right, Jump, UseRight).
    static const std::pair<const char*, Action> names[] = {
        {"Left", Action::Left},         {"Right", Action::Right},       {"Up", Action::Up},
        {"Down", Action::Down},         {"Jump", Action::Jump},         {"UseRight", Action::UseRight},
        {"UseLeft", Action::UseLeft},   {"Interact", Action::Interact}, {"Inventory", Action::Inventory},
        {"Pause", Action::Pause},       {"Confirm", Action::Confirm},   {"Cancel", Action::Cancel},
        {"Drop", Action::Drop}};
    std::stringstream ss(options_.script);
    for (std::string step; std::getline(ss, step, ',');) {
        const auto colon = step.find(':');
        if (colon == std::string::npos) continue;
        const std::string name = step.substr(0, colon);
        const std::string range = step.substr(colon + 1);
        const auto dash = range.find('-');
        const int from = std::stoi(range.substr(0, dash));
        const int to = dash == std::string::npos ? from : std::stoi(range.substr(dash + 1));
        for (const auto& [n, a] : names) {
            if (name == n) script_.push_back({a, from, to});
        }
    }
}

void App::applyScript() {
    for (const ScriptStep& s : script_) {
        bool held = false;
        for (const ScriptStep& o : script_) {
            if (o.action == s.action && tickCount_ >= o.from && tickCount_ <= o.to) held = true;
        }
        input_.setScripted(s.action, held);
    }
}

void App::run() {
    running_ = true;
    const double tick = 1.0 / kTickRate;
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    Uint64 previous = SDL_GetPerformanceCounter();
    double accumulator = 0.0;

    while (running_) {
        processEvents();

        const Uint64 now = SDL_GetPerformanceCounter();
        // Clamp long frames (window drag, breakpoint) so the simulation doesn't spiral.
        const double frame = std::min((now - previous) / freq, 0.25);
        previous = now;
        accumulator += frame;
        if (frame > 0.0) fps_ = fps_ * 0.95f + static_cast<float>(1.0 / frame) * 0.05f;

        while (accumulator >= tick) {
            if (!script_.empty()) applyScript();
            scene_->update(static_cast<float>(tick));
            input_.consumeEdges();
            accumulator -= tick;
            ++tickCount_;
            if (nextScene_) break;
        }

        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
        SDL_RenderClear(renderer_);
        scene_->render(renderer_, static_cast<float>(accumulator / tick));
        if (debugDraw) {
            SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
            SDL_RenderDebugTextFormat(renderer_, kViewWidth - 72, 4, "%3.0f fps", fps_);
        }
        if (!options_.screenshotPath.empty() && tickCount_ >= options_.screenshotTicks) {
            saveScreenshot(options_.screenshotPath);
            running_ = false;
        }
        SDL_RenderPresent(renderer_);

        if (nextScene_) {
            scene_ = std::move(nextScene_);
            accumulator = 0.0;
        }
    }
}

void App::saveScreenshot(const std::string& path) {
    SDL_Surface* shot = SDL_RenderReadPixels(renderer_, nullptr);
    if (!shot) {
        SDL_Log("Screenshot failed: %s", SDL_GetError());
        return;
    }
    if (!SDL_SaveBMP(shot, path.c_str())) SDL_Log("Saving %s failed: %s", path.c_str(), SDL_GetError());
    SDL_DestroySurface(shot);
}
