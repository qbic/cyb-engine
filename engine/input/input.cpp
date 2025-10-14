#include "input/input.h"
#include "input/input-raw.h"
#include "systems/profiler.h"

namespace cyb::input
{
    KeyboardState keyboard;
    MouseState mouse;
    std::atomic_bool initialized{ false };

    void KeyboardState::Update(bool ignoreInput)
    {
        if (ignoreInput)
            memset(m_currentState, 0, 256);

        memcpy(m_keys[m_active ^ 1], m_currentState, 256);
        m_active ^= 1;
    }

    bool KeyboardState::KeyDown(int key) const
    {
        return (m_keys[m_active][key] == 1);
    }

    bool KeyboardState::KeyUp(int key) const
    {
        return !KeyDown(key);
    }

    bool KeyboardState::KeyChanged(int key) const
    {
        return m_keys[0][key] ^ m_keys[1][key];
    }

    bool KeyboardState::KeyPressed(int key) const
    {
        return m_keys[m_active][key] & (uint8_t)KeyChanged(key);
    }

    bool KeyboardState::KeyReleased(int key) const
    {
        return m_keys[m_active ^ 1][key] & (uint8_t)KeyChanged(key);
    }

    void KeyboardState::SetKey(int key, bool active)
    {
        m_currentState[key] = active ? 1 : 0;
    }

    void MouseState::Update(bool ignoreInput)
    {
        if (ignoreInput)
            currentButtonState = MouseButton::None;

        buttons[activeButtonIndex ^ 1] = currentButtonState;
        activeButtonIndex ^= 1;
    }

    void Initialize(WindowHandle window)
    {
        Timer timer;

#ifdef _WIN32
        rawinput::Initialize(window);
#endif

        CYB_INFO("Input system initialized in {:.2f}ms", timer.ElapsedMilliseconds());
        initialized.store(true);
    }

    void Update(WindowHandle window)
    {
        if (!initialized.load())
            return;

        CYB_PROFILE_CPU_SCOPE("Input");

#ifdef _WIN32
        rawinput::Update(keyboard, mouse);

        // Since raw input doesn't contain absolute mouse position, we get it
        // with though old trusty winapi.
        POINT p{};
        GetCursorPos(&p);
        ScreenToClient(window, &p);
        mouse.pointerPosition = XMFLOAT2((float)p.x, (float)p.y);
#endif

        // Ignore inputs if application isn't in foreground
        const bool ignoreInput = (::GetForegroundWindow() != window);

        keyboard.Update(ignoreInput);
        mouse.Update(ignoreInput);
    }

    const KeyboardState& GetKeyboardState()
    {
        return keyboard;
    }

    const MouseState& GetMouseState()
    {
        return mouse;
    }

    bool KeyDown(uint32_t key)
    {
        return keyboard.KeyDown(key);
    }

    bool KeyUp(uint32_t key)
    {
        return keyboard.KeyUp(key);
    }

    bool KeyPressed(uint32_t key)
    {
        return keyboard.KeyPressed(key);
    }

    bool KeyReleased(uint32_t key)
    {
        return keyboard.KeyReleased(key);
    }

    MouseButton MouseButtons()
    {
        return mouse.GetButtons();
    }

    MouseButton MouseButtonsPressed()
    {
        return mouse.GetPressedButtons();
    }

    MouseButton MouseButtonsReleased()
    {
        return mouse.GetReleasedButtons();
    }
}