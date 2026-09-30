@echo off
cd /d "%~dp0"
set "OUT=out\build\win-amd64-release"
if not exist "%OUT%\SCDAOffline.exe" call build.bat || exit /b 1
del /q SCDARecomp.zip 2>nul
rem an empty assets folder beside the executables, where the game (files or .iso) goes
rd /s /q "%OUT%\release" 2>nul
mkdir "%OUT%\release\assets"
7z a SCDARecomp.zip ".\%OUT%\SCDAOffline.exe" ".\%OUT%\SCDAOnline.exe" ".\%OUT%\release\assets"
