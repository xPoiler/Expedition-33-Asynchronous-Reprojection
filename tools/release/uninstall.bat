@echo off
rem Double-click to pick a game from a list, or: uninstall.bat "Expedition 33"  /  uninstall.bat "D:\Games\Some Game"
title FrameWarp uninstaller
if "%~1"=="" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" -Uninstall
) else if exist "%~1\" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" -GameDir "%~1" -Uninstall
) else (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" -Game "%~1" -Uninstall
)
pause
