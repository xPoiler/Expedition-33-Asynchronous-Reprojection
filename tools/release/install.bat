@echo off
rem Double-click to pick a game from a list, or: install.bat "Expedition 33"  /  install.bat "D:\Games\Some Game"
rem Put nvngx_latewarp.dll next to this file first (see README.md).
title FrameWarp installer
if "%~1"=="" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" -Latewarp "%~dp0nvngx_latewarp.dll"
) else if exist "%~1\" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" -GameDir "%~1" -Latewarp "%~dp0nvngx_latewarp.dll"
) else (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" -Game "%~1" -Latewarp "%~dp0nvngx_latewarp.dll"
)
pause
