@echo off
setlocal
cd /d F:\VisionAIFlowV1\out\qmake\Debug\cpp\apps\hosts\trainer || exit /b 1
link.exe %*
set "vafExit=%ERRORLEVEL%"
endlocal & exit /b %vafExit%
