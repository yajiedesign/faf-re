#include "targets.h"

#include "moho/math/QuaternionMath.h"
#include "moho/math/Vector2f.h"
#include "moho/math/Vector3f.h"
#include "moho/math/Vector4f.h"
#include "legacy/math/X87Math.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace fafdiff
{
namespace
{
  // The table of comparison rows. One row is one paired function: the retail
  // entry point, the engine source that implements it, and the generator that
  // feeds both sides the same typed input.
  //
  // The rows live in a vector rather than a plain array because an empty
  // initializer list is not a valid array in C++ (MSVC: C2079), and the table
  // started out empty. Add rows below, one brace group per row.

  // ---------------------------------------------------------------------
  // NormalizeVectorInto - retail 0x0044F7E0
  //
  // The retail convention, measured from the image (not from the header):
  //
  //   0x0044F7E0  sub  esp, 0x0C
  //   0x0044F7E3  movss xmm2, [edi]          <- the source vector
  //   0x0044F7E7  movss xmm1, [edi + 4]
  //   0x0044F7EC  movss xmm0, [edi + 8]
  //   ...        lengthSq = (x*x + y*y) + z*z   (SSE, float)
  //   0x0044F81A  call 0x00452FC0             <- the retail's own sqrt
  //                (fld; fsqrt - x87, so the length is a _PC_24 fsqrt)
  //   0x0044F81F  fst  dword [esp + 8]        <- the length, rounded to float
  //   0x0044F82A  mov  eax, esi               <- the return value: dest
  //   0x0044F82C  fcompi st(1)                <- length against 0
  //   0x0044F830  jbe  0x0044F869             <- not positive (or NaN): zeros
  //   0x0044F832  divss xmm0, [esp + 4]       <- 1.0f / length
  //   0x0044F84A  movss [esi],      xmm1      <- dest.x = source.x * inv
  //   0x0044F85B  movss [esi + 4],  xmm1      <- dest.y = inv * source.y
  //   0x0044F860  movss [esi + 8],  xmm0      <- dest.z = inv * source.z
  //   0x0044F865  add  esp, 0x0C / ret
  //   0x0044F869  xorps xmm0, xmm0 / three stores / add esp, 0x0C / ret
  //
  // So: dest in ESI, source in EDI, the return value is the destination
  // pointer in EAX, and the epilogue only restores the stack (`add esp, 0x0C`;
  // the `push ecx` at 0x0044F810 is the four-byte slot the float argument to
  // the sqrt call lives in, not a saved register - the function never writes
  // ECX).
  //
  // The zero branch is taken when the length is not positive *or* unordered,
  // i.e. a NaN length also produces the zero vector.

  struct NormIn
  {
    Wm3::Vector3f source;
  };

  struct NormOut
  {
    Wm3::Vector3f dest;
    // The retail returns the destination pointer in EAX (0x0044F82A: `mov eax,
    // esi`, on both paths). The two sides write into *different* buffers, so
    // the pointers themselves cannot be compared; what is compared is the
    // answer to the question the return value answers: is it the destination
    // pointer? 1 = yes, 0 = no.
    std::uint32_t returnedDest;
  };

  // A lane for the Ordinary draw: finite, normal (never subnormal), never
  // exactly zero.
  [[nodiscard]] float GenOrdinaryLane(Rng& rng)
  {
    float value = GenPlain(rng, 100.0f);
    if (!std::isfinite(value) || std::fpclassify(value) == FP_SUBNORMAL || value == 0.0f) {
      value = 1.0f + GenPlain(rng, 10.0f);
      if (value == 0.0f) {
        value = 1.0f;
      }
    }
    return value;
  }

  void GenNorm(Rng& rng, void* in, const DrawKind kind)
  {
    NormIn& typed = *static_cast<NormIn*>(in);
    float v[3] = {0.0f, 0.0f, 0.0f};

    if (kind == DrawKind::Ordinary) {
      // Ordinary: every lane finite, in range and normal - and the vector
      // has a strictly positive length, so the degenerate branch (the retail
      // writes the zero vector when the length is not positive) is *not*
      // reached here. The degenerate case belongs to WithExtremes: it is a
      // corner of the domain, not a well-behaved input.
      for (int k = 0; k < 3; ++k) {
        v[k] = GenOrdinaryLane(rng);
      }
    } else {
      const std::uint32_t mode = rng.Below(100);
      if (mode < 40) {
        // unit vectors, plain vectors, mixed magnitudes, axis-aligned lanes
        GenVec3(rng, v);
      } else if (mode < 55) {
        // the degenerate branch: a zero-length vector (with signed zeros)
        v[0] = 0.0f;
        v[1] = 0.0f;
        v[2] = 0.0f;
        if (rng.Chance(50)) {
          v[0] = -0.0f;
        }
        if (rng.Chance(25)) {
          v[1] = -0.0f;
        }
      } else if (mode < 70) {
        // NaN-laden and infinite lanes: the length is NaN (the retail's
        // `fcompi` is unordered, so the zero branch is taken)
        for (int k = 0; k < 3; ++k) {
          v[k] = GenFloat(rng);
        }
        v[rng.Below(3)] = FromBits(0x7FC00000u); // quiet NaN
        if (rng.Chance(30)) {
          v[rng.Below(3)] = FromBits(0x7F800000u); // +inf
        }
      } else if (mode < 85) {
        // denormal lanes: the squared length underflows, the length is
        // denormal or zero
        for (int k = 0; k < 3; ++k) {
          v[k] = FromBits(0x00000001u + rng.Below(0x007FFFFFu));
          if (rng.Chance(50)) {
            v[k] = -v[k];
          }
        }
      } else {
        // a unit direction at a random magnitude: the squared length
        // overflows to infinity or underflows to zero
        GenUnitVec3(rng, v);
        const double magnitude =
          std::ldexp(rng.Range(1.0, 2.0), static_cast<int>(rng.Below(200)) - 100);
        for (int k = 0; k < 3; ++k) {
          v[k] = static_cast<float>(v[k] * magnitude);
        }
      }
    }

    typed.source = Wm3::Vector3f(v[0], v[1], v[2]);
  }

  // The retail side: dest in ESI, source in EDI, the absolute address of the
  // retail entry point in EAX.
  __declspec(naked) void RetailNormalizeVectorInto(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebx, [esp + 24]      // out
      mov  esi, ebx             // the retail's destination
      mov  edi, [esp + 20]      // the retail's source
      mov  eax, 0x0044F7E0
      call eax
      cmp  eax, ebx         // the retail returns the destination pointer
      sete al
      movzx eax, al
      mov  [ebx + 12], eax   // 1 when it does
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursNormalizeVectorInto(const void* in, void* out)
  {
    const NormIn& typedIn = *static_cast<const NormIn*>(in);
    NormOut& typedOut = *static_cast<NormOut*>(out);
    typedOut.returnedDest =
      (moho::NormalizeVectorInto(&typedOut.dest, typedIn.source) == &typedOut.dest) ? 1u : 0u;
  }

  // ---------------------------------------------------------------------
  // The quaternion rows.
  //
  // All eight share one convention: the quaternion is stored scalar-first,
  // `w` in lane 0 and `x`, `y`, `z` in lanes 1..3 - the layout
  // `Wm3::Quaternionf` has (`m_afTuple[0]` is `w`). The retail reads lane 0
  // as the scalar in every one of the eight, and the engine's own quaternion
  // code does the same.
  //
  // The retail thunks below are naked functions: they set the registers the
  // retail entry point expects and jump to it. Every convention quoted here
  // was measured from the image with `.tmp/disasm.py <va>`, not read off a
  // header.

  // The Ordinary draw of a quaternion: a unit quaternion whose lanes are all
  // finite and normal. The extreme draw (GenQuat in gen.cpp) adds the
  // unnormalized, the zero and the NaN-laden quaternions.
  void GenOrdinaryQuat(Rng& rng, float* q)
  {
    double d[4];
    do {
      for (double& x : d) {
        x = rng.Range(-1.0, 1.0);
      }
    } while (d[0] * d[0] + d[1] * d[1] + d[2] * d[2] + d[3] * d[3] < 1e-3);
    const double s = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2] + d[3] * d[3]);
    for (int i = 0; i < 4; ++i) {
      q[i] = static_cast<float>(d[i] / s);
    }
  }

  // The Ordinary draw of a quaternion pair: independent ones, nearly equal
  // ones and nearly antipodal ones (the short-arc decision turns on the sign
  // of the dot product, so both sides of zero have to be drawn).
  void GenOrdinaryQuatPair(Rng& rng, float* q1, float* q2)
  {
    GenOrdinaryQuat(rng, q1);
    const std::uint32_t mode = rng.Below(100);
    if (mode < 30) {
      GenOrdinaryQuat(rng, q2);
      return;
    }
    const double sign = (mode < 65) ? 1.0 : -1.0;
    for (int i = 0; i < 4; ++i) {
      q2[i] = static_cast<float>(sign * q1[i] + rng.Range(-1e-5, 1e-5));
    }
  }

  // --- MatrixToQuat - retail 0x004EB3F0 ---------------------------------
  //
  //   0x004EB3F0  sub  esp, 0x20
  //   0x004EB3F3  push ebx / push ebp / push edi
  //   0x004EB3F6  mov  edi, eax         <- the matrix
  //   0x004EB3F8  movss xmm2, [edi]     <- row 0, lane 0
  //   0x004EB3FC  movss xmm1, [edi + 0x10]  <- row 1, lane 0
  //   0x004EB401  movss xmm3, [edi + 0x20]  <- row 2, lane 0
  //   ...
  //   0x004EB491  mov  eax, esi         <- the return value: the destination
  //
  // The matrix is three packed rows of three floats (row 1 starts at +0x0C,
  // row 2 at +0x18), which is exactly `Wm3::Vector3f rows[3]`. The trace is
  // the diagonal, the dominant-axis branch reads the cross-shuffle table at
  // 0x00F61F1C ({1, 2, 0}), and the quaternion is written as
  // [w, x, y, z].

  struct MatQuatIn
  {
    Wm3::Vector3f rows[3];
  };

  struct MatQuatOut
  {
    Wm3::Quaternionf quat;
    std::uint32_t returnedDest;
  };

  // A rotation matrix built from a random axis and angle: the trace is
  // 1 + 2*cos(angle), so the draw covers the trace branch (angle < pi/2) and
  // the dominant-axis branch (angle near pi) alike.
  void GenRotationRows(Rng& rng, Wm3::Vector3f* rows)
  {
    float axis[3];
    GenUnitVec3(rng, axis);
    const double angle = rng.Range(-3.2, 3.2);
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    // R = c*I + (1-c)*a a^T + s*[a]x, in row-major order.
    float lanes[9];
    for (int row = 0; row < 3; ++row) {
      for (int col = 0; col < 3; ++col) {
        const int k = (row + 1) % 3;
        const int l = (row + 2) % 3;
        const double skew = (col == k) ? axis[l] : ((col == l) ? -axis[k] : 0.0);
        lanes[row * 3 + col] = static_cast<float>(
          (row == col ? c : 0.0) + (1.0 - c) * axis[row] * axis[col] + s * skew
        );
      }
    }
    for (int row = 0; row < 3; ++row) {
      rows[row] = Wm3::Vector3f(lanes[row * 3], lanes[row * 3 + 1], lanes[row * 3 + 2]);
    }
  }

  // The extreme lanes of a matrix row: NaN, infinity, denormal and zero
  // lanes, which drive the trace test and the sqrt into their unordered
  // branches.
  void GenExtremeRow(Rng& rng, float* row)
  {
    for (int col = 0; col < 3; ++col) {
      row[col] = GenFloat(rng);
    }
  }

  void GenMatQuat(Rng& rng, void* in, const DrawKind kind)
  {
    MatQuatIn& typed = *static_cast<MatQuatIn*>(in);
    if (kind == DrawKind::Ordinary) {
      // Ordinary: a well-formed rotation matrix - every lane finite and
      // normal.
      GenRotationRows(rng, typed.rows);
      return;
    }
    const std::uint32_t mode = rng.Below(100);
    if (mode < 45) {
      GenRotationRows(rng, typed.rows);
    } else if (mode < 65) {
      // A plain matrix: the trace is the sum of the three diagonal lanes.
      for (int row = 0; row < 3; ++row) {
        float lanes[3];
        for (int col = 0; col < 3; ++col) {
          lanes[col] = GenPlain(rng, 1.0f);
        }
        typed.rows[row] = Wm3::Vector3f(lanes[0], lanes[1], lanes[2]);
      }
    } else {
      for (int row = 0; row < 3; ++row) {
        float lanes[3];
        GenExtremeRow(rng, lanes);
        typed.rows[row] = Wm3::Vector3f(lanes[0], lanes[1], lanes[2]);
      }
    }
  }

  __declspec(naked) void RetailMatrixToQuat(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebx, [esp + 24]       // out
      mov  esi, ebx              // the retail's destination
      mov  eax, [esp + 20]       // the retail's matrix
      mov  ecx, 0x004EB3F0
      call ecx
      cmp  eax, ebx              // the retail returns the destination pointer
      sete al
      movzx eax, al
      mov  [ebx + 16], eax       // 1 when it does
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursMatrixToQuat(const void* in, void* out)
  {
    const MatQuatIn& typedIn = *static_cast<const MatQuatIn*>(in);
    MatQuatOut& typedOut = *static_cast<MatQuatOut*>(out);
    typedOut.returnedDest =
      (moho::MatrixToQuat(typedIn.rows, &typedOut.quat) == &typedOut.quat) ? 1u : 0u;
  }

  // --- RotateQuatByAngle - retail 0x004EB740 ----------------------------
  //
  //   0x004EB740  push ebp / mov ebp, esp / and esp, 0xfffffff8
  //   0x004EB749  fld  dword ptr [ebp + 8]     <- the angle, in radians
  //   0x004EB74C  fmul dword ptr [0x00E4F724]  <- * 0.5f
  //   0x004EB752  fabs
  //   0x004EB758  fld  dword ptr [0x00E4F7A0]  <- pi/2
  //   0x004EB760  fcomi st(1) / jb            <- |rads|/2 < pi/2 continues
  //   ...
  //   0x004EB822  mov  al, 1                   <- the return value
  //
  // So: the quaternion in ESI, the angle on the stack (the first argument),
  // the answer in AL. The half-angle gate rejects |rads|/2 >= pi/2; the
  // second gate rejects a quaternion whose squared axis length is not
  // greater than sin^2 of the half angle.

  struct RotQuatIn
  {
    Wm3::Quaternionf quat;
    float rads;
  };

  struct RotQuatOut
  {
    Wm3::Quaternionf quat;
    std::uint32_t accepted;
  };

  void GenRotQuat(Rng& rng, void* in, const DrawKind kind)
  {
    RotQuatIn& typed = *static_cast<RotQuatIn*>(in);
    float lanes[4];
    if (kind == DrawKind::Ordinary) {
      GenOrdinaryQuat(rng, lanes);
    } else {
      GenQuat(rng, lanes);
    }
    typed.quat = Wm3::Quaternionf(lanes[0], lanes[1], lanes[2], lanes[3]);
    typed.rads = (kind == DrawKind::Ordinary) ? static_cast<float>(rng.Range(-3.0, 3.0)) : GenAngle(rng);
  }

  __declspec(naked) void RetailRotateQuatByAngle(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebp, [esp + 20]       // in
      mov  ebx, [esp + 24]       // out
      // The retail works in place: the input quaternion is copied into the
      // output buffer first, and the retail is handed that buffer.
      mov  eax, [ebp]
      mov  [ebx], eax
      mov  eax, [ebp + 4]
      mov  [ebx + 4], eax
      mov  eax, [ebp + 8]
      mov  [ebx + 8], eax
      mov  eax, [ebp + 12]
      mov  [ebx + 12], eax
      mov  esi, ebx              // the retail's quaternion
      push dword ptr [ebp + 16]      // the angle, the first stack argument
      mov  eax, 0x004EB740
      call eax
      add  esp, 4                // the retail does not pop its argument
      movzx eax, al              // the answer is in AL
      mov  [ebx + 16], eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursRotateQuatByAngle(const void* in, void* out)
  {
    const RotQuatIn& typedIn = *static_cast<const RotQuatIn*>(in);
    RotQuatOut& typedOut = *static_cast<RotQuatOut*>(out);
    typedOut.quat = typedIn.quat;
    typedOut.accepted = moho::RotateQuatByAngle(&typedOut.quat, typedIn.rads) ? 1u : 0u;
  }

  // --- QuatLERP - retail 0x004EBA80 -------------------------------------
  //
  //   0x004EBA80  sub  esp, 0x18 / push esi / push edi
  //   0x004EBA85  mov  esi, eax         <- q1
  //   0x004EBA87  mov  edi, ecx         <- q2
  //   0x004EBA89  call 0x004EDED0       <- QuatsNearEqual(q2, q1)
  //   ...
  //   0x004EBAB8  movss xmm7, [esp + 0x24]  <- the amount, the first argument
  //   ...
  //   0x004EBBF1  mov  eax, ebx         <- the return value: the destination
  //
  // So: q1 in EAX, q2 in ECX, the destination in EBX, the amount on the
  // stack (the first argument, which the retail leaves for the caller to
  // pop). The dot product is summed in storage order (w first), and the
  // endpoint that is negated is q1.

  struct LerpIn
  {
    Wm3::Quaternionf q1;
    Wm3::Quaternionf q2;
    float amount;
  };

  struct LerpOut
  {
    Wm3::Quaternionf quat;
    std::uint32_t returnedDest;
  };

  // The blend parameter, with the ends and the neighbourhood of the ends.
  float GenBlendAmount(Rng& rng)
  {
    switch (rng.Below(10)) {
    case 0:
      return 0.0f;
    case 1:
      return 1.0f;
    case 2:
      return FromBits(0x3F800001u); // 1 + 1 ulp
    case 3:
      return FromBits(0x3F800000u - 1u); // just below 1
    case 4:
      return FromBits(0x00000001u); // the smallest positive denormal
    case 5:
      return -0.0f;
    case 6:
      return FromBits(0x80000000u);
    case 7:
      return FromBits(0x7FC00000u); // NaN
    case 8:
      return static_cast<float>(rng.Range(-0.2, 1.2));
    default:
      return static_cast<float>(rng.Unit());
    }
  }

  void GenLerp(Rng& rng, void* in, const DrawKind kind)
  {
    LerpIn& typed = *static_cast<LerpIn*>(in);
    float q1[4];
    float q2[4];
    if (kind == DrawKind::Ordinary) {
      GenOrdinaryQuatPair(rng, q1, q2);
    } else {
      GenQuatPair(rng, q1, q2);
    }
    typed.q1 = Wm3::Quaternionf(q1[0], q1[1], q1[2], q1[3]);
    typed.q2 = Wm3::Quaternionf(q2[0], q2[1], q2[2], q2[3]);
    typed.amount = (kind == DrawKind::Ordinary) ? static_cast<float>(rng.Unit()) : GenBlendAmount(rng);
  }

  __declspec(naked) void RetailQuatLERP(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebp, [esp + 20]       // in
      mov  ebx, [esp + 24]       // out
      mov  eax, ebp             // q1
      lea  ecx, [ebp + 16]      // q2
      push dword ptr [ebp + 32]      // the amount, the first stack argument
      mov  edx, 0x004EBA80
      call edx
      add  esp, 4                // the retail does not pop its argument
      cmp  eax, ebx              // the retail returns the destination pointer
      sete al
      movzx eax, al
      mov  [ebx + 16], eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursQuatLERP(const void* in, void* out)
  {
    const LerpIn& typedIn = *static_cast<const LerpIn*>(in);
    LerpOut& typedOut = *static_cast<LerpOut*>(out);
    typedOut.returnedDest =
      (moho::QuatLERP(&typedIn.q1, &typedIn.q2, &typedOut.quat, typedIn.amount) == &typedOut.quat)
      ? 1u
      : 0u;
  }

  // --- SLERP - retail 0x004EBC00 ----------------------------------------
  //
  //   0x004EBC00  push ebp / mov ebp, esp / and esp, 0xffffffc0
  //   0x004EBC0B  mov  esi, eax         <- q2
  //   0x004EBC0D  mov  edi, ecx         <- q1
  //   0x004EBC0F  call 0x004EDED0       <- QuatsNearEqual(q1, q2)
  //   ...
  //   0x004EBC3E  movss xmm1, [ebp + 8] <- t, the first stack argument
  //   ...
  //   0x004EBEB0  mov  eax, ebx         <- the return value: the destination
  //
  // So: q2 in EAX, q1 in ECX, the destination in EBX, t on the stack (the
  // first argument, which the retail leaves for the caller to pop). Note the
  // argument order: the retail's first register argument is q2, the second is
  // q1 - the other way round from QuatLERP.

  struct SlerpIn
  {
    Wm3::Quaternionf q2;
    Wm3::Quaternionf q1;
    float t;
  };

  struct SlerpOut
  {
    Wm3::Quaternionf quat;
    std::uint32_t returnedDest;
  };

  void GenSlerp(Rng& rng, void* in, const DrawKind kind)
  {
    SlerpIn& typed = *static_cast<SlerpIn*>(in);
    float q1[4];
    float q2[4];
    if (kind == DrawKind::Ordinary) {
      GenOrdinaryQuatPair(rng, q1, q2);
    } else {
      GenQuatPair(rng, q1, q2);
    }
    typed.q2 = Wm3::Quaternionf(q2[0], q2[1], q2[2], q2[3]);
    typed.q1 = Wm3::Quaternionf(q1[0], q1[1], q1[2], q1[3]);
    typed.t = (kind == DrawKind::Ordinary) ? static_cast<float>(rng.Unit()) : GenBlendAmount(rng);
  }

  __declspec(naked) void RetailSLERP(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebp, [esp + 20]       // in
      mov  ebx, [esp + 24]       // out
      mov  eax, ebp             // q2
      lea  ecx, [ebp + 16]      // q1
      push dword ptr [ebp + 32]      // t, the first stack argument
      mov  edx, 0x004EBC00
      call edx
      add  esp, 4                // the retail does not pop its argument
      cmp  eax, ebx              // the retail returns the destination pointer
      sete al
      movzx eax, al
      mov  [ebx + 16], eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursSLERP(const void* in, void* out)
  {
    const SlerpIn& typedIn = *static_cast<const SlerpIn*>(in);
    SlerpOut& typedOut = *static_cast<SlerpOut*>(out);
    typedOut.returnedDest =
      (moho::SLERP(&typedIn.q2, &typedIn.q1, &typedOut.quat, typedIn.t) == &typedOut.quat) ? 1u : 0u;
  }

  // --- NormalizeQuatInPlace - retail 0x004EDAA0 -------------------------
  //
  //   0x004EDAA0  push ecx
  //   0x004EDAA1  movss xmm3, [esi]        <- w
  //   0x004EDAA5  movss xmm2, [esi + 4]    <- x
  //   0x004EDAAF  movss xmm1, [esi + 8]    <- y
  //   0x004EDAB4  movss xmm0, [esi + 0xC]  <- z
  //   ...        the squares are summed in storage order: ((w^2 + x^2) + y^2) + z^2
  //   0x004EDAE2  call 0x00452FC0          <- the retail's own sqrt (x87 fsqrt)
  //   0x004EDAF6  fcompi st(1) / jbe       <- not greater than 1e-6: zero it
  //   0x004EDB0D  divss xmm1, xmm0         <- 1.0f / length
  //
  // So: the quaternion in ESI, nothing on the stack, nothing returned. The
  // degenerate branch (the length is not positive, or unordered) writes four
  // zero lanes.

  struct NormQuatIn
  {
    Wm3::Quaternionf quat;
  };

  struct NormQuatOut
  {
    Wm3::Quaternionf quat;
  };

  void GenNormQuat(Rng& rng, void* in, const DrawKind kind)
  {
    NormQuatIn& typed = *static_cast<NormQuatIn*>(in);
    float lanes[4];
    if (kind == DrawKind::Ordinary) {
      GenOrdinaryQuat(rng, lanes);
    } else {
      GenQuat(rng, lanes);
    }
    typed.quat = Wm3::Quaternionf(lanes[0], lanes[1], lanes[2], lanes[3]);
  }

  __declspec(naked) void RetailNormalizeQuatInPlace(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebp, [esp + 20]       // in
      mov  ebx, [esp + 24]       // out
      mov  eax, [ebp]
      mov  [ebx], eax
      mov  eax, [ebp + 4]
      mov  [ebx + 4], eax
      mov  eax, [ebp + 8]
      mov  [ebx + 8], eax
      mov  eax, [ebp + 12]
      mov  [ebx + 12], eax
      mov  esi, ebx              // the retail's quaternion
      mov  eax, 0x004EDAA0
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursNormalizeQuatInPlace(const void* in, void* out)
  {
    const NormQuatIn& typedIn = *static_cast<const NormQuatIn*>(in);
    NormQuatOut& typedOut = *static_cast<NormQuatOut*>(out);
    typedOut.quat = typedIn.quat;
    moho::NormalizeQuatInPlace(&typedOut.quat);
  }

  // --- QuatsNearEqual - retail 0x004EDED0 -------------------------------
  //
  //   0x004EDED0  push ecx
  //   0x004EDED1  movss xmm0, [edi]        <- a, lane 0
  //   0x004EDED5  subss xmm0, [esi]        <- a - b
  //   0x004EDEDF  call 0x0040DAB0          <- the retail's fabs
  //   0x004EDEE4  fld  dword ptr [0x00DFFBE8]  <- 1e-6f
  //   0x004EDEED  fcompi st(1) / jb        <- |a - b| < 1e-6 continues
  //   ...
  //   0x004EDF5F  mov  eax, 1              <- the return value
  //
  // So: the first operand in EDI, the second in ESI, the answer in EAX
  // (0 or 1). The lanes are compared in storage order (w first), and the
  // test is |a - b| <= 1e-6.

  struct NearIn
  {
    Wm3::Quaternionf a;
    Wm3::Quaternionf b;
  };

  struct NearOut
  {
    std::uint32_t equal;
  };

  void GenNear(Rng& rng, void* in, const DrawKind kind)
  {
    NearIn& typed = *static_cast<NearIn*>(in);
    float a[4];
    float b[4];
    if (kind == DrawKind::Ordinary) {
      GenOrdinaryQuatPair(rng, a, b);
    } else {
      GenQuatPair(rng, a, b);
    }
    typed.a = Wm3::Quaternionf(a[0], a[1], a[2], a[3]);
    typed.b = Wm3::Quaternionf(b[0], b[1], b[2], b[3]);
  }

  __declspec(naked) void RetailQuatsNearEqual(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebp, [esp + 20]       // in
      mov  ebx, [esp + 24]       // out
      mov  edi, ebp             // a
      lea  esi, [ebp + 16]      // b
      mov  eax, 0x004EDED0
      call eax
      mov  [ebx], eax            // 0 or 1
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursQuatsNearEqual(const void* in, void* out)
  {
    const NearIn& typedIn = *static_cast<const NearIn*>(in);
    NearOut& typedOut = *static_cast<NearOut*>(out);
    typedOut.equal = moho::QuatsNearEqual(typedIn.a, typedIn.b) ? 1u : 0u;
  }

  // --- QuatFromVecRot - retail 0x0069AA50 --------------------------------
  //
  //   0x0069AA50  sub  esp, 0x24 / push ebx / push ebp / push esi / push edi
  //   0x0069AA53  fld  dword ptr [ecx]        <- the reference axis
  //   0x0069AA62  mov  ebp, [esp + 0x30]      <- the quaternion, the first argument
  //   0x0069AA90  movss xmm2, [ebp + 8]       <- the quaternion's lanes
  //   ...
  //   0x0069AAF3  lea  edi, [esp + 0x2C]      <- the fallback axis (0, 1, 0)
  //   0x0069AB04  call 0x0044F880             <- QuatCrossAdd
  //   0x0069AB15  call 0x004EB740             <- RotateQuatByAngle
  //   ...
  //   0x0069AC09  movss [ebp], xmm4           <- the quaternion, in place
  //
  // So: the reference axis in ECX, the quaternion on the stack (the first
  // argument) and the angle on the stack (the second argument). The retail
  // writes the quaternion in place.

  struct VecRotIn
  {
    Wm3::Quaternionf quat;
    Wm3::Vector3f refAxis;
    float rads;
  };

  struct VecRotOut
  {
    Wm3::Quaternionf quat;
  };

  void GenVecRot(Rng& rng, void* in, const DrawKind kind)
  {
    VecRotIn& typed = *static_cast<VecRotIn*>(in);
    float lanes[4];
    if (kind == DrawKind::Ordinary) {
      GenOrdinaryQuat(rng, lanes);
    } else {
      GenQuat(rng, lanes);
    }
    typed.quat = Wm3::Quaternionf(lanes[0], lanes[1], lanes[2], lanes[3]);
    float axis[3];
    if (kind == DrawKind::Ordinary) {
      GenUnitVec3(rng, axis);
    } else {
      GenVec3(rng, axis);
    }
    typed.refAxis = Wm3::Vector3f(axis[0], axis[1], axis[2]);
    typed.rads = (kind == DrawKind::Ordinary) ? static_cast<float>(rng.Range(-3.0, 3.0)) : GenAngle(rng);
  }

  __declspec(naked) void RetailQuatFromVecRot(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebp, [esp + 20]       // in
      mov  ebx, [esp + 24]       // out
      // The retail writes the quaternion in place: the input quaternion is
      // copied into the output buffer first.
      mov  eax, [ebp]
      mov  [ebx], eax
      mov  eax, [ebp + 4]
      mov  [ebx + 4], eax
      mov  eax, [ebp + 8]
      mov  [ebx + 8], eax
      mov  eax, [ebp + 12]
      mov  [ebx + 12], eax
      lea  ecx, [ebp + 16]       // the reference axis
      push dword ptr [ebp + 28]      // the angle, the second stack argument
      push ebx                   // the quaternion, the first stack argument
      mov  eax, 0x0069AA50
      call eax
      add  esp, 8                // the retail does not pop its arguments
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursQuatFromVecRot(const void* in, void* out)
  {
    const VecRotIn& typedIn = *static_cast<const VecRotIn*>(in);
    VecRotOut& typedOut = *static_cast<VecRotOut*>(out);
    typedOut.quat = typedIn.quat;
    moho::QuatFromVecRot(&typedOut.quat, &typedIn.refAxis, typedIn.rads);
  }

  // --- QuatToAxisAndAngle - retail 0x006C1070 ---------------------------
  //
  //   0x006C1070  push ebp / mov ebp, esp / and esp, 0xfffffff8
  //   0x006C1079  movss xmm2, [esi + 8]      <- y
  //   0x006C107E  movss xmm1, [esi + 0xC]    <- z
  //   0x006C1083  movss xmm0, [esi + 4]      <- x
  //   0x006C10A2  comiss xmm0, [0x00D71BFC]  <- the squared axis length, 1e-6f
  //   0x006C10AF  jbe  0x006C111E            <- degenerate: (1, 0, 0) and 0
  //   0x006C10EF  fld  dword ptr [esp + 4]   <- the squared axis length
  //   0x006C10FB  fsqrt / fld1 / fdivrp      <- 1 / |axis|, on the x87 stack
  //   0x006C10FD  movss [ebx], xmm0          <- the angle
  //   0x006C110A  fstp dword ptr [edi]       <- the axis
  //
  // So: the quaternion in ESI, the axis out in EDI, the angle out in EBX.

  struct AxisAngleIn
  {
    Wm3::Quaternionf quat;
  };

  struct AxisAngleOut
  {
    Wm3::Vector3f axis;
    float angle;
  };

  void GenAxisAngle(Rng& rng, void* in, const DrawKind kind)
  {
    AxisAngleIn& typed = *static_cast<AxisAngleIn*>(in);
    float lanes[4];
    if (kind == DrawKind::Ordinary) {
      GenOrdinaryQuat(rng, lanes);
    } else {
      GenQuat(rng, lanes);
    }
    typed.quat = Wm3::Quaternionf(lanes[0], lanes[1], lanes[2], lanes[3]);
  }

  __declspec(naked) void RetailQuatToAxisAndAngle(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebp, [esp + 20]       // in
      mov  ebx, [esp + 24]       // out
      mov  esi, ebp             // the retail's quaternion
      lea  edi, [ebx]            // the retail's axis
      lea  eax, [ebx + 12]       // the retail's angle
      mov  ebx, eax
      mov  eax, 0x006C1070
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursQuatToAxisAndAngle(const void* in, void* out)
  {
    const AxisAngleIn& typedIn = *static_cast<const AxisAngleIn*>(in);
    AxisAngleOut& typedOut = *static_cast<AxisAngleOut*>(out);
    moho::QuatToAxisAndAngle(typedIn.quat, &typedOut.axis, &typedOut.angle);
  }

  // --- QuatCrossAdd - retail 0x0044F880 ----------------------------------
  //
  //   0x0044F880  sub  esp, 0x10 / push esi
  //   0x0044F884  lea  ecx, [esp + 0x18]     <- v1, the first stack argument
  //   0x0044F888  call 0x00452AF0           <- Wm3::Vector3f::Normalize(v1)
  //   0x0044F88F  lea  ecx, [esp + 0x24]     <- v2, the second stack argument
  //   0x0044F893  call 0x00452AF0           <- Wm3::Vector3f::Normalize(v2)
  //   ...
  //   0x0044F970  lea  esi, [esp + 0x18]     <- the degenerate branch
  //   0x0044F974  call 0x0044F7E0           <- NormalizeVectorInto(fallback)
  //   ...
  //   0x0044F965  mov  eax, ebx             <- the return value: the destination
  //
  // So: the destination in EBX, the fallback axis in EDI, and the two
  // vectors BY VALUE on the stack - v1 in the first argument slot, v2 in the
  // second (three floats each). The degenerate branch (the half-vector does
  // not normalize) writes a half turn about the fallback axis: the scalar
  // lane is zero and the imaginary lanes carry the normalised fallback.

  struct CrossAddIn
  {
    Wm3::Vector3f v1;
    Wm3::Vector3f v2;
    Wm3::Vector3f fallback;
  };

  struct CrossAddOut
  {
    Wm3::Quaternionf quat;
    std::uint32_t returnedDest;
  };

  void GenCrossAdd(Rng& rng, void* in, const DrawKind kind)
  {
    CrossAddIn& typed = *static_cast<CrossAddIn*>(in);
    float v1[3];
    float v2[3];
    if (kind == DrawKind::Ordinary) {
      GenUnitVec3(rng, v1);
      // either the same direction, a nearby one, or the opposite one - the
      // opposite one is the degenerate branch.
      const std::uint32_t mode = rng.Below(100);
      if (mode < 40) {
        GenUnitVec3(rng, v2);
      } else if (mode < 70) {
        const double sign = (mode < 55) ? 1.0 : -1.0;
        for (int k = 0; k < 3; ++k) {
          v2[k] = static_cast<float>(sign * v1[k] + rng.Range(-1e-4, 1e-4));
        }
      } else {
        for (int k = 0; k < 3; ++k) {
          v2[k] = -v1[k];
        }
      }
    } else {
      GenVecPair(rng, v1, v2);
    }
    typed.v1 = Wm3::Vector3f(v1[0], v1[1], v1[2]);
    typed.v2 = Wm3::Vector3f(v2[0], v2[1], v2[2]);
    float fallback[3];
    if (kind == DrawKind::Ordinary) {
      GenUnitVec3(rng, fallback);
    } else {
      GenVec3(rng, fallback);
    }
    typed.fallback = Wm3::Vector3f(fallback[0], fallback[1], fallback[2]);
  }

  __declspec(naked) void RetailQuatCrossAdd(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebp, [esp + 20]       // in
      mov  ebx, [esp + 24]       // out
      // The two vectors go on the stack BY VALUE: v1 in the first argument
      // slot, v2 in the second. Each vector is three floats, and the last
      // lane is pushed first.
      push dword ptr [ebp + 20]  // v2.z
      push dword ptr [ebp + 16]  // v2.y
      push dword ptr [ebp + 12]  // v2.x
      push dword ptr [ebp + 8]   // v1.z
      push dword ptr [ebp + 4]   // v1.y
      push dword ptr [ebp]       // v1.x
      mov  edi, ebp
      add  edi, 24              // the fallback axis
      mov  eax, 0x0044F880
      call eax
      add  esp, 24               // the retail does not pop its arguments
      cmp  eax, ebx              // the retail returns the destination pointer
      sete al
      movzx eax, al
      mov  [ebx + 16], eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursQuatCrossAdd(const void* in, void* out)
  {
    const CrossAddIn& typedIn = *static_cast<const CrossAddIn*>(in);
    CrossAddOut& typedOut = *static_cast<CrossAddOut*>(out);
    typedOut.returnedDest =
      (moho::QuatCrossAdd(&typedOut.quat, typedIn.v1, typedIn.v2, typedIn.fallback) == &typedOut.quat)
      ? 1u
      : 0u;
  }

  // ---------------------------------------------------------------------
  // The vector rows.
  //
  // Every convention quoted below was measured from the retail image with
  // `.tmp/disasm.py <va>`, and cross-read against the callers that set the
  // registers up (the callers are only used to confirm the convention; whether
  // the engine calls a function is not what makes it a row).

  // --- Vector4fAssign - retail 0x0046FB20 --------------------------------
  //
  //   0x0046FB20  fld  dword ptr [ecx]      <- the source lane
  //   0x0046FB22  fstp dword ptr [eax]      <- the destination lane
  //   0x0046FB24  fld  dword ptr [ecx + 4]
  //   0x0046FB27  fstp dword ptr [eax + 4]
  //   ...        (four lanes, x87 load/store each)
  //   0x0046FB36  ret
  //
  // So: the destination in EAX, the source in ECX, and the return value is
  // the destination pointer (EAX is never written). The lanes are copied
  // through the x87 stack, so a quiet NaN keeps its payload and only a
  // signalling NaN would be quieted by the load.
  //
  // The comparison is the four destination lanes plus the answer to the
  // question the return value answers: is it the destination pointer?

  struct V4AssignIn
  {
    moho::Vector4f rhs;
  };

  struct V4AssignOut
  {
    moho::Vector4f dest;
    std::uint32_t returnedDest;
  };

  void GenV4Assign(Rng& rng, void* in, const DrawKind kind)
  {
    V4AssignIn& typed = *static_cast<V4AssignIn*>(in);
    float lanes[4];
    if (kind == DrawKind::Ordinary) {
      for (int k = 0; k < 4; ++k) {
        lanes[k] = GenOrdinaryLane(rng);
      }
    } else {
      const std::uint32_t mode = rng.Below(100);
      if (mode < 40) {
        // the full float mixture: NaN, infinity, denormal, signed zero
        for (int k = 0; k < 4; ++k) {
          lanes[k] = GenFloat(rng);
        }
      } else if (mode < 65) {
        // the edge lanes, signed zeros included
        for (int k = 0; k < 4; ++k) {
          lanes[k] = rng.Chance(60) ? GenEdge(rng) : GenPlain(rng, 10.0f);
        }
      } else if (mode < 85) {
        // axis aligned lanes with signed zeros
        static constexpr float kLane[] = {0.0f, -0.0f, 1.0f, -1.0f, 0.5f, -0.5f};
        for (int k = 0; k < 4; ++k) {
          lanes[k] = rng.Chance(25) ? GenPlain(rng, 10.0f) : kLane[rng.Below(6)];
        }
      } else {
        for (int k = 0; k < 4; ++k) {
          lanes[k] = GenPlain(rng, 1000.0f);
        }
      }
    }
    // The lanes are written through the members, not through a
    // `moho::Vector4f` temporary: the copy assignment is the very function
    // this row compares, and a temporary would be assigned into the input
    // with it - which would quietly normalise the input with the code under
    // test.
    typed.rhs.x = lanes[0];
    typed.rhs.y = lanes[1];
    typed.rhs.z = lanes[2];
    typed.rhs.w = lanes[3];
  }

  __declspec(naked) void RetailVector4fAssign(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebx, [esp + 24]      // out
      mov  eax, ebx             // the retail's destination
      mov  ecx, [esp + 20]      // the retail's source
      mov  edx, 0x0046FB20
      call edx
      cmp  eax, ebx             // the retail returns the destination pointer
      sete al
      movzx eax, al
      mov  [ebx + 16], eax      // 1 when it does
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursVector4fAssign(const void* in, void* out)
  {
    const V4AssignIn& typedIn = *static_cast<const V4AssignIn*>(in);
    V4AssignOut& typedOut = *static_cast<V4AssignOut*>(out);
    moho::Vector4f& result = (typedOut.dest = typedIn.rhs);
    typedOut.returnedDest = (&result == &typedOut.dest) ? 1u : 0u;
  }

  // --- VEC_LargestAxis - retail 0x005382E0 --------------------------------
  //
  //   0x005382E0  push ebp / mov ebp, esp / and esp, 0xfffffff8
  //   0x005382E6  fld  dword ptr [ecx + 4]   <- |y|
  //   0x005382ED  fld  dword ptr [ecx]       <- |x|
  //   0x005382F3  fcompi st(1)               <- |y| against |x|
  //   0x005382F7  jbe  0x005382FE            <- not greater: x wins
  //   0x005382F9  mov  edx, 1                <- y wins
  //   0x005382FE  fld  dword ptr [ecx + 8]   <- |z|
  //   0x00538308  fld  dword ptr [ecx + edx*4] <- the winner's magnitude
  //   0x0053830F  fcompi st(1)
  //   0x00538313  ja   0x00538317            <- |z| greater: z wins
  //   0x00538315  mov  eax, edx              <- the x/y winner
  //   0x00538317  mov  esp, ebp / pop ebp / ret
  //
  // So: the vector in ECX, the axis index (0, 1 or 2) in EAX. The `jbe` and
  // `ja` are the *ordered* comparisons, so a NaN lane never wins: the x/y
  // winner survives, and a NaN z keeps it.
  //
  // The comparison is the index - a plain value, not a flag - so this row is
  // a full witness of the tie-break, not a weak boolean one.

  struct LargestAxisIn
  {
    Wm3::Vector3f value;
  };

  struct LargestAxisOut
  {
    std::uint32_t axis;
  };

  void GenLargestAxis(Rng& rng, void* in, const DrawKind kind)
  {
    LargestAxisIn& typed = *static_cast<LargestAxisIn*>(in);
    float lanes[3];
    if (kind == DrawKind::Ordinary) {
      for (int k = 0; k < 3; ++k) {
        lanes[k] = GenOrdinaryLane(rng);
      }
    } else {
      const std::uint32_t mode = rng.Below(100);
      if (mode < 25) {
        // ties: two lanes of exactly the same magnitude - the tie-break is
        // the whole point of this row
        const float a = GenOrdinaryLane(rng);
        const std::uint32_t which = rng.Below(3);
        lanes[0] = (which == 0) ? a : GenPlain(rng, 10.0f);
        lanes[1] = (which == 1) ? a : GenPlain(rng, 10.0f);
        lanes[2] = (which == 2) ? a : GenPlain(rng, 10.0f);
        // and the third lane is either the same magnitude again or a NaN
        if (rng.Chance(30)) {
          lanes[rng.Below(3)] = FromBits(0x7FC00000u);
        } else if (rng.Chance(40)) {
          lanes[rng.Below(3)] = a;
        }
      } else if (mode < 50) {
        // the full mixture: NaN, infinity, denormal, signed zero
        for (int k = 0; k < 3; ++k) {
          lanes[k] = GenFloat(rng);
        }
      } else if (mode < 75) {
        GenVec3(rng, lanes);
      } else {
        // axis aligned lanes, magnitudes equal or not
        static constexpr float kLane[] = {0.0f, -0.0f, 1.0f, -1.0f, 2.0f, -2.0f};
        for (int k = 0; k < 3; ++k) {
          lanes[k] = kLane[rng.Below(6)];
        }
      }
    }
    typed.value = Wm3::Vector3f(lanes[0], lanes[1], lanes[2]);
  }

  __declspec(naked) void RetailVECLargestAxis(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebx, [esp + 24]      // out
      mov  ecx, [esp + 20]      // the retail's vector
      mov  eax, 0x005382E0
      call eax
      mov  [ebx], eax           // the axis index
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursVECLargestAxis(const void* in, void* out)
  {
    const LargestAxisIn& typedIn = *static_cast<const LargestAxisIn*>(in);
    LargestAxisOut& typedOut = *static_cast<LargestAxisOut*>(out);
    typedOut.axis = static_cast<std::uint32_t>(moho::VEC_LargestAxis(typedIn.value));
  }

  // --- VecSetLength - retail 0x005B1C90 ----------------------------------
  //
  //   0x005B1C90  sub  esp, 8
  //   0x005B1C93  movss xmm2, [esi + 4]      <- the vector's lanes
  //   0x005B1CAA  mulss xmm3, xmm2           <- y*y
  //   0x005B1CAE  mulss xmm0, xmm0           <- x*x
  //   0x005B1CB5  addss xmm0, xmm3           <- x*x + y*y
  //   0x005B1CBD  addss xmm0, xmm2           <- + z*z
  //   0x005B1CC1  comiss xmm0, [0x00E4F6E0]  <- against 0.0f
  //   0x005B1CCD  jbe  0x005B1CFE            <- not greater: fail
  //   0x005B1CCF  fld  dword ptr [esp]       <- the squared length
  //   0x005B1CD6  call 0x0040EAF0            <- the retail's sqrt
  //   0x005B1CDB  fdivr dword ptr [esp+0x10] <- targetLength / sqrt(lenSq)
  //   0x005B1CE2  mov  al, 1                 <- the answer
  //   0x005B1CEA  fstp dword ptr [esi]       <- x * scale
  //   0x005B1CF1  fstp dword ptr [esi + 4]   <- scale * y
  //   0x005B1CF7  fstp dword ptr [esi + 8]   <- scale * z
  //   0x005B1CFE  xor  al, al                <- the answer: false
  //
  // So: the vector in ESI (worked in place), the target length as the first
  // stack argument (the retail leaves it for the caller to pop), and the
  // answer in AL. The scale stays on the x87 stack across the three
  // multiplies, one float store per lane.

  struct SetLengthIn
  {
    Wm3::Vector3f vector;
    float targetLength;
  };

  struct SetLengthOut
  {
    Wm3::Vector3f vector;
    std::uint32_t accepted;
  };

  void GenSetLength(Rng& rng, void* in, const DrawKind kind)
  {
    SetLengthIn& typed = *static_cast<SetLengthIn*>(in);
    float lanes[3];
    if (kind == DrawKind::Ordinary) {
      GenUnitVec3(rng, lanes);
    } else {
      const std::uint32_t mode = rng.Below(100);
      if (mode < 25) {
        // the degenerate vector: no positive squared length
        lanes[0] = 0.0f;
        lanes[1] = 0.0f;
        lanes[2] = 0.0f;
        if (rng.Chance(40)) {
          lanes[rng.Below(3)] = -0.0f;
        }
      } else if (mode < 45) {
        // denormal lanes: the squared length underflows to zero
        for (int k = 0; k < 3; ++k) {
          lanes[k] = FromBits(0x00000001u + rng.Below(0x007FFFFFu));
        }
      } else if (mode < 65) {
        // NaN and infinity lanes: the length is unordered or infinite
        for (int k = 0; k < 3; ++k) {
          lanes[k] = GenFloat(rng);
        }
      } else {
        GenVec3(rng, lanes);
      }
    }
    typed.vector = Wm3::Vector3f(lanes[0], lanes[1], lanes[2]);

    if (kind == DrawKind::Ordinary) {
      typed.targetLength = static_cast<float>(rng.Range(0.25, 10.0));
    } else {
      switch (rng.Below(12)) {
      case 0:
        typed.targetLength = 0.0f;
        break;
      case 1:
        typed.targetLength = -0.0f;
        break;
      case 2:
        typed.targetLength = FromBits(0x00000001u); // the smallest denormal
        break;
      case 3:
        typed.targetLength = FromBits(0x7F800000u); // +inf
        break;
      case 4:
        typed.targetLength = FromBits(0xFF800000u); // -inf
        break;
      case 5:
        typed.targetLength = FromBits(0x7FC00000u); // NaN
        break;
      case 6:
        typed.targetLength = FromBits(0x7F7FFFFFu); // FLT_MAX
        break;
      case 7:
        typed.targetLength = FromBits(0x00800000u); // FLT_MIN
        break;
      case 8:
        typed.targetLength = -static_cast<float>(rng.Range(0.25, 10.0));
        break;
      case 9:
        typed.targetLength = static_cast<float>(rng.Range(0.0, 1.0));
        break;
      default:
        typed.targetLength = static_cast<float>(rng.Range(0.25, 100.0));
        break;
      }
    }
  }

  __declspec(naked) void RetailVecSetLength(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebp, [esp + 20]      // in
      mov  ebx, [esp + 24]      // out
      // The retail works in place: the input vector is copied into the output
      // buffer first, and the retail is handed that buffer.
      mov  eax, [ebp]
      mov  [ebx], eax
      mov  eax, [ebp + 4]
      mov  [ebx + 4], eax
      mov  eax, [ebp + 8]
      mov  [ebx + 8], eax
      mov  esi, ebx             // the retail's vector
      push dword ptr [ebp + 12] // the target length, the first stack argument
      mov  eax, 0x005B1C90
      call eax
      add  esp, 4               // the retail does not pop its argument
      movzx eax, al             // the answer is in AL
      mov  [ebx + 12], eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursVecSetLength(const void* in, void* out)
  {
    const SetLengthIn& typedIn = *static_cast<const SetLengthIn*>(in);
    SetLengthOut& typedOut = *static_cast<SetLengthOut*>(out);
    typedOut.vector = typedIn.vector;
    typedOut.accepted = moho::VecSetLength(&typedOut.vector, typedIn.targetLength) ? 1u : 0u;
  }

  // --- VecSetLengthTo - retail 0x0069A360 --------------------------------
  //
  //   0x0069A360  push ebp / mov ebp, esp / and esp, 0xfffffff8
  //   0x0069A369  movss xmm2, [ecx + 4]      <- the source's lanes
  //   0x0069A397  comiss xmm0, [0x00E4F6E0]  <- the source's squared length
  //   0x0069A3A3  jbe  0x0069A3DA            <- not greater: copy the target
  //   0x0069A3A5  fld  dword ptr [edx + 8]   <- the target's lanes
  //   0x0069A3BB  fdiv dword ptr [esp]       <- |target|^2 / sourceLengthSq
  //   0x0069A3BE  fsqrt                     <- the scale
  //   0x0069A3C6  fstp dword ptr [eax]       <- source.x * scale
  //   0x0069A3DA  fld  dword ptr [edx]       <- the fallback: the target
  //   0x0069A3EC  pop  ebp / ret
  //
  // So: the destination in EAX, the source vector in ECX, the target length
  // vector in EDX, and the return value is the destination pointer. The
  // squared lengths and the scale stay on the x87 stack.

  struct SetLengthToIn
  {
    Wm3::Vector3f target;
    Wm3::Vector3f source;
  };

  struct SetLengthToOut
  {
    Wm3::Vector3f dest;
    std::uint32_t returnedDest;
  };

  void GenSetLengthTo(Rng& rng, void* in, const DrawKind kind)
  {
    SetLengthToIn& typed = *static_cast<SetLengthToIn*>(in);
    float target[3];
    float source[3];
    if (kind == DrawKind::Ordinary) {
      GenUnitVec3(rng, target);
      GenUnitVec3(rng, source);
    } else {
      const std::uint32_t mode = rng.Below(100);
      if (mode < 25) {
        // the degenerate source: no positive squared length, so the target is
        // copied instead
        source[0] = 0.0f;
        source[1] = 0.0f;
        source[2] = 0.0f;
        if (rng.Chance(40)) {
          source[rng.Below(3)] = -0.0f;
        }
      } else if (mode < 45) {
        for (int k = 0; k < 3; ++k) {
          source[k] = FromBits(0x00000001u + rng.Below(0x007FFFFFu));
        }
      } else if (mode < 65) {
        for (int k = 0; k < 3; ++k) {
          source[k] = GenFloat(rng);
        }
      } else {
        GenVec3(rng, source);
      }
      if (mode >= 25 && mode < 45) {
        GenVec3(rng, target);
      } else if (rng.Chance(50)) {
        GenVec3(rng, target);
      } else {
        GenUnitVec3(rng, target);
      }
    }
    typed.target = Wm3::Vector3f(target[0], target[1], target[2]);
    typed.source = Wm3::Vector3f(source[0], source[1], source[2]);
  }

  __declspec(naked) void RetailVecSetLengthTo(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebp, [esp + 20]      // in
      mov  esi, [esp + 24]      // out
      mov  eax, esi             // the retail's destination
      lea  ecx, [ebp + 12]      // the retail's source vector
      mov  edx, ebp             // the retail's target length vector
      mov  ebx, 0x0069A360
      call ebx
      cmp  eax, esi             // the retail returns the destination pointer
      sete al
      movzx eax, al
      mov  [esi + 12], eax      // 1 when it does
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursVecSetLengthTo(const void* in, void* out)
  {
    const SetLengthToIn& typedIn = *static_cast<const SetLengthToIn*>(in);
    SetLengthToOut& typedOut = *static_cast<SetLengthToOut*>(out);
    Wm3::Vector3f* const result =
      moho::VecSetLengthTo(&typedOut.dest, &typedIn.target, &typedIn.source);
    typedOut.returnedDest = (result == &typedOut.dest) ? 1u : 0u;
  }

  // --- VEC_HeadingFromXZ - retail 0x00565A10 ------------------------------
  //
  //   0x00565A10  push ecx
  //   0x00565A11  fld  dword ptr [eax + 8]   <- z
  //   0x00565A1B  fld  dword ptr [eax]       <- x
  //   0x00565A20  call 0x004E9DF0            <- the retail's atan2 (fpatan)
  //   0x00565A25  add  esp, 8 / pop ecx / ret
  //
  // So: the vector in EAX, and the answer comes back **on the x87 stack**
  // (the retail's atan2 does not spill it). The harness rounds it to a float
  // exactly once, which is what the engine's own `msvc8::atan2` does as well
  // (its `fstp result`).
  //
  // The retail's atan2 is `fld y; fld x; fpatan` with the two floats on the
  // stack: the retail pushes z first and x second, so the call is
  // atan2(y = x, x = z) - the planar heading of the vector.

  struct HeadingIn
  {
    Wm3::Vector3f value;
  };

  struct HeadingOut
  {
    float heading;
  };

  void GenHeading(Rng& rng, void* in, const DrawKind kind)
  {
    HeadingIn& typed = *static_cast<HeadingIn*>(in);
    float lanes[3];
    if (kind == DrawKind::Ordinary) {
      GenUnitVec3(rng, lanes);
    } else {
      const std::uint32_t mode = rng.Below(100);
      if (mode < 30) {
        // the corners of atan2: the axes, the origin, and the sign corners
        static constexpr float kLane[][2] = {
          { 0.0f, 0.0f },  // the origin
          { 1.0f, 0.0f },  // +x
          { -1.0f, 0.0f }, // -x
          { 0.0f, 1.0f },  // +z
          { 0.0f, -1.0f }, // -z
          { -0.0f, 0.0f }, // -0 in x
          { 0.0f, -0.0f }, // -0 in z
        };
        const std::uint32_t which = rng.Below(7);
        lanes[0] = kLane[which][0];
        lanes[2] = kLane[which][1];
        lanes[1] = GenPlain(rng, 10.0f);
      } else if (mode < 55) {
        // NaN, infinity and denormal lanes: the quadrant is unordered
        for (int k = 0; k < 3; ++k) {
          lanes[k] = GenFloat(rng);
        }
      } else {
        GenVec3(rng, lanes);
      }
    }
    typed.value = Wm3::Vector3f(lanes[0], lanes[1], lanes[2]);
  }

  __declspec(naked) void RetailVECHeadingFromXZ(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebx, [esp + 24]      // out
      mov  eax, [esp + 20]      // the retail's vector
      mov  ecx, 0x00565A10
      call ecx
      fstp dword ptr [ebx]      // the answer comes back on the x87 stack
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursVECHeadingFromXZ(const void* in, void* out)
  {
    const HeadingIn& typedIn = *static_cast<const HeadingIn*>(in);
    HeadingOut& typedOut = *static_cast<HeadingOut*>(out);
    typedOut.heading = moho::VEC_HeadingFromXZ(&typedIn.value);
  }

  // --- EulerRollToQuat - retail 0x00570750 -------------------------------
  //
  //   0x00570750  sub  esp, 8
  //   0x00570753  fld  dword ptr [esp + 0xc]  <- the roll, the first argument
  //   0x00570758  fmul dword ptr [0x00E4F724] <- * 0.5f
  //   0x00570765  call 0x005734E0             <- the retail's sinf
  //   0x00570775  call 0x005734D0             <- the retail's cosf
  //   0x0057077A  fstp dword ptr [esi]        <- the scalar lane
  //   0x0057077C  mov  eax, esi               <- the return value: the output
  //   0x0057077E  fld  dword ptr [edi]        <- the axis
  //   0x00570788  fstp dword ptr [esi + 4]    <- axis.x * sin
  //   0x00570790  fstp dword ptr [esi + 8]    <- axis.y * sin
  //   0x00570796  fstp dword ptr [esi + 0xc]  <- axis.z * sin
  //   0x0057079C  ret 4                       <- the argument is popped
  //
  // So: the axis in EDI, the output quaternion in ESI, the roll as the first
  // stack argument (the retail pops it itself), and the return value is the
  // output pointer. The quaternion is written as [w, x, y, z].
  //
  // The engine's `msvc8::sinf` / `msvc8::cosf` are the retail's own
  // `fld; fsin` / `fld; fcos`, and the scale is applied on the x87 stack
  // with one float store per lane, so the lane products are the retail's.

  struct EulerIn
  {
    Wm3::Vector3f axis;
    float roll;
  };

  struct EulerOut
  {
    Wm3::Quatf quat;
    std::uint32_t returnedDest;
  };

  void GenEuler(Rng& rng, void* in, const DrawKind kind)
  {
    EulerIn& typed = *static_cast<EulerIn*>(in);
    float lanes[3];
    if (kind == DrawKind::Ordinary) {
      GenUnitVec3(rng, lanes);
    } else {
      const std::uint32_t mode = rng.Below(100);
      if (mode < 30) {
        // the corners of the roll: the half-turn, the quarter turn and the
        // multiples of pi
        lanes[0] = 0.0f;
        lanes[1] = 0.0f;
        lanes[2] = 1.0f;
      } else if (mode < 55) {
        for (int k = 0; k < 3; ++k) {
          lanes[k] = GenFloat(rng);
        }
      } else {
        GenVec3(rng, lanes);
      }
    }
    typed.axis = Wm3::Vector3f(lanes[0], lanes[1], lanes[2]);

    if (kind == DrawKind::Ordinary) {
      typed.roll = static_cast<float>(rng.Range(-6.0, 6.0));
    } else {
      switch (rng.Below(14)) {
      case 0:
        typed.roll = 0.0f;
        break;
      case 1:
        typed.roll = -0.0f;
        break;
      case 2:
        typed.roll = FromBits(0x40490FDBu); // pi
        break;
      case 3:
        typed.roll = FromBits(0x40490FDBu + 1u); // pi + 1 ulp
        break;
      case 4:
        typed.roll = FromBits(0x40490FDBu - 1u); // pi - 1 ulp
        break;
      case 5:
        typed.roll = FromBits(0x3FC90FDBu); // pi/2
        break;
      case 6:
        typed.roll = FromBits(0xBFC90FDBu); // -pi/2
        break;
      case 7:
        typed.roll = FromBits(0x40C90FDBu); // 2pi
        break;
      case 8:
        typed.roll = FromBits(0x7FC00000u); // NaN
        break;
      case 9:
        typed.roll = FromBits(0x7F800000u); // +inf
        break;
      case 10:
        typed.roll = FromBits(0xFF800000u); // -inf
        break;
      case 11:
        typed.roll = FromBits(0x00000001u); // the smallest denormal
        break;
      case 12:
        typed.roll = FromBits(0x7F7FFFFFu); // FLT_MAX
        break;
      default:
        typed.roll = static_cast<float>(rng.Range(-20.0, 20.0));
        break;
      }
    }
  }

  __declspec(naked) void RetailEulerRollToQuat(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebp, [esp + 20]      // in
      mov  ebx, [esp + 24]      // out
      lea  edi, [ebp]           // the retail's axis
      lea  esi, [ebx]           // the retail's output quaternion
      push dword ptr [ebp + 12] // the roll, the first stack argument
      mov  eax, 0x00570750
      call eax
      // the retail pops its own argument (`ret 4`), so the stack is already
      // back where it was
      cmp  eax, ebx             // the retail returns the output pointer
      sete al
      movzx eax, al
      mov  [ebx + 16], eax      // 1 when it does
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursEulerRollToQuat(const void* in, void* out)
  {
    const EulerIn& typedIn = *static_cast<const EulerIn*>(in);
    EulerOut& typedOut = *static_cast<EulerOut*>(out);
    Wm3::Quatf* const result =
      moho::EulerRollToQuat(&typedIn.axis, &typedOut.quat, typedIn.roll);
    typedOut.returnedDest = (result == &typedOut.quat) ? 1u : 0u;
  }

  // --- IsValidVector3f - retail 0x005657F0 -------------------------------
  //
  //   0x005657F0  push ebp / mov ebp, esp / and esp, 0xffffffc0
  //   0x005657F6  fld  dword ptr [esi]        <- the lane, as a float
  //   0x005657FB  fstp qword ptr [esp]        <- widened to a double
  //   0x005657FE  call 0x00A83F03             <- the CRT's _isnan
  //   0x00565806  test eax, eax / jne         <- a NaN lane rejects
  //   ...        (the same for the y and the z lane)
  //   0x00565834  mov  eax, 1                 <- the answer
  //   0x0056583D  xor  eax, eax
  //
  // So: the vector in ESI, and the answer (0 or 1) in EAX. A NaN in any lane
  // rejects; an infinity does not.
  //
  // This is a **weak witness**: a boolean only says "the lane is not a NaN",
  // so it cannot witness the arithmetic behind it. The generator therefore
  // puts the lanes on the NaN boundary itself (the largest finite float, the
  // infinities, the smallest NaN, the quiet NaNs with payloads, the signed
  // zeros and the denormals), because that is the only place the answer can
  // change.

  struct Valid3In
  {
    Wm3::Vector3f value;
  };

  struct Valid3Out
  {
    std::uint32_t valid;
  };

  // A lane drawn from the neighbourhood of the NaN boundary: the infinities,
  // the quiet NaNs, the signed zeros, the denormals and the largest finite
  // float.
  [[nodiscard]] float GenNaNBoundaryLane(Rng& rng) noexcept
  {
    switch (rng.Below(14)) {
    case 0:
      return FromBits(0x7F7FFFFFu); // FLT_MAX
    case 1:
      return FromBits(0xFF7FFFFFu); // -FLT_MAX
    case 2:
      return FromBits(0x7F800000u); // +inf
    case 3:
      return FromBits(0xFF800000u); // -inf
    case 4:
      return FromBits(0x7FC00000u); // the quiet NaN
    case 5:
      return FromBits(0xFFC00000u); // the quiet NaN, negative
    case 6:
      return FromBits(0x7FC12345u); // a quiet NaN with a payload
    case 7:
      return FromBits(0x7F800001u); // the smallest NaN
    case 8:
      return FromBits(0x00000000u); // +0
    case 9:
      return FromBits(0x80000000u); // -0
    case 10:
      return FromBits(0x00000001u); // the smallest denormal
    case 11:
      return FromBits(0x007FFFFFu); // the largest denormal
    case 12:
      return FromBits(0x00800000u); // FLT_MIN
    default:
      return GenPlain(rng, 100.0f);
    }
  }

  void GenValid3(Rng& rng, void* in, const DrawKind kind)
  {
    Valid3In& typed = *static_cast<Valid3In*>(in);
    float lanes[3];
    if (kind == DrawKind::Ordinary) {
      for (int k = 0; k < 3; ++k) {
        lanes[k] = GenOrdinaryLane(rng);
      }
    } else {
      const std::uint32_t mode = rng.Below(100);
      if (mode < 45) {
        for (int k = 0; k < 3; ++k) {
          lanes[k] = GenNaNBoundaryLane(rng);
        }
      } else if (mode < 70) {
        for (int k = 0; k < 3; ++k) {
          lanes[k] = GenFloat(rng);
        }
      } else {
        for (int k = 0; k < 3; ++k) {
          lanes[k] = GenPlain(rng, 100.0f);
        }
      }
    }
    typed.value = Wm3::Vector3f(lanes[0], lanes[1], lanes[2]);
  }

  __declspec(naked) void RetailIsValidVector3f(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebx, [esp + 24]      // out
      mov  esi, [esp + 20]      // the retail's vector
      mov  eax, 0x005657F0
      call eax
      mov  [ebx], eax           // 0 or 1
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursIsValidVector3f(const void* in, void* out)
  {
    const Valid3In& typedIn = *static_cast<const Valid3In*>(in);
    Valid3Out& typedOut = *static_cast<Valid3Out*>(out);
    typedOut.valid = moho::IsValidVector3f(typedIn.value) ? 1u : 0u;
  }

  // --- IsUnitLengthVector3f - retail 0x0069A1D0 --------------------------
  //
  //   0x0069A1D0  movss xmm2, [eax]          <- x
  //   0x0069A1E1  mulss xmm3, xmm2           <- x*x
  //   0x0069A1E8  mulss xmm2, xmm1           <- y*y
  //   0x0069A1EC  addss xmm3, xmm2           <- x*x + y*y
  //   0x0069A1F3  mulss xmm1, xmm0           <- z*z
  //   0x0069A1F7  addss xmm3, xmm1           <- + z*z
  //   0x0069A1FB  subss xmm3, [0x00DFEC20]   <- - 1.0f
  //   0x0069A209  call 0x0040DAB0            <- the retail's fabs
  //   0x0069A20E  fld  dword ptr [0x00DFF0AC] <- 0.001f
  //   0x0069A217  fcompi st(1)
  //   0x0069A21B  jb  0x0069A223             <- greater: reject
  //   0x0069A21D  mov  eax, 1                <- the answer
  //
  // So: the vector in EAX, and the answer (0 or 1) in EAX. The squared
  // length is summed in storage order, and the unordered case (a NaN lane)
  // rejects.
  //
  // This is a **weak witness** as well, but the epsilon is the interesting
  // part: the answer flips when the squared length sits exactly on 0.001f
  // from one. The generator therefore constructs the squared lengths that sit
  // on that boundary - 1.001f and its two neighbours, 1.0f and its two
  // neighbours, and 0.999f - instead of leaving them to chance.

  struct UnitLenIn
  {
    Wm3::Vector3f value;
  };

  struct UnitLenOut
  {
    std::uint32_t unit;
  };

  // Builds a vector whose squared length is exactly `target`, as the engine
  // computes it: (x*x + y*y) + z*z, in float.
  //
  // Two shapes are tried: (t, 0, 0), whose squared length is t*t, and
  // (1, t, 0), whose squared length is 1 + t*t - the second one has a much
  // finer grid near one and is what reaches the 1.001f boundary.
  [[nodiscard]] bool MakeSquaredLength(const float target, float* lanes) noexcept
  {
    {
      const double approx = std::sqrt(static_cast<double>(target));
      if (approx > 0.0 && std::isfinite(approx)) {
        const std::uint32_t base = Bits(static_cast<float>(approx));
        for (std::uint32_t k = 0; k < 8192u; ++k) {
          for (int side = 0; side < 2; ++side) {
            const std::uint32_t bits = (side == 0) ? (base + k) : (base - k);
            if ((side == 0 && base + k < base) || (side == 1 && base < k)) {
              continue;
            }
            const float t = FromBits(bits);
            volatile float squared = t * t;
            if (squared == target) {
              lanes[0] = t;
              lanes[1] = 0.0f;
              lanes[2] = 0.0f;
              return true;
            }
          }
        }
      }
    }
    {
      const double approx = std::sqrt(static_cast<double>(target) - 1.0);
      if (approx > 0.0 && std::isfinite(approx)) {
        const std::uint32_t base = Bits(static_cast<float>(approx));
        for (std::uint32_t k = 0; k < 8192u; ++k) {
          for (int side = 0; side < 2; ++side) {
            const std::uint32_t bits = (side == 0) ? (base + k) : (base - k);
            if ((side == 0 && base + k < base) || (side == 1 && base < k)) {
              continue;
            }
            const float t = FromBits(bits);
            volatile float squared = 1.0f + t * t;
            if (squared == target) {
              lanes[0] = 1.0f;
              lanes[1] = t;
              lanes[2] = 0.0f;
              return true;
            }
          }
        }
      }
    }
    return false;
  }

  // The squared lengths that sit on the epsilon: 0.001f from one, and one
  // ulp either side of each of them.
  [[nodiscard]] const std::vector<float>& UnitLengthTargets()
  {
    static const std::vector<float> targets = [] {
      std::vector<float> list;
      const float one = 1.0f;
      const float eps = 0.001f;
      const float above = FromBits(Bits(one + eps) + 1u); // one ulp above 1.001f
      const float below = FromBits(Bits(one + eps) - 1u); // one ulp below 1.001f
      const float unitAbove = FromBits(Bits(one) + 1u);
      const float unitBelow = FromBits(Bits(one) - 1u);
      const float under = FromBits(Bits(one - eps) + 1u); // 0.999f + 1 ulp
      const float underOne = one - eps;                  // 0.999f
      for (const float value : {one, unitAbove, unitBelow, one + eps, above, below, underOne, under}) {
        list.push_back(value);
      }
      return list;
    }();
    return targets;
  }

  void GenUnitLen(Rng& rng, void* in, const DrawKind kind)
  {
    UnitLenIn& typed = *static_cast<UnitLenIn*>(in);
    float lanes[3];
    if (kind == DrawKind::Ordinary) {
      // a vector whose length is near one, and one whose length is not
      if (rng.Chance(50)) {
        GenUnitVec3(rng, lanes);
      } else {
        GenVec3(rng, lanes);
      }
    } else {
      const std::uint32_t mode = rng.Below(100);
      if (mode < 40) {
        // the squared lengths that sit on the epsilon
        const std::vector<float>& targets = UnitLengthTargets();
        const float target = targets[rng.Below(static_cast<std::uint32_t>(targets.size()))];
        if (!MakeSquaredLength(target, lanes)) {
          GenUnitVec3(rng, lanes);
        }
      } else if (mode < 60) {
        // NaN, infinity and denormal lanes: the squared length is unordered
        for (int k = 0; k < 3; ++k) {
          lanes[k] = GenFloat(rng);
        }
      } else if (mode < 80) {
        GenUnitVec3(rng, lanes);
      } else {
        GenVec3(rng, lanes);
      }
    }
    typed.value = Wm3::Vector3f(lanes[0], lanes[1], lanes[2]);
  }

  __declspec(naked) void RetailIsUnitLengthVector3f(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebx, [esp + 24]      // out
      mov  eax, [esp + 20]      // the retail's vector
      mov  ecx, 0x0069A1D0
      call ecx
      mov  [ebx], eax           // 0 or 1
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursIsUnitLengthVector3f(const void* in, void* out)
  {
    const UnitLenIn& typedIn = *static_cast<const UnitLenIn*>(in);
    UnitLenOut& typedOut = *static_cast<UnitLenOut*>(out);
    typedOut.unit = moho::IsUnitLengthVector3f(typedIn.value) ? 1u : 0u;
  }

  // --- IsValidVector2f - retail 0x005657B0 -------------------------------
  //
  //   0x005657B0  fld  dword ptr [esi]        <- the x lane
  //   0x005657B5  fstp qword ptr [esp]        <- widened to a double
  //   0x005657B8  call 0x00A83F03             <- the CRT's _isnan
  //   0x005657C0  test eax, eax / jne         <- a NaN lane rejects
  //   0x005657C4  fld  dword ptr [esi + 4]    <- the y lane, the same again
  //   0x005657D9  mov  eax, 1                 <- the answer
  //   0x005657DF  xor  eax, eax
  //
  // So: the vector in ESI, and the answer (0 or 1) in EAX. As above, this is
  // a weak witness; the generator puts the lanes on the NaN boundary.

  struct Valid2In
  {
    Wm3::Vector2f value;
  };

  struct Valid2Out
  {
    std::uint32_t valid;
  };

  void GenValid2(Rng& rng, void* in, const DrawKind kind)
  {
    Valid2In& typed = *static_cast<Valid2In*>(in);
    float lanes[2];
    if (kind == DrawKind::Ordinary) {
      for (int k = 0; k < 2; ++k) {
        lanes[k] = GenOrdinaryLane(rng);
      }
    } else {
      const std::uint32_t mode = rng.Below(100);
      if (mode < 45) {
        for (int k = 0; k < 2; ++k) {
          lanes[k] = GenNaNBoundaryLane(rng);
        }
      } else if (mode < 70) {
        for (int k = 0; k < 2; ++k) {
          lanes[k] = GenFloat(rng);
        }
      } else {
        for (int k = 0; k < 2; ++k) {
          lanes[k] = GenPlain(rng, 100.0f);
        }
      }
    }
    typed.value = Wm3::Vector2f(lanes[0], lanes[1]);
  }

  __declspec(naked) void RetailIsValidVector2f(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebx, [esp + 24]      // out
      mov  esi, [esp + 20]      // the retail's vector
      mov  eax, 0x005657B0
      call eax
      mov  [ebx], eax           // 0 or 1
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursIsValidVector2f(const void* in, void* out)
  {
    const Valid2In& typedIn = *static_cast<const Valid2In*>(in);
    Valid2Out& typedOut = *static_cast<Valid2Out*>(out);
    typedOut.valid = moho::IsValidVector2f(typedIn.value) ? 1u : 0u;
  }

  // ---------------------------------------------------------------------
  // The second batch of comparison rows.
  //
  // Every retail convention below was measured from the image with
  // `.tmp/disasm.py <va>`; none of them is read off a header.

  // --- MatrixRowsToQuatCanonical - retail 0x004F0CB0 ---------------------
  //
  //   0x004F0CB0  sub  esp, 0x10 / push ebx / push ebp / push esi
  //   0x004F0CB6  mov  esi, [esp + 0x20]   <- the matrix (the first argument)
  //   0x004F0CBA  movss xmm2, [esi]        <- m00
  //   0x004F0CBE  movss xmm1, [esi + 0x10] <- m11
  //   0x004F0CC3  movss xmm3, [esi + 0x20] <- m22
  //   0x004F0CC8  movaps xmm0, xmm2
  //   0x004F0CCC  addss xmm0, xmm1 / addss xmm0, xmm3   <- the trace
  //   0x004F0CD6  comiss xmm0, [0x00E4F6E0] <- against 0.0f
  //   0x004F0CE1  jbe  0x004F0D58           <- the dominant-axis branch
  //   0x004F0CD0  mov  edi, ecx             <- the destination
  //   0x004F0D4C  mov  eax, edi             <- the return value: the destination
  //   0x004F0D55  ret  4                    <- the argument is popped
  //
  // The matrix is a PACKED 3x3 (three rows of three floats, 12 bytes each):
  // the diagonal is at +0, +0x10 and +0x20, i.e. 12*row + 4*col.
  //
  // The trace branch: the square root is the retail's own (0x00452FC0, an x87
  // `fsqrt`), the float copy of it is rounded by `fst dword ptr [esp+0x28]`,
  // the scalar lane is `sqrt * 0.5f` on the x87 stack, and the inverse root
  // is `0.5f / sqrt` in SSE float.
  //
  // The dominant-axis branch reads the shuffle table at 0x00F3D2B4 ({1, 2, 0})
  // and forms the root as `(m_d - m_S(d)) - m_S2(d) + 1.0f` - the two
  // subtractions are separate (0x004F0D91 and 0x004F0D9B), which is what the
  // engine's source does too.

  // The matrix draw for the canonical matrix-to-quaternion rows: rotation
  // matrices, plain (non-orthonormal) matrices, singular ones (a zero row, a
  // repeated row - the dominant-axis branch and its square root are the
  // interesting part) and the full float mixture.
  void GenMatQuatCanonical(Rng& rng, void* in, const DrawKind kind)
  {
    MatQuatIn& typed = *static_cast<MatQuatIn*>(in);
    if (kind == DrawKind::Ordinary) {
      // Ordinary: a well-formed rotation matrix - every lane finite and
      // normal.
      GenRotationRows(rng, typed.rows);
      return;
    }
    const std::uint32_t mode = rng.Below(100);
    if (mode < 40) {
      GenRotationRows(rng, typed.rows);
    } else if (mode < 60) {
      // A plain matrix: the trace is an arbitrary sum of three lanes.
      for (int row = 0; row < 3; ++row) {
        float lanes[3];
        for (int col = 0; col < 3; ++col) {
          lanes[col] = GenPlain(rng, 1.0f);
        }
        typed.rows[row] = Wm3::Vector3f(lanes[0], lanes[1], lanes[2]);
      }
    } else if (mode < 80) {
      // Singular matrices: a zero row, a repeated row, a rank-one matrix.
      // The dominant-axis branch and its square root are what is at stake.
      const std::uint32_t which = rng.Below(4);
      for (int row = 0; row < 3; ++row) {
        float lanes[3];
        for (int col = 0; col < 3; ++col) {
          lanes[col] = GenPlain(rng, 1.0f);
        }
        if (which == 0) {
          if (row == 1) {
            lanes[0] = 0.0f;
            lanes[1] = 0.0f;
            lanes[2] = 0.0f;
          }
        } else if (which == 1) {
          if (row == 2) {
            lanes[0] = lanes[0];
            lanes[1] = lanes[1];
            lanes[2] = lanes[2];
          }
        } else if (which == 2) {
          // a rank-one matrix: every row is a multiple of the first one
          if (row != 0) {
            lanes[0] *= static_cast<float>(row);
            lanes[1] *= static_cast<float>(row);
            lanes[2] *= static_cast<float>(row);
          }
        } else {
          // the identity with one lane changed: the trace branch and the
          // dominant-axis branch disagree right at the boundary
          if (row == 0) {
            lanes[0] = 1.0f;
            lanes[1] = 0.0f;
            lanes[2] = 0.0f;
          } else if (row == 1) {
            lanes[0] = 0.0f;
            lanes[1] = 1.0f;
            lanes[2] = 0.0f;
          } else {
            lanes[0] = 0.0f;
            lanes[1] = 0.0f;
            lanes[2] = 1.0f;
          }
        }
        typed.rows[row] = Wm3::Vector3f(lanes[0], lanes[1], lanes[2]);
      }
    } else {
      for (int row = 0; row < 3; ++row) {
        float lanes[3];
        GenExtremeRow(rng, lanes);
        typed.rows[row] = Wm3::Vector3f(lanes[0], lanes[1], lanes[2]);
      }
    }
  }

  __declspec(naked) void RetailMatrixRowsToQuatCanonical(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebp, [esp + 20]      // in
      mov  ebx, [esp + 24]      // out
      push ebp                  // the matrix: the retail's first stack argument
      mov  esi, [esp]           // the retail's matrix
      mov  ecx, ebx             // the retail's destination
      mov  eax, 0x004F0CB0
      call eax
      // the retail pops its own argument (`ret 4`)
      cmp  eax, ebx             // the retail returns the destination pointer
      sete al
      movzx eax, al
      mov  [ebx + 16], eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursMatrixRowsToQuatCanonical(const void* in, void* out)
  {
    const MatQuatIn& typedIn = *static_cast<const MatQuatIn*>(in);
    MatQuatOut& typedOut = *static_cast<MatQuatOut*>(out);
    typedOut.returnedDest =
      (moho::MatrixRowsToQuatCanonical(&typedIn.rows[0], &typedOut.quat) == &typedOut.quat) ? 1u : 0u;
  }

  // --- MatrixColumnsToQuatCanonical - retail 0x004F0AE0 -------------------
  //
  //   0x004F0AE0  sub  esp, 0x28
  //   0x004F0AE3  mov  edx, [esp + 0x2c]   <- the columns (the first argument)
  //   0x004F0B10  fld  dword ptr [edx - 8]  <- the transposed lanes
  //   0x004F0B2F  lea  eax, [esp + 4]       <- the transposed rows
  //   0x004F0B33  push eax
  //   0x004F0B34  call 0x004F0CB0           <- the canonical row conversion
  //   0x004F0B39  add  esp, 0x28
  //   0x004F0B3C  ret  4
  //
  // So: the columns as the first stack argument (the retail pops it), the
  // destination in ECX, and the return value is the destination pointer (the
  // inner call's EAX, which the transpose wrapper does not touch).
  //
  // The transpose itself copies the nine lanes through the x87 stack, so it is
  // exact - the only interesting part is the canonical conversion.

  __declspec(naked) void RetailMatrixColumnsToQuatCanonical(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebp, [esp + 20]      // in
      mov  ebx, [esp + 24]      // out
      push ebp                  // the columns: the retail's first stack argument
      mov  ecx, ebx             // the retail's destination
      mov  eax, 0x004F0AE0
      call eax
      // the retail pops its own argument (`ret 4`)
      cmp  eax, ebx             // the retail returns the destination pointer
      sete al
      movzx eax, al
      mov  [ebx + 16], eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursMatrixColumnsToQuatCanonical(const void* in, void* out)
  {
    const MatQuatIn& typedIn = *static_cast<const MatQuatIn*>(in);
    MatQuatOut& typedOut = *static_cast<MatQuatOut*>(out);
    typedOut.returnedDest =
      (moho::MatrixColumnsToQuatCanonical(&typedIn.rows[0], &typedOut.quat) == &typedOut.quat) ? 1u : 0u;
  }

  // --- NormalizeAngleSignedRadians - retail 0x0062FB50 --------------------
  //
  //   0x0062FB50  push ebp / mov ebp, esp / and esp, 0xfffffff8
  //   0x0062FB56  sub  esp, 8
  //   0x0062FB59  fld  dword ptr [ebp + 8]   <- the angle
  //   0x0062FB5C  fld  qword ptr [0x00E4F780] <- 2pi as a double
  //   0x0062FB62  call 0x00A8E06A            <- the CRT's _CIfmod
  //   0x0062FB67  fst  dword ptr [esp + 4]   <- the float copy of the remainder
  //   0x0062FB6B  fld  dword ptr [0x00E4F77C] <- -pi
  //   0x0062FB71  movss xmm0, dword ptr [esp + 4]
  //   0x0062FB77  fcompi st(1) / fstp st(0) / jbe 0x0062FB89
  //   0x0062FB7D  addss xmm0, [0x00DFF08C]   <- + 2pi
  //   0x0062FB89  comiss xmm0, [0x00DFF088]  <- against +pi
  //   0x0062FB92  subss xmm0, [0x00DFF08C]   <- - 2pi
  //
  // So: the angle is the first stack argument (the retail does not pop it),
  // and the answer comes back in XMM0 as a float.
  //
  // The two constants are the *float* 2pi (0x00DFF08C) and the *double*
  // widening of that same float (0x00E4F780) - the divisor of the fmod is
  // therefore 6.2831854820251465, not the mathematical 2pi.
  //
  // The first test compares the UNROUNDED remainder against -pi, the second
  // one the ROUNDED copy against +pi; the engine's source keeps that
  // distinction.

  struct AngleIn
  {
    float angle;
  };

  struct AngleOut
  {
    float angle;
  };

  // The angles that sit on the wrap boundaries: the multiples of pi, the
  // half-turns, and the values just either side of them.
  float GenSignedAngle(Rng& rng)
  {
    switch (rng.Below(16)) {
    case 0:
      return 0.0f;
    case 1:
      return -0.0f;
    case 2:
      return FromBits(0x40490FDBu); // pi
    case 3:
      return FromBits(0xC0490FDBu); // -pi
    case 4:
      return FromBits(0x40490FDBu + 1u); // pi + 1 ulp
    case 5:
      return FromBits(0x40490FDBu - 1u); // pi - 1 ulp
    case 6:
      return FromBits(0x3FC90FDBu); // pi/2
    case 7:
      return FromBits(0xBFC90FDBu); // -pi/2
    case 8:
      return FromBits(0x40C90FDBu); // 2pi
    case 9:
      return FromBits(0xC0C90FDBu); // -2pi
    case 10:
      return FromBits(0x7FC00000u); // NaN
    case 11:
      return FromBits(0x7F800000u); // +inf
    case 12:
      return FromBits(0xFF800000u); // -inf
    case 13:
      return FromBits(0x00000001u); // the smallest denormal
    case 14:
      return FromBits(0x7F7FFFFFu); // FLT_MAX
    default:
      return static_cast<float>(rng.Range(-12.0, 12.0));
    }
  }

  void GenAngleIn(Rng& rng, void* in, const DrawKind kind)
  {
    AngleIn& typed = *static_cast<AngleIn*>(in);
    typed.angle = (kind == DrawKind::Ordinary) ? static_cast<float>(rng.Range(-10.0, 10.0))
                                               : GenSignedAngle(rng);
  }

  __declspec(naked) void RetailNormalizeAngleSignedRadians(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebp, [esp + 20]      // in
      mov  ebx, [esp + 24]      // out
      push dword ptr [ebp]      // the angle: the retail's first stack argument
      mov  eax, 0x0062FB50
      call eax
      add  esp, 4                // the retail does not pop its argument
      movss [ebx], xmm0          // the answer comes back in XMM0
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursNormalizeAngleSignedRadians(const void* in, void* out)
  {
    const AngleIn& typedIn = *static_cast<const AngleIn*>(in);
    AngleOut& typedOut = *static_cast<AngleOut*>(out);
    typedOut.angle = moho::NormalizeAngleSignedRadians(typedIn.angle);
  }

  // --- QuatFromAxisAngleVector - retail 0x00697360 ------------------------
  //
  //   0x00697360  sub  esp, 8
  //   0x00697363  lea  ecx, [esp + 0xc]     <- the axis-angle vector, BY VALUE
  //   0x00697367  call 0x00452AF0           <- Wm3::Vector3f::Normalize
  //   0x0069736C  fmul dword ptr [0x00E4F724] <- * 0.5f (the half angle)
  //   0x00697372  push ecx / fst dword [esp+4] / fstp dword [esp]
  //   0x0069737A  call 0x005734E0           <- the retail's sinf
  //   0x00697387  fstp dword [esp] / call 0x005734D0  <- the retail's cosf
  //   0x0069738F  fstp dword [esi]          <- the scalar lane
  //   0x00697391  mov  eax, esi             <- the return value: the quaternion
  //   0x00697393  fld dword [esp+8] / fld dword [esp+0x10] / fmul st(1)
  //   0x0069739D  fstp dword [esi + 4]      <- axis.x * sin
  //   0x006973A9  fmul dword [esp+0x18] / fstp dword [esi + 0xc]
  //
  // So: the quaternion in ESI, the axis-angle vector BY VALUE on the stack
  // (twelve bytes, the first argument), and the return value is the
  // quaternion pointer. The retail normalises its own copy of the vector on
  // the stack, and the three lane products stay on the x87 stack.

  struct AxisAngleVecIn
  {
    Wm3::Vector3f axisAngle;
  };

  struct AxisAngleVecOut
  {
    Wm3::Quaternionf quat;
    std::uint32_t returnedDest;
  };

  void GenAxisAngleVector(Rng& rng, void* in, const DrawKind kind)
  {
    AxisAngleVecIn& typed = *static_cast<AxisAngleVecIn*>(in);
    float lanes[3];
    if (kind == DrawKind::Ordinary) {
      // A unit direction with an angle in [0, 6].
      GenUnitVec3(rng, lanes);
      const double magnitude = rng.Range(0.0, 6.0);
      for (int k = 0; k < 3; ++k) {
        lanes[k] = static_cast<float>(lanes[k] * magnitude);
      }
    } else {
      const std::uint32_t mode = rng.Below(100);
      if (mode < 25) {
        // The zero vector: the normalise branch that cannot normalise, and
        // the half angle that comes out of it.
        lanes[0] = 0.0f;
        lanes[1] = 0.0f;
        lanes[2] = 0.0f;
        if (rng.Chance(40)) {
          lanes[rng.Below(3)] = -0.0f;
        }
      } else if (mode < 45) {
        // The full float mixture: NaN, infinity, denormal lanes.
        for (int k = 0; k < 3; ++k) {
          lanes[k] = GenFloat(rng);
        }
      } else if (mode < 60) {
        // Denormal lanes: the squared length underflows, so the length is
        // zero or denormal.
        for (int k = 0; k < 3; ++k) {
          lanes[k] = FromBits(0x00000001u + rng.Below(0x007FFFFFu));
        }
      } else {
        GenVec3(rng, lanes);
      }
    }
    typed.axisAngle = Wm3::Vector3f(lanes[0], lanes[1], lanes[2]);
  }

  __declspec(naked) void RetailQuatFromAxisAngleVector(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebp, [esp + 20]      // in
      mov  ebx, [esp + 24]      // out
      // the axis-angle vector goes on the stack BY VALUE: three floats, the
      // last lane first.
      push dword ptr [ebp + 8]  // z
      push dword ptr [ebp + 4]  // y
      push dword ptr [ebp]      // x
      mov  esi, ebx             // the retail's quaternion
      mov  eax, 0x00697360
      call eax
      add  esp, 12              // the retail does not pop its arguments
      cmp  eax, ebx             // the retail returns the quaternion pointer
      sete al
      movzx eax, al
      mov  [ebx + 16], eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursQuatFromAxisAngleVector(const void* in, void* out)
  {
    const AxisAngleVecIn& typedIn = *static_cast<const AxisAngleVecIn*>(in);
    AxisAngleVecOut& typedOut = *static_cast<AxisAngleVecOut*>(out);
    typedOut.returnedDest =
      (moho::QuatFromAxisAngleVector(&typedOut.quat, typedIn.axisAngle) == &typedOut.quat) ? 1u : 0u;
  }

  // --- BuildRotationMatrixFromEulerHPR - retail 0x006D1E30 ----------------
  //
  //   0x006D1E30  sub  esp, 0x98
  //   0x006D1E36  fld  dword ptr [esp + 0x9c]  <- heading (the first argument)
  //   0x006D1E41  call 0x005734D0            <- the retail's cosf
  //   0x006D1E54  call 0x005734E0            <- the retail's sinf
  //   ...        (the three matrices are laid out on the stack, packed)
  //   0x006D1F81  call 0x006D2680            <- Multiply3x3RowMatrices
  //   0x006D1F90  call 0x006D2680            <- and once more
  //   0x006D1F9B  call 0x00A48F30            <- memcpy(out, result, 36)
  //   0x006D1FA0  mov  eax, esi
  //   0x006D1FA8  ret  0xc                   <- the arguments are popped
  //
  // So: the output matrix in ESI, the three angles as the stack arguments
  // (the retail pops them itself), and the return value is the output
  // pointer.
  //
  // The two matrix products are `pitch * roll` and then `heading * that` -
  // the argument order of 0x006D2680 puts the matrix pushed LAST in the "a"
  // slot, so the first product is pitch*roll and the second heading*(pitch*roll).
  //
  // The retail's multiply (0x006D2680) sums `product + accumulator` starting
  // from a +0.0f accumulator, exactly like the engine's own multiply.

  struct EulerHprIn
  {
    float heading;
    float pitch;
    float roll;
  };

  struct EulerHprOut
  {
    Wm3::Vector3f rows[3];
    std::uint32_t returnedDest;
  };

  void GenEulerHpr(Rng& rng, void* in, const DrawKind kind)
  {
    EulerHprIn& typed = *static_cast<EulerHprIn*>(in);
    if (kind == DrawKind::Ordinary) {
      typed.heading = static_cast<float>(rng.Range(-3.2, 3.2));
      typed.pitch = static_cast<float>(rng.Range(-3.2, 3.2));
      typed.roll = static_cast<float>(rng.Range(-3.2, 3.2));
      return;
    }
    const float angles[3] = {GenSignedAngle(rng), GenSignedAngle(rng), GenSignedAngle(rng)};
    typed.heading = angles[0];
    typed.pitch = angles[1];
    typed.roll = angles[2];
  }

  __declspec(naked) void RetailBuildRotationMatrixFromEulerHPR(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebp, [esp + 20]      // in
      mov  ebx, [esp + 24]      // out
      push dword ptr [ebp + 8]  // roll: the third stack argument
      push dword ptr [ebp + 4]  // pitch: the second stack argument
      push dword ptr [ebp]      // heading: the first stack argument
      mov  esi, ebx             // the retail's output matrix
      mov  eax, 0x006D1E30
      call eax
      // the retail pops its own arguments (`ret 0xc`)
      cmp  eax, ebx             // the retail returns the output pointer
      sete al
      movzx eax, al
      mov  [ebx + 36], eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursBuildRotationMatrixFromEulerHPR(const void* in, void* out)
  {
    const EulerHprIn& typedIn = *static_cast<const EulerHprIn*>(in);
    EulerHprOut& typedOut = *static_cast<EulerHprOut*>(out);
    typedOut.returnedDest = (moho::BuildRotationMatrixFromEulerHPR(
                               &typedOut.rows[0], typedIn.heading, typedIn.pitch, typedIn.roll)
                             == &typedOut.rows[0])
      ? 1u
      : 0u;
  }

  // --- BuildTiltShortestArcDelta - retail 0x0050CB50 ----------------------
  //
  //   0x0050CB50  sub  esp, 0x1c
  //   0x0050CB53  movss xmm0, [esi]        <- currentUp.x
  //   0x0050CB57  movss xmm1, [eax + 4]    <- targetNormal.y
  //   0x0050CB62  addss xmm0, [eax]        <- currentUp.x + targetNormal.x
  //   0x0050CB8F  lea  ecx, [esp + 0x10]   <- the half vector
  //   0x0050CB9F  call 0x00452AF0          <- Wm3::Vector3f::Normalize
  //   0x0050CBC4  mulss xmm1, xmm5         <- currentUp.x * half.x
  //   0x0050CBA6  movss xmm4, [esp + 0x18] <- half.z
  //   0x0050CBDE  ucomiss xmm0, [0x00E4F6E0]  <- the scalar against 0.0f
  //   0x0050CBE5  lahf / test ah, 0x44 / jnp 0x0050CC3F
  //   0x0050CC3F  fld dword [esi] / call 0x0050D140   <- the retail's fabs
  //   0x0050CC87  call 0x0050D150                      <- the retail's inv_sqrt
  //
  // So: ESI = the current up vector, EAX = the target normal, EDI = the
  // output delta quaternion, and the return value is the output pointer.
  //
  // The scalar is `(currentUp.z * half.z + currentUp.x * half.x) +
  // currentUp.y * half.y`, and the degenerate branch is taken when the
  // `ucomiss` sets ZF - that is, when the scalar is zero **or unordered**
  // (the `lahf`/`test ah, 0x44` sequence is the "jump if equal or unordered"
  // idiom MSVC emits for `== 0.0f` on a NaN-producing expression).

  struct TiltIn
  {
    Wm3::Vector3f targetNormal;
    Wm3::Vector3f currentUp;
  };

  struct TiltOut
  {
    Wm3::Quaternionf quat;
    std::uint32_t returnedDest;
  };

  void GenTilt(Rng& rng, void* in, const DrawKind kind)
  {
    TiltIn& typed = *static_cast<TiltIn*>(in);
    float target[3];
    float current[3];
    if (kind == DrawKind::Ordinary) {
      GenUnitVec3(rng, target);
      GenUnitVec3(rng, current);
    } else {
      const std::uint32_t mode = rng.Below(100);
      if (mode < 30) {
        // exactly opposed: the half vector is degenerate and the scalar is
        // zero - the branch that picks the perpendicular axis
        GenUnitVec3(rng, current);
        for (int k = 0; k < 3; ++k) {
          target[k] = -current[k];
        }
      } else if (mode < 45) {
        // the zero vectors
        for (int k = 0; k < 3; ++k) {
          target[k] = 0.0f;
          current[k] = 0.0f;
        }
      } else if (mode < 65) {
        // NaN and infinity lanes: the scalar is unordered
        for (int k = 0; k < 3; ++k) {
          target[k] = GenFloat(rng);
          current[k] = GenFloat(rng);
        }
      } else {
        GenVec3(rng, target);
        GenVec3(rng, current);
      }
    }
    typed.targetNormal = Wm3::Vector3f(target[0], target[1], target[2]);
    typed.currentUp = Wm3::Vector3f(current[0], current[1], current[2]);
  }

  __declspec(naked) void RetailBuildTiltShortestArcDelta(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebp, [esp + 20]      // in
      mov  ebx, [esp + 24]      // out
      lea  esi, [ebp + 12]      // the retail's current up vector
      mov  edi, ebx             // the retail's output quaternion
      lea  eax, [ebp]           // the retail's target normal
      mov  ecx, 0x0050CB50
      call ecx
      cmp  eax, ebx             // the retail returns the output pointer
      sete al
      movzx eax, al
      mov  [ebx + 16], eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursBuildTiltShortestArcDelta(const void* in, void* out)
  {
    const TiltIn& typedIn = *static_cast<const TiltIn*>(in);
    TiltOut& typedOut = *static_cast<TiltOut*>(out);
    typedOut.returnedDest = (moho::BuildTiltShortestArcDelta(
                               typedIn.targetNormal, &typedOut.quat, typedIn.currentUp)
                             == &typedOut.quat)
      ? 1u
      : 0u;
  }

  // --- BlendOrientationDeltaByMaxAngle - retail 0x004EB830 -----------------
  //
  //   0x004EB830  sub  esp, 0x28
  //   0x004EB83B  fld  dword ptr [esp + 0x30]  <- the turn step (the second
  //                                               stack argument)
  //   0x004EB83F  movss xmm7, [ebx]           <- the goal (EBX)
  //   0x004EB851  mov  ebp, [esp + 0x30]      <- the current (the first stack
  //                                               argument)
  //   0x004EB85A  subss xmm1, [ebp + 4]        <- the conjugate of the current
  //   0x004EB946  lea  esi, [esp + 0x14]       <- the delta quaternion
  //   0x004EB93F  fstp dword ptr [esp]         <- the turn step, for the call
  //   0x004EB956  call 0x004EB740              <- RotateQuatByAngle
  //   0x004EB95E  test al, al / je 0x004EBA55  <- the rejected step
  //   0x004EBA38  mov  byte ptr [eax], 0       <- *outNoStep = false
  //   0x004EBA4D  mov  eax, edi                <- the return value: the output
  //   0x004EBA6C  mov  byte ptr [ecx], 1       <- *outNoStep = true
  //
  // So: EBX = the goal orientation, EDI = the output orientation, EBP = the
  // current orientation (the first stack argument), the turn step (the second
  // stack argument), the "no step" flag (the third stack argument), and the
  // return value is the output pointer. The retail does not pop its
  // arguments.
  //
  // The retail reuses the first stack argument's slot as scratch for the
  // goal's scalar lane (0x004EB85F), which is why the rejected branch can
  // still read the goal's scalar lane back (0x004EBA5C).

  struct BlendIn
  {
    Wm3::Quaternionf goal;
    Wm3::Quaternionf current;
    float turnStep;
  };

  struct BlendOut
  {
    Wm3::Quaternionf quat;
    // The retail writes the "no step" flag as a BYTE (0x004EBA38 `mov byte
    // ptr [eax], 0` and 0x004EBA6C `mov byte ptr [ecx], 1`), and so does the
    // engine (its `bool*` is a byte). The slot is therefore a byte, and the
    // three bytes above it stay at the canary on both sides.
    std::uint8_t noStep;
    std::uint32_t returnedDest;
  };

  void GenBlend(Rng& rng, void* in, const DrawKind kind)
  {
    BlendIn& typed = *static_cast<BlendIn*>(in);
    float goal[4];
    float current[4];
    if (kind == DrawKind::Ordinary) {
      GenOrdinaryQuat(rng, goal);
      GenOrdinaryQuat(rng, current);
      typed.turnStep = static_cast<float>(rng.Range(0.0, 3.0));
    } else {
      GenQuat(rng, goal);
      GenQuat(rng, current);
      typed.turnStep = GenAngle(rng);
    }
    typed.goal = Wm3::Quaternionf(goal[0], goal[1], goal[2], goal[3]);
    typed.current = Wm3::Quaternionf(current[0], current[1], current[2], current[3]);
  }

  __declspec(naked) void RetailBlendOrientationDeltaByMaxAngle(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  esi, [esp + 20]      // in
      mov  ebx, [esp + 24]      // out
      mov  edi, ebx             // the retail's output orientation
      lea  eax, [ebx + 16]      // the "no step" flag
      push eax                  // the third stack argument
      push dword ptr [esi + 32] // the turn step: the second stack argument
      lea  eax, [esi + 16]      // the address of the current orientation
      push eax                  // the first stack argument
      lea  ebx, [esi]           // the retail's goal orientation
      lea  ebp, [esi + 16]      // the retail's current orientation
      mov  ecx, 0x004EB830
      call ecx
      add  esp, 12              // the retail does not pop its arguments
      cmp  eax, edi             // the retail returns the output pointer
      sete al
      movzx eax, al
      mov  [edi + 20], eax      // EDI is the retail's output pointer
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursBlendOrientationDeltaByMaxAngle(const void* in, void* out)
  {
    const BlendIn& typedIn = *static_cast<const BlendIn*>(in);
    BlendOut& typedOut = *static_cast<BlendOut*>(out);
    typedOut.returnedDest = (moho::BlendOrientationDeltaByMaxAngle(
                               typedIn.goal, typedIn.current, typedIn.turnStep,
                               reinterpret_cast<bool*>(&typedOut.noStep), &typedOut.quat)
                             == &typedOut.quat)
      ? 1u
      : 0u;
  }

  // ---------------------------------------------------------------------
  // The x87 helper rows.
  //
  // These four are the engine's own re-implementations of the MSVC8 CRT's
  // float math, and each one has a retail body at the address the source
  // comment quotes. The retail bodies are the CRT's own: the argument comes
  // in on the stack (or, for the CRT's `_CIasin`, on the x87 stack) and the
  // answer comes back on the x87 stack, unspilled.

  // --- msvc8::atan2f - retail 0x004E9DF0 ---------------------------------
  //
  //   0x004E9DF0  push ebp / mov ebp, esp / and esp, 0xfffffff8
  //   0x004E9DF6  fld  dword ptr [ebp + 8]    <- y (the first argument)
  //   0x004E9DF9  fld  dword ptr [ebp + 0xc]  <- x (the second argument)
  //   0x004E9DFC  fpatan
  //   0x004E9DFE  mov  esp, ebp / pop ebp / ret
  //
  // So: y is the first stack argument, x the second, and the answer comes
  // back on the x87 stack, unspilled - the harness rounds it to a float
  // exactly once, which is what the engine's own `msvc8::atan2f` does with
  // its `fstp result`.

  struct Atan2In
  {
    float y;
    float x;
  };

  struct Atan2Out
  {
    float result;
  };

  void GenAtan2(Rng& rng, void* in, const DrawKind kind)
  {
    Atan2In& typed = *static_cast<Atan2In*>(in);
    if (kind == DrawKind::Ordinary) {
      typed.y = static_cast<float>(rng.Range(-100.0, 100.0));
      typed.x = static_cast<float>(rng.Range(-100.0, 100.0));
      return;
    }
    const std::uint32_t mode = rng.Below(100);
    if (mode < 35) {
      // the corners of atan2: the origin, the four axis corners and the
      // signed zeros
      static constexpr float kLane[][2] = {
        { 0.0f, 0.0f },  { 1.0f, 0.0f },  { -1.0f, 0.0f }, { 0.0f, 1.0f },
        { 0.0f, -1.0f }, { -0.0f, 0.0f }, { 0.0f, -0.0f }, { -0.0f, -0.0f },
      };
      const std::uint32_t which = rng.Below(8);
      typed.y = kLane[which][0];
      typed.x = kLane[which][1];
    } else if (mode < 65) {
      typed.y = GenFloat(rng);
      typed.x = GenFloat(rng);
    } else {
      typed.y = GenPlain(rng, 1000.0f);
      typed.x = GenPlain(rng, 1000.0f);
    }
  }

  __declspec(naked) void RetailAtan2f(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebp, [esp + 20]      // in
      mov  ebx, [esp + 24]      // out
      push dword ptr [ebp + 4]  // x: the second stack argument
      push dword ptr [ebp]      // y: the first stack argument
      mov  eax, 0x004E9DF0
      call eax
      add  esp, 8                // the retail does not pop its arguments
      fstp dword ptr [ebx]       // the answer comes back on the x87 stack
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursAtan2f(const void* in, void* out)
  {
    const Atan2In& typedIn = *static_cast<const Atan2In*>(in);
    Atan2Out& typedOut = *static_cast<Atan2Out*>(out);
    typedOut.result = msvc8::atan2f(typedIn.y, typedIn.x);
  }

  // --- msvc8::cosf / msvc8::sinf - retail 0x005734D0 / 0x005734E0 --------
  //
  //   0x005734D0  push ebp / mov ebp, esp / and esp, 0xfffffff8
  //   0x005734D6  fld  dword ptr [ebp + 8]   <- the argument
  //   0x005734D9  fcos
  //   0x005734DB  mov  esp, ebp / pop ebp / ret
  //
  //   0x005734E0  ... fld dword ptr [ebp + 8] / fsin ...
  //
  // So: the argument is the first stack argument, and the answer comes back
  // on the x87 stack, unspilled. The harness rounds it to a float exactly
  // once, which is what the engine's own `msvc8::cosf` / `msvc8::sinf` do
  // with their `fstp result`.

  struct TrigIn
  {
    float x;
  };

  struct TrigOut
  {
    float result;
  };

  // The arguments that stress the transcendental: the huge ones (the
  // argument reduction), the exact multiples of pi/2, and the specials.
  float GenTrigArgument(Rng& rng)
  {
    switch (rng.Below(16)) {
    case 0:
      return 0.0f;
    case 1:
      return -0.0f;
    case 2:
      return FromBits(0x40490FDBu); // pi
    case 3:
      return FromBits(0xC0490FDBu); // -pi
    case 4:
      return FromBits(0x3FC90FDBu); // pi/2
    case 5:
      return FromBits(0xBFC90FDBu); // -pi/2
    case 6:
      return FromBits(0x7F800000u); // +inf
    case 7:
      return FromBits(0xFF800000u); // -inf
    case 8:
      return FromBits(0x7FC00000u); // NaN
    case 9:
      return FromBits(0x00000001u); // the smallest denormal
    case 10:
      return FromBits(0x7F7FFFFFu); // FLT_MAX
    case 11:
      return FromBits(0xFF7FFFFFu); // -FLT_MAX
    case 12:
      return FromBits(0x5F800000u); // 2^64
    case 13:
      return FromBits(0x1F800000u); // 2^-64
    case 14:
      return static_cast<float>(rng.Range(-100.0, 100.0));
    default:
      return static_cast<float>(rng.Range(-1.0e6, 1.0e6));
    }
  }

  void GenTrig(Rng& rng, void* in, const DrawKind kind)
  {
    TrigIn& typed = *static_cast<TrigIn*>(in);
    typed.x = (kind == DrawKind::Ordinary) ? static_cast<float>(rng.Range(-20.0, 20.0))
                                           : GenTrigArgument(rng);
  }

  __declspec(naked) void RetailCosf(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebp, [esp + 20]      // in
      mov  ebx, [esp + 24]      // out
      push dword ptr [ebp]      // the argument: the retail's first argument
      mov  eax, 0x005734D0
      call eax
      add  esp, 4                // the retail does not pop its argument
      fstp dword ptr [ebx]       // the answer comes back on the x87 stack
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursCosf(const void* in, void* out)
  {
    const TrigIn& typedIn = *static_cast<const TrigIn*>(in);
    TrigOut& typedOut = *static_cast<TrigOut*>(out);
    typedOut.result = msvc8::cosf(typedIn.x);
  }

  __declspec(naked) void RetailSinf(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebp, [esp + 20]      // in
      mov  ebx, [esp + 24]      // out
      push dword ptr [ebp]      // the argument: the retail's first argument
      mov  eax, 0x005734E0
      call eax
      add  esp, 4                // the retail does not pop its argument
      fstp dword ptr [ebx]       // the answer comes back on the x87 stack
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursSinf(const void* in, void* out)
  {
    const TrigIn& typedIn = *static_cast<const TrigIn*>(in);
    TrigOut& typedOut = *static_cast<TrigOut*>(out);
    typedOut.result = msvc8::sinf(typedIn.x);
  }

  // --- msvc8::frexp - retail 0x00A8DA05 -----------------------------------
  //
  //   0x00A8DA05  push ebp / mov ebp, esp
  //   0x00A8DA0C  mov  edi, dword ptr [ebp + 0x10]   <- the exponent pointer
  //   0x00A8DA11  cmp  edi, ebx / jne 0x00A8DA34     <- a null pointer raises
  //   0x00A8DA45  fld  qword ptr [ebp + 8]           <- the value
  //   0x00A8DA4E  and  ax, 0x7ff0                    <- the exponent field
  //   0x00A8DA52  cmp  ax, 0x7ff0 / jne 0x00A8DABD  <- not inf/NaN
  //   0x00A8DA5A  or   dword ptr [edi], 0xffffffff   <- the special case
  //   0x00A8DAC3  call 0x00A99F05                    <- the general case
  //   0x00A8DACF  push dword ptr [ebp + 0x10] / call 0x00A9A813
  //   0x00A8DAD4  fld  qword ptr [ebp - 8]            <- the mantissa
  //   0x00A8DADE  ret
  //
  // So: the value is the first stack argument (a double), the exponent
  // pointer the second, and the mantissa comes back on the x87 stack. The
  // engine's `msvc8::frexp` reproduces the special cases (a non-finite value
  // sets the exponent to -1, a zero value to 0 and answers +0) and defers to
  // the host CRT otherwise.

  struct FrexpIn
  {
    double value;
  };

  struct FrexpOut
  {
    double mantissa;
    std::int32_t exponent;
  };

  void GenFrexp(Rng& rng, void* in, const DrawKind kind)
  {
    FrexpIn& typed = *static_cast<FrexpIn*>(in);
    if (kind == DrawKind::Ordinary) {
      typed.value = rng.Range(-1.0e6, 1.0e6);
      return;
    }
    switch (rng.Below(12)) {
    case 0:
      typed.value = 0.0;
      break;
    case 1:
      typed.value = -0.0;
      break;
    case 2:
      typed.value = std::ldexp(1.0, -1074); // the smallest denormal
      break;
    case 3:
      typed.value = std::ldexp(1.0, -1022); // the smallest normal
      break;
    case 4:
      typed.value = 1.0e300;
      break;
    case 5:
      typed.value = -1.0e300;
      break;
    case 6:
      typed.value = 1.0e-300;
      break;
    case 7:
      typed.value = std::numeric_limits<double>::infinity();
      break;
    case 8:
      typed.value = -std::numeric_limits<double>::infinity();
      break;
    case 9:
      typed.value = std::numeric_limits<double>::quiet_NaN();
      break;
    case 10:
      typed.value = std::ldexp(rng.Range(1.0, 2.0), static_cast<int>(rng.Below(2200)) - 1100);
      break;
    default:
      typed.value = rng.Range(-1.0e6, 1.0e6);
      break;
    }
  }

  __declspec(naked) void RetailFrexp(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebp, [esp + 20]      // in
      mov  ebx, [esp + 24]      // out
      lea  eax, [ebx + 8]       // the exponent slot
      push eax                  // the second stack argument
      push dword ptr [ebp + 4]  // the value, high word
      push dword ptr [ebp]      // the value, low word
      mov  eax, 0x00A8DA05
      call eax
      add  esp, 12              // the retail does not pop its arguments
      fstp qword ptr [ebx]       // the mantissa comes back on the x87 stack
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursFrexp(const void* in, void* out)
  {
    const FrexpIn& typedIn = *static_cast<const FrexpIn*>(in);
    FrexpOut& typedOut = *static_cast<FrexpOut*>(out);
    typedOut.mantissa = msvc8::frexp(typedIn.value, &typedOut.exponent);
  }

  // --- msvc8::asin - retail 0x00A8DE20 (the CRT's _CIasin) ----------------
  //
  //   0x00A8DE20  cmp  dword ptr [0x01107030], 0   <- global_mode_sse2
  //   0x00A8DE27  je   0x00A8DE5B                 <- the x87 lane
  //   0x00A8DE41  fnstcw word ptr [esp]
  //   0x00A8DE4C  cmp  ax, 0x7f / jne 0x00A8DE5B
  //   0x00A8DE56  jmp  0x00AA7D50                 <- the SSE2 lane
  //   0x00A8DE5B  sub  esp, 0xc / fst qword ptr [esp] / call 0x00AA7C48
  //
  // The argument arrives ON THE X87 STACK (0x00AA7D59 `fstp qword ptr
  // [esp]`), not in a register and not on the memory stack, and the answer
  // comes back the same way.
  //
  // The harness seeds the retail's `global_mode_sse2` (0x01107030) with 1 and
  // runs with MXCSR 0x1F80 and the x87 control word 0x007F, so the retail
  // takes its SSE2 lane - exactly the lane the engine's `msvc8::asin` takes
  // (its own `global_mode_sse2` is 1 as well).

  struct AsinIn
  {
    double value;
  };

  struct AsinOut
  {
    double result;
  };

  void GenAsin(Rng& rng, void* in, const DrawKind kind)
  {
    AsinIn& typed = *static_cast<AsinIn*>(in);
    if (kind == DrawKind::Ordinary) {
      typed.value = rng.Range(-1.0, 1.0);
      return;
    }
    switch (rng.Below(12)) {
    case 0:
      typed.value = 0.0;
      break;
    case 1:
      typed.value = -0.0;
      break;
    case 2:
      typed.value = 1.0;
      break;
    case 3:
      typed.value = -1.0;
      break;
    case 4:
      typed.value = 1.0 + std::ldexp(rng.Range(1.0, 2.0), -52); // just outside
      break;
    case 5:
      typed.value = -1.0 - std::ldexp(rng.Range(1.0, 2.0), -52);
      break;
    case 6:
      typed.value = 2.0;
      break;
    case 7:
      typed.value = -2.0;
      break;
    case 8:
      typed.value = std::numeric_limits<double>::quiet_NaN();
      break;
    case 9:
      typed.value = std::numeric_limits<double>::infinity();
      break;
    case 10:
      typed.value = std::ldexp(rng.Range(1.0, 2.0), -1100); // a denormal
      break;
    default:
      typed.value = rng.Range(-1.0, 1.0);
      break;
    }
  }

  __declspec(naked) void RetailAsin(const void* in, void* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ebp, [esp + 20]      // in
      mov  ebx, [esp + 24]      // out
      fld  qword ptr [ebp]      // the argument arrives on the x87 stack
      mov  eax, 0x00A8DE20
      call eax
      fstp qword ptr [ebx]       // the answer comes back on the x87 stack
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  void OursAsin(const void* in, void* out)
  {
    const AsinIn& typedIn = *static_cast<const AsinIn*>(in);
    AsinOut& typedOut = *static_cast<AsinOut*>(out);
    typedOut.result = msvc8::asin(typedIn.value);
  }

  const std::vector<Target> kTargets = {
    {
      "NormalizeVectorInto",
      0x0044F7E0u,
      "src/moho/math/Vector3f.cpp:25",
      sizeof(NormIn),
      sizeof(NormOut),
      &GenNorm,
      &RetailNormalizeVectorInto,
      &OursNormalizeVectorInto,
    },
    {
      "MatrixToQuat",
      0x004EB3F0u,
      "src/moho/math/QuaternionMath.cpp:256",
      sizeof(MatQuatIn),
      sizeof(MatQuatOut),
      &GenMatQuat,
      &RetailMatrixToQuat,
      &OursMatrixToQuat,
    },
    {
      "RotateQuatByAngle",
      0x004EB740u,
      "src/moho/math/QuaternionMath.cpp:557",
      sizeof(RotQuatIn),
      sizeof(RotQuatOut),
      &GenRotQuat,
      &RetailRotateQuatByAngle,
      &OursRotateQuatByAngle,
    },
    {
      "QuatLERP",
      0x004EBA80u,
      "src/moho/math/QuaternionMath.cpp:408",
      sizeof(LerpIn),
      sizeof(LerpOut),
      &GenLerp,
      &RetailQuatLERP,
      &OursQuatLERP,
    },
    {
      "SLERP",
      0x004EBC00u,
      "src/moho/math/QuaternionMath.cpp:466",
      sizeof(SlerpIn),
      sizeof(SlerpOut),
      &GenSlerp,
      &RetailSLERP,
      &OursSLERP,
    },
    {
      "NormalizeQuatInPlace",
      0x004EDAA0u,
      "src/moho/math/QuaternionMath.cpp:220",
      sizeof(NormQuatIn),
      sizeof(NormQuatOut),
      &GenNormQuat,
      &RetailNormalizeQuatInPlace,
      &OursNormalizeQuatInPlace,
    },
    {
      "QuatsNearEqual",
      0x004EDED0u,
      "src/moho/math/QuaternionMath.cpp:201",
      sizeof(NearIn),
      sizeof(NearOut),
      &GenNear,
      &RetailQuatsNearEqual,
      &OursQuatsNearEqual,
    },
    {
      "QuatFromVecRot",
      0x0069AA50u,
      "src/moho/math/QuaternionMath.cpp:606",
      sizeof(VecRotIn),
      sizeof(VecRotOut),
      &GenVecRot,
      &RetailQuatFromVecRot,
      &OursQuatFromVecRot,
    },
    {
      "QuatToAxisAndAngle",
      0x006C1070u,
      "src/moho/math/QuaternionMath.cpp:673",
      sizeof(AxisAngleIn),
      sizeof(AxisAngleOut),
      &GenAxisAngle,
      &RetailQuatToAxisAndAngle,
      &OursQuatToAxisAndAngle,
    },
    {
      "QuatCrossAdd",
      0x0044F880u,
      "src/moho/sim/Sim.cpp:6286",
      sizeof(CrossAddIn),
      sizeof(CrossAddOut),
      &GenCrossAdd,
      &RetailQuatCrossAdd,
      &OursQuatCrossAdd,
    },
    {
      "Vector4fAssign",
      0x0046FB20u,
      "src/moho/math/Vector4f.cpp:84",
      sizeof(V4AssignIn),
      sizeof(V4AssignOut),
      &GenV4Assign,
      &RetailVector4fAssign,
      &OursVector4fAssign,
    },
    {
      "VEC_LargestAxis",
      0x005382E0u,
      "src/moho/math/Vector3f.cpp:116",
      sizeof(LargestAxisIn),
      sizeof(LargestAxisOut),
      &GenLargestAxis,
      &RetailVECLargestAxis,
      &OursVECLargestAxis,
    },
    {
      "VecSetLength",
      0x005B1C90u,
      "src/moho/math/Vector3f.cpp:149",
      sizeof(SetLengthIn),
      sizeof(SetLengthOut),
      &GenSetLength,
      &RetailVecSetLength,
      &OursVecSetLength,
    },
    {
      "VecSetLengthTo",
      0x0069A360u,
      "src/moho/math/Vector3f.cpp:200",
      sizeof(SetLengthToIn),
      sizeof(SetLengthToOut),
      &GenSetLengthTo,
      &RetailVecSetLengthTo,
      &OursVecSetLengthTo,
    },
    {
      "VEC_HeadingFromXZ",
      0x00565A10u,
      "src/moho/math/Vector3f.cpp:137",
      sizeof(HeadingIn),
      sizeof(HeadingOut),
      &GenHeading,
      &RetailVECHeadingFromXZ,
      &OursVECHeadingFromXZ,
    },
    {
      "EulerRollToQuat",
      0x00570750u,
      "src/moho/math/Vector3f.cpp:311",
      sizeof(EulerIn),
      sizeof(EulerOut),
      &GenEuler,
      &RetailEulerRollToQuat,
      &OursEulerRollToQuat,
    },
    {
      "IsValidVector3f",
      0x005657F0u,
      "src/moho/math/Vector3f.cpp:12",
      sizeof(Valid3In),
      sizeof(Valid3Out),
      &GenValid3,
      &RetailIsValidVector3f,
      &OursIsValidVector3f,
    },
    {
      "IsUnitLengthVector3f",
      0x0069A1D0u,
      "src/moho/math/Vector3f.cpp:73",
      sizeof(UnitLenIn),
      sizeof(UnitLenOut),
      &GenUnitLen,
      &RetailIsUnitLengthVector3f,
      &OursIsUnitLengthVector3f,
    },
    {
      "IsValidVector2f",
      0x005657B0u,
      "src/moho/math/Vector2f.cpp:8",
      sizeof(Valid2In),
      sizeof(Valid2Out),
      &GenValid2,
      &RetailIsValidVector2f,
      &OursIsValidVector2f,
    },
    {
      "MatrixRowsToQuatCanonical",
      0x004F0CB0u,
      "src/moho/math/QuaternionMath.cpp:317",
      sizeof(MatQuatIn),
      sizeof(MatQuatOut),
      &GenMatQuatCanonical,
      &RetailMatrixRowsToQuatCanonical,
      &OursMatrixRowsToQuatCanonical,
    },
    {
      "MatrixColumnsToQuatCanonical",
      0x004F0AE0u,
      "src/moho/math/QuaternionMath.cpp:391",
      sizeof(MatQuatIn),
      sizeof(MatQuatOut),
      &GenMatQuatCanonical,
      &RetailMatrixColumnsToQuatCanonical,
      &OursMatrixColumnsToQuatCanonical,
    },
    {
      "NormalizeAngleSignedRadians",
      0x0062FB50u,
      "src/moho/math/QuaternionMath.cpp:117",
      sizeof(AngleIn),
      sizeof(AngleOut),
      &GenAngleIn,
      &RetailNormalizeAngleSignedRadians,
      &OursNormalizeAngleSignedRadians,
    },
    {
      "QuatFromAxisAngleVector",
      0x00697360u,
      "src/moho/math/QuaternionMath.cpp:719",
      sizeof(AxisAngleVecIn),
      sizeof(AxisAngleVecOut),
      &GenAxisAngleVector,
      &RetailQuatFromAxisAngleVector,
      &OursQuatFromAxisAngleVector,
    },
    {
      "BuildRotationMatrixEulerHPR",
      0x006D1E30u,
      "src/moho/math/QuaternionMath.cpp:840",
      sizeof(EulerHprIn),
      sizeof(EulerHprOut),
      &GenEulerHpr,
      &RetailBuildRotationMatrixFromEulerHPR,
      &OursBuildRotationMatrixFromEulerHPR,
    },
    {
      "BuildTiltShortestArcDelta",
      0x0050CB50u,
      "src/moho/math/QuaternionMath.cpp:983",
      sizeof(TiltIn),
      sizeof(TiltOut),
      &GenTilt,
      &RetailBuildTiltShortestArcDelta,
      &OursBuildTiltShortestArcDelta,
    },
    {
      "BlendOrientationDeltaByMaxAngle",
      0x004EB830u,
      "src/moho/math/QuaternionMath.cpp:739",
      sizeof(BlendIn),
      sizeof(BlendOut),
      &GenBlend,
      &RetailBlendOrientationDeltaByMaxAngle,
      &OursBlendOrientationDeltaByMaxAngle,
    },
    {
      "msvc8_atan2f",
      0x004E9DF0u,
      "src/legacy/math/X87Math.cpp:465",
      sizeof(Atan2In),
      sizeof(Atan2Out),
      &GenAtan2,
      &RetailAtan2f,
      &OursAtan2f,
    },
    {
      "msvc8_cosf",
      0x005734D0u,
      "src/legacy/math/X87Math.cpp:431",
      sizeof(TrigIn),
      sizeof(TrigOut),
      &GenTrig,
      &RetailCosf,
      &OursCosf,
    },
    {
      "msvc8_sinf",
      0x005734E0u,
      "src/legacy/math/X87Math.cpp:413",
      sizeof(TrigIn),
      sizeof(TrigOut),
      &GenTrig,
      &RetailSinf,
      &OursSinf,
    },
    {
      "msvc8_frexp",
      0x00A8DA05u,
      "src/legacy/math/X87Math.cpp:291",
      sizeof(FrexpIn),
      sizeof(FrexpOut),
      &GenFrexp,
      &RetailFrexp,
      &OursFrexp,
    },
    {
      "msvc8_asin",
      0x00A8DE20u,
      "src/legacy/math/X87Math.cpp:773",
      sizeof(AsinIn),
      sizeof(AsinOut),
      &GenAsin,
      &RetailAsin,
      &OursAsin,
    },
  };
}  // namespace

const Target* Targets(int& count)
{
  // The aggregator idiom wmdiff uses: each slice's count is fetched in its
  // own statement, because MSVC evaluates the trailing `n` of an argument
  // list before the earlier ones. With one slice it does not matter, but keep
  // the shape so the second slice cannot break it.
  count = static_cast<int>(kTargets.size());
  return kTargets.data();
}
}  // namespace fafdiff
