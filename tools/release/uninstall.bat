@echo off
rem Usage: uninstall.bat "Steam game folder name"   or   uninstall.bat "C:\path\to\game"
if "%~1"=="" (
  echo Usage: uninstall.bat "Expedition 33"
  exit /b 1
)
if exist "%~1\" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" -GameDir "%~1" -Uninstall
) else (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" -Game "%~1" -Uninstall
)
pause
