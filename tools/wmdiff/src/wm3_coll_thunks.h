#pragma once

// Retail side of the Wild Magic 3.8 collision/distance comparison.
//
// A thunk is here only once its retail address is known. The functions that are
// reachable but have no retail address yet are listed in the README's "What is
// in scope" section and in wmdiff/reachable.md. See wm3_coll_targets.cpp and
// the README's address procedure.

#include <cstdint>

namespace wm3collretail
{
  using u32 = std::uint32_t;

  // Every one of these classes is built by construction and then queried, and
  // each holds pointers to caller-owned data. The retail constructors are
  // therefore driven too rather than the object layout being fabricated by
  // hand: they also set the vtable pointer and call the base constructor, and
  // the queries read those members back.

  // ---- Wm3::DistVector3Box3<float> ----------------------------------------
  // Retail 0x00A45730. ECX = `this`; the two reference members are passed as
  // pointers and stored at [this+0x34] and [this+0x38]; `RET 8`.
  void DistVector3Box3Ctor(void* self, const float* vec, const float* box);

  // Retail 0x00A45C00. ECX = `this`, no stack arguments, result in ST(0).
  float DistVector3Box3GetSquared(const void* self);

  // ---- Wm3::DistVector3Segment3<float> ------------------------------------
  // Retail 0x00A48030. Same shape as the Box3 constructor: the two references
  // land at [this+0x34] and [this+0x38], vtable 0x00D71E6C, `RET 8`.
  void DistVector3Segment3Ctor(void* self, const float* vec, const float* segment);

  // Retail 0x00A484F0, a direct CALL from Moho::Projectile::CheckCollision
  // (0x0069D1D0). ECX = `this`, no stack arguments, result in ST(0).
  float DistVector3Segment3GetSquared(const void* self);

// Retail 0x00A43420 - the Intersector vtable's +0x0C slot for
  // IntrBox3Sphere3, which is Find() as well. ECX = `this`; three stack
  // arguments, fTMax then the two velocities, ending in `RET 0xC`, bool in AL.
  // The argument slots are read at [ESP+0x84] (&rkVelocity1) and [ESP+0x8C]
  // (&rkVelocity0) inside the body, which is where this thunk puts them.
  u32 IntrBox3Sphere3Find(void* self, float fTMax, const float* vel0, const float* vel1);

  // Retail 0x00A414C0. ECX = `this`, no stack arguments, and EAX comes back
  // holding `this + 0x14` - a pointer to m_kContactPoint, which Find fills in
  // on the success paths only.
  const float* IntrBox3Sphere3GetContactPoint(const void* self);

  // Retail 0x00A480E0 - the address the engine actually CALLs, e.g. from
  // Moho::Projectile::CheckCollision. ECX = `this`, no stack arguments, result in
  // ST(0). The whole body is seven instructions: load the vtable, dispatch
  // GetSquared through slot +0x18, take its sqrt, narrow that to float through a
  // stack slot, reload it and return.
  //
  // The vtable slot itself (0x00A48190) is a stack-probe-and-SEH-framed
  // override that calls the double sqrt directly; 0x00A480E0 is the thin
  // dispatch the rebuilt `Get()` corresponds to, and it is the one with a call
  // site anchoring it.
  float DistVector3Segment3Get(const void* self);

  // ---- Wm3::IntrBox2Circle2<float> ----------------------------------------
  // Retail 0x00A4E5F0. The references land at [this+0x0C] and [this+0x10],
  // vtable 0x00D71EC4, `RET 8`.
  void IntrBox2Circle2Ctor(void* self, const float* box, const float* circle);

  // Retail 0x00A4E620, a direct CALL from Moho::VisionDB::Entry::TryAdd
  // (0x0081B490). ECX = `this`, no stack arguments, the bool comes back in
  // EAX (`MOV EAX,1` on the hit path, `XOR EAX,EAX` on the miss path).
  u32 IntrBox2Circle2Test(const void* self);

