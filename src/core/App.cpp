#include "core/App.h"

#include "game/GameplayScene.h"

#include <algorithm>

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
    scene_ = std::make_unique<GameplayScene>(*this);
    return true;
}

void App::shutdown() {
    scene_.reset();
    assets_.clear();
    input_.shutdown();
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

void App::toggleFullscreen() {
    fullscreen_ = !fullscreen_;
    SDL_SetWindowFullscreen(window_, fullscreen_);
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

        if (options_.holdRight) {
            SDL_Event fake{};
            fake.type = SDL_EVENT_KEY_DOWN;
            fake.key.scancode = SDL_SCANCODE_RIGHT;
            input_.handleEvent(fake);
        }
        while (accumulator >= tick) {
            scene_->update(static_cast<float>(tick));
            input_.consumeEdges();
            accumulator -= tick;
        }

        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
        SDL_RenderClear(renderer_);
        scene_->render(renderer_, static_cast<float>(accumulator / tick));
        if (debugDraw) {
            SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
            SDL_RenderDebugTextFormat(renderer_, kViewWidth - 72, 4, "%3.0f fps", fps_);
        }
        if (!options_.screenshotPath.empty() && --options_.screenshotFrames <= 0) {
            saveScreenshot(options_.screenshotPath);
            running_ = false;
        }
        SDL_RenderPresent(renderer_);
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
