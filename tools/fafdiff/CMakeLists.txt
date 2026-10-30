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
  # src/moho/math/Vector4f.cpp is the third unit under test (the row for
  # Moho::Vector4f::operator=).
  "${CMAKE_SOURCE_DIR}/src/moho/math/Vector4f.cpp"
  # src/moho/math/Vector2f.cpp is the fourth unit under test (the row for
  # moho::IsValidVector2f).
  "${CMAKE_SOURCE_DIR}/src/moho/math/Vector2f.cpp"
  # A dependency of the TU above, not a unit under test: Vector4f.cpp's
  # Moho::ToString formats its lanes with gpg::STR_Printf, whose body lives in
  # src/gpg/core/containers/String.cpp.
  "${CMAKE_SOURCE_DIR}/src/gpg/core/containers/String.cpp"
  # A dependency of the TU above: gpg::STR_Printf returns an msvc8::string, and
  # that class's body lives in src/legacy/containers/String.cpp.
  "${CMAKE_SOURCE_DIR}/src/legacy/containers/String.cpp"
  # A dependency of the QuaternionMath.cpp TU above, not a unit under test:
  # Wm3::Quaternion's copy assignment (used by QuatLERP's `*out = *q2` and by
  # SLERP's lane copies) calls Wm3::System::Memcpy, whose body lives in Wild
  # Magic's Foundation/System/Wm3System.cpp. It is compiled with the engine's
  # own switches as well, so the copy behaves in the harness exactly as it does
  # in the engine.
  "${WM3_ROOT}/Foundation/System/Wm3System.cpp"
  # src/moho/math/Wm3AxisAlignedBox3FafExtras.cpp is a unit under test (the
  # row for moho::RayBoxSlabClip, retail 0x00473DF0).
  "${CMAKE_SOURCE_DIR}/src/moho/math/Wm3AxisAlignedBox3FafExtras.cpp"
  # src/moho/math/Wm3Segment3FafExtras.cpp is a unit under test (the row for
  # Wm3::MakeSegment3fFromEndpoints, retail 0x004FE130).
  "${CMAKE_SOURCE_DIR}/src/moho/math/Wm3Segment3FafExtras.cpp"
  # src/moho/math/GridPos.cpp is a unit under test (the rows for
  # moho::GridPos::GridPos, retail 0x00506E20, and moho::FloorSecondsToTicks,
  # retail 0x0066D1D0).
  "${CMAKE_SOURCE_DIR}/src/moho/math/GridPos.cpp"
  # A dependency of the Wm3AxisAlignedBox3FafExtras.cpp TU above, not a unit
  # under test: that file's other function (moho::AxisAlignedBoundsFromBox)
  # calls Wm3::ContAlignedBox, whose body lives in Wild Magic's
  # Foundation/Containment/Wm3ContBox3.cpp. The linker needs the whole symbol
  # -- the object file is pulled in whole, and ContAlignedBox's own siblings
  # (ContOrientedBox, ContMinBox) drag in the convex hull and the eigen
  # solver, so those come along as well.
  "${WM3_ROOT}/Foundation/Containment/Wm3ContBox3.cpp"
  "${WM3_ROOT}/Foundation/Containment/Wm3ContBox2.cpp"
  "${WM3_ROOT}/Foundation/Containment/Wm3ConvexHull3.cpp"
  "${WM3_ROOT}/Foundation/Containment/Wm3ConvexHull1.cpp"
  "${WM3_ROOT}/Foundation/Containment/Wm3ConvexHull2.cpp"
  "${WM3_ROOT}/Foundation/Containment/Wm3ConvexHull.cpp"
  "${WM3_ROOT}/Foundation/Containment/Wm3HullEdge2.cpp"
  "${WM3_ROOT}/Foundation/Containment/Wm3HullTriangle3.cpp"
  "${WM3_ROOT}/Foundation/Approximation/Wm3ApprGaussPointsFit3.cpp"
  "${WM3_ROOT}/Foundation/Approximation/Wm3ApprGaussPointsFit2.cpp"
  "${WM3_ROOT}/Foundation/Numerics/Wm3Eigen.cpp")

# --- The functions extracted from a TU the harness cannot link ------------------------
# src/moho/sim/Sim.cpp holds moho::QuatCrossAdd (the row for retail 0x0044F880,
# called by QuatFromVecRot). The whole TU cannot be linked here: its non-COMDAT
# .CRT$XCU static initializer is a link root that references every static
# object in the file, which drags in most of the engine (591 of 941 TUs). The
# functions the rows need are therefore copied out of the source by
# scripts/extract_sources.py, as declared in extract.json, and the copy is
# compiled like any other engine translation unit.
#
# The extraction runs at build time, not at configure time: configure runs once,
# and a source edit after that would go unnoticed. As a build command it is
# re-run whenever the config or one of the sources changes, and the run fails
# the build when a hash does not match, so a stale copy can never be compiled.
#
# The config carries both hashes (the source and the generated file); when one
# no longer matches, the script prints the value to fill in. See extract.json.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(FAFDIFF_EXTRACT_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/extract.json")
set(FAFDIFF_EXTRACT_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract_sources.py")
set(FAFDIFF_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

# Only the file names are read at configure time (the target has to know its
# sources); no hashing, no extraction, nothing that can go stale here.
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${FAFDIFF_EXTRACT_SCRIPT}"
          --config "${FAFDIFF_EXTRACT_CONFIG}" --root "${CMAKE_SOURCE_DIR}" --deps
  OUTPUT_VARIABLE _fafdiff_extract_deps
  RESULT_VARIABLE _fafdiff_extract_sources_rc
  OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${FAFDIFF_EXTRACT_SCRIPT}"
          --config "${FAFDIFF_EXTRACT_CONFIG}" --root "${CMAKE_SOURCE_DIR}"
          --outdir "${FAFDIFF_GENERATED_DIR}" --outputs
  OUTPUT_VARIABLE FAFDIFF_EXTRACTED
  RESULT_VARIABLE _fafdiff_extract_outputs_rc
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _fafdiff_extract_sources_rc EQUAL 0 OR NOT _fafdiff_extract_outputs_rc EQUAL 0)
  message(FATAL_ERROR "fafdiff: ${FAFDIFF_EXTRACT_CONFIG} cannot be read")
endif()

set_source_files_properties(${FAFDIFF_EXTRACTED} PROPERTIES GENERATED TRUE)
add_custom_command(
  OUTPUT ${FAFDIFF_EXTRACTED}
  COMMAND "${Python3_EXECUTABLE}" "${FAFDIFF_EXTRACT_SCRIPT}"
          --config "${FAFDIFF_EXTRACT_CONFIG}" --root "${CMAKE_SOURCE_DIR}"
          --outdir "${FAFDIFF_GENERATED_DIR}"
  DEPENDS "${FAFDIFF_EXTRACT_CONFIG}" "${FAFDIFF_EXTRACT_SCRIPT}" ${_fafdiff_extract_deps}
  COMMENT "fafdiff: extracting the engine functions under test"
  VERBATIM)

add_executable(fafdiff EXCLUDE_FROM_ALL ${FAFDIFF_SOURCES} ${FAFDIFF_EXTRACTED})
add_custom_target(fafdiff_extract DEPENDS ${FAFDIFF_EXTRACTED})
add_dependencies(fafdiff fafdiff_extract)
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
