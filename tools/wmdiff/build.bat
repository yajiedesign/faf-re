@echo off
rem ---------------------------------------------------------------------------
rem wmdiff build: a standalone 32-bit console tool. It never touches
rem main.vcxproj or any .vcxproj in this tree; it compiles the harness sources
rem together with the Wild Magic 3.8 headers, using the same float-relevant
rem switches main.vcxproj uses for Win32 (v143 defaults /arch:SSE2 /fp:precise,
rem C++20, and in Release /O2 + /Oi).
rem
rem   build.bat            -> build\wmdiff_release.exe and build\wmdiff_debug.exe
rem   build.bat release    -> only the Release-flavoured exe
rem   build.bat debug      -> only the Debug-flavoured exe
rem
rem The exe is linked at /BASE:0x10000000 /DYNAMICBASE:NO so the retail image
rem can be mapped at its own preferred base 0x00400000 (it has no relocations).
rem
rem This machine's VS 2022 lives on G:, not the C: default this file assumes;
rem build_wm3.bat in the parent repo sets VCToolsInstallDir before calling us,
rem or invoke build.bat from a developer prompt that already has vcvars loaded.
rem ---------------------------------------------------------------------------
setlocal EnableDelayedExpansion

set "HERE=%~dp0"
for %%I in ("%HERE%..") do set "WM3=%%~fI"

set "INC=/I "%WM3%\Foundation" /I "%WM3%\Foundation\Math" /I "%WM3%\Foundation\Distance" /I "%WM3%\Foundation\Intersection" /I "%WM3%\Foundation\Containment" /I "%WM3%\Foundation\System" /I "%WM3%\Foundation\ObjectSystem" /I "%WM3%\Foundation\Approximation" /I "%WM3%\Foundation\Numerics" /I "%WM3%\Foundation\Meshes" /I "%WM3%\Foundation\RationalArithmetic" /I "%HERE%src""
set "COMMON=/nologo /c /std:c++20 /Zc:inline /fp:precise /EHsc /W3 /sdl /GS /wd4073 /wd4244 /wd4273 /wd4834 /wd4996 /D WIN32 /D _CONSOLE /D WIN32_LEAN_AND_MEAN /D NOMINMAX /D _CRT_SECURE_NO_WARNINGS %INC%"

