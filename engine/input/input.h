#pragma once
#include <array>
#include "core/enum_flags.h"
#include "core/platform.h"

namespace cyb::input
{
    enum KeyboardButton
    {
        BUTTON_NONE = 0,

        KEYBOARD_BUTTON_UP,
        KEYBOARD_BUTTON_DOWN,
        KEYBOARD_BUTTON_LEFT,
        KEYBOARD_BUTTON_RIGHT,
        KEYBOARD_BUTTON_SPACE,
        KEYBOARD_BUTTON_F1,
        KEYBOARD_BUTTON_F2,
        KEYBOARD_BUTTON_F3,
        KEYBOARD_BUTTON_F4,
        KEYBOARD_BUTTON_F5,
        KEYBOARD_BUTTON_F6,
        KEYBOARD_BUTTON_F7,
        KEYBOARD_BUTTON_F8,
        KEYBOARD_BUTTON_F9,
        KEYBOARD_BUTTON_F10,
        KEYBOARD_BUTTON_F11,
        KEYBOARD_BUTTON_F12,
        KEYBOARD_BUTTON_ESCAPE,
        KEYBOARD_BUTTON_ENTER,
        KEYBOARD_BUTTON_LSHIFT,
        KEYBOARD_BUTTON_RSHIFT,

        SPECIAL_KEY_COUNT,

        CHARACHTER_RANGE_START = 'A',       // 'A' == 65
    };

    class KeyboardState
    {
    public:
        KeyboardState() = default;
        ~KeyboardState() = default;

        void Update(bool ignoreInput);
        bool KeyDown(int key) const;
        bool KeyUp(int key) const;
        bool KeyChanged(int key) const;
        bool KeyPressed(int key) const;
        bool KeyReleased(int key) const;

        void SetKey(int key, bool active);

    private:
        int8_t m_keys[2][256]{ 0 };     // Keep 2 keyboard states, one latest state, one previous
        int m_active{ 0 };              // Index to active state
        int8_t m_currentState[256]{ 0 };
    };

    enum class MouseButton : uint8_t
    {
        None    = 0,
        Left    = BIT(0),
        Right   = BIT(1),
        Middle  = BIT(2)
    };
    CYB_ENABLE_BITMASK_OPERATORS(MouseButton);

    struct MouseState
    {
        MouseState() = default;
        ~MouseState() = default;

        void Update(WindowHandle window, bool ignoreInput);

        [[nodiscard]] MouseButton GetButtons() const { return buttons[activeButtonIndex]; }
        [[nodiscard]] MouseButton GetChangedButtons() const { return buttons[0] ^ buttons[1]; }
        [[nodiscard]] MouseButton GetPressedButtons() const { return GetChangedButtons() & GetButtons(); }
        [[nodiscard]] MouseButton GetReleasedButtons() const { return GetChangedButtons() & buttons[activeButtonIndex ^ 1]; }

        MouseButton buttons[2]{ MouseButton::None };
        MouseButton currentButtonState{ MouseButton::None };
        uint8_t activeButtonIndex{ 0 };
        XMFLOAT2 pointerPosition{ 0, 0 };
        XMFLOAT2 pointerDelta{ 0, 0 };
        float wheelDelta{ 0 };
    };

    /**
     * @brief Initialize all the needed subsystem used by input.
     */
    void Initialize(WindowHandle window);

    /*
     * @brief Update the states of all input devices.
     *
     * Call this once per frame (before processing new input events).
     */
    void Update(WindowHandle window);

    /**
     * @brief Get a const reference to the raw keyboard state.
     */
    [[nodiscard]] const KeyboardState& GetKeyboardState();

    /**
     * @brief Get a const reference to the raw mouse state.
     */
    [[nodiscard]] const MouseState& GetMouseState();

    /**
     * @brief Check if a keyboard key is current down.
     */
    [[nodiscard]] bool KeyDown(uint32_t key);

    /**
     * @brief Check if a keyboard key is currently up.
     */
    [[nodiscard]] bool KeyUp(uint32_t key);

    /**
     * @brief Check if a keystate changed to 'down' from previous frame.
     */
    [[nodiscard]] bool KeyPressed(uint32_t key);

    /**
     * @brief Check if a keystate changed to 'up' from previous frame.
     */
    [[nodiscard]] bool KeyReleased(uint32_t key);

    /**
     * @brief Get a bitmask of the mouse button states.
     */
    [[nodiscard]] MouseButton MouseButtons();

    /**
     * @brief Get a bitmask of mouse buttonstates that changed to 'down' from previous frame.
     */
    [[nodiscard]] MouseButton MouseButtonsPressed();

    /**
     * @brief Get a bitmask of mouse buttonstates that changed to 'up' from previous frame.
     */
    [[nodiscard]] MouseButton MouseButtonsReleased();
}
