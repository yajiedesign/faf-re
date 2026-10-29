# fafdiff :: coverage ledger

Per-function coverage ledger for the x87-tier math surface of the engine
rebuild (`src/moho/math/**`, `src/legacy/math/**`) as it exists
in the retail image.

| item | value |
|---|---|
| functions in the ledger | **38** |
| `src` commit | `d814addb` |
| screen data | `.tmp/fafdiff/screen.json` |
| status data | `tools/fafdiff/data/coverage_status.json` |
| statuses | `未匹配` 19, `已匹配` 18, `失败` 0, `不适用` 1 |

A row's status is one of four:

* `未匹配` -- not paired yet.
* `已匹配` -- paired, bit-for-bit identical to retail, with both mutation directions witnessed (see below).
* `失败` -- paired, but the two sides differ (or the pair does not discriminate); the reason is in `tools/fafdiff/README.md`.
* `不适用` -- deliberately out of scope; the reason is listed below.

## The ledger

| # | function | address | source | status |
|---:|---|---|---|---|
| 1 | `NormalizeVectorInto(Wm3::Vector3f* const dest, const Wm3::Vector3f& source)` | `0x0044F7E0` | `src/moho/math/Vector3f.cpp:25` | 已匹配 |
| 2 | `Vector4f::operator=(const Vector4f& rhs)` | `0x0046FB20` | `src/moho/math/Vector4f.cpp:84` | 已匹配 |
| 3 | `RayBoxSlabClip(RaySlabState& state, const Wm3::AxisAlignedBox3f& box)` | `0x00473DF0` | `src/moho/math/Wm3AxisAlignedBox3FafExtras.cpp:105` | 未匹配 |
| 4 | `atan2f(const float y, const float x)` | `0x004E9DF0` | `src/legacy/math/X87Math.cpp:465` | 未匹配 |
| 5 | `MatrixToQuat(const Wm3::Vector3f* const matrixRows, Wm3::Quaternionf* const out)` | `0x004EB3F0` | `src/moho/math/QuaternionMath.cpp:256` | 已匹配 |
| 6 | `RotateQuatByAngle(Wm3::Quaternionf* quat, float rads)` | `0x004EB740` | `src/moho/math/QuaternionMath.cpp:557` | 已匹配 |
| 7 | `BlendOrientationDeltaByMaxAngle(const Wm3::Quaternionf& goalOrientation, const Wm3::Quaternionf& currentOrientation, const float turnStepRadians, bool* const outNoStep, Wm3::Quaternionf* const outOrientation)` | `0x004EB830` | `src/moho/math/QuaternionMath.cpp:739` | 未匹配 |
| 8 | `QuatLERP(const Wm3::Quaternionf* const q1, const Wm3::Quaternionf* const q2, Wm3::Quaternionf* const out, float amount)` | `0x004EBA80` | `src/moho/math/QuaternionMath.cpp:408` | 已匹配 |
| 9 | `SLERP(const Wm3::Quaternionf* const q2, const Wm3::Quaternionf* const q1, Wm3::Quaternionf* const out, const float t)` | `0x004EBC00` | `src/moho/math/QuaternionMath.cpp:466` | 已匹配 |
| 10 | `ToString(const Vector4f& value)` | `0x004ECED0` | `src/moho/math/Vector4f.cpp:11` | 不适用 |
| 11 | `NormalizeQuatInPlace(Wm3::Quaternionf* const quat)` | `0x004EDAA0` | `src/moho/math/QuaternionMath.cpp:220` | 已匹配 |
| 12 | `QuatsNearEqual(const Wm3::Quaternionf& lhs, const Wm3::Quaternionf& rhs)` | `0x004EDED0` | `src/moho/math/QuaternionMath.cpp:201` | 已匹配 |
| 13 | `MatrixColumnsToQuatCanonical(const Wm3::Vector3f* const matrixColumns, Wm3::Quaternionf* const out)` | `0x004F0AE0` | `src/moho/math/QuaternionMath.cpp:391` | 未匹配 |
| 14 | `MatrixRowsToQuatCanonical(const Wm3::Vector3f* const matrixRows, Wm3::Quaternionf* const out)` | `0x004F0CB0` | `src/moho/math/QuaternionMath.cpp:317` | 未匹配 |
| 15 | `MakeSegment3fFromEndpoints(const Vector3f& start, const Vector3f& end)` | `0x004FE130` | `src/moho/math/Wm3Segment3FafExtras.cpp:8` | 未匹配 |
| 16 | `GridPos::GridPos(Wm3::Vec3f* const wldPos, const int gridSize)` | `0x00506E20` | `src/moho/math/GridPos.cpp:22` | 未匹配 |
| 17 | `BuildTiltShortestArcDelta(const Wm3::Vector3f& targetNormal, Wm3::Quaternionf* const outDelta, const Wm3::Vector3f& currentUp)` | `0x0050CB50` | `src/moho/math/QuaternionMath.cpp:983` | 未匹配 |
| 18 | `CoordsAbsFloat(const float value)` | `0x0050D140` | `src/moho/math/QuaternionMath.cpp:946` | 未匹配 |
| 19 | `CoordsScaleByInvSqrt(const float lengthSq, const float a, const float b, float& outA, float& outB)` | `0x0050D150` | `src/moho/math/QuaternionMath.cpp:959` | 未匹配 |
| 20 | `VEC_LargestAxis(const Wm3::Vector3f& value)` | `0x005382E0` | `src/moho/math/Vector3f.cpp:116` | 已匹配 |
| 21 | `IsValidVector2f(const Wm3::Vector2f& value)` | `0x005657B0` | `src/moho/math/Vector2f.cpp:8` | 已匹配 |
| 22 | `IsValidVector3f(const Wm3::Vector3f& value)` | `0x005657F0` | `src/moho/math/Vector3f.cpp:12` | 已匹配 |
| 23 | `VEC_HeadingFromXZ(const Wm3::Vector3f* const value)` | `0x00565A10` | `src/moho/math/Vector3f.cpp:137` | 已匹配 |
| 24 | `EulerRollToQuat(const Wm3::Vector3f* const axis, Wm3::Quatf* const outQuaternion, const float roll)` | `0x00570750` | `src/moho/math/Vector3f.cpp:311` | 已匹配 |
| 25 | `cosf(const float x)` | `0x005734D0` | `src/legacy/math/X87Math.cpp:431` | 未匹配 |
| 26 | `sinf(const float x)` | `0x005734E0` | `src/legacy/math/X87Math.cpp:413` | 未匹配 |
| 27 | `VecSetLength(Wm3::Vector3f* const vector, const float targetLength)` | `0x005B1C90` | `src/moho/math/Vector3f.cpp:149` | 已匹配 |
| 28 | `NormalizeAngleSignedRadians(const float angleRadians)` | `0x0062FB50` | `src/moho/math/QuaternionMath.cpp:117` | 未匹配 |
| 29 | `FloorSecondsToTicks(float seconds)` | `0x0066D1D0` | `src/moho/math/GridPos.h:33` | 未匹配 |
| 30 | `QuatFromAxisAngleVector(Wm3::Quaternionf* const quat, Wm3::Vector3f axisAngle)` | `0x00697360` | `src/moho/math/QuaternionMath.cpp:719` | 未匹配 |
| 31 | `IsUnitLengthVector3f(const Wm3::Vector3f& value)` | `0x0069A1D0` | `src/moho/math/Vector3f.cpp:73` | 已匹配 |
| 32 | `VecSetLengthTo(Wm3::Vector3f* const destination, const Wm3::Vector3f* const targetLengthVector, const Wm3::Vector3f* const sourceVector)` | `0x0069A360` | `src/moho/math/Vector3f.cpp:200` | 已匹配 |
| 33 | `QuatFromVecRot(Wm3::Quaternionf* quat, const Wm3::Vector3f* refAxis, float rads)` | `0x0069AA50` | `src/moho/math/QuaternionMath.cpp:606` | 已匹配 |
| 34 | `QuatToAxisAndAngle(const Wm3::Quaternionf& quaternion, Wm3::Vector3f* const axisOut, float* const angleRadiansOut)` | `0x006C1070` | `src/moho/math/QuaternionMath.cpp:673` | 已匹配 |
| 35 | `BuildRotationMatrixFromEulerHPR(Wm3::Vector3f* const outMatrix, const float heading, const float pitch, const float roll)` | `0x006D1E30` | `src/moho/math/QuaternionMath.cpp:840` | 未匹配 |
| 36 | `QuaternionToRotationMatrix3x3(const double* const quaternionLanes, double* const outMatrixLanes)` | `0x00A3AD70` | `src/moho/math/QuaternionMath.cpp:73` | 未匹配 |
| 37 | `frexp(const double x, int* const exponent)` | `0x00A8DA05` | `src/legacy/math/X87Math.cpp:291` | 未匹配 |
| 38 | `asin(const double x)` | `0x00A8DE20` | `src/legacy/math/X87Math.cpp:773` | 未匹配 |

