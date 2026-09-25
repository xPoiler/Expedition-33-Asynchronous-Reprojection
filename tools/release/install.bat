@echo off
rem Usage: install.bat "Steam game folder name"   or   install.bat "C:\path\to\game"
rem Put nvngx_latewarp.dll next to this file first (see README.md).
if "%~1"=="" (
  echo Usage: install.bat "Expedition 33"
  echo    or: install.bat "D:\Games\Some Game"
  exit /b 1
)
if exist "%~1\" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" -GameDir "%~1" -Latewarp "%~dp0nvngx_latewarp.dll"
) else (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" -Game "%~1" -Latewarp "%~dp0nvngx_latewarp.dll"
)
pause
