#include <thread>
#include "editor/filedialog.h"
#include "core/sys.h"

namespace cyb
{
    static WindowHandle g_parentWindow{ nullptr };

    void SetFileDialogParentWindow(WindowHandle window)
    {
        g_parentWindow = window;
    }

    [[nodiscard]] static std::string BuildFilterString(const std::vector<FileDialogFilter>& filters)
    {
        std::string filterStr{};
        filterStr.reserve(64);

        for (const auto& filter : filters)
        {
            filterStr.append(filter.description);
            filterStr.push_back('\0');

            const std::string_view& ext = filter.extensions;
            size_t start{ 0 };
            std::string patternList{};
            bool first{ true };

            for (size_t pos = 0; pos != std::string_view::npos; )
            {
                pos = ext.find(';', start);

                const size_t len = (pos == std::string_view::npos)
                    ? ext.size() - start
                    : pos - start;

                if (len > 0)
                {
                    if (!first)
                        patternList.push_back(';');

                    patternList.append("*.");
                    patternList.append(ext.substr(start, len));
                    first = false;
                }

                start = (pos == std::string_view::npos)
                    ? ext.size()
                    : pos + 1;
            }

            filterStr.append(patternList);
            filterStr.push_back('\0');
        }

        // Double null terminate
        filterStr.push_back('\0'); 
        return filterStr;
    }

    std::optional<std::string> OpenLoadFileDialog(const std::vector<FileDialogFilter>& filters)
    {
        WCHAR szFile[MAX_PATH]{ 0 };
        const std::wstring filter = Utf8ToWide(BuildFilterString(filters));
        
        OPENFILENAMEW ofn{ 0 };
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = g_parentWindow;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = std::size(szFile);
        ofn.lpstrFilter = filter.c_str();
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = nullptr;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = nullptr;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;

        if (GetOpenFileNameW(&ofn) == FALSE)
            return std::nullopt;

        return WideToUtf8(ofn.lpstrFile);
    }

    void OpenLoadFileDialogAsync(const std::vector<FileDialogFilter>& filters, FileDialogCallback callback)
    {
        std::thread([=] {
            auto res = OpenLoadFileDialog(filters);
            if (res.has_value())
                callback(res.value());
        }).detach();
    }

    std::optional<std::string> OpenSaveFileDialog(const std::vector<FileDialogFilter>& filters)
    {
        WCHAR szFile[MAX_PATH]{ 0 };
        const std::wstring filter = Utf8ToWide(BuildFilterString(filters));

        OPENFILENAMEW ofn{ 0 };
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = g_parentWindow;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = std::size(szFile);
        ofn.lpstrFilter = filter.c_str();
        ofn.nFilterIndex = 1;
        ofn.lpstrDefExt = L"fu";
        ofn.lpstrFileTitle = nullptr;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = nullptr;
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR | OFN_EXPLORER;

        if (GetSaveFileNameW(&ofn) == FALSE)
            return std::nullopt;

        return WideToUtf8(ofn.lpstrFile);
    }

    void OpenSaveFileDialogAsync(const std::vector<FileDialogFilter>& filters, FileDialogCallback callback)
    {
        std::thread([=] {
            auto res = OpenSaveFileDialog(filters);
            if (res.has_value())
                callback(res.value());
        }).detach();
    }

} // namespace cyb