## Why a row is 不适用

* `0x004ECED0` ToString(const Vector4f& value) - 零售有对应体：0x004ECED0 是 Moho::ToString（把四个 lane 加宽成 double，用 "x=%f,y=%f,z=%f,w=%f" 格式化进调用方提供的字符串对象，调用 gpg::STR_Printf 0x00938F10，返回该对象）。但零售这一侧在本 harness 里跑不起来：调用进入 gpg::STR_Va (0x00938E00) 之后不再返回（int3 探针在 0x00938E00/0x00938E02/0x00938E07/0x00938E0D/0x00938E0E 都触发，0x00938E15 及其后都不触发；0x00938E0E 是 `mov dword ptr fs:[0], esp`，它装上的零售自己的 SEH 帧吞掉了随后发生的故障，展开再也没完成）。这一条因此无法逐位比对，记为不适用；证据见 tools/fafdiff/README.md。

## What 已匹配 means

A row is `已匹配` only when the pair has passed **both** directions of the
mutation check, at **200,000** and **1,000,000** iterations, and the batch's
five-seed sweep (five seeds x 1,000,000 iterations):

* **direction 1 - mutate the rebuild.** Change one instruction of the
  rebuilt function (for example the operand of one x87 op, or one float
  constant). The pair must then **fail**. If it still passes, the row does
  not witness what it claims to witness, and it is not `已匹配`.
