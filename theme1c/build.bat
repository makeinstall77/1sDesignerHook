@echo off
setlocal
set "MSVC=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207"
set "PATH=%MSVC%\bin\Hostx64\x64;%PATH%"
set "INCLUDE=%MSVC%\include"
set "LIB="
cd /d "%~dp0"
lib /nologo /def:kernel32.def /machine:x64 /out:kernel32.lib
if errorlevel 1 exit /b 1
lib /nologo /def:user32.def /machine:x64 /out:user32.lib
if errorlevel 1 exit /b 1
lib /nologo /def:shell32.def /machine:x64 /out:shell32.lib
if errorlevel 1 exit /b 1
cl /nologo /O2 /Oi- /GS- /GR- /EHs-c- /W3 /utf-8 /LD cairo1c.cpp crtfix.cpp /link /NODEFAULTLIB /ENTRY:DllMain kernel32.lib /OUT:cairo1c8.dll
if errorlevel 1 exit /b 1
cl /nologo /O2 /Oi- /GS- /GR- /EHs-c- /W3 /utf-8 launcher.cpp crtfix.cpp /link /NODEFAULTLIB /ENTRY:wWinMain /SUBSYSTEM:WINDOWS kernel32.lib user32.lib shell32.lib /OUT:Theme1C.exe
if errorlevel 1 exit /b 1
echo BUILD_OK
