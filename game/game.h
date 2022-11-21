#pragma once

#include "cyb-engine.h"

class GameRenderer : public cyb::hli::RenderPath3D
{
public:
    void Load() override;
    void Update(float dt) override;

    void CameraControl(float dt);
};

class Game : public cyb::hli::Application
{
private:
    GameRenderer renderer;

public:
    virtual ~Game() = default;
    void Initialize() override;
};