  // ---- Wm3::IntrLine3Box3<float> ------------------------------------------
  // Retail 0x00A4FE30. The references land at [this+0x0C] and [this+0x10],
  // vtable 0x00D71EF4, `RET 8`.
  //
  // Located through the vtable rather than a direct CALL: Moho::UpdateSelection
  // (0x0086F520, annotated at src/sdk/moho/ui/UiRuntimeTypes.cpp:20778) calls
  // the constructor and then dispatches through slot +0x08, whose contents are
  // this address. That still satisfies the scope rule - located in the retail
  // image, and the disassembly (three-axis slab test against the box extent and
  // centre and the line origin/direction) identifies it as
  // Wm3::IntrLine3Box3<float>::Test.
  void IntrLine3Box3Ctor(void* self, const float* line, const float* box);

  // Retail 0x00A4FE60. ECX = `this`, no stack arguments, bool in AL/EAX.
  u32 IntrLine3Box3Test(const void* self);

// ---- Wm3::IntrBox3Sphere3<float> ---------------------------------------
  // Retail 0x00A41530, confirmed three ways rather than by adjacency: a direct
  // CALL at 0x004FE891 in Moho::CColPrimitive_Sphere::CollideBox (0x004FE860),
  // a vtable store of 0x00D71CFC whose slot +0x08 is the Test below, and the
  // same shape as the two sibling constructors - ECX = `this`, the two
  // references at [this+0x0C] / [this+0x10], base Intersection3f constructor
  // called first, `RET 8`.
  void IntrBox3Sphere3Ctor(void* self, const float* box, const float* sphere);

  // Retail 0x00A41560. ECX = `this`, no stack arguments, bool in AL only.
  u32 IntrBox3Sphere3Test(const void* self);

  // Retail 0x00A41740 - GetVertexIntersection, a private static member of the
  // WM3 class that the retail's Find calls instead of inlining it (the CALL
  // is at 0x00A43831, inside Find at 0x00A43420). cdecl, not thiscall: seven
  // float arguments on the stack - fDx, fDy, fDz, fVx, fVy, fVz, fRSqr, in
  // that order - and the caller removes them (the retail's own caller does
  // `ADD ESP,0x1C`). The result comes back in ST(0): the body ends in
  // `FLD float ptr [ESP+0x18]; RET`, and EAX is never written.
  float IntrBox3Sphere3GetVertexIntersection(float fDx, float fDy, float fDz,
                                             float fVx, float fVy, float fVz,
                                             float fRSqr);

  // Retail 0x00A41800 - GetEdgeIntersection, the same shape with six
  // arguments: fDx, fDz, fVx, fVz, fVSqr, fRSqr. cdecl, the result in ST(0).
  // The retail's Find calls it at 0x00A43868 and at 0x00A438D1.
  float IntrBox3Sphere3GetEdgeIntersection(float fDx, float fDz, float fVx,
                                           float fVz, float fVSqr, float fRSqr);

  // ---- Wm3::IntrBox3Sphere3<float>::FindFaceRegionIntersection ------------
  //
  // Retail 0x00A41880. A thiscall member function of the WM3 class: ECX is
  // `this` (the body does `MOV ESI,ECX` and then reads the sphere reference
  // at [ESI+0x10] and the radius at [sphere+0x0C]), and the thirteen
  // arguments are on the stack. The callee removes them - the body ends in
  // `RET 0x34`, thirteen dwords - so this is the MSVC __thiscall shape with
  // callee cleanup.
  //
  // The argument order is the Wild Magic declaration order, which the retail's
  // own body confirms:
  //
  //   arg1  fEx    the extent along the face's x axis
  //   arg2  fEy    the extent along the face's y axis
  //   arg3  fEz    the extent along the face's normal (z) axis
  //   arg4  fCx    the sphere centre, x
  //   arg5  fCy    the sphere centre, y
  //   arg6  fCz    the sphere centre, z
  //   arg7  fVx    the velocity, x
  //   arg8  fVy    the velocity, y
  //   arg9  fVz    the velocity, z
  //   arg10 &rfIx  out: the intersection's x
  //   arg11 &rfIy  out: the intersection's y
  //   arg12 &rfIz  out: the intersection's z
  //   arg13 bAboveFace  a flag
  //
  // Evidence for the mapping: the body loads arg6 (the centre's z) and arg3
  // (the normal extent), adds the radius to arg3 and compares the two - the
  // "the sphere already touches the face" test - then reads arg13 as a byte
  // (the flag), and arg9 against zero (the "moving away from the face" test).
  //
  // The body writes the contact time into `this` at [this+0x04] and the
  // three intersection coordinates through the pointer arguments.
  int IntrBox3Sphere3FindFaceRegionIntersection(void* self, float fEx, float fEy,
      float fEz, float fCx, float fCy, float fCz, float fVx, float fVy,
      float fVz, float* pix, float* piy, float* piz, u32 above);

