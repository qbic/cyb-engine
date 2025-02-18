#include "input/input.h"
#include "input/input-raw.h"
#include "core/profiler.h"

namespace cyb::input
{
    KeyboardState keyboard;
    MouseState mouse;
    std::atomic_bool initialized = false;

    void ButtonState::RegisterKeyDown()
    {
        isDown = true;
        halfTransitionCount++;
    }

    void ButtonState::RegisterKeyUp()
    {
        isDown = false;
    }

    void ButtonState::Reset()
    {
        halfTransitionCount = 0;
    }

    void Initialize()
    {
        Timer timer;

        rawinput::Initialize();

        CYB_INFO("Input system initialized in {:.2f}ms", timer.ElapsedMilliseconds());
        initialized.store(true);
    }

    void Update(WindowHandle window)
    {
        if (!initialized.load())
            return;

        CYB_PROFILE_CPU_SCOPE("Input");

        bool hasWindowFocus = (GetFocus() == window);
        rawinput::Update(hasWindowFocus);

#ifdef _WIN32
        rawinput::GetKeyboardState(&keyboard);
        rawinput::GetMouseState(&mouse);

        // Since raw input doesn't contain absolute mouse position, we get it with regular winapi:
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(window, &p);
        mouse.position = XMFLOAT2((float)p.x, (float)p.y);
#endif // _WIN32
    }

    const KeyboardState& GetKeyboardState()
    {
        return keyboard;
    }

    const MouseState& GetMouseState()
    {
        return mouse;
    }

    [[nodiscard]] static const ButtonState& GetButtonState(uint32_t button)
    {
        uint32_t keycode = button;

        switch (button)
        {
        case input::MOUSE_BUTTON_LEFT:
            return mouse.leftButton;
        case input::MOUSE_BUTTON_MIDDLE:
            return mouse.middleButton;
        case input::MOUSE_BUTTON_RIGHT:
            return mouse.rightButton;
#ifdef _WIN32
        case KEYBOARD_BUTTON_UP:
            keycode = VK_UP;
            break;
        case KEYBOARD_BUTTON_DOWN:
            keycode = VK_DOWN;
            break;
        case KEYBOARD_BUTTON_LEFT:
            keycode = VK_LEFT;
            break;
        case KEYBOARD_BUTTON_RIGHT:
            keycode = VK_RIGHT;
            break;
        case KEYBOARD_BUTTON_SPACE:
            keycode = VK_SPACE;
            break;
        case KEYBOARD_BUTTON_F1:
            keycode = VK_F1;
            break;
        case KEYBOARD_BUTTON_F2:
            keycode = VK_F2;
            break;
        case KEYBOARD_BUTTON_F3:
            keycode = VK_F3;
            break;
        case KEYBOARD_BUTTON_F4:
            keycode = VK_F4;
            break;
        case KEYBOARD_BUTTON_F5:
            keycode = VK_F5;
            break;
        case KEYBOARD_BUTTON_F6:
            keycode = VK_F6;
            break;
        case KEYBOARD_BUTTON_F7:
            keycode = VK_F7;
            break;
        case KEYBOARD_BUTTON_F8:
            keycode = VK_F8;
            break;
        case KEYBOARD_BUTTON_F9:
            keycode = VK_F9;
            break;
        case KEYBOARD_BUTTON_F10:
            keycode = VK_F10;
            break;
        case KEYBOARD_BUTTON_F11:
            keycode = VK_F11;
            break;
        case KEYBOARD_BUTTON_F12:
            keycode = VK_F12;
            break;
        case KEYBOARD_BUTTON_ESCAPE:
            keycode = VK_ESCAPE;
            break;
        case KEYBOARD_BUTTON_ENTER:
            keycode = VK_RETURN;
            break;
        case KEYBOARD_BUTTON_LSHIFT:
            keycode = VK_LSHIFT;
            break;
        case KEYBOARD_BUTTON_RSHIFT:
            keycode = VK_RSHIFT;
            break;
#endif // _WIN32
        }

        assert(keycode < _countof(keyboard.buttons));
        return keyboard.buttons[keycode];
    }

    bool IsDown(uint32_t button)
    {
        if (!initialized.load())
            return false;

        const ButtonState& buttonState = GetButtonState(button);
        return buttonState.isDown;
    }

    bool WasPressed(uint32_t button)
    {
        if (!initialized.load())
            return false;

        const ButtonState& buttonState = GetButtonState(button);
        return ((buttonState.halfTransitionCount > 1) || (buttonState.halfTransitionCount == 1 && buttonState.isDown));
    }
}