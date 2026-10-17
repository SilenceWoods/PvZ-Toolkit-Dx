@echo off
rem ---------------------------------------------------------------------------
rem Build the Dear ImGui + GLFW frontend prototype (32-bit).
rem
rem Full link recipe discovered while wiring the toolkit core into ImGui:
rem   * core sources      : pvz/process/data/code/lineup/utils/names/pak.cpp
rem                         (pak.cpp is used by the misc page unpack/pack)
rem                         KEEP THIS FILE ASCII-ONLY: cmd.exe reads .cmd as ANSI, so a
rem                         UTF-8 Chinese comment can swallow the line break and corrupt
rem                         the rest of the script (fragments get executed as commands).
rem   * FLTK dependency   : ONLY libfltk_z.a (84 KB) -- for lineup.h's zlib usage.
rem                         libfltk.a (2.5 MB) is NOT needed.
rem   * -idirafter        : REQUIRED. Our own process.h would otherwise shadow the
rem                         system <process.h> and produce bizarre compile errors.
rem   * -DUNICODE -D_UNICODE : utils.cpp uses CreateFileW-style APIs.
rem   * -D_PTK_CHINESE_UI    : names.cpp picks plants_zh/zombies_zh instead of the
rem                           English tables.  Without it the core helpers
rem                           PlantTypeName/ZombieTypeName return English, which is
rem                           why the two live monitors used to show English names.
rem   * libs              : glfw3 opengl32 gdi32 user32 comdlg32 ole32 shell32
rem                         winmm advapi32 wintrust crypt32 fltk_z
rem     (wintrust/crypt32 come from utils.cpp's signature verification)
rem
rem   * resources         : tool-src/res/version_imgui.rc is compiled with the
rem                         toolchain's windres and linked in, which is what gives
rem                         this exe its icon (RT_GROUP_ICON + 13 RT_ICON images)
rem                         and its RT_VERSION (VERSIONINFO) block.  The icon is
rem                         the pre-existing artwork tool-src/res/logo.ico -- the
rem                         exact same file pvztoolkit.exe embeds via
rem                         tool-src/res/version.rc -- so nothing new is drawn.
rem
rem Three windres details are load bearing here:
rem   (a) -O coff.  g++ only accepts a real COFF object as a link input; windres'
rem       default .res output is rejected by ld as "not an object".
rem   (b) the working directory must be tool-src/res.  version_imgui.rc names the
rem       icon as "logo.ico", a relative path resolved against the current
rem       directory, not against the .rc file's own directory.  Running windres
rem       from the repo root fails with "logo.ico: No such file or directory".
rem   (c) the .rc must NOT embed a manifest.  GCC's linker spec contains
rem       %:if-exists(default-manifest.o%s), so every non-shared link already
rem       pulls in the toolchain's default-manifest.o (installed by
rem       build_toolkit.cmd with activeCodePage=UTF-8).  Adding a second one
rem       aborts the link with ".rsrc merge failure: multiple non-default
rem       manifests" -- which is why version_imgui.rc has no MANIFEST statement
rem       and this build keeps the manifest exactly as it was before.
rem
rem       VERIFIED 2026-10-10: the toolchain object really does carry this
rem       project's manifest again -- 1322 bytes, activeCodePage=UTF-8, read
rem       back out of the linked exe with LoadLibraryExW(AS_DATAFILE) +
rem       FindResource/SizeofResource/LoadResource (RT_MANIFEST = type 24).
rem       It is NOT guaranteed by this script though: the manifest content
rem       lives in the toolchain copy, so if
rem       %TC%\i686-w64-mingw32\lib\default-manifest.o ever reverts to the
rem       factory one (a fresh extract of env\mingw32 followed by a
rem       provision.cmd rebuild does exactly that), this exe silently loses
rem       activeCodePage again.  Check and repair per HANDOVER.md 3.6 before
rem       trusting a release build.
rem
rem Resource compilation is best effort: if windres, the .rc file or the icon
rem header include is missing, the script warns and still links the frontend, so
rem the previously working (resource-less) build can never be broken by it.
rem
rem ---------------------------------------------------------------------------
rem REPRODUCIBLE BUILD (2026-10-10)
rem
rem Upstream read the About box's build time from the compiler's __DATE__ and
rem __TIME__, and ld stamped the current time into the PE header -- so two
rem builds of identical sources produced two different binaries and a release
rem could not be verified by hash.  Both sources of variation are gone now:
rem
rem   * the stamp is resolved below and passed in as -DPZTK_BUILD_DATE, which
rem     imgui_main.cpp uses instead of __DATE__/__TIME__ (see its file header);
rem   * the link line carries -Wl,--no-insert-timestamp, so the PE header
rem     TimeDateStamp is zero.
rem
rem Stamp priority (first one that is set wins):
rem   1. PZTK_BUILD_DATE   -- e.g.  set PZTK_BUILD_DATE=2026-10-11
rem   2. SOURCE_DATE_EPOCH -- unix seconds, converted to UTC YYYY-MM-DD
rem   3. the newest date in tool-src\CHANGELOG.md (the derived default)
rem Set PZTK_BUILD_DATE when you deliberately want a real build timestamp.
rem Result: running this script twice in a row yields byte-identical output.
rem ---------------------------------------------------------------------------
rem
rem Usage:  env\scripts\build_imgui_demo.cmd [output-exe-name]
rem         (default pztk_imgui.exe; pass another name when that file is in use,
rem          e.g. pztk_imgui_next.exe -- a running exe cannot be overwritten)
rem         set PZTK_BUILD_DATE=YYYY-MM-DD to override the build stamp
rem ---------------------------------------------------------------------------
setlocal enabledelayedexpansion