  // ---- Wm3::IntrBox3Sphere3<float>::FindJustEdgeIntersection --------------
  //
  // Retail 0x00A41CC0. The same thiscall shape, twelve arguments (`RET 0x30`),
  // and the same Wild Magic declaration order:
  //
  //   arg1  fCy    the sphere centre, y
  //   arg2  fEx    the edge's x
  //   arg3  fEy    the edge's half extent in y
  //   arg4  fEz    the edge's z
  //   arg5  fDx    the sphere centre's offset from the edge, x
  //   arg6  fDz    the sphere centre's offset from the edge, z
  //   arg7  fVx    the velocity, x
  //   arg8  fVy    the velocity, y
  //   arg9  fVz    the velocity, z
  //   arg10 &rfIx  out: the intersection's x
  //   arg11 &rfIy  out: the intersection's y
  //   arg12 &rfIz  out: the intersection's z
  //
  // Evidence: the body compares arg8 against zero (the sign of the velocity
  // along the edge), forms arg1 - arg3 (the sphere's distance from the edge's
  // end) and arg5, arg6, arg7 and arg9 are the offsets and the velocity.
  int IntrBox3Sphere3FindJustEdgeIntersection(void* self, float fCy, float fEx,
      float fEy, float fEz, float fDx, float fDz, float fVx, float fVy,
      float fVz, float* pix, float* piy, float* piz);

  // ---- Wm3::IntrBox3Sphere3<float>::FindEdgeRegionIntersection ------------
  //
  // Retail 0x00A41F50. The same thiscall shape, thirteen arguments
  // (`RET 0x34`), and the same Wild Magic declaration order:
  //
  //   arg1  fEx    the box's extent along x
  //   arg2  fEy    the box's extent along y
  //   arg3  fEz    the box's extent along z
  //   arg4  fCx    the sphere centre, x
  //   arg5  fCy    the sphere centre, y
  //   arg6  fCz    the sphere centre, z
  //   arg7  fVx    the velocity, x
  //   arg8  fVy    the velocity, y
  //   arg9  fVz    the velocity, z
  //   arg10 &rfIx  out: the intersection's x
  //   arg11 &rfIy  out: the intersection's y
  //   arg12 &rfIz  out: the intersection's z
  //   arg13 bAboveEdge  a flag
  //
  // Evidence: the body forms arg4 - arg1 and arg6 - arg3 (the sphere's
  // offsets beyond the two faces the edge is formed by) and reads arg13 as a
  // byte.
  int IntrBox3Sphere3FindEdgeRegionIntersection(void* self, float fEx, float fEy,
      float fEz, float fCx, float fCy, float fCz, float fVx, float fVy,
      float fVz, float* pix, float* piy, float* piz, u32 above);

