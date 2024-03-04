#!/usr/bin/env python3
import os
import subprocess
import platform
from urllib.request import urlretrieve
from pathlib import Path
import zipfile
import tarfile

if (platform.system() == 'Windows'):
    PREMAKE_URL = 'https://github.com/premake/premake-core/releases/download/v5.0.0-beta2/premake-5.0.0-beta2-windows.zip'
    PREMAKE_FILENAME = 'premake-5.0.0-beta2-windows.zip'
    PREMAKE_DIR = './tools/premake'
    PREMAKE_BIN = f'{PREMAKE_DIR}/premake5.exe'
    PREMAKE_TARGET = 'vs2022'
    VULKAN_SDK = os.environ.get('VULKAN_SDK')
    UNPACK_FUNCTION = zipfile.ZipFile
elif  (platform.system() == 'Linux'):
    PREMAKE_URL = 'https://github.com/premake/premake-core/releases/download/v5.0.0-beta2/premake-5.0.0-beta2-linux.tar.gz'
    PREMAKE_FILENAME = 'premake-5.0.0-beta2-linux.tar.gz'
    PREMAKE_DIR = './tools/premake'
    PREMAKE_BIN = f'{PREMAKE_DIR}/premake5'
    PREMAKE_TARGET = 'gmake'
    VULKAN_SDK = os.environ.get('VULKAN_SDK')
    UNPACK_FUNCTION = tarfile.open
else:
    print(f'{platform.system()} is currently not supported by cyb-engine')
    exit(1)

VULKAN_VERSION_REQUIRED = '1.2.170.0'

def download_hook(blocknum, blocksize, totalsize) -> None:
    bytesread = blocknum * blocksize
    if totalsize > 0:
        percent = min(100, bytesread * 1e2 / totalsize)
        print(f'\r{percent:.1f}% ({min(bytesread, totalsize)} / {totalsize} bytes)', end='')
        if bytesread >= totalsize:
            print()

def has_premake() -> bool:
    return Path(PREMAKE_BIN).exists()

def install_premake() -> None:
    print(f'Downloading {PREMAKE_URL}...')
    urlretrieve(PREMAKE_URL, PREMAKE_FILENAME, download_hook)
    print(f'Extracting {PREMAKE_FILENAME} premake to {PREMAKE_DIR}')
    with UNPACK_FUNCTION(PREMAKE_FILENAME) as archive:
        archive.extractall(PREMAKE_DIR)   
    os.remove(PREMAKE_FILENAME)

def has_vulkan_sdk(verbose=False) -> bool:
    if (VULKAN_SDK is None):
        if verbose:
            print('Vulkan SDK not installed correctly.')
            print('Ensure evironment variable VULKAN_SDK is properly set.')
        return False
    
    vulkan_version = VULKAN_SDK.split('\\')[-1]
    required_major, required_minor = VULKAN_VERSION_REQUIRED.split(".")[:2]
    installed_major, installed_minor = vulkan_version.split(".")[:2]
    if (not (installed_major == required_major and installed_minor >= required_minor)):
        if verbose:
            print(f'Old version of vulkan detected ({VULKAN_SDK}), cyb-engine requires vulkan sdk version {VULKAN_VERSION_REQUIRED} or later.')
        return False
    return True

if __name__ == '__main__':
     # change working directory to cyb-engine root path
    os.chdir('./../')
    success = True

    if (not has_premake()):
        install_premake()
        if (not has_premake()):
            success = False
    print(f'Checking Premake [{"OK" if has_premake() else "FAILED"}]')

    print(f'Checking Vulkan SDK [{"OK" if has_vulkan_sdk() else "FAILED"}]')
    if (not has_vulkan_sdk(True)):
        print('Check out https://vulkan.lunarg.com/sdk/home/ to download the latest version.')
        success = False

    if (not success):
        print('Setup failed, see output for details.')
        exit(1)

    subprocess.call([os.path.abspath(PREMAKE_BIN), '--verbose', PREMAKE_TARGET])
