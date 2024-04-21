@echo off
pushd "%~dp0"/..

set PREMAKE_PATH=tools/premake
set PREMAKE_DOWNLOAD_URL="https://github.com/premake/premake-core/releases/download/v5.0.0-beta2/premake-5.0.0-beta2-windows.zip"

:CheckPremake
echo | set /p="Checking Premake5: "
if not exist "%PREMAKE_PATH%/premake5.exe" (
    echo [Not Found]
    echo Downloading Premake5 to %PREMAKE_PATH%...
    curl -OJL --create-dirs --output-dir %PREMAKE_PATH% %PREMAKE_DOWNLOAD_URL%
    pushd "%PREMAKE_PATH%"
    tar -xf premake-5.0.0-beta2-windows.zip
    del premake-5.0.0-beta2-windows.zip
    popd
    goto CheckPremake
) else (
    echo [OK]
)

set VULKAN_VERSION_REQUIRED=1.2.170.0
echo | set /p="Checking Vulkan: "
if not defined VULKAN_SDK (
    echo [Failed]
    echo cyb-engine requires Vulkan SDK %VULKAN_VERSION_REQUIRED% or later.
    echo Visit https://vulkan.lunarg.com/sdk/home/ to download the latest version.
    goto Error
) else (
    echo [OK]
)

:GenerateProjectFiles
echo Generate files
set PREMAKE_EXECUTABLE="%PREMAKE_PATH%/premake5.exe"
call %PREMAKE_EXECUTABLE% --verbose vs2022 || goto Error

popd
exit /b 0

:Error
popd
exit /b 1