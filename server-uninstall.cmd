@ECHO OFF

SET installDir="C:\Program Files\game-templates"
SET serverTag=game-templates-server

NET STOP %serverTag%
IF /I "%ERRORLEVEL%" NEQ "0" (

    ECHO Failure: Need to be ADMIN!
    GOTO :EOF
)

SC DELETE %serverTag%

RD /s /q %installDir%