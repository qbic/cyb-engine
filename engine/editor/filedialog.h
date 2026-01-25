#pragma once
#include <vector>
#include <string>
#include <string_view>
#include <functional>
#include <optional>
#include "graphics/display.h"

namespace cyb
{
    /**
     * @brief Set the parent window for file dialog boxes.
     * 
     * This is optional, but recommended to set so that the file dialog is
     * modal to the main application window.
     */
    void SetFileDialogParentWindow(cyb::WindowHandle window);

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
     * @brief Open a file browser dialog window for saving a file.
     */
    std::optional<std::string> OpenSaveFileDialog(const std::vector<FileDialogFilter>& filters);

    /**
     * @brief Open a file browser dialog window for saving a file on a separate thread.
     * The callback is executed with the selected file path.
     * The thread will be detached after the dialog is closed.
     */
    void OpenSaveFileDialogAsync(const std::vector<FileDialogFilter>& filters, FileDialogCallback callback);

    /**
     * @brief Open a file browser dialog window for loading a file.
     */
    std::optional<std::string> OpenLoadFileDialog(const std::vector<FileDialogFilter>& filters);

    /**
     * @brief Open a file browser dialog window for loading a file on a separate thread.
     * The callback is executed with the selected file path.
     * The thread will be detached after the dialog is closed.
     */
    void OpenLoadFileDialogAsync(const std::vector<FileDialogFilter>& filters, FileDialogCallback callback);

} // namespace cyb