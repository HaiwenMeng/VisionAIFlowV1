@echo off
setlocal
cd /d F:\VisionAIFlowV1\out\qmake\Release\cpp\trainer_host || exit /b 1
F:\VS2022\BuildTools\VC\Tools\MSVC\14.36.32532\bin\Hostx64\x64\link.exe %*
set "vafExit=%ERRORLEVEL%"
endlocal & exit /b %vafExit%
