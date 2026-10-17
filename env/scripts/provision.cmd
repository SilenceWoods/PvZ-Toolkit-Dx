@echo off
rem ---------------------------------------------------------------------------
rem Provision an ASCII-path build environment for the PvZ Toolkit.
rem
rem Background
rem   The repository lives under a path with non-ASCII characters.  The
rem   MinGW-w64 toolchain resolves junctions back to the real directory, so a
rem   path alias cannot relocate the toolchain itself: the toolchain MUST sit
rem   on a physically ASCII path.  Command line arguments however are passed
rem   through verbatim, so a junction alias is enough for sources and build
rem   outputs (and files still land inside the repository).
rem
rem   The only ASCII location this sandbox lets us write to is the session
rem   temp directory, hence the default build root below.
rem
rem Produces
rem   %ROOT%\mingw32   real copy of the i686 toolchain (ASCII path)
rem   %ROOT%\ws        junction -> repository root (ASCII path alias)
rem
rem Usage:  env\scripts\provision.cmd [build-root]
rem         set PZTK_BUILD_ROOT=<dir>   same as passing [build-root]
rem
rem   The argument wins over the environment variable; if neither is given the
rem   root defaults to %TEMP%\pztk.  Set PZTK_BUILD_ROOT when you want a second,
rem   independent workspace (for example to build the sources from a different
rem   copy without touching the default root).
rem ---------------------------------------------------------------------------
setlocal

set "REPO=%~dp0..\.."
pushd "%REPO%"
set "REPO=%CD%"
popd

rem ---------------------------------------------------------------------------
rem Step 0: the *source* toolchain must be there before anything is copied.
rem   A fresh clone has no env\mingw32 - it is about 1 GB and .gitignore keeps
rem   it out - so the copy below cannot work at all.  Check for it up front and
rem   name what is missing, instead of failing later with a bare and misleading
rem   "robocopy failed" line.  Setup steps: HANDOVER.md 3.1.
rem ---------------------------------------------------------------------------
if not exist "%REPO%\env\mingw32\bin\gcc.exe" (
    echo ERROR: MinGW-w64 i686 toolchain not found - cannot provision 1>&2
    echo        missing file: env\mingw32\bin\gcc.exe 1>&2
    echo        The toolchain is NOT part of this repository: it is about 1 GB 1>&2
    echo        and .gitignore keeps it out.  To fix it: 1>&2
    echo        1. download the winlibs MinGW-w64 i686 msvcrt zip.  The script 1>&2
    echo           env\scripts\download_deps.py fetches it into env\downloads 1>&2
    echo           but does NOT extract it 1>&2
    echo        2. extract the whole zip into env\mingw32 so that these files 1>&2
    echo           exist: 1>&2
    echo           env\mingw32\bin\gcc.exe 1>&2
    echo           env\mingw32\bin\g++.exe 1>&2
    echo           env\mingw32\bin\cmake.exe 1>&2
    echo        3. run this script again 1>&2
    echo        Setup steps: BUILD.md section 0 - source package, HANDOVER.md 3.1 - dev repo 1>&2
    exit /b 1
)

set "ROOT=%~1"
if not defined ROOT set "ROOT=%PZTK_BUILD_ROOT%"
if not defined ROOT set "ROOT=%TEMP%\pztk"
if not exist "%ROOT%\" mkdir "%ROOT%"

set "WS=%ROOT%\ws"
if not exist "%WS%\" (
    mklink /J "%WS%" "%REPO%" >nul
)

set "TC=%ROOT%\mingw32"

rem Check files from several parts of the tree before trusting an existing copy:
rem a copy interrupted by a full disk can still leave bin\g++.exe behind while
rem cmake's Modules directory is missing, which then fails with the very
rem unhelpful "Could not find CMAKE_ROOT !!!".
set "TC_OK="
call :pztk_check_toolchain
if not defined TC_OK (
    echo provisioning toolchain -^> %TC%
    if exist "%TC%" rd /s /q "%TC%"
    robocopy "%REPO%\env\mingw32" "%TC%" /E /MT:16 /NFL /NDL /NJH /NJS /NP >nul
    rem robocopy reports success as 0..7 (1 means "files were copied") and only
    rem 8 and above are real failures - do not let that leak out as our exit code.
    if errorlevel 8 (
        rem %ERRORLEVEL% would be expanded when this block is parsed, i.e. long
        rem before robocopy ran, so it always printed "code 0".  Read the real
        rem code in a subroutine - a CALL does not touch the current ERRORLEVEL.
        call :pztk_robocopy_failed
        exit /b 1
    )
    call :pztk_check_toolchain
)

rem windres cannot even parse <Windows.h> without winresrc.h, so a copy that is
rem missing it dies with "fatal error: winresrc.h: No such file or directory"
rem while compiling the ImGui icon/VERSIONINFO resource.  Report that case on its
rem own: re-running this script would not help, the header has to be in the
rem source copy under env\mingw32.
if defined TC_NO_WINRESRC (
    echo ERROR: toolchain copy is incomplete: winresrc.h is missing 1>&2
    echo        expected: %TC%\i686-w64-mingw32\include\winresrc.h 1>&2
    echo        windres needs it for tool-src\res\version_imgui.rc; without it 1>&2
    echo        the ImGui build silently loses its icon and version info. 1>&2
    echo        Re-extract env\mingw32 from the winlibs zip, then run again. 1>&2
    exit /b 1
)
if not defined TC_OK (
    echo ERROR: toolchain copy is incomplete - is the disk full? 1>&2
    echo        the copy needs about 1 GB of free space 1>&2
    exit /b 1
)
if not exist "%WS%\tool-src\CMakeLists.txt" (
    echo ERROR: workspace alias is not usable 1>&2
    exit /b 1
)

rem ---------------------------------------------------------------------------
rem Install the project manifest into BOTH toolchain copies.
rem
rem This has to run after the copy above is ready, and it has to cover the master
rem copy env\mingw32 as well.  Otherwise the copy we just rebuilt inherits the
rem *factory* default-manifest.o from the master and every exe linked afterwards
rem silently loses activeCodePage=UTF-8 - which is exactly the accident recorded
rem in HANDOVER.md 3.6.  install_manifest.cmd never lets windres write below
rem env\mingw32 (that would delete the very file it is meant to install; the
rem path is non-ASCII and windres unlinks its target before failing).
rem ---------------------------------------------------------------------------
call "%~dp0install_manifest.cmd" "%TC%"
if errorlevel 1 (
    echo ERROR: installing the project manifest failed 1>&2
    echo        see the [manifest] lines above 1>&2
    exit /b 1
)

echo ROOT=%ROOT%
echo TC=%TC%
echo WS=%WS%
endlocal & exit /b 0

:pztk_check_toolchain
set "TC_OK=1"
set "TC_NO_WINRESRC="
if not exist "%TC%\bin\g++.exe" set "TC_OK="
if not exist "%TC%\bin\cmake.exe" set "TC_OK="
set "PZTK_TC_MOD="
for /d %%d in ("%TC%\share\cmake-*") do if exist "%%d\Modules\CMakeDetermineSystem.cmake" set "PZTK_TC_MOD=1"
if not defined PZTK_TC_MOD set "TC_OK="
rem The ImGui target compiles its icon/VERSIONINFO resource with windres, and
rem <Windows.h> pulls in <winresrc.h>; a copy without that one header stops at
rem "windows.h:27:10: fatal error: winresrc.h: No such file or directory".
if not exist "%TC%\i686-w64-mingw32\include\winresrc.h" (
    set "TC_OK="
    set "TC_NO_WINRESRC=1"
)
goto :eof

:pztk_robocopy_failed
rem Only reached from inside the "if not defined TC_OK" block above; ERRORLEVEL
rem still holds robocopy's own exit code here (8 and above = a real failure).
echo ERROR: robocopy failed with code %ERRORLEVEL% 1>&2
echo        the toolchain copy is incomplete: %TC% 1>&2
echo        free about 1 GB on that drive, then run this script again. 1>&2
goto :eof
