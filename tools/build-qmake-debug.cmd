@echo off
setlocal
call "D:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
if errorlevel 1 exit /b %errorlevel%
set "VAF_ROOT=F:\VisionAIFlowV1"
set "VAF_QMAKE=F:\Qt6.7.3\6.7.3\msvc2019_64\bin\qmake.exe"
set "VAF_BUILD_DIR=%VAF_ROOT%\out\qmake\Debug"
if not exist "%VAF_QMAKE%" exit /b 1
if not exist "%VAF_BUILD_DIR%" mkdir "%VAF_BUILD_DIR%" || exit /b 1
copy /y "%VAF_ROOT%\.qmake.cache" "%VAF_BUILD_DIR%\.qmake.cache" >nul || exit /b %errorlevel%
cd /d "%VAF_BUILD_DIR%" || exit /b 1
"%VAF_QMAKE%" -cache "%VAF_BUILD_DIR%\.qmake.cache" -r "%VAF_ROOT%\VisionAIFlowV1.pro" "CONFIG+=debug" || exit /b %errorlevel%
nmake > "%VAF_BUILD_DIR%\nmake.stdout.log" 2> "%VAF_BUILD_DIR%\nmake.stderr.log"
exit /b %errorlevel%
