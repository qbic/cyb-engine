@ECHO OFF
SETLOCAL
PUSHD ..\..

SET PremakePath=tools/premake
SET PremakeZipURL="https://github.com/premake/premake-core/releases/download/v5.0.0-beta2/premake-5.0.0-beta2-windows.zip"

:CheckPremake
ECHO | SET /p="Checking Premake5: "
IF NOT EXIST "%PremakePath%/premake5.exe" (
    ECHO [Not Found]
    ECHO Downloading Premake5 to %PremakePath%...
    curl -OJL --create-dirs --output-dir %PremakePath% %PremakeZipURL%
    PUSHD "%PremakePath%"
    tar -xf premake-5.0.0-beta2-windows.zip
    DEL premake-5.0.0-beta2-windows.zip
    POPD
    GOTO CheckPremake
) ELSE (
    ECHO [OK]
)

SET RequiredVulkanVersion=1.2.170.0
ECHO | SET /p="Checking Vulkan: "
IF NOT DEFINED VULKAN_SDK (
    ECHO [Failed]
    ECHO cyb-engine requires Vulkan SDK %RequiredVulkanVersion% or later.
    ECHO Visit https://vulkan.lunarg.com/sdk/home/ to download the latest version.
    GOTO Exit
) ELSE (
    ECHO [OK]
)

:GenerateProjectFiles
ECHO Generate files
SET PremakeExe="%PremakePath%/premake5.exe"
%PremakeExe% --verbose vs2022

:Exit
PAUSE