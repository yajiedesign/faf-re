#include "wm3_thunks.h"

// All bodies are empty on purpose - see wm3_thunks.h. Each one returns without
// touching its output buffer, which the harness's canary then reports as a
// mismatch. Wire a thunk up by giving it a __declspec(naked) call through the
// retail entry point, the way tools/difffuzz/src/retail_thunks.cpp does.

namespace wm3retail
{
  // The one wired-up thunk so far. difffuzz's Wm3Normalize got this wrong by
  // declaring a float return and comparing the leftover in EAX; retail is void,
  // so nothing comes back and the target must not look at a return value.
  __declspec(naked) void Normalize(float* v)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]      // our `v` -> the retail `this` in ECX
      mov  eax, 0x00452AF0
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00452AF0, keeping the length the body leaves in ST(0) - see the
  // declaration in wm3_thunks.h.
  __declspec(naked) float NormalizeRet(float* v)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]      // our `v` -> the retail `this` in ECX
      mov  eax, 0x00452AF0
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                        // ST(0) still holds the length
    }
  }

  // Retail 0x004757E0. `this` in ECX, **no** stack arguments, plain RET.
  // Returns ST(0) in x87 extended precision - it never rounds to float, so the
  // caller's own store is what rounds it. Leaving that to the C++ side (the
  // function is declared as returning float) is deliberate: comparing the raw
  // 80-bit value against our float would compare two different things.
  __declspec(naked) float LengthSquared(const float* v)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]      // our `v` -> the retail `this` in ECX
      mov  eax, 0x004757E0
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                        // float result is still live in ST(0)
    }
  }

  // Retail 0x00452AD0. thiscall: ECX = `this`, one stack argument at [esp+4],
  // and the function ends in `RET 4` so it pops that argument itself. Returns
  // ST(0) in x87 extended precision (the body is FLD/FMUL/FADDP, never FST).
  //
  // There is no separate static `Dot(a,b)` in the retail image - the instance
  // method at this one address is the only dot product in it - so both of our
  // Dot rows funnel through here.
  __declspec(naked) float Dot(const float* a, const float* b)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `a` -> the retail `this` in ECX
      push dword ptr [esp + 24]     // our `b` -> the retail stack argument
      mov  eax, 0x00452AD0
      call eax                       // `RET 4` pops `b` for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00452BC0. ECX = `this`, and because it returns a Vector3 by value
  // the real signature is (this, Vector3* result, const Vector3* v): [esp+4] is
  // the hidden return pointer and [esp+8] the operand, ending in `RET 8`. The
  // result therefore never comes back in EAX or ST(0) - it is written straight
  // into the caller's buffer.
  __declspec(naked) void Cross(const float* a, const float* b, float* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `a` -> the retail `this` in ECX
      push dword ptr [esp + 24]     // our `b`
      push dword ptr [esp + 32]     // our `out` (offsets shift after each push)
      mov  eax, 0x00452BC0
      call eax                       // `RET 8` pops both arguments for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00452FC0. cdecl, one float argument at [EBP+8], plain RET (the
  // caller pops), result in ST(0) only - EAX is never written and the value is
  // not rounded to float before returning, which is why the thunk pops it itself
  // and leaves it in ST(0) for the C++ caller to store.
  //
  // The `FUN_00452FC0` note in Wm3Vector3.h is now confirmed, but not on that
  // note's authority: the address was reached from two independent callers -
  // Vector3<float>::Normalize (0x00452AF0) and Moho::func_UnitsWillCollide
  // (0x00596930) - each matching the surrounding source store for store.
  __declspec(naked) float SqrtfBinary(float x)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      push dword ptr [esp + 20]     // our `x` -> the retail stack argument
      mov  eax, 0x00452FC0
      call eax                       // plain RET, so the argument is still ours
      add  esp, 4
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                            // float result is still live in ST(0)
    }
  }

} // namespace wm3retail