  // ---- Wm3::IntrBox3Sphere3<float>::FindVertexRegionIntersection ----------
  //
  // Retail 0x00A421E0. The same thiscall shape, twelve arguments
  // (`RET 0x30`), and the same Wild Magic declaration order:
  //
  //   arg1  fEx    the corner's x
  //   arg2  fEy    the corner's y
  //   arg3  fEz    the corner's z
  //   arg4  fCx    the sphere centre, x
  //   arg5  fCy    the sphere centre, y
  //   arg6  fCz    the sphere centre, z
  //   arg7  fVx    the velocity, x
  //   arg8  fVy    the velocity, y
  //   arg9  fVz    the velocity, z
  //   arg10 &rfIx  out: the intersection's x
  //   arg11 &rfIy  out: the intersection's y
  //   arg12 &rfIz  out: the intersection's z
  //
  // Evidence: the body forms arg4 - arg1, arg5 - arg2 and arg6 - arg3 (the
  // sphere's offsets beyond the corner) and then the cross products of the
  // velocity with those offsets.
  int IntrBox3Sphere3FindVertexRegionIntersection(void* self, float fEx,
      float fEy, float fEz, float fCx, float fCy, float fCz, float fVx,
      float fVy, float fVz, float* pix, float* piy, float* piz);

  // ---- Wm3::IntrSegment3Box3<float> --------------------------------------
  // Retail 0x00A463A0. Three arguments this time: the Segment3 and Box3
  // references land at [this+0x0C] / [this+0x10] and the `bool bSolid` member
  // at [this+0x14]; vtable 0x00D71DDC; `RET 0xC`.
  void IntrSegment3Box3Ctor(void* self, const float* segment, const float* box, u32 solid);

  // Retail 0x00A462A0. ECX = `this`, no stack arguments of its own (the EDX
  // values it pushes are arguments to a nested call), bool in AL.
  u32 IntrSegment3Box3Find(const void* self);

  // Retail 0x00A46300, six call sites in Moho::CColPrimitive_Box::CollideLine.
  // ECX = `this`, one dword argument (`int i`) at [esp+4], `RET 4`, and the
  // return value is a **pointer** - `&m_akPoint[i]`, computed as
  // `this + 0x1C + 12*i` - not a Vector3 by value. The target dereferences it on
  // both sides rather than comparing addresses.
  const float* IntrSegment3Box3GetPoint(const void* self, u32 index);

  // ---- Wm3::IntrSegment3Sphere3<float> -------------------------------------
  // Retail 0x00A46A10 - the constructor, a direct CALL from
  // Moho::CColPrimitive_Sphere::CollideLine (0x004FE9D0) at 0x004FEA13,
  // which is annotated at src/sdk/moho/collision/CColPrimitiveBase.cpp:569.
  // ECX = `this`; the two reference members are passed as pointers, the segment
  // first and the sphere second, and land at [this+0x10] and [this+0x14]. The
  // constructor also writes the 1e-6f ZeroThreshold at [this+0x0C] and the
  // vtable 0x00D71E0C. `RET 8`.
  void IntrSegment3Sphere3Ctor(void* self, const float* segment,
                               const float* sphere);

  // Retail 0x00A471C0 - the no-argument Find(), a direct CALL from the same
  // CollideLine at 0x004FEA27, and also the class vtable's +0x10 slot.
  // ECX = `this`, no stack arguments, the bool comes back in AL only. The body
  // writes m_iQuantity at [this+0x18] and the intersection points at
  // [this+0x1C] and [this+0x28].
  u32 IntrSegment3Sphere3Find(const void* self);

// Retail 0x00A46940 - GetPoint(int), called three times from the same
  // CollideLine (0x004FEA3A, 0x004FEA98, 0x004FEAED).
  // ECX = `this`, one dword stack argument (the index) at [esp+4], `RET 4`,
  // and EAX comes back holding &m_akPoint[index] - a pointer into the object
  // at this + 0x1C + 12*index, not a Vector3 by value.
  const float* IntrSegment3Sphere3GetPoint(const void* self, u32 index);

  // ---- Wm3::IntrBox2Box2<float> --------------------------------------------
  // Retail 0x00A47BF0 - the constructor, a direct CALL from the AI steering
  // test (0x00596930) at 0x00596DB2, which is annotated at
  // src/sdk/moho/ai/CAiSteeringImpl.cpp:268.
  // ECX = `this`; the two reference members are passed as pointers and land at
  // [this+0x0C] and [this+0x10]; the vtable is 0x00D71E3C. `RET 8`.
  void IntrBox2Box2Ctor(void* self, const float* box0, const float* box1);

