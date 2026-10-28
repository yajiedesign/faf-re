#include "wm3_geom_thunks.h"

// See wm3_geom_thunks.h. Stubs return without touching their output buffer;
// the harness's canary then reports the row as not wired up.

namespace wm3geomretail
{
  // Retail 0x00506010, reached from the address src/sdk/moho/mesh/Mesh.cpp:489
  // already records for this function, so the anchor check is satisfied by the
  // engine's own annotation. The body is a three-iteration loop of
  // COMISS/JA pairs against Min/Max in both boxes and returns a bool in AL.
  //
  // The register assignment is the unusual part and is the reason this thunk is
  // not a copy of the Vector3 ones: `this` is in EDX and rkBox in EDI, not ECX.
  // Both COMISS/JA pairs were checked against the source
  // (`Max[i] < rkBox.Min[i] || Min[i] > rkBox.Max[i]`): the first compares
  // rkBox.Min[i] against this->Max[i] and the second this->Min[i] against
  // rkBox.Max[i], so JA (taken -> return false) is the source's condition with
  // the comparison written the other way round.
  __declspec(naked) u32 Aabb3TestIntersection(const float* self, const float* other)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  edx, [esp + 20]           // our `self`   -> the retail `this` in EDX
      mov  edi, [esp + 24]           // our `other`  -> the retail `rkBox` in EDI
      mov  eax, 0x00506010
      call eax
      movzx eax, al                  // the bool is in AL; widen it for C
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x004FFF00, reached from the CALL inside moho::AxisAlignedBoundsFromBox
  // (0x004FE2D0, anchored by src/sdk/moho/math/Wm3AxisAlignedBox3FafExtras.cpp:25)
  // and independently from func_DrawSelectionBrackets (0x007FC820) via
  // src/sdk/moho/render/SelectionBracketRenderer.cpp:299.
  //
  // thiscall: ECX = this, one stack argument holding the Vector3[8] output
  // array, and the body ends in `RET 0x4` so it pops that argument itself.
  // void - EAX is read as the output pointer on entry and is a leftover on
  // exit, so the target must not compare a return value.
  __declspec(naked) void Box3ComputeVertices(const float* self, float* out24)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      push dword ptr [esp + 24]     // our `out24` -> the retail stack argument
      mov  eax, 0x004FFF00
      call eax                       // `RET 4` pops `out24` for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }
} // namespace wm3geomretail
