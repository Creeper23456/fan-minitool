@echo off
setlocal EnableExtensions

set "ROOT=%~dp0"
set "OUT=%ROOT%build"

if not defined VSCMD_VER (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if not exist "%VSWHERE%" (
        echo Could not find vswhere.exe. Run this script from a Visual Studio Developer Command Prompt.
        exit /b 1
    )
    for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        call "%%I\Common7\Tools\VsDevCmd.bat" -arch=x64
    )
)

if not defined VSCMD_VER (
    echo Visual Studio C++ tools were not initialized.
    exit /b 1
)

if not exist "%OUT%" mkdir "%OUT%"
cl /nologo /std:c17 /utf-8 /W4 /WX /O2 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /D_WIN32_WINNT=0x0A00 /D_CRT_SECURE_NO_WARNINGS /wd4055 /Fe:"%OUT%\fan-minitool.exe" /Fo:"%OUT%\" "%ROOT%src\main.c" "%ROOT%src\service.c" "%ROOT%src\config.c" "%ROOT%src\controller.c" "%ROOT%src\lhm.c" "%ROOT%src\nvapi.c" "%ROOT%src\log.c" Advapi32.lib Winhttp.lib
if errorlevel 1 exit /b %errorlevel%
copy /Y "%ROOT%fan-minitool.ini.example" "%OUT%\fan-minitool.ini" >nul
echo Built %OUT%\fan-minitool.exe
