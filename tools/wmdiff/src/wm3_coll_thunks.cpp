#include "wm3_coll_thunks.h"

namespace wm3collretail
{
  // Retail 0x00A45730, the DistVector3Box3<float> constructor, reached as a
  // direct CALL from Moho::CColPrimitive_Box::CollideSphere (0x004FF150), the
  // function annotated at src/sdk/moho/collision/CColPrimitiveBase.cpp:420.
  //
  // It is a real thiscall: ECX = this, one pointer argument per reference
  // member, `RET 8`, returning this in EAX. It calls the Distance base
  // constructor first and stores the vtable pointer, so the caller has to hand
  // it writable storage rather than fabricate the object layout by hand.
  __declspec(naked) void DistVector3Box3Ctor(void* self, const float* vec, const float* box)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      push dword ptr [esp + 28]     // our `box` (offset shifts after each push)
      push dword ptr [esp + 28]     // our `vec`
      mov  eax, 0x00A45730
      call eax                       // `RET 8` pops both arguments for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A45C00, also a direct CALL from CollideSphere (0x004FF18E).
  // ECX = this, no stack arguments, and the squared distance comes back in
  // ST(0) - the body ends in FLD/RET, never writing EAX.
  __declspec(naked) float DistVector3Box3GetSquared(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A45C00
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                            // float result is still live in ST(0)
    }
  }

  // Retail 0x00A48030, the DistVector3Segment3<float> constructor. Identical
  // shape to the Box3 one: the two references at [this+0x34] / [this+0x38],
  // vtable 0x00D71E6C, `RET 8`, base Distance constructor called first.
  __declspec(naked) void DistVector3Segment3Ctor(void* self, const float* vec, const float* segment)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      push dword ptr [esp + 28]     // our `segment`
      push dword ptr [esp + 28]     // our `vec`
      mov  eax, 0x00A48030
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A484F0, a direct CALL from Moho::Projectile::CheckCollision
  // (0x0069D692), annotated at src/sdk/moho/projectile/Projectile.cpp:1388.
  // ECX = this, no stack arguments, result in ST(0).
  __declspec(naked) float DistVector3Segment3GetSquared(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A484F0
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A480E0. ECX = this, no stack arguments, result in ST(0).
  // Nothing here touches the x87 stack, so leaving it alone on the way out is
  // what hands the float back.
  __declspec(naked) float DistVector3Segment3Get(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A480E0
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                            // ST(0) still holds the distance
    }
  }

  // Retail 0x00A4F580. ECX = this, three stack arguments ending in `RET 0xC`.
  // All three reads stay at [esp+16]: pushing drops ESP by 4 while the argument
  // being read stays put, so walking the arguments down (vel1, vel0, fTMax)
  // keeps them in step.
  // Retail 0x00A458C0. ECX = this, three stack arguments ending in `RET 0xC`.
  // Retail 0x00A48190. ECX = this, three stack arguments ending in `RET 0xC`.
  // Retail 0x00A4F580. ECX = this, three stack arguments ending in `RET 0xC`.
  // All three reads stay at [esp+16]: pushing drops ESP by 4 while the argument
  // being read stays put, so walking the arguments down (vel1, vel0, fTMax)
  // keeps them in step.
  __declspec(naked) u32 IntrBox2Circle2Find(void* self, float fTMax,
                                            const float* vel0, const float* vel1)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      // [esp + 32], not [esp + 16]. With the four pushes above, ESP sits at
      // T - 16 where T is the entry ESP, so self is [esp + 20] (correct) and the
      // three arguments are at [esp + 24], [esp + 28] and [esp + 32]. [esp + 16]
      // reads T + 0 - the return address - so the retail function was being
      // handed a code pointer as fTMax and two stack words as the velocities.
      // That is visible in the callee: fVx and fVy arrived as NaN and every
      // ordered comparison against them came out false, which is why
      // IntrBox2Circle2::Find answered "contact" on inputs that cannot touch.
      push dword ptr [esp + 32]     // our `vel1`
      push dword ptr [esp + 32]     // our `vel0`
      push dword ptr [esp + 32]     // our `fTMax`
      mov  eax, 0x00A4F580
      call eax                       // `RET 0xC` pops all three
      movzx eax, al
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A457E0. ECX = this, no stack arguments, result in ST(0).
  __declspec(naked) float DistVector3Box3Get(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A457E0
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }


  // Retail 0x00A43420. ECX = this, three stack arguments ending in `RET 0xC`.
  __declspec(naked) u32 IntrBox3Sphere3Find(void* self, float fTMax,
                                            const float* vel0, const float* vel1)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      // [esp + 32], not [esp + 16]. With the four pushes above, ESP sits at
      // T - 16 where T is the entry ESP, so self is [esp + 20] (correct) and the
      // three arguments are at [esp + 24], [esp + 28] and [esp + 32]. [esp + 16]
      // reads T + 0 - the return address - so the retail function was being
      // handed a code pointer as fTMax and two stack words as the velocities.
      // That is visible in the callee: fVx and fVy arrived as NaN and every
      // ordered comparison against them came out false, which is why
      // IntrBox2Circle2::Find answered "contact" on inputs that cannot touch.
      push dword ptr [esp + 32]     // our `vel1`
      push dword ptr [esp + 32]     // our `vel0`
      push dword ptr [esp + 32]     // our `fTMax`
      mov  eax, 0x00A43420
      call eax                       // `RET 0xC` pops all three
      movzx eax, al
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A414C0. ECX = this, EAX = this + 0x14 (&m_kContactPoint).
  // Find has to run first and has to have succeeded - that member is only
  // written on the success paths.
  __declspec(naked) const float* IntrBox3Sphere3GetContactPoint(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A414C0
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                            // EAX still holds &m_kContactPoint
    }
  }

  // Retail 0x00A4E5F0, the IntrBox2Circle2<float> constructor. Its references
  // land at [this+0x0C] / [this+0x10] and the vtable is 0x00D71EC4, so the
  // object needs less room than the Distance-derived ones but the same
  // 64-byte buffer serves both.
  __declspec(naked) void IntrBox2Circle2Ctor(void* self, const float* box, const float* circle)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      push dword ptr [esp + 28]     // our `circle`
      push dword ptr [esp + 28]     // our `box`
      mov  eax, 0x00A4E5F0
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A4E620, a direct CALL from Moho::VisionDB::Entry::TryAdd
  // (0x0081B53D), annotated at src/sdk/moho/vision/VisionDB.cpp:396.
  //
  // ECX = this, no stack arguments, bool in EAX. The body installs SEH and a
  // stack cookie before it does anything else; that is ordinary MSVC /GS
  // codegen and works in this process, so it is left alone rather than
  // special-cased.
  __declspec(naked) u32 IntrBox2Circle2Test(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A4E620
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A4FE30, the IntrLine3Box3<float> constructor. Reached from
  // Moho::UpdateSelection (0x0086F520) at 0x0086FB03; the query that follows it
  // goes through vtable slot +0x08 rather than a direct CALL, which is why the
  // address is recorded the way it is in wm3_coll_thunks.h.
  __declspec(naked) void IntrLine3Box3Ctor(void* self, const float* line, const float* box)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      push dword ptr [esp + 28]     // our `box`
      push dword ptr [esp + 28]     // our `line`
      mov  eax, 0x00A4FE30
      call eax                       // `RET 8`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A4FE60. ECX = this, no stack arguments, bool in EAX: `MOV AL,1`
  // on the hit path, cleared on the miss path.
  __declspec(naked) u32 IntrLine3Box3Test(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A4FE60
      call eax
      movzx eax, al                 // the bool lands in AL only; EAX's
                                   // upper bytes are call residue
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A463A0, the IntrSegment3Box3<float> constructor, reached from
  // Moho::CColPrimitive_Box::CollideLine (0x004FF2D0). It takes three
  // arguments this time: two references and the `bool bSolid` member, so the
  // cleanup is `RET 0xC`.
  __declspec(naked) void IntrSegment3Box3Ctor(void* self, const float* segment,
                                              const float* box, u32 solid)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      // Three arguments, so the offsets step by 4 each time and every read
      // lands on [esp+32]: pushing shifts ESP down 4 while the argument being
      // read stays where it is. Pushed in reverse so the retail side sees
      // segment at [esp+4], box at [esp+8] and solid at [esp+0xC].
      push dword ptr [esp + 32]     // our `solid`
      push dword ptr [esp + 32]     // our `box`
      push dword ptr [esp + 32]     // our `segment`
      mov  eax, 0x00A463A0
      call eax                       // `RET 0xC` pops all three
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A462A0. ECX = this, no stack arguments, bool in AL. The EDX
  // values it pushes early on are arguments to a nested call, not to Find.
  __declspec(naked) u32 IntrSegment3Box3Find(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A462A0
      call eax
      movzx eax, al                 // the bool lands in AL only; EAX's
                                   // upper bytes are call residue
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A39230. ECX = this, no stack arguments, EAX = this + 0x1C.
  // GetSquared has to run first: it is what fills m_kClosestPoint1, and unlike
  // IntrSegment3Box3::GetPoint this member is assigned on all three branches,
  // so no conditional gating is needed here - only the ordering.
  __declspec(naked) const float* DistVector3Segment3GetClosestPoint1(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A39230
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                            // EAX still holds &m_kClosestPoint1
    }
  }

  // Retail 0x00A41530, the IntrBox3Sphere3<float> constructor. Reached from
  // Moho::CColPrimitive_Sphere::CollideBox (0x004FE860) at 0x004FE891.
  __declspec(naked) void IntrBox3Sphere3Ctor(void* self, const float* box,
                                              const float* sphere)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      push dword ptr [esp + 28]     // our `sphere`
      push dword ptr [esp + 28]     // our `box`
      mov  eax, 0x00A41530
      call eax                       // `RET 8`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A41560. ECX = this, no stack arguments, and the body writes AL
  // only on the hit path, so the upper bytes of EAX have to be discarded.
  __declspec(naked) u32 IntrBox3Sphere3Test(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A41560
      call eax
      movzx eax, al
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A41740 - GetVertexIntersection. It is a private static member
  // of the WM3 class, and the retail's Find calls it rather than inlining it
  // (the CALL is at 0x00A43831), so it has an out-of-line body of its own.
  //
  // It is cdecl, not thiscall: no register argument at all, seven floats on
  // the stack - fDx, fDy, fDz, fVx, fVy, fVz, fRSqr - and the caller removes
  // them (the retail's own caller does `ADD ESP,0x1C` after the CALL). The
  // result comes back in ST(0).
  //
  // Our arguments sit at [ESP+0x14] .. [ESP+0x2C] once the four registers
  // are pushed (argument 1 at [ESP+0x14]), and they have to go on the stack
  // again for the retail, last one first. Every push moves the frame down by
  // four bytes, so every read is at the same offset from the current ESP.
  __declspec(naked) float IntrBox3Sphere3GetVertexIntersection(float fDx,
      float fDy, float fDz, float fVx, float fVy, float fVz, float fRSqr)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      push dword ptr [esp + 0x2C]   // fRSqr
      push dword ptr [esp + 0x2C]   // fVz
      push dword ptr [esp + 0x2C]   // fVy
      push dword ptr [esp + 0x2C]   // fVx
      push dword ptr [esp + 0x2C]   // fDz
      push dword ptr [esp + 0x2C]   // fDy
      push dword ptr [esp + 0x2C]   // fDx
      mov  eax, 0x00A41740
      call eax
      add  esp, 0x1C                 // the retail's cdecl arguments
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                            // the float is still live in ST(0)
    }
  }

  // Retail 0x00A41800 - GetEdgeIntersection. The same shape with six
  // arguments: fDx, fDz, fVx, fVz, fVSqr, fRSqr. cdecl, result in ST(0).
  __declspec(naked) float IntrBox3Sphere3GetEdgeIntersection(float fDx,
      float fDz, float fVx, float fVz, float fVSqr, float fRSqr)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      push dword ptr [esp + 0x28]   // fRSqr
      push dword ptr [esp + 0x28]   // fVSqr
      push dword ptr [esp + 0x28]   // fVz
      push dword ptr [esp + 0x28]   // fVx
      push dword ptr [esp + 0x28]   // fDz
      push dword ptr [esp + 0x28]   // fDx
      mov  eax, 0x00A41800
      call eax
      add  esp, 0x18                   // the retail's cdecl arguments
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                            // the float is still live in ST(0)
    }
  }

  // ---- Wm3::IntrBox3Sphere3<float>::FindFaceRegionIntersection ------------
  //
  // Retail 0x00A41880. A thiscall member function with thirteen stack
  // arguments, and the callee removes them: the body ends in `RET 0x34`,
  // thirteen dwords.  ECX is `this`.
  //
  // The thirteen arguments are pushed here in reverse order (the thirteenth
  // first), so that the first one lands at the lowest address, which is
  // where the retail's own caller puts it.  The retail pops all thirteen
  // itself, so nothing is left to clean up after the call.
  //
  // Our own arguments sit above the four saved registers, so the thirteenth
  // of ours is at [ESP + 0x48] once the registers are pushed.  Every push
  // moves ESP down by four bytes and moves the next argument we want down by
  // four bytes as well, so the offset stays the same for all thirteen.
  __declspec(naked) int IntrBox3Sphere3FindFaceRegionIntersection(void* self,
      float fEx, float fEy, float fEz, float fCx, float fCy, float fCz,
      float fVx, float fVy, float fVz, float* pix, float* piy, float* piz,
      u32 above)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]          // our `self`
      push dword ptr [esp + 0x48]   // our `above`
      push dword ptr [esp + 0x48]   // our `piz`
      push dword ptr [esp + 0x48]   // our `piy`
      push dword ptr [esp + 0x48]   // our `pix`
      push dword ptr [esp + 0x48]   // our `fVz`
      push dword ptr [esp + 0x48]   // our `fVy`
      push dword ptr [esp + 0x48]   // our `fVx`
      push dword ptr [esp + 0x48]   // our `fCz`
      push dword ptr [esp + 0x48]   // our `fCy`
      push dword ptr [esp + 0x48]   // our `fCx`
      push dword ptr [esp + 0x48]   // our `fEz`
      push dword ptr [esp + 0x48]   // our `fEy`
      push dword ptr [esp + 0x48]   // our `fEx`
      mov  eax, 0x00A41880
      call eax                      // `RET 0x34` pops all thirteen
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::IntrBox3Sphere3<float>::FindJustEdgeIntersection --------------
  //
  // Retail 0x00A41CC0. The same shape with twelve arguments (`RET 0x30`).
  // Our thirteenth word is the thirteenth argument, so the offset is
  // [ESP + 0x44] for all twelve pushes.
  __declspec(naked) int IntrBox3Sphere3FindJustEdgeIntersection(void* self,
      float fCy, float fEx, float fEy, float fEz, float fDx, float fDz,
      float fVx, float fVy, float fVz, float* pix, float* piy, float* piz)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]          // our `self`
      push dword ptr [esp + 0x44]   // our `piz`
      push dword ptr [esp + 0x44]   // our `piy`
      push dword ptr [esp + 0x44]   // our `pix`
      push dword ptr [esp + 0x44]   // our `fVz`
      push dword ptr [esp + 0x44]   // our `fVy`
      push dword ptr [esp + 0x44]   // our `fVx`
      push dword ptr [esp + 0x44]   // our `fDz`
      push dword ptr [esp + 0x44]   // our `fDx`
      push dword ptr [esp + 0x44]   // our `fEz`
      push dword ptr [esp + 0x44]   // our `fEy`
      push dword ptr [esp + 0x44]   // our `fEx`
      push dword ptr [esp + 0x44]   // our `fCy`
      mov  eax, 0x00A41CC0
      call eax                      // `RET 0x30` pops all twelve
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::IntrBox3Sphere3<float>::FindEdgeRegionIntersection ------------
  //
  // Retail 0x00A41F50. The same shape as FindFaceRegionIntersection:
  // thirteen arguments, `RET 0x34`, the flag as the thirteenth.
  __declspec(naked) int IntrBox3Sphere3FindEdgeRegionIntersection(void* self,
      float fEx, float fEy, float fEz, float fCx, float fCy, float fCz,
      float fVx, float fVy, float fVz, float* pix, float* piy, float* piz,
      u32 above)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]          // our `self`
      push dword ptr [esp + 0x48]   // our `above`
      push dword ptr [esp + 0x48]   // our `piz`
      push dword ptr [esp + 0x48]   // our `piy`
      push dword ptr [esp + 0x48]   // our `pix`
      push dword ptr [esp + 0x48]   // our `fVz`
      push dword ptr [esp + 0x48]   // our `fVy`
      push dword ptr [esp + 0x48]   // our `fVx`
      push dword ptr [esp + 0x48]   // our `fCz`
      push dword ptr [esp + 0x48]   // our `fCy`
      push dword ptr [esp + 0x48]   // our `fCx`
      push dword ptr [esp + 0x48]   // our `fEz`
      push dword ptr [esp + 0x48]   // our `fEy`
      push dword ptr [esp + 0x48]   // our `fEx`
      mov  eax, 0x00A41F50
      call eax                      // `RET 0x34` pops all thirteen
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::IntrBox3Sphere3<float>::FindVertexRegionIntersection ----------
  //
  // Retail 0x00A421E0. The same shape as FindJustEdgeIntersection: twelve
  // arguments, `RET 0x30`, no flag.
  __declspec(naked) int IntrBox3Sphere3FindVertexRegionIntersection(void* self,
      float fEx, float fEy, float fEz, float fCx, float fCy, float fCz,
      float fVx, float fVy, float fVz, float* pix, float* piy, float* piz)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]          // our `self`
      push dword ptr [esp + 0x44]   // our `piz`
      push dword ptr [esp + 0x44]   // our `piy`
      push dword ptr [esp + 0x44]   // our `pix`
      push dword ptr [esp + 0x44]   // our `fVz`
      push dword ptr [esp + 0x44]   // our `fVy`
      push dword ptr [esp + 0x44]   // our `fVx`
      push dword ptr [esp + 0x44]   // our `fCz`
      push dword ptr [esp + 0x44]   // our `fCy`
      push dword ptr [esp + 0x44]   // our `fCx`
      push dword ptr [esp + 0x44]   // our `fEz`
      push dword ptr [esp + 0x44]   // our `fEy`
      push dword ptr [esp + 0x44]   // our `fEx`
      mov  eax, 0x00A421E0
      call eax                      // `RET 0x30` pops all twelve
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A46300, six call sites in Moho::CColPrimitive_Box::CollideLine.
  // ECX = this, one dword stack argument (the index), `RET 4`, and EAX comes
  // back holding &m_akPoint[index] - a pointer into the object, at
  // this + 0x1C + 12*index. Returning the pointer unchanged is right: the two
  // objects live at different addresses, so the target dereferences it on both
  // sides and compares the three floats.
  __declspec(naked) const float* IntrSegment3Box3GetPoint(const void* self, u32 index)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      push dword ptr [esp + 24]     // our `index`
      mov  eax, 0x00A46300
      call eax                       // `RET 4` pops the index
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                            // EAX still holds &m_akPoint[index]
    }
  }

  // ---- Wm3::IntrSegment3Sphere3<float> -------------------------------------
  //
  // Retail 0x00A46A10, the constructor. A direct CALL from
  // Moho::CColPrimitive_Sphere::CollideLine (0x004FE9D0) at 0x004FEA13, which
  // is annotated at src/sdk/moho/collision/CColPrimitiveBase.cpp:569; the same
  // function calls Find and GetPoint below. The retail's own body:
  //
  //   00a46a13  CALL 0x00a45500        ; the base constructor
  //   00a46a18  MOV EAX,dword ptr [ESP + 0x8]
  //   00a46a1c  MOV ECX,dword ptr [ESP + 0xc]
  //   00a46a20  MOV dword ptr [ESI + 0x10],EAX   ; the segment
  //   00a46a23  MOV dword ptr [ESI],0xd71e0c      ; the vtable
  //   00a46a29  MOV dword ptr [ESI + 0x14],ECX   ; the sphere
  //   00a46a33  FLD float ptr [0x00d71bfc]        ; 1e-6f
  //   00a46a39  FSTP float ptr [ESI + 0xc]        ; ZeroThreshold
  //   00a46a3f  RET 0x8
  //
  // ECX = `this`; one pointer per reference member, `RET 8`.
  //
  // Both reads are [esp + 28]. With the four pushes above ESP sits at T - 16,
  // so the second argument is at [esp + 28] (T + 12); after the first push has
  // moved ESP to T - 20 the first argument is at [esp + 28] as well (T + 8).
  // Pushed in reverse order, so the retail sees the segment at [esp+4] and the
  // sphere at [esp+8].
  //
  // The same rule the three-argument constructor uses: each push moves ESP
  // down by 4 while the argument being read stays where it is, so every read
  // lands on the same offset. [esp + 12] would be T - 8 - the saved ESI, not
  // an argument - which is what the two-argument constructors used to read;
  // they worked only because the caller happened to leave the first argument
  // in ESI at the call site.
  __declspec(naked) void IntrSegment3Sphere3Ctor(void* self, const float* segment,
                                                 const float* sphere)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      push dword ptr [esp + 28]     // our `sphere`
      push dword ptr [esp + 28]     // our `segment`
      mov  eax, 0x00A46A10
      call eax                       // `RET 8` pops both arguments for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A471C0, the no-argument Find(). A direct CALL from the same
  // CollideLine at 0x004FEA27 (`TEST AL,AL` on the result), and the class
  // vtable's +0x10 slot. ECX = `this`, no stack arguments, and the bool comes
  // back in AL only - the body ends in `SETG AL`, so EAX's upper bytes are
  // call residue.
  //
  // The body writes m_iQuantity at [this+0x18] and the intersection points at
  // [this+0x1C] and [this+0x28], which is what makes this class a better row
  // than IntrBox3Sphere3: what it writes is defined.
  __declspec(naked) u32 IntrSegment3Sphere3Find(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A471C0
      call eax
      movzx eax, al                 // the bool lands in AL only
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A46C80, the no-argument Test(). The class vtable's +0x08 slot
  // (the dynamic Find dispatches it through that slot: "MOV EDX,[EAX+0x8];
  // CALL EDX"), so the vtable is the anchor here.
  //
  // ECX = `this`, no stack arguments, the bool comes back in AL only. The
  // body is the same quadratic as Find() - the segment at [this+0x10], the
  // sphere at [this+0x14], the discriminant, then the two extent tests -
  // without the roots.
  __declspec(naked) u32 IntrSegment3Sphere3Test(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A46C80
      call eax
      movzx eax, al                 // the bool lands in AL only
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A46B10, the swept Test. The class vtable's +0x04 slot.
  //
  // ECX = `this`; three stack arguments and `RET 0xC` pops all three.
  //
  // The three reads are [esp + 32]: each push moves ESP down by 4 while the
  // argument being read stays where it is, so every read lands on the same
  // offset.
  //
  // Push order is fTMax, vel1, vel0 - NOT vel1, vel0, fTMax. The callee's
  // arg1 is the value pushed *last*, so this order leaves arg1 = vel0,
  // arg2 = vel1, arg3 = fTMax. That is what retail's swept path needs: it
  // computes Delta = arg2 - arg1 (0x00A46B68: `fld [eax]` / `fsub [ecx]`
  // with eax = [ESP+0x90], ecx = [ESP+0x8C]), and the relative velocity has to
  // come out as vel1 - vel0. Pushed the other way round the retail sees
  // arg1 = fTMax and computes vel0 - fTMax, which is garbage.
  //
  // Note the signature difference this thunk absorbs: retail 3.8 takes
  // (vel0, vel1, tMax) while the rebuilt WM3.4 API takes
  // Test(fTMax, velocity0, velocity1). Bridging belongs here, not in the
  // engine's API.
  __declspec(naked) u32 IntrSegment3Sphere3TestDynamic(void* self, float fTMax,
                                                       const float* vel0,
                                                       const float* vel1)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      push dword ptr [esp + 32]     // our `fTMax`  -> arg3
      push dword ptr [esp + 32]     // our `vel1`   -> arg2
      push dword ptr [esp + 32]     // our `vel0`   -> arg1
      mov  eax, 0x00A46B10
      call eax                       // `RET 0xC` pops all three
      movzx eax, al                 // the bool lands in AL only
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A46DB0, the swept Find. The class vtable's +0x0C slot. Same
  // argument shape and the same push order as the swept Test above - the
  // callee must see arg1 = vel0, arg2 = vel1, arg3 = fTMax. The body also
  // writes the contact time and the contact point.
  __declspec(naked) u32 IntrSegment3Sphere3FindDynamic(void* self, float fTMax,
                                                       const float* vel0,
                                                       const float* vel1)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      push dword ptr [esp + 32]     // our `fTMax`  -> arg3
      push dword ptr [esp + 32]     // our `vel1`   -> arg2
      push dword ptr [esp + 32]     // our `vel0`   -> arg1
      mov  eax, 0x00A46DB0
      call eax                       // `RET 0xC` pops all three
      movzx eax, al                 // the bool lands in AL only
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A46940, GetPoint(int). Three call sites in the same CollideLine
  // (0x004FEA3A, 0x004FEA98, 0x004FEAED). ECX = `this`, one dword stack
  // argument (the index) at [esp+4], `RET 4`, and EAX comes back holding
  // &m_akPoint[index]:  //
  //   00a46940  MOV EAX,dword ptr [ESP + 0x4]
  //   00a46944  LEA EAX,[EAX + EAX*0x2]
  //   00a46947  LEA EAX,[ECX + EAX*0x4 + 0x1c]
  //   00a4694b  RET 0x4
  //
  // Returning the pointer unchanged is right: the two objects live at
  // different addresses, so the target dereferences it on both sides.
  __declspec(naked) const float* IntrSegment3Sphere3GetPoint(const void* self,
                                                             u32 index)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      push dword ptr [esp + 24]     // our `index` (T + 8, ESP = T - 16)
      mov  eax, 0x00A46940
      call eax                       // `RET 4` pops the index
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                            // EAX still holds &m_akPoint[index]
    }
  }

  // ---- Wm3::IntrBox2Box2<float> --------------------------------------------
  //
  // Retail 0x00A47BF0, the constructor. A direct CALL from the AI steering
  // test at 0x00596DB2 (the function 0x00596930, annotated at
  // src/sdk/moho/ai/CAiSteeringImpl.cpp:268):
  //
  //   00a47bf3  CALL 0x00a454e0        ; the base constructor
  //   00a47bf8  MOV EAX,dword ptr [ESP + 0x8]
  //   00a47bfc  MOV ECX,dword ptr [ESP + 0xc]
  //   00a47c00  MOV dword ptr [ESI + 0xc],EAX   ; box 0
  //   00a47c03  MOV dword ptr [ESI],0xd71e3c      ; the vtable
  //   00a47c09  MOV dword ptr [ESI + 0x10],ECX   ; box 1
  //   00a47c0f  RET 0x8
  //
  // ECX = `this`; one pointer per reference member, `RET 8`. Both reads are
  // [esp + 28] - see the note on the IntrSegment3Sphere3 constructor above.
  __declspec(naked) void IntrBox2Box2Ctor(void* self, const float* box0,
                                           const float* box1)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      push dword ptr [esp + 28]     // our `box1`
      push dword ptr [esp + 28]     // our `box0`
      mov  eax, 0x00A47BF0
      call eax                       // `RET 8` pops both arguments for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A47D40, Test(). A direct CALL from the same AI steering test at
  // 0x00596DC3 (`MOV BL,AL` on the result), and the class vtable's +0x0C slot.
  // ECX = `this`, no stack arguments, the bool comes back in AL only. The body
  // is the separating-axis test: the difference of the two centres, the four
  // projections of the two boxes' extents onto the four axes, and a `FCOMPP`
  // per axis - `JP` for the unordered case, so a NaN falls through to the next
  // axis exactly as the rebuilt `if (fAbsAdD > fRSum) return false;` does.
  __declspec(naked) u32 IntrBox2Box2Test(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A47D40
      call eax
      movzx eax, al                 // the bool lands in AL only
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::DistSegment3Segment3<float> ----------------------------------
  // Retail 0x00A7E190, Wm3::DistSegment3Segment3<float>'s constructor. Same
  // shape as DistVector3Box3's: the two segments land at [this+0x34] and
  // [this+0x38], the vtable is 0x00D72254, `RET 8`. The body reads its two
  // arguments at [ESP+0x08] and [ESP+0x0C] *after* its own `push esi`, so they
  // are the two slots this thunk fills.
  __declspec(naked) void DistSegment3Segment3Ctor(void* self, const float* segment0,
                                                   const float* segment1)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      push dword ptr [esp + 28]     // our `segment1`
      push dword ptr [esp + 28]     // our `segment0`
      mov  eax, 0x00A7E190
      call eax                       // `RET 8` pops both arguments for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A7E660, Wm3::DistSegment3Segment3<float>::GetSquared. ECX =
  // `this`, no stack arguments, and like the other distance queries it ends in
  // FLD/RET with the result in ST(0) - EAX is scratch. So the float is stored
  // through the caller's `out` and read back as raw bits by the caller.
  __declspec(naked) void DistSegment3Segment3GetSquared(const void* self, float* out)
  {
    __asm {
      push ebp
      mov  ecx, [esp + 8]            // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A7E660
      call eax
      mov  eax, [esp + 12]           // our `out`
      fstp dword ptr [eax]
      pop  ebp
      ret
    }
  }

  // Retail 0x00A6BE80, Wm3::IntrSegment3Capsule3<float>'s constructor. ECX =
  // `this`; the segment and the capsule are two stack pointers, stored at
  // [this+0x0C] and [this+0x10] in that order (segment first, capsule
  // second, the WM3.8 member order), vtable 0x00D72094, `RET 8`.
  __declspec(naked) void IntrSegment3Capsule3Ctor(void* self, const float* segment,
                                                  const float* capsule)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      push dword ptr [esp + 28]     // our `capsule`
      push dword ptr [esp + 28]     // our `segment`
      mov  eax, 0x00A6BE80
      call eax                       // `RET 8`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A6BEB0, IntrSegment3Capsule3<float>::Test(). ECX = `this`, no
  // stack arguments, the bool in AL only.
  __declspec(naked) u32 IntrSegment3Capsule3Test(void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A6BEB0
      call eax
      movzx eax, al
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A7F730, Wm3::IntrLine3Capsule3<float>'s constructor. ECX =
  // `this`; the line and the capsule are two stack pointers, stored at
  // [this+0x0C] and [this+0x10] in that order (line first, capsule second,
  // the WM3.8 member order), vtable 0x00D722A4, `RET 8`.
  __declspec(naked) void IntrLine3Capsule3Ctor(void* self, const float* line,
                                              const float* capsule)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      push dword ptr [esp + 28]     // our `capsule`
      push dword ptr [esp + 28]     // our `line`
      mov  eax, 0x00A7F730
      call eax                       // `RET 8`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A7F760, IntrLine3Capsule3<float>::Test(). ECX = `this`, no
  // stack arguments, the bool in EAX: `MOV EAX,1` on the hit path and
  // `XOR EAX,EAX` on the miss path.
  __declspec(naked) u32 IntrLine3Capsule3Test(void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A7F760
      call eax
      movzx eax, al                 // the bool lands in AL only
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A67120, Wm3::ConvexHull3<float>'s constructor. ECX = `this`,
  // five stack arguments (iVertexQuantity, akVertex, fEpsilon, bOwner,
  // eQueryType) and `RET 0x14`.
  __declspec(naked) void ConvexHull3Ctor(void* self, int n, const float* verts,
                                         float eps, u32 owner, u32 type)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this`
      mov  ebx, [esp + 24]           // `n`
      mov  esi, [esp + 28]           // `verts`
      mov  edi, [esp + 32]           // `eps` (the raw bits)
      mov  ebp, [esp + 36]           // `owner`
      push dword ptr [esp + 40]      // `type`
      push ebp                       // `owner`
      push edi                       // `eps`
      push esi                       // `verts`
      push ebx                       // `n`
      mov  eax, 0x00A67120
      call eax                       // `RET 0x14`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A66440, Wm3::ConvexHull3<float>'s destructor (the class's own,
  // not the vtable's scalar deleting destructor at 0x00A665D0). ECX = `this`,
  // no stack arguments, a plain `RET`.
  __declspec(naked) void ConvexHull3Dtor(void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A66440
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A55CE0, Wm3::Query3<float>::ToPlane - the class vtable
  // 0x00D71F38's +0x08 slot. ECX = `this`, four stack arguments (the point's
  // address and the three vertex indices), `RET 0x10`, the sign in EAX.
  __declspec(naked) int Query3ToPlane(const void* self, const float* point,
                                      int iV0, int iV1, int iV2)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this`
      push dword ptr [esp + 36]      // `iV2`
      push dword ptr [esp + 36]      // `iV1`
      push dword ptr [esp + 36]      // `iV0`
      push dword ptr [esp + 36]      // `point`
      mov  eax, 0x00A55CE0
      call eax                       // `RET 0x10`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A7E230, Wm3::DistSegment3Segment3<float>::Get(). Seven
  // instructions: dispatch the vtable slot +0x18 (GetSquared), take the CRT
  // square root at 0x00A91000, narrow that to float through a stack slot,
  // reload it and return in ST(0).
  __declspec(naked) float DistSegment3Segment3Get(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A7E230
      call eax                       // the float result is live in ST(0)
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::DistLine3Segment3<float> ---------------------------------------
  // Retail 0x00A80900, Wm3::DistLine3Segment3<float>'s constructor. The same
  // shape as DistSegment3Segment3's: it calls the base constructor at
  // 0x00A39F20, stores the line at [this+0x34] and the segment at
  // [this+0x38], writes the vtable 0x00D722D4 and does `RET 8`. The body
  // reads its two arguments at [ESP+0x08] and [ESP+0x0C] *after* its own
  // `push esi`, so they are the two slots this thunk fills.
  __declspec(naked) void DistLine3Segment3Ctor(void* self, const float* line,
                                               const float* segment)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      push dword ptr [esp + 28]     // our `segment`
      push dword ptr [esp + 28]     // our `line`
      mov  eax, 0x00A80900
      call eax                       // `RET 8` pops both arguments for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A80DE0, Wm3::DistLine3Segment3<float>::GetSquared. ECX =
  // `this`, no stack arguments, and like the other distance queries it ends
  // in FLD/RET with the result in ST(0) - the body ends with FABS/FSTP/FLD,
  // so the float is the absolute value of the squared distance.
  __declspec(naked) void DistLine3Segment3GetSquared(const void* self, float* out)
  {
    __asm {
      push ebp
      mov  ecx, [esp + 8]            // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A80DE0
      call eax
      mov  eax, [esp + 12]           // our `out`
      fstp dword ptr [eax]
      pop  ebp
      ret
    }
  }

  // Retail 0x00A809A0, Wm3::DistLine3Segment3<float>::Get(). It dispatches
  // the vtable slot +0x18 (GetSquared) and jumps to the CRT square root at
  // 0x00A91000; the float comes back in ST(0).
  __declspec(naked) float DistLine3Segment3Get(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A809A0
      call eax                       // the float result is live in ST(0)
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A80A40, Wm3::DistLine3Segment3<float>::Get(fT,v0,v1). Thiscall
  // with three stack arguments - fT, &rkVelocity0, &rkVelocity1 - and `RET
  // 0xC`; the float comes back in ST(0).
  __declspec(naked) void DistLine3Segment3GetSwept(const void* self, float fT,
                                                   const float* vel0,
                                                   const float* vel1, float* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      push dword ptr [esp + 32]     // our `vel1`
      push dword ptr [esp + 32]     // our `vel0`
      push dword ptr [esp + 32]     // our `fT`
      mov  eax, 0x00A80A40
      call eax                       // `RET 0xC` pops the three arguments
      mov  eax, [esp + 36]           // our `out`
      fstp dword ptr [eax]
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A80C10, Wm3::DistLine3Segment3<float>::GetSquared(fT,v0,v1).
  // The same shape as the Get above.
  __declspec(naked) void DistLine3Segment3GetSquaredSwept(const void* self,
                                                          float fT,
                                                          const float* vel0,
                                                          const float* vel1,
                                                          float* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      push dword ptr [esp + 32]     // our `vel1`
      push dword ptr [esp + 32]     // our `vel0`
      push dword ptr [esp + 32]     // our `fT`
      mov  eax, 0x00A80C10
      call eax                       // `RET 0xC` pops the three arguments
      mov  eax, [esp + 36]           // our `out`
      fstp dword ptr [eax]
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A6C070, Wm3::IntrSegment3Capsule3<float>::Find(). ECX = `this`,
  // no stack arguments, the bool comes back in AL only. The body leaves
  // m_iQuantity at [this+0x14], the intersection points at [this+0x18] and
  // [this+0x24] and the segment parameters at [this+0x30] and [this+0x34].
  __declspec(naked) u32 IntrSegment3Capsule3Find(void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A6C070
      call eax
      movzx eax, al
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A7F920, the static Wm3::IntrLine3Capsule3<float>::Find(). Four
  // pointer arguments - the line's origin, the line's direction, the capsule
  // and the afT output array - and the number of roots comes back in EAX.
  //
  // The retail's body leaves the four arguments on the stack (it ends in
  // `ADD ESP,0x3C; RET`), so the caller removes them: `ADD ESP,0x10` here.
  //
  // The four arguments are read out of our own frame before they are pushed:
  // after the four callee-saved pushes our arguments sit at [ESP+0x20] to
  // [ESP+0x2C], and the pushes go in the reverse order the callee expects
  // them (afT first, origin last), so that the callee sees the origin at
  // [ESP+4], the direction at [ESP+8], the capsule at [ESP+0xC] and afT at
  // [ESP+0x10].
  __declspec(naked) int IntrLine3Capsule3Find(const float* origin, const float* dir,
                                              const float* capsule, float* afT)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  eax, [esp + 20]           // our `origin`
      mov  ebx, [esp + 24]           // our `dir`
      mov  ecx, [esp + 28]           // our `capsule`
      mov  edx, [esp + 32]           // our `afT`
      push edx
      push ecx
      push ebx
      push eax
      mov  eax, 0x00A7F920
      call eax
      add  esp, 16
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A6C5E0, Wm3::DistVector2Box2<float>'s constructor. Same shape
  // as DistVector3Box3's: the vector and the box land at [this+0x2C] and
  // [this+0x30], the vtable is 0x00D720C4, `RET 8`.
  __declspec(naked) void DistVector2Box2Ctor(void* self, const float* vec,
                                             const float* box)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      push dword ptr [esp + 28]     // our `box`
      push dword ptr [esp + 28]     // our `vec`
      mov  eax, 0x00A6C5E0
      call eax                       // `RET 8` pops both arguments for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A6C9D0, Wm3::DistVector2Box2<float>::GetSquared. ECX = `this`,
  // no stack arguments, and the float ends in ST(0), so it is stored through
  // the caller's `out` and read back as raw bits.
  __declspec(naked) void DistVector2Box2GetSquared(const void* self, float* out)
  {
    __asm {
      push ebp
      mov  ecx, [esp + 8]            // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A6C9D0
      call eax
      mov  eax, [esp + 12]           // our `out`
      fstp dword ptr [eax]
      pop  ebp
      ret
    }
  }

  // Retail 0x00A6C680, Wm3::DistVector2Box2<float>::Get() - the class vtable's
  // +0x0C slot. The body dispatches the vtable's +0x18 slot (GetSquared), takes
  // the CRT's double square root at 0x00A91000 and narrows the result back to
  // a float, so the float ends in ST(0). Same shape as GetSquared's thunk.
  __declspec(naked) void DistVector2Box2Get(const void* self, float* out)
  {
    __asm {
      push ebp
      mov  ecx, [esp + 8]            // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A6C680
      call eax
      mov  eax, [esp + 12]           // our `out`
      fstp dword ptr [eax]
      pop  ebp
      ret
    }
  }

  // Retail 0x00A508F0, Wm3::IntrLine3Box3<float>::Find() - the class vtable's
  // +0x10 slot. ECX = `this`, no stack arguments, the bool in AL only. The body
  // tail-calls the shared DoClipping at 0x00A50220, whose last three
  // instructions are `XOR EAX,EAX; CMP [ECX],EAX; SETNE AL`.
  __declspec(naked) u32 IntrLine3Box3Find(void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A508F0
      call eax
      movzx eax, al                  // the bool lands in AL only
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A6BDF0, Wm3::IntrSegment3Capsule3<float>::GetPoint(int).
  // ECX = `this`, one dword stack argument (the index), `RET 4`, and EAX comes
  // back holding &m_akPoint[index] - a pointer into the object at
  // this + 0x18 + 12*index, not a Vector3 by value. Returning the pointer
  // unchanged is right: the two objects live at different addresses, so the
  // target dereferences it on both sides and compares the three floats.
  __declspec(naked) const float* IntrSegment3Capsule3GetPoint(const void* self,
                                                              u32 index)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      push dword ptr [esp + 24]      // our `index`
      mov  eax, 0x00A6BDF0
      call eax                       // `RET 4` pops the index
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                            // EAX still holds &m_akPoint[index]
    }
  }

  // Retail 0x00A6BE00, Wm3::IntrSegment3Capsule3<float>::GetParameter(int).
  // ECX = `this`, one dword stack argument (the index), `RET 4`, and the float
  // ends in ST(0) - it is FLDed out of m_afParameter at this + 0x30 + 4*index.
  __declspec(naked) void IntrSegment3Capsule3GetParameter(const void* self,
                                                          u32 index, float* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  ebx, [esp + 28]           // our `out`
      push dword ptr [esp + 24]      // our `index`
      mov  eax, 0x00A6BE00
      call eax                       // `RET 4` pops the index
      fstp dword ptr [ebx]           // ST(0) -> our `out`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A39110, Wm3::Distance<float,Vector3<float>>::GetDerivative -
  // the base class's generic body, at the class vtable's +0x1C slot. ECX =
  // `this`, three stack arguments (fT, rkVelocity0, rkVelocity1), `RET 0xC`,
  // and the float in ST(0).
  //
  // The three arguments are read out of our own frame before they are pushed:
  // after the four callee-saved pushes our arguments sit at [ESP+0x20] to
  // [ESP+0x2C], and the same literal offset reads the next argument each
  // time, because every push moves ESP down by four while the argument being
  // read stays where it is. The order is the reverse of the callee's, so the
  // callee sees fT at [ESP+4], the velocity0 pointer at [ESP+8] and the
  // velocity1 pointer at [ESP+0xC].
  __declspec(naked) void DistanceVector3GetDerivative(const void* self, float fT,
                                                      const float* vel0,
                                                      const float* vel1, float* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  edi, [esp + 36]           // our `out`
      push dword ptr [esp + 32]      // our `vel1`
      push dword ptr [esp + 32]      // our `vel0`
      push dword ptr [esp + 32]      // our `fT`, pushed as its raw bits
      mov  eax, 0x00A39110
      call eax                       // `RET 0xC` pops the three arguments
      fstp dword ptr [edi]           // ST(0) -> our `out`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A39180, Wm3::Distance<float,Vector3<float>>::GetDerivativeSquared
  // - the base class's generic body, at the class vtable's +0x20 slot. Same
  // ABI as GetDerivative: ECX = `this`, three stack arguments, `RET 0xC`, the
  // float in ST(0).
  __declspec(naked) void DistanceVector3GetDerivativeSquared(const void* self,
                                                              float fT,
                                                              const float* vel0,
                                                              const float* vel1,
                                                              float* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  edi, [esp + 36]           // our `out`
      push dword ptr [esp + 32]      // our `vel1`
      push dword ptr [esp + 32]      // our `vel0`
      push dword ptr [esp + 32]      // our `fT`, pushed as its raw bits
      mov  eax, 0x00A39180
      call eax                       // `RET 0xC` pops the three arguments
      fstp dword ptr [edi]           // ST(0) -> our `out`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A46440, Wm3::IntrSegment3Box3<float>::Test() - the class
  // vtable's +0x08 slot. ECX = `this`, no stack arguments, the bool in AL.
  __declspec(naked) u32 IntrSegment3Box3Test(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A46440
      call eax
      movzx eax, al                  // the bool lands in AL only
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A3BC10, Wm3::InBox<float>. A free function, so there is no
  // `this`: the point and the box are two stack pointers and the callee is
  // cdecl - it ends in `ADD ESP,0x0C; RET` and leaves the two arguments for
  // the caller to remove. The bool comes back in AL.
  __declspec(naked) u32 InBox(const float* point, const float* box)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      push dword ptr [esp + 24]      // our `box`
      push dword ptr [esp + 24]      // our `point`
      mov  eax, 0x00A3BC10
      call eax                       // cdecl: the arguments stay on the stack
      movzx eax, al                  // the bool lands in AL only
      add  esp, 8                    // ... and the caller removes them
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A48190, Wm3::DistVector3Segment3<float>::Get(fT,v0,v1) - the
  // class vtable's +0x08 slot. ECX = `this`, three stack arguments (fT,
  // rkVelocity0, rkVelocity1), `RET 0xC` - the callee pops them - and the
  // float in ST(0).
  __declspec(naked) void DistVector3Segment3Get(const void* self, float fT,
                                                 const float* vel0,
                                                 const float* vel1, float* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  edi, [esp + 36]           // our `out`
      push dword ptr [esp + 32]      // our `vel1`
      push dword ptr [esp + 32]      // our `vel0`
      push dword ptr [esp + 32]      // our `fT`, pushed as its raw bits
      mov  eax, 0x00A48190
      call eax                       // `RET 0xC` pops the three arguments
      fstp dword ptr [edi]           // ST(0) -> our `out`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A48340, Wm3::DistVector3Segment3<float>::GetSquared(fT,v0,v1) -
  // the class vtable's +0x14 slot. Same ABI as the swept Get: ECX = `this`,
  // three stack arguments, `RET 0xC`, the float in ST(0).
  __declspec(naked) void DistVector3Segment3GetSquared(const void* self, float fT,
                                                         const float* vel0,
                                                         const float* vel1, float* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  edi, [esp + 36]           // our `out`
      push dword ptr [esp + 32]      // our `vel1`
      push dword ptr [esp + 32]      // our `vel0`
      push dword ptr [esp + 32]      // our `fT`, pushed as its raw bits
      mov  eax, 0x00A48340
      call eax                       // `RET 0xC` pops the three arguments
      fstp dword ptr [edi]           // ST(0) -> our `out`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A3AE40, Wm3::ContAlignedBox<float>. It returns a Box3 by
  // value, so the hidden return pointer is the first stack argument, and it
  // is cdecl - it ends in a plain RET and leaves its arguments for the
  // caller to remove.
  //
  // It takes FOUR arguments: (Box3* out, int iQuantity, const Vector3*
  // akPoint, const unsigned char* akMask). The engine's own call site,
  // Moho::AxisAlignedBoundsFromBox at 0x004FE2D0, pushes `0` for the mask
  // (0x004FE2E0) before the other three, so the retail takes its no-mask
  // path. Without that fourth argument the slot held whatever the caller's
  // stack happened to carry, and a nonzero garbage pointer made the retail
  // walk its mask path instead - skipping every point whose mask byte was
  // zero - which is what made the retail's box cover fewer points than the
  // count it was given.
  __declspec(naked) void ContAlignedBox(void* out, int quantity,
                                        const float* points)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  eax, [esp + 20]           // our `out` - the hidden return pointer
      mov  ebx, [esp + 24]           // our `quantity`
      mov  ecx, [esp + 28]           // our `points`
      push 0                         // the retail's fourth argument: no mask
      push ecx
      push ebx
      push eax
      mov  eax, 0x00A3AE40
      call eax                       // cdecl: the arguments stay on the stack
      add  esp, 16                   // ... and the caller removes them
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A3AE40, Wm3::ContAlignedBox<float>, called with the mask.
  // Same ABI as the thunk above, except that the fourth stack argument - the
  // `abValid` mask - is the caller's pointer instead of a literal zero. The
  // retail reads that argument one byte at a time (0x00A3B30E:
  // cmp byte ptr [ecx+esi],0), so a nonzero byte means "keep this point" and
  // a null pointer selects the unmasked path.
  __declspec(naked) void ContAlignedBoxMask(void* out, int quantity,
                                            const float* points,
                                            const unsigned char* mask)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  eax, [esp + 20]           // our `out` - the hidden return pointer
      mov  ebx, [esp + 24]           // our `quantity`
      mov  ecx, [esp + 28]           // our `points`
      mov  edx, [esp + 32]           // our `mask`
      push edx                       // the retail's fourth argument: the mask
      push ecx
      push ebx
      push eax
      mov  eax, 0x00A3AE40
      call eax                       // cdecl: the arguments stay on the stack
      add  esp, 16                   // ... and the caller removes them
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A685F0, Wm3::ContAlignedBox<float> - the 2D one. It was found
  // by its shape, not by a call site: the engine never calls it, so it has no
  // code xref in the retail image. The body writes the 2D identity first -
  // the centre (0,0) from 0x00F3DBE0, the axis (1,0) from 0x00F3DBE8, the
  // axis (0,1) from 0x00F3DBF0 and the two unit extents from `FLD1` - and
  // only then computes the extremes over the points, exactly as the 3D one at
  // 0x00A3AE40 writes the 3D identity before its extremes.
  //
  // It returns a Box2 by value, so the hidden return pointer is the first
  // stack argument, and it is cdecl - it ends in a plain RET and leaves its
  // arguments for the caller to remove. The stack is (Box2* out,
  // int iQuantity, const Vector2* akPoint, const unsigned char* akMask):
  // four arguments, the fourth being the `abValid` mask, as in the 3D one.
  // The retail reads the mask a byte at a time (0x00A688B4:
  // cmp byte ptr [ecx+esi],0), so a nonzero byte means "keep this point".
  __declspec(naked) void ContAlignedBox2(void* out, int quantity,
                                         const float* points)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  eax, [esp + 20]           // our `out` - the hidden return pointer
      mov  ebx, [esp + 24]           // our `quantity`
      mov  ecx, [esp + 28]           // our `points`
      push 0                         // the retail's fourth argument: no mask
      push ecx
      push ebx
      push eax
      mov  eax, 0x00A685F0
      call eax                       // cdecl: the arguments stay on the stack
      add  esp, 16                   // ... and the caller removes them
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A685F0, Wm3::ContAlignedBox<float> - the 2D one, called with
  // the mask. Same ABI as the thunk above, except that the fourth stack
  // argument - the `abValid` mask - is the caller's pointer instead of a
  // literal zero.
  __declspec(naked) void ContAlignedBox2Mask(void* out, int quantity,
                                             const float* points,
                                             const unsigned char* mask)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  eax, [esp + 20]           // our `out` - the hidden return pointer
      mov  ebx, [esp + 24]           // our `quantity`
      mov  ecx, [esp + 28]           // our `points`
      mov  edx, [esp + 32]           // our `mask`
      push edx                       // the retail's fourth argument: the mask
      push ecx
      push ebx
      push eax
      mov  eax, 0x00A685F0
      call eax                       // cdecl: the arguments stay on the stack
      add  esp, 16                   // ... and the caller removes them
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::IntrBox2Circle2<float>::TestVertexRegion ---------------------
  //
  // Retail 0x00A4E6C0. A thiscall member function with eight stack arguments
  // - fCx, fCy, fVx, fVy, fEx, fEy, &rfIx, &rfIy - and the callee removes
  // them: the body ends in `RET 0x20`, eight dwords. ECX is `this`.
  //
  // The arguments are pushed here in reverse order, so that the first one
  // lands at the lowest address, which is where the retail's own caller
  // (Find) puts it. The retail pops all eight itself, so nothing is left to
  // clean up after the call.
  //
  // Our own arguments sit above the four saved registers, so the eighth of
  // ours is at [ESP + 0x34] once the registers are pushed. Every push moves
  // ESP down by four bytes and moves the next argument we want down by four
  // bytes as well, so the offset stays the same for all eight.
  __declspec(naked) int IntrBox2Circle2TestVertexRegion(void* self, float fCx,
      float fCy, float fVx, float fVy, float fEx, float fEy, float* pix,
      float* piy)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self`
      push dword ptr [esp + 0x34]    // our `piy`
      push dword ptr [esp + 0x34]    // our `pix`
      push dword ptr [esp + 0x34]    // our `fEy`
      push dword ptr [esp + 0x34]    // our `fEx`
      push dword ptr [esp + 0x34]    // our `fVy`
      push dword ptr [esp + 0x34]    // our `fVx`
      push dword ptr [esp + 0x34]    // our `fCy`
      push dword ptr [esp + 0x34]    // our `fCx`
      mov  eax, 0x00A4E6C0
      call eax                       // `RET 0x20` pops all eight
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::IntrBox2Circle2<float>::TestEdgeRegion -----------------------
  //
  // Retail 0x00A4EB60. The same shape as TestVertexRegion: eight stack
  // arguments, `RET 0x20`, ECX is `this`.
  __declspec(naked) int IntrBox2Circle2TestEdgeRegion(void* self, float fCx,
      float fCy, float fVx, float fVy, float fEx, float fEy, float* pix,
      float* piy)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self`
      push dword ptr [esp + 0x34]    // our `piy`
      push dword ptr [esp + 0x34]    // our `pix`
      push dword ptr [esp + 0x34]    // our `fEy`
      push dword ptr [esp + 0x34]    // our `fEx`
      push dword ptr [esp + 0x34]    // our `fVy`
      push dword ptr [esp + 0x34]    // our `fVx`
      push dword ptr [esp + 0x34]    // our `fCy`
      push dword ptr [esp + 0x34]    // our `fCx`
      mov  eax, 0x00A4EB60
      call eax                       // `RET 0x20` pops all eight
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- the swept Distance queries ----------------------------------------
  //
  // All six have the shape DistVector3Segment3's two swept rows already use
  // (0x00A48190 / 0x00A48340): `this` in ECX, three stack arguments - fT,
  // rkVelocity0, rkVelocity1 - and `RET 0xC`, so the callee pops them. The
  // float comes back in ST(0).
  //
  // The retail bodies are stack-probe-and-SEH-framed (they install their own
  // exception handler and probe the stack before touching it), and the
  // existing DistVector3Segment3 thunks already call into such a body
  // unchanged: the framing is the retail's business, the thunk only has to
  // hand over the arguments in the retail's order.
  //
  // The three arguments are pushed in the retail's order - fT last, so it
  // ends up at the lowest address - and each read stays at [esp + 32]:
  // pushing drops ESP by four while the argument being read stays put, so
  // the fixed offset walks the arguments down in step.
  __declspec(naked) void DistVector3Box3GetSwept(const void* self, float fT,
                                                  const float* vel0,
                                                  const float* vel1, float* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  edi, [esp + 36]           // our `out`
      push dword ptr [esp + 32]      // our `vel1`
      push dword ptr [esp + 32]      // our `vel0`
      push dword ptr [esp + 32]      // our `fT`, pushed as its raw bits
      mov  eax, 0x00A458C0
      call eax                       // `RET 0xC` pops the three arguments
      fstp dword ptr [edi]           // ST(0) -> our `out`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  __declspec(naked) void DistVector3Box3GetSquaredSwept(const void* self,
                                                         float fT,
                                                         const float* vel0,
                                                         const float* vel1,
                                                         float* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  edi, [esp + 36]
      push dword ptr [esp + 32]      // our `vel1`
      push dword ptr [esp + 32]      // our `vel0`
      push dword ptr [esp + 32]      // our `fT`
      mov  eax, 0x00A45A60
      call eax                       // `RET 0xC` pops the three arguments
      fstp dword ptr [edi]
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  __declspec(naked) void DistVector2Box2GetSwept(const void* self, float fT,
                                                  const float* vel0,
                                                  const float* vel1, float* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  edi, [esp + 36]
      push dword ptr [esp + 32]      // our `vel1`
      push dword ptr [esp + 32]      // our `vel0`
      push dword ptr [esp + 32]      // our `fT`
      mov  eax, 0x00A6C730
      call eax                       // `RET 0xC` pops the three arguments
      fstp dword ptr [edi]
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  __declspec(naked) void DistVector2Box2GetSquaredSwept(const void* self,
                                                         float fT,
                                                         const float* vel0,
                                                         const float* vel1,
                                                         float* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  edi, [esp + 36]
      push dword ptr [esp + 32]      // our `vel1`
      push dword ptr [esp + 32]      // our `vel0`
      push dword ptr [esp + 32]      // our `fT`
      mov  eax, 0x00A6C880
      call eax                       // `RET 0xC` pops the three arguments
      fstp dword ptr [edi]
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  __declspec(naked) void DistSegment3Segment3GetSwept(const void* self, float fT,
                                                       const float* vel0,
                                                       const float* vel1,
                                                       float* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  edi, [esp + 36]
      push dword ptr [esp + 32]      // our `vel1`
      push dword ptr [esp + 32]      // our `vel0`
      push dword ptr [esp + 32]      // our `fT`
      mov  eax, 0x00A7E2A0
      call eax                       // `RET 0xC` pops the three arguments
      fstp dword ptr [edi]
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  __declspec(naked) void DistSegment3Segment3GetSquaredSwept(const void* self,
                                                              float fT,
                                                              const float* vel0,
                                                              const float* vel1,
                                                              float* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  edi, [esp + 36]
      push dword ptr [esp + 32]      // our `vel1`
      push dword ptr [esp + 32]      // our `vel0`
      push dword ptr [esp + 32]      // our `fT`
      mov  eax, 0x00A7E480
      call eax                       // `RET 0xC` pops the three arguments
      fstp dword ptr [edi]
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::IntrLine3Box3<float>::DoClipping and ::Clip -------------------
  //
  // DoClipping is retail 0x00A50220, a static member: nine stack arguments
  // and no `this`. The arguments are pushed in the reverse of the retail's
  // own order, so that fT0 ends up at the lowest address; each read stays at
  // [esp + 36], because pushing drops ESP by four while the argument being
  // read stays put.
  //
  // The retail's body is cdecl - it ends in a plain `RET` and leaves the nine
  // arguments for the caller - so the caller removes the thirty-six bytes
  // here. The bool comes back in AL.
  //
  // No callee-saved register is touched here, and the retail body balances
  // its own EBX/ESI/EDI, so nothing has to be saved around the call.
  __declspec(naked) u32 IntrLine3Box3DoClipping(float fT0, float fT1,
                                                 const float* origin,
                                                 const float* direction,
                                                 const float* box, int solid,
                                                 int* quantity, float* points,
                                                 int* type)
  {
    __asm {
      push dword ptr [esp + 36]      // our `type`
      push dword ptr [esp + 36]      // our `points`
      push dword ptr [esp + 36]      // our `quantity`
      push dword ptr [esp + 36]      // our `solid`
      push dword ptr [esp + 36]      // our `box`
      push dword ptr [esp + 36]      // our `direction`
      push dword ptr [esp + 36]      // our `origin`
      push dword ptr [esp + 36]      // our `fT1`
      push dword ptr [esp + 36]      // our `fT0`
      mov  eax, 0x00A50220
      call eax
      add  esp, 36                   // cdecl: the callee leaves its nine arguments
      movzx eax, al                  // the bool lands in AL only
      ret
    }
  }

  // Clip is retail 0x00A4FCC0, a private static member: four stack arguments
  // - fDenom, fNumer, &rfT0, &rfT1 - and cdecl as well (the body ends in a
  // plain `RET`; the retail's own call site at 0x00A5032D does
  // `ADD ESP,0x10`). The bool comes back in AL.
  __declspec(naked) u32 IntrLine3Box3Clip(float denom, float numer, float* t0,
                                           float* t1)
  {
    __asm {
      push dword ptr [esp + 16]      // our `t1`
      push dword ptr [esp + 16]      // our `t0`
      push dword ptr [esp + 16]      // our `numer`
      push dword ptr [esp + 16]      // our `denom`
      mov  eax, 0x00A4FCC0
      call eax
      add  esp, 16                   // cdecl: the callee leaves its four arguments
      movzx eax, al                  // the bool lands in AL only
      ret
    }
  }

  // ---- the base class's window queries -----------------------------------
  //
  // Both are thiscall with four stack arguments - fTMin, fTMax,
  // &rkVelocity0, &rkVelocity1 - and `RET 0x10`, so the callee pops them.
  // The float comes back in ST(0).
  //
  // The four arguments are pushed in the retail's order - fTMin last, so it
  // ends up at the lowest address - and each read stays at [esp + 36]:
  // pushing drops ESP by four while the argument being read stays put, so
  // the fixed offset walks the arguments down in step.
  __declspec(naked) void DistVector3Segment3WindowGet(const void* self,
                                                       float fTMin, float fTMax,
                                                       const float* vel0,
                                                       const float* vel1,
                                                       float* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  edi, [esp + 40]           // our `out`
      push dword ptr [esp + 36]      // our `vel1`
      push dword ptr [esp + 36]      // our `vel0`
      push dword ptr [esp + 36]      // our `fTMax`
      push dword ptr [esp + 36]      // our `fTMin`
      mov  eax, 0x00A38C50
      call eax                       // `RET 0x10` pops the four arguments
      fstp dword ptr [edi]           // ST(0) -> our `out`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  __declspec(naked) void DistVector3Segment3WindowGetSquared(const void* self,
                                                             float fTMin,
                                                             float fTMax,
                                                             const float* vel0,
                                                             const float* vel1,
                                                             float* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  edi, [esp + 40]
      push dword ptr [esp + 36]      // our `vel1`
      push dword ptr [esp + 36]      // our `vel0`
      push dword ptr [esp + 36]      // our `fTMax`
      push dword ptr [esp + 36]      // our `fTMin`
      mov  eax, 0x00A38EB0
      call eax                       // `RET 0x10` pops the four arguments
      fstp dword ptr [edi]
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::Distance<float,Vector3f> --------------------------------------
  __declspec(naked) void DistanceCtor(void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A39F20
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  __declspec(naked) void DistanceSetDifferenceStep(void* self, float step)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self`
      push dword ptr [esp + 24]      // our `step`, pushed as its raw bits
      mov  eax, 0x00A391D0
      call eax                       // `RET 4` pops the argument
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  __declspec(naked) float DistanceGetDifferenceStep(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self`
      mov  eax, 0x00A39200
      call eax                       // the float comes back in ST(0)
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                            // ST(0) still holds the step
    }
  }

  // ---- Wm3::Distance<double,Vector3d> and ::Distance<double,Vector2d> -----
  //
  // The double instantiation of the base class.  The retail's bodies are x87
  // code and the engine runs under `_PC_24`, so the arithmetic is a 24-bit
  // significand - see the header for the addresses and the member offsets.

  // Retail 0x00A39FA0 - Wm3::Distance<double,Vector3d>'s base constructor.
  // ECX = `this`, no stack arguments, a plain `RET`.
  __declspec(naked) void Distance3dCtor(void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A39FA0
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A39F60 - Wm3::Distance<double,Vector2d>'s base constructor.
  __declspec(naked) void Distance2dCtor(void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A39F60
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A39DB0 - Wm3::Distance<double,Vector3d>::SetDifferenceStep.
  // ECX = `this`, one double argument on the stack; the callee pops it
  // (`RET 8`).  The argument is pushed as its two raw halves, low first.
  __declspec(naked) void Distance3dSetDifferenceStep(void* self, double step)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self`
      mov  edx, [esp + 24]           // our `step`, the low half
      mov  eax, [esp + 28]           // our `step`, the high half
      push eax
      push edx
      mov  eax, 0x00A39DB0
      call eax                       // `RET 8` pops the double for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A397C0 - Wm3::Distance<double,Vector2d>::SetDifferenceStep.
  __declspec(naked) void Distance2dSetDifferenceStep(void* self, double step)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  edx, [esp + 24]
      mov  eax, [esp + 28]
      push eax
      push edx
      mov  eax, 0x00A397C0
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A39DE0 - Wm3::Distance<double,Vector3d>::GetDifferenceStep.
  // ECX = `this`, no stack arguments; the double comes back in ST(0).
  __declspec(naked) double Distance3dGetDifferenceStep(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A39DE0
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                            // ST(0) still holds the step
    }
  }

  // Retail 0x00A397F0 - Wm3::Distance<double,Vector2d>::GetDifferenceStep.
  __declspec(naked) double Distance2dGetDifferenceStep(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A397F0
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- the base class's own four bodies, for the two double bases ---------
  //
  // The stack arguments are pushed by hand, because the retail takes the
  // doubles in the middle of the argument list.  Our own frame (after the
  // four register pushes) is
  //
  //   window   [esp+20] self  [esp+24] fTMin(8)  [esp+32] fTMax(8)
  //            [esp+40] vel0   [esp+44] vel1       [esp+48] out
  //   deriv    [esp+20] self  [esp+24] fT(8)      [esp+32] vel0
  //            [esp+36] vel1   [esp+40] out
  //
  // and the retail wants, at the call, fTMin(8), fTMax(8), &v0, &v1 (the
  // window) or fT(8), &v0, &v1 (the derivative).  Every argument happens to
  // sit at the same offset from the current stack pointer while it is being
  // pushed, because the pointer moves down by four bytes and so does the
  // argument.

  __declspec(naked) void Distance3dWindowGet(const void* self, double fTMin,
                                             double fTMax, const double* vel0,
                                             const double* vel1, double* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this`
      mov  edi, [esp + 48]           // our `out`
      push dword ptr [esp + 44]      // our `vel1`
      push dword ptr [esp + 44]      // our `vel0`
      mov  edx, [esp + 44]           // our `fTMax`, the high half
      mov  eax, [esp + 40]           // our `fTMax`, the low half
      push edx
      push eax
      mov  edx, [esp + 44]           // our `fTMin`, the high half
      mov  eax, [esp + 40]           // our `fTMin`, the low half
      push edx
      push eax
      mov  eax, 0x00A39860
      call eax                       // `RET 0x18` pops the 24 bytes
      fstp qword ptr [edi]           // ST(0) -> our `out`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  __declspec(naked) void Distance3dWindowGetSquared(const void* self,
                                                    double fTMin, double fTMax,
                                                    const double* vel0,
                                                    const double* vel1,
                                                    double* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  edi, [esp + 48]           // our `out`
      push dword ptr [esp + 44]      // our `vel1`
      push dword ptr [esp + 44]      // our `vel0`
      mov  edx, [esp + 44]           // our `fTMax`, the high half
      mov  eax, [esp + 40]           // our `fTMax`, the low half
      push edx
      push eax
      mov  edx, [esp + 44]           // our `fTMin`, the high half
      mov  eax, [esp + 40]           // our `fTMin`, the low half
      push edx
      push eax
      mov  eax, 0x00A39AB0
      call eax
      fstp qword ptr [edi]
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  __declspec(naked) void Distance3dGetDerivative(const void* self, double fT,
                                                  const double* vel0,
                                                  const double* vel1,
                                                  double* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  edi, [esp + 40]           // our `out`
      push dword ptr [esp + 36]      // our `vel1`
      push dword ptr [esp + 36]      // our `vel0`
      mov  edx, [esp + 36]           // our `fT`, the high half
      mov  eax, [esp + 32]           // our `fT`, the low half
      push edx
      push eax
      mov  eax, 0x00A39D00
      call eax                       // `RET 0x10` pops the 16 bytes
      fstp qword ptr [edi]
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  __declspec(naked) void Distance3dGetDerivativeSquared(const void* self,
                                                         double fT,
                                                         const double* vel0,
                                                         const double* vel1,
                                                         double* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  edi, [esp + 40]           // our `out`
      push dword ptr [esp + 36]      // our `vel1`
      push dword ptr [esp + 36]      // our `vel0`
      mov  edx, [esp + 36]           // our `fT`, the high half
      mov  eax, [esp + 32]           // our `fT`, the low half
      push edx
      push eax
      mov  eax, 0x00A39D60
      call eax
      fstp qword ptr [edi]
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  __declspec(naked) void Distance2dWindowGet(const void* self, double fTMin,
                                             double fTMax, const double* vel0,
                                             const double* vel1, double* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  edi, [esp + 48]           // our `out`
      push dword ptr [esp + 44]      // our `vel1`
      push dword ptr [esp + 44]      // our `vel0`
      mov  edx, [esp + 44]           // our `fTMax`, the high half
      mov  eax, [esp + 40]           // our `fTMax`, the low half
      push edx
      push eax
      mov  edx, [esp + 44]           // our `fTMin`, the high half
      mov  eax, [esp + 40]           // our `fTMin`, the low half
      push edx
      push eax
      mov  eax, 0x00A39270
      call eax
      fstp qword ptr [edi]
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  __declspec(naked) void Distance2dWindowGetSquared(const void* self,
                                                    double fTMin, double fTMax,
                                                    const double* vel0,
                                                    const double* vel1,
                                                    double* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  edi, [esp + 48]           // our `out`
      push dword ptr [esp + 44]      // our `vel1`
      push dword ptr [esp + 44]      // our `vel0`
      mov  edx, [esp + 44]           // our `fTMax`, the high half
      mov  eax, [esp + 40]           // our `fTMax`, the low half
      push edx
      push eax
      mov  edx, [esp + 44]           // our `fTMin`, the high half
      mov  eax, [esp + 40]           // our `fTMin`, the low half
      push edx
      push eax
      mov  eax, 0x00A394C0
      call eax
      fstp qword ptr [edi]
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  __declspec(naked) void Distance2dGetDerivative(const void* self, double fT,
                                                  const double* vel0,
                                                  const double* vel1,
                                                  double* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  edi, [esp + 40]           // our `out`
      push dword ptr [esp + 36]      // our `vel1`
      push dword ptr [esp + 36]      // our `vel0`
      mov  edx, [esp + 36]           // our `fT`, the high half
      mov  eax, [esp + 32]           // our `fT`, the low half
      push edx
      push eax
      mov  eax, 0x00A39710
      call eax
      fstp qword ptr [edi]
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  __declspec(naked) void Distance2dGetDerivativeSquared(const void* self,
                                                         double fT,
                                                         const double* vel0,
                                                         const double* vel1,
                                                         double* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  edi, [esp + 40]           // our `out`
      push dword ptr [esp + 36]      // our `vel1`
      push dword ptr [esp + 36]      // our `vel0`
      mov  edx, [esp + 36]           // our `fT`, the high half
      mov  eax, [esp + 32]           // our `fT`, the low half
      push edx
      push eax
      mov  eax, 0x00A39770
      call eax
      fstp qword ptr [edi]
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::DistVector3Box3<double> ---------------------------------------

  // Retail 0x00A45780 - the constructor. ECX = `this`, the vector and the
  // box pointers on the stack; the two references land at [this+0x68] and
  // [this+0x6C], and the class vtable 0x00D71DB4 is installed. `RET 8`.
  __declspec(naked) void DistVector3Box3dCtor(void* self, const double* vec,
                                             const double* box)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      push dword ptr [esp + 28]     // our `box`
      push dword ptr [esp + 28]     // our `vec`
      mov  eax, 0x00A45780
      call eax                       // `RET 8` pops both arguments for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A460D0 - the static GetSquared, the class vtable's +0x18
  // slot. ECX = `this`, no stack arguments, the double in ST(0).
  __declspec(naked) double DistVector3Box3dGetSquared(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A460D0
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                            // ST(0) still holds the squared distance
    }
  }

  // Retail 0x00A45800 - the static Get, the class vtable's +0x0C slot: it
  // dispatches the +0x18 slot and jumps to the CRT's square root.
  __declspec(naked) double DistVector3Box3dGet(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A45800
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                            // ST(0) still holds the distance
    }
  }

  // Retail 0x00A45E90 - the swept Get, the class vtable's +0x08 slot. ECX =
  // `this`, three stack arguments - the double fT and the two velocity
  // pointers - and `RET 0x10`.
  __declspec(naked) void DistVector3Box3dGetSwept(const void* self, double fT,
                                                  const double* vel0,
                                                  const double* vel1,
                                                  double* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self`
      mov  edi, [esp + 40]           // our `out`
      push dword ptr [esp + 36]      // our `vel1`
      push dword ptr [esp + 36]      // our `vel0`
      push dword ptr [esp + 36]      // our `fT`, the high half
      push dword ptr [esp + 36]      // our `fT`, the low half
      mov  eax, 0x00A45E90
      call eax                       // `RET 0x10` pops the three arguments
      fstp qword ptr [edi]           // the distance -> our `out`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A45FB0 - the swept GetSquared, the class vtable's +0x14 slot.
  __declspec(naked) void DistVector3Box3dGetSquaredSwept(const void* self,
                                                         double fT,
                                                         const double* vel0,
                                                         const double* vel1,
                                                         double* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self`
      mov  edi, [esp + 40]           // our `out`
      push dword ptr [esp + 36]      // our `vel1`
      push dword ptr [esp + 36]      // our `vel0`
      push dword ptr [esp + 36]      // our `fT`, the high half
      push dword ptr [esp + 36]      // our `fT`, the low half
      mov  eax, 0x00A45FB0
      call eax                       // `RET 0x10` pops the three arguments
      fstp qword ptr [edi]           // the squared distance -> our `out`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::DistVector3Segment3<double> ------------------------------------

  // Retail 0x00A48080 - the constructor. ECX = `this`, the vector and the
  // segment pointers on the stack; the two references land at [this+0x68] and
  // [this+0x6C], and the class vtable 0x00D71E94 is installed. `RET 8`.
  __declspec(naked) void DistVector3Segment3dCtor(void* self, const double* vec,
                                                  const double* segment)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      push dword ptr [esp + 28]     // our `segment`
      push dword ptr [esp + 28]     // our `vec`
      mov  eax, 0x00A48080
      call eax                       // `RET 8` pops both arguments for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A48910 - the static GetSquared, the class vtable's +0x18
  // slot. ECX = `this`, no stack arguments, the double in ST(0).
  __declspec(naked) double DistVector3Segment3dGetSquared(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A48910
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                            // ST(0) still holds the squared distance
    }
  }

  // Retail 0x00A48100 - the static Get, the class vtable's +0x0C slot.
  __declspec(naked) double DistVector3Segment3dGet(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A48100
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                            // ST(0) still holds the distance
    }
  }

  // Retail 0x00A486B0 - the swept Get, the class vtable's +0x08 slot. ECX =
  // `this`, three stack arguments - the double fT and the two velocity
  // pointers - and `RET 0x10`.  The double comes back in ST(0).
  //
  // The argument offsets below are the caller's: with the four registers
  // pushed, [esp+20] is our `self`, [esp+24]/[esp+28] are the two halves of
  // our `fT`, [esp+32]/[esp+36] are the two velocity pointers and [esp+40]
  // is our `out`.  Each push shifts the frame, so the offsets in the push
  // sequence are read before the push they belong to.
  __declspec(naked) void DistVector3Segment3dGetSwept(const void* self, double fT,
                                                      const double* vel0,
                                                      const double* vel1,
                                                      double* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self`
      mov  edi, [esp + 40]           // our `out`
      push dword ptr [esp + 36]      // our `vel1`
      push dword ptr [esp + 36]      // our `vel0`
      push dword ptr [esp + 36]      // our `fT`, the high half
      push dword ptr [esp + 36]      // our `fT`, the low half
      mov  eax, 0x00A486B0
      call eax                       // `RET 0x10` pops the three arguments
      fstp qword ptr [edi]           // the distance -> our `out`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A487E0 - the swept GetSquared, the class vtable's +0x14 slot.
  __declspec(naked) void DistVector3Segment3dGetSquaredSwept(const void* self,
                                                             double fT,
                                                             const double* vel0,
                                                             const double* vel1,
                                                             double* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self`
      mov  edi, [esp + 40]           // our `out`
      push dword ptr [esp + 36]      // our `vel1`
      push dword ptr [esp + 36]      // our `vel0`
      push dword ptr [esp + 36]      // our `fT`, the high half
      push dword ptr [esp + 36]      // our `fT`, the low half
      mov  eax, 0x00A487E0
      call eax                       // `RET 0x10` pops the three arguments
      fstp qword ptr [edi]           // the squared distance -> our `out`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::DistVector2Box2<double> --------------------------------------

  // Retail 0x00A6C630 - the double instantiation's constructor. ECX = `this`,
  // the vector and the box pointers on the stack, stored at [this+0x58] and
  // [this+0x5C]; `RET 8` pops both arguments for us.
  __declspec(naked) void DistVector2Box2dCtor(void* self, const double* vec,
                                              const double* box)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      push dword ptr [esp + 28]      // our `box`
      push dword ptr [esp + 28]      // our `vec`
      mov  eax, 0x00A6C630
      call eax                       // `RET 8` pops both arguments for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A6CD70 - the static GetSquared, the class vtable's +0x18
  // slot. ECX = `this`, no stack arguments, the double in ST(0).
  __declspec(naked) double DistVector2Box2dGetSquared(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A6CD70
      call eax
      fstp qword ptr [esp + 24]      // the squared distance
      fld qword ptr [esp + 24]
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A6C6A0 - the static Get, the class vtable's +0x0C slot. It
  // dispatches the +0x18 slot and jumps to the CRT's square root at
  // 0x00A91000, so the double comes back in ST(0).
  __declspec(naked) double DistVector2Box2dGet(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A6C6A0
      call eax
      fstp qword ptr [esp + 24]      // the distance
      fld qword ptr [esp + 24]
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A6CB50 - the swept Get, the class vtable's +0x08 slot. ECX =
  // `this`, three stack arguments - the double fT and the two velocity
  // pointers - and `RET 0x10`; the double comes back in ST(0).
  __declspec(naked) void DistVector2Box2dGetSwept(const void* self, double fT,
                                                  const double* vel0,
                                                  const double* vel1,
                                                  double* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self`
      mov  edi, [esp + 40]           // our `out`
      push dword ptr [esp + 36]      // our `vel1`
      push dword ptr [esp + 36]      // our `vel0`
      push dword ptr [esp + 36]      // our `fT`, the high half
      push dword ptr [esp + 36]      // our `fT`, the low half
      mov  eax, 0x00A6CB50
      call eax                       // `RET 0x10` pops the three arguments
      fstp qword ptr [edi]           // the distance -> our `out`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A6CC60 - the swept GetSquared, the class vtable's +0x14 slot.
  // Same ABI as the swept Get.
  __declspec(naked) void DistVector2Box2dGetSquaredSwept(const void* self,
                                                         double fT,
                                                         const double* vel0,
                                                         const double* vel1,
                                                         double* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self`
      mov  edi, [esp + 40]           // our `out`
      push dword ptr [esp + 36]      // our `vel1`
      push dword ptr [esp + 36]      // our `vel0`
      push dword ptr [esp + 36]      // our `fT`, the high half
      push dword ptr [esp + 36]      // our `fT`, the low half
      mov  eax, 0x00A6CC60
      call eax                       // `RET 0x10` pops the three arguments
      fstp qword ptr [edi]           // the squared distance -> our `out`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::DistSegment3Segment3<double> ----------------------------------

  // Retail 0x00A7E1E0 - the double instantiation's constructor. ECX = `this`,
  // the two segment pointers on the stack, stored at [this+0x68] and
  // [this+0x6C]; `RET 8` pops both arguments for us.
  __declspec(naked) void DistSegment3Segment3dCtor(void* self, const double* seg0,
                                                   const double* seg1)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      push dword ptr [esp + 28]      // our `seg1`
      push dword ptr [esp + 28]      // our `seg0`
      mov  eax, 0x00A7E1E0
      call eax                       // `RET 8` pops both arguments for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A7F100 - the static GetSquared, the class vtable's +0x18
  // slot. ECX = `this`, no stack arguments, the double in ST(0).
  __declspec(naked) double DistSegment3Segment3dGetSquared(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A7F100
      call eax
      fstp qword ptr [esp + 24]      // the squared distance
      fld qword ptr [esp + 24]
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A7E250 - the static Get, the class vtable's +0x0C slot. It
  // dispatches the +0x18 slot and jumps to the CRT's square root at
  // 0x00A91000, so the double comes back in ST(0).
  __declspec(naked) double DistSegment3Segment3dGet(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A7E250
      call eax
      fstp qword ptr [esp + 24]      // the distance
      fld qword ptr [esp + 24]
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A7EE60 - the swept Get, the class vtable's +0x08 slot. ECX =
  // `this`, three stack arguments - the double fT and the two velocity
  // pointers - and `RET 0x10`; the double comes back in ST(0).
  __declspec(naked) void DistSegment3Segment3dGetSwept(const void* self,
                                                       double fT,
                                                       const double* vel0,
                                                       const double* vel1,
                                                       double* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self`
      mov  edi, [esp + 40]           // our `out`
      push dword ptr [esp + 36]      // our `vel1`
      push dword ptr [esp + 36]      // our `vel0`
      push dword ptr [esp + 36]      // our `fT`, the high half
      push dword ptr [esp + 36]      // our `fT`, the low half
      mov  eax, 0x00A7EE60
      call eax                       // `RET 0x10` pops the three arguments
      fstp qword ptr [edi]           // the distance -> our `out`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A7EFB0 - the swept GetSquared, the class vtable's +0x14 slot.
  // Same ABI as the swept Get.
  __declspec(naked) void DistSegment3Segment3dGetSquaredSwept(const void* self,
                                                              double fT,
                                                              const double* vel0,
                                                              const double* vel1,
                                                              double* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self`
      mov  edi, [esp + 40]           // our `out`
      push dword ptr [esp + 36]      // our `vel1`
      push dword ptr [esp + 36]      // our `vel0`
      push dword ptr [esp + 36]      // our `fT`, the high half
      push dword ptr [esp + 36]      // our `fT`, the low half
      mov  eax, 0x00A7EFB0
      call eax                       // `RET 0x10` pops the three arguments
      fstp qword ptr [edi]           // the squared distance -> our `out`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::DistLine3Segment3<double> -------------------------------------

  // Retail 0x00A80950 - the double instantiation's constructor. ECX = `this`,
  // the line and the segment pointers on the stack, stored at [this+0x68] and
  // [this+0x6C]; `RET 8` pops both arguments for us.
  __declspec(naked) void DistLine3Segment3dCtor(void* self, const double* line,
                                                const double* segment)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      push dword ptr [esp + 28]      // our `segment`
      push dword ptr [esp + 28]      // our `line`
      mov  eax, 0x00A80950
      call eax                       // `RET 8` pops both arguments for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A81330 - the static GetSquared, the class vtable's +0x18
  // slot. ECX = `this`, no stack arguments, the double in ST(0).
  __declspec(naked) double DistLine3Segment3dGetSquared(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A81330
      call eax
      fstp qword ptr [esp + 24]      // the squared distance
      fld qword ptr [esp + 24]
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A809C0 - the static Get, the class vtable's +0x0C slot. It
  // dispatches the +0x18 slot and jumps to the CRT's square root at
  // 0x00A91000, so the double comes back in ST(0).
  __declspec(naked) double DistLine3Segment3dGet(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A809C0
      call eax
      fstp qword ptr [esp + 24]      // the distance
      fld qword ptr [esp + 24]
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A810B0 - the swept Get, the class vtable's +0x08 slot. ECX =
  // `this`, three stack arguments - the double fT and the two velocity
  // pointers - and `RET 0x10`; the double comes back in ST(0).
  __declspec(naked) void DistLine3Segment3dGetSwept(const void* self,
                                                    double fT,
                                                    const double* vel0,
                                                    const double* vel1,
                                                    double* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self`
      mov  edi, [esp + 40]           // our `out`
      push dword ptr [esp + 36]      // our `vel1`
      push dword ptr [esp + 36]      // our `vel0`
      push dword ptr [esp + 36]      // our `fT`, the high half
      push dword ptr [esp + 36]      // our `fT`, the low half
      mov  eax, 0x00A810B0
      call eax                       // `RET 0x10` pops the three arguments
      fstp qword ptr [edi]           // the distance -> our `out`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A811F0 - the swept GetSquared, the class vtable's +0x14 slot.
  // Same ABI as the swept Get.
  __declspec(naked) void DistLine3Segment3dGetSquaredSwept(const void* self,
                                                           double fT,
                                                           const double* vel0,
                                                           const double* vel1,
                                                           double* out)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self`
      mov  edi, [esp + 40]           // our `out`
      push dword ptr [esp + 36]      // our `vel1`
      push dword ptr [esp + 36]      // our `vel0`
      push dword ptr [esp + 36]      // our `fT`, the high half
      push dword ptr [esp + 36]      // our `fT`, the low half
      mov  eax, 0x00A811F0
      call eax                       // `RET 0x10` pops the three arguments
      fstp qword ptr [edi]           // the squared distance -> our `out`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::IntrBox2Circle2<double> ---------------------------------------

  // Retail 0x00A4EE20 - the double instantiation's constructor. ECX = `this`,
  // the box and the circle pointers on the stack, stored at [this+0x18] and
  // [this+0x1C]; `RET 8` pops both arguments for us.
  __declspec(naked) void IntrBox2Circle2dCtor(void* self, const double* box,
                                               const double* circle)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      push dword ptr [esp + 28]      // our `circle`
      push dword ptr [esp + 28]      // our `box`
      mov  eax, 0x00A4EE20
      call eax                       // `RET 8` pops both arguments for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A4F960 - the class vtable's +0x0C slot, the Test. ECX = `this`,
  // no stack arguments, the bool in EAX.
  __declspec(naked) u32 IntrBox2Circle2dTest(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A4EE50
      call eax
      movzx eax, al              // the retail sets AL only; the bool is in AL
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::IntrSegment3Capsule3<double> -----------------------------------

  // Retail 0x00A6BF50 - the double instantiation's constructor. ECX = `this`,
  // the segment and the capsule pointers on the stack; `RET 8` pops both
  // arguments for us.
  __declspec(naked) void IntrSegment3Capsule3dCtor(void* self,
                                                  const double* segment,
                                                  const double* capsule)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      push dword ptr [esp + 28]      // our `capsule`
      push dword ptr [esp + 28]      // our `segment`
      mov  eax, 0x00A6BF50
      call eax                       // `RET 8` pops both arguments for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A6BF80 - the class vtable's +0x08 slot, the Test. ECX = `this`,
  // no stack arguments, the bool in EAX.
  __declspec(naked) u32 IntrSegment3Capsule3dTest(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A6BF80
      call eax
      movzx eax, al              // the retail sets AL only; the bool is in AL
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A6C3C0 - the class vtable's +0x0C slot, the Find. ECX = `this`,
  // no stack arguments, the bool in AL.
  __declspec(naked) u32 IntrSegment3Capsule3dFind(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A6C3C0
      call eax
      movzx eax, al              // the retail sets AL only; the bool is in AL
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::IntrBox3Sphere3<double> ---------------------------------------

  // Retail 0x00A42590 - the double instantiation's constructor. ECX = `this`,
  // the box and the sphere pointers on the stack; `RET 8` pops both arguments
  // for us.
  __declspec(naked) void IntrBox3Sphere3dCtor(void* self, const double* box,
                                              const double* sphere)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      push dword ptr [esp + 28]      // our `sphere`
      push dword ptr [esp + 28]      // our `box`
      mov  eax, 0x00A42590
      call eax                       // `RET 8` pops both arguments for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A425C0 - the class vtable's +0x08 slot, the Test. ECX = `this`,
  // no stack arguments, the bool in EAX.
  __declspec(naked) u32 IntrBox3Sphere3dTest(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A425C0
      call eax
      movzx eax, al              // the retail sets AL only; the bool is in AL
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::IntrBox2Box2<double> ----------------------------------------------

  // Retail 0x00A47C40 - the double instantiation's constructor. ECX = `this`, the
  // shapes on the stack.
  __declspec(naked) void IntrBox2Box2dCtor(void* self, const double* a, const double* b)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      push dword ptr [esp + 28]      // our `b`
      push dword ptr [esp + 28]      // our `a`
      mov  eax, 0x00A47C40
      call eax                       // `RET 8` pops both arguments for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A47F00 - the class vtable's +0x08 slot, the Test. ECX = `this`, no
  // stack arguments, the bool in AL - the retail sets AL only, so the upper
  // bits of EAX are left as they were and have to be masked off.
  __declspec(naked) u32 IntrBox2Box2dTest(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A47F00
      call eax
      movzx eax, al
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::IntrLine3Box3<double> ----------------------------------------------

  // Retail 0x00A50060 - the double instantiation's constructor. ECX = `this`, the
  // shapes on the stack.
  __declspec(naked) void IntrLine3Box3dCtor(void* self, const double* a, const double* b)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      push dword ptr [esp + 28]      // our `b`
      push dword ptr [esp + 28]      // our `a`
      mov  eax, 0x00A50060
      call eax                       // `RET 8` pops both arguments for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A50090 - the class vtable's +0x08 slot, the Test. ECX = `this`, no
  // stack arguments, the bool in AL - the retail sets AL only, so the upper
  // bits of EAX are left as they were and have to be masked off.
  __declspec(naked) u32 IntrLine3Box3dTest(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A50090
      call eax
      movzx eax, al
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A50940 - the class vtable's +0x0C slot, the Find. ECX = `this`,
  // no stack arguments, the bool in AL - the retail sets AL only, so the upper
  // bits of EAX are left as they were and have to be masked off.
  __declspec(naked) u32 IntrLine3Box3dFind(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A50940
      call eax
      movzx eax, al
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::IntrSegment3Box3<double> ----------------------------------------------

  // Retail 0x00A463D0 - the double instantiation's constructor. ECX = `this`, the
  // shapes on the stack.
  __declspec(naked) void IntrSegment3Box3dCtor(void* self, const double* a, const double* b, u32 flag)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      push dword ptr [esp + 32]      // our `flag`
      push dword ptr [esp + 32]      // our `b`
      push dword ptr [esp + 32]      // our `a`
      mov  eax, 0x00A463D0
      call eax                       // `RET 12` pops the three arguments for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A46720 - the class vtable's +0x08 slot, the Test. ECX = `this`, no
  // stack arguments, the bool in AL - the retail sets AL only, so the upper
  // bits of EAX are left as they were and have to be masked off.
  __declspec(naked) u32 IntrSegment3Box3dTest(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A46720
      call eax
      movzx eax, al
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A46330 - the class vtable's +0x0C slot, the Find. ECX = `this`,
  // no stack arguments, the bool in AL - the retail sets AL only, so the upper
  // bits of EAX are left as they were and have to be masked off.
  __declspec(naked) u32 IntrSegment3Box3dFind(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A46330
      call eax
      movzx eax, al
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::IntrSegment3Sphere3<double> ----------------------------------------------

  // Retail 0x00A46A50 - the double instantiation's constructor. ECX = `this`, the
  // shapes on the stack.
  __declspec(naked) void IntrSegment3Sphere3dCtor(void* self, const double* a, const double* b)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      push dword ptr [esp + 28]      // our `b`
      push dword ptr [esp + 28]      // our `a`
      mov  eax, 0x00A46A50
      call eax                       // `RET 8` pops both arguments for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A475E0 - the class vtable's +0x08 slot, the Test. ECX = `this`, no
  // stack arguments, the bool in AL - the retail sets AL only, so the upper
  // bits of EAX are left as they were and have to be masked off.
  __declspec(naked) u32 IntrSegment3Sphere3dTest(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A475E0
      call eax
      movzx eax, al
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A479B0 - the class vtable's +0x0C slot, the static Find. ECX =
  // `this`, no stack arguments, the bool in AL - the retail sets AL only, so
  // the upper bits of EAX are left as they were and have to be masked off.
  __declspec(naked) u32 IntrSegment3Sphere3dFind(const void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A479B0
      call eax
      movzx eax, al
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A47690 - the dynamic Find, the class vtable's +0x0C slot.
  // ECX = `this`, three stack arguments - the time limit (a 64-bit double
  // at [ESP+0x04] on entry) and the two velocity pointers (at [ESP+0x0C]
  // and [ESP+0x10]) - and `RET 0x10`, so the callee pops all sixteen bytes.
  // The pushes below put the time limit's low half at the lowest address,
  // then its high half, then velocity0 and then velocity1, which is the
  // order the callee reads.
  __declspec(naked) u32 IntrSegment3Sphere3dFindDynamic(void* self, double fTMax,
                                                         const double* vel0,
                                                         const double* vel1)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      push dword ptr [esp + 36]      // our `vel1`
      push dword ptr [esp + 36]      // our `vel0`
      push dword ptr [esp + 36]      // our `fTMax`, the high half
      push dword ptr [esp + 36]      // our `fTMax`, the low half
      mov  eax, 0x00A47690
      call eax                       // `RET 0x10` pops all three arguments
      movzx eax, al                 // the bool lands in AL only
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::ConvexHull1<float> and Wm3::ConvexHull2<float> ----------------

  // Retail 0x00A74560, Wm3::ConvexHull1<float>'s constructor. ECX = `this`,
  // five stack arguments (iVertexQuantity, afVertex, fEpsilon, bOwner,
  // eQueryType) and `RET 0x14`.
  __declspec(naked) void ConvexHull1Ctor(void* self, int n, const float* values,
                                         float eps, u32 owner, u32 type)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this`
      mov  ebx, [esp + 24]           // `n`
      mov  esi, [esp + 28]           // `values`
      mov  edi, [esp + 32]           // `eps` (the raw bits)
      mov  ebp, [esp + 36]           // `owner`
      push dword ptr [esp + 40]      // `type`
      push ebp                       // `owner`
      push edi                       // `eps`
      push esi                       // `values`
      push ebx                       // `n`
      mov  eax, 0x00A74560
      call eax                       // `RET 0x14`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A71EA0, Wm3::ConvexHull1<float>'s destructor (the class's own,
  // not the vtable's scalar deleting destructor at 0x00A71F20). ECX = `this`,
  // no stack arguments, a plain `RET`.
  __declspec(naked) void ConvexHull1Dtor(void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A71EA0
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A78E30, Wm3::ConvexHull2<float>'s constructor. ECX = `this`,
  // five stack arguments (iVertexQuantity, akVertex, fEpsilon, bOwner,
  // eQueryType) and `RET 0x14`.
  __declspec(naked) void ConvexHull2Ctor(void* self, int n, const float* verts,
                                         float eps, u32 owner, u32 type)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this`
      mov  ebx, [esp + 24]           // `n`
      mov  esi, [esp + 28]           // `verts`
      mov  edi, [esp + 32]           // `eps` (the raw bits)
      mov  ebp, [esp + 36]           // `owner`
      push dword ptr [esp + 40]      // `type`
      push ebp                       // `owner`
      push edi                       // `eps`
      push esi                       // `verts`
      push ebx                       // `n`
      mov  eax, 0x00A78E30
      call eax                       // `RET 0x14`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A74970, Wm3::ConvexHull2<float>'s destructor (the class's own,
  // not the vtable's scalar deleting destructor at 0x00A74DD0). ECX = `this`,
  // no stack arguments, a plain `RET`.
  __declspec(naked) void ConvexHull2Dtor(void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A74970
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::Query3<float> -------------------------------------------------
  //
  // The retail builds this class inline inside its ConvexHull3<float>
  // constructor (0x00A6743B), so there is no constructor to call: the object
  // carries the vtable pointer at [this+0x00], the vertex count at [this+0x04]
  // and the vertex array at [this+0x08].
  //
  // Every thunk below is thiscall: ECX = `this`, the arguments on the stack in
  // the order the retail's own forwarders use, and the callee pops them.

  // Retail 0x00A530A0, the Query3<float> class vtable 0x00D71F38's +0x0C slot:
  // ToPlane(int i, int iV0, int iV1, int iV2). The body computes
  // &m_akVertex[i] and forwards to the +0x08 slot, so it is reached by
  // address like every other one. `RET 0x10`, the sign in EAX.
  __declspec(naked) int Query3ToPlaneIndex(const void* self, int i, int iV0,
                                           int iV1, int iV2)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this`
      push dword ptr [esp + 36]      // `iV2`
      push dword ptr [esp + 36]      // `iV1`
      push dword ptr [esp + 36]      // `iV0`
      push dword ptr [esp + 36]      // `i`
      mov  eax, 0x00A530A0
      call eax                       // `RET 0x10`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A530D0, the +0x10 slot:
  // ToTetrahedron(const Vector3<float>&, int iV0, int iV1, int iV2, int iV3).
  // The body calls the +0x08 slot four times and combines the four signs.
  // `RET 0x14`, the sign in EAX.
  __declspec(naked) int Query3ToTetrahedron(const void* self, const float* point,
                                            int iV0, int iV1, int iV2, int iV3)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this`
      push dword ptr [esp + 40]      // `iV3`
      push dword ptr [esp + 40]      // `iV2`
      push dword ptr [esp + 40]      // `iV1`
      push dword ptr [esp + 40]      // `iV0`
      push dword ptr [esp + 40]      // `point`
      mov  eax, 0x00A530D0
      call eax                       // `RET 0x14`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A53180, the +0x14 slot:
  // ToTetrahedron(int i, int iV0, int iV1, int iV2, int iV3). `RET 0x14`.
  __declspec(naked) int Query3ToTetrahedronIndex(const void* self, int i,
                                                 int iV0, int iV1, int iV2,
                                                 int iV3)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this`
      push dword ptr [esp + 40]      // `iV3`
      push dword ptr [esp + 40]      // `iV2`
      push dword ptr [esp + 40]      // `iV1`
      push dword ptr [esp + 40]      // `iV0`
      push dword ptr [esp + 40]      // `i`
      mov  eax, 0x00A53180
      call eax                       // `RET 0x14`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A55DF0, the +0x18 slot:
  // ToCircumsphere(const Vector3<float>&, int iV0, int iV1, int iV2, int iV3).
  // `RET 0x14`, the sign in EAX.
  __declspec(naked) int Query3ToCircumsphere(const void* self, const float* point,
                                             int iV0, int iV1, int iV2,
                                             int iV3)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this`
      push dword ptr [esp + 40]      // `iV3`
      push dword ptr [esp + 40]      // `iV2`
      push dword ptr [esp + 40]      // `iV1`
      push dword ptr [esp + 40]      // `iV0`
      push dword ptr [esp + 40]      // `point`
      mov  eax, 0x00A55DF0
      call eax                       // `RET 0x14`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A531B0, the +0x1C slot:
  // ToCircumsphere(int i, int iV0, int iV1, int iV2, int iV3). `RET 0x14`.
  __declspec(naked) int Query3ToCircumsphereIndex(const void* self, int i,
                                                   int iV0, int iV1, int iV2,
                                                   int iV3)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this`
      push dword ptr [esp + 40]      // `iV3`
      push dword ptr [esp + 40]      // `iV2`
      push dword ptr [esp + 40]      // `iV1`
      push dword ptr [esp + 40]      // `iV0`
      push dword ptr [esp + 40]      // `i`
      mov  eax, 0x00A531B0
      call eax                       // `RET 0x14`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::Query2<float> -------------------------------------------------

  // Retail 0x00A75C00, the Query2<float> class vtable 0x00D72134's +0x08
  // slot: ToLine(const Vector2<float>&, int iV0, int iV1). `RET 0x0C`, the
  // sign in EAX.
  __declspec(naked) int Query2ToLine(const void* self, const float* point,
                                     int iV0, int iV1)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this`
      push dword ptr [esp + 32]      // `iV1`
      push dword ptr [esp + 32]      // `iV0`
      push dword ptr [esp + 32]      // `point`
      mov  eax, 0x00A75C00
      call eax                       // `RET 0x0C`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A74E70, the +0x0C slot: ToLine(int i, int iV0, int iV1).
  // `RET 0x0C`.
  __declspec(naked) int Query2ToLineIndex(const void* self, int i, int iV0,
                                         int iV1)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this`
      push dword ptr [esp + 32]      // `iV1`
      push dword ptr [esp + 32]      // `iV0`
      push dword ptr [esp + 32]      // `i`
      mov  eax, 0x00A74E70
      call eax                       // `RET 0x0C`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A74EA0, the +0x10 slot:
  // ToTriangle(const Vector2<float>&, int iV0, int iV1, int iV2). `RET 0x10`.
  __declspec(naked) int Query2ToTriangle(const void* self, const float* point,
                                         int iV0, int iV1, int iV2)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this`
      push dword ptr [esp + 36]      // `iV2`
      push dword ptr [esp + 36]      // `iV1`
      push dword ptr [esp + 36]      // `iV0`
      push dword ptr [esp + 36]      // `point`
      mov  eax, 0x00A74EA0
      call eax                       // `RET 0x10`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A74F30, the +0x14 slot:
  // ToTriangle(int i, int iV0, int iV1, int iV2). `RET 0x10`.
  __declspec(naked) int Query2ToTriangleIndex(const void* self, int i,
                                             int iV0, int iV1, int iV2)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this`
      push dword ptr [esp + 36]      // `iV2`
      push dword ptr [esp + 36]      // `iV1`
      push dword ptr [esp + 36]      // `iV0`
      push dword ptr [esp + 36]      // `i`
      mov  eax, 0x00A74F30
      call eax                       // `RET 0x10`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A75C80, the +0x18 slot:
  // ToCircumcircle(const Vector2<float>&, int iV0, int iV1, int iV2).
  // `RET 0x10`.
  __declspec(naked) int Query2ToCircumcircle(const void* self, const float* point,
                                             int iV0, int iV1, int iV2)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this`
      push dword ptr [esp + 36]      // `iV2`
      push dword ptr [esp + 36]      // `iV1`
      push dword ptr [esp + 36]      // `iV0`
      push dword ptr [esp + 36]      // `point`
      mov  eax, 0x00A75C80
      call eax                       // `RET 0x10`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A74F60, the +0x1C slot:
  // ToCircumcircle(int i, int iV0, int iV1, int iV2). `RET 0x10`.
  __declspec(naked) int Query2ToCircumcircleIndex(const void* self, int i,
                                                   int iV0, int iV1, int iV2)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this`
      push dword ptr [esp + 36]      // `iV2`
      push dword ptr [esp + 36]      // `iV1`
      push dword ptr [esp + 36]      // `iV0`
      push dword ptr [esp + 36]      // `i`
      mov  eax, 0x00A74F60
      call eax                       // `RET 0x10`
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Vector3<Real>::GenerateOrthonormalBasis ----------------------------
  //
  // The retail's double body is 0x00A3D150 and its float body is 0x00A3CF80.
  // Both are cdecl statics with four stack arguments - &rkU, &rkV, &rkW and
  // the bUnitLengthW flag - and both end in a plain `RET`, so the caller
  // pops the four arguments.  The two output vectors are written through
  // the first two pointers; the thunks point them at the first and the
  // second half of the caller's output array, so the retail's U and V land
  // in it directly.
  //
  // The retail normalises W in place when the flag is false, so it gets a
  // copy of the caller's vector.
  void Vector3GenerateOrthonormalBasisd(const double* w, u32 unitLengthW,
                                        double* out)
  {
    double kW[3];
    kW[0] = w[0];
    kW[1] = w[1];
    kW[2] = w[2];
    const double* pw = kW;
    double* pu = out;
    double* pv = out + 3;
    __asm {
      push dword ptr unitLengthW     // the flag is the last argument, so it
      push dword ptr pw              // is pushed first
      push dword ptr pv
      push dword ptr pu
      mov  eax, 0x00A3D150
      call eax
      add  esp, 16                   // cdecl: the callee leaves its arguments
    }
  }

  void Vector3GenerateOrthonormalBasisf(const float* w, u32 unitLengthW,
                                        float* out)
  {
    float kW[3];
    kW[0] = w[0];
    kW[1] = w[1];
    kW[2] = w[2];
    const float* pw = kW;
    float* pu = out;
    float* pv = out + 3;
    __asm {
      push dword ptr unitLengthW
      push dword ptr pw
      push dword ptr pv
      push dword ptr pu
      mov  eax, 0x00A3CF80
      call eax
      add  esp, 16
    }
  }

  // Retail 0x00A3D0A0 - the double Vector3<double>::Normalize. `this` in ECX,
  // no stack arguments, plain RET, and the length in ST(0) at the ret.
  __declspec(naked) double Vector3NormalizeD(void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      mov  eax, 0x00A3D0A0
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                            // ST(0) still holds the length
    }
  }

  // ---- Wm3::Eigen<float> -------------------------------------------------
  //
  // Retail 0x00A719E0, `Eigen<float>::Eigen (int iSize)`: ECX = `this`,
  // the size is a stack argument the callee pops (`RET 4`), and the body
  // allocates the two Real arrays and the GMatrix on the retail's heap.
  __declspec(naked) void EigenfCtor(void* self, int size)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this` in ECX
      push dword ptr [esp + 24]      // our `size`
      mov  eax, 0x00A719E0
      call eax                       // `RET 4` pops the size for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A70820, `~Eigen<float>()`: ECX = `this`, no stack
  // arguments, and the body frees m_afSubd, m_afDiag and the two arrays of
  // the GMatrix with the retail's operator delete.
  __declspec(naked) void EigenfDtor(void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A70820
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A6D2D0, `Real& operator() (int iRow, int iCol)`: ECX =
  // `this`, the row and the column are stack arguments (`RET 8`), and the
  // address of the element comes back in EAX.
  __declspec(naked) float* EigenfElement(void* self, int row, int col)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      push dword ptr [esp + 28]      // our `col`
      push dword ptr [esp + 28]      // our `row`
      mov  eax, 0x00A6D2D0
      call eax                       // `RET 8` pops both arguments
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A70D70, `Eigen<float>::IncrSortEigenStuff2 ()`: ECX =
  // `this`, no stack arguments.  The body is Tridiagonal2 (0x00A6D420),
  // QLAlgorithm (0x00A6E7C0), IncreasingSort (0x00A6ED00) and the
  // GuaranteeRotation inlined.
  __declspec(naked) void EigenfIncrSort2(void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A70D70
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A70DB0, `Eigen<float>::IncrSortEigenStuff3 ()`: ECX =
  // `this`, no stack arguments.  The body is Tridiagonal3 (0x00A6D480),
  // QLAlgorithm, IncreasingSort and the GuaranteeRotation inlined.
  __declspec(naked) void EigenfIncrSort3(void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A70DB0
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A6CE70, `Real Eigen<float>::GetEigenvalue (int i) const`:
  // ECX = `this`, the index is a stack argument (`RET 4`), and the float
  // comes back in ST(0).
  __declspec(naked) float EigenfGetEigenvalue(void* self, int i)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      push dword ptr [esp + 24]      // our `i`
      mov  eax, 0x00A6CE70
      call eax                       // `RET 4` pops the index
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                            // the float is still live in ST(0)
    }
  }

  // Retail 0x00A6D350, `void Eigen<float>::GetEigenvector (int i,
  // Vector3<Real>& rkV) const`: ECX = `this`, the index and the address of
  // the vector are stack arguments (`RET 8`).
  __declspec(naked) void EigenfGetEigenvector3(void* self, int i, float* v)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      push dword ptr [esp + 28]      // our `v`
      push dword ptr [esp + 28]      // our `i`
      mov  eax, 0x00A6D350
      call eax                       // `RET 8` pops both arguments
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A6D3A0, the Vector2 overload of the same member.
  __declspec(naked) void EigenfGetEigenvector2(void* self, int i, float* v)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      push dword ptr [esp + 28]      // our `v`
      push dword ptr [esp + 28]      // our `i`
      mov  eax, 0x00A6D3A0
      call eax                       // `RET 8` pops both arguments
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::Eigen<double> -------------------------------------------------
  //
  // Retail 0x00A71DC0, `Eigen<double>::Eigen (int iSize)`: ECX = `this`,
  // the size is a stack argument the callee pops (`RET 4`), and the body
  // allocates the two Real arrays and the GMatrix on the retail's heap.
  __declspec(naked) void EigendCtor(void* self, int size)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      push dword ptr [esp + 24]      // our `size`
      mov  eax, 0x00A71DC0
      call eax                       // `RET 4` pops the size for us
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A70ED0, `~Eigen<double>()`: ECX = `this`, no stack
  // arguments, and the body frees m_afSubd, m_afDiag and the two arrays of
  // the GMatrix with the retail's operator delete.
  __declspec(naked) void EigendDtor(void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A70ED0
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A6EDC0, `Real& operator() (int iRow, int iCol)`: ECX =
  // `this`, the row and the column are stack arguments (`RET 8`), and the
  // address of the element comes back in EAX.
  __declspec(naked) double* EigendElement(void* self, int row, int col)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      push dword ptr [esp + 28]      // our `col`
      push dword ptr [esp + 28]      // our `row`
      mov  eax, 0x00A6EDC0
      call eax                       // `RET 8` pops both arguments
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A71420, `Eigen<double>::IncrSortEigenStuff2 ()`: ECX =
  // `this`, no stack arguments.
  __declspec(naked) void EigendIncrSort2(void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A71420
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A71460, `Eigen<double>::IncrSortEigenStuff3 ()`: ECX =
  // `this`, no stack arguments.
  __declspec(naked) void EigendIncrSort3(void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A71460
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A6CEA0, `Real Eigen<double>::GetEigenvalue (int i) const`:
  // ECX = `this`, the index is a stack argument (`RET 4`), and the double
  // comes back in ST(0).
  __declspec(naked) double EigendGetEigenvalue(void* self, int i)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      push dword ptr [esp + 24]      // our `i`
      mov  eax, 0x00A6CEA0
      call eax                       // `RET 4` pops the index
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                            // the double is still live in ST(0)
    }
  }

  // Retail 0x00A6EE40, `void Eigen<double>::GetEigenvector (int i,
  // Vector3<Real>& rkV) const`: ECX = `this`, the index and the address of
  // the vector are stack arguments (`RET 8`).
  __declspec(naked) void EigendGetEigenvector3(void* self, int i, double* v)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      push dword ptr [esp + 28]      // our `v`
      push dword ptr [esp + 28]      // our `i`
      mov  eax, 0x00A6EE40
      call eax                       // `RET 8` pops both arguments
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A6EE90, the Vector2 overload of the same member.
  __declspec(naked) void EigendGetEigenvector2(void* self, int i, double* v)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      push dword ptr [esp + 28]      // our `v`
      push dword ptr [esp + 28]      // our `i`
      mov  eax, 0x00A6EE90
      call eax                       // `RET 8` pops both arguments
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // ---- Wm3::ConvexHull<double> --------------------------------------------
  //
  // Retail 0x00A6BD30, Wm3::ConvexHull<double>::GetDimension: ECX =
  // `this`, no stack arguments, the dimension in EAX.
  __declspec(naked) int ConvexHulldGetDimension(void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A6BD30
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A6BD40, Wm3::ConvexHull<double>::GetSimplexQuantity: the
  // same shape, the quantity in EAX.
  __declspec(naked) int ConvexHulldGetSimplexQuantity(void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A6BD40
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A6BD50, Wm3::ConvexHull<double>::GetIndices: the same shape,
  // the address of the index array in EAX.
  __declspec(naked) const int* ConvexHulldGetIndices(void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A6BD50
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A79220, Wm3::ConvexHull2<double>'s constructor.  The five
  // arguments are the vertex quantity, the vertex array, the epsilon (a
  // double), bOwner and the query type; the callee pops all of them.
  __declspec(naked) void ConvexHull2dCtor(void* self, int n,
                                         const double* verts, double eps,
                                         u32 owner, u32 type)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]           // our `self` -> the retail `this`
      mov  ebx, [esp + 24]           // `n`
      mov  esi, [esp + 28]           // `verts`
      mov  edi, [esp + 32]           // `eps` (the low half)
      mov  ebp, [esp + 36]           // `eps` (the high half)
      push dword ptr [esp + 44]      // `type`
      push dword ptr [esp + 44]      // `owner` (the offset moved by the push)
      sub  esp, 8
      mov  dword ptr [esp + 0], edi   // `eps` (the low half)
      mov  dword ptr [esp + 4], ebp   // `eps` (the high half)
      push esi                       // `verts`
      push ebx                       // `n`
      mov  eax, 0x00A79220
      call eax                       // `RET 0x18` pops all 24 bytes
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00A74BA0, Wm3::ConvexHull2<double>'s own destructor.
  __declspec(naked) void ConvexHull2dDtor(void* self)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]
      mov  eax, 0x00A74BA0
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  // Retail 0x00452A90, Wm3::Vector3<float>::Length.  `this` in ECX, no stack
  // arguments, a plain RET, and the length comes back in ST(0) - the body
  // computes the squares with SSE and calls the CRT's square root at
  // 0x00452FC0, so the result is the 24-bit root the x87 gives.
  __declspec(naked) float Vector3Length(const float* v)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]      // our `v` -> the retail `this` in ECX
      mov  eax, 0x00452A90
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                        // the float result is still live in ST(0)
    }
  }

  // Retail 0x004F0A50, Wm3::Vector3<float>::CompareArrays.  `this` in ECX,
  // the other vector's address on the stack, and `RET 4` - the callee pops
  // the argument.  The result is an int in EAX: the body is the compiler's
  // inlined memcmp over the twelve bytes, and it answers -1, 0 or +1.
  __declspec(naked) int Vector3CompareArrays(const float* a, const float* b)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]      // our `a` -> the retail `this` in ECX
      mov  eax, [esp + 24]      // our `b` -> the retail's stack argument
      push eax                  // the argument; the retail's `RET 4` pops it
      mov  eax, 0x004F0A50
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                        // the int result is in EAX
    }
  }

  // Retail 0x004F08B0, Wm3::Vector3<float>::operator==.  `this` in ECX, the
  // other vector's address on the stack, and the body pops it (`RET 4`).
  __declspec(naked) int Vector3OperatorEquals(const float* a, const float* b)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]      // our `a` -> the retail `this` in ECX
      mov  eax, [esp + 24]      // our `b` -> the retail's stack argument
      push eax                  // the argument; the retail's `RET 4` pops it
      mov  eax, 0x004F08B0
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                        // the bool is in EAX
    }
  }

  // Retail 0x004F0B40, Wm3::Quaternion<float>::CompareArrays.  The argument
  // comes in EAX, not on the stack, and the body returns with a plain RET -
  // so nothing is pushed here and nothing is popped here either.
  __declspec(naked) int QuaternionCompareArrays(const float* a, const float* b)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]      // our `a` -> the retail `this` in ECX
      mov  eax, [esp + 24]      // our `b` -> the retail's EAX
      mov  edx, 0x004F0B40      // the target goes in EDX: EAX is the argument
      call edx
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                        // the int is in EAX
    }
  }

  // Retail 0x004F08E0, Wm3::Quaternion<float>::operator==.  The same register
  // convention as CompareArrays: the body starts with a bare call and
  // assumes ECX and EAX are already set.
  __declspec(naked) int QuaternionOperatorEquals(const float* a, const float* b)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]      // our `a` -> the retail `this` in ECX
      mov  eax, [esp + 24]      // our `b` -> the retail's EAX
      mov  edx, 0x004F08E0      // the target goes in EDX: EAX is the argument
      call edx
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret                        // the bool is in EAX
    }
  }

  // Retail 0x004740C0, Wm3::Sphere3<float>::Sphere3.  ECX is the hidden
  // return pointer - the object - and the centre and the radius are the two
  // stack arguments, which the body's `RET 8` pops.
  __declspec(naked) void Sphere3Ctor(void* object, const float* center,
                                      float radius)
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  ecx, [esp + 20]      // our `object` -> the retail's hidden pointer
      mov  eax, [esp + 24]      // our `center` -> the retail's first argument
      movss xmm0, [esp + 28]    // our `radius` -> the retail's second argument
      // cdecl pushes: the last push lands at the lowest address, and the
      // retail reads the centre at [esp+4] and the radius at [esp+8] - so
      // the radius goes down first and the centre last.
      sub  esp, 4
      movss [esp], xmm0         // the radius argument
      push eax                  // the centre argument
      mov  eax, 0x004740C0
      call eax                  // the retail's `RET 8` pops the two arguments
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

} // namespace wm3collretail
