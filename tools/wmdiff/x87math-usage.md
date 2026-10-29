# `src/legacy/math/X87Math` — what it is, who calls it, and whether it hides the WM3.8 float mismatches

Scope: read-only investigation of `D:/D/BC/2026/faf-re`. No build was run.

Question answered: *does the engine rebuild work around Wild Magic 3.8's floating-point
differences by routing its maths through its own x87 module (`src/legacy/math/X87Math.*`)
instead of changing the WM3.8 source?*

**Short answer: no.** `X87Math` covers a *different* set of operations from the eight
mismatching WM3.8 functions. It is the engine's own reproduction of the **retail engine's
inlined x87 code** (functions the retail binary implemented inline in its own object code,
which have no WM3.8 counterpart at all). The eight mismatching WM3.8 library bodies are
still compiled into the engine and are still called by name — `Wm3::Vector3f::LengthSq`,
`Wm3::Vec3f::Dot`, `Wm3::DistVector3Segment3f` — so those mismatches reach the game.
For the remaining five, the engine simply never calls them.

---

## 1. What `X87Math` provides

`src/legacy/math/X87Math.h` declares everything in `namespace msvc8`. The implementation
(`src/legacy/math/X87Math.cpp`) is **inline x87 assembly on x86** (`__asm { fld … }`) and
**the same instruction sequences in MASM on x64** (`src/legacy/math/X87Math64.asm`, one
leaf routine per helper, e.g. `msvc8_x87_sinf PROC`, `msvc8_x87_bilinear_height PROC`).
The x64 file does not change the answer — it exists only because MSVC has no inline
assembly on x64, and it reproduces the *same* x87 instruction sequences, so x86, x64 and
the shipped binary agree bit for bit. A few helpers are software models instead of
assembly (they are listed as "software" below):

* `RoundExactProductToFloat` / `RoundProductX87` (`X87Math.cpp:80`, `:104`) — model an
  x87 `fmul` under `_PC_24` followed by `fstp dword` in `double` + `std::fma` residual
  recovery.
* `TruncateToInt` (`X87Math.cpp:131`), `fmod` (`:280`), `frexp` (`:293`), `MultiplyX87`
  (`:310`), `MultiplyIntegerX87` (`:315`), `HalfSumX87` (`:320`),
  `HalfAbsDifferenceX87` (`:333`), `MulTwisterWord` (`:405`).

