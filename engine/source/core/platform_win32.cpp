#include <Windows.h>
#include "core/platform.h"
#include "core/logger.h"

// The main window needs to communicate with both imgui and the input layer:
extern LRESULT CALLBACK ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern LRESULT CALLBACK Win32_InputProcHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

namespace cyb::platform
{
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