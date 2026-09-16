@echo off
setlocal EnableExtensions

set "PORT_A=%~1"
set "PORT_B=%~2"
if not defined PORT_A set "PORT_A=COM10"
if not defined PORT_B set "PORT_B=COM11"

net session >nul 2>&1
if not "%ERRORLEVEL%"=="0" (
    echo Run this script from an elevated Command Prompt.
    exit /b 1
)

set "SETUPC=%ProgramFiles(x86)%\com0com\setupc.exe"
if not exist "%SETUPC%" set "SETUPC=%ProgramFiles%\com0com\setupc.exe"
if not exist "%SETUPC%" call :install_com0com
if not "%ERRORLEVEL%"=="0" exit /b 1

if not exist "%SETUPC%" (
    echo com0com was not installed or setupc.exe was not found.
    exit /b 1
)

echo Creating the virtual null-modem pair %PORT_A% ^<--^> %PORT_B%...
"%SETUPC%" install PortName=%PORT_A% PortName=%PORT_B%
if not "%ERRORLEVEL%"=="0" (
    echo Could not create the pair. Choose unused COM numbers and try again.
    exit /b 1
)

echo.
echo Current virtual pairs:
"%SETUPC%" list
echo.
echo Start com_app.exe twice. Select %PORT_A% in one window and %PORT_B% in the other.
echo Select the same stop-bit setting in both windows.
exit /b 0

:install_com0com
where curl.exe >nul 2>&1
if not "%ERRORLEVEL%"=="0" (
    echo curl.exe is required to download com0com.
    exit /b 1
)

set "CACHE_DIR=%~dp0.cache\virtual-ports"
if not exist "%CACHE_DIR%" mkdir "%CACHE_DIR%"
set "INSTALLER=%CACHE_DIR%\com0com-setup.exe"

echo Downloading the com0com signed installer...
curl.exe --fail --location --retry 3 --output "%INSTALLER%" ^
    "https://sourceforge.net/projects/signed-drivers/files/com0com/v3.0/Setup_com0com_v3.0.0.0_W7_x64_signed.exe/download"
if not "%ERRORLEVEL%"=="0" exit /b 1

echo Installing com0com. Accept the Windows driver prompt if it appears.
start "" /wait "%INSTALLER%"
if not "%ERRORLEVEL%"=="0" exit /b 1

set "SETUPC=%ProgramFiles(x86)%\com0com\setupc.exe"
if not exist "%SETUPC%" set "SETUPC=%ProgramFiles%\com0com\setupc.exe"
exit /b 0
