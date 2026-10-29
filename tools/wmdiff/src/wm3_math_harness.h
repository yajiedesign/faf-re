#pragma once

// Our side of the Wm3::Math<float> comparison harness.
//
// Same rule as wm3_harness.h: nothing here is a copy. Every function forwards
// straight into the real Wild Magic 3.8 headers, so the code under test is the
// source the engine compiles (Foundation/Math/Wm3Math.inl).
//
// Scalar results are returned as raw u32 bit patterns so the harness never has
// to decide how to store a NaN or a signed zero. The single exception is
// Sign(int), which returns an int and therefore comes back as its raw u32
// encoding.

#include <cstdint>

namespace wm3mathharness
{
  using u32 = std::uint32_t;

  // ---- Wm3::Math<float>: Real -> Real, one argument -----------------------

  u32 ACos(float x);
  u32 ASin(float x);
  u32 ATan(float x);
  u32 Ceil(float x);
  u32 Cos(float x);
  u32 Exp(float x);
  u32 FAbs(float x);
  u32 Floor(float x);
  u32 Log(float x);
  u32 Log2(float x);
  u32 Log10(float x);
  u32 Sin(float x);
  u32 Sqr(float x);
  u32 Sqrt(float x);
  u32 Tan(float x);
} // namespace wm3mathharness