set "HERE=%~dp0"
for /f "usebackq tokens=1,* delims==" %%a in (`"%HERE%provision.cmd"`) do (
    if /i "%%a"=="TC" set "TC=%%b"
    if /i "%%a"=="WS" set "WS=%%b"
)
if not defined TC ( echo ERROR: provision failed 1>&2 & exit /b 1 )

set "GXX=%TC%\bin\g++.exe"
set "WINDRES=%TC%\bin\windres.exe"
set "PATH=%TC%\bin;%SystemRoot%\system32;%SystemRoot%"

set "IMGUI=%WS%\env\src\imgui-1.91.8-docking"
set "GLFW=%WS%\env\glfw32"
set "FLTK=%WS%\env\fltk32"
set "SRC=%WS%\tool-src\src"
set "FE=%WS%\tool-src\frontend"
set "INC=%WS%\tool-src\inc"
set "RES=%WS%\tool-src\res"
set "OUTNAME=pztk_imgui.exe"
if not "%~1"=="" set "OUTNAME=%~1"
set "OUT=%WS%\build\%OUTNAME%"

if not exist "%IMGUI%\imgui.cpp" ( echo ERROR: run download_deps.py first & exit /b 1 )
if not exist "%GLFW%\lib\libglfw3.a" ( echo ERROR: run build_glfw.cmd first & exit /b 1 )
if not exist "%FLTK%\lib\libfltk_z.a" ( echo ERROR: run build_zlib.cmd first & exit /b 1 )
if not exist "%WS%\build" mkdir "%WS%\build"

rem ---------------------------------------------------------------------------
rem Resolve the reproducible build stamp (see the header).  Everything in this
rem block is ASCII and uses absolute tool paths, because PATH is minimal here.
rem Those paths must stay UNQUOTED: inside a for /f backquote the whole command
rem is handed to `cmd /c`, and a command line that *starts* with a quote makes
rem cmd apply its stripped-quote rule and fail with
rem   The system cannot find the path specified.
rem (measured 2026-10-10 -- quoting "%SystemRoot%\System32\findstr.exe" breaks
rem  the derivation; the fallback silently masked it).  %SystemRoot% itself
rem never contains a space, and the scripts already assume that.
rem ---------------------------------------------------------------------------
set "BUILD_DATE="
if defined PZTK_BUILD_DATE set "BUILD_DATE=%PZTK_BUILD_DATE%"
if not defined BUILD_DATE if defined SOURCE_DATE_EPOCH (
    for /f "usebackq delims=" %%d in (`%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe -NoProfile -Command "[DateTimeOffset]::FromUnixTimeSeconds([int64]$env:SOURCE_DATE_EPOCH).UtcDateTime.ToString('yyyy-MM-dd')" 2^>nul`) do set "BUILD_DATE=%%d"
)
if not defined BUILD_DATE (
    for /f "usebackq tokens=2" %%d in (`%SystemRoot%\System32\findstr.exe /b /r /c:"## 20[0-9][0-9]-[0-9][0-9]-[0-9][0-9]" "%WS%\tool-src\CHANGELOG.md"`) do (
        if not defined BUILD_DATE set "BUILD_DATE=%%d"
    )
)
if not defined BUILD_DATE set "BUILD_DATE=2026-10-10"
echo [imgui] build stamp: %BUILD_DATE%

rem ---------------------------------------------------------------------------
rem Optional dev-only layout probe (see imgui_main.cpp "PZTK_LAYOUT_PROBE").
rem Set PZTK_LAYOUT_PROBE=1 to compile the probe in; it writes a one-line layout
rem report (block height / content height / slack) to pztk_dev_layout.txt in the
rem current directory while PZTK_DEV=1 is set.  Release builds MUST NOT define it:
rem the check is that the shipped exe contains no "pztk_dev_layout.txt" string.
rem ---------------------------------------------------------------------------
set "PROBEARG="
if defined PZTK_LAYOUT_PROBE set "PROBEARG=-DPZTK_LAYOUT_PROBE=1"
if defined PROBEARG echo [imgui] PZTK_LAYOUT_PROBE is ON (dev-only layout probe; DO NOT SHIP)

