@echo off
rem Builds Insaniquarium Co-op from this folder (downloads its build tools the first time).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\windows\build.ps1" %*
echo.
pause
