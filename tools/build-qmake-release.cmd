@echo off
setlocal
call "D:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
if errorlevel 1 exit /b %errorlevel%
set "VAF_ROOT=F:\VisionAIFlowV1"
set "VAF_QMAKE=F:\Qt6.7.3\6.7.3\msvc2019_64\bin\qmake.exe"
if not exist "%VAF_QMAKE%" exit /b 1
if not exist "%VAF_ROOT%\out\qmake\Release" mkdir "%VAF_ROOT%\out\qmake\Release" || exit /b 1
cd /d "%VAF_ROOT%\out\qmake\Release" || exit /b 1
"%VAF_QMAKE%" -cache "%VAF_ROOT%\.qmake.cache" -r "%VAF_ROOT%\VisionAIFlowV1.pro" "CONFIG+=release" || exit /b %errorlevel%
nmake > "%VAF_ROOT%\out\qmake\Release\nmake.stdout.log" 2> "%VAF_ROOT%\out\qmake\Release\nmake.stderr.log"
exit /b %errorlevel%
