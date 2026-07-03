#include "input/input.h"
#include "input/input_raw.h"
#include "systems/profiler.h"

namespace cyb::input
{
    KeyboardState g_keyboard{};
    MouseState g_mouse{};
    static std::atomic_bool initialized{ false };

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

    void Initialize(NativeWindowHandle window) noexcept
    {
        Timer timer;

#ifdef _WIN32
        rawinput::Init(window);
#endif

        CYB_INFO("Input system initialized in {:.2f}ms", timer.ElapsedMilliseconds());
        initialized.store(true);
    }

    void Update(NativeWindowHandle window) noexcept
    {
        assert(initialized.load());

        CYB_PROFILE_CPU_SCOPE("Input");

#ifdef _WIN32
        // Reset mouse delta position
        g_mouse.pointerDelta.x = 0.0f;
        g_mouse.pointerDelta.y = 0.0f;

        // Since raw input doesn't contain absolute mouse position, we get it
        // with though old trusty winapi.
        POINT p{};
        GetCursorPos(&p);
        ScreenToClient(window, &p);
        g_mouse.pointerPosition = Vec2{ (float)p.x, (float)p.y };
#endif

        // Ignore inputs if application isn't in foreground
        const bool ignoreInput = (::GetForegroundWindow() != window);

        g_keyboard.Update(ignoreInput);
        g_mouse.Update(ignoreInput);
    }

    bool KeyDown(uint32_t key) noexcept
    {
        return g_keyboard.KeyDown(key);
    }

    bool KeyUp(uint32_t key) noexcept
    {
        return g_keyboard.KeyUp(key);
    }

    bool KeyPressed(uint32_t key) noexcept
    {
        return g_keyboard.KeyPressed(key);
    }

    bool KeyReleased(uint32_t key) noexcept
    {
        return g_keyboard.KeyReleased(key);
    }

    Vec2 PointerDelta() noexcept
    {
        return g_mouse.pointerDelta;
    }

    MouseButton MouseButtons() noexcept
    {
        return g_mouse.GetButtons();
    }

    MouseButton MouseButtonsPressed() noexcept
    {
        return g_mouse.GetPressedButtons();
    }

    MouseButton MouseButtonsReleased() noexcept
    {
        return g_mouse.GetReleasedButtons();
    }
}