  // Retail 0x00A47D40 - Test(), a direct CALL from the same AI steering test
  // at 0x00596DC3 and the class vtable's +0x0C slot.
  // ECX = `this`, no stack arguments, the bool comes back in AL only. The body
  // is the separating-axis test on the two boxes.
  u32 IntrBox2Box2Test(const void* self);

// ---- Wm3::DistVector2Box2<float> ----------------------------------------
  // Retail 0x00A6C5E0 (constructor) and 0x00A6C9D0 (GetSquared). The
  // constructor is the same thiscall shape as DistVector3Box3's: ECX = `this`,
  // the vector and the box on the stack, stored at [this+0x2C] and [this+0x30],
  // vtable 0x00D720C4, `RET 8`. GetSquared: ECX = `this`, no stack arguments;
  // the result comes back in ST(0) and is stored through `out`.
  void DistVector2Box2Ctor(void* self, const float* vec, const float* box);
  void DistVector2Box2GetSquared(const void* self, float* out);

  // Retail 0x00A6C680, Wm3::DistVector2Box2<float>::Get() - the class vtable's
  // +0x0C slot, and the slot IntrBox2Circle2<float>::Test (0x00A4E620)
  // dispatches through: that body builds a DistVector2Box2 on its own stack
  // with the constructor at 0x00A6C5E0 and then calls
  // `MOV EAX,[EDX+0x0C]; CALL EAX`.
  //
  // ECX = `this`, no stack arguments. The body is six instructions: dispatch
  // the vtable's +0x18 slot (GetSquared), take the CRT's double square root
  // at 0x00A91000, narrow the result to a float through a stack slot, reload
  // it and return - so the float ends in ST(0).
  void DistVector2Box2Get(const void* self, float* out);

// ---- Wm3::InBox<float> ---------------------------------------------------
  // Retail 0x00A3BC10, a free function rather than a member. The engine calls
  // it from Moho::CColPrimitiveBase::CollideBox (0x004FE860), which the
  // engine's own comment names: "Calls Wild Magic's `InBox<float>`
  // (0x00A3BC10, Wm3ContBox3.cpp) with the point and `mShape`".
  //
  // It is cdecl: the point and the box are two stack pointers, the callee
  // leaves them there (`ADD ESP,0x0C; RET`), and the caller removes them. The
  // bool comes back in AL - AL = 1 when the point is inside the box.
  u32 InBox(const float* point, const float* box);

  // ---- Wm3::DistVector3Segment3<float>::Get(fT,v0,v1) --------------------
  // Retail 0x00A48190 - the class vtable's +0x08 slot, the swept Get.
  //
  // ECX = `this`, three stack arguments (fT, rkVelocity0, rkVelocity1),
  // `RET 0xC` - the callee pops them - and the float in ST(0).
  //
  // The body computes the moving shapes and then dispatches the vtable's
  // +0x0C slot, the no-argument Get, which is the same shape as the rebuilt
  // DistVector3Segment3::Get(fT,v0,v1).
  void DistVector3Segment3Get(const void* self, float fT, const float* vel0,
                               const float* vel1, float* out);

  // ---- Wm3::DistVector3Segment3<float>::GetSquared(fT,v0,v1) -------------
  // Retail 0x00A48340 - the class vtable's +0x14 slot, the swept GetSquared.
  // Same ABI as the swept Get: ECX = `this`, three stack arguments, `RET 0xC`,
  // the float in ST(0). The body dispatches the vtable's +0x18 slot, the
  // no-argument GetSquared.
  void DistVector3Segment3GetSquared(const void* self, float fT,
                                     const float* vel0, const float* vel1,
                                     float* out);

