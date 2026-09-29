@echo off
rem Lets your partner connect to a game you host with Insaniquarium Co-op.
rem Adds one Windows Firewall rule for InsaniquariumCoop.exe in this folder.

net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Asking Windows for permission to change the firewall...
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

netsh advfirewall firewall delete rule name="Insaniquarium Co-op" >nul 2>&1
netsh advfirewall firewall add rule name="Insaniquarium Co-op" dir=in action=allow program="%~dp0InsaniquariumCoop.exe" enable=yes profile=any >nul
if %errorlevel% neq 0 (
    echo Something went wrong adding the firewall rule.
) else (
    echo Done! Insaniquarium Co-op can now accept your partner's connection.
)
echo.
pause
