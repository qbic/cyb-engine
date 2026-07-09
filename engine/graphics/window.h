#pragma once
#include <functional>
#include "core/non_copyable.h"
#include "core/sys.h"
#include "graphics/display.h"

namespace cyb
{
    struct ClientWindowDesc
    {
        std::string_view title{ "cyb-engine" };
        uint32_t width{ 1920 };
        uint32_t height{ 1080 };
        bool resizable{ true };
        bool decorated{ true };
        NativeWindowHandle parent{ nullptr };
    };

    using WindowResizeFn = std::function<void()>;

    class ClientWindow : public MovableNonCopyable
    {
    public:
        static ClientWindow Create(const ClientWindowDesc& desc);

        ClientWindow() noexcept = default;
        ClientWindow(ClientWindow&& other) noexcept;
        ClientWindow& operator=(ClientWindow&& other) noexcept;
        ~ClientWindow();

        /** 
         * Parses all window events. Call this once per frame.
         * This will also call appropriate window callbacks if set.
         * Returns false if window is requested to close.
         */
        bool PollEvents() noexcept;

        /** Sets the callback function to be called when the window is resized. */
        void SetWindowResizeCallback(WindowResizeFn callback) noexcept { m_windowResizeCallback = std::move(callback); }

        [[nodiscard]] uint32_t GetWidth() const noexcept { return m_width; }
        [[nodiscard]] uint32_t GetHeight() const noexcept { return m_height; }
        [[nodiscard]] NativeWindowHandle GetNativeHandle() const noexcept { return m_nativeHandle; }
        [[nodiscard]] bool IsOpen() const noexcept { return m_isOpen; }
        [[nodiscard]] bool IsActive() const noexcept { return m_isActive; }
        [[nodiscard]] bool IsMinimized() const noexcept { return m_isMinimized; }

#ifdef _WIN32
        void HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
#endif // _WIN32

    private:
        /** Private constructor. Use ClientWindow::Create to create window. */
        explicit ClientWindow(NativeWindowHandle windowHandle, const ClientWindowDesc& desc);

        uint32_t m_width{ 0 };
        uint32_t m_height{ 0 };
        bool m_isOpen{ false };
        bool m_isMinimized{ false };
        bool m_isActive{ false };
        NativeWindowHandle m_nativeHandle{};
        WindowResizeFn m_windowResizeCallback{};
    };
} // namespace cyb