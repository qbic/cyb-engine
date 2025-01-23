#pragma once
#include "cyb-engine.h"

class Game : public cyb::hli::RenderPath3D
{
public:
    void Load() override;
    void Update(double dt) override;

    void CameraControl(double dt);

private:
    float m_mouseSensitivity = 0.15f;
    float m_moveSpeed = 12.0f;
    float m_moveAcceleration = 0.18f;

    XMFLOAT3 m_cameraVelocity = XMFLOAT3(0, 0, 0);
};

class GameApplication : public cyb::hli::Application
{
public:
    virtual ~GameApplication() = default;

    cyb::hli::RenderPath* GetRenderPath() const override
    {
        static Game renderPath;
        return &renderPath;
    }
};