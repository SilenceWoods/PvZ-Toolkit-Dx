@echo off
rem ---------------------------------------------------------------------------
rem Install this project's manifest as the toolchain default manifest.
rem
rem Why this exists
rem   GCC's spec contains "%:if-exists(default-manifest.o%s)", so every non-shared
rem   link pulls in <toolchain>\i686-w64-mingw32\lib\default-manifest.o.  That one
rem   object is the only thing that decides the RT_MANIFEST embedded in
rem   pztk_imgui.exe / pvztoolkit.exe.  The stock toolchain object carries the
rem   factory manifest (780 B object / 599 B manifest / no activeCodePage);
rem   ours is compiled from tool-src\res\pvztoolkit.manifest (1508 B object /
rem   1322 B manifest / activeCodePage=UTF-8), so the object must be overwritten.
rem
rem   That overwrite used to live inline in build_toolkit.cmd and touched only
rem   the provisioned copy (%TEMP%\pztk\mingw32).  The master copy env\mingw32
rem   was never maintained, so the first provision.cmd that decided to rebuild
rem   the copy from the master silently reverted BOTH of them to the factory
rem   manifest, and the shipped exe lost activeCodePage=UTF-8.
rem   This script installs into both copies, and both provision.cmd and
rem   build_toolkit.cmd call it, so re-extracting env\mingw32 is repaired by the
rem   next provision/build instead of being silently accepted.
rem
rem Two rules this script must never break
rem   (1) windres must NEVER be the env\mingw32 copy.  That copy sits on a path
rem       with non-ASCII characters and its windres.exe cannot write ANY output
rem       file anywhere -- measured: "Permission denied" even for a target inside
rem       %TEMP% -- nor open a source below that path -- measured: "can't open
rem       file `D:/.../pvztoolkit.manifest': Invalid argument".  Worse, on the
rem       create path it unlinks the target before failing, so aiming it at
rem       env\mingw32\...\default-manifest.o DELETES the existing object.
rem       Hence: compile with the *provisioned* copy's windres (its own path is
rem       pure ASCII), stage both the manifest and the .rc under %TEMP%\pztk, and
rem       install with copy -- a cmd builtin, immune to all of this.
rem       COFF object bytes do not depend on the path of the .rc or of the
rem       manifest it includes -- verified: the %TEMP%\pztk\ws junction path, a
rem       copy under %TEMP%\pztk and the real repo path all yield
rem       1508 B / 3D3DFD7DAB5F4031A8EC31AFC77A49B86568501A3DBDB14D15AABA0AC9FDCA31.
rem   (2) an existing object must never be lost.  The temporary object is
rem       compiled and verified first and only then copied over a target, so any
rem       failure (no windres, bad manifest, no space) leaves both toolchain
rem       copies exactly as they were.
rem
rem Usage:  env\scripts\install_manifest.cmd [toolchain-copy]
rem         toolchain-copy : provisioned copy, default %TEMP%\pztk\mingw32
rem   Exit code 0 = both copies hold the verified project object, 1 = no change
rem   was made and the reason was printed on stderr.
rem   Hashes are printed lowercase because that is certutil's format.
rem ---------------------------------------------------------------------------
setlocal

set "REPO=%~dp0..\.."
pushd "%REPO%"
set "REPO=%CD%"
popd

set "SRCMF=%REPO%\tool-src\res\pvztoolkit.manifest"
set "MOTHER=%REPO%\env\mingw32"

set "TC=%~1"
if "%TC%"=="" set "TC=%TEMP%\pztk\mingw32"

rem Recorded known-good object for the manifest currently in tool-src\res.
rem A mismatch is reported loudly but is not fatal: pvztoolkit.manifest may have
rem been edited on purpose, and then both values here have to be updated.
set "PZTK_MF_SIZE=1508"
set "PZTK_MF_SHA256=3D3DFD7DAB5F4031A8EC31AFC77A49B86568501A3DBDB14D15AABA0AC9FDCA31"
rem The factory object's size: seeing it again means nothing useful was compiled.
set "PZTK_MF_FACTORY_SIZE=780"

set "WR=%TC%\bin\windres.exe"
set "MFOBJ=%TC%\i686-w64-mingw32\lib\default-manifest.o"
set "MFOUT=%MOTHER%\i686-w64-mingw32\lib\default-manifest.o"

set "TMPDIR=%TEMP%\pztk"
if not exist "%TMPDIR%\" mkdir "%TMPDIR%"
set "MFSRC=%TMPDIR%\pvztoolkit.manifest"
set "MFRC=%TMPDIR%\default-manifest.rc"
set "MFNEW=%TMPDIR%\default-manifest.o.new"

if not exist "%SRCMF%" (
    echo ERROR: manifest source not found: "%SRCMF%" 1>&2
    exit /b 1
)
if not exist "%WR%" (
    echo ERROR: windres not found: "%WR%" 1>&2
    echo        pass the provisioned toolchain copy as argument 1 1>&2
    exit /b 1
)
if not exist "%TC%\i686-w64-mingw32\lib\" (
    echo ERROR: not a usable toolchain copy: "%TC%" 1>&2
    exit /b 1
)

rem ---------------------------------------------------------------------------
rem 1) stage the manifest and its .rc under an ASCII path (rule 1)
rem ---------------------------------------------------------------------------
copy /y "%SRCMF%" "%MFSRC%" >nul
if errorlevel 1 (
    echo ERROR: cannot stage "%SRCMF%" 1>&2
    echo        to "%MFSRC%" 1>&2
    exit /b 1
)
set "MFSRCF=%MFSRC:\=/%"
> "%MFRC%" echo 1 24 "%MFSRCF%"

rem ---------------------------------------------------------------------------
rem 2) compile with the provisioned copy's windres, never inside %MOTHER%
rem ---------------------------------------------------------------------------
if exist "%MFNEW%" del /f /q "%MFNEW%" >nul 2>&1
echo [manifest] windres default-manifest.o
"%WR%" -O coff -o "%MFNEW%" "%MFRC%"
if errorlevel 1 (
    echo ERROR: windres failed to compile "%MFRC%" 1>&2
    echo        nothing installed - both toolchain copies are unchanged 1>&2
    exit /b 1
)
if not exist "%MFNEW%" (
    echo ERROR: windres reported success but produced no "%MFNEW%" 1>&2
    exit /b 1
)

rem ---------------------------------------------------------------------------
rem 3) verify the compiled object before it is allowed to replace anything (2)
rem ---------------------------------------------------------------------------
for %%a in ("%MFNEW%") do set "NEWSIZE=%%~za"
if "%NEWSIZE%"=="0" (
    echo ERROR: compiled object is empty: "%MFNEW%" 1>&2
    exit /b 1
)
if "%NEWSIZE%"=="%PZTK_MF_FACTORY_SIZE%" (
    echo ERROR: compiled object is %NEWSIZE% bytes - that is the *factory* object, 1>&2
    echo        the project manifest was not compiled in.  manifest source: 1>&2
    echo        "%SRCMF%" 1>&2
    exit /b 1
)
call :pztk_markers "%MFNEW%"
if errorlevel 1 (
    echo ERROR: compiled object "%MFNEW%" is not the project manifest 1>&2
    exit /b 1
)
call :pztk_hash "%MFNEW%" NEWSHA
if not defined NEWSHA (
    echo ERROR: cannot compute a SHA256 for "%MFNEW%" 1>&2
    echo        tried certutil and powershell 1>&2
    exit /b 1
)
if /i not "%NEWSIZE%"=="%PZTK_MF_SIZE%" (
    echo WARNING: object is %NEWSIZE% bytes, recorded known-good is %PZTK_MF_SIZE% 1>&2
    echo WARNING: if tool-src\res\pvztoolkit.manifest was changed on purpose, 1>&2
    echo WARNING: update PZTK_MF_SIZE / PZTK_MF_SHA256 in this script 1>&2
)
if /i not "%NEWSHA%"=="%PZTK_MF_SHA256%" (
    echo WARNING: object SHA256 %NEWSHA% 1>&2
    echo WARNING: recorded known-good  %PZTK_MF_SHA256% 1>&2
)

rem ---------------------------------------------------------------------------
rem 4) install into both toolchain copies and read every byte back
rem ---------------------------------------------------------------------------
call :pztk_install "%MFOBJ%" "toolchain copy"
if errorlevel 1 exit /b 1
if exist "%MOTHER%\i686-w64-mingw32\lib\" (
    call :pztk_install "%MFOUT%" "master copy env\mingw32"
    if errorlevel 1 exit /b 1
) else (
    echo WARNING: master copy not usable, skipped: "%MOTHER%" 1>&2
)

rem the verified object is installed; the staged copy and .rc stay behind on
rem purpose (they are the manual recovery artifacts documented in HANDOVER 3.6)
if exist "%MFNEW%" del /f /q "%MFNEW%" >nul 2>&1

echo [manifest] project manifest installed -^> %NEWSIZE% bytes / %NEWSHA%
endlocal & exit /b 0

rem ---------------------------------------------------------------------------
rem helpers
rem ---------------------------------------------------------------------------
:pztk_install
rem %1 = target default-manifest.o, %2 = human readable label
set "PZTK_T=%~1"
if not exist "%~dp1" (
    echo ERROR: directory missing for %~2: "%~dp1" 1>&2
    exit /b 1
)
copy /y "%MFNEW%" "%PZTK_T%" >nul
if errorlevel 1 (
    echo ERROR: copy into %~2 failed: "%PZTK_T%" 1>&2
    echo        the previous object there was not touched by us 1>&2
    exit /b 1
)
fc /b "%MFNEW%" "%PZTK_T%" >nul 2>&1
if errorlevel 1 (
    echo ERROR: %~2 does not read back byte-identical: "%PZTK_T%" 1>&2
    exit /b 1
)
echo [manifest] installed -^> %PZTK_T% ^(%~2^)
goto :eof

:pztk_markers
rem %1 = object; the factory object has neither activeCodePage nor our identity
findstr /m /c:"activeCodePage" "%~1" >nul 2>&1
if errorlevel 1 exit /b 1
findstr /m /c:"asInvoker" "%~1" >nul 2>&1
if errorlevel 1 exit /b 1
findstr /m /c:"CubeStudio.PvZ-Toolkit" "%~1" >nul 2>&1
if errorlevel 1 exit /b 1
goto :eof

:pztk_hash
rem %1 = file, %2 = variable to receive the lowercase hex digest
set "%~2="
rem certutil prints "SHA256 hash of file X:" / the digest / a success line, and
rem older builds separate the digest into 8 groups of 8 hex digits - token
rem concatenation covers both spellings.
for /f "usebackq skip=1 tokens=1,2,3,4,5,6,7,8 delims= " %%a in (`certutil -hashfile "%~1" SHA256 2^>nul`) do (
    if not defined %~2 set "%~2=%%a%%b%%c%%d%%e%%f%%g%%h"
)
if not defined %~2 (
    set "PZTK_HF=%~1"
    for /f "usebackq delims=" %%h in (`powershell -NoProfile -NonInteractive -Command "(Get-FileHash -LiteralPath $env:PZTK_HF -Algorithm SHA256).Hash" 2^>nul`) do set "%~2=%%h"
)
goto :eof
