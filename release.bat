@echo off
cd /d "%~dp0"
set "OUT=out\build\win-amd64-release"
if not exist "%OUT%\SCDA.exe" call build.bat || exit /b 1
del /q SCDARecomp.zip 2>nul
7z a SCDARecomp.zip ".\%OUT%\SCDA.exe" .\README.md
