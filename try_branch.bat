@echo off
REM try_branch.bat - build a branch Claude pushed, to try it at the scope,
REM and come back to master afterwards.
REM
REM The owner, 2026-11-22: "I want this all on the master so future nights
REM it's easier to develop outside." A fix normally reaches the scope only
REM after its PR's CI (about an hour) and a merge. With this, Claude pushes
REM a branch, you build it in a few minutes, try it on the sky, and paste
REM the log; master is never touched.
REM
REM   try_branch.bat claude/some-fix     switch to that branch and build it
REM   try_branch.bat                     pull the branch you are on again and rebuild
REM   try_branch.bat master              back to master, up to date, and build
REM
REM build_all.bat closes RadukCapture first, so never run this mid-recording.
REM If git refuses because of local changes, nothing is switched or lost:
REM paste the message to Claude.

setlocal
pushd "%~dp0"

set BRANCH=%~1
if "%BRANCH%"=="" (
    for /f "delims=" %%b in ('git rev-parse --abbrev-ref HEAD') do set BRANCH=%%b
)

echo.
echo ============================================================
echo  Switching to %BRANCH% and building it
echo  (RadukCapture will be closed by the build)
echo ============================================================

git fetch origin %BRANCH%
if errorlevel 1 (
    echo *** Could not fetch %BRANCH% from GitHub. Check the name and the network.
    popd & exit /b 1
)

git checkout %BRANCH%
if errorlevel 1 (
    echo *** git would not switch to %BRANCH% - probably local changes. Nothing was changed.
    echo     Paste this message to Claude.
    popd & exit /b 1
)

git pull --ff-only origin %BRANCH%
if errorlevel 1 (
    echo *** Could not bring %BRANCH% up to date. Paste this message to Claude.
    popd & exit /b 1
)

for /f "delims=" %%v in ('findstr /c:"RADUK_VERSION_A" src\Version.h') do echo  Now on: %BRANCH%   %%v
call build_all.bat
set RC=%ERRORLEVEL%

echo.
if not "%BRANCH%"=="master" (
    echo  This is a test build of %BRANCH%. To go back:  try_branch.bat master
)
popd
exit /b %RC%
