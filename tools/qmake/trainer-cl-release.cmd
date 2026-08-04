@echo off
setlocal
cd /d F:\VisionAIFlowV1\out\qmake\Release\cpp\apps\hosts\trainer || exit /b 1
cl.exe %*
set "vafExit=%ERRORLEVEL%"
endlocal & exit /b %vafExit%
