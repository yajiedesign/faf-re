@echo off
setlocal
set "GH=G:\soft\ghidra_12.1.4_PUBLIC"
set "PROJ=G:\temp\2026\faf-re-ghidra"
set "PROG=ForgedAlliance_6c6c92f.exe"
set "SCRIPTS=%~dp0"
if "%SCRIPTS:~-1%"=="\" set "SCRIPTS=%SCRIPTS:~0,-1%"
call "%GH%\support\analyzeHeadless.bat" "%PROJ%" faf-re -process "%PROG%" -noanalysis -readOnly -scriptPath "%SCRIPTS%" -postScript ScanIntr.java
exit /b %ERRORLEVEL%