* **direction 2 - restore the rebuild.** Undo the mutation. The pair must
  **pass** again. If it still fails, the failure was not caused by the
  mutation and the row is not `已匹配` either.

`200,000` iterations catch gross mismatches; `1,000,000` iterations and the
five-seed sweep catch the ones that only show up in a corner of the input
domain. The per-row evidence -- iteration counts, mutation, seed list --
lives in `tools/fafdiff/README.md`; this table only carries the verdict.

## How this table is generated

`tools/fafdiff/scripts/gen_coverage.py` writes this file from two inputs:

* `tools/fafdiff/scripts/screen_math.py` -> `.tmp/fafdiff/screen.json`
  (the screen: one row per retail address, with the x87/SSE census and
  the source annotation each address came from)
* `tools/fafdiff/data/coverage_status.json` -- the hand-maintained status
  of every address in the set

Regenerate with:

```bash
PATH=.tmp/venv/Scripts:$PATH .tmp/venv/Scripts/python.exe \
    tools/fafdiff/scripts/screen_math.py --out .tmp/fafdiff
PYTHONUTF8=1 .tmp/venv/Scripts/python.exe \
    tools/fafdiff/scripts/gen_coverage.py
```

The set is defined by exactly this predicate, in exactly one place
(`LEDGER_PREDICATE_TEXT` / `in_ledger()` in the generator):

```
tier == 'x87' AND section_is_code AND x87_to_first_ret > 0 AND (file starts with 'src/moho/math/' OR 'src/legacy/math/') AND NOT file ends with 'MathReflection.cpp' AND NOT '/backends/' in file AND NOT 'D3D' in file AND symbol does not contain '(dynamic initializer)'
```

The set currently holds **38** functions. The generator asserts that the set
is non-empty and that every row is a real, named function (a `.cpp`
definition or a real inline in a header) -- no dynamic initializers, no
unresolved `sub_XXXX` names -- and fails loudly otherwise.

