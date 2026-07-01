#pragma once
#include "core/non_copyable.h"
#include "core/sys.h"

namespace cyb
{
    struct ClientWindowDesc
    {
        std::string_view title{ "cyb-engine" };
        uint32_t width{ 1920 };
        uint32_t height{ 1080 };
        bool resizable{ true };
        bool decorated{ true };
    };

    class ClientWindow : public MovableNonCopyable
    {
    public:
#ifdef _WIN32
        struct Handle
        {
            HINSTANCE hInstance{ nullptr };
            HWND hWnd{ nullptr };
        };
#else // _WIN32
        using Handle = void*;
#endif // _WIN32
        static ClientWindow Create(const ClientWindowDesc& desc);

        ClientWindow() noexcept = default;
        ClientWindow(ClientWindow&& other) noexcept;
        ClientWindow& operator=(ClientWindow&& other) noexcept;
        ~ClientWindow();

        /** 
         * Parses all window events. Call this once per frame.
         * Returns false is window is requested to close.
         */
        bool PollEvents() noexcept;

        [[nodiscard]] uint32_t GetWidth() const noexcept { return m_width; }
        [[nodiscard]] uint32_t GetHeight() const noexcept { return m_height; }
        [[nodiscard]] Handle GetNativeHandle() const noexcept { return m_nativeHandle; }
        [[nodiscard]] bool IsOpen() const noexcept { return m_isOpen; }
        [[nodiscard]] bool IsActive() const noexcept { return m_isActive; }
        [[nodiscard]] bool IsMinimized() const noexcept { return m_isMinimized; }

#ifdef _WIN32
        void HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
#endif // _WIN32

    private:
        /** Private constructor. Use ClientWindow::Create to create window. */
        explicit ClientWindow(Handle handle, const ClientWindowDesc& desc);

        uint32_t m_width{ 0 };
        uint32_t m_height{ 0 };
        bool m_isOpen{ true };
        bool m_isMinimized{ false };
        bool m_isActive{ true };
        Handle m_nativeHandle{};
    };
} // namespace cyb