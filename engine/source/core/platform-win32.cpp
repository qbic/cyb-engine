#include "core/platform.h"
#include "core/logger.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <ShellScalingAPI.h>
#pragma comment(lib, "Shcore.lib")

// The main window needs to communicate with both imgui and the input layer:
extern LRESULT CALLBACK ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern LRESULT CALLBACK Win32_InputProcHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

namespace cyb::platform
{
	Instance applicationInstance = nullptr;
	const TCHAR* windowClassName = TEXT("CybEngineWindow");

	class LogOutputModule_VisualStudio : public logger::LogOutputModule
	{
	public:
		void Write(const logger::LogMessage& log) override
		{
			OutputDebugStringA(log.message.c_str());
		}
	};

	class Window_Win32 : public Window
	{
	private:
		HWND handle;

	public:
		explicit Window_Win32(const WindowCreateDescription& desc);
		~Window_Win32() = default;

		virtual LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		virtual void* GetNativePtr() override { return handle; }

		bool IsActive() const override
		{
			HWND foregroundWindow = GetForegroundWindow();
			return (foregroundWindow == handle);
		}

		XMINT2 GetClientSize() const override
		{
			RECT rect;
			GetClientRect(handle, &rect);
			return XMINT2(rect.right - rect.left, rect.bottom - rect.top);
		}
	};

	static std::string Win32_GetLastErrorMessage()
	{
		LPSTR s = 0;
		const DWORD error_code = GetLastError();
		FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPSTR)&s, 0, NULL);
		assert(s);
		std::string str(s);
		LocalFree(s);
		return str;
	}

	Window_Win32::Window_Win32(const WindowCreateDescription& desc)
	{
		// Register a log output for visual studio's output window
		logger::RegisterOutputModule(std::make_shared<LogOutputModule_VisualStudio>());

		assert(applicationInstance);
		DWORD style = WS_POPUP | WS_OVERLAPPED | WS_SYSMENU | WS_BORDER | WS_CAPTION;
		DWORD exStyle = WS_EX_APPWINDOW;

		if (HasFlag(desc.flags, WindowCreateFlags::AllowMinimizeBit))
			style |= WS_MINIMIZEBOX;
		if (HasFlag(desc.flags, WindowCreateFlags::AllowMaximizeBit))
			style |= WS_MAXIMIZEBOX;

		// Adjust window size and positions to take into account window border
		RECT windowRect = { 0, 0, (LONG)desc.size.x, (LONG)desc.size.y };
		AdjustWindowRect(&windowRect, style, FALSE);
		LONG x = (LONG)desc.position.x - windowRect.left;
		LONG y = (LONG)desc.position.y - windowRect.top;
		LONG windowWidth = windowRect.right - windowRect.left;
		LONG windowHeight = windowRect.bottom - windowRect.top;

		// Create the window
		handle = CreateWindowEx(
			exStyle,
			windowClassName,
			desc.title.c_str(),
			style,
			x,
			y,
			windowWidth,
			windowHeight,
			desc.parent ? static_cast<HWND>(desc.parent->GetNativePtr()) : nullptr,
			nullptr,
			(HINSTANCE)applicationInstance,
			this);

		if (!handle)
		{
			CreateMessageWindow(Win32_GetLastErrorMessage(), "CreateWindow");
			ExitProcess(1);
		}

		ShowWindow(handle, true);
	}

	LRESULT Window_Win32::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		Win32_InputProcHandler(hwnd, msg, wParam, lParam);
		if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
		{
			// If ImGUI takes an input from the user we need to return
			// so that it doesen't follow though to the game.
			return true;
		}

		switch (msg)
		{
		case WM_CLOSE:
		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		case WM_PAINT:
		{
			PAINTSTRUCT paint;
			HDC hDC = BeginPaint(hwnd, &paint);
			EndPaint(hwnd, &paint);
		} break;

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
			//assert(!"Keyboard input came in through a non-dispatch message!");
			break;
		}

		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		Window_Win32* window = (Window_Win32*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

		if (window)
			return window->WndProc(hwnd, msg, wParam, lParam);

		switch (msg)
		{
		case WM_CREATE:
			LPCREATESTRUCT pcs = (LPCREATESTRUCT)lParam;
			window = reinterpret_cast<Window_Win32*>(pcs->lpCreateParams);
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)window);
			return 0;
		}

		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	void Initialize(Instance appInstance)
	{
		assert(appInstance);
		applicationInstance = appInstance;

		// Create the application windows class
		WNDCLASS wc = {};
		wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
		wc.lpfnWndProc = WndProc;
		wc.hInstance = (HINSTANCE)appInstance;
		wc.hCursor = LoadCursor(0, IDC_ARROW);
		wc.lpszClassName = windowClassName;
		ATOM ar = RegisterClass(&wc);
		assert(SUCCEEDED(ar));

		// Enable high dpi awareness
		HRESULT hr = SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
		assert(SUCCEEDED(hr));
	}

	Instance GetInstance()
	{
		return applicationInstance;
	}

	std::shared_ptr<Window> CreateNewWindow(const WindowCreateDescription& settings)
	{
		return std::make_shared<Window_Win32>(settings);
	}

	void CreateMainWindow(const WindowCreateDescription& settings)
	{
		platform::main_window = CreateNewWindow(settings);
	}

	void Exit(int exit_code)
	{
		//PostQuitMessage(exit_code);
		ExitProcess(exit_code);
	}

	void CreateMessageWindow(const std::string& msg, const std::string& caption)
	{
		MessageBoxA(GetActiveWindow(), msg.c_str(), caption.c_str(), 0);
	}
}