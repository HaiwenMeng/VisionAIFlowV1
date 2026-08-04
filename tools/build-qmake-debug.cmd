@echo off
setlocal
call F:\VS2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat x64 -vcvars_ver=14.36
if errorlevel 1 exit /b %errorlevel%
cd /d F:\VisionAIFlowV1\out\qmake\Debug
nmake > F:\VisionAIFlowV1\out\qmake\Debug\nmake.stdout.log 2> F:\VisionAIFlowV1\out\qmake\Debug\nmake.stderr.log
exit /b %errorlevel%
