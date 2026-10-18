@echo off
setlocal enabledelayedexpansion
title Astrosis-C build

echo ================================================
echo  Astrosis-C - Windows build (native Win32 UI)
echo ================================================
echo.

REM --- if gcc is not on PATH, try the usual MinGW/MSYS2 spots ---
where gcc >nul 2>nul
if errorlevel 1 (
  if exist "C:\msys64\mingw64\bin\gcc.exe" set "PATH=C:\msys64\mingw64\bin;%PATH%"
)
if defined MINGW_HOME (
  if exist "%MINGW_HOME%\bin\gcc.exe" set "PATH=%MINGW_HOME%\bin;%PATH%"
)

where gcc >nul 2>nul
if errorlevel 1 (
  echo [ERROR] gcc was not found on PATH.
  echo.
  echo   Install MSYS2, open the "MSYS2 MINGW64" shell, then run:
  echo       pacman -S mingw-w64-x86_64-gcc
  echo   and run this build.bat again from that shell.
  echo.
  pause
  exit /b 1
)

echo Using compiler:
gcc --version
echo.

set CFLAGS=-std=c11 -O2 -Wall -Wextra -Iinclude
set SRCS=src\catalog.c src\physics.c src\kd_tree.c src\brute_force.c src\engine.c src\grid.c src\octree.c src\bvh.c src\scenes.c src\sim.c src\win32_ui.c src\main.c
set LIBS=-lcomctl32 -lgdi32 -luser32 -lm

echo [1/2] astro.exe      (console: GUI + --bench) ...
gcc %CFLAGS% %SRCS% -o astro.exe %LIBS%
if errorlevel 1 (
  echo.
  echo [ERROR] Build failed - see the compiler messages above.
  echo.
  pause
  exit /b 1
)

echo [2/2] astro_gui.exe  (windowed: no console window) ...
gcc %CFLAGS% %SRCS% -o astro_gui.exe -mwindows %LIBS%
if errorlevel 1 (
  echo.
  echo [ERROR] Build failed - see the compiler messages above.
  echo.
  pause
  exit /b 1
)

echo.
echo Done.
echo   astro_gui.exe          -> double-click, pure GUI window
echo   astro.exe              -> same GUI + --bench table in the console
echo   astro.exe --scene kessler
echo   astro.exe --bench --steps 300
echo.
pause
endlocal
