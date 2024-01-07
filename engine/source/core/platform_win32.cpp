#include <Windows.h>
#include "core/platform.h"
#include "core/logger.h"

namespace cyb::platform
{
	bool GetVideoModesForDisplay(int32_t displayNum, std::vector<VideoMode>& modeList)
	{
		modeList.clear();

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

		for (DWORD modeNum = 0; ; modeNum++)
		{
			if (!EnumDisplaySettingsA(device.DeviceName, modeNum, &devmode))
				break;

			float aspect = (float)devmode.dmPelsWidth / (float)devmode.dmPelsHeight;
			if (devmode.dmBitsPerPel != 32 ||
				devmode.dmDisplayFrequency != systemDisplayHz ||
				devmode.dmPelsHeight < 720 ||
				devmode.dmDisplayFixedOutput != DMDFO_DEFAULT ||
				aspect < 1.6f)
				continue;

			VideoMode mode;
			mode.width = devmode.dmPelsWidth;
			mode.height = devmode.dmPelsHeight;
			mode.displayHz = devmode.dmDisplayFrequency;
			if (std::find(modeList.begin(), modeList.end(), mode) == modeList.end())
				modeList.push_back(mode);
		}

		auto compare = [](const VideoMode& a, const VideoMode& b) -> bool
		{
			if (a.displayHz != b.displayHz)
				return a.displayHz > b.displayHz;
			if (a.width != b.width)
				return a.width > b.width;
			return a.height > b.height;
		};

		std::sort(modeList.begin(), modeList.end(), compare);
		return true;
	}

	void Exit(int exitCode)
	{
		//PostQuitMessage(exitCode);
		ExitProcess(exitCode);
	}

	void CreateMessageWindow(const std::string& msg, const std::string& caption)
	{
		MessageBoxA(GetActiveWindow(), msg.c_str(), caption.c_str(), 0);
	}

	bool FileDialog(FileDialogMode mode, const std::string& filters, std::string& output)
	{
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
		switch (mode)
		{
		case FileDialogMode::Open:
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
			ofn.Flags |= OFN_NOCHANGEDIR;
			ok = GetOpenFileNameA(&ofn);
			break;
		case FileDialogMode::Save:
			ofn.Flags = OFN_OVERWRITEPROMPT;
			ofn.Flags |= OFN_NOCHANGEDIR;
			ok = GetSaveFileNameA(&ofn);
			break;
		}

		output = ofn.lpstrFile;
		return ok;
	}
}