| `X87Math` function | what it emulates | call sites in `src/` | what it replaces |
|---|---|---|---|
| `RoundToInt(float)` | x87 `fld; fistp` under the caller's rounding mode (`:117`) | `moho/resource/CSimResources.cpp:22`; `moho/unit/core/Unit.cpp:15592-15593`; `moho/unit/core/UnitWeapon.cpp:3365-3366`; `moho/unit/CUnitMotion.cpp:2165-2166,3361-3362`; `moho/entity/Entity.cpp:8068,8070`; `moho/script/CUnitScriptTask.cpp:555-556,636-637`; `moho/projectile/Projectile.cpp:780`; `moho/ai/CAiBrain.cpp:4055-4058`; `moho/ai/CAiPersonality.cpp:364`; `moho/sim/SimDriver.cpp:253` | the retail's `FUN_006BDEE0` float→cell conversion (not a WM3.8 member) |
| `TruncateToInt(float)` | MSVC8 `__ftol2_sse`: truncation toward zero, `0x80000000` for NaN/out-of-range (`:131`) | `lua/LuaObject.cpp:20860` | retail's `cvttsd2si` behaviour |
| `MulTwisterWord(float,uint32)` | `fild word; fadd 2^32; fmul factor` under PC24 — one rounding of the exact product (`:405`) | `moho/unit/CUnitMotion.cpp:108`; `moho/sim/CArmyImpl.cpp:1556-1557`; `moho/sim/CRandomStream.cpp:227`; `moho/sim/Sim.cpp:12345,14728`; `moho/projectile/ProjectileStartupRegistrations.cpp:2552-2554` | retail `CRandomStream::FRand` (0x0051B5C0) — not WM3.8 |
| `ScaleVectorByMaxLengthX87(x,y,z,maxLen)` | `fld lenSq; fsqrt; fdivr maxLen` then three multiplies on the x87 stack (`:140`, asm `:153`) | `moho/math/Vector3f.cpp:68` (`moho::VecLimitLengthTo`) | retail `FUN_005D1E70` (inlined in the retail engine) |
| `ScaleVectorToLengthX87(dest,src,lenSq,len)` | `fsqrt` of the float squared length, `fdivr length`, scale kept on the stack across three multiplies (`:179`, asm `:186`) | `moho/math/Vector3f.cpp:168` (`VecSetLength`); `moho/math/QuaternionMath.cpp:715,976`; `moho/ai/CAiPathSpline.cpp:57` | retail `FUN_005B1C90` / `FUN_006C1070` |
| `ScaleVectorX87(dest,src,scale)` | one x87 `fmul` per lane, stored once (denormal-range product differs from SSE) (`:206`, asm `:209`) | `moho/math/Vector3f.cpp:323` (`EulerRollToQuat`); `moho/math/QuaternionMath.cpp:734`; `moho/collision/CColPrimitiveBase.cpp:177` | retail `FUN_00697393` / `FUN_0057077E` |
| `BoxRadiusOnAxisX87(box,axis)` | `(|a2|e2 + |a1|e1) + |a0|e0` summed on the x87 stack (`:227`, asm `:231`) | `moho/collision/CColPrimitiveBase.cpp:67` | retail `ProjectBoxOntoAxis` 0x0047555E |
| `fmod(double,double)` | MSVC8 `_CIfmod` (x87 dispatch): `fmod(-0,y) == +0` (`:280`) | `moho/math/QuaternionMath.cpp:140`; `moho/app/WEmitterWx.cpp:1220`; `lua/LuaObject.cpp:10922`; `moho/effects/rendering/CEfxEmitter.cpp:905,936`; `moho/animation/CAnimTexture.cpp:56`; `moho/sim/CWldSession.cpp:4532,4857,11539`; `moho/ui/UiRuntimeTypes.cpp:14317`; `moho/mesh/Mesh.cpp:4163,4478,4688`; `moho/mesh/HardwareMeshBatch.cpp:965`; `moho/render/camera/CameraImpl.cpp:1405` | the retail's CRT `fmod` |
| `frexp(double,int*)` | MSVC8 `_decomp`/`frexp` (`:293`) | `lua/LuaObject.cpp:10703` | the retail's CRT `frexp` |
| `MultiplyX87(a,b)` / `MultiplyIntegerX87(int,float)` | x87 `fmul` + `fstp dword` under PC24 (one rounding; the integer form needs up to 55 bits) (`:310`, `:315`) | `lua/LuaObject.cpp:11010,11024` (`MultiplyX87`), `:11185,11202` (`MultiplyIntegerX87`) | the retail's `/Oi` `exp`/`log` expansions in the Lua math library |
| `HalfSumX87(a,b)` / `HalfAbsDifferenceX87(a,b)` | `fld a; fadd b; fmul 0.5` on the x87 stack (`:320`, `:333`) | `moho/ai/CAimManipulator.cpp:1264,1268,1266,1270`; `moho/ai/CBuilderArmManipulator.cpp:329,331,333,335` | retail `CAimManipulator::SetFiringArc` 0x00630CB9, `CBuilderArmManipulator::SetAimingArc` 0x00636499 |
| `ScaleVectorToLengthOfX87(dest,src,target,srcLenSq)` | `|t|²`, the division and the `fsqrt` all on the x87 stack (`:338`, asm `:345`) | `moho/math/Vector3f.cpp:226` (`VecSetLengthTo`) | retail `FUN_0069A360` |
| `BilinearHeight(h00,h01,h10,h11,fx,fz)` | terrain interpolation in the sim's 24-bit x87 arithmetic (`:373`, asm `:379`) | `moho/sim/STIMap.cpp:1369` | retail `FUN_0044FB90` (inlined block) |
| `sinf/cosf/tanf/atanf/atan2f` (+ inline `sin/cos/tan/atan/asin/acos/exp/log/atan2/pow` float overloads, `X87Math.h:166-313`) | x87 `fsin`/`fcos`/`fpatan` and the /Oi `exp`/`log`/`log10` expansions (`:415`-`:673`) | `lua/LuaObject.cpp:10804-10875,10954,11132`; `moho/ai/CAiBrain.cpp:136-137`; `moho/ai/CAimManipulator.cpp:535-578,1331-1449`; `moho/ai/CAiFormationInstance.cpp:1388-1389`; `moho/animation/CRotateManipulator.cpp:536`; `moho/ai/CAiPathSpline.cpp:346`; `moho/ai/CBuilderArmManipulator.cpp:275-556`; `moho/entity/Entity.cpp:6573,7839-7911`; `moho/entity/MotorFallDown.cpp:288,467`; `moho/entity/CollisionBeamStartupRegistrations.cpp:185-186`; `moho/entity/CTextureScroller.cpp:360-361`; `moho/math/Vector4f.cpp:121-136`; `moho/math/Vector3f.cpp:145,319-320`; `moho/math/QuaternionMath.cpp:530,580,600,709,729-731,853-858`; `moho/math/MathReflection.cpp:169-174`; `moho/unit/tasks/CUnitCallTransport.cpp:276`; `moho/unit/tasks/CUnitCallLandTransport.cpp:257`; `moho/unit/core/Unit.cpp:11373-11374`; `moho/unit/CUnitMotion.cpp:3162-3165`; `moho/sim/Sim.cpp:6414-6591`; `moho/sim/SimDriver.cpp:1301`; `moho/sim/CWldMap.cpp:2147,3926` | the retail engine's inlined transcendentals (0x005734E0 `fsin`, 0x005734D0 `fcos`, 0x004E9DF0 `fpatan`, Lua `math_*` at 0x00919xxx) |
| `Atan2Difference(...)` / `Atan2Plus(y,x,addend)` | `fpatan` result kept unrounded on the x87 stack (`:488`, `:508`) | `moho/unit/core/UnitWeapon.cpp:341`; `moho/unit/CUnitMotion.cpp:2738,3134`; `moho/ai/CAiFormationInstance.cpp:1377`; `moho/ai/CAimManipulator.cpp:730,1323` | retail `FUN_00695180` fall axis, `CAimManipulator` heading lane 0x00630AB2 |
| `SinTimesCos` / `CosTimesCos` / `SinTimes` | `fsin`/`fcos` results stay unrounded on the stack; only the product rounds (`:525`, `:545`, and `CosTimesCos` in the same file) | `moho/entity/MotorFallDown.cpp:287-289`; `moho/unit/CUnitMotion.cpp:2551` | retail `CUnitMotion::HandleDivingAndSurfacing` 0x006C337A |
| `SlerpSphericalBlend(out,q1,rhs,angle,1-t,t)` | the whole SLERP weight chain on the x87 stack (`:561`, asm `:569`) | `moho/math/QuaternionMath.cpp:538` | retail `FUN_004EBC00` |
| `IntrinsicExp` / `IntrinsicLn` / `IntrinsicLog10` | MSVC8 `/Oi` expansions of `exp`/`log`/`log10` (`f2xm1/fscale`, `fyl2x`) (`:616`-`:673`) | `lua/LuaObject.cpp:10969,10982,10995`; `moho/sim/CRandomStream.cpp:291` | the retail's Lua `math.exp/log/log10` (0x00919D5D, 0x00919CFD, 0x00919D2D) |
| `IntrinsicLnQuotient(x,base)` | two inlined `log` intrinsics then one `fdivp` (`:675`) | `moho/sim/SimDriver.cpp:248` | retail `RATE_GameRateToSpeed` 0x0055AE19 |
| `ScaledLnQuotient(num,den,scale)` | `fld; fdiv; fldln2; fxch; fyl2x; fmul; fstp` (HaStar edge-cost quantizer) | `gpg/core/algorithms/Cluster.cpp:2017` | retail 0x0092D8B0 / 0x00954FDF |
| `VectorLengthZYXX87(lanes)` | `sqrt((z*z + y*y) + x*x)` entirely on the x87 stack (`:292` decl) | `moho/projectile/ProjectileStartupRegistrations.cpp:1654` | retail `cfunc_ProjectileGetCurrentSpeedL` 0x006A2846 |
| `asin(double)` / `acos(double)` / `pow(double,double)` | the MSVC8 CRT `_CI*` **dispatch**: the game's SSE2 lane is off, so the x87 body runs (`atan2(x, sqrt((1+x)(1-x)))`, `fyl2x/frndint/f2xm1/fscale`) | `lua/LuaObject.cpp:10846,10954,11132`; `moho/math/QuaternionMath.cpp:530,709`; `moho/math/Vector4f.cpp:130` | the retail CRT bodies (0x00A8DE20, 0x00A8DCD0, 0x00A8E0E0) |

