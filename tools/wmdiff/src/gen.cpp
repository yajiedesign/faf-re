#include "fuzz.h"

#include <cfloat>
#include <cmath>

namespace wmdiff
{
  bool gAllowSnan = false;

  namespace
  {
    constexpr std::uint32_t kEdgeBits[] = {
      0x00000000u, 0x80000000u,             // +-0
      0x7F800000u, 0xFF800000u,             // +-inf
      0x7FC00000u, 0xFFC00000u, 0x7FC12345u, // qNaN, x86 "indefinite", payload
      0x00800000u, 0x80800000u,             // +-FLT_MIN
      0x00000001u, 0x80000001u,             // +-smallest denormal
      0x007FFFFFu, 0x807FFFFFu,             // +-largest denormal
      0x7F7FFFFFu, 0xFF7FFFFFu,             // +-FLT_MAX
      0x3F800000u, 0xBF800000u,             // +-1
      0x3F000000u, 0xBF000000u,             // +-0.5
      0x40000000u, 0xC0000000u,             // +-2
      0x358637BDu, 0xB58637BDu,             // +-1e-6
      0x3A83126Fu, 0xBA83126Fu,             // +-1e-3
      0x3FC90FDBu, 0xBFC90FDBu,             // +-pi/2
      0x40490FDBu, 0xC0490FDBu,             // +-pi
      0x40C90FDBu, 0xC0C90FDBu,             // +-2pi
      0x1E3CE508u, 0x60AD78ECu,             // 1e-20, 1e20
      0x3F800001u, 0x3F7FFFFFu,             // 1 +- 1ulp
      0x5F800000u, 0x1F800000u,             // 2^64, 2^-64 (x87 range probes)
    };
    constexpr std::uint32_t kEdgeCount = sizeof(kEdgeBits) / sizeof(kEdgeBits[0]);

    [[nodiscard]] float Sanitize(std::uint32_t u) noexcept
    {
      const bool nan = (u & 0x7F800000u) == 0x7F800000u && (u & 0x007FFFFFu) != 0;
      if (nan && !gAllowSnan) {
        u |= 0x00400000u;
      }
      return FromBits(u);
    }

    [[nodiscard]] float Sign(Rng& rng, const double v) noexcept
    {
      return static_cast<float>(rng.Chance(50) ? -v : v);
    }

    void NormalizeD(double* v, const int n) noexcept
    {
      double s = 0.0;
      for (int i = 0; i < n; ++i) {
        s += v[i] * v[i];
      }
      s = std::sqrt(s);
      for (int i = 0; i < n; ++i) {
        v[i] /= s;
      }
    }

    [[nodiscard]] double Perturbation(Rng& rng) noexcept
    {
      static constexpr double kEps[] = {0.0, 1e-8, 1e-7, 1e-6, 1e-5, 1e-4, 1e-3, 1e-2};
      return kEps[rng.Below(8)] * rng.Range(-1.0, 1.0);
    }

    // The binary64 edge values: the zeros, the infinities, the NaNs, the
    // smallest and largest normal and subnormal, and a few exact values in
    // between.  Every one of them is spelled as a bit pattern, so it is the
    // value the row means rather than the nearest double to a decimal.
    constexpr std::uint64_t kDoubleEdgeBits[] = {
      0x0000000000000000ull, // +0
      0x8000000000000000ull, // -0
      0x7FF0000000000000ull, // +inf
      0xFFF0000000000000ull, // -inf
      0x7FF8000000000000ull, // qNaN
      0xFFF8000000000000ull, // -qNaN
      0x7FF0000000000001ull, // sNaN
      0x0010000000000000ull, // DBL_MIN (2^-1022)
      0x000FFFFFFFFFFFFFull, // the largest subnormal
      0x0000000000000001ull, // the smallest subnormal
      0x7FEFFFFFFFFFFFFFull, // DBL_MAX
      0x3FF0000000000000ull, // 1
      0xBFF0000000000000ull, // -1
      0x3FE0000000000000ull, // 0.5
      0x4000000000000000ull, // 2
      0x3F50624DD2F1A9FCull, // 1e-3
      0x3EE5555555555555ull, // 1e-6
      0x3F1A36E2EB1C432Dull, // 1e-4
      0x7E37E43C8800759Cull, // 1e300
      0x01A56E1FC2F8F359ull, // 1e-300
      0x3FF0000000000001ull, // 1 + 1ulp
      0x3FEFFFFFFFFFFFFFull, // 1 - 1ulp
    };
    constexpr std::uint32_t kDoubleEdgeCount =
        sizeof(kDoubleEdgeBits) / sizeof(kDoubleEdgeBits[0]);
  } // namespace