rem ??????????????, ????????????
taskkill /F /IM pztk_imgui.exe >nul 2>&1
ping -n 2 127.0.0.1 >nul

rem ---------------------------------------------------------------------------
rem Compile the icon + VERSIONINFO resource (best effort).
rem
rem Run inside %RES% so the relative "logo.ico" path in version_imgui.rc resolves;
rem specify the toolchain's own include dir so <Windows.h> / <winresrc.h> are
rem found even if the copy in %TC% is incomplete (that happens: the provisioned
rem toolchain has been observed missing winresrc.h, which makes windres stop at
rem 'windows.h:27:10: fatal error: winresrc.h: No such file or directory').
rem ---------------------------------------------------------------------------
set "RESOBJ=%TEMP%\pztk\version_imgui.o"
set "RESOBJARG="
set "RESLOG=%TEMP%\pztk\windres_imgui.log"
if not exist "%TEMP%\pztk\" mkdir "%TEMP%\pztk"

if not exist "%WINDRES%" (
    echo [imgui] WARNING: windres not found at "%WINDRES%" 1>&2
    echo [imgui] WARNING: linking WITHOUT icon and version info 1>&2
) else if not exist "%RES%\version_imgui.rc" (
    echo [imgui] WARNING: "%RES%\version_imgui.rc" is missing 1>&2
    echo [imgui] WARNING: linking WITHOUT icon and version info 1>&2
) else if not exist "%RES%\logo.ico" (
    echo [imgui] WARNING: "%RES%\logo.ico" is missing 1>&2
    echo [imgui] WARNING: linking WITHOUT icon and version info 1>&2
) else (
    echo [imgui] windres version_imgui.rc
    pushd "%RES%"
    "%WINDRES%" --codepage=65001 -O coff ^
      -I "%TC%\i686-w64-mingw32\include" ^
      -D _PTK_CHINESE_UI ^
      -o "%RESOBJ%" ^
      "version_imgui.rc" 2>"%RESLOG%"
    set "RCERR=!errorlevel!"
    popd
    if exist "%RESOBJ%" if "!RCERR!"=="0" (
        set RESOBJARG=!RESOBJ!
    ) else (
        echo [imgui] WARNING: windres failed with exit code !RCERR! 1>&2
        echo [imgui] WARNING: linking WITHOUT icon and version info 1>&2
        rem Prefix every line of the log so it cannot be mistaken for a build
        rem failure of its own; `type` would print it unprefixed.
        for /f "usebackq delims=" %%l in ("%RESLOG%") do echo [imgui]   windres: %%l 1>&2
    )
)

echo [imgui] build frontend prototype
"%GXX%" -std=c++17 -O2 -mwindows -static -static-libgcc -static-libstdc++ ^
  -DUNICODE -D_UNICODE -D_PTK_CHINESE_UI ^
  -DPZTK_BUILD_DATE=\"%BUILD_DATE%\" %PROBEARG% ^
  -I "%IMGUI%" -I "%IMGUI%\backends" -I "%GLFW%\include" ^
  -idirafter "%INC%" -idirafter "%SRC%" -idirafter "%FE%" -idirafter "%FLTK%\include" ^
  "%FE%\imgui_main.cpp" ^
  "%IMGUI%\imgui.cpp" "%IMGUI%\imgui_draw.cpp" "%IMGUI%\imgui_tables.cpp" "%IMGUI%\imgui_widgets.cpp" ^
  "%IMGUI%\backends\imgui_impl_glfw.cpp" "%IMGUI%\backends\imgui_impl_opengl3.cpp" ^
  "%SRC%\pvz.cpp" "%SRC%\process.cpp" "%SRC%\data.cpp" "%SRC%\code.cpp" "%SRC%\lineup.cpp" "%SRC%\utils.cpp" "%SRC%\names.cpp" "%SRC%\pak.cpp" ^
  -L "%GLFW%\lib" -lglfw3 ^
  -lopengl32 -lgdi32 -luser32 -lcomdlg32 -lole32 -lshell32 -lwinmm -ladvapi32 -lwintrust -lcrypt32 ^
  -L "%FLTK%\lib" -lfltk_z ^
  -Wl,--no-insert-timestamp ^
  %RESOBJARG% ^
  -o "%OUT%"
if errorlevel 1 ( echo ERROR: imgui frontend build failed 1>&2 & exit /b 1 )

echo [imgui] done -^> %OUT%
exit /b 0