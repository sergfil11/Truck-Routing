@echo off
setlocal
cd /d "%~dp0.."
cmake --preset gurobi %*
if errorlevel 1 exit /b 1
cmake --build --preset gurobi --parallel
exit /b %errorlevel%