  float GenEdge(Rng& rng)
  {
    return Sanitize(kEdgeBits[rng.Below(kEdgeCount)]);
  }

  double GenEdgeDouble(Rng& rng)
  {
    const std::uint64_t u = kDoubleEdgeBits[rng.Below(kDoubleEdgeCount)];
    double d = 0.0;
    std::memcpy(&d, &u, sizeof d);
    if (std::isnan(d) && !gAllowSnan) {
      const std::uint64_t quiet = 0x7FF8000000000000ull |
          (u & 0x0007FFFFFFFFFFFFull);
      std::memcpy(&d, &quiet, sizeof d);
    }
    return d;
  }

  float GenPlain(Rng& rng, const float lim)
  {
    return static_cast<float>(rng.Range(-lim, lim));
  }

  double GenDouble(Rng& rng)
  {
    // The degenerate cases a double row needs are exact in binary64: powers
    // of two, halves, and the extremes of the format.  The x87's extended
    // exponent range covers the whole binary64 range, so a double never
    // overflows on the retail side - the only thing that separates the
    // retail's arithmetic from binary64 is the 24-bit significand.
    switch (rng.Below(12)) {
    case 0:
    case 1:
      return rng.Range(-1.0, 1.0);
    case 2:
      return rng.Range(-1000.0, 1000.0);
    case 3:
      return rng.Range(-1.0e6, 1.0e6);
    case 4: { // log-uniform over the whole exponent range
      const int e = static_cast<int>(rng.Below(2048)) - 1024;
      const double m = rng.Range(1.0, 2.0);
      const double v = std::ldexp(m, e);
      return rng.Chance(50) ? v : -v;
    }
    case 5: { // a raw bit pattern, with the signalling NaNs quieted
      const std::uint64_t u = rng.Next();
      double d = 0.0;
      std::memcpy(&d, &u, sizeof d);
      if (std::isnan(d) && !gAllowSnan) {
        const std::uint64_t bits = 0x7FF8000000000000ull |
            (u & 0x0007FFFFFFFFFFFFull);
        std::memcpy(&d, &bits, sizeof d);
      }
      return d;
    }
    case 6:
    case 7:
      return GenEdgeDouble(rng);
    case 8: { // exactly representable powers of two and halves
      const int e = static_cast<int>(rng.Below(80)) - 40;
      const double v = std::ldexp(1.0, e);
      return rng.Chance(50) ? v : -v;
    }
    case 9: { // tiny
      const int e = static_cast<int>(rng.Below(300)) - 300;
      const double v = std::ldexp(rng.Range(1.0, 2.0), e);
      return rng.Chance(50) ? v : -v;
    }
    case 10: { // huge
      const int e = static_cast<int>(rng.Below(300)) + 300;
      const double v = std::ldexp(rng.Range(1.0, 2.0), e);
      return rng.Chance(50) ? v : -v;
    }
    default:
      return static_cast<double>(rng.Below(9)) - 4.0;
    }
  }

