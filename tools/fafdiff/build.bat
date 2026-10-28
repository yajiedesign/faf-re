@echo off
rem ---------------------------------------------------------------------------
rem fafdiff build: a standalone 32-bit console tool. It never touches
rem src\sdk\main.vcxproj or any .vcxproj in this tree; it compiles the engine
rem translation units under test straight from src\sdk, using the same
rem float-relevant switches main.vcxproj uses for Win32 (v143 defaults
rem /arch:SSE2 /fp:precise, C++20, and in Release /O2 + /Oi and the forced
rem include platform\X87SqrtCall.h).
rem
rem   build.bat            -> build\fafdiff_release.exe and build\fafdiff_debug.exe
rem   build.bat release    -> only the Release-flavoured exe
rem   build.bat debug      -> only the Debug-flavoured exe
rem
rem The exe is linked at /BASE:0x10000000 /DYNAMICBASE:NO so the retail image
rem can be mapped at its own preferred base 0x00400000 (it has no relocations).
rem
rem FLAG AUDIT: the reference for the engine-side flags is the exact `cl`
rem command line the engine build used -
rem   buildstage\main\Win32\Release\main.tlog\CL.command.1.tlog
rem Any future change to the engine-side flags in this file (the /D list, the
rem /FI, the include paths) must be diffed against that file first.  A flag
rem that drifts away from the engine's build makes the harness measure a
rem program the engine never built.
rem ---------------------------------------------------------------------------
setlocal EnableDelayedExpansion

set "HERE=%~dp0"
for %%I in ("%HERE%..\..") do set "REPO=%%~fI"
set "SDK=%REPO%\src\sdk"
set "WM3=%REPO%\dependencies\WildMagic3p8\Foundation"
set "OUT=%HERE%build"

rem Locate the toolchain. This machine has VS 2022 on G:, not on the C: default;
rem if the caller already has vcvars loaded, use that.
if not defined VCToolsInstallDir (
  for %%V in (
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsamd64_x86.bat"
    "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsamd64_x86.bat"
    "G:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsamd64_x86.bat"
    "G:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsamd64_x86.bat"
  ) do (
    if exist %%V (
      call %%V >nul
      if not errorlevel 1 goto :have_vcvars
    )
  )
  echo no vcvarsamd64_x86.bat found - set up the environment first
  exit /b 1
)
:have_vcvars

if not exist "%OUT%" mkdir "%OUT%"

set "WHICH=%~1"
if "%WHICH%"=="" set "WHICH=all"

rem The engine headers include Wm3*.h, so the Wild Magic 3.8 Foundation include
rem paths come from main.vcxproj's $(Wm3IncludePath) as well.
set "INC=/I "%SDK%" /I "%WM3%" /I "%WM3%\Math" /I "%WM3%\Distance" /I "%WM3%\Intersection" /I "%WM3%\Containment" /I "%WM3%\System" /I "%WM3%\ObjectSystem" /I "%WM3%\Approximation" /I "%WM3%\Numerics" /I "%WM3%\Meshes" /I "%WM3%\RationalArithmetic" /I "%HERE%src""
rem Engine defines: main.vcxproj PreprocessorDefinitions (the Release|Win32 set
rem plus _UNICODE/UNICODE).  NDEBUG is not here but in the release branch
rem below: main.vcxproj's Debug|Win32 configuration defines _DEBUG and does
rem not define NDEBUG, so the debug build must not have it either.
set "COMMON=/nologo /c /std:c++20 /permissive- /Zc:inline /fp:precise /EHsc /W3 /sdl /GS /wd4073 /wd4244 /wd4273 /wd4834 /wd4996 /D WIN32 /D _WINDOWS /D WIN32_LEAN_AND_MEAN /D NOMINMAX /D LUAPLUS_LIB /D BOOST_ALL_NO_LIB /D BOOST_NO_AUTO_PTR /D _CRT_SECURE_NO_WARNINGS /D _UNICODE /D UNICODE /D _CONSOLE %INC%"

rem Engine sources under test. Empty for now: the translation units are added
rem here one at a time, and each one added here is one more engine file that is
rem compiled by the engine's own switches rather than mirrored by hand.
rem Example (the arrangement tools/difffuzz/build.bat already uses):
rem   set "SDK_SRCS="%SDK%\moho\math\QuaternionMath.cpp" "%SDK%\moho\math\Vector3f.cpp" "%SDK%\legacy\math\X87Math.cpp""
set "SDK_SRCS="

rem Harness sources.
set "H_SRCS="%HERE%src\main.cpp" "%HERE%src\pe_image.cpp" "%HERE%src\gen.cpp" "%HERE%src\targets.cpp" "%HERE%src\engine_globals.cpp""

set "LINKFLAGS=/nologo /SUBSYSTEM:CONSOLE /MACHINE:X86 /BASE:0x10000000 /FIXED:NO /DYNAMICBASE:NO /NXCOMPAT /INCREMENTAL:NO /DEBUG"

if /I "%WHICH%"=="all"     call :build release || exit /b 1
if /I "%WHICH%"=="all"     call :build debug   || exit /b 1
if /I "%WHICH%"=="release" call :build release || exit /b 1
if /I "%WHICH%"=="debug"   call :build debug   || exit /b 1
echo build OK
exit /b 0

:build
set "CFG=%~1"
set "OBJ=%OUT%\obj_%CFG%"
if not exist "%OBJ%" mkdir "%OBJ%"
if /I "%CFG%"=="release" (
  rem main.vcxproj Release^|Win32: /O2 (implies /Oi), /Oy-, /Gy, release CRT,
  rem forced include platform\X87SqrtCall.h.
  set "CFGFLAGS=/O2 /Oi /Oy- /Gy /MD /D NDEBUG /FI platform\X87SqrtCall.h"
) else (
  rem main.vcxproj Debug^|Win32: /Od /Ob1, no RTC, no JMC, debug CRT.
  rem No /FI here: the engine's Debug configuration does not force-include
  rem platform\X87SqrtCall.h either.
  set "CFGFLAGS=/Od /Ob1 /MDd /D _DEBUG /D _ITERATOR_DEBUG_LEVEL=0"
)
echo [fafdiff] compiling %CFG%
cl %COMMON% !CFGFLAGS! /Zi /Fd"%OBJ%\vc.pdb" /Fo"%OBJ%\\" %SDK_SRCS% %H_SRCS%
if errorlevel 1 exit /b 1
link %LINKFLAGS% /OUT:"%OUT%\fafdiff_%CFG%.exe" "%OBJ%\*.obj"
if errorlevel 1 exit /b 1
exit /b 0
