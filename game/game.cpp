#include "editor/editor.h"
#include "editor/terrain-generator.h"
#include "game.h"

#include "core/filesystem.h"
#include "hli/canvas.h"

using namespace cyb;

void Game::Initialize()
{
    Application::Initialize();

    renderer.Load();
    ActivePath(&renderer);
}

void GameRenderer::Load()
{
#if 1 
    {
        Timer timer;
        scene::Scene& scene = scene::GetScene();
        std::string filename = "../terrain_04.cbs";
        std::vector<uint8_t> sceneFile;
        if (filesystem::ReadFile(filename, sceneFile))
        {
            Archive archive(sceneFile.data(), sceneFile.size());
            Serializer ser(archive);
            scene.Clear();
            scene.Serialize(ser);
            CYB_TRACE("Loaded scene (filename={0}) in {1:.2f}ms", filename, timer.ElapsedMilliseconds());
        }
    }

    //scene::LoadModel("../terrain_04.cbs");
#else
    editor::TerrainMeshDesc terrainDesc;
    terrainDesc.size = 1000;
    terrainDesc.maxAltitude = 120;
    //terrainMeshDesc.mapResolution = 400;
    terrainDesc.numChunks = 1;
    terrainDesc.heightmap_desc.function = editor::TerrainNoiseDescription::NoiseFunction::PERLIN;
    terrainDesc.heightmap_desc.seed = 15236;
    terrainDesc.heightmap_desc.frequency = 7;
    terrainDesc.heightmap_desc.octaves = 4;
    terrainDesc.moisturemap_desc.seed = 33412;
    terrainDesc.moisturemap_desc.frequency = 5.4f;

    editor::SetTerrainGenerationParams(&m_meshDesc);
    editor::GenerateTerrain(&m_meshDesc, scene);

    scene->weathers.Create(ecs::CreateEntity());

    scene->CreateLight("Sun00", XMFLOAT3(122.0f, 101.0f, 170.0f), XMFLOAT3(1.0f, 1.0f, 1.0f), 1.0f, 10000.0f, scene::LightType::Directional);
    scene->CreateLight("Sun01", XMFLOAT3(-32.0f, -53.0f, -42.0f), XMFLOAT3(1.0f, 1.0f, 1.0f), 0.75f, 10000.0f, scene::LightType::Directional);
    scene->CreateLight("Sun02", XMFLOAT3(-63.0f, 162.0f, -200.0f), XMFLOAT3(1.0f, 1.0f, 1.0f), 0.6f, 10000.0f, scene::LightType::Directional);
    scene->CreateLight("Light_Point01", XMFLOAT3(0.0f, 20.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 1.0f), 0.6f, 100.0f, scene::LightType::Point);
#endif

    cameraTransform.Translate(XMFLOAT3(0, 2, -10));

    // NOTE: i feel like this should be handled by the engine initialization somewhere sence
    // all this really does is loding icons for the editor
    RenderPath3D::Load();
}

void GameRenderer::Update(double dt)
{
#ifndef NO_EDITOR
    // Stop input from going though the editor
    if (!editor::WantInput())
        CameraControl(dt);
#else
    CameraControl(dt);
#endif

    RenderPath3D::Update(dt);
}

void GameRenderer::CameraControl(double dt)
{
    float xDif = 0;
    float yDif = 0;
    if (input::IsDown(input::MOUSE_BUTTON_RIGHT))
    {
        const input::MouseState& mouse = input::GetMouseState();
        xDif = m_mouseSensitivity * mouse.deltaPosition.x * (1.0f / 60.0f);
        yDif = m_mouseSensitivity * mouse.deltaPosition.y * (1.0f / 60.0f);
    }

    // if dt > 100 millisec, don't allow the camera to jump too far...
    const double clampedDt = std::min(dt, 0.1);

    const double speed = (input::IsDown('F') ? 3.0 : 1.0) * 10.0 * m_moveSpeed * clampedDt;
    XMVECTOR velocity = XMLoadFloat3(&m_cameraVelocity);
    XMVECTOR moveNew = XMVectorSet(0, 0, 0, 0);

    if (input::IsDown('A'))
        moveNew += XMVectorSet(-1, 0, 0, 0);
    if (input::IsDown('D'))
        moveNew += XMVectorSet(1, 0, 0, 0);
    if (input::IsDown('S'))
        moveNew += XMVectorSet(0, 0, -1, 0);
    if (input::IsDown('W'))
        moveNew += XMVectorSet(0, 0, 1, 0);
    if (input::IsDown('C'))
        moveNew += XMVectorSet(0, -1, 0, 0);
    if (input::IsDown(input::KEYBOARD_BUTTON_SPACE))
        moveNew += XMVectorSet(0, 1, 0, 0);
    moveNew = XMVector3Normalize(moveNew) * static_cast<float>(speed);

    const double blend = m_moveAcceleration * clampedDt / 0.0166;
    velocity = XMVectorLerp(velocity, moveNew, static_cast<float>(blend)); // smooth the movement a bit
    const float moveLength = XMVectorGetX(XMVector3Length(velocity));

    if (moveLength < 0.0001f)
        velocity = XMVectorSet(0, 0, 0, 0);

    //if (abs(xDif) + abs(yDif) > 0 || moveLength > 0.0001f)
    {
        XMMATRIX cameraRotation = XMMatrixRotationQuaternion(XMLoadFloat4(&cameraTransform.rotation_local));
        XMVECTOR rotatedMove = XMVector3TransformNormal(velocity, cameraRotation);
        XMFLOAT3 rotatedMoveStored;
        XMStoreFloat3(&rotatedMoveStored, rotatedMove);
        cameraTransform.Translate(rotatedMoveStored);
        cameraTransform.RotateRollPitchYaw(XMFLOAT3(yDif, xDif, 0));
    }

    cameraTransform.UpdateTransform();
    XMStoreFloat3(&m_cameraVelocity, velocity);
}


