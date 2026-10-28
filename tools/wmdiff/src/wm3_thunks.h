#pragma once

// Retail side of the Wild Magic 3.8 comparison harness.
//
// A thunk is here only once its retail virtual address is known, and then its
// body is a live call into the retail image rather than a stub: difffuzz
// reached these addresses through the IDA export under decomp/, which this
// machine does not have, so the mapping is established separately (Ghidra) and
// each thunk is then written one at a time.
//
// The functions that are reachable but have no retail address yet are not
// carried here. They are listed in the README's "What is in scope" section and
// in wmdiff/reachable.md, which is generated from the reachability analysis.

#include <cstdint>

namespace wm3retail
{
  // Real (float) lanes.
  using u32 = std::uint32_t;

  // Retail 0x00452AF0. Traced instruction by instruction and *not* the
  // "returns 1/len and gates on JBE" shape an earlier note claimed: the fsqrt
  // result is stored with FST float [ESP+0x8] (so [ESP+0x4] holds the *length*),
  // and the following DIVSS XMM0,[ESP+0x4] computes 1.0f/length only as a scale
  // factor reused by three MULSS instructions. EAX is never written and ST(0)
  // is never laid out, so the function returns void. `this` arrives in ECX with
  // no stack arguments, and the threshold is a .rdata constant at 0x00d71bfc
  // rather than a parameter - hence no eps in the signature either. Retail
  // semantics match the 3.4 source exactly (len > eps ? v *= 1/len : v = 0).
  // __declspec(naked) goes on the definition only: MSVC rejects it on a
  // declaration that has no body (C2488), same as difffuzz's headers.
  void __cdecl Normalize(float* v);

  // Retail 0x00452AF0 again, this time keeping what the body leaves in ST(0).
  //
  // The body ends with
  //
  //   FLD  dword ptr [0x00d71bfc]   ; the 1e-6 threshold
  //   FXCH ST(1)
  //   FCOMI ST(1)
  //   FSTP ST(1)                    ; writes ST(0) into ST(1) and pops
  //
  // so the *length* - the value FCOMI compared - is what survives in ST(0) at
  // the RET: FSTP ST(1) writes before it pops, and the pop brings the copy in
  // ST(1) back to the top.  The degenerate branch (length <= the threshold)
  // pops the length and leaves 0.0f instead.
  //
  // That is the WM member's own return value (the 3.4 source returns the
  // length, and so does the rebuild), so the row below compares it.
  float NormalizeRet(float* v);

  // Retail 0x004757E0 (SquaredLength / static LengthSq - the retail image has
  // only the instance form). ECX = `this`, no stack arguments, plain RET.
  // Returns ST(0) in x87 extended precision and never rounds it to float; the
  // declared `float` return is where the rounding happens.
  float LengthSquared(const float* v);

  // Retail 0x00452AD0. thiscall: ECX = `this`, one stack argument, `RET 4`.
  // Returns ST(0) in x87 extended precision. This is the only dot product in
  // the image - the static form does not exist there.
  float Dot(const float* a, const float* b);

  // Retail 0x00452BC0. ECX = `this`, and the real signature carries a hidden
  // return pointer at [esp+4] with the operand at [esp+8], ending in `RET 8`.
  // The result is written into the caller's buffer, not returned in a register.
  void Cross(const float* a, const float* b, float* out);

  // Retail 0x00452FC0 - same function as Math::Sqrt below, reached from two
  // independent callers (Vector3<float>::Normalize and Moho::func_UnitsWillCollide).
  float SqrtfBinary(float x);
} // namespace wm3retail
