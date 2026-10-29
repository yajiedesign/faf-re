#include "targets.h"

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
