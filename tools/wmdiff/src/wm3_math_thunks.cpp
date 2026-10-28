#include "wm3_math_thunks.h"

// The three wired leaves. The retail build returns a Real; the thunk's return
// type is the raw u32 bit pattern so a live one can hand back unmangled NaN /
// signed-zero results.

namespace wm3mathretail
{
  // The wired-up unary leaves. All three retail bodies have the same shape:
  // one float under cdecl, a plain RET (the caller pops), and the result
  // **only in ST(0)** - EAX is never written and nothing is rounded to float on
  // the way out, so each thunk has to do the rounding itself with FSTP and hand
  // back the raw bits the target row compares.
  //
  // EBP is a private frame so [ebp-4] can hold the rounded result; the saved
  // EBP makes it safe to call retail code that clobbers EBP.
  //
  // Written out one by one rather than through a macro on purpose: MSVC does not
  // splice backslash-newline continuations inside an __asm block, so a macro
  // here hands the assembler literal backslashes and the whole block fails to
  // parse (C2400 plus a C4405 per instruction).
  //
  //   FAbs     0x0040DAB0  FLD float ptr [EBP+8]; FABS; RET
  //   InvSqrt  0x0050D150  FLD float ptr [EBP+8]; FSQRT; FLD1; FDIVRP; RET
  //   Sqrt     0x00452FC0  PUSH EBP; MOV EBP,ESP; AND ESP,-8;
  //                        FLD float ptr [EBP+8]; FSQRT; MOV ESP,EBP; POP EBP; RET
  __declspec(naked) u32 FAbs(float x)
  {
    __asm {
      push ebp
      mov  ebp, esp
      push dword ptr [ebp + 8]
      mov  eax, 0x0040DAB0
      call eax
      add  esp, 4
      fstp dword ptr [ebp - 4]
      mov  eax, [ebp - 4]
      mov  esp, ebp
      pop  ebp
      ret
    }
  }

  __declspec(naked) u32 InvSqrt(float x)
  {
    __asm {
      push ebp
      mov  ebp, esp
      push dword ptr [ebp + 8]
      mov  eax, 0x0050D150
      call eax
      add  esp, 4
      fstp dword ptr [ebp - 4]
      mov  eax, [ebp - 4]
      mov  esp, ebp
      pop  ebp
      ret
    }
  }

  __declspec(naked) u32 Sqrt(float x)
  {
    __asm {
      push ebp
      mov  ebp, esp
      push dword ptr [ebp + 8]
      mov  eax, 0x00452FC0
      call eax
      add  esp, 4
      fstp dword ptr [ebp - 4]
      mov  eax, [ebp - 4]
      mov  esp, ebp
      pop  ebp
      ret
    }
  }

} // namespace wm3mathretail