**Not in the module:** there is *no* `X87Math` equivalent of `Vector3<float>::Dot`,
`Vector3<float>::LengthSq`, `Math<float>::InvSqrt`, or of any `Dist*`/`GenerateOrthonormalBasis`
body. `VectorLengthZYXX87` is the only length-like helper and it computes a *square root*,
and it exists for one projectile-speed call site.

There is no `X87Math` usage in `tests/` (only `msvc8::string`/`vector`/`auto_ptr` from the
container library).

---

## 2. Redirection: **none**

Exhaustively checked:

* **No macro redirection.** `grep -rn "#define (sin|cos|sqrt|Dot|LengthSq)" src/` → only
  HLSL string literals (`src/gpg/gal/effects/ModernHlsl.cpp:89`,
  `src/gpg/gal/backends/d3d11/EffectSourceD3D11.cpp:744`), which are shader source, not C++.
* **No `using namespace msvc8` for the math part.** The single `using namespace msvc8;`
  in the tree is `src/legacy/containers/Map.cpp:2` (containers, not maths).
* **No `using msvc8::sin;` / typedef / `namespace msvc8 = …` alias for maths.** Every
  use is an explicit `msvc8::` qualification at the call site.
* **No shadow `Wm3::` implementation.** `moho::Vector3f` *is* the WM3.8 type:
  `src/moho/math/Vector3f.h:8` — `using Vector3f = Wm3::Vector3f;`. The engine compiles and
  links the WM3.8 library itself (`cmake/deps/wildmagic/CMakeLists.txt:218-220`,
  `target_link_libraries(faf_engine PUBLIC … Wm3Dx9Application)` at `src/CMakeLists.txt:65`),
  with the same `faf_codegen` flags as the harness (`cmake/deps/wildmagic/CMakeLists.txt:22`).
  So the WM3.8 bodies the harness measures are literally the bodies the engine runs.
