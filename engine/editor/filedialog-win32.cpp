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
        std::string filterStr;
        filterStr.reserve(64);

        for (const auto& filter : filters)
        {
            filterStr.append(filter.description);
            filterStr.push_back('\0');

            const std::string_view& ext = filter.extensions;
            size_t start = 0;

            for (size_t pos = 0; pos != std::string_view::npos; )
            {
                pos = ext.find(';', start);

                const size_t len = (pos == std::string_view::npos)
                    ? ext.size() - start
                    : pos - start;

                filterStr.append("*.");
                filterStr.append(ext.substr(start, len));
                filterStr.push_back('\0');

                start = (pos == std::string_view::npos)
                    ? ext.size()
                    : pos + 1;
            }
        }

        filterStr.push_back('\0'); // Double null terminate
        return filterStr;
    }


    std::optional<std::string> OpenLoadFileDialog(const std::vector<FileDialogFilter>& filters)
    {
        OPENFILENAMEA ofn{ 0 };
        CHAR szFile[MAX_PATH]{ 0 };

        const std::string filter = BuildFilterString(filters);

        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = g_parentWindow;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = filter.c_str();
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = nullptr;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = nullptr;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameA(&ofn) == FALSE)
            return std::nullopt;

        return std::string(ofn.lpstrFile);
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
        CHAR szFile[MAX_PATH]{ 0 };

        const std::string filter = BuildFilterString(filters);

        OPENFILENAMEA ofn{ 0 };
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = g_parentWindow;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = filter.c_str();
        ofn.nFilterIndex = 1;
        ofn.lpstrDefExt = "fu";
        ofn.lpstrFileTitle = nullptr;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = nullptr;
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

        if (GetSaveFileNameA(&ofn) == FALSE)
            return std::nullopt;

        return std::string(ofn.lpstrFile);
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