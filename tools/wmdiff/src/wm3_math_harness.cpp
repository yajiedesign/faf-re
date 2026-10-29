#include "wm3_math_harness.h"

// Forwards to the real Wild Magic 3.8 sources - no copies. Build with the same
// float-relevant switches main.vcxproj uses for Win32 (/fp:precise, /O2 /Oi in
// release), so the arithmetic exercised here is the arithmetic the engine runs.

#include "fuzz.h" // Bits

#include "Wm3Math.h"

namespace wm3mathharness
{
  namespace
  {
    using Mathf = Wm3::Math<float>;
  } // namespace

  // ---- Real -> Real, one argument -----------------------------------------
  //
  // Pure forwarding: read the lane, hand it to Math<float>, store the raw bit
  // pattern of whatever comes back. Nothing is normalised or clamped here - the
  // clamping inside ACos/ASin in Wm3Math.inl is part of what is under test.

#define WM3MATH_FWD_UNARY(NAME)       \
  u32 NAME(const float x)             \
  {                                  \
    return wmdiff::Bits(Mathf::NAME(x)); \
  }

  WM3MATH_FWD_UNARY(Ceil)
  WM3MATH_FWD_UNARY(FAbs)
  WM3MATH_FWD_UNARY(Floor)
  WM3MATH_FWD_UNARY(Sqrt)

#undef WM3MATH_FWD_UNARY

  // ---- Real -> Real, two arguments ----------------------------------------

} // namespace wm3mathharness