  // ---- Wm3::ContAlignedBox<float> -----------------------------------------
  // Retail 0x00A3AE40, a direct CALL at 0x004FE2EE in the engine's
  // Moho::AxisAlignedBoundsFromBox (0x004FE2D0) - the function whose rebuilt
  // source calls Wm3::ContAlignedBox(8, vertices).
  //
  // It is a free function that returns a Box3 **by value**, so the hidden
  // return pointer is the first stack argument; the callee leaves all three
  // arguments on the stack (it ends in a plain RET) and the caller removes
  // them. The stack is (Box3* out, int iQuantity, const Vector3* akPoint),
  // and `out` receives fifteen floats: the centre, the three axes and the
  // three extents, in the rebuild's layout.
  void ContAlignedBox(void* out, int quantity, const float* points);

// ---- Wm3::ContAlignedBox<float>, with the mask ---------------------------
  // The same retail entry point (0x00A3AE40), but the fourth stack argument -
  // the `abValid` mask - is passed through instead of being a literal zero.
  // The retail tests that argument a byte at a time (0x00A3B30E:
  // cmp byte ptr [ecx+esi],0), so a nonzero byte means "this point counts";
  // a null mask selects the retail's unmasked path.
  void ContAlignedBoxMask(void* out, int quantity, const float* points,
                          const unsigned char* mask);

  // ---- the swept Distance queries ----------------------------------------
  //
  // All six of these are the *derived* class's own virtual, the one the class
  // vtable puts at +0x08 (Get) and +0x14 (GetSquared). The base class's
  // generic GetSquared(fT,v0,v1) sits at +0x10 and is NOT wired here: the
  // rebuilt base has no counterpart for it, so there is nothing to compare it
  // against.
  //
  // The shape is the one DistVector3Segment3's two swept rows already use
  // (0x00A48190 / 0x00A48340): a thiscall with `this` in ECX, three stack
  // arguments - fT, rkVelocity0, rkVelocity1 - and `RET 0xC`, so the callee
  // pops them. The float comes back in ST(0) and is stored through `out`.
  //
  // These retail bodies are stack-probe-and-SEH-framed: they set up their own
  // exception handler and probe the stack before touching it. The thunk does
  // not have to know about that - it only has to hand over the arguments in
  // the retail's order and read ST(0) on the way back.

// ---- Wm3::IntrLine3Box3<float>::DoClipping and ::Clip -------------------
  //
  // DoClipping is retail 0x00A50220, a *static* member, so there is no
  // `this`: nine stack arguments, in this order -
  //
  //   fT0, fT1, &origin, &direction, &box, bSolid, &riQuantity, akPoint,
  //   &riIntrType
  //
  // and the body is cdecl: it ends in a plain `RET` and the caller removes
  // the thirty-six bytes (the retail's own Find at 0x00A508F0 does
  // `ADD ESP,0x2C` after the call). The bool comes back in AL.
  //
  // The nine arguments are established from the body itself: the first three
  // pushed by the retail's Find are &riIntrType, akPoint and &riQuantity
  // (0x00A508FC, 0x00A50904, 0x00A5090C), then the literal 1 for bSolid
  // (0x00A5091B), then &box, &direction and &origin (0x00A50913, 0x00A5091E,
  // 0x00A50922) and finally the two floats fT0 and fT1 in the eight bytes
  // the `SUB ESP,8` at 0x00A50923 reserves.
  //
  // Clip is retail 0x00A4FCC0, a *private static* member: four stack
  // arguments - fDenom, fNumer, &rfT0, &rfT1 - cdecl as well (the body ends
  // in a plain `RET`, and the retail's own call site at 0x00A5032D does
  // `ADD ESP,0x10`), and the bool comes back in AL.
  u32 IntrLine3Box3DoClipping(float fT0, float fT1, const float* origin,
                              const float* direction, const float* box,
                              int solid, int* quantity, float* points,
                              int* type);
  u32 IntrLine3Box3Clip(float denom, float numer, float* t0, float* t1);

// ---- Wm3::Distance<float,Vector3f> --------------------------------------
  //
  // The base class's own vftable is at 0x00D71C54 (the base constructor
  // installs it), so these three bodies belong to the base itself, not to a
  // derived class.
  //
  // 0x00A39F20 is the base constructor: ECX is `this`, no stack arguments.
  // It installs the vtable, MaximumIterations = 8 at [this+0x04], the
  // ZeroThreshold at [this+0x08], the contact time at [this+0x0C], the two
  // flags at [this+0x28] and [this+0x29], the difference step 1e-3 at
  // [this+0x2C] and its reciprocal 500.0f at [this+0x30]. The two defaults
  // are *inlined* here - the constructor does not call the setter.
  //
  // 0x00A391D0 is SetDifferenceStep: ECX is `this`, one float argument on the
  // stack, and the callee pops it (`RET 4`).
  //
  // 0x00A39200 is GetDifferenceStep: ECX is `this`, no stack argument, the
  // float in ST(0).
  void DistanceCtor(void* self);
  void DistanceSetDifferenceStep(void* self, float step);

// ---- Wm3::Vector3<float>::Length ---------------------------------------
  //
  // Retail 0x00452A90: ECX = `this`, no stack arguments, a plain RET, and
  // the length comes back in ST(0).  The body computes the three squares
  // with SSE (MOVSS/MULSS) and calls the CRT's square root at 0x00452FC0, so
  // the sum behaves as binary32 in the denormal range - which is where this
  // body differs from the x87 SquaredLength body at 0x004757E0.
  float Vector3Length(const float* v);

