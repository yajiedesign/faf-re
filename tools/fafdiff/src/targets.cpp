#include "targets.h"

#include "moho/math/QuaternionMath.h"
#include "moho/math/Vector3f.h"

#include <cmath>
#include <cstdint>
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
      "src/moho/math/QuaternionMath.cpp:1057",
      sizeof(CrossAddIn),
      sizeof(CrossAddOut),
      &GenCrossAdd,
      &RetailQuatCrossAdd,
      &OursQuatCrossAdd,
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
