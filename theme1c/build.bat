@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

REM --- Locate Visual Studio / call vcvars64.bat (SDK + CRT for Theme1C.exe) ---
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VCVARS="
if exist "%VSWHERE%" (
  for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do (
    if exist "%%I\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%%I\VC\Auxiliary\Build\vcvars64.bat"
  )
)

if defined VCVARS (
  echo Using vcvars: !VCVARS!
  call "!VCVARS!"
  if errorlevel 1 (
    echo ERROR: vcvars64.bat failed
    exit /b 1
  )
) else (
  REM Fallback: hard-coded MSVC tools only (DLL may build; EXE needs Windows SDK)
  set "MSVC=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207"
  echo vswhere/vcvars not found - fallback MSVC: !MSVC!
  if not exist "!MSVC!\bin\Hostx64\x64\cl.exe" (
    echo ERROR: cl.exe not found. Install VS Build Tools with C++ x64 tools.
    exit /b 1
  )
  set "PATH=!MSVC!\bin\Hostx64\x64;!PATH!"
  set "INCLUDE=!MSVC!\include"
)

where cl >nul 2>&1
if errorlevel 1 (
  echo ERROR: cl.exe not on PATH
  exit /b 1
)

REM --- Stub import lib for freestanding DLL (explicit OpenFileMappingW / MapViewOfFile / …) ---
lib /nologo /def:kernel32.def /machine:x64 /out:kernel32.lib
if errorlevel 1 exit /b 1

REM --- cairo1c8.dll: freestanding, no CRT ---
cl /nologo /O2 /Oi- /GS- /GR- /EHs-c- /W3 /utf-8 /LD cairo1c.cpp crtfix.cpp /link /NODEFAULTLIB /ENTRY:DllMain "%~dp0kernel32.lib" /OUT:cairo1c8.dll
if errorlevel 1 exit /b 1

REM --- Theme1C.exe: tray host WITH CRT (do not compile launcher.cpp) ---
cl /nologo /O2 /W3 /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE ^
  tray_main.cpp inject.cpp settings_ui.cpp onec_paths.cpp ^
  theme_ir.cpp appdata.cpp config.cpp theme_convert.cpp theme_state_host.cpp ^
  /link /SUBSYSTEM:WINDOWS /OUT:Theme1C.exe ^
  user32.lib shell32.lib gdi32.lib comctl32.lib advapi32.lib ole32.lib comdlg32.lib
if errorlevel 1 exit /b 1

echo BUILD_OK
endlocal
exit /b 0
