# fafdiff: the engine-side paired comparison harness.
#
# It maps the retail ForgedAlliance image and compares it bit for bit against
# the rebuilt main engine (src/). It is built by the engine's own CMake - same
# compiler (clang-cl), same code-generation contract (faf_codegen) as the
# engine - so the code under test is compiled exactly the way the engine
# compiles it. A comparison that measured a differently compiled program would
# measure nothing.
#
#   cmake --preset clang-x86-release
#   cmake --build --preset clang-x86-release --target fafdiff
#
# The binary lands in build/clang-x86-release/tools/fafdiff/fafdiff.exe.
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

# --- The harness ----------------------------------------------------------------------
set(FAFDIFF_SOURCES
  src/main.cpp
  src/pe_image.cpp
  src/gen.cpp
  src/targets.cpp
  src/engine_globals.cpp
  # The engine translation units under test are added to this list, one at a
  # time. Each one added here is one more engine file compiled by the engine's
  # own switches rather than mirrored by hand.
  #
  # src/moho/math/Vector3f.cpp is the first one under test (the row for
  # moho::NormalizeVectorInto, retail 0x0044F7E0).
  "${CMAKE_SOURCE_DIR}/src/moho/math/Vector3f.cpp"
  # A dependency of the TU above, not a unit under test: Vector3f.cpp calls
  # the msvc8:: x87 helpers, and their bodies live in src/legacy/math/
  # X87Math.cpp. It is compiled with the engine's own switches as well, so
  # the helpers behave in the harness exactly as they do in the engine.
  "${CMAKE_SOURCE_DIR}/src/legacy/math/X87Math.cpp"
  # src/moho/math/QuaternionMath.cpp is the second unit under test (the rows
  # for moho::MatrixToQuat, moho::RotateQuatByAngle, moho::QuatLERP,
  # moho::SLERP, moho::NormalizeQuatInPlace, moho::QuatsNearEqual,
  # moho::QuatFromVecRot and moho::QuatToAxisAndAngle).
  "${CMAKE_SOURCE_DIR}/src/moho/math/QuaternionMath.cpp"
  # A dependency of the TU above, not a unit under test: Wm3::Quaternion's
  # copy assignment (used by QuatLERP's `*out = *q2` and by SLERP's lane
  # copies) calls Wm3::System::Memcpy, whose body lives in Wild Magic's
  # Foundation/System/Wm3System.cpp. It is compiled with the engine's own
  # switches as well, so the copy behaves in the harness exactly as it does
  # in the engine.
  "${WM3_ROOT}/Foundation/System/Wm3System.cpp")

add_executable(fafdiff EXCLUDE_FROM_ALL ${FAFDIFF_SOURCES})
set_target_properties(fafdiff PROPERTIES OUTPUT_NAME fafdiff)

# The harness compiles the engine's own translation units, so it takes the
# engine's own settings - the definitions, the include paths, the code
# generation contract (faf_codegen) and the compile options - from the one
# place they live: the interface target faf_engine_options
# (src/CMakeLists.txt). Nothing here may restate them: a second copy is a
# copy that drifts, and a harness that compiled a TU with different switches
# would measure a program the engine never built.
target_link_libraries(fafdiff PRIVATE faf_engine_options)

# Only what is the harness's own: its sources' include path, the Wild Magic
# headers the harness compiles against (Foundation/System/Wm3System.cpp) and
# the console subsystem macro.
target_include_directories(fafdiff PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  ${WM3_FOUNDATION_INCLUDES})

# The retail image is mapped at its preferred base 0x00400000 and has no
# relocations, so the harness must sit elsewhere and stay put: no ASLR, no
# incremental link, fixed image base well above the retail one.
target_link_options(fafdiff PRIVATE
  /SUBSYSTEM:CONSOLE /MACHINE:X86 /BASE:0x10000000 /FIXED:NO /DYNAMICBASE:NO
  /NXCOMPAT /INCREMENTAL:NO)
