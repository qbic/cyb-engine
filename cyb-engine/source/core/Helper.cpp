#include <Windows.h>
#include <commctrl.h>
#include <fstream>
#include <thread>
#include <algorithm>
#include <sstream>
#include "core/logger.h"
#include "Core/Helper.h"
#include "Core/Platform.h"

namespace cyb::helper
{
    std::string ToUpper(const std::string& s)
    {
        std::string result;

        for (unsigned int i = 0; i < s.length(); ++i)
        {
            result += (char)std::toupper(s.at(i));
        }

        return result;
    }

    std::string GetExtensionFromFileName(const std::string& filename)
    {
        size_t idx = filename.rfind('.');

        if (idx != std::string::npos)
        {
            std::string extension = filename.substr(idx + 1);
            return extension;
        }

        // No extension found
        return "";
    }

	std::string GetBasePath(const std::string& path)
	{
		size_t idx = path.find_last_of("/\\");

		if (idx != std::string::npos)
		{
			std::string basePath = path.substr(0, idx + 1);
			return basePath;
		}

		return "";
	}

    bool FileRead(const std::string& filename, std::vector<uint8_t>& data, bool quiet)
    {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (file.is_open())
        {
            size_t dataSize = (size_t)file.tellg();
            file.seekg(0, file.beg);
            data.resize(dataSize);
            file.read((char*)data.data(), dataSize);
            file.close();
            return true;
        }

		std::string error_message = fmt::format("Failed to open file (filename={0}): {1}", filename, strerror(errno));
		if (!quiet)
			CYB_ERROR(error_message);
		else
			std::copy(error_message.begin(), error_message.end(), std::back_inserter(data));

        return false;
    }

	bool FileWrite(const std::string& filename, const uint8_t* data, size_t size)
	{
		if (size <= 0)
		{
			return false;
		}

		std::ofstream file(filename, std::ios::binary | std::ios::trunc);
		if (file.is_open())
		{
			file.write((const char*)data, size);
			file.close();
			return true;
		}
		
		CYB_ERROR("Failed to open file (filename={0}): {1}\n", filename, strerror(errno));
		return false;
	}

    bool FileExist(const std::string& filename)
    {
        std::ifstream file(filename);
        return file.is_open();
    }

	void TokenizeString(std::string const& str, const char delim, std::vector<std::string>& out)
	{
		size_t start;
		size_t end = 0;

		while ((start = str.find_first_not_of(delim, end)) != std::string::npos)
		{
			end = str.find(delim, start);
			out.push_back(str.substr(start, end - start));
		}
	}

	std::string JoinStrings(std::vector<std::string>::iterator begin, std::vector<std::string>::iterator end, const char delim) 
	{
		std::string result;

		while (begin != end)
		{
			result += *begin + delim;
			begin++;
		}
		
		return result;
	}

	void FileDialog(FileOp mode, const std::string& filters, std::function<void(std::string fileName)> onSuccess)
	{
		std::thread([=] {

			char szFile[256];

			OPENFILENAMEA ofn;
			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = nullptr;
			ofn.lpstrFile = szFile;
			// Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
			// use the contents of szFile to initialize itself.
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
			case FileOp::OPEN:
				ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
				ofn.Flags |= OFN_NOCHANGEDIR;
				ok = GetOpenFileNameA(&ofn) == TRUE;
				break;
			case FileOp::SAVE:
				ofn.Flags = OFN_OVERWRITEPROMPT;
				ofn.Flags |= OFN_NOCHANGEDIR;
				ok = GetSaveFileNameA(&ofn) == TRUE;
				break;
			}

			if (ok)
			{
				onSuccess(ofn.lpstrFile);
			}

			}).detach();
	}

	LRESULT CALLBACK SourceEditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
	{
		switch (uMsg)
		{
		case WM_KEYDOWN:
		case WM_SETCURSOR:
			// Send an event to parent to that line status updates correctly
			// (this is probably not the right way of doing this)
			SendMessage(GetParent(hWnd), WM_NOTIFY, 0, 0);
			// pass through to default:

		default:
			return DefSubclassProc(hWnd, uMsg, wParam, lParam);
			break;
		}
	}

	static HWND hwndSourceEdit;
	static HWND hwndErrorEdit;
	static HWND hwndStatusbar;

	LRESULT CALLBACK ShaderDebugWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_CREATE:
		{
			HINSTANCE hInstance = (HINSTANCE)platform::GetInstance();

			hwndStatusbar = CreateWindowEx(
				0,
				STATUSCLASSNAME,
				NULL,
				WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
				0, 0, 0, 0,
				hWnd,
				(HMENU)1000,
				hInstance,
				NULL);

			int iStatusWidths[] = { 100 };
			SendMessage(hwndStatusbar, SB_SETPARTS, 1, (LPARAM)iStatusWidths);

			hwndSourceEdit = CreateWindow(
				L"EDIT",
				L"",
				WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL,
				1, 1, 0, 0,
				hWnd,
				NULL,
				hInstance,
				NULL
			);
			SetWindowSubclass(hwndSourceEdit, &SourceEditProc, 1, 0);
			SendMessage(hwndSourceEdit, WM_NOTIFY, 0, 0);

			hwndErrorEdit = CreateWindow(
				L"EDIT",
				L"",
				WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL,
				1, 1, 0, 0,
				hWnd,
				NULL,
				hInstance,
				NULL
			);

			HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
			SendMessage(hwndSourceEdit, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(FALSE, 0));
			SendMessage(hwndErrorEdit, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(FALSE, 0));
		} break;

		case WM_NOTIFY:
		{
			LRESULT lineIndex = SendMessage(hwndSourceEdit, EM_LINEFROMCHAR, (WPARAM)-1, 0);
			wchar_t buf[128];
			swprintf_s(buf, L"Line: %lld", lineIndex);
			SendMessage(hwndStatusbar, SB_SETTEXT, 0, (LPARAM)buf);
		} break;

		case WM_SIZE:
		{
			RECT rcClient;
			GetClientRect(hWnd, &rcClient);

			const int scrollHeight = GetSystemMetrics(SM_CYHSCROLL);
			const int statusbarHeight = 20;
			const int errorEditHeight = 80;
			const int sourceEditHeight = rcClient.bottom - rcClient.top - errorEditHeight - scrollHeight - statusbarHeight;
			const int errorEditBegin = sourceEditHeight + scrollHeight;

			SetWindowPos(hwndSourceEdit, NULL, 0, 0, rcClient.right, sourceEditHeight, SWP_NOZORDER);
			SetWindowPos(hwndErrorEdit, NULL, 0, errorEditBegin, rcClient.right, errorEditHeight, SWP_NOZORDER);
			SendMessage(hwndStatusbar, WM_SIZE, 0, 0);
		} break;

		default:
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
			break;
		}

		return 0;
	}

	void ShaderDebugDialog(const std::string& source, const std::string& errorMessage)
	{
		HINSTANCE hInstance = (HINSTANCE)platform::GetInstance();

		WNDCLASS wc = {};
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = ShaderDebugWindowProc;
		wc.hInstance = hInstance;
		wc.lpszClassName = L"SHADERCOMP";
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		RegisterClass(&wc);
		
		CreateWindow(
			L"SHADERCOMP",
			L"Shader Compiler Error",
			WS_VISIBLE | WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT,
			500, 500,
			NULL,
			NULL,
			hInstance,
			NULL
		);

		SendMessageA(hwndSourceEdit, WM_SETTEXT, 0, (LPARAM)source.c_str());
		SendMessageA(hwndErrorEdit, WM_SETTEXT, 0, (LPARAM)errorMessage.c_str());
	}
};