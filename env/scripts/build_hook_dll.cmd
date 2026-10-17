@echo off
rem ---------------------------------------------------------------------------
rem Build pztk_hook.dll (32-bit, injected into the game by the toolkit)
rem
rem NOTE: keep this file PURE ASCII -- non-ASCII bytes break cmd parsing.
rem
rem NOTE: this toolchain has no dllcrt2.o, so we link with -nostdlib and supply
rem       our own minimal CRT (tool-src/hook/hook_crt.cpp: entry point + memcpy/memset).
rem ---------------------------------------------------------------------------
setlocal

if "%~1"=="" (set OUTNAME=pztk_hook.dll) else (set OUTNAME=%~1)

rem ---------------------------------------------------------------------------
rem The toolchain and the workspace alias are taken from provision.cmd, exactly
rem the way build_glfw.cmd / build_imgui_demo.cmd do it.  They used to be
rem hardcoded to %TEMP%\pztk, which silently ignored PZTK_BUILD_ROOT and made a
rem build in another root write its DLL into the DEFAULT workspace's repository.
rem provision.cmd's own errors go to stderr and stay visible.
rem ---------------------------------------------------------------------------
set "TC="
set "WS="
for /f "usebackq tokens=1,* delims==" %%a in (`"%~dp0provision.cmd"`) do (
    if /i "%%a"=="TC" set "TC=%%b"
    if /i "%%a"=="WS" set "WS=%%b"
)
if not defined TC (
    echo ERROR: provision failed - it printed no TC= line
    exit /b 1
)
if not defined WS (
    echo ERROR: provision failed - it printed no WS= line
    exit /b 1
)
if not exist "%TC%\bin\g++.exe" (
    echo ERROR: toolchain not found at %TC%
    exit /b 1
)
if not exist "%WS%\tool-src\hook\hook_main.cpp" (
    echo ERROR: workspace junction not ready at %WS%
    exit /b 1
)

echo [hook] build %OUTNAME%

rem NOTE: -fno-builtin / -fno-tree-loop-distribute-patterns are REQUIRED:
rem       without them GCC turns our own memcpy/memset loops into calls to
rem       memcpy/memset themselves -> infinite recursion -> stack overflow crash.
rem
rem NOTE (2026-10-12): --no-insert-timestamp + --image-base are REQUIRED for a
rem       reproducible DLL.  Measured before this change: two links of the very
rem       same sources produced two different files, because
rem         (a) ld stamped the current time into the PE header TimeDateStamp, and
rem         (b) ld picks a RANDOM preferred image base for -shared (observed
rem             0x66D40000 vs 0x6A7C0000) -- which changes every absolute address
rem             in .text and therefore hundreds of relocation bytes.
rem       With both flags two builds to the SAME output name are byte-identical
rem       (diff = 0 bytes).  The image base is only a *preference*: this DLL has a
rem       full .reloc section and DllCharacteristics keeps DYNAMIC_BASE, so the
rem       loader still places it wherever it wants.
rem       * Build the release DLL under its FINAL name (pztk_hook.dll): the module
rem         name is embedded in the export directory, so linking to a different
rem         file name changes (and only changes) that string + the PE checksum.
"%TC%\bin\g++.exe" -std=c++17 -O2 -m32 -shared -nostdlib -nostartfiles ^
    -fno-exceptions -fno-rtti -fno-stack-protector ^
    -fno-builtin -fno-tree-loop-distribute-patterns ^
    -DUNICODE -D_UNICODE -DWIN32 -D_WINDOWS ^
    -I "%WS%\tool-src\hook" -I "%WS%\tool-src\hook\minhook" ^
    "%WS%\tool-src\hook\hook_main.cpp" ^
    "%WS%\tool-src\hook\hook_crt.cpp" ^
    "%WS%\tool-src\hook\minhook\buffer.cpp" ^
    "%WS%\tool-src\hook\minhook\hde32.cpp" ^
    "%WS%\tool-src\hook\minhook\hook.cpp" ^
    "%WS%\tool-src\hook\minhook\trampoline.cpp" ^
    -Wl,-e,_DllMainCRTStartup@12 -Wl,-s ^
    -Wl,--no-insert-timestamp -Wl,--image-base,0x10000000 ^
    -lkernel32 -luser32 ^
    -o "%WS%\build\%OUTNAME%"

if errorlevel 1 (
    echo ERROR: hook dll build failed
    exit /b 1
)
echo [hook] done -^> %WS%\build\%OUTNAME%
endlocal
