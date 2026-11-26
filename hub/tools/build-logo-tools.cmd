@echo off
setlocal
set "ROOT=%~dp0"
set "OUT=%ROOT%bin"
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist "%OUT%" mkdir "%OUT%"
cl /nologo /O2 /D_CRT_SECURE_NO_WARNINGS "%ROOT%ctr-logobuilder\ctr-logobuilder.c" "%ROOT%ctr-logobuilder\utils.c" /Fe:"%OUT%\ctr-logobuilder.exe"
if errorlevel 1 exit /b 1
cl /nologo /O2 /EHsc /TP /D_CRT_SECURE_NO_WARNINGS /I"%ROOT%ctr-gputextool" /I"%ROOT%." "%ROOT%ctr-gputextool\ctr-gputextool.c" "%ROOT%ctr-gputextool\utils.c" "%ROOT%lodepng\lodepng.cpp" /Fe:"%OUT%\ctr-gputextool.exe"
if errorlevel 1 exit /b 1
cl /nologo /O2 /D_CRT_SECURE_NO_WARNINGS /I"%ROOT%darctool\wincompat" "%ROOT%darctool\darctool.c" "%ROOT%darctool\utils.c" "%ROOT%darctool\wincompat\iconv_compat.c" /Fe:"%OUT%\darctool.exe"
if errorlevel 1 exit /b 1
echo Logo conversion tools built in "%OUT%".
