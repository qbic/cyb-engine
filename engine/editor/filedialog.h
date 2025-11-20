#pragma once
#include <vector>
#include <string>
#include <string_view>
#include <functional>

namespace cyb
{
    /**
     * @brief Filter for file dialog.
     * 
     * Example usage:
     * std::vector<FileDialogFilter> filters = {
     *      { "glTF 2.0 (*.gltf; *.glb)", "gltf;glb" },
     *      { "Image Files", "png;jpg;jpeg;bmp" },
     *      { "All Files", "*" }
     * };
     */
    struct FileDialogFilter
    {
        std::string_view description;
        std::string_view extensions;    // Semicolon separated, or '*' for all files. e.g. "png;jpg"
    };

    using FileDialogCallback = std::function<void(const std::string&)>;
    
    /**
     * @brief Open a file browser dialog window for saving a file on a separate thread.
     * The callback is executed with the selected file path.
     * The thread will be detached after the dialog is closed.
     */
    void OpenSaveFileDialog(const std::vector<FileDialogFilter>& filters, FileDialogCallback callback);

    /**
     * @brief Open a file browser dialog window for loading a file on a separate thread.
     * The callback is executed with the selected file path.
     * The thread will be detached after the dialog is closed.
     */
    void OpenLoadFileDialog(const std::vector<FileDialogFilter>& filters, FileDialogCallback callback);
} // namespace cyb