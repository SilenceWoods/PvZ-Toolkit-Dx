@echo off
rem ---------------------------------------------------------------------------
rem Build ONLY zlib, out of the FLTK source tree, and install it into
rem env\fltk32 so that pztk_imgui.exe / pztk_hook.dll can be linked.
rem
rem KEEP THIS FILE PURE ASCII.  cmd.exe reads .cmd files as ANSI, so a UTF-8
rem Chinese comment can swallow the line break and corrupt the rest of the
rem script.
rem
rem WHY THIS SCRIPT EXISTS
rem   The two targets this source package builds (the Dear ImGui frontend and
rem   the injected DLL) need zlib for .zbl lineup compression and for PAK
rem   packing/unpacking; the core layer also does
rem       #include <FL/images/zlib.h>
rem   That zlib is the one bundled with FLTK, and it is compiled with
rem   -DZ_PREFIX, so every symbol comes out prefixed: crc32 -> fltk_z_crc32.
rem   The linker line therefore says  -L ...\env\fltk32\lib -lfltk_z  and the
rem   archive has to be called libfltk_z.a.
rem
rem   This package ships NO FLTK frontend and does NOT link FLTK itself, so
rem   there is no reason to configure/build/install all of FLTK just to get
rem   that one archive.  This script targets the single CMake target `fltk_z`
rem   (zlib/CMakeLists.txt) and installs nothing but the archive plus the three
rem   public zlib headers.
rem
rem WHAT IT PRODUCES
rem   <ws>\env\fltk-build\lib\libfltk_z.a     CMake's own output (kept for
rem                                          reference / incremental reuse)
rem   <ws>\env\fltk32\lib\libfltk_z.a        what the build actually links
rem   <ws>\env\fltk32\include\FL\images\{zlib.h,zconf.h,zutil.h}
rem                                          only copied when missing
rem
rem WHY THE OUTPUT IS COMPATIBLE WITH THE EXISTING libfltk_z.a
rem   Same sources (env\src\fltk-1.4.5\zlib, unpatched), same compiler, same
rem   flags as the archive that used to come out of the old full-FLTK build:
rem   CMake build type MinSizeRel plus -O2 gives exactly
rem       -O2 -Os -DNDEBUG -DFL_LIBRARY
rem   (compare env\fltk-build\zlib\CMakeFiles\fltk_z.dir\flags.make), and the
rem   archive is produced by the same `ar qc` + `ranlib` pair.  The script
rem   prints the SHA256 of what it installed, so it can be compared directly
rem   against an archive you already trust.
rem
rem IDEMPOTENT
rem   If <ws>\env\fltk32\lib\libfltk_z.a already exists the script does nothing
rem   but report it.  Delete that file to force a rebuild.
rem
rem ASCII PATH ALIAS
rem   Like every other build script here, sources/build trees/prefixes are
rem   addressed through the junction produced by provision.cmd, because the
rem   MinGW toolchain cannot handle the non-ASCII repository path.
rem
rem USAGE
rem   env\scripts\build_zlib.cmd
rem   env\scripts\build_zlib.cmd <toolchain-dir> <workspace-alias>   (internal)
rem   set PZTK_BUILD_ROOT=<dir>   build in a workspace root of your own
rem                               (default %TEMP%\pztk, read by provision.cmd)
rem
rem build_all.cmd calls this automatically when libfltk_z.a is missing.
rem ---------------------------------------------------------------------------
setlocal enabledelayedexpansion

set "TC=%~1"
set "WS=%~2"
if not defined WS (
    for /f "usebackq tokens=1,* delims==" %%a in (`"%~dp0provision.cmd"`) do (
        if /i "%%a"=="TC" set "TC=%%b"
        if /i "%%a"=="WS" set "WS=%%b"
    )
)
if not defined TC ( echo ERROR: provision failed ^(no TC^) 1>&2 & exit /b 1 )
if not defined WS ( echo ERROR: provision failed ^(no WS^) 1>&2 & exit /b 1 )

set "CMAKE=%TC%\bin\cmake.exe"
set "GCC=%TC%\bin\gcc.exe"
set "AR=%TC%\bin\ar.exe"
set "RANLIB=%TC%\bin\ranlib.exe"

set "SRC=%WS%\env\src\fltk-1.4.5"
set "ZSRC=%SRC%\zlib"
set "BLD=%WS%\env\fltk-build"
set "INS=%WS%\env\fltk32"
set "OUT=%INS%\lib\libfltk_z.a"

rem NOTE on style: the existence checks below use a delayed !VAR! plus the
rem one-letter loop variable %%f, never "if not exist %%~t".  cmd expands %%~t
rem and re-parses the enclosing block, which trips the classic
rem   . was unexpected at this time.
rem error as soon as one of the paths contains a dot (every .exe does) --
rem measured, not guessed.
for %%f in (cmake.exe mingw32-make.exe gcc.exe ar.exe ranlib.exe) do (
    set "F=%TC%\bin\%%f"
    if not exist "!F!" (
        echo ERROR: toolchain file not found: !F! 1>&2
        echo        the provisioned toolchain copy is incomplete - free up some 1>&2
        echo        disk space and re-run env\scripts\provision.cmd 1>&2
        endlocal & exit /b 1
    )
)

if not exist "%ZSRC%\CMakeLists.txt" (
    echo ERROR: FLTK source tree is missing: %ZSRC% 1>&2
    echo        zlib is built out of FLTK's own zlib/ subdirectory: 1>&2
    echo          env\src\fltk-1.4.5\zlib 1>&2
    echo        Here FLTK is a zlib SOURCE only - its frontend is not built 1>&2
    echo        and not shipped with this package.  Get the tarball with 1>&2
    echo          python env\scripts\download_deps.py 1>&2
    echo        then extract fltk-1.4.5-source.tar.gz to env\src\ and rename the 1>&2
    echo        folder to fltk-1.4.5.  See BUILD.md section 1.3. 1>&2
    endlocal & exit /b 1
)

rem zlib/CMakeLists.txt lists exactly these 15 files; a partially extracted
rem tree would otherwise fail much later inside the compiler.
for %%f in (adler32.c compress.c crc32.c deflate.c gzclose.c gzlib.c gzread.c ^
            gzwrite.c inflate.c infback.c inftrees.c inffast.c trees.c ^
            uncompr.c zutil.c) do (
    set "F=%ZSRC%\%%f"
    if not exist "!F!" (
        echo ERROR: zlib source file missing: !F! 1>&2
        echo        the FLTK source tree is incomplete - extract it again 1>&2
        endlocal & exit /b 1
    )
)

rem zlib.h includes zconf.h; the build fails with a much less obvious message
rem without it.
for %%f in (zlib.h zconf.h zutil.h) do (
    set "F=%ZSRC%\%%f"
    if not exist "!F!" (
        echo ERROR: zlib header missing: !F! 1>&2
        echo        the FLTK source tree is incomplete - extract it again 1>&2
        endlocal & exit /b 1
    )
)

if exist "%OUT%" (
    echo [zlib] already present, skipping -^> %OUT%
    echo [zlib] delete that file to force a rebuild
    exit /b 0
)

rem Never use Ninja here: it captures every child process' output through a
rem pipe, which the sandbox blocks, so each build rule would hang.  GNU Make
rem inherits stdio and works.  Keep PATH minimal so that CMake does not pick up
rem an unrelated sh.exe (the MinGW Makefiles generator rejects that).
set "PATH=%TC%\bin;%SystemRoot%\system32;%SystemRoot%"

set "TCF=%TC:\=/%"
set "WSF=%WS:\=/%"

rem ---------------------------------------------------------------------------
rem Primary path: the upstream CMake build system, single target fltk_z.
rem ---------------------------------------------------------------------------
set "MADE="
if exist "%BLD%\CMakeCache.txt" (
    echo [zlib] build ^(reusing %BLD%^)
) else (
    echo [zlib] configure ^(FLTK_USE_SYSTEM_ZLIB=OFF, only the fltk_z target is built^)
)
"%CMAKE%" -G "MinGW Makefiles" -S "%SRC%" -B "%BLD%" ^
  -D CMAKE_BUILD_TYPE=MinSizeRel ^
  -D CMAKE_MAKE_PROGRAM="%TCF%/bin/mingw32-make.exe" ^
  -D CMAKE_C_COMPILER="%TCF%/bin/gcc.exe" ^
  -D CMAKE_CXX_COMPILER="%TCF%/bin/g++.exe" ^
  -D CMAKE_RC_COMPILER="%TCF%/bin/windres.exe" ^
  -D CMAKE_C_FLAGS="-O2" ^
  -D CMAKE_CXX_FLAGS="-O2" ^
  -D FLTK_BUILD_SHARED_LIBS=OFF ^
  -D FLTK_BUILD_FORMS=OFF ^
  -D FLTK_BUILD_FLUID=OFF ^
  -D FLTK_BUILD_FLTK_OPTIONS=OFF ^
  -D FLTK_BUILD_EXAMPLES=OFF ^
  -D FLTK_BUILD_TEST=OFF ^
  -D FLTK_BUILD_GL=OFF ^
  -D FLTK_BUILD_HTML_DOCS=OFF ^
  -D FLTK_BUILD_PDF_DOCS=OFF ^
  -D FLTK_INSTALL_HTML_DOCS=OFF ^
  -D FLTK_INSTALL_PDF_DOCS=OFF ^
  -D FLTK_GRAPHICS_GDIPLUS=ON ^
  -D FLTK_OPTION_FILESYSTEM_SUPPORT=ON ^
  -D FLTK_OPTION_LARGE_FILE=ON ^
  -D FLTK_OPTION_PRINT_SUPPORT=OFF ^
  -D FLTK_OPTION_STD=OFF ^
  -D FLTK_OPTION_SVG=ON ^
  -D FLTK_USE_SYSTEM_LIBJPEG=OFF ^
  -D FLTK_USE_SYSTEM_LIBPNG=OFF ^
  -D FLTK_USE_SYSTEM_ZLIB=OFF ^
  -D FLTK_USE_PTHREADS=ON
if errorlevel 1 (
    echo [zlib] WARNING: cmake configure failed - falling back to a direct gcc build 1>&2
) else (
    if exist "%BLD%\zlib\CMakeFiles\fltk_z.dir\flags.make" (
        echo [zlib] compile flags from CMake:
        findstr /b /c:"C_FLAGS" /c:"C_DEFINES" "%BLD%\zlib\CMakeFiles\fltk_z.dir\flags.make"
    )
    echo [zlib] build target fltk_z
    "%CMAKE%" --build "%BLD%" --target fltk_z
    if errorlevel 1 (
        echo [zlib] WARNING: cmake build failed 1>&2
        rem Written without else-if: cmd's "else if exist" parses unreliably.
        if exist "%BLD%\lib\libfltk_z.a" (set "MADE=1") else (set "MADE=")
    ) else (
        if exist "%BLD%\lib\libfltk_z.a" (set "MADE=1") else (set "MADE=")
    )
)

