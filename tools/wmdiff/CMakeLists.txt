# wmdiff: the paired comparison harness.
#
# It maps the retail ForgedAlliance image and compares it bit for bit against
# the rebuilt Wild Magic 3.8 in this tree. It is built by the engine's own
# CMake - same compiler (clang-cl), same code-generation contract
# (faf_codegen) and the same Wild Magic flags (faf_wm3_build_flags) as the
# engine's own Wild Magic library - so the code under test is compiled exactly
# the way the engine compiles it. A comparison that measured a differently
# compiled program would measure nothing.
#
#   cmake --preset clang-x86-release
#   cmake --build --preset clang-x86-release --target wmdiff
#
# The binary lands in build/clang-x86-release/tools/wmdiff/wmdiff.exe.
#
# The harness maps a 32-bit retail image, so this is x86 only; the root
# CMakeLists.txt adds this directory for x86 builds only.

set(WM3_ROOT "${FAF_DEPENDENCIES_DIR}/WildMagic3p8")
set(WM3_FOUNDATION "${WM3_ROOT}/Foundation")
set(WM3_FOUNDATION_SUBDIRS
  Approximation Collision Containment Controllers Curves Detail Distance Effects
  ImageAnalysis Interpolation Intersection Math Meshes Miscellaneous Numerics
  ObjectSystem Physics RationalArithmetic SceneGraph Shaders SharedArrays Sorting
  Surfaces System Terrain Tessellation)
set(WM3_FOUNDATION_INCLUDES "${WM3_FOUNDATION}")
foreach(_dir IN LISTS WM3_FOUNDATION_SUBDIRS)
  list(APPEND WM3_FOUNDATION_INCLUDES "${WM3_FOUNDATION}/${_dir}")
endforeach()

# --- The Wild Magic sources under test ------------------------------------------------
# The list is the one the harness has always compiled: the Foundation .cpp
# files the harness needs. Foundation/Math/Wm3Math.cpp is in it because it
# carries the explicit specialisations of the Math<float> constants (PI,
# HALF_PI, LN_2, ...), which ACos/ASin return; do not resolve that by
# redefining the constants here, that would take them out of the comparison.
set(WMDIFF_WM3_SOURCES
  Math/Wm3Math.cpp
  Distance/Wm3Distance.cpp
  Distance/Wm3DistVector2Box2.cpp
  Distance/Wm3DistVector3Box3.cpp
  Distance/Wm3DistVector3Segment3.cpp
  Intersection/Wm3Intersector.cpp
  Intersection/Wm3IntrBox2Box2.cpp
  Intersection/Wm3IntrBox2Circle2.cpp
  Intersection/Wm3IntrLine3Box3.cpp
  Intersection/Wm3IntrSegment3Box3.cpp
  Intersection/Wm3IntrSegment3Sphere3.cpp
  Intersection/Wm3IntrSegment3Capsule3.cpp
  Intersection/Wm3IntrLine3Capsule3.cpp
  Distance/Wm3DistLine3Segment3.cpp
  Distance/Wm3DistSegment3Segment3.cpp
  Intersection/Wm3IntrBox3Sphere3.cpp
  Math/Wm3Vector2.cpp
  Math/Wm3Vector3.cpp
  Math/Wm3Quaternion.cpp
  Math/Wm3Vector4.cpp
  System/Wm3System.cpp
  Containment/Wm3ContBox3.cpp
  Containment/Wm3ContBox2.cpp
  Containment/Wm3ConvexHull3.cpp
  Containment/Wm3ConvexHull1.cpp
  Containment/Wm3ConvexHull2.cpp
  Containment/Wm3ConvexHull.cpp
  Containment/Wm3HullEdge2.cpp
  Containment/Wm3HullTriangle3.cpp
  Approximation/Wm3ApprGaussPointsFit3.cpp
  Approximation/Wm3ApprGaussPointsFit2.cpp
  Numerics/Wm3Eigen.cpp)
list(TRANSFORM WMDIFF_WM3_SOURCES PREPEND "${WM3_FOUNDATION}/")

add_library(wmdiff_wm3 OBJECT ${WMDIFF_WM3_SOURCES})
target_include_directories(wmdiff_wm3 PRIVATE ${WM3_FOUNDATION_INCLUDES})
# The engine's Wild Magic flags: /EHsc /W0, WIN32 NDEBUG _LIB, and
# faf_codegen (/fp:precise, SSE2 on x86, no contraction, ...).
target_link_libraries(wmdiff_wm3 PRIVATE faf_wm3_build_flags)

# --- The harness ----------------------------------------------------------------------
set(WMDIFF_HARNESS_SOURCES
  src/main.cpp
  src/pe_image.cpp
  src/gen.cpp
  src/targets_wm3.cpp
  src/wm3_vec_targets.cpp
  src/wm3_geom_targets.cpp
  src/wm3_geom_thunks.cpp
  src/wm3_geom_harness.cpp
  src/wm3_coll_targets.cpp
  src/wm3_coll_thunks.cpp
  src/wm3_coll_harness.cpp
  src/wm3_math_targets.cpp
  src/wm3_thunks.cpp
  src/wm3_harness.cpp
  src/wm3_math_thunks.cpp
  src/wm3_math_harness.cpp)

add_executable(wmdiff EXCLUDE_FROM_ALL ${WMDIFF_HARNESS_SOURCES}
  $<TARGET_OBJECTS:wmdiff_wm3>)
set_target_properties(wmdiff PROPERTIES OUTPUT_NAME wmdiff)

target_include_directories(wmdiff PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  ${WM3_FOUNDATION_INCLUDES})
target_compile_definitions(wmdiff PRIVATE
  WIN32 _CONSOLE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_compile_options(wmdiff PRIVATE /EHs /permissive- /W3)

# The retail image is mapped at its preferred base 0x00400000 and has no
# relocations, so the harness must sit elsewhere and stay put: no ASLR, no
# incremental link, fixed image base well above the retail one.
target_link_options(wmdiff PRIVATE
  /SUBSYSTEM:CONSOLE /MACHINE:X86 /BASE:0x10000000 /FIXED:NO /DYNAMICBASE:NO
  /NXCOMPAT /INCREMENTAL:NO)
