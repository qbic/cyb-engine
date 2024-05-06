#include <Windows.h>
#include "core/platform.h"
#include "core/logger.h"

namespace cyb {

	WindowInfo GetWindowInfo(WindowHandle window) {
		WindowInfo info;
		info.dpi = (float)GetDpiForWindow(window);
		RECT rect;
		GetClientRect(window, &rect);
		info.width = int(rect.right - rect.left);
		info.height = int(rect.bottom - rect.top);
		
		return info;
	}

	bool GetVideoModesForDisplay(std::vector<VideoMode>& modes, int32_t displayNum) {
		modes.clear();

		DISPLAY_DEVICEA device;
		device.cb = sizeof(device);
		if (!EnumDisplayDevicesA(0, displayNum, &device, 0))
			return false;

		if (!(device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP))
			return false;

		DISPLAY_DEVICEA monitor;
		monitor.cb = sizeof(monitor);
		if (!EnumDisplayDevicesA(device.DeviceName, 0, &monitor, 0))
			return false;

		DEVMODEA devmode;
		devmode.dmSize = sizeof(devmode);

		DWORD systemDisplayHz = 60;
		if (EnumDisplaySettingsA(device.DeviceName, ENUM_CURRENT_SETTINGS, &devmode))
			systemDisplayHz = devmode.dmDisplayFrequency;

		for (DWORD modeNum = 0; ; modeNum++) {
			if (!EnumDisplaySettingsA(device.DeviceName, modeNum, &devmode))
				break;

			float aspect = (float)devmode.dmPelsWidth / (float)devmode.dmPelsHeight;
			if (devmode.dmBitsPerPel < 16 ||
				devmode.dmDisplayFrequency != systemDisplayHz ||
				devmode.dmPelsHeight < 720 ||
				devmode.dmDisplayFixedOutput != DMDFO_DEFAULT ||
				aspect < 1.6f)
				continue;

			VideoMode mode;
			mode.width = devmode.dmPelsWidth;
			mode.height = devmode.dmPelsHeight;
			mode.bitsPerPixel = devmode.dmBitsPerPel;
			mode.displayFrequency = devmode.dmDisplayFrequency;
			if (std::find(modes.begin(), modes.end(), mode) == modes.end())
				modes.push_back(mode);
		}

		std::sort(modes.begin(), modes.end());
		return true;
	}

	void Exit(int exitCode) {
		//PostQuitMessage(exitCode);
		ExitProcess(exitCode);
	}

	void FatalError(const std::string& text) {
		MessageBoxA(GetActiveWindow(), fmt::format("Fatal error from application:\n{}", text).c_str(), "Error", MB_OK);
		Exit(1);
	}

	static [[nodiscard]] bool FileDialog(std::string& path, bool openFile, const std::string& filters) {
		char szFile[MAX_PATH] = {};

		OPENFILENAMEA ofn = {};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = NULL;
		ofn.lpstrFile = szFile;
		ofn.lpstrFile[0] = '\0';
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFileTitle = NULL;
		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = NULL;
		ofn.nFilterIndex = 1;
		ofn.lpstrFilter = filters.c_str();

		BOOL ok = FALSE;
		if (openFile) {
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
			ofn.Flags |= OFN_NOCHANGEDIR;
			ok = GetOpenFileNameA(&ofn);
		} else {
			ofn.Flags = OFN_OVERWRITEPROMPT;
			ofn.Flags |= OFN_NOCHANGEDIR;
			ok = GetSaveFileNameA(&ofn);
		}

		path = ofn.lpstrFile;
		return ok;
	}

	bool FileOpenDialog(std::string& path, const std::string& filters) {
		return FileDialog(path, true, filters);
	}

	bool FileSaveDialog(std::string& path, const std::string& filters) {
		return FileDialog(path, false, filters);
	}

}