@ECHO OFF
SETLOCAL
PUSHD ..\..

SET premake_dir=tools/premake
SET premake_zip_url="https://github.com/premake/premake-core/releases/download/v5.0.0-beta2/premake-5.0.0-beta2-windows.zip"

:CheckPremake
ECHO | SET /p="Checking Premake5: "
IF NOT EXIST "%premake_dir%/premake5.exe" (
    ECHO [Not Found]
    ECHO Downloading Premake5 to %premake_dir%...
    curl -OJL --create-dirs --output-dir %premake_dir% https://github.com/premake/premake-core/releases/download/v5.0.0-beta2/premake-5.0.0-beta2-windows.zip
    PUSHD "%premake_dir%"
    tar -xf premake-5.0.0-beta2-windows.zip
    del premake-5.0.0-beta2-windows.zip
    POPD
    GOTO CheckPremake
) ELSE (
    ECHO [OK]
)

SET required_vulkan_varsion="1.2.170.0"
ECHO | SET /p="Checking Vulkan: "
IF NOT DEFINED VULKAN_SDK (
    ECHO [Failed]
    ECHO cyb-engine requires Vulkan SDK {vulkanRequiredVersion} or later.
    ECHO Visit https://vulkan.lunarg.com/sdk/home/ to download the latest version.
    GOTO Exit
) ELSE (
    ECHO [OK]
)

:GenerateProjectFiles
ECHO Generate files
SET exec="%premake_dir%/premake5.exe"
%exec% --verbose vs2022

:Exit
PAUSE