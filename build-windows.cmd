@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-windows.ps1" %*
if errorlevel 1 (
  echo.
  echo Build failed. See the error above and WINDOWS-BUILD.ko.md.
  pause
  exit /b 1
)
echo.
pause
