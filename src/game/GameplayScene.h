#pragma once

#include "core/Scene.h"
#include "entity/Player.h"
#include "world/Area.h"
#include "world/Camera.h"

#include <string>

class GameplayScene : public Scene {
public:
    explicit GameplayScene(App& app);

    void update(float dt) override;
    void render(SDL_Renderer* renderer, float alpha) override;

private:
    // Starts a fade-out; the area swap happens when the screen is black.
    void requestTransition(const std::string& areaId, const std::string& spawn);
    void enterArea(const std::string& areaId, const std::string& spawn);
    void checkDoors();
    const MapObject* doorUnderPlayer() const;

    Area area_;
    Player player_;
    Camera camera_;

    std::string pendingArea_;
    std::string pendingSpawn_;
    float fade_ = 1.0f;        // 0 = clear, 1 = black
    int fadeDirection_ = -1;   // -1 fading in, +1 fading out, 0 idle
    float areaTitleTimer_ = 0.0f;
};
