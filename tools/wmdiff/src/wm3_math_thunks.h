#pragma once

// Retail side of the Wm3::Math<float> comparison harness.
//
// A thunk is here only once its retail address is known, and then the body is
// a live call, not a stub: the three below are wired and the rest of Math is
// not.
//
// The functions that are reachable but have no retail address yet - ACos,
// ASin, ATan, Cos, Exp, Log, Log2, Log10, Sin, Sqr, Tan and the two-argument
// and fast-polynomial leaves - are not carried here. They are listed in the
// README's "What is in scope" section and in wmdiff/reachable.md, which is
// generated from the reachability analysis.
//
// Retail addresses for Wm3::Math<float> are established with Ghidra (the IDA
// export under decomp/ is not present on this machine). Fill in the matching
// thunk one at a time with a naked call through that entry point, mirroring
// tools/wmdiff/src/retail_thunks.cpp.

#include <cstdint>

namespace wm3mathretail
{
  using u32 = std::uint32_t;

  // ---- Real -> Real, one argument -----------------------------------------

// Retail 0x0040DAB0: FLD float ptr [EBP+8]; FABS; RET. Result in ST(0) only.
  u32 FAbs(float x);

  // Retail 0x00452FC0: PUSH EBP; MOV EBP,ESP; AND ESP,-8; FLD float ptr
  // [EBP+8]; FSQRT; MOV ESP,EBP; POP EBP; RET. Reached independently by two
  // address-hunting lanes (as Math::Sqrt and as Vector3::SqrtfBinary), which
  // agree. A byte-identical second copy sits at 0x0040EAF0 in another library;
  // both are the same body and neither writes EAX.
  u32 Sqrt(float x);
} // namespace wm3mathretail