  float GenFloat(Rng& rng)
  {
    switch (rng.Below(12)) {
    case 0:
    case 1:
      return GenPlain(rng, 1.0f);
    case 2:
      return GenPlain(rng, 1000.0f);
    case 3:
      return GenPlain(rng, 1.0e6f);
    case 4: // log-uniform over every exponent, may overflow to inf
      return Sign(rng, std::ldexp(rng.Range(1.0, 2.0), static_cast<int>(rng.Below(280)) - 150));
    case 5:
      return Sanitize(rng.U32());
    case 6:
    case 7:
      return GenEdge(rng);
    case 8: // +-k ulp around an edge value
      return Sanitize(kEdgeBits[rng.Below(kEdgeCount)] + rng.Below(7) - 3);
    case 9: // tiny
      return Sign(rng, std::ldexp(rng.Range(1.0, 2.0), -40 - static_cast<int>(rng.Below(110))));
    case 10: // huge
      return Sign(rng, std::ldexp(rng.Range(1.0, 2.0), 40 + static_cast<int>(rng.Below(88))));
    default:
      return static_cast<float>(rng.Below(9)) - 4.0f; // small integers incl. 0
    }
  }

  void GenUnitVec3(Rng& rng, float* v)
  {
    double d[3];
    do {
      d[0] = rng.Range(-1, 1);
      d[1] = rng.Range(-1, 1);
      d[2] = rng.Range(-1, 1);
    } while (d[0] * d[0] + d[1] * d[1] + d[2] * d[2] < 1e-6);
    NormalizeD(d, 3);
    for (int i = 0; i < 3; ++i) {
      v[i] = static_cast<float>(d[i]);
    }
  }

  void GenVec3(Rng& rng, float* v)
  {
    const std::uint32_t mode = rng.Below(100);
    if (mode < 35) {
      GenUnitVec3(rng, v);
    } else if (mode < 60) {
      for (int i = 0; i < 3; ++i) {
        v[i] = GenFloat(rng);
      }
    } else if (mode < 75) {
      for (int i = 0; i < 3; ++i) {
        v[i] = GenPlain(rng, 100.0f);
      }
    } else if (mode < 88) { // axis-aligned / signed zeros
      static constexpr float kLane[] = {0.0f, -0.0f, 1.0f, -1.0f, 0.0f, 0.0f};
      for (int i = 0; i < 3; ++i) {
        v[i] = rng.Chance(20) ? GenPlain(rng, 10.0f) : kLane[rng.Below(6)];
      }
    } else { // unit direction at a random magnitude
      GenUnitVec3(rng, v);
      const double mag = std::ldexp(rng.Range(1.0, 2.0), static_cast<int>(rng.Below(200)) - 100);
      for (int i = 0; i < 3; ++i) {
        v[i] = static_cast<float>(v[i] * mag);
      }
    }
  }

  void GenUnitVec3d(Rng& rng, double* v)
  {
    double d[3];
    do {
      d[0] = rng.Range(-1, 1);
      d[1] = rng.Range(-1, 1);
      d[2] = rng.Range(-1, 1);
    } while (d[0] * d[0] + d[1] * d[1] + d[2] * d[2] < 1e-6);
    NormalizeD(d, 3);
    for (int i = 0; i < 3; ++i) {
      v[i] = d[i];
    }
  }

  void GenVec3d(Rng& rng, double* v)
  {
    const std::uint32_t mode = rng.Below(100);
    if (mode < 35) {
      GenUnitVec3d(rng, v);
    } else if (mode < 60) {
      for (int i = 0; i < 3; ++i) {
        v[i] = GenDouble(rng);
      }
    } else if (mode < 75) {
      for (int i = 0; i < 3; ++i) {
        v[i] = rng.Range(-100.0, 100.0);
      }
    } else if (mode < 88) { // axis-aligned / signed zeros
      static const double kLane[] = {0.0, -0.0, 1.0, -1.0, 0.0, 0.0};
      for (int i = 0; i < 3; ++i) {
        v[i] = rng.Chance(20) ? rng.Range(-10.0, 10.0) : kLane[rng.Below(6)];
      }
    } else { // unit direction at a random magnitude
      GenUnitVec3d(rng, v);
      const double mag = std::ldexp(rng.Range(1.0, 2.0), static_cast<int>(rng.Below(200)) - 100);
      for (int i = 0; i < 3; ++i) {
        v[i] *= mag;
      }
    }
  }

