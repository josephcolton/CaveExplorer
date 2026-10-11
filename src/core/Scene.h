#pragma once

#include <SDL3/SDL.h>

class App;

class Scene {
public:
    explicit Scene(App& app) : app_(app) {}
    virtual ~Scene() = default;

    // Fixed-timestep simulation step.
    virtual void update(float dt) = 0;
    // alpha in [0,1): how far we are between the previous and current simulation state.
    virtual void render(SDL_Renderer* renderer, float alpha) = 0;

protected:
    App& app_;
};
