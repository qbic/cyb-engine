#pragma once

#include "cyb-engine.h"

class GameRenderer : public cyb::hli::RenderPath3D
{
public:
    void Load() override;
    void Update(float dt) override;

    void CameraControl(float dt);

private:
    float m_mouseSensitivity = 0.15f;
    float m_moveSpeed = 12.0f;
    float m_moveAcceleration = 0.18f;

    XMFLOAT3 m_cameraMove = XMFLOAT3(0, 0, 0);
};

class Game : public cyb::hli::Application
{
private:
    GameRenderer renderer;

public:
    virtual ~Game() = default;
    void Initialize() override;
};