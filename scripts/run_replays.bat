@echo on
setlocal EnableExtensions

set "exePath=generalszh.exe"
set "replayFile=%~1"
if "%replayFile%"=="" set "replayFile=%REPLAY_ARG%"
set "debugCRCFromFrame=%~2"
if "%debugCRCFromFrame%"=="" set "debugCRCFromFrame=%DEBUG_CRC_FROM_FRAME%"
set "debugCRCUntilFrame=%~3"
if "%debugCRCUntilFrame%"=="" set "debugCRCUntilFrame=%DEBUG_CRC_UNTIL_FRAME%"

rem Write logs to C:\ so the Linux wrapper can read them from the prefix
set "stdoutPath=C:\stdout.log"
set "stderrPath=C:\stderr.log"

del /q "%stdoutPath%" "%stderrPath%" 2>nul

if "%replayFile%"=="" (
  echo ERROR: No replay file specified
  echo ERROR: No replay file specified>>"%stderrPath%"
  exit /b 2
)

if not exist "%exePath%" (
  echo ERROR: Executable not found at %cd%\%exePath%
  echo ERROR: Executable not found at %cd%\%exePath%>>"%stderrPath%"
  exit /b 1
)

set "crcArgs="
if not "%debugCRCFromFrame%"=="" set "crcArgs=%crcArgs% -DebugCRCFromFrame %debugCRCFromFrame%"
if not "%debugCRCUntilFrame%"=="" set "crcArgs=%crcArgs% -DebugCRCUntilFrame %debugCRCUntilFrame%"

echo Run "%cd%\%exePath%" -headless -replay "%replayFile%"%crcArgs%
echo Run "%cd%\%exePath%" -headless -replay "%replayFile%"%crcArgs%>>"%stdoutPath%"

rem Run in the foreground; exit code will propagate
"%exePath%" -headless -replay *.rep 1>>"%stdoutPath%" 2>>"%stderrPath%"
set "exitCode=%ERRORLEVEL%"

echo Exit code: %exitCode%
echo Exit code: %exitCode%>>"%stdoutPath%"

exit /b %exitCode%
