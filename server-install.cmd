@ECHO OFF

SET installDir="C:\Program Files\game-templates"
SET serverTag=game-templates-server

NET SESSION >nul 2>&1
IF /I "%ERRORLEVEL%" NEQ "0" (

    ECHO Failure: Need to be ADMIN!
    GOTO :EOF
)

IF NOT EXIST %installDir% (

    MD %installDir%
)
IF NOT EXIST %installDir%\accounts (

    MD %installDir%\accounts
)

ECHO port=12000 > %installDir%\settings.ini
ECHO max-login=10 >> %installDir%\settings.ini

SET exePath=%~dp0\bin\manager.exe
IF NOT EXIST %exePath% (

    ECHO Failure: Be sure to compile the manager first!
    GOTO :EOF
)
COPY %exePath% %installDir%\manager.exe

SET exePath=%~dp0\bin\server.exe
IF NOT EXIST %exePath% (

    ECHO Failure: Be sure to compile the server first!
    GOTO :EOF
)
COPY %exePath% %installDir%\server.exe

SC CREATE %serverTag% binPath=%installDir%\server.exe start=auto

NET START %serverTag%
IF /I "%ERRORLEVEL%" EQU "0" (

    ECHO To make accounts, run %installDir%\manager.exe
)