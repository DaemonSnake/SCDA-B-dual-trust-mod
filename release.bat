@echo off
cd /d "%~dp0"
set "OUT=out\build\release"
set "D3DCOMPILER=%ProgramFiles(x86)%\Windows Kits\10\Redist\D3D\x64\d3dcompiler_47.dll"
if not exist "%D3DCOMPILER%" (echo d3dcompiler_47.dll not found: install the Windows SDK from the Visual Studio Installer & exit /b 1)
if not exist "%OUT%\SCDA.exe" call build.bat || exit /b 1
rem One name for every release, so releases/latest/download/SCDA-XBERecomp.zip stays a working link
set "ZIP=SCDA-XBERecomp.zip"
del /q "%ZIP%" 2>nul
rem an empty assets folder beside the executables, where the game (files or .iso) goes
rd /s /q "%OUT%\release" 2>nul
mkdir "%OUT%\release\assets"
rem the game's shadercache\ is a link to the one at the top (not in git): deleting out loses no shaders
if not exist shadercache mkdir shadercache
dir /AL /B "%OUT%" 2>nul | findstr /X /I shadercache >nul || (
    if exist "%OUT%\shadercache" (
        copy /y "%OUT%\shadercache\*" shadercache\ >nul
        rd /s /q "%OUT%\shadercache"
    )
    mklink /J "%OUT%\shadercache" "%CD%\shadercache" >nul
)
7z a "%ZIP%" ".\%OUT%\SCDA.exe" ".\%OUT%\SCDA_Versus.exe" ".\%OUT%\release\assets" "%D3DCOMPILER%"
