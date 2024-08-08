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

SET assembly=comm
SET compilerFlags=-g -Wvarargs -Wall -Werror -O0
REM -Wall -Werror
SET includeFlags=-Isrc -I../src/
SET defines=-D_CRT_SECURE_NO_WARNINGS

ECHO "Building %assembly%%..."
clang %cFilenames% %compilerFlags% -o "./bin/%assembly%.exe" %defines% %includeFlags% %linkerFlags%