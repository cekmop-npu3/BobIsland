@echo off
setlocal EnableExtensions

pushd "%~dp0" || exit /b 1
set "CACHE_DIR=%CD%\.cache\setup"
set "TOOLS_DIR=%CD%\.cache\tools"
if not exist "%CACHE_DIR%" mkdir "%CACHE_DIR%"
if not exist "%TOOLS_DIR%" mkdir "%TOOLS_DIR%"

call :add_known_paths
call :ensure_msvc
if errorlevel 1 goto :failure
call :ensure_cmake
if errorlevel 1 goto :failure
call :ensure_ninja
if errorlevel 1 goto :failure
call :ensure_llvm
if errorlevel 1 goto :failure

cmake --preset debug
if errorlevel 1 goto :failure
cmake --build --preset debug
if errorlevel 1 goto :failure

echo.
echo Setup complete.
echo Compilation database: build\debug\compile_commands.json
popd
exit /b 0

:ensure_msvc
call :activate_msvc
if not errorlevel 1 exit /b 0

echo Installing MSVC Build Tools with winget...
where winget.exe >nul 2>&1
if not errorlevel 1 (
    winget.exe install --exact --id Microsoft.VisualStudio.2022.BuildTools ^
        --accept-source-agreements --accept-package-agreements ^
        --disable-interactivity ^
        --override "--wait --passive --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
)
if errorlevel 1 call :download_msvc
if errorlevel 1 exit /b 1

call :activate_msvc
if not errorlevel 1 exit /b 0
call :download_msvc
if errorlevel 1 exit /b 1
call :activate_msvc
exit /b %ERRORLEVEL%

:ensure_cmake
where cmake.exe >nul 2>&1
if not errorlevel 1 exit /b 0

echo Installing CMake with winget...
call :winget_install Kitware.CMake
if errorlevel 1 call :download_cmake
if errorlevel 1 exit /b 1
call :add_known_paths
where cmake.exe >nul 2>&1
if not errorlevel 1 exit /b 0
call :download_cmake
if errorlevel 1 exit /b 1
call :add_known_paths
where cmake.exe >nul 2>&1
exit /b %ERRORLEVEL%

:ensure_ninja
where ninja.exe >nul 2>&1
if not errorlevel 1 exit /b 0

echo Installing Ninja with winget...
call :winget_install Ninja-build.Ninja
if errorlevel 1 call :download_ninja
if errorlevel 1 exit /b 1
call :add_known_paths
where ninja.exe >nul 2>&1
if not errorlevel 1 exit /b 0
call :download_ninja
if errorlevel 1 exit /b 1
call :add_known_paths
where ninja.exe >nul 2>&1
exit /b %ERRORLEVEL%

:ensure_llvm
where clangd.exe >nul 2>&1
if not errorlevel 1 exit /b 0

echo Installing LLVM tools for clangd with winget...
call :winget_install LLVM.LLVM
if errorlevel 1 call :download_llvm
if errorlevel 1 exit /b 1
call :add_known_paths
where clangd.exe >nul 2>&1
if not errorlevel 1 exit /b 0
call :download_llvm
if errorlevel 1 exit /b 1
call :add_known_paths
where clangd.exe >nul 2>&1
exit /b %ERRORLEVEL%

:winget_install
where winget.exe >nul 2>&1 || exit /b 1
winget.exe install --exact --id "%~1" --accept-source-agreements ^
    --accept-package-agreements --disable-interactivity
exit /b %ERRORLEVEL%

:download_msvc
call :download "https://aka.ms/vs/17/release/vs_buildtools.exe" ^
    "%CACHE_DIR%\vs_buildtools.exe"
if errorlevel 1 exit /b 1
start "" /wait "%CACHE_DIR%\vs_buildtools.exe" --quiet --wait --norestart ^
    --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended
exit /b %ERRORLEVEL%

:download_cmake
call :download "https://cmake.org/files/v4.4/cmake-4.4.3-windows-x86_64.zip" ^
    "%CACHE_DIR%\cmake.zip"
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -Command ^
    "Expand-Archive -LiteralPath '%CACHE_DIR%\cmake.zip' -DestinationPath '%TOOLS_DIR%\cmake' -Force"
if errorlevel 1 exit /b 1
for /d %%D in ("%TOOLS_DIR%\cmake\cmake-*") do set "PATH=%%~fD\bin;%PATH%"
exit /b 0

:download_ninja
call :download "https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-win.zip" ^
    "%CACHE_DIR%\ninja.zip"
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -Command ^
    "Expand-Archive -LiteralPath '%CACHE_DIR%\ninja.zip' -DestinationPath '%TOOLS_DIR%\ninja' -Force"
if errorlevel 1 exit /b 1
set "PATH=%TOOLS_DIR%\ninja;%PATH%"
exit /b 0

:download_llvm
call :download "https://github.com/llvm/llvm-project/releases/download/llvmorg-23.1.1/LLVM-23.1.1-win64.exe" ^
    "%CACHE_DIR%\llvm.exe"
if errorlevel 1 exit /b 1
start "" /wait "%CACHE_DIR%\llvm.exe" /S
exit /b %ERRORLEVEL%

:download
where curl.exe >nul 2>&1 || exit /b 1
echo Downloading %~1
curl.exe --fail --location --retry 3 --output "%~2" "%~1"
exit /b %ERRORLEVEL%

:add_known_paths
for %%D in ("%ProgramFiles%\CMake\bin" "%ProgramFiles%\Ninja" "%ProgramFiles%\LLVM\bin") do (
    if exist "%%~D" set "PATH=%%~D;%PATH%"
)
exit /b 0

:activate_msvc
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" exit /b 1
set "VS_ROOT="
for /f "usebackq delims=" %%D in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_ROOT=%%D"
if not defined VS_ROOT exit /b 1
call "%VS_ROOT%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
where cl.exe >nul 2>&1
exit /b %ERRORLEVEL%

:failure
echo.
echo Setup failed. Install the reported dependency, then run setup.bat again.
popd
exit /b 1