rem Harness sources only - the Wild Magic 3.8 code under test is reached through
rem its headers (the *_harness.cpp files forward; nothing is mirrored by hand).
rem
rem Foundation\Math\Wm3Math.cpp is the one library .cpp that has to be compiled
rem rather than header-reached: it carries the explicit specialisations of the
rem Math<float> constants (PI, HALF_PI, LN_2, ...), and ACos/ASin return them.
rem Without it the link fails with unresolved externals - do not "fix" that by
rem redefining the constants here, that would take them out of the comparison.
set "H_SRCS="%HERE%src\main.cpp" "%HERE%src\pe_image.cpp" "%HERE%src\gen.cpp" "%HERE%src\targets_wm3.cpp" "%HERE%src\wm3_vec_targets.cpp" "%HERE%src\wm3_geom_targets.cpp" "%HERE%src\wm3_geom_thunks.cpp" "%HERE%src\wm3_geom_harness.cpp" "%HERE%src\wm3_coll_targets.cpp" "%HERE%src\wm3_coll_thunks.cpp" "%HERE%src\wm3_coll_harness.cpp" "%HERE%src\wm3_math_targets.cpp" "%HERE%src\wm3_thunks.cpp" "%HERE%src\wm3_harness.cpp" "%HERE%src\wm3_math_thunks.cpp" "%HERE%src\wm3_math_harness.cpp""
set "WM_SRCS="%WM3%\Foundation\Math\Wm3Math.cpp" "%WM3%\Foundation\Distance\Wm3Distance.cpp" "%WM3%\Foundation\Distance\Wm3DistVector2Box2.cpp" "%WM3%\Foundation\Distance\Wm3DistVector3Box3.cpp" "%WM3%\Foundation\Distance\Wm3DistVector3Segment3.cpp" "%WM3%\Foundation\Intersection\Wm3Intersector.cpp" "%WM3%\Foundation\Intersection\Wm3IntrBox2Box2.cpp" "%WM3%\Foundation\Intersection\Wm3IntrBox2Circle2.cpp" "%WM3%\Foundation\Intersection\Wm3IntrLine3Box3.cpp" "%WM3%\Foundation\Intersection\Wm3IntrSegment3Box3.cpp" "%WM3%\Foundation\Intersection\Wm3IntrSegment3Sphere3.cpp" "%WM3%\Foundation\Intersection\Wm3IntrSegment3Capsule3.cpp" "%WM3%\Foundation\Intersection\Wm3IntrLine3Capsule3.cpp" "%WM3%\Foundation\Distance\Wm3DistLine3Segment3.cpp" "%WM3%\Foundation\Distance\Wm3DistSegment3Segment3.cpp" "%WM3%\Foundation\Intersection\Wm3IntrBox3Sphere3.cpp" "%WM3%\Foundation\Math\Wm3Vector2.cpp" "%WM3%\Foundation\Math\Wm3Vector3.cpp" "%WM3%\Foundation\Math\Wm3Quaternion.cpp" "%WM3%\Foundation\System\Wm3System.cpp" "%WM3%\Foundation\Containment\Wm3ContBox3.cpp" "%WM3%\Foundation\Containment\Wm3ContBox2.cpp" "%WM3%\Foundation\Containment\Wm3ConvexHull3.cpp" "%WM3%\Foundation\Containment\Wm3ConvexHull1.cpp" "%WM3%\Foundation\Containment\Wm3ConvexHull2.cpp" "%WM3%\Foundation\Containment\Wm3ConvexHull.cpp" "%WM3%\Foundation\Containment\Wm3HullEdge2.cpp" "%WM3%\Foundation\Containment\Wm3HullTriangle3.cpp" "%WM3%\Foundation\Approximation\Wm3ApprGaussPointsFit3.cpp" "%WM3%\Foundation\Approximation\Wm3ApprGaussPointsFit2.cpp" "%WM3%\Foundation\Numerics\Wm3Eigen.cpp""

set "LINKFLAGS=/nologo /SUBSYSTEM:CONSOLE /MACHINE:X86 /BASE:0x10000000 /FIXED:NO /DYNAMICBASE:NO /NXCOMPAT /INCREMENTAL:NO /DEBUG"

set "WHICH=%~1"
if "%WHICH%"=="" set "WHICH=all"

if /I "%WHICH%"=="all"     call :build release || exit /b 1
if /I "%WHICH%"=="all"     call :build debug   || exit /b 1
if /I "%WHICH%"=="release" call :build release || exit /b 1
if /I "%WHICH%"=="debug"   call :build debug   || exit /b 1
echo build OK
exit /b 0

:build
set "CFG=%~1"
set "OBJ=%HERE%build\obj_%CFG%"
if not exist "%OBJ%" mkdir "%OBJ%"
if /I "%CFG%"=="release" (
  rem main.vcxproj Release^|Win32: /O2 (implies /Oi), /Gy, release CRT.
  set "CFGFLAGS=/O2 /Oi /Gy /MD /D NDEBUG"
) else (
  rem main.vcxproj Debug^|Win32: /Od /Ob1, no RTC, no JMC, debug CRT.
  set "CFGFLAGS=/Od /Ob1 /MDd /D _DEBUG /D _ITERATOR_DEBUG_LEVEL=0"
)
echo [wmdiff] compiling %CFG%
rem One conformance mode, the engine's: main.vcxproj compiles with
rem   /permissive-, and the Wild Magic sources are built the same way
rem   now - Foundation.vcxproj was switched to match, which needed the
rem   RationalArithmetic calls to qualify their dependent base.
cl %COMMON% /permissive- !CFGFLAGS! /Zi /Fd"%OBJ%\vc.pdb" /Fo"%OBJ%\\" %H_SRCS% %WM_SRCS%
if errorlevel 1 exit /b 1
link %LINKFLAGS% /OUT:"%HERE%build\wmdiff_%CFG%.exe" "%OBJ%\*.obj"
if errorlevel 1 exit /b 1
exit /b 0
