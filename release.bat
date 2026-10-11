@echo off
cd /d "%~dp0"
set "OUT=out\build\release"
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
rem this PC's compiled shaders (GPU independent) as the ones a new player starts with; their own shaders.bin is left alone
mkdir "%OUT%\release\shadercache"
if exist shadercache\shaders.bin copy /y shadercache\shaders.bin "%OUT%\release\shadercache\base_shaders.bin" >nul
7z a "%ZIP%" ".\%OUT%\SCDA.exe" ".\%OUT%\SCDA_Versus.exe" ".\%OUT%\release\assets" ".\%OUT%\release\shadercache"