* **The `-D`/flag set changes nothing about which maths runs.**
  `cmake/FafCodegen.cmake` only: `/fp:precise`, `-ffp-contract=off`, `-fno-strict-aliasing`,
  `-fwrapv`, `-fno-delete-null-pointer-checks`, `-fno-strict-float-cast-overflow`,
  `-fno-finite-loops`, `/Gy`, `/arch:SSE2` (x86), and `-fno-builtin-*` for the
  transcendental family (`FafCodegen.cmake:40-49`) plus `-fno-builtin-sqrt*` on x86
  (`FafCodegen.cmake:47-49`). Those keep the CRT's x87 bodies reachable; they do not
  substitute them.

### The forced include

`src/platform/X87SqrtCall.h` is `/FI`'d into every **x86** engine TU
(`src/CMakeLists.txt:81-88`; on x64 the `/FI` slot is taken by `platform/X64LayoutAsserts.h`).
Its entire content is:

```c
#if defined(_M_IX86)
#pragma function(sqrt)
#endif
```

That is all it does. It **does not redirect anything else** — no `sin`, no `cos`, no
`sqrt` wrapper, no namespace, no typedef. Its purpose is to stop `/Oi` (implied by `/O2`)
from turning `sqrt(double)` into an inlined SSE2 `sqrtsd`, so the CRT's `fsqrt` (24-bit
under the sim's `_PC_24`) is used instead (header comment lines 5-23). On x64 the same
effect is obtained by `-fno-builtin-sqrt*` in `cmake/FafCodegen.cmake:48`.

The x87 precision control itself is set per thread by
`platform::SetX87PrecisionControl(_PC_24)` (`src/platform/X87Precision.h:14`), called from
e.g. `src/moho/app/WinApp.cpp:2503`, `src/moho/sim/SimDriver.cpp:1118`,
`src/moho/sim/CWldSessionLoaderImpl.cpp:110`, `src/moho/sim/CArmyStats.cpp:971`,
`src/moho/app/FrameDumpCommands.cpp:111`, `src/moho/net/CGpgNetInterface.cpp:2168`.

---

## 3. Coverage of the eight mismatching WM3.8 functions

The eight are the *ordinary-input* float failures listed in
`tools/wmdiff/float-analysis-ordinary.md:12-28` (17 rows / 8 distinct functions; the
full run's 34 rows are 17 ordinary + 17 extreme-input rows,
`tools/wmdiff/float-analysis-extreme.md:31-32`).

