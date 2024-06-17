@echo off
pushd "%~dp0"/..

set PREMAKE_DIR=tools/premake
set PREMAKE_URL="https://github.com/premake/premake-core/releases/download/v5.0.0-beta2/premake-5.0.0-beta2-windows.zip"
set PREMAKE_EXE="%PREMAKE_DIR%/premake5.exe"

:CheckPremake
echo | set /p="Checking Premake5: "
if not exist "%PREMAKE_EXE%" (
    echo [Not Found]
    echo Downloading Premake5 to %PREMAKE_DIR%...
    curl -OJL --create-dirs --output-dir %PREMAKE_DIR% %PREMAKE_URL%
    pushd "%PREMAKE_DIR%"
    tar -xf premake-5.0.0-beta2-windows.zip
    del premake-5.0.0-beta2-windows.zip
    popd
    goto CheckPremake
) else (
    echo [OK]
)

:CheckVulkan
set VULKAN_VERSION_REQUIRED=1.2.170.0
set VULKAN_SDK_URL="https://sdk.lunarg.com/sdk/download/latest/windows/vulkan-sdk.exe"
echo | set /p="Checking Vulkan SDK: "
if not defined VULKAN_SDK (
    echo [Failed]
    echo cyb-engine requires Vulkan SDK %VULKAN_VERSION_REQUIRED% or later.
    set /p "answer=Would you like to download and install the latest vulkan sdk? [Y/n]: "
    if not "%answer%" == "n" (
        curl -OJL %VULKAN_SDK_URL%
        goto CheckVulkan
    )
    else (
        goto Error
    )
) else (
    echo [OK]
)

:GenerateProjectFiles
echo Generate files
set PREMAKE_EXE="%PREMAKE_DIR%/premake5.exe"
call %PREMAKE_EXE% --verbose vs2022 || goto Error

popd
exit /b 0

:Error
popd
exit /b 1