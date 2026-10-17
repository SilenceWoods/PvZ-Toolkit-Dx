@echo off
rem ---------------------------------------------------------------------------
rem Build GLFW 3.4 as a 32-bit static library with the i686 MinGW toolchain.
rem
rem Used by the new Dear ImGui frontend.  Only the Win32 backend is needed, so
rem no extra dependencies are required.
rem
rem Source, build tree and install prefix all live inside the repository and are
rem reached through the ASCII junction alias produced by provision.cmd.
rem
rem Usage:  env\scripts\build_glfw.cmd
rem ---------------------------------------------------------------------------
setlocal enabledelayedexpansion

set "HERE=%~dp0"
for /f "usebackq tokens=1,* delims==" %%a in (`"%HERE%provision.cmd"`) do (
    if /i "%%a"=="TC" set "TC=%%b"
    if /i "%%a"=="WS" set "WS=%%b"
)
if not defined TC ( echo ERROR: provision failed 1>&2 & exit /b 1 )

rem CMake cannot escape backslash paths in -D values, so pass forward slashes.
set "TCF=%TC:\=/%"
set "WSF=%WS:\=/%"

set "CMAKE=%TC%\bin\cmake.exe"
set "SRC=%WS%\env\src\glfw-3.4"
set "BLD=%WS%\env\glfw-build"
set "INS=%WS%\env\glfw32"
set "TGZ=%WS%\env\downloads\glfw-3.4.tar.gz"

rem Ninja cannot be used here (it captures child output through a pipe, which the
rem sandbox blocks).  Keep PATH minimal so CMake does not find an unrelated
rem sh.exe -- the MinGW Makefiles generator rejects that.
set "PATH=%TC%\bin;%SystemRoot%\system32;%SystemRoot%"

rem Extract once.
if not exist "%SRC%\CMakeLists.txt" (
    if not exist "%TGZ%" ( echo ERROR: missing %TGZ% & exit /b 1 )
    echo [glfw] extract
    if not exist "%WS%\env\src" mkdir "%WS%\env\src"
    "%SystemRoot%\system32\tar.exe" -xf "%TGZ%" -C "%WS%\env\src"
    if not exist "%SRC%\CMakeLists.txt" ( echo ERROR: extract failed & exit /b 1 )
)

rem Skip when already installed for this very alias path.
set "STAMP=%INS%\.pztk-alias"
if exist "%STAMP%" if exist "%INS%\lib\libglfw3.a" (
    set /p OLD_ALIAS=<"%STAMP%"
    if /i "!OLD_ALIAS!"=="%WSF%" (
        echo [glfw] already installed for this build path, skipping
        echo [glfw] done -^> %INS%
        exit /b 0
    )
)

echo [glfw] configure
"%CMAKE%" -G "MinGW Makefiles" -S "%SRC%" -B "%BLD%" ^
  -D CMAKE_BUILD_TYPE=MinSizeRel ^
  -D CMAKE_INSTALL_PREFIX="%WSF%/env/glfw32" ^
  -D CMAKE_MAKE_PROGRAM="%TCF%/bin/mingw32-make.exe" ^
  -D CMAKE_C_COMPILER="%TCF%/bin/gcc.exe" ^
  -D CMAKE_CXX_COMPILER="%TCF%/bin/g++.exe" ^
  -D CMAKE_RC_COMPILER="%TCF%/bin/windres.exe" ^
  -D CMAKE_C_FLAGS="-O2" ^
  -D BUILD_SHARED_LIBS=OFF ^
  -D GLFW_BUILD_EXAMPLES=OFF ^
  -D GLFW_BUILD_TESTS=OFF ^
  -D GLFW_BUILD_DOCS=OFF
if errorlevel 1 ( echo ERROR: glfw configure failed 1>&2 & exit /b 1 )

echo [glfw] build
"%CMAKE%" --build "%BLD%" --parallel
if errorlevel 1 ( echo ERROR: glfw build failed 1>&2 & exit /b 1 )

echo [glfw] install
"%CMAKE%" --install "%BLD%"
if errorlevel 1 ( echo ERROR: glfw install failed 1>&2 & exit /b 1 )

> "%STAMP%" echo %WSF%
echo [glfw] done -^> %INS%
exit /b 0
