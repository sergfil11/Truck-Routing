@echo off
setlocal
cd /d "%~dp0.."
cmake --preset cpsat %*
if errorlevel 1 exit /b 1
cmake --build --preset cpsat --parallel
exit /b %errorlevel%
