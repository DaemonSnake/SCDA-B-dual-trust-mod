@echo off
cd /d "%~dp0"
set "OUT=out\build\release"
if not exist "%OUT%\SCDA.exe" call build.bat || exit /b 1
del /q SCDARecomp.zip 2>nul
rem an empty assets folder beside the executables, where the game (files or .iso) goes
rd /s /q "%OUT%\release" 2>nul
mkdir "%OUT%\release\assets"
7z a SCDARecomp.zip ".\%OUT%\SCDA.exe" ".\%OUT%\SCDA_Versus.exe" ".\%OUT%\release\assets"