rem ---------------------------------------------------------------------------
rem Fallback: compile the very same 15 files with the very same flags ourselves.
rem This also covers a stale/foreign env\fltk-build tree whose makefiles decide
rem that the archive is "up to date" even though it is not there any more.
rem ---------------------------------------------------------------------------
if not defined MADE (
    echo [zlib] direct gcc build ^(-O2 -Os -DNDEBUG -DFL_LIBRARY^)
    set "OBJ=%BLD%\zlib-direct"
    if exist "!OBJ!" rd /s /q "!OBJ!"
    mkdir "!OBJ!"
    for %%f in (adler32.c compress.c crc32.c deflate.c gzclose.c gzlib.c gzread.c ^
                gzwrite.c inflate.c infback.c inftrees.c inffast.c trees.c ^
                uncompr.c zutil.c) do (
        "%GCC%" -O2 -Os -DNDEBUG -DFL_LIBRARY -c "%ZSRC%\%%f" -o "!OBJ!\%%~nf.o"
        if errorlevel 1 (
            echo ERROR: gcc failed on the file listed above 1>&2
            exit /b 1
        )
    )
    if not exist "%BLD%\lib" mkdir "%BLD%\lib"
    if exist "%BLD%\lib\libfltk_z.a" del /q "%BLD%\lib\libfltk_z.a"
    "%AR%" qc "%BLD%\lib\libfltk_z.a" "!OBJ!\adler32.o" "!OBJ!\compress.o" ^
        "!OBJ!\crc32.o" "!OBJ!\deflate.o" "!OBJ!\gzclose.o" "!OBJ!\gzlib.o" ^
        "!OBJ!\gzread.o" "!OBJ!\gzwrite.o" "!OBJ!\inflate.o" "!OBJ!\infback.o" ^
        "!OBJ!\inftrees.o" "!OBJ!\inffast.o" "!OBJ!\trees.o" "!OBJ!\uncompr.o" ^
        "!OBJ!\zutil.o"
    if errorlevel 1 ( echo ERROR: ar failed 1>&2 & exit /b 1 )
    "%RANLIB%" "%BLD%\lib\libfltk_z.a"
    if errorlevel 1 ( echo ERROR: ranlib failed 1>&2 & exit /b 1 )
    set "MADE=1"
)

if not defined MADE ( echo ERROR: zlib build produced no archive 1>&2 & exit /b 1 )
if not exist "%BLD%\lib\libfltk_z.a" ( echo ERROR: missing %BLD%\lib\libfltk_z.a 1>&2 & exit /b 1 )

rem ---------------------------------------------------------------------------
rem Install: the archive the link line wants, plus the public headers that
rem <FL/images/zlib.h> resolves to.  Headers are only copied when missing, so
rem an existing prefix is never overwritten.
rem ---------------------------------------------------------------------------
if not exist "%INS%\lib" mkdir "%INS%\lib"
copy /y "%BLD%\lib\libfltk_z.a" "%OUT%" >nul
if not exist "%OUT%" ( echo ERROR: could not install %OUT% 1>&2 & exit /b 1 )

if not exist "%INS%\include\FL\images" mkdir "%INS%\include\FL\images"
for %%f in (zlib.h zconf.h zutil.h) do (
    if not exist "%INS%\include\FL\images\%%f" copy /y "%ZSRC%\%%f" "%INS%\include\FL\images\%%f" >nul
)

rem The old full-FLTK script left a stamp here meaning "FLTK itself is already
rem installed for this alias".  That is not true any more (we only built zlib),
rem so drop it: if these sources are ever paired with the FLTK frontend again,
rem its own build script must not skip the FLTK libraries we never built.
if exist "%INS%\.pztk-alias" del /q "%INS%\.pztk-alias"

rem Print the fingerprint of the archive we just installed.  The path is handed
rem over through an environment variable, and the PowerShell executable path is
rem deliberately left UNQUOTED: inside a `for /f` backquote cmd hands the line
rem to `cmd /c`, and a command line that starts with a quote makes cmd apply
rem its stripped-quote rule and fail with
rem   'C:\Windows\...\powershell.exe" -NoProfile ...' is not recognized
rem (the very trap build_imgui_demo.cmd documents for its findstr call).
rem %SystemRoot% never contains a space and the other build scripts already
rem assume as much.  This line is cosmetic: it can never fail the build.
set "PZTK_ZLIB_OUT=%OUT%"
set "ZHASH="
set "PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if exist "%PS%" (
    for /f "usebackq tokens=*" %%h in (`%PS% -NoProfile -NonInteractive -Command "(Get-FileHash -LiteralPath $env:PZTK_ZLIB_OUT -Algorithm SHA256).Hash" 2^>nul`) do set "ZHASH=%%h"
)
set "PZTK_ZLIB_OUT="
if defined ZHASH (
    echo [zlib] archive sha256: %ZHASH%
) else (
    echo [zlib] archive sha256: could not compute - use certutil -hashfile
)
echo [zlib] done -^> %OUT%
endlocal & exit /b 0
