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

// ---- Wm3::Distance<double,Vector3d> and ::Distance<double,Vector2d> -----
  //
  // The double instantiation of the base class.  The retail's bodies are x87
  // code and the engine runs under `_PC_24`, so the arithmetic is a 24-bit
  // significand - see the header for the addresses and the member offsets.

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

// ---- Wm3::DistVector3Box3<double> ---------------------------------------

// ---- Wm3::DistVector3Segment3<double> ------------------------------------

// ---- Wm3::DistVector2Box2<double> --------------------------------------

// ---- Wm3::DistSegment3Segment3<double> ----------------------------------

// ---- Wm3::DistLine3Segment3<double> -------------------------------------

// ---- Wm3::IntrBox2Circle2<double> ---------------------------------------

// ---- Wm3::IntrSegment3Capsule3<double> -----------------------------------

// ---- Wm3::IntrBox3Sphere3<double> ---------------------------------------

// ---- Wm3::IntrBox2Box2<double> ----------------------------------------------

// ---- Wm3::IntrLine3Box3<double> ----------------------------------------------

// ---- Wm3::IntrSegment3Box3<double> ----------------------------------------------

// ---- Wm3::IntrSegment3Sphere3<double> ----------------------------------------------

// ---- Wm3::ConvexHull1<float> and Wm3::ConvexHull2<float> ----------------

// ---- Wm3::Query3<float> -------------------------------------------------
  //
  // The retail builds this class inline inside its ConvexHull3<float>
  // constructor (0x00A6743B), so there is no constructor to call: the object
  // carries the vtable pointer at [this+0x00], the vertex count at [this+0x04]
  // and the vertex array at [this+0x08].
  //
  // Every thunk below is thiscall: ECX = `this`, the arguments on the stack in
  // the order the retail's own forwarders use, and the callee pops them.

// ---- Wm3::Query2<float> -------------------------------------------------

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
