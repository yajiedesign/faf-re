# fafdiff :: coverage ledger

Per-function coverage ledger for the x87-tier math surface of the engine
rebuild (`src/sdk/moho/math/**`, `src/sdk/legacy/math/**`) as it exists
in the retail image.

| item | value |
|---|---|
| functions in the ledger | **195** |
| `src/sdk` commit | `dee23dd2` |
| screen data | `.tmp/fafdiff/screen.json` |
| status data | `tools/fafdiff/data/coverage_status.json` |
| statuses | `未匹配` 176, `已匹配` 0, `失败` 0, `不适用` 19 |

A row's status is one of four:

* `未匹配` -- not paired yet.
* `已匹配` -- paired, bit-for-bit identical to retail, with both mutation directions witnessed (see below).
* `失败` -- paired, but the two sides differ (or the pair does not discriminate); the reason is in `tools/fafdiff/README.md`.
* `不适用` -- deliberately out of scope; the reason is listed below.

## The ledger

| # | function | address | source | status |
|---:|---|---|---|---|
| 1 | `NormalizeVectorInto(Wm3::Vector3f* const dest, const Wm3::Vector3f& source)` | `0x0044F7E0` | `src/sdk/moho/math/Vector3f.cpp:25` | 不适用 |
| 2 | `Vector4f::operator=(const Vector4f& rhs)` | `0x0046FB20` | `src/sdk/moho/math/Vector4f.cpp:79` | 未匹配 |
| 3 | `RayBoxSlabClip(RaySlabState& state, const Wm3::AxisAlignedBox3f& box)` | `0x00473DF0` | `src/sdk/moho/math/Wm3AxisAlignedBox3FafExtras.cpp:105` | 未匹配 |
| 4 | `atan2f(const float y, const float x)` | `0x004E9DF0` | `src/sdk/legacy/math/X87Math.cpp:315` | 不适用 |
| 5 | `MatrixToQuat(const Wm3::Vector3f* const matrixRows, Wm3::Quaternionf* const out)` | `0x004EB3F0` | `src/sdk/moho/math/QuaternionMath.cpp:256` | 不适用 |
| 6 | `RotateQuatByAngle(Wm3::Quaternionf* quat, float rads)` | `0x004EB740` | `src/sdk/moho/math/QuaternionMath.cpp:557` | 未匹配 |
| 7 | `BlendOrientationDeltaByMaxAngle(const Wm3::Quaternionf& goalOrientation, const Wm3::Quaternionf& currentOrientation, const float turnStepRadians, bool* const outNoStep, Wm3::Quaternionf* const outOrientation)` | `0x004EB830` | `src/sdk/moho/math/QuaternionMath.cpp:739` | 未匹配 |
| 8 | `QuatLERP(const Wm3::Quaternionf* const q1, const Wm3::Quaternionf* const q2, Wm3::Quaternionf* const out, float amount)` | `0x004EBA80` | `src/sdk/moho/math/QuaternionMath.cpp:408` | 不适用 |
| 9 | `SLERP(const Wm3::Quaternionf* const q2, const Wm3::Quaternionf* const q1, Wm3::Quaternionf* const out, const float t)` | `0x004EBC00` | `src/sdk/moho/math/QuaternionMath.cpp:466` | 不适用 |
| 10 | `ToString(const Vector4f& value)` | `0x004ECED0` | `src/sdk/moho/math/Vector4f.cpp:11` | 未匹配 |
| 11 | `NormalizeQuatInPlace(Wm3::Quaternionf* const quat)` | `0x004EDAA0` | `src/sdk/moho/math/QuaternionMath.cpp:220` | 不适用 |
| 12 | `QuatsNearEqual(const Wm3::Quaternionf& lhs, const Wm3::Quaternionf& rhs)` | `0x004EDED0` | `src/sdk/moho/math/QuaternionMath.cpp:201` | 不适用 |
| 13 | `MatrixColumnsToQuatCanonical(const Wm3::Vector3f* const matrixColumns, Wm3::Quaternionf* const out)` | `0x004F0AE0` | `src/sdk/moho/math/QuaternionMath.cpp:391` | 未匹配 |
| 14 | `MatrixRowsToQuatCanonical(const Wm3::Vector3f* const matrixRows, Wm3::Quaternionf* const out)` | `0x004F0CB0` | `src/sdk/moho/math/QuaternionMath.cpp:317` | 不适用 |
| 15 | `MakeSegment3fFromEndpoints(const Vector3f& start, const Vector3f& end)` | `0x004FE130` | `src/sdk/moho/math/Wm3Segment3FafExtras.cpp:8` | 未匹配 |
| 16 | `GridPos::GridPos(Wm3::Vec3f* const wldPos, const int gridSize)` | `0x00506E20` | `src/sdk/moho/math/GridPos.cpp:22` | 未匹配 |
| 17 | `BuildTiltShortestArcDelta(const Wm3::Vector3f& targetNormal, Wm3::Quaternionf* const outDelta, const Wm3::Vector3f& currentUp)` | `0x0050CB50` | `src/sdk/moho/math/QuaternionMath.cpp:983` | 未匹配 |
| 18 | `CoordsAbsFloat(const float value)` | `0x0050D140` | `src/sdk/moho/math/QuaternionMath.cpp:946` | 未匹配 |
| 19 | `CoordsScaleByInvSqrt(const float lengthSq, const float a, const float b, float& outA, float& outB)` | `0x0050D150` | `src/sdk/moho/math/QuaternionMath.cpp:959` | 未匹配 |
| 20 | `VEC_LargestAxis(const Wm3::Vector3f& value)` | `0x005382E0` | `src/sdk/moho/math/Vector3f.cpp:116` | 不适用 |
| 21 | `IsValidVector2f(const Wm3::Vector2f& value)` | `0x005657B0` | `src/sdk/moho/math/Vector2f.cpp:8` | 未匹配 |
| 22 | `IsValidVector3f(const Wm3::Vector3f& value)` | `0x005657F0` | `src/sdk/moho/math/Vector3f.cpp:12` | 未匹配 |
| 23 | `VEC_HeadingFromXZ(const Wm3::Vector3f* const value)` | `0x00565A10` | `src/sdk/moho/math/Vector3f.cpp:137` | 不适用 |
| 24 | `EulerRollToQuat(const Wm3::Vector3f* const axis, Wm3::Quatf* const outQuaternion, const float roll)` | `0x00570750` | `src/sdk/moho/math/Vector3f.cpp:311` | 不适用 |
| 25 | `cosf(const float x)` | `0x005734D0` | `src/sdk/legacy/math/X87Math.cpp:281` | 不适用 |
| 26 | `sinf(const float x)` | `0x005734E0` | `src/sdk/legacy/math/X87Math.cpp:263` | 不适用 |
| 27 | `VecSetLength(Wm3::Vector3f* const vector, const float targetLength)` | `0x005B1C90` | `src/sdk/moho/math/Vector3f.cpp:149` | 不适用 |
| 28 | `NormalizeAngleSignedRadians(const float angleRadians)` | `0x0062FB50` | `src/sdk/moho/math/QuaternionMath.cpp:117` | 不适用 |
| 29 | `FloorSecondsToTicks(float seconds)` | `0x0066D1D0` | `src/sdk/moho/math/GridPos.h:33` | 未匹配 |
| 30 | `QuatFromAxisAngleVector(Wm3::Quaternionf* const quat, Wm3::Vector3f axisAngle)` | `0x00697360` | `src/sdk/moho/math/QuaternionMath.cpp:719` | 不适用 |
| 31 | `IsUnitLengthVector3f(const Wm3::Vector3f& value)` | `0x0069A1D0` | `src/sdk/moho/math/Vector3f.cpp:73` | 不适用 |
| 32 | `VecSetLengthTo(Wm3::Vector3f* const destination, const Wm3::Vector3f* const targetLengthVector, const Wm3::Vector3f* const sourceVector)` | `0x0069A360` | `src/sdk/moho/math/Vector3f.cpp:200` | 不适用 |
| 33 | `QuatFromVecRot(Wm3::Quaternionf* quat, const Wm3::Vector3f* refAxis, float rads)` | `0x0069AA50` | `src/sdk/moho/math/QuaternionMath.cpp:606` | 未匹配 |
| 34 | `QuatToAxisAndAngle(const Wm3::Quaternionf& quaternion, Wm3::Vector3f* const axisOut, float* const angleRadiansOut)` | `0x006C1070` | `src/sdk/moho/math/QuaternionMath.cpp:673` | 不适用 |
| 35 | `BuildRotationMatrixFromEulerHPR(Wm3::Vector3f* const outMatrix, const float heading, const float pitch, const float roll)` | `0x006D1E30` | `src/sdk/moho/math/QuaternionMath.cpp:840` | 未匹配 |
| 36 | `QuaternionToRotationMatrix3x3(const double* const quaternionLanes, double* const outMatrixLanes)` | `0x00A3AD70` | `src/sdk/moho/math/QuaternionMath.cpp:73` | 未匹配 |
| 37 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BCAED0` | `src/sdk/moho/math/QuaternionMath.h:354` | 未匹配 |
| 38 | `kQuatYawPos90 (dynamic initializer)` | `0x00BCAF30` | `src/sdk/moho/math/QuaternionMath.h:400` | 未匹配 |
| 39 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BCAF90` | `src/sdk/moho/math/QuaternionMath.h:446` | 未匹配 |
| 40 | `kQuatYawPos45 (dynamic initializer)` | `0x00BCAFF0` | `src/sdk/moho/math/QuaternionMath.h:492` | 未匹配 |
| 41 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BCB540` | `src/sdk/moho/math/QuaternionMath.h:355` | 未匹配 |
| 42 | `kQuatYawPos90 (dynamic initializer)` | `0x00BCB5A0` | `src/sdk/moho/math/QuaternionMath.h:401` | 未匹配 |
| 43 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BCB600` | `src/sdk/moho/math/QuaternionMath.h:447` | 未匹配 |
| 44 | `kQuatYawPos45 (dynamic initializer)` | `0x00BCB660` | `src/sdk/moho/math/QuaternionMath.h:493` | 未匹配 |
| 45 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BCBBD0` | `src/sdk/moho/math/QuaternionMath.h:356` | 未匹配 |
| 46 | `kQuatYawPos90 (dynamic initializer)` | `0x00BCBC30` | `src/sdk/moho/math/QuaternionMath.h:402` | 未匹配 |
| 47 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BCBC90` | `src/sdk/moho/math/QuaternionMath.h:448` | 未匹配 |
| 48 | `kQuatYawPos45 (dynamic initializer)` | `0x00BCBCF0` | `src/sdk/moho/math/QuaternionMath.h:494` | 未匹配 |
| 49 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BCBFB0` | `src/sdk/moho/math/QuaternionMath.h:357` | 未匹配 |
| 50 | `kQuatYawPos90 (dynamic initializer)` | `0x00BCC010` | `src/sdk/moho/math/QuaternionMath.h:403` | 未匹配 |
| 51 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BCC070` | `src/sdk/moho/math/QuaternionMath.h:449` | 未匹配 |
| 52 | `kQuatYawPos45 (dynamic initializer)` | `0x00BCC0D0` | `src/sdk/moho/math/QuaternionMath.h:495` | 未匹配 |
| 53 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BCC460` | `src/sdk/moho/math/QuaternionMath.h:358` | 未匹配 |
| 54 | `kQuatYawPos90 (dynamic initializer)` | `0x00BCC4C0` | `src/sdk/moho/math/QuaternionMath.h:404` | 未匹配 |
| 55 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BCC520` | `src/sdk/moho/math/QuaternionMath.h:450` | 未匹配 |
| 56 | `kQuatYawPos45 (dynamic initializer)` | `0x00BCC580` | `src/sdk/moho/math/QuaternionMath.h:496` | 未匹配 |
| 57 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BCCA70` | `src/sdk/moho/math/QuaternionMath.h:359` | 未匹配 |
| 58 | `kQuatYawPos90 (dynamic initializer)` | `0x00BCCAD0` | `src/sdk/moho/math/QuaternionMath.h:405` | 未匹配 |
| 59 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BCCB30` | `src/sdk/moho/math/QuaternionMath.h:451` | 未匹配 |
| 60 | `kQuatYawPos45 (dynamic initializer)` | `0x00BCCB90` | `src/sdk/moho/math/QuaternionMath.h:497` | 未匹配 |
| 61 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BCCE40` | `src/sdk/moho/math/QuaternionMath.h:360` | 未匹配 |
| 62 | `kQuatYawPos90 (dynamic initializer)` | `0x00BCCEA0` | `src/sdk/moho/math/QuaternionMath.h:406` | 未匹配 |
| 63 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BCCF00` | `src/sdk/moho/math/QuaternionMath.h:452` | 未匹配 |
| 64 | `kQuatYawPos45 (dynamic initializer)` | `0x00BCCF60` | `src/sdk/moho/math/QuaternionMath.h:498` | 未匹配 |
| 65 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BCD0F0` | `src/sdk/moho/math/QuaternionMath.h:361` | 未匹配 |
| 66 | `kQuatYawPos90 (dynamic initializer)` | `0x00BCD150` | `src/sdk/moho/math/QuaternionMath.h:407` | 未匹配 |
| 67 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BCD1B0` | `src/sdk/moho/math/QuaternionMath.h:453` | 未匹配 |
| 68 | `kQuatYawPos45 (dynamic initializer)` | `0x00BCD210` | `src/sdk/moho/math/QuaternionMath.h:499` | 未匹配 |
| 69 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BCD420` | `src/sdk/moho/math/QuaternionMath.h:362` | 未匹配 |
| 70 | `kQuatYawPos90 (dynamic initializer)` | `0x00BCD480` | `src/sdk/moho/math/QuaternionMath.h:408` | 未匹配 |
| 71 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BCD4E0` | `src/sdk/moho/math/QuaternionMath.h:454` | 未匹配 |
| 72 | `kQuatYawPos45 (dynamic initializer)` | `0x00BCD540` | `src/sdk/moho/math/QuaternionMath.h:500` | 未匹配 |
| 73 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BCD9F0` | `src/sdk/moho/math/QuaternionMath.h:363` | 未匹配 |
| 74 | `kQuatYawPos90 (dynamic initializer)` | `0x00BCDA50` | `src/sdk/moho/math/QuaternionMath.h:409` | 未匹配 |
| 75 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BCDAB0` | `src/sdk/moho/math/QuaternionMath.h:455` | 未匹配 |
| 76 | `kQuatYawPos45 (dynamic initializer)` | `0x00BCDB10` | `src/sdk/moho/math/QuaternionMath.h:501` | 未匹配 |
| 77 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BCE220` | `src/sdk/moho/math/QuaternionMath.h:364` | 未匹配 |
| 78 | `kQuatYawPos90 (dynamic initializer)` | `0x00BCE280` | `src/sdk/moho/math/QuaternionMath.h:410` | 未匹配 |
| 79 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BCE2E0` | `src/sdk/moho/math/QuaternionMath.h:456` | 未匹配 |
| 80 | `kQuatYawPos45 (dynamic initializer)` | `0x00BCE340` | `src/sdk/moho/math/QuaternionMath.h:502` | 未匹配 |
| 81 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BCE550` | `src/sdk/moho/math/QuaternionMath.h:365` | 未匹配 |
| 82 | `kQuatYawPos90 (dynamic initializer)` | `0x00BCE5B0` | `src/sdk/moho/math/QuaternionMath.h:411` | 未匹配 |
| 83 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BCE610` | `src/sdk/moho/math/QuaternionMath.h:457` | 未匹配 |
| 84 | `kQuatYawPos45 (dynamic initializer)` | `0x00BCE670` | `src/sdk/moho/math/QuaternionMath.h:503` | 未匹配 |
| 85 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BCF0D0` | `src/sdk/moho/math/QuaternionMath.h:366` | 未匹配 |
| 86 | `kQuatYawPos90 (dynamic initializer)` | `0x00BCF130` | `src/sdk/moho/math/QuaternionMath.h:412` | 未匹配 |
| 87 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BCF190` | `src/sdk/moho/math/QuaternionMath.h:458` | 未匹配 |
| 88 | `kQuatYawPos45 (dynamic initializer)` | `0x00BCF1F0` | `src/sdk/moho/math/QuaternionMath.h:504` | 未匹配 |
| 89 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BCF320` | `src/sdk/moho/math/QuaternionMath.h:367` | 未匹配 |
| 90 | `kQuatYawPos90 (dynamic initializer)` | `0x00BCF380` | `src/sdk/moho/math/QuaternionMath.h:413` | 未匹配 |
| 91 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BCF3E0` | `src/sdk/moho/math/QuaternionMath.h:459` | 未匹配 |
| 92 | `kQuatYawPos45 (dynamic initializer)` | `0x00BCF440` | `src/sdk/moho/math/QuaternionMath.h:505` | 未匹配 |
| 93 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BCF590` | `src/sdk/moho/math/QuaternionMath.h:368` | 未匹配 |
| 94 | `kQuatYawPos90 (dynamic initializer)` | `0x00BCF5F0` | `src/sdk/moho/math/QuaternionMath.h:414` | 未匹配 |
| 95 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BCF650` | `src/sdk/moho/math/QuaternionMath.h:460` | 未匹配 |
| 96 | `kQuatYawPos45 (dynamic initializer)` | `0x00BCF6B0` | `src/sdk/moho/math/QuaternionMath.h:506` | 未匹配 |
| 97 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BCFAC0` | `src/sdk/moho/math/QuaternionMath.h:369` | 未匹配 |
| 98 | `kQuatYawPos90 (dynamic initializer)` | `0x00BCFB20` | `src/sdk/moho/math/QuaternionMath.h:415` | 未匹配 |
| 99 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BCFB80` | `src/sdk/moho/math/QuaternionMath.h:461` | 未匹配 |
| 100 | `kQuatYawPos45 (dynamic initializer)` | `0x00BCFBE0` | `src/sdk/moho/math/QuaternionMath.h:507` | 未匹配 |
| 101 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BCFE30` | `src/sdk/moho/math/QuaternionMath.h:370` | 未匹配 |
| 102 | `kQuatYawPos90 (dynamic initializer)` | `0x00BCFE90` | `src/sdk/moho/math/QuaternionMath.h:416` | 未匹配 |
| 103 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BCFEF0` | `src/sdk/moho/math/QuaternionMath.h:462` | 未匹配 |
| 104 | `kQuatYawPos45 (dynamic initializer)` | `0x00BCFF50` | `src/sdk/moho/math/QuaternionMath.h:508` | 未匹配 |
| 105 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BD0080` | `src/sdk/moho/math/QuaternionMath.h:371` | 未匹配 |
| 106 | `kQuatYawPos90 (dynamic initializer)` | `0x00BD00E0` | `src/sdk/moho/math/QuaternionMath.h:417` | 未匹配 |
| 107 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BD0140` | `src/sdk/moho/math/QuaternionMath.h:463` | 未匹配 |
| 108 | `kQuatYawPos45 (dynamic initializer)` | `0x00BD01A0` | `src/sdk/moho/math/QuaternionMath.h:509` | 未匹配 |
| 109 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BD0390` | `src/sdk/moho/math/QuaternionMath.h:372` | 未匹配 |
| 110 | `kQuatYawPos90 (dynamic initializer)` | `0x00BD03F0` | `src/sdk/moho/math/QuaternionMath.h:418` | 未匹配 |
| 111 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BD0450` | `src/sdk/moho/math/QuaternionMath.h:464` | 未匹配 |
| 112 | `kQuatYawPos45 (dynamic initializer)` | `0x00BD04B0` | `src/sdk/moho/math/QuaternionMath.h:510` | 未匹配 |
| 113 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BD0760` | `src/sdk/moho/math/QuaternionMath.h:373` | 未匹配 |
| 114 | `kQuatYawPos90 (dynamic initializer)` | `0x00BD07C0` | `src/sdk/moho/math/QuaternionMath.h:419` | 未匹配 |
| 115 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BD0820` | `src/sdk/moho/math/QuaternionMath.h:465` | 未匹配 |
| 116 | `kQuatYawPos45 (dynamic initializer)` | `0x00BD0880` | `src/sdk/moho/math/QuaternionMath.h:511` | 未匹配 |
| 117 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BD0A10` | `src/sdk/moho/math/QuaternionMath.h:374` | 未匹配 |
| 118 | `kQuatYawPos90 (dynamic initializer)` | `0x00BD0A70` | `src/sdk/moho/math/QuaternionMath.h:420` | 未匹配 |
| 119 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BD0AD0` | `src/sdk/moho/math/QuaternionMath.h:466` | 未匹配 |
| 120 | `kQuatYawPos45 (dynamic initializer)` | `0x00BD0B30` | `src/sdk/moho/math/QuaternionMath.h:512` | 未匹配 |
| 121 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BD0C60` | `src/sdk/moho/math/QuaternionMath.h:375` | 未匹配 |
| 122 | `kQuatYawPos90 (dynamic initializer)` | `0x00BD0CC0` | `src/sdk/moho/math/QuaternionMath.h:421` | 未匹配 |
| 123 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BD0D20` | `src/sdk/moho/math/QuaternionMath.h:467` | 未匹配 |
| 124 | `kQuatYawPos45 (dynamic initializer)` | `0x00BD0D80` | `src/sdk/moho/math/QuaternionMath.h:513` | 未匹配 |
| 125 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BD0EB0` | `src/sdk/moho/math/QuaternionMath.h:376` | 未匹配 |
| 126 | `kQuatYawPos90 (dynamic initializer)` | `0x00BD0F10` | `src/sdk/moho/math/QuaternionMath.h:422` | 未匹配 |
| 127 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BD0F70` | `src/sdk/moho/math/QuaternionMath.h:468` | 未匹配 |
| 128 | `kQuatYawPos45 (dynamic initializer)` | `0x00BD0FD0` | `src/sdk/moho/math/QuaternionMath.h:514` | 未匹配 |
| 129 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BD1160` | `src/sdk/moho/math/QuaternionMath.h:377` | 未匹配 |
| 130 | `kQuatYawPos90 (dynamic initializer)` | `0x00BD11C0` | `src/sdk/moho/math/QuaternionMath.h:423` | 未匹配 |
| 131 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BD1220` | `src/sdk/moho/math/QuaternionMath.h:469` | 未匹配 |
| 132 | `kQuatYawPos45 (dynamic initializer)` | `0x00BD1280` | `src/sdk/moho/math/QuaternionMath.h:515` | 未匹配 |
| 133 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BD13F0` | `src/sdk/moho/math/QuaternionMath.h:378` | 未匹配 |
| 134 | `kQuatYawPos90 (dynamic initializer)` | `0x00BD1450` | `src/sdk/moho/math/QuaternionMath.h:424` | 未匹配 |
| 135 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BD14B0` | `src/sdk/moho/math/QuaternionMath.h:470` | 未匹配 |
| 136 | `kQuatYawPos45 (dynamic initializer)` | `0x00BD1510` | `src/sdk/moho/math/QuaternionMath.h:516` | 未匹配 |
| 137 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BD1640` | `src/sdk/moho/math/QuaternionMath.h:379` | 未匹配 |
| 138 | `kQuatYawPos90 (dynamic initializer)` | `0x00BD16A0` | `src/sdk/moho/math/QuaternionMath.h:425` | 未匹配 |
| 139 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BD1700` | `src/sdk/moho/math/QuaternionMath.h:471` | 未匹配 |
| 140 | `kQuatYawPos45 (dynamic initializer)` | `0x00BD1760` | `src/sdk/moho/math/QuaternionMath.h:517` | 未匹配 |
| 141 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BD1AB0` | `src/sdk/moho/math/QuaternionMath.h:380` | 未匹配 |
| 142 | `kQuatYawPos90 (dynamic initializer)` | `0x00BD1B10` | `src/sdk/moho/math/QuaternionMath.h:426` | 未匹配 |
| 143 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BD1B70` | `src/sdk/moho/math/QuaternionMath.h:472` | 未匹配 |
| 144 | `kQuatYawPos45 (dynamic initializer)` | `0x00BD1BD0` | `src/sdk/moho/math/QuaternionMath.h:518` | 未匹配 |
| 145 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BD1DE0` | `src/sdk/moho/math/QuaternionMath.h:381` | 未匹配 |
| 146 | `kQuatYawPos90 (dynamic initializer)` | `0x00BD1E40` | `src/sdk/moho/math/QuaternionMath.h:427` | 未匹配 |
| 147 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BD1EA0` | `src/sdk/moho/math/QuaternionMath.h:473` | 未匹配 |
| 148 | `kQuatYawPos45 (dynamic initializer)` | `0x00BD1F00` | `src/sdk/moho/math/QuaternionMath.h:519` | 未匹配 |
| 149 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BD2840` | `src/sdk/moho/math/QuaternionMath.h:382` | 未匹配 |
| 150 | `kQuatYawPos90 (dynamic initializer)` | `0x00BD28A0` | `src/sdk/moho/math/QuaternionMath.h:428` | 未匹配 |
| 151 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BD2900` | `src/sdk/moho/math/QuaternionMath.h:474` | 未匹配 |
| 152 | `kQuatYawPos45 (dynamic initializer)` | `0x00BD2960` | `src/sdk/moho/math/QuaternionMath.h:520` | 未匹配 |
| 153 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BD32E0` | `src/sdk/moho/math/QuaternionMath.h:383` | 未匹配 |
| 154 | `kQuatYawPos90 (dynamic initializer)` | `0x00BD3340` | `src/sdk/moho/math/QuaternionMath.h:429` | 未匹配 |
| 155 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BD33A0` | `src/sdk/moho/math/QuaternionMath.h:475` | 未匹配 |
| 156 | `kQuatYawPos45 (dynamic initializer)` | `0x00BD3400` | `src/sdk/moho/math/QuaternionMath.h:521` | 未匹配 |
| 157 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BD4A20` | `src/sdk/moho/math/QuaternionMath.h:384` | 未匹配 |
| 158 | `kQuatYawPos90 (dynamic initializer)` | `0x00BD4A80` | `src/sdk/moho/math/QuaternionMath.h:430` | 未匹配 |
| 159 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BD4AE0` | `src/sdk/moho/math/QuaternionMath.h:476` | 未匹配 |
| 160 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BD6170` | `src/sdk/moho/math/QuaternionMath.h:385` | 未匹配 |
| 161 | `kQuatYawPos90 (dynamic initializer)` | `0x00BD61D0` | `src/sdk/moho/math/QuaternionMath.h:431` | 未匹配 |
| 162 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BD6230` | `src/sdk/moho/math/QuaternionMath.h:477` | 未匹配 |
| 163 | `kQuatYawPos45 (dynamic initializer)` | `0x00BD6290` | `src/sdk/moho/math/QuaternionMath.h:522` | 未匹配 |
| 164 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BD6870` | `src/sdk/moho/math/QuaternionMath.h:386` | 未匹配 |
| 165 | `kQuatYawPos90 (dynamic initializer)` | `0x00BD68D0` | `src/sdk/moho/math/QuaternionMath.h:432` | 未匹配 |
| 166 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BD6930` | `src/sdk/moho/math/QuaternionMath.h:478` | 未匹配 |
| 167 | `kQuatYawPos45 (dynamic initializer)` | `0x00BD6990` | `src/sdk/moho/math/QuaternionMath.h:523` | 未匹配 |
| 168 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BD6DE0` | `src/sdk/moho/math/QuaternionMath.h:387` | 未匹配 |
| 169 | `kQuatYawPos90 (dynamic initializer)` | `0x00BD6E40` | `src/sdk/moho/math/QuaternionMath.h:433` | 未匹配 |
| 170 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BD6EA0` | `src/sdk/moho/math/QuaternionMath.h:479` | 未匹配 |
| 171 | `kQuatYawPos45 (dynamic initializer)` | `0x00BD6F00` | `src/sdk/moho/math/QuaternionMath.h:524` | 未匹配 |
| 172 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BD7330` | `src/sdk/moho/math/QuaternionMath.h:388` | 未匹配 |
| 173 | `kQuatYawPos90 (dynamic initializer)` | `0x00BD7390` | `src/sdk/moho/math/QuaternionMath.h:434` | 未匹配 |
| 174 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BD73F0` | `src/sdk/moho/math/QuaternionMath.h:480` | 未匹配 |
| 175 | `kQuatYawPos45 (dynamic initializer)` | `0x00BD7450` | `src/sdk/moho/math/QuaternionMath.h:525` | 未匹配 |
| 176 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BD75A0` | `src/sdk/moho/math/QuaternionMath.h:389` | 未匹配 |
| 177 | `kQuatYawPos90 (dynamic initializer)` | `0x00BD7600` | `src/sdk/moho/math/QuaternionMath.h:435` | 未匹配 |
| 178 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BD7660` | `src/sdk/moho/math/QuaternionMath.h:481` | 未匹配 |
| 179 | `kQuatYawPos45 (dynamic initializer)` | `0x00BD76C0` | `src/sdk/moho/math/QuaternionMath.h:526` | 未匹配 |
| 180 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BD7790` | `src/sdk/moho/math/QuaternionMath.h:390` | 未匹配 |
| 181 | `kQuatYawPos90 (dynamic initializer)` | `0x00BD77F0` | `src/sdk/moho/math/QuaternionMath.h:436` | 未匹配 |
| 182 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BD7850` | `src/sdk/moho/math/QuaternionMath.h:482` | 未匹配 |
| 183 | `kQuatYawPos45 (dynamic initializer)` | `0x00BD78B0` | `src/sdk/moho/math/QuaternionMath.h:527` | 未匹配 |
| 184 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BD8590` | `src/sdk/moho/math/QuaternionMath.h:391` | 未匹配 |
| 185 | `kQuatYawPos90 (dynamic initializer)` | `0x00BD85F0` | `src/sdk/moho/math/QuaternionMath.h:437` | 未匹配 |
| 186 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BD8650` | `src/sdk/moho/math/QuaternionMath.h:483` | 未匹配 |
| 187 | `kQuatYawPos45 (dynamic initializer)` | `0x00BD86B0` | `src/sdk/moho/math/QuaternionMath.h:528` | 未匹配 |
| 188 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BD90E0` | `src/sdk/moho/math/QuaternionMath.h:392` | 未匹配 |
| 189 | `kQuatYawPos90 (dynamic initializer)` | `0x00BD9140` | `src/sdk/moho/math/QuaternionMath.h:438` | 未匹配 |
| 190 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BD91A0` | `src/sdk/moho/math/QuaternionMath.h:484` | 未匹配 |
| 191 | `kQuatYawPos45 (dynamic initializer)` | `0x00BD9200` | `src/sdk/moho/math/QuaternionMath.h:529` | 未匹配 |
| 192 | `kQuatYawNeg90 (dynamic initializer)` | `0x00BD9680` | `src/sdk/moho/math/QuaternionMath.h:393` | 未匹配 |
| 193 | `kQuatYawPos90 (dynamic initializer)` | `0x00BD96E0` | `src/sdk/moho/math/QuaternionMath.h:439` | 未匹配 |
| 194 | `kQuatYawNeg45 (dynamic initializer)` | `0x00BD9740` | `src/sdk/moho/math/QuaternionMath.h:485` | 未匹配 |
| 195 | `kQuatYawPos45 (dynamic initializer)` | `0x00BD97A0` | `src/sdk/moho/math/QuaternionMath.h:530` | 未匹配 |

