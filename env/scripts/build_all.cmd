@echo off
rem ---------------------------------------------------------------------------
rem One-shot build entry point for THIS source package.
rem
rem What this package builds
rem   build\pztk_imgui.exe   the Dear ImGui frontend (the current main program)
rem   build\pztk_hook.dll    the injected component (must sit next to the exe)
rem
rem   There is NO FLTK frontend step here: upstream's chain
rem   build_fltk.cmd -> build_toolkit.cmd (= pvztoolkit.exe) does not exist in
rem   this package and is never called.  The FLTK source tree is used as a zlib
rem   SOURCE only, and only when that zlib is missing (step 1).
rem
rem Steps
rem   0. provision.cmd         ASCII path alias + i686 toolchain copy
rem   1. build_zlib.cmd        ONLY when env\fltk32\lib\libfltk_z.a is missing
rem   2. build_glfw.cmd        GLFW 3.4 static library -> env\glfw32
rem   3. build_imgui_demo.cmd  ImGui frontend         -> build\pztk_imgui.exe
rem   4. build_hook_dll.cmd    injected DLL           -> build\pztk_hook.dll
rem
rem Every step is checked: the first failure prints an ERROR: line and stops
rem this script with exit code 1.  Safe to re-run - steps 1..4 are incremental
rem and step 1 is skipped while libfltk_z.a is already there (delete that file
rem to force a zlib rebuild).
rem
rem Keep this file PURE ASCII: cmd.exe reads .cmd as ANSI, so a UTF-8 Chinese
rem comment can swallow the line break and corrupt the rest of the script.
rem
rem Usage:  env\scripts\build_all.cmd
rem         set PZTK_BUILD_ROOT=<dir>   build in a workspace root of your own
rem                                     (default %TEMP%\pztk; provision.cmd,
rem                                      which every step below goes through,
rem                                      is what reads it - no option needed)
rem ---------------------------------------------------------------------------
setlocal

set "HERE=%~dp0"

rem ---------------------------------------------------------------------------
rem 0) provision.  Creates <root>\mingw32 (a real copy on an ASCII path) and
rem    <root>\ws (junction to this package), then prints ROOT= / TC= / WS=.
rem    Those lines are parsed out exactly the way the other build scripts in
rem    this directory do it.  provision's own errors go to stderr and stay
rem    visible; a missing TC=/WS= line becomes an explicit error here instead
rem    of a confusing "not defined" cascade further down.
rem ---------------------------------------------------------------------------
set "TC="
set "WS="
echo [all] step 0/4: provision
for /f "usebackq tokens=1,* delims==" %%a in (`"%HERE%provision.cmd"`) do (
    if /i "%%a"=="ROOT" echo       ROOT=%%b
    if /i "%%a"=="TC" set "TC=%%b"
    if /i "%%a"=="WS" set "WS=%%b"
)
if not defined TC (
    echo ERROR: provision failed - it printed no TC= line 1>&2
    echo        fix the messages printed above, then run this script again 1>&2
    exit /b 1
)
if not defined WS (
    echo ERROR: provision failed - it printed no WS= line 1>&2
    echo        fix the messages printed above, then run this script again 1>&2
    exit /b 1
)
if not exist "%WS%\tool-src\CMakeLists.txt" (
    echo ERROR: workspace alias is not usable: %WS% 1>&2
    echo        expected to find %WS%\tool-src\CMakeLists.txt 1>&2
    exit /b 1
)

rem ---------------------------------------------------------------------------
rem 1) zlib - only when the archive is missing.  Both targets link
rem    -L env\fltk32\lib -lfltk_z, so without it they stop early.
rem    build_zlib.cmd takes the toolchain and the workspace alias as arguments,
rem    so it does not provision (and print) a second time.
rem ---------------------------------------------------------------------------
set "ZLIB=%WS%\env\fltk32\lib\libfltk_z.a"
if exist "%ZLIB%" (
    echo [all] step 1/4: zlib already present, skipping -^> %ZLIB%
) else (
    echo [all] step 1/4: zlib missing - building it out of the FLTK source tree
    call "%HERE%build_zlib.cmd" "%TC%" "%WS%"
    if errorlevel 1 (
        echo ERROR: zlib build failed - see the [zlib] lines above 1>&2
        echo        source needed: env\src\fltk-1.4.5\zlib 1>&2
        echo        FLTK is a zlib SOURCE here - its frontend is not built 1>&2
        echo        get it with: python env\scripts\download_deps.py 1>&2
        exit /b 1
    )
    if not exist "%ZLIB%" (
        echo ERROR: zlib build reported success but the archive is missing: 1>&2
        echo        %ZLIB% 1>&2
        exit /b 1
    )
    echo [all] step 1/4: zlib built -^> %ZLIB%
)

rem ---------------------------------------------------------------------------
rem 2) GLFW
rem ---------------------------------------------------------------------------
echo [all] step 2/4: glfw
call "%HERE%build_glfw.cmd"
if errorlevel 1 (
    echo ERROR: glfw build failed - see the [glfw] lines above 1>&2
    exit /b 1
)
if not exist "%WS%\env\glfw32\lib\libglfw3.a" (
    echo ERROR: glfw build reported success but the library is missing: 1>&2
    echo        %WS%\env\glfw32\lib\libglfw3.a 1>&2
    exit /b 1
)

rem ---------------------------------------------------------------------------
rem 3) ImGui frontend
rem ---------------------------------------------------------------------------
echo [all] step 3/4: imgui frontend
call "%HERE%build_imgui_demo.cmd"
if errorlevel 1 (
    echo ERROR: imgui frontend build failed - see the [imgui] lines above 1>&2
    exit /b 1
)
if not exist "%WS%\build\pztk_imgui.exe" (
    echo ERROR: imgui build reported success but the exe is missing: 1>&2
    echo        %WS%\build\pztk_imgui.exe 1>&2
    exit /b 1
)

rem ---------------------------------------------------------------------------
rem 4) injected DLL
rem ---------------------------------------------------------------------------
echo [all] step 4/4: hook dll
call "%HERE%build_hook_dll.cmd"
if errorlevel 1 (
    echo ERROR: hook dll build failed - see the [hook] lines above 1>&2
    exit /b 1
)
if not exist "%WS%\build\pztk_hook.dll" (
    echo ERROR: hook build reported success but the dll is missing: 1>&2
    echo        %WS%\build\pztk_hook.dll 1>&2
    exit /b 1
)

echo.
echo BUILD ALL OK
echo   %WS%\build\pztk_imgui.exe
echo   %WS%\build\pztk_hook.dll
echo   ^(keep the exe and the dll in the same directory^)
endlocal & exit /b 0