The `calls <= 3` clause of the original specification is **not** applied:
on the current tree it removes 7 rows and leaves 31, not 38. The rows
it would remove are listed here, so the difference is explicit:

* `0x004EBC00` Moho::SLERP (`src/moho/math/QuaternionMath.cpp:466`)
* `0x004EDED0` func_QuatsNearEqual (`src/moho/math/QuaternionMath.cpp:201`)
* `0x004F0AE0` MatrixColumnsToQuatCanonical (`src/moho/math/QuaternionMath.cpp:391`)
* `0x0050CB50` <unknown> (`src/moho/math/QuaternionMath.cpp:983`)
* `0x006D1E30` sub_6D1E30 (`src/moho/math/QuaternionMath.cpp:840`)
* `0x00A8DA05` frexp (`src/legacy/math/X87Math.cpp:291`)
* `0x00A8DE20` asin (`src/legacy/math/X87Math.cpp:773`)

Whether a row is a leaf is answered by the notes above, not by
dropping rows silently. Set `CALLS_LIMIT = 3` in the generator to
apply the clause anyway; the difference is then printed here.

**Do not edit this file by hand.** Every line below the header is written
by the generator. Edit `tools/fafdiff/data/coverage_status.json` and
re-run the generator instead.

## Notes

The compiler-generated dynamic initializers of the four static
quaternions in `src/moho/math/QuaternionMath.h` (one copy per including
translation unit) are **excluded** by the predicate: they are x87 code
in the retail image, but they are not functions and nothing can be
paired against them.

Rows whose screen symbol is an IDA-style name (`sub_6D1E30`,
`func_QuatLERP`, `x00BCB540`, ...) carry the source-level name read
from the annotated source line instead:

* `0x00473DF0` screen symbol `sub_473DF0` -> `RayBoxSlabClip(RaySlabState& state, const Wm3::AxisAlignedBox3f& box)` (src/moho/math/Wm3AxisAlignedBox3FafExtras.cpp:105)
* `0x004EB3F0` screen symbol `func_MatrixToQuat` -> `MatrixToQuat(const Wm3::Vector3f* const matrixRows, Wm3::Quaternionf* const out)` (src/moho/math/QuaternionMath.cpp:256)
* `0x004EB740` screen symbol `fun_RotateQuat` -> `RotateQuatByAngle(Wm3::Quaternionf* quat, float rads)` (src/moho/math/QuaternionMath.cpp:557)
* `0x004EB830` screen symbol `sub_4EB830` -> `BlendOrientationDeltaByMaxAngle(const Wm3::Quaternionf& goalOrientation, const Wm3::Quaternionf& currentOrientation, const float turnStepRadians, bool* const outNoStep, Wm3::Quaternionf* const outOrientation)` (src/moho/math/QuaternionMath.cpp:739)
* `0x004EBA80` screen symbol `func_QuatLERP` -> `QuatLERP(const Wm3::Quaternionf* const q1, const Wm3::Quaternionf* const q2, Wm3::Quaternionf* const out, float amount)` (src/moho/math/QuaternionMath.cpp:408)
* `0x004EDAA0` screen symbol `func_NormalizeQuatInPlace` -> `NormalizeQuatInPlace(Wm3::Quaternionf* const quat)` (src/moho/math/QuaternionMath.cpp:220)
* `0x004EDED0` screen symbol `func_QuatsNearEqual` -> `QuatsNearEqual(const Wm3::Quaternionf& lhs, const Wm3::Quaternionf& rhs)` (src/moho/math/QuaternionMath.cpp:201)
* `0x0069AA50` screen symbol `sub_69AA50` -> `QuatFromVecRot(Wm3::Quaternionf* quat, const Wm3::Vector3f* refAxis, float rads)` (src/moho/math/QuaternionMath.cpp:606)
* `0x006D1E30` screen symbol `sub_6D1E30` -> `BuildRotationMatrixFromEulerHPR(Wm3::Vector3f* const outMatrix, const float heading, const float pitch, const float roll)` (src/moho/math/QuaternionMath.cpp:840)