## Why a row is 不适用

* `0x0044F7E0` NormalizeVectorInto(Wm3::Vector3f* const dest, const Wm3::Vector3f& source) - difffuzz already covers it; fafdiff adds no dimension
* `0x004E9DF0` atan2f(const float y, const float x) - difffuzz already covers it; fafdiff adds no dimension
* `0x004EB3F0` MatrixToQuat(const Wm3::Vector3f* const matrixRows, Wm3::Quaternionf* const out) - difffuzz already covers it; fafdiff adds no dimension
* `0x004EBA80` QuatLERP(const Wm3::Quaternionf* const q1, const Wm3::Quaternionf* const q2, Wm3::Quaternionf* const out, float amount) - difffuzz already covers it; fafdiff adds no dimension
* `0x004EBC00` SLERP(const Wm3::Quaternionf* const q2, const Wm3::Quaternionf* const q1, Wm3::Quaternionf* const out, const float t) - difffuzz already covers it; fafdiff adds no dimension
* `0x004EDAA0` NormalizeQuatInPlace(Wm3::Quaternionf* const quat) - difffuzz already covers it; fafdiff adds no dimension
* `0x004EDED0` QuatsNearEqual(const Wm3::Quaternionf& lhs, const Wm3::Quaternionf& rhs) - difffuzz already covers it; fafdiff adds no dimension
* `0x004F0CB0` MatrixRowsToQuatCanonical(const Wm3::Vector3f* const matrixRows, Wm3::Quaternionf* const out) - difffuzz already covers it; fafdiff adds no dimension
* `0x005382E0` VEC_LargestAxis(const Wm3::Vector3f& value) - difffuzz already covers it; fafdiff adds no dimension
* `0x00565A10` VEC_HeadingFromXZ(const Wm3::Vector3f* const value) - difffuzz already covers it; fafdiff adds no dimension
* `0x00570750` EulerRollToQuat(const Wm3::Vector3f* const axis, Wm3::Quatf* const outQuaternion, const float roll) - difffuzz already covers it; fafdiff adds no dimension
* `0x005734D0` cosf(const float x) - difffuzz already covers it; fafdiff adds no dimension
* `0x005734E0` sinf(const float x) - difffuzz already covers it; fafdiff adds no dimension
* `0x005B1C90` VecSetLength(Wm3::Vector3f* const vector, const float targetLength) - difffuzz already covers it; fafdiff adds no dimension
* `0x0062FB50` NormalizeAngleSignedRadians(const float angleRadians) - difffuzz already covers it; fafdiff adds no dimension
* `0x00697360` QuatFromAxisAngleVector(Wm3::Quaternionf* const quat, Wm3::Vector3f axisAngle) - difffuzz already covers it; fafdiff adds no dimension
* `0x0069A1D0` IsUnitLengthVector3f(const Wm3::Vector3f& value) - difffuzz already covers it; fafdiff adds no dimension
* `0x0069A360` VecSetLengthTo(Wm3::Vector3f* const destination, const Wm3::Vector3f* const targetLengthVector, const Wm3::Vector3f* const sourceVector) - difffuzz already covers it; fafdiff adds no dimension
* `0x006C1070` QuatToAxisAndAngle(const Wm3::Quaternionf& quaternion, Wm3::Vector3f* const axisOut, float* const angleRadiansOut) - difffuzz already covers it; fafdiff adds no dimension

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
tier == 'x87' AND section_is_code AND x87_to_first_ret > 0 AND (file starts with 'src/sdk/moho/math/' OR 'src/sdk/legacy/math/') AND NOT file ends with 'MathReflection.cpp' AND NOT '/backends/' in file AND NOT 'D3D' in file
```

The set currently holds **195** functions; the generator asserts that number
and fails loudly if the tree, the screen or the predicate changes it.

The `calls <= 3` clause of the original specification is **not** applied:
on the current tree it removes 11 rows and leaves 184, not 195. The rows
it would remove are listed here, so the difference is explicit:

* `0x004EBC00` Moho::SLERP (`src/sdk/moho/math/QuaternionMath.cpp:466`)
* `0x004EDED0` func_QuatsNearEqual (`src/sdk/moho/math/QuaternionMath.cpp:201`)
* `0x004F0AE0` MatrixColumnsToQuatCanonical (`src/sdk/moho/math/QuaternionMath.cpp:391`)
* `0x0050CB50` <unknown> (`src/sdk/moho/math/QuaternionMath.cpp:983`)
* `0x006D1E30` sub_6D1E30 (`src/sdk/moho/math/QuaternionMath.cpp:840`)
* `0x00BCFF50` x00BD01A0 (`src/sdk/moho/math/QuaternionMath.h:508`)
* `0x00BD0B30` x00BD0D80 (`src/sdk/moho/math/QuaternionMath.h:512`)
* `0x00BD0D80` x00BD0FD0 (`src/sdk/moho/math/QuaternionMath.h:513`)
* `0x00BD0FD0` x00BD1280 (`src/sdk/moho/math/QuaternionMath.h:514`)
* `0x00BD1280` x00BD1510 (`src/sdk/moho/math/QuaternionMath.h:515`)
* `0x00BD1760` x00BD1BD0 (`src/sdk/moho/math/QuaternionMath.h:517`)

Set `CALLS_LIMIT = 3` in the generator to apply the clause; the count
assertion then fails loudly, which is the intended behaviour.

**Do not edit this file by hand.** Every line below the header is written
by the generator. Edit `tools/fafdiff/data/coverage_status.json` and
re-run the generator instead.

## Notes

Rows whose screen symbol is an IDA-style name (`sub_6D1E30`,
`func_QuatLERP`, `x00BCB540`, ...) carry the source-level name read
from the annotated source line instead:

* `0x00473DF0` screen symbol `sub_473DF0` -> `RayBoxSlabClip(RaySlabState& state, const Wm3::AxisAlignedBox3f& box)` (src/sdk/moho/math/Wm3AxisAlignedBox3FafExtras.cpp:105)
* `0x004EB3F0` screen symbol `func_MatrixToQuat` -> `MatrixToQuat(const Wm3::Vector3f* const matrixRows, Wm3::Quaternionf* const out)` (src/sdk/moho/math/QuaternionMath.cpp:256)
* `0x004EB740` screen symbol `fun_RotateQuat` -> `RotateQuatByAngle(Wm3::Quaternionf* quat, float rads)` (src/sdk/moho/math/QuaternionMath.cpp:557)
* `0x004EB830` screen symbol `sub_4EB830` -> `BlendOrientationDeltaByMaxAngle(const Wm3::Quaternionf& goalOrientation, const Wm3::Quaternionf& currentOrientation, const float turnStepRadians, bool* const outNoStep, Wm3::Quaternionf* const outOrientation)` (src/sdk/moho/math/QuaternionMath.cpp:739)
* `0x004EBA80` screen symbol `func_QuatLERP` -> `QuatLERP(const Wm3::Quaternionf* const q1, const Wm3::Quaternionf* const q2, Wm3::Quaternionf* const out, float amount)` (src/sdk/moho/math/QuaternionMath.cpp:408)
* `0x004EDAA0` screen symbol `func_NormalizeQuatInPlace` -> `NormalizeQuatInPlace(Wm3::Quaternionf* const quat)` (src/sdk/moho/math/QuaternionMath.cpp:220)
* `0x004EDED0` screen symbol `func_QuatsNearEqual` -> `QuatsNearEqual(const Wm3::Quaternionf& lhs, const Wm3::Quaternionf& rhs)` (src/sdk/moho/math/QuaternionMath.cpp:201)
* `0x0069AA50` screen symbol `sub_69AA50` -> `QuatFromVecRot(Wm3::Quaternionf* quat, const Wm3::Vector3f* refAxis, float rads)` (src/sdk/moho/math/QuaternionMath.cpp:606)
* `0x006D1E30` screen symbol `sub_6D1E30` -> `BuildRotationMatrixFromEulerHPR(Wm3::Vector3f* const outMatrix, const float heading, const float pitch, const float roll)` (src/sdk/moho/math/QuaternionMath.cpp:840)

Rows marked *variable* are **compiler-generated dynamic initializers**
of a `static const` object (one copy per including translation unit),
not hand-written functions. There are 159 of them; they are x87 code
in the retail image and therefore belong to the set, but they cannot be
paired one-by-one -- they are one row per TU copy of the same source
object. (Their screen symbol is the *next* address label, because the
screen script reads the first symbol after the annotation line, which
for these rows is the following `Address:` entry.)