| mismatching WM3.8 function | retail addr | does the engine reach the **WM3.8 body**? | `X87Math` equivalent? | decisive call site / evidence |
|---|---|---|---|---|
| `Vector3<float>::Dot` | `0x00452AD0` | **YES** | none | `src/moho/render/camera/VTransform.h:284` — `Wm3::Vec3f::Dot(rotatedNormal, transform.pos_)`; `src/moho/render/CWldTerrainDecal.cpp:65` — `Wm3::Vec3f::Dot(normal, anchor)`. `Wm3::Vec3f` is `Vector3<float>` (`dependencies/WildMagic3p8/Foundation/Math/Wm3Vector3.h:265`), and the body is the mismatching `a.x*b.x + a.y*b.y + a.z*b.z` (`Wm3Vector3.h:145-148`) — the left-to-right association the retail does not use |
| `Vector3<float>::LengthSq` / `SquaredLength` | `0x004757E0` | **YES** | none (no 3-lane squared-length helper exists) | `src/moho/collision/CColPrimitiveBase.cpp:165` — `Wm3::Vector3f::LengthSq(axis)` (the same file then calls `Wm3::SqrtfBinary` and `msvc8::ScaleVectorX87`, i.e. an x87-faithful scale on top of a *mismatching* length); also `CColPrimitiveBase.cpp:394,587,620`; `src/moho/audio/AudioEngine.cpp:1134`; `src/moho/unit/CUnitMotion.cpp:4175,4340,4419`; `src/moho/ai/CAiSteeringImpl.cpp:603,637`; and `CAiSteeringImpl.cpp:198,1458` via `velocity.Length()` (which is `sqrt(LengthSq(v))`, `Wm3Vector3.h:159-162`) |
| `Math<float>::InvSqrt` | `0x0050D150` | **NO** — no engine call site | none | `grep -rn "InvSqrt" src/` → only `moho::CoordsScaleByInvSqrt` (`src/moho/math/QuaternionMath.cpp:972`, own helper), never `Wm3::Math<float>::InvSqrt`. The WM3.8 body is `(Real)(1.0/sqrt((double)fValue))` (`dependencies/WildMagic3p8/Foundation/Math/Wm3Math.inl:93-96`) |
| `DistVector3Segment3<float>` | `0x00A480E0`/`0x00A484F0` | **YES** | none | `src/moho/ui/CUIWorldView.cpp:276` — `Wm3::DistVector3Segment3f pointDistance(points[index], span);`; `src/moho/projectile/Projectile.cpp:1552` — `Wm3::DistVector3Segment3f targetDistance(target->mVarDat.mCurTransform.pos_, segment);` (both include `Wm3DistVector3Segment3.h`, e.g. `Projectile.cpp:22`, `CUIWorldView.cpp:13`) |
| `DistLine3Segment3<float>` | `0x00A809A0`/`0x00A80DE0` | **NO** — no engine call site | none | `grep -rn "DistLine3Segment3" src/` → 0 hits |
| `DistSegment3Segment3<float>` | `0x00A7E2A0`/`0x00A7E480` | **NO** — no engine call site | none | `grep -rn "DistSegment3Segment3" src/` → 0 hits (only a comment in `src/moho/math/Wm3Segment3FafExtras.h:8` listing the class) |
| `DistVector2Box2<float>` | `0x00A6C730`/`0x00A6C880` | **YES** - corrected, see the note below | `Foundation/Intersection/Wm3IntrBox2Circle2.cpp:42` calls `DistVector2Box2f::Get()` from `IntrBox2Circle2f::Test()`; the engine calls that `Test()` at `src/moho/vision/VisionDB.cpp:89`. | The original verdict here was NO, based on `grep -rn "DistVector2Box2" src/` returning only the `VisionShader.h:46` comment. That grep is correct and irrelevant: the call is one level down, inside Wild Magic. |
| `GenerateOrthonormalBasis` | `0x00A3CF80` | **NO** — no engine call site | none | `grep -rn "GenerateOrthonormalBasis\|Orthonormalize" src/` → 0 hits |

---

## 4. Verdict

1. **Does the engine bypass WM3.8 for any of the mismatching operations?**
   **No.** Not one of the eight has an `X87Math` stand-in, and three of them are called
   by name from engine code (`Vector3<float>::Dot`, `Vector3<float>::LengthSq`,
   `DistVector3Segment3<float>`). The WM3.8 tree is not bypassed — it is *linked into* the
   engine and compiled with the same `faf_codegen` flags the harness uses, and
   `moho::Vector3f` is literally `Wm3::Vector3f` (`src/moho/math/Vector3f.h:8`).