  // ---- Wm3::Vector3<float>::CompareArrays -------------------------------
  //
  // Retail 0x004F0A50: ECX = `this`, one stack argument (the address of the
  // other vector) and `RET 4`, so the callee pops it.  The result is an int
  // in EAX: the body is the compiler's inlined memcmp over the twelve bytes,
  // and the inlined memcmp answers -1, 0 or +1 - not the byte difference the
  // CRT's memcmp returns.  There is no floating point in it at all.
  int Vector3CompareArrays(const float* a, const float* b);

  // ---- Wm3::Vector3<float>::operator== ---------------------------------
  //
  // Retail 0x004F08B0: ECX = `this`, the other vector's address on the stack,
  // and `RET 4` - the body pops the argument itself.  The body loads the
  // argument with `mov eax,[esp+4]`, pushes it for the memcmp at 0x004F0A50
  // and folds that result into a bool (`neg eax; sbb eax,eax; add eax,1`),
  // so EAX comes back 0 or 1.
  int Vector3OperatorEquals(const float* a, const float* b);

  // ---- Wm3::Quaternion<float>::CompareArrays ----------------------------
  //
  // Retail 0x004F0B40: **ECX = `this` and EAX = the other quaternion**, and
  // the body ends in a plain RET - it takes no stack argument at all.  It is
  // the compiler's out-of-line memcmp over the sixteen bytes, and it answers
  // -1, 0 or +1.  A thunk that pushed the argument would change the stack
  // the body returns to, so this one sets the two registers and pushes
  // nothing.
  int QuaternionCompareArrays(const float* a, const float* b);

  // ---- Wm3::Quaternion<float>::operator== -------------------------------
  //
  // Retail 0x004F08E0: the same register convention as CompareArrays - ECX =
  // `this`, EAX = the other quaternion, a plain RET - and the body starts
  // with a bare `call 0x004F0B40`, so it assumes both registers are already
  // set.  The result is the memcmp result folded into a bool.
  int QuaternionOperatorEquals(const float* a, const float* b);

  // ---- Wm3::Sphere3<float>::Sphere3(const Wm3::Vector3<float>&, float) -----
  //
  // Retail 0x004740C0: **ECX is the hidden return pointer** - the object the
  // constructor writes - the centre's address is at [esp+4] and the radius at
  // [esp+8], and the body ends in `RET 8`, so the callee pops the two
  // arguments.  It writes all sixteen bytes of the object: the three centre
  // lanes (through the x87, so a quiet NaN keeps its payload) at +0, +4 and
  // +8, and the radius (a plain `movss`) at +0xC.  No padding, no
  // references, no byte either side leaves unwritten.
  void Sphere3Ctor(void* object, const float* center, float radius);

} // namespace wm3collretail
