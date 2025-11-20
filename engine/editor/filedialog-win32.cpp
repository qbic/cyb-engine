#include <thread>
#include "editor/filedialog.h"
#include "core/sys.h"

namespace cyb
{
    std::string BuildFilterString(const std::vector<FileDialogFilter>& filters)
    {
        std::string filterStr;
        for (const auto& filter : filters)
        {
            filterStr.append(filter.description);
            filterStr.push_back('\0');

            std::string_view ext = filter.extensions;
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

        filterStr.push_back('\0'); // double null terminate
        return filterStr;
    }

    void OpenLoadFileDialog(const std::vector<FileDialogFilter>& filters, FileDialogCallback callback)
    {
        std::thread([=] {
            OPENFILENAMEA ofn{ 0 };
            CHAR szFile[MAX_PATH]{ 0 };

            std::string filter = BuildFilterString(filters);

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = nullptr;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = filter.c_str();
            ofn.nFilterIndex = 1;
            ofn.lpstrFileTitle = nullptr;
            ofn.nMaxFileTitle = 0;
            ofn.lpstrInitialDir = nullptr;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

            if (GetOpenFileNameA(&ofn) == TRUE)
                callback(ofn.lpstrFile);
        }).detach();
    }

    void OpenSaveFileDialog(const std::vector<FileDialogFilter>& filters, FileDialogCallback callback)
    {
        std::thread([=] {
            OPENFILENAMEA ofn{ 0 };
            CHAR szFile[MAX_PATH]{ 0 };

            std::string filter = BuildFilterString(filters);

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = nullptr;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = filter.c_str();
            ofn.nFilterIndex = 1;
            ofn.lpstrFileTitle = nullptr;
            ofn.nMaxFileTitle = 0;
            ofn.lpstrInitialDir = nullptr;
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

            if (GetSaveFileNameA(&ofn) == TRUE)
                callback(ofn.lpstrFile);
        }).detach();
    }
} // namespace cyb