REM Build script for ircbot
@ECHO OFF
SetLocal EnableDelayedExpansion

SET rootFolder=%~dp0..\

REM let's find all of the .c source files in our src folder
SET cFilenames=
FOR %%f in (.\src\*.c) do (
    SET cFilenames=!cFilenames! "%%f"
)

ECHO %cFilenames%

SET assembly=uploader
SET compilerFlags=-Wvarargs -Wall -Werror -O2
SET includeFlags=""
SET defines=-D_CRT_SECURE_NO_WARNINGS

ECHO "Building %assembly%%..."
clang %cFilenames% %compilerFlags% -o ".\bin\%assembly%.exe" %defines% %includeFlags% %linkerFlags%