2. **Which mismatches are invisible to the engine?**
   Four of the eight, and only because the engine never calls them:
   `Math<float>::InvSqrt`, `DistLine3Segment3<float>`, `DistSegment3Segment3<float>`,
   `GenerateOrthonormalBasis`.

   **Correction (2026-10-29).**  This section first said five, counting
   `DistVector2Box2<float>`.  The 156-row audit in `reachable.md` found the call
   one level down: `IntrBox2Circle2f::Test()` calls it
   (`Wm3IntrBox2Circle2.cpp:42`) and the engine calls that `Test()`
   (`src/moho/vision/VisionDB.cpp:89`).  A direct-name grep cannot see a call
   made inside the library - the same trap the double audit recorded.

3. **Which mismatches still reach the game?**
   * `Vector3<float>::Dot` — via `Wm3::Vec3f::Dot` in `VTransform.h:284` (render camera
     plane transform) and `CWldTerrainDecal.cpp:65` (terrain decal). Retail sums
     right-to-left; the rebuild sums left-to-right → 1 ulp.
   * `Vector3<float>::LengthSq` / `SquaredLength` — via `Wm3::Vector3f::LengthSq` in
     `CColPrimitiveBase.cpp` (box SAT, 4 sites), `AudioEngine.cpp:1134`,
     `CUnitMotion.cpp:4175,4340,4419`, `CAiSteeringImpl.cpp:603,637` and through
     `Vector3<float>::Length()` at `CAiSteeringImpl.cpp:198,1458`. Retail keeps the
     squares and the sum in the x87 register file; the rebuild's SSE2 codegen rounds every
     intermediate to binary32 → differs in the subnormal band.
   * `DistVector3Segment3<float>` — via `Wm3::DistVector3Segment3f` in
     `CUIWorldView.cpp:276` and `Projectile.cpp:1552`.

4. **Is `X87Math` a deliberate upstream workaround for the WM3.8 mismatches?**
   **No — it is a workaround for a different problem.** Every helper in it reproduces
   arithmetic that the **retail engine performed inline in its own code** (the retail's
   `FUN_005D1E70` clamp, `FUN_005B1C90` set-length, `FUN_0069A360`, the quaternion/SLERP
   chains, the Lua `math.*` library, the terrain bilinear block, the `/Oi` transcendentals).
   Those have **no WM3.8 counterpart** — the retail never called a library for them — so
   there is nothing in the WM3.8 tree to fix and the engine re-implements them itself in
   `src/legacy/math/X87Math.*`, `src/moho/math/Vector3f.cpp`, `src/moho/math/QuaternionMath.cpp`.
   This is the *mirror image* of the WM3.8 problem: instead of "engine bypasses WM3.8",
   it is "engine re-implements, in x87-faithful form, what WM3.8 never had to provide".

   Where the two meet, the mix is visible in one function:
   `src/moho/collision/CColPrimitiveBase.cpp:165` computes
   `Wm3::Vector3f::LengthSq(axis)` (mismatching WM3.8 body, SSE2-rounded intermediates)
   and then scales the axis with `msvc8::ScaleVectorX87` (`:177`) — an x87-faithful scale
   built on top of a length that is *not* x87-faithful.

5. **Where the WM3.8 mismatches are supposed to be fixed.**
   Not in `src/`, and not in the compiler options (hard rule in `tools/wmdiff/AGENTS.md`:
   "不要为了让比对通过而修改任何编译选项"). The convergence point is the WM3.8 rebuild
   source itself — `dependencies/WildMagic3p8/Foundation/Math/Wm3Vector3.h:145-158`
   (Dot / LengthSq) and `Wm3Math.inl:93-96` (InvSqrt) — plus the swept `Dist*` bodies
   under `dependencies/WildMagic3p8/Foundation/Distance/`. The tree is a patched
   submodule (`dependencies/patches/wildmagic3p8_faf_required.patch`), and
   `tools/wmdiff/float-analysis-ordinary.md` §1 already names the exact mechanisms
   ("C-order", "C-precision", "C-root").
   Note that the current tree carries **no** x87-modelling helpers of its own — there is
   no `Wm3Detail::X87*` / `RoundTo24` in `dependencies/WildMagic3p8/Foundation/Math/`
   (that vocabulary belongs to an older tree revision quoted in the wmdiff docs), so the
   float-side x87 transcription of the WM3.8 bodies has not been done yet.

### Practical consequence

The three live mismatches (`Dot`, `LengthSq`/`SquaredLength`, `DistVector3Segment3`) are
*not* neutralised by `X87Math`, and `X87Math` cannot neutralise them: it has no counterpart
for a three-lane dot product or squared length. Conversely, the engine's own x87 helpers
must not be "simplified" into plain float expressions, because they encode the retail's
inlined x87 chains, which are a different (and already-matching) surface.