  void GenVecPair(Rng& rng, float* v1, float* v2)  {
    GenVec3(rng, v1);
    const std::uint32_t mode = rng.Below(100);
    if (mode < 30) {
      GenVec3(rng, v2);
      return;
    }
    const double sign = mode < 65 ? 1.0 : -1.0;
    const double scale = rng.Chance(50) ? 1.0 : std::ldexp(rng.Range(1.0, 2.0), static_cast<int>(rng.Below(20)) - 10);
    for (int i = 0; i < 3; ++i) {
      v2[i] = static_cast<float>(sign * scale * v1[i] + Perturbation(rng));
    }
  }

  void GenQuat(Rng& rng, float* q)
  {
    const std::uint32_t mode = rng.Below(100);
    if (mode < 45 || mode >= 85) {
      double d[4];
      do {
        for (double& x : d) {
          x = rng.Range(-1, 1);
        }
      } while (d[0] * d[0] + d[1] * d[1] + d[2] * d[2] + d[3] * d[3] < 1e-6);
      NormalizeD(d, 4);
      const double scale = mode < 45 ? 1.0 : rng.Range(0.25, 4.0);
      for (int i = 0; i < 4; ++i) {
        q[i] = static_cast<float>(d[i] * scale);
      }
    } else if (mode < 60) { // near identity (scalar-first)
      q[0] = static_cast<float>(1.0 + Perturbation(rng));
      for (int i = 1; i < 4; ++i) {
        q[i] = static_cast<float>(Perturbation(rng));
      }
    } else if (mode < 75) {
      for (int i = 0; i < 4; ++i) {
        q[i] = GenFloat(rng);
      }
    } else {
      for (int i = 0; i < 4; ++i) {
        q[i] = rng.Chance(50) ? GenEdge(rng) : GenPlain(rng, 1.0f);
      }
    }
  }

  void GenQuatPair(Rng& rng, float* q1, float* q2)
  {
    GenQuat(rng, q1);
    const std::uint32_t mode = rng.Below(100);
    if (mode < 30) {
      GenQuat(rng, q2);
      return;
    }
    const double sign = mode < 70 ? 1.0 : -1.0;
    for (int i = 0; i < 4; ++i) {
      q2[i] = static_cast<float>(sign * q1[i] + Perturbation(rng));
    }
  }

  float GenAngle(Rng& rng)
  {
    const std::uint32_t mode = rng.Below(100);
    if (mode < 40) {
      return GenPlain(rng, 6.2831855f);
    }
    if (mode < 55) {
      return GenPlain(rng, 0.1f);
    }
    if (mode < 70) {
      return GenPlain(rng, 3.1415927f);
    }
    if (mode < 85) { // +-k ulp around +-pi (the |rads/2| >= pi/2 gate)
      const std::uint32_t pi = 0x40490FDBu + rng.Below(9) - 4;
      return FromBits(rng.Chance(50) ? pi : (pi | 0x80000000u));
    }
    return GenFloat(rng);
  }

  float GenT(Rng& rng)
  {
    const std::uint32_t mode = rng.Below(100);
    if (mode < 50) {
      return static_cast<float>(rng.Unit());
    }
    if (mode < 65) {
      static constexpr std::uint32_t kT[] = {0x00000000u, 0x80000000u, 0x3F800000u, 0x3F000000u, 0x3F7FFFFFu, 0x00000001u};
      return FromBits(kT[rng.Below(6)]);
    }
    if (mode < 80) {
      return static_cast<float>(rng.Range(-0.5, 1.5));
    }
    return GenFloat(rng);
  }
} // namespace wmdiff
