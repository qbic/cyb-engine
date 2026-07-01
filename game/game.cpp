#include <Windows.h>
#include "resource.h"
#include "editor/editor.h"
#include "game.h"

#include "cyb-engine.h"

using namespace cyb;

#define MAX_LOADSTRING 100

WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];
WCHAR szTextlogFile[MAX_LOADSTRING];

void Game::Load()
{
    std::string filename = resourcemanager::FindFile("scenes/terrain_01.csd");
    SerializeFromFile(filename, scene::GetScene());

    camera->zFarPlane = 1500.f;
    cameraTransform.Translate(XMFLOAT3(0, 2, -10));

    RenderPath3D::Load();
}

void Game::Update(double dt)
{
#ifdef NO_EDITOR
    const bool editorWantsInput = false;
#else
    const bool editorWantsInput = editor::WantInput();
#endif

    if (!editorWantsInput)
        CameraControl(dt);

    RenderPath3D::Update(dt);
}

void Game::CameraControl(double dt)
{
    Vec2 mouseMove{ 0.0f, 0.0f };
    if (HasFlag(input::MouseButtons(), input::MouseButton::Right))
        mouseMove = input::GetMousePointerDelta() * m_mouseSensitivity * (1.0f / 60.0f);

    // if dt > 100 millisec, don't allow the camera to jump too far...
    const float clampedDt = std::min((float)dt, 0.1f);

    const float speed = (input::KeyDown('F') ? 3.0f : 1.0f) * 10.0f * m_moveSpeed * clampedDt;
    XMVECTOR move = XMLoadFloat3(&m_cameraVelocity);
    XMVECTOR moveNew = XMVectorSet(0, 0, 0, 0);

    if (input::KeyDown('A'))
        moveNew += XMVectorSet(-1, 0, 0, 0);
    if (input::KeyDown('D'))
        moveNew += XMVectorSet(1, 0, 0, 0);
    if (input::KeyDown('S'))
        moveNew += XMVectorSet(0, 0, -1, 0);
    if (input::KeyDown('W'))
        moveNew += XMVectorSet(0, 0, 1, 0);
    if (input::KeyDown('C'))
        moveNew += XMVectorSet(0, -1, 0, 0);
    if (input::KeyDown(input::KEYBOARD_BUTTON_SPACE))
        moveNew += XMVectorSet(0, 1, 0, 0);
    moveNew = XMVector3Normalize(moveNew) * speed;

    move = XMVectorLerp(move, moveNew, m_moveAcceleration * clampedDt / 0.0166f);
    const float moveLength = XMVectorGetX(XMVector3Length(move));

    if (moveLength < 0.0001f)
        move = XMVectorSet(0, 0, 0, 0);

    if (abs(mouseMove.x) + abs(mouseMove.y) > 0 || moveLength > 0.0001f)
    {
        XMMATRIX cameraRotation = XMMatrixRotationQuaternion(XMLoadFloat4(&cameraTransform.rotation_local));
        XMVECTOR rotatedMove = XMVector3TransformNormal(move, cameraRotation);
        XMFLOAT3 rotatedMoveStored;
        XMStoreFloat3(&rotatedMoveStored, rotatedMove);
        cameraTransform.Translate(rotatedMoveStored);
        cameraTransform.RotateRollPitchYaw(XMFLOAT3(mouseMove.y, mouseMove.x, 0));
    }

    cameraTransform.UpdateTransform();
    XMStoreFloat3(&m_cameraVelocity, move);
}


int WINAPI WinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE /* hPrevInstance */,
    _In_ LPSTR /* lpCmdLine */,
    _In_ int nShowCmd)
{
    // load resource strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CYBGAME, szWindowClass, MAX_LOADSTRING);
    LoadStringW(hInstance, IDS_TEXTLOG, szTextlogFile, MAX_LOADSTRING);

    // setup engine logger output modules
    cyb::RegisterLogOutputModule<cyb::LogOutputModule_VisualStudio>();
    cyb::RegisterLogOutputModule<cyb::LogOutputModule_File>(szTextlogFile);

    // configure asset search paths
    cyb::resourcemanager::AddSearchPath("assets/");
    cyb::resourcemanager::AddSearchPath("../assets/");

    GameApplication game{};
    game.Init();
    game.UpdateLoop();

    return 0;
}