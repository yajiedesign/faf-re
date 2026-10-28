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

  // Retail 0x00A4F580 - the Intersector vtable's +0x0C slot, which is Find().
  // ECX = `this`; three stack arguments, fTMax then the two velocities, ending
  // in `RET 0xC`. The bool comes back in AL only. The velocities are Vector2
  // here, not Vector3.
  // Retail 0x00A458C0 and 0x00A48190 - the Distance vtable's +0x08 slot, which
  // is Get(fT, rkVelocity0, rkVelocity1). ECX = `this`; three stack arguments,
  // fT then the two velocities, ending in `RET 0xC`; result in ST(0).
  //
  // Retail 0x00A4F580 - the Intersector vtable's +0x0C slot, which is Find().
  // ECX = `this`; three stack arguments, fTMax then the two velocities, ending
  // in `RET 0xC`. The bool comes back in AL only. The velocities are Vector2.
  u32 IntrBox2Circle2Find(void* self, float fTMax, const float* vel0, const float* vel1);

  // Retail 0x00A457E0 - the Distance vtable's +0x0C slot, which is Get().
  // ECX = `this`, no stack arguments, result in ST(0). Seven instructions, the
  // same shape as DistVector3Segment3::Get: dispatch slot +0x18 (GetSquared),
  // take the CRT's square root, narrow it to float and return.
  float DistVector3Box3Get(const void* self);

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

  // Retail 0x00A39230, a direct CALL from Moho::Projectile::CheckCollision at
  // 0x0069D706. ECX = `this`, no stack arguments, and EAX comes back holding
  // `this + 0x1C` - the base class's m_kClosestPoint1, which the previous
  // GetSquared call filled in. A pointer again, so the target dereferences it.
  const float* DistVector3Segment3GetClosestPoint1(const void* self);

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

  // Retail 0x00A46C80 - the no-argument Test(), the class vtable's +0x08 slot
  // (the dynamic Find above dispatches it through that slot).
  // ECX = `this`, no stack arguments, the bool comes back in AL only. The body
  // is the same quadratic as Find(), without the roots: it reads the segment
  // at [this+0x10] and the sphere at [this+0x14] and answers whether the
  // segment meets the sphere.
  u32 IntrSegment3Sphere3Test(const void* self);

  // Retail 0x00A46B10 - the swept Test(fTMax, velocity0, velocity1), the class
  // vtable's +0x04 slot. ECX = `this`; three stack arguments, fTMax then the
  // two velocities, ending in `RET 0xC`; the bool comes back in AL only.
  // The body calls the static Test through the vtable first (MOV EDX,[EAX+8];
  // CALL EDX) and then reduces the swept sphere to a segment-versus-capsule
  // test, which it answers with a bool and nothing else.
  u32 IntrSegment3Sphere3TestDynamic(void* self, float fTMax, const float* vel0,
                                     const float* vel1);

  // Retail 0x00A46DB0 - the swept Find(fTMax, velocity0, velocity1), the class
  // vtable's +0x0C slot. ECX = `this`; three stack arguments, fTMax then the
  // two velocities, ending in `RET 0xC`; the bool comes back in AL only.
  // The body computes the contact time and the contact point: it takes the
  // segment parameter from the capsule (GetParameter(0), 0x00A6BE00) and
  // writes the sphere centre plus that parameter times the swept direction
  // into m_kContactPoint at [this+0x14].
  u32 IntrSegment3Sphere3FindDynamic(void* self, float fTMax, const float* vel0,
                                     const float* vel1);

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

  // ---- Wm3::DistSegment3Segment3<float> -----------------------------------
  // Retail 0x00A7E190 (constructor) and 0x00A7E660 (GetSquared). Both are
  // wired rows now, not a probe: the retail's distance formulation has been
  // extracted into Foundation/Distance/Wm3DistSegment3Segment3.cpp and the two
  // rows compare it against retail.
  //
  // The constructor is the same thiscall shape as DistVector3Box3's: ECX =
  // `this`, the two segment pointers on the stack, stored at [this+0x34] and
  // [this+0x38], `RET 8`. The object needs 0x44 bytes, not the 0x3C the other
  // rows use.
  void DistSegment3Segment3Ctor(void* self, const float* segment0,
                                const float* segment1);

  // GetSquared leaves the result in ST(0), so this thunk FSTPs it into `out`
  // and hands back the raw bits rather than a float through EAX.
  void DistSegment3Segment3GetSquared(const void* self, float* out);

  // Retail 0x00A6BE80 / 0x00A6BEB0, Wm3::IntrSegment3Capsule3<float>'s
  // constructor and Test(). Same thiscall shape as the Intersector rows: the
  // segment and the capsule are two stack pointers, stored at [this+0x0C] and
  // [this+0x10] respectively - segment first, capsule second - vtable
  // 0x00D72094, `RET 8`; the Test takes no arguments, bool in AL.
  //
  // The capsule pointer is a pointer to a whole Capsule3 - the spine Segment3
  // followed by the radius - not to the spine alone: the retail's Find reads
  // the radius at [capsule+0x1C].
  void IntrSegment3Capsule3Ctor(void* self, const float* segment,
                                const float* capsule);
  u32 IntrSegment3Capsule3Test(void* self);

  // Retail 0x00A7F730 / 0x00A7F760, Wm3::IntrLine3Capsule3<float>'s
  // constructor and Test().
  //
  // The constructor is the same thiscall shape as the other Intersector
  // constructors: ECX = `this`, the line and the capsule on the stack, stored
  // at [this+0x0C] and [this+0x10] respectively - line first, capsule second
  // - vtable 0x00D722A4, `RET 8`. The capsule pointer is a pointer to a whole
  // Capsule3 - the spine Segment3 followed by the radius - and the retail's
  // Test reads the radius at [capsule+0x1C].
  //
  // The Test is the class vtable's +0x08 slot, so the address was read out of
  // the class's own vtable (0x00D722A4) and confirmed by the constructor's
  // vtable immediate and by the body: it builds a DistLine3Segment3<float>
  // from the line and the capsule's spine, dispatches its +0x0C slot (the
  // distance, 0x00A809A0) and compares the result with the capsule's radius.
  // ECX = `this`, no stack arguments, the bool in AL.
  void IntrLine3Capsule3Ctor(void* self, const float* line,
                             const float* capsule);
  u32 IntrLine3Capsule3Test(void* self);

  // ---- Wm3::ConvexHull3<float> --------------------------------------------
  // Retail 0x00A67120 (constructor) and 0x00A66440 (destructor).
  //
  // The class has exactly one virtual slot - the destructor - so the hull
  // construction is not reachable through the vtable: it is reached by
  // address, the way the free functions are.
  //
  // The constructor is thiscall: ECX = `this`, five stack arguments -
  // iVertexQuantity, akVertex, fEpsilon, bOwner, eQueryType - and `RET 0x14`.
  // The five arguments are the WM3.8 order, established from the call to the
  // base ConvexHull<float> constructor at 0x00A6BBC0, which takes
  // (iVertexQuantity, fEpsilon, bOwner, eQueryType) and stores them at
  // [this+0x08], [this+0x18], [this+0x1C] and [this+0x04]; the derived
  // constructor hands it its own arguments 1, 3, 4 and 5 and keeps argument 2
  // (the vertex array) for itself, storing it at [this+0x20].
  //
  // The destructor is the class's own, not the vtable's scalar deleting
  // destructor (that one is at 0x00A665D0): ECX = `this`, no stack
  // arguments, a plain `RET`. It frees m_akSVertex, m_pkQuery and the hull's
  // triangles, so calling it after the comparison keeps the run's memory flat.
  void ConvexHull3Ctor(void* self, int n, const float* verts, float eps,
                       u32 owner, u32 type);
  void ConvexHull3Dtor(void* self);

  // ---- Wm3::Query3<float>::ToPlane ----------------------------------------
  // Retail 0x00A55CE0 - the Query3<float> class vtable 0x00D71F38's +0x08
  // slot. The class identity is the RTTI name behind that vtable: the
  // Complete Object Locator at 0x00E5B538 names the type descriptor
  // '.?AV?$Query3@M@Wm3@@'.
  //
  // The retail's ConvexHull3<float> constructor builds this class inline
  // (0x00A67433..0x00A67442: the vtable pointer at [this+0x00], the vertex
  // count at [this+0x04] and the vertex array at [this+0x08]), so there is
  // no separate constructor to call.
  //
  // ECX = `this`, four stack arguments - the point's address and the three
  // vertex indices - and `RET 0x10`. The body computes the determinant of
  // (P - V[iV0], V[iV1] - V[iV0], V[iV2] - V[iV0]) through the shared
  // Det3 helper at 0x00A54050 and returns its sign: +1, -1 or 0, in EAX.
  int Query3ToPlane(const void* self, const float* point, int iV0, int iV1,
                    int iV2);

  // Retail 0x00A7E230, Wm3::DistSegment3Segment3<float>::Get(). Dispatches the
  // vtable slot +0x18 (GetSquared), takes the CRT's square root at 0x00A91000
  // and narrows it back to float through a stack slot. ECX = `this`, no stack
  // arguments, the float in ST(0).
  float DistSegment3Segment3Get(const void* self);

  // ---- Wm3::DistLine3Segment3<float> ---------------------------------------
  // The class the retail does emit and this sweep has not covered yet: the
  // RTTI scan of the retail's .rdata (every Complete Object Locator whose
  // type descriptor names a Wm3 class) lists five derived Distance classes -
  // DistVector3Box3, DistVector3Segment3, DistVector2Box2,
  // DistSegment3Segment3 and DistLine3Segment3 - and this is the fifth.
  //
  // The vtable is 0x00D722D4: the Complete Object Locator at 0x00E5BEE0
  // carries the type descriptor '.?AV?$DistLine3Segment3@M@Wm3@@', and the
  // constructor at 0x00A80900 writes the vtable pointer. The slots are the
  // base's layout: +0x08 is the derived Get(fT,v0,v1), +0x0C the derived
  // Get(), +0x14 the derived GetSquared(fT,v0,v1) and +0x18 the derived
  // GetSquared().
  //
  // The constructor is the same thiscall shape as the other distance
  // constructors: ECX = `this`, the line and the segment on the stack, stored
  // at [this+0x34] and [this+0x38], `RET 8`. The object needs 0x44 bytes.
  void DistLine3Segment3Ctor(void* self, const float* line,
                             const float* segment);

  // Retail 0x00A80DE0, the static GetSquared. ECX = `this`, no stack
  // arguments, the result in ST(0) - the body ends with FABS/FSTP/FLD, so
  // the float is the absolute value of the squared distance.
  void DistLine3Segment3GetSquared(const void* self, float* out);

  // Retail 0x00A809A0, the static Get: it dispatches the vtable slot +0x18
  // and jumps to the CRT's square root at 0x00A91000. ECX = `this`, no stack
  // arguments, the float in ST(0).
  float DistLine3Segment3Get(const void* self);

  // Retail 0x00A80A40 / 0x00A80C10, the two swept queries: Get(fT,v0,v1) and
  // GetSquared(fT,v0,v1). Thiscall with three stack arguments - fT, then the
  // pointers to the two velocities - `RET 0xC`, the float in ST(0).
  void DistLine3Segment3GetSwept(const void* self, float fT, const float* vel0,
                                  const float* vel1, float* out);
  void DistLine3Segment3GetSquaredSwept(const void* self, float fT,
                                         const float* vel0, const float* vel1,
                                         float* out);

  // ---- Wm3::IntrSegment3Capsule3<float>::Find() ---------------------------
  // Retail 0x00A6C070. ECX = `this`, no stack arguments, the bool comes back
  // in AL only. The body calls the static IntrLine3Capsule3::Find at
  // 0x00A7F920 with the segment's origin, the segment's direction, the
  // capsule and &m_afParameter, then keeps the roots whose parameter is
  // within the segment's extent.
  //
  // The members it leaves behind (read out of the object storage by the
  // target, the same way the IntrBox3Sphere3 rows read the contact point):
  //   m_iQuantity  at [this+0x14]
  //   m_akPoint[2] at [this+0x18] and [this+0x24]   (three floats each)
  //   m_afParameter[2] at [this+0x30] and [this+0x34]
  u32 IntrSegment3Capsule3Find(void* self);

  // ---- Wm3::IntrLine3Capsule3<float>::Find() - the static one ------------
  // Retail 0x00A7F920. Four pointer arguments, in this order: the line's
  // origin, the line's direction, the capsule and the afT output array.
  // The result is the number of roots in EAX; the caller removes the four
  // arguments (the retail's body leaves them on the stack).
  int IntrLine3Capsule3Find(const float* origin, const float* dir,
                            const float* capsule, float* afT);

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

  // ---- Wm3::IntrLine3Box3<float>::Find() ----------------------------------
  // Retail 0x00A508F0 - the class vtable's +0x10 slot, the static Find. The
  // layout is the one five classes agree on:
  //   [dtor][Test(fTMax)][Test()][Find(fTMax)][Find()]
  // so +0x0C is the base's swept stub and +0x10 is this one. The class vtable
  // is 0x00D71EF4, whose +0x08 slot is 0x00A4FE60 - the Test that is already
  // a row - which pins the layout.
  //
  // ECX = `this`, no stack arguments, the bool comes back in AL only: the body
  // tail-calls the shared DoClipping at 0x00A50220, whose last three
  // instructions are `XOR EAX,EAX; CMP [ECX],EAX; SETNE AL`.
  //
  // The retail's Find passes fT0 = -MAX_REAL and fT1 = +MAX_REAL - the float
  // 0x7F7FFFFF at 0x00D71C00 - and bSolid = 1 to that DoClipping, which is
  // what the rebuilt Find does as well (`true`). The README's open question
  // about the bSolid flag is answered by the instruction sequence, not by a
  // measurement: the flag is pushed as the literal 1.
  //
  // The members it leaves behind, read out of the object storage the same way
  // the IntrBox3Sphere3 rows read the contact point:
  //   m_iIntersectionType at [this+0x08]
  //   m_iQuantity        at [this+0x14]
  //   m_akPoint[2]       at [this+0x18] and [this+0x24]
  u32 IntrLine3Box3Find(void* self);

  // ---- Wm3::IntrSegment3Capsule3<float>::GetPoint / GetParameter ----------
  // Retail 0x00A6BDF0 (GetPoint) and 0x00A6BE00 (GetParameter).
  //
  // Both take `this` in ECX and the index as a dword stack argument, and both
  // end in `RET 4`:
  //   0x00A6BDF0  MOV EAX,[ESP+4] / LEA EAX,[EAX+EAX*2+6] /
  //               LEA EAX,[ECX+EAX*4] / RET 4
  //   0x00A6BE00  MOV EAX,[ESP+4] / FLD dword ptr [ECX+EAX*4+0x30] / RET 4
  //
  // GetPoint therefore returns &m_akPoint[index] in EAX - a pointer into the
  // object at this + 0x18 + 12*index - and GetParameter returns
  // m_afParameter[index] in ST(0). The pointer is handed back untouched and
  // the target dereferences it on both sides; the float is stored through the
  // caller's `out`.
  //
  // Neither body has a code reference in the retail image: the compiler
  // inlined both accessors at their call sites, so the out-of-line copies are
  // dead code there. The bodies themselves are unambiguous - the pointer
  // arithmetic matches the class's member layout exactly, and 0x00A6BDE0
  // (GetQuantity, `MOV EAX,[ECX+0x14]; RET`) sits between them - so they are
  // compared as rows rather than left as unverified source.
  const float* IntrSegment3Capsule3GetPoint(const void* self, u32 index);
  void IntrSegment3Capsule3GetParameter(const void* self, u32 index, float* out);

  // ---- Wm3::Distance<float,Vector3<float>>::GetDerivative ----------------
  // Retail 0x00A39110 - the base class's generic body, at the class vtable's
  // +0x1C slot. The same body is in the Vector2 instantiation at 0x00A38AF0,
  // which is what makes the reading a property of the base rather than of one
  // class.
  //
  // ECX = `this`, three stack arguments (fT, rkVelocity0, rkVelocity1),
  // `RET 0xC`, and the float in ST(0). The body reads the step at
  // [this+0x2C], calls the vtable's +0x08 slot - the swept Get - twice with
  // fT+step and fT-step, and returns the difference times [this+0x30], which
  // is m_fInvTwoDifferenceStep.
  void DistanceVector3GetDerivative(const void* self, float fT,
                                    const float* vel0, const float* vel1,
                                    float* out);

  // ---- Wm3::Distance<float,Vector3<float>>::GetDerivativeSquared ---------
  // Retail 0x00A39180 - the base class's generic body, at the class vtable's
  // +0x20 slot (the Vector2 instantiation is 0x00A38B60). Same ABI as
  // GetDerivative: ECX = `this`, three stack arguments, `RET 0xC`, the float
  // in ST(0). The body calls the +0x08 slot and then the +0x1C slot and
  // doubles the result, which is 2*d*d'.
  void DistanceVector3GetDerivativeSquared(const void* self, float fT,
                                            const float* vel0, const float* vel1,
                                            float* out);

  // ---- Wm3::IntrSegment3Box3<float>::Test() -------------------------------
  // Retail 0x00A46440 - the class vtable's +0x08 slot, the same slot the
  // already-wired IntrLine3Box3::Test occupies. ECX = `this`, no stack
  // arguments, the bool in AL.
  u32 IntrSegment3Box3Test(const void* self);

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

  // ---- Wm3::ContAlignedBox<float>, the 2D one -----------------------------
  // Retail 0x00A685F0. The same shape as the 3D one (0x00A3AE40): a free
  // function that returns a Box2 **by value**, so the hidden return pointer
  // is the first stack argument; cdecl, the callee leaves its arguments and
  // the caller removes them (the body ends in `ADD ESP,0x10` / `RET`).
  //
  // The stack is (Box2* out, int iQuantity, const Vector2* akPoint, const
  // unsigned char* akMask) - four arguments, the fourth being the `abValid`
  // mask, exactly as in the 3D one. The body writes the 2D identity first
  // (the centre 0,0, the axes (1,0) and (0,1) and the extents 1,1) from the
  // constants at 0x00F3DBE0 - the shape match that found it - and only then
  // computes the extremes. `out` receives eight floats: the centre (two),
  // the two axes (four) and the two extents (two), in the rebuild's layout.
  void ContAlignedBox2(void* out, int quantity, const float* points);

  // The same retail entry point, but the fourth stack argument - the
  // `abValid` mask - is passed through instead of being a literal zero. The
  // retail tests that argument a byte at a time (0x00A688B4:
  // cmp byte ptr [ecx+esi],0), so a nonzero byte means "keep this point" and
  // a null mask selects the unmasked path.
  void ContAlignedBox2Mask(void* out, int quantity, const float* points,
                           const unsigned char* mask);

  // ---- Wm3::IntrBox2Circle2<float>::TestVertexRegion ---------------------
  // Retail 0x00A4E6C0. A thiscall member function with eight stack arguments
  // - fCx, fCy, fVx, fVy, fEx, fEy, &rfIx, &rfIy - and the callee removes
  // them: the body ends in `RET 0x20`, eight dwords. ECX is `this`.
  //
  // The retail's Find calls it out of line (0x00A4F67D, 0x00A4F815 and
  // 0x00A4F8AF), so the helper has a body of its own in the retail image.
  // The argument order is the Wild Magic one: the body computes fCx+fEx and
  // fCy+fEy first (0x00A4E6C3 loads the first argument and 0x00A4E6C8 the
  // fifth, and the two sums are the distances to the corner the helper works
  // in), and then their squares minus the radius squared - the vertex
  // region's fDiff.
  int IntrBox2Circle2TestVertexRegion(void* self, float fCx, float fCy,
                                      float fVx, float fVy, float fEx, float fEy,
                                      float* pix, float* piy);

  // ---- Wm3::IntrBox2Circle2<float>::TestEdgeRegion -----------------------
  // Retail 0x00A4EB60. The same shape as TestVertexRegion: eight stack
  // arguments, `RET 0x20`, ECX is `this`. The retail's Find calls it out of
  // line too (0x00A4F75E, 0x00A4F7D2 and 0x00A4F866). This body computes
  // fCx+fEx and adds the radius to it - the edge region's fXSignedDist -
  // and answers -1 when that is not negative.
  int IntrBox2Circle2TestEdgeRegion(void* self, float fCx, float fCy,
                                    float fVx, float fVy, float fEx, float fEy,
                                    float* pix, float* piy);

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

  // Retail 0x00A458C0, Wm3::DistVector3Box3<float>::Get(fT,v0,v1) - the
  // class vtable 0x00D71D8C's +0x08 slot.
  void DistVector3Box3GetSwept(const void* self, float fT, const float* vel0,
                               const float* vel1, float* out);

  // Retail 0x00A45A60, Wm3::DistVector3Box3<float>::GetSquared(fT,v0,v1) -
  // the same vtable's +0x14 slot.
  void DistVector3Box3GetSquaredSwept(const void* self, float fT,
                                       const float* vel0, const float* vel1,
                                       float* out);

  // Retail 0x00A6C730, Wm3::DistVector2Box2<float>::Get(fT,v0,v1) - the
  // class vtable 0x00D720C4's +0x08 slot. The velocities are Vector2 here,
  // so the two velocity arguments are two floats each.
  void DistVector2Box2GetSwept(const void* self, float fT, const float* vel0,
                               const float* vel1, float* out);

  // Retail 0x00A6C880, Wm3::DistVector2Box2<float>::GetSquared(fT,v0,v1) -
  // the same vtable's +0x14 slot.
  void DistVector2Box2GetSquaredSwept(const void* self, float fT,
                                       const float* vel0, const float* vel1,
                                       float* out);

  // Retail 0x00A7E2A0, Wm3::DistSegment3Segment3<float>::Get(fT,v0,v1) - the
  // class vtable 0x00D72254's +0x08 slot.
  void DistSegment3Segment3GetSwept(const void* self, float fT,
                                    const float* vel0, const float* vel1,
                                    float* out);

  // Retail 0x00A7E480, Wm3::DistSegment3Segment3<float>::GetSquared(fT,v0,v1)
  // - the same vtable's +0x14 slot.
  void DistSegment3Segment3GetSquaredSwept(const void* self, float fT,
                                            const float* vel0,
                                            const float* vel1, float* out);

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

  // ---- the base class's window queries -----------------------------------
  //
  // The base class's *own* vftable is at 0x00D71C54 - the base constructor
  // at 0x00A38C40 writes that pointer into the object
  // (`C7 01 54 1C D7 00`) - and its RTTI type descriptor names
  // ".?AV?$Distance@MV?$Vector3@M@Wm3@@@Wm3@@", i.e.
  // Wm3::Distance<float,Vector3f>.  Reading the slots from *there* - not
  // from a derived class's table - gives the base's own layout:
  //
  //   +0x00  0x00A39E60  the destructor
  //   +0x04  0x00A38C50  Get(fTMin,fTMax,rkVelocity0,rkVelocity1)
  //   +0x08  0x00A82547  Get(fT,rkVelocity0,rkVelocity1)      - pure virtual
  //   +0x0C  0x00A82547  Get()                                - pure virtual
  //   +0x10  0x00A38EB0  GetSquared(fTMin,fTMax,rkVelocity0,rkVelocity1)
  //   +0x14  0x00A82547  GetSquared(fT,rkVelocity0,rkVelocity1) - pure virtual
  //   +0x18  0x00A82547  GetSquared()                          - pure virtual
  //   +0x1C  0x00A39110  GetDerivative
  //   +0x20  0x00A39180  GetDerivativeSquared
  //
  // So the base has no generic three-argument GetSquared at all: the retail
  // declares it pure virtual, exactly as the rebuilt source does, and the
  // slots +0x08 and +0x14 of a *derived* class's table are the derived
  // class's own swept Get and GetSquared.
  //
  // The two window bodies are thiscall: ECX is `this`, four stack arguments
  // - fTMin, fTMax, &rkVelocity0, &rkVelocity1 - and `RET 0x10`, so the
  // callee pops them.  The float comes back in ST(0).
  //
  // The body dispatches through the object's vtable: the +0x08 slot for the
  // distance and the +0x1C slot for the derivative.  So the row has to run on
  // an object of a *derived* class - a DistVector3Segment3f - for the
  // dispatch to reach the derived class's own swept Get.
  void DistVector3Segment3WindowGet(const void* self, float fTMin, float fTMax,
                                    const float* vel0, const float* vel1,
                                    float* out);
  void DistVector3Segment3WindowGetSquared(const void* self, float fTMin,
                                            float fTMax, const float* vel0,
                                            const float* vel1, float* out);

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
  float DistanceGetDifferenceStep(const void* self);

  // ---- Wm3::Distance<double,Vector3d> and ::Distance<double,Vector2d> -----
  //
  // The double instantiation of the same base class.  The retail emits it as
  // a separate set of bodies, and the RTTI behind the two class vtables says
  // which is which: the Complete Object Locator in front of 0x00D71CA4 names
  // '.?AV?$Distance@N?$Vector3@N@Wm3@@@Wm3@@' and the one in front of
  // 0x00D71C7C names the Vector2 one ('N' is MSVC's mangling for double).
  //
  // The bodies are x87 code, and the engine runs under `_PC_24`, so every
  // arithmetic result the retail computes is a 24-bit significand with the
  // extended exponent - which is why the rebuild's double path needs the
  // Wm3Detail emulation and not plain binary64.
  //
  //   base ctor           0x00A39FA0 (Vector3)  0x00A39F60 (Vector2)
  //   SetDifferenceStep   0x00A39DB0 (Vector3)  0x00A397C0 (Vector2)
  //   GetDifferenceStep   0x00A39DE0 (Vector3)  0x00A397F0 (Vector2)
  //
  // The base constructor is thiscall with `this` in ECX and no stack
  // arguments; it installs the class's own vtable, MaximumIterations = 8 at
  // [this+0x08], the zero threshold at [this+0x10], the contact time at
  // [this+0x18], the two flags at [this+0x50] / [this+0x51] (Vector3) or
  // [this+0x40] / [this+0x41] (Vector2), the difference step 1e-3 at
  // [this+0x58] (Vector3) or [this+0x48] (Vector2) and its reciprocal 500.0
  // at [this+0x60] (Vector3) or [this+0x50] (Vector2).
  //
  // SetDifferenceStep is thiscall with one double argument on the stack, and
  // the callee pops it (`RET 8`): a step that is not positive is replaced by
  // the default 1e-3, and the reciprocal 0.5/step is computed on the x87 and
  // stored next to the step.
  //
  // GetDifferenceStep is thiscall with no stack arguments; the step comes
  // back in ST(0).
  void Distance3dCtor(void* self);
  void Distance3dSetDifferenceStep(void* self, double step);
  double Distance3dGetDifferenceStep(const void* self);
  void Distance2dCtor(void* self);
  void Distance2dSetDifferenceStep(void* self, double step);
  double Distance2dGetDifferenceStep(const void* self);

  // ---- the base class's own four bodies, for the two double bases ---------
  //
  // The two base classes have their own vtables, and the four bodies below
  // are the base's own members - the ones the derived classes do *not*
  // override:
  //
  //   Distance<double,Vector3d>  vtable 0x00D71CA4
  //     +0x04 0x00A39860  Get(fTMin,fTMax,&v0,&v1)          - the window
  //     +0x10 0x00A39AB0  GetSquared(fTMin,fTMax,&v0,&v1)   - the window
  //     +0x1C 0x00A39D00  GetDerivative(fT,&v0,&v1)
  //     +0x20 0x00A39D60  GetDerivativeSquared(fT,&v0,&v1)
  //
  //   Distance<double,Vector2d>  vtable 0x00D71C7C
  //     +0x04 0x00A39270  Get(fTMin,fTMax,&v0,&v1)
  //     +0x10 0x00A394C0  GetSquared(fTMin,fTMax,&v0,&v1)
  //     +0x1C 0x00A39710  GetDerivative(fT,&v0,&v1)
  //     +0x20 0x00A39770  GetDerivativeSquared(fT,&v0,&v1)
  //
  // The slots in between are pure virtual in the retail - 0x00A82547, a stub
  // that aborts - so a row that landed there would be a crash and not a
  // mismatch.  The base's own +0x08 / +0x0C / +0x14 / +0x18 are exactly
  // those four stubs.
  //
  // The two window bodies are thiscall with `this` in ECX and four stack
  // arguments - fTMin, fTMax, &rkVelocity0, &rkVelocity1, the two doubles
  // first - and `RET 0x18`, so the callee pops all 24 bytes.  The two
  // derivative bodies take three stack arguments - the double fT and the two
  // velocity pointers - and `RET 0x10`.  Every one of them returns the
  // double in ST(0).
  //
  // The window bodies dispatch through the object's vtable - the +0x08 slot
  // for the distance and the +0x1C slot for the derivative - so the row has
  // to run on an object of a *derived* class for the dispatch to reach the
  // derived class's own swept Get.  The derivative bodies do the same.
  void Distance3dWindowGet(const void* self, double fTMin, double fTMax,
                           const double* vel0, const double* vel1, double* out);
  void Distance3dWindowGetSquared(const void* self, double fTMin, double fTMax,
                                  const double* vel0, const double* vel1,
                                  double* out);
  void Distance3dGetDerivative(const void* self, double fT, const double* vel0,
                               const double* vel1, double* out);
  void Distance3dGetDerivativeSquared(const void* self, double fT,
                                      const double* vel0, const double* vel1,
                                      double* out);
  void Distance2dWindowGet(const void* self, double fTMin, double fTMax,
                           const double* vel0, const double* vel1, double* out);
  void Distance2dWindowGetSquared(const void* self, double fTMin, double fTMax,
                                  const double* vel0, const double* vel1,
                                  double* out);
  void Distance2dGetDerivative(const void* self, double fT, const double* vel0,
                               const double* vel1, double* out);
  void Distance2dGetDerivativeSquared(const void* self, double fT,
                                      const double* vel0, const double* vel1,
                                      double* out);

  // ---- Wm3::DistVector3Box3<double> ---------------------------------------
  //
  // The double instantiation of the class the float rows already compare.
  // The class vtable is 0x00D71DB4 - the RTTI slot in front of it holds the
  // Complete Object Locator whose type descriptor names
  // '.?AV?$DistVector3Box3@N@Wm3@@' ('N' is MSVC's mangling for double).
  //
  //   constructor  0x00A45780  ECX = `this`, the vector and the box pointers
  //                            on the stack, stored at [this+0x68] and
  //                            [this+0x6C]; `RET 8`.
  //   GetSquared   0x00A460D0  the class vtable's +0x18 slot. ECX = `this`,
  //                            no stack arguments, the double in ST(0).
  //   Get          0x00A45800  the class vtable's +0x0C slot: it dispatches
  //                            +0x18 and jumps to the CRT's square root at
  //                            0x00A91000. The double in ST(0).
  void DistVector3Box3dCtor(void* self, const double* vec, const double* box);
  double DistVector3Box3dGetSquared(const void* self);
  double DistVector3Box3dGet(const void* self);

  // The two swept bodies of the same class:
  //   Get(fT,v0,v1)         0x00A45E90  the class vtable's +0x08 slot.
  //   GetSquared(fT,v0,v1)  0x00A45FB0  the class vtable's +0x14 slot.
  // Both are thiscall with three stack arguments - the double fT and the
  // two velocity pointers - and `RET 0x10`; the double comes back in ST(0).
  void DistVector3Box3dGetSwept(const void* self, double fT, const double* vel0,
                                 const double* vel1, double* out);
  void DistVector3Box3dGetSquaredSwept(const void* self, double fT,
                                        const double* vel0, const double* vel1,
                                        double* out);

  // ---- Wm3::DistVector3Segment3<double> ------------------------------------
  //
  // The double instantiation of the class the float rows already compare.
  // The class vtable is 0x00D71E94 - the RTTI slot in front of it holds the
  // Complete Object Locator whose type descriptor names
  // '.?AV?$DistVector3Segment3@N@Wm3@@' - and the constructor at 0x00A48080
  // writes it (`C7 06 94 1E D7 00`).
  //
  //   constructor  0x00A48080  ECX = `this`, the vector and the segment
  //                            pointers on the stack, stored at [this+0x68]
  //                            and [this+0x6C]; `RET 8`.
  //   GetSquared   0x00A48910  the class vtable's +0x18 slot. ECX = `this`,
  //                            no stack arguments, the double in ST(0).
  //   Get          0x00A48100  the class vtable's +0x0C slot: it dispatches
  //                            +0x18 and jumps to the CRT's square root at
  //                            0x00A91000. The double in ST(0).
  //   Get(fT,v0,v1)          0x00A486B0  the class vtable's +0x08 slot.
  //   GetSquared(fT,v0,v1)   0x00A487E0  the class vtable's +0x14 slot.
  //
  // The two swept bodies are thiscall with three stack arguments - the
  // double fT and the two velocity pointers - and `RET 0x10` (the body ends
  // in `C2 10 00`).  The double comes back in ST(0).
  void DistVector3Segment3dCtor(void* self, const double* vec,
                                const double* segment);
  double DistVector3Segment3dGetSquared(const void* self);
  double DistVector3Segment3dGet(const void* self);
  void DistVector3Segment3dGetSwept(const void* self, double fT,
                                     const double* vel0, const double* vel1,
                                     double* out);
  void DistVector3Segment3dGetSquaredSwept(const void* self, double fT,
                                            const double* vel0,
                                            const double* vel1, double* out);

  // ---- Wm3::DistVector2Box2<double> --------------------------------------
  //
  // The double instantiation of the class the float rows already compare.
  // The class vtable is 0x00D720EC - the RTTI slot in front of it holds the
  // Complete Object Locator whose type descriptor names
  // '.?AV?$DistVector2Box2@M@Wm3@@' - and the constructor at 0x00A6C630
  // writes it.
  //
  //   constructor  0x00A6C630  ECX = `this`, the vector and the box
  //                            pointers on the stack, stored at [this+0x58]
  //                            and [this+0x5C]; `RET 8`.
  //   GetSquared   0x00A6CD70  the class vtable's +0x18 slot. ECX = `this`,
  //                            no stack arguments, the double in ST(0).
  //   Get          0x00A6C6A0  the +0x0C slot: it dispatches +0x18 and
  //                            jumps to the CRT's square root at 0x00A91000.
  //   Get(fT,v0,v1)         0x00A6CB50  the +0x08 slot.
  //   GetSquared(fT,v0,v1)  0x00A6CC60  the +0x14 slot.
  //
  // The two swept bodies are thiscall with three stack arguments - the
  // double fT and the two velocity pointers - and `RET 0x10`; the double
  // comes back in ST(0).
  void DistVector2Box2dCtor(void* self, const double* vec, const double* box);
  double DistVector2Box2dGetSquared(const void* self);
  double DistVector2Box2dGet(const void* self);
  void DistVector2Box2dGetSwept(const void* self, double fT, const double* vel0,
                                 const double* vel1, double* out);
  void DistVector2Box2dGetSquaredSwept(const void* self, double fT,
                                        const double* vel0, const double* vel1,
                                        double* out);

  // ---- Wm3::DistSegment3Segment3<double> ----------------------------------
  //
  // The double instantiation of the class the float rows already compare.
  // The class vtable is 0x00D7227C - the RTTI slot in front of it holds the
  // Complete Object Locator whose type descriptor names
  // '.?AV?$DistSegment3Segment3@N@Wm3@@' - and the constructor at 0x00A7E1E0
  // writes it.
  //
  //   constructor  0x00A7E1E0  ECX = `this`, the two segment pointers on the
  //                            stack, stored at [this+0x68] and [this+0x6C];
  //                            `RET 8`.
  //   GetSquared   0x00A7F100  the class vtable's +0x18 slot. ECX = `this`,
  //                            no stack arguments, the double in ST(0).
  //   Get          0x00A7E250  the +0x0C slot: it dispatches +0x18 and
  //                            jumps to the CRT's square root at 0x00A91000.
  //   Get(fT,v0,v1)         0x00A7EE60  the +0x08 slot.
  //   GetSquared(fT,v0,v1)  0x00A7EFB0  the +0x14 slot.
  //
  // The two swept bodies are thiscall with three stack arguments - the
  // double fT and the two velocity pointers - and `RET 0x10`; the double
  // comes back in ST(0).
  void DistSegment3Segment3dCtor(void* self, const double* seg0,
                                 const double* seg1);
  double DistSegment3Segment3dGetSquared(const void* self);
  double DistSegment3Segment3dGet(const void* self);
  void DistSegment3Segment3dGetSwept(const void* self, double fT,
                                      const double* vel0, const double* vel1,
                                      double* out);
  void DistSegment3Segment3dGetSquaredSwept(const void* self, double fT,
                                             const double* vel0,
                                             const double* vel1, double* out);

  // ---- Wm3::DistLine3Segment3<double> -------------------------------------
  //
  // The double instantiation of the class the float rows already compare.
  // The class vtable is 0x00D722FC - the RTTI slot in front of it holds the
  // Complete Object Locator whose type descriptor names
  // '.?AV?$DistLine3Segment3@N@Wm3@@' - and the constructor at 0x00A80950
  // writes it.
  //
  //   constructor  0x00A80950  ECX = `this`, the line and the segment
  //                            pointers on the stack, stored at [this+0x68]
  //                            and [this+0x6C]; `RET 8`.
  //   GetSquared   0x00A81330  the class vtable's +0x18 slot. ECX = `this`,
  //                            no stack arguments, the double in ST(0).
  //   Get          0x00A809C0  the +0x0C slot: it dispatches +0x18 and
  //                            jumps to the CRT's square root at 0x00A91000.
  //   Get(fT,v0,v1)         0x00A810B0  the +0x08 slot.
  //   GetSquared(fT,v0,v1)  0x00A811F0  the +0x14 slot.
  //
  // The two swept bodies are thiscall with three stack arguments - the
  // double fT and the two velocity pointers - and `RET 0x10`; the double
  // comes back in ST(0).
  void DistLine3Segment3dCtor(void* self, const double* line,
                              const double* segment);
  double DistLine3Segment3dGetSquared(const void* self);
  double DistLine3Segment3dGet(const void* self);
  void DistLine3Segment3dGetSwept(const void* self, double fT,
                                   const double* vel0, const double* vel1,
                                   double* out);
  void DistLine3Segment3dGetSquaredSwept(const void* self, double fT,
                                          const double* vel0,
                                          const double* vel1, double* out);

  // ---- Wm3::IntrBox2Circle2<double> ---------------------------------------
  //
  // The double instantiation of the class the float rows already compare.
  // The class vtable is 0x00D71EDC - the RTTI slot in front of it holds the
  // Complete Object Locator - and the constructor at 0x00A4EE20 writes it.
  //
  //   constructor  0x00A4EE20  ECX = `this`, the box and the circle pointers
  //                            on the stack, stored at [this+0x18] and
  //                            [this+0x1C]; `RET 8`.
  //   Test          0x00A4EE50  the class vtable's +0x08 slot.
  //                            ECX = `this`, no stack arguments, the bool in
  //                            EAX.
  void IntrBox2Circle2dCtor(void* self, const double* box, const double* circle);
  u32 IntrBox2Circle2dTest(const void* self);

  // ---- Wm3::IntrSegment3Capsule3<double> -----------------------------------
  //
  // The double instantiation of the class the float rows already compare.
  // The class vtable is 0x00D720AC, read out of the image, and the
  // constructor at 0x00A6BF50 writes the pointer.
  //
  //   constructor  0x00A6BF50  ECX = `this`, the segment and the capsule
  //                            pointers on the stack, stored at [this+0x18]
  //                            and [this+0x1C]; `RET 8`.
  //   Test          0x00A6BF80  the class vtable's +0x08 slot - ECX = `this`,
  //                            no stack arguments, the bool in EAX.
  //   Find          0x00A6C3C0  the class vtable's +0x0C slot - ECX = `this`,
  //                            no stack arguments, the bool in AL. The body
  //                            calls the static Find (0x00A80080) and then
  //                            keeps the roots the query segment's extent
  //                            admits, computing their points in x87.
  void IntrSegment3Capsule3dCtor(void* self, const double* segment,
                                 const double* capsule);
  u32 IntrSegment3Capsule3dTest(const void* self);
  u32 IntrSegment3Capsule3dFind(const void* self);

  // ---- Wm3::IntrBox3Sphere3<double> ---------------------------------------
  //
  // The double instantiation of the class the float rows already compare.
  // The class vtable is 0x00D71D14, read out of the image, and the
  // constructor at 0x00A42590 writes the pointer.
  //
  //   constructor  0x00A42590  ECX = `this`, the box and the sphere pointers
  //                            on the stack, stored at [this+0x18] and
  //                            [this+0x1C]; `RET 8`.
  //   Test          0x00A425C0  the class vtable's +0x08 slot - ECX = `this`,
  //                            no stack arguments, the bool in EAX.
  void IntrBox3Sphere3dCtor(void* self, const double* box, const double* sphere);
  u32 IntrBox3Sphere3dTest(const void* self);

  // ---- Wm3::IntrBox2Box2<double> ----------------------------------------------
  //
  // The double instantiation of the class the float rows already compare.
  // The class vtable is 0x00D71E54, read out of the image, and the constructor
  // at 0x00A47C40 writes the pointer.
  //
  //   constructor  0x00A47C40  ECX = `this`, the shapes on the stack.
  //   Test          0x00A47F00  the class vtable's +0x08 slot - ECX = `this`, no
  //                            stack arguments, the bool in AL.
  void IntrBox2Box2dCtor(void* self, const double* a, const double* b);
  u32 IntrBox2Box2dTest(const void* self);

  // ---- Wm3::IntrLine3Box3<double> ----------------------------------------------
  //
  // The double instantiation of the class the float rows already compare.
  // The class vtable is 0x00D71F0C, read out of the image, and the constructor
  // at 0x00A50060 writes the pointer.
  //
  //   constructor  0x00A50060  ECX = `this`, the shapes on the stack.
  //   Test          0x00A50090  the class vtable's +0x08 slot - ECX = `this`, no
  //                            stack arguments, the bool in AL.
  void IntrLine3Box3dCtor(void* self, const double* a, const double* b);
  u32 IntrLine3Box3dTest(const void* self);
  // Find is the class vtable's +0x0C slot, 0x00A50940 - a thin wrapper that
  // calls the shared double DoClipping (0x00A505F0) with the parameter range
  // [-MAX_REAL, +MAX_REAL] and the solid flag set.  ECX = `this`, no stack
  // arguments, the bool in AL.
  u32 IntrLine3Box3dFind(const void* self);

  // ---- Wm3::IntrSegment3Box3<double> ----------------------------------------------
  //
  // The double instantiation of the class the float rows already compare.
  // The class vtable is 0x00D71DF4, read out of the image, and the constructor
  // at 0x00A463D0 writes the pointer.
  //
  //   constructor  0x00A463D0  ECX = `this`, the shapes on the stack.
  //   Test          0x00A46720  the class vtable's +0x08 slot - ECX = `this`, no
  //                            stack arguments, the bool in AL.
  void IntrSegment3Box3dCtor(void* self, const double* a, const double* b, u32 flag);
  u32 IntrSegment3Box3dTest(const void* self);
  // Find is the class vtable's +0x0C slot, 0x00A46330 - a thin wrapper that
  // calls the shared double DoClipping (0x00A505F0) with the parameter range
  // [-Extent, +Extent] and the object's own solid flag.  ECX = `this`, no
  // stack arguments, the bool in AL.
  u32 IntrSegment3Box3dFind(const void* self);

  // ---- Wm3::IntrSegment3Sphere3<double> ----------------------------------------------
  //
  // The double instantiation of the class the float rows already compare.
  // The class vtable is 0x00D71E24, read out of the image, and the constructor
  // at 0x00A46A50 writes the pointer.
  //
  //   constructor  0x00A46A50  ECX = `this`, the shapes on the stack.
  //   Test          0x00A475E0  the class vtable's +0x08 slot - ECX = `this`, no
  //                            stack arguments, the bool in AL.
  void IntrSegment3Sphere3dCtor(void* self, const double* a, const double* b);
  u32 IntrSegment3Sphere3dTest(const void* self);
  // Find is the class vtable's +0x0C slot, 0x00A479B0 - the *static* Find
  // (the dynamic one, 0x00A47690, is the other slot).  ECX = `this`, no stack
  // arguments, the bool in AL.
  u32 IntrSegment3Sphere3dFind(const void* self);
  // The dynamic Find, 0x00A47690 - the class vtable's +0x0C slot.  ECX =
  // `this`, three stack arguments - the time limit (a 64-bit double), the
  // two velocity pointers - and `RET 0x10`, so the callee pops all sixteen
  // bytes.  The body writes the contact time at [this+0x08], the
  // intersection type at [this+0x10] and the contact point at [this+0x30];
  // the bool is in AL.
  u32 IntrSegment3Sphere3dFindDynamic(void* self, double fTMax,
                                      const double* vel0, const double* vel1);

  // ---- Wm3::ConvexHull1<float> and Wm3::ConvexHull2<float> ----------------
  //
  // Both classes have exactly one virtual slot - the destructor - so their
  // construction is not reachable through the vtable and is reached by
  // address, the way the free functions are.
  //
  // The class vtables are 0x00D72114 (ConvexHull1<float>) and 0x00D72124
  // (ConvexHull2<float>): the RTTI slots at 0x00D72110 and 0x00D72120 hold
  // the Complete Object Locators whose type descriptors name
  // '.?AV?$ConvexHull1@M@Wm3@@' and '.?AV?$ConvexHull2@M@Wm3@@'.
  //
  // The constructors:
  //
  //   ConvexHull1<float>  0x00A74560  (the vtable pointer is written at
  //                                   0x00A745C8, and the base
  //                                   ConvexHull<float> constructor is called
  //                                   at 0x00A745A5)
  //   ConvexHull2<float>  0x00A78E30  (the vtable pointer is written at
  //                                   0x00A78E7F, and the base
  //                                   ConvexHull<float> constructor is called
  //                                   at 0x00A78E76)
  //
  // Both are thiscall: ECX = `this`, five stack arguments - iVertexQuantity,
  // the vertex array, fEpsilon, bOwner and eQueryType - and `RET 0x14`. The
  // argument order is the base ConvexHull<float> constructor's call order
  // (0x00A6BBC0), which takes (iVertexQuantity, fEpsilon, bOwner,
  // eQueryType): the derived constructor hands the base its own arguments 1,
  // 3, 4 and 5 and keeps argument 2 for itself.
  //
  // The destructors are the classes' own, not the vtable's scalar deleting
  // destructors (those are at 0x00A71F20 and 0x00A74DD0):
  //
  //   ConvexHull1<float>  0x00A71EA0
  //   ConvexHull2<float>  0x00A74970
  //
  // Both are thiscall: ECX = `this`, no stack arguments, a plain `RET`. They
  // free the hull's own allocations, so calling them after the comparison
  // keeps the run's memory flat.
  void ConvexHull1Ctor(void* self, int n, const float* values, float eps,
                       u32 owner, u32 type);
  void ConvexHull1Dtor(void* self);
  void ConvexHull2Ctor(void* self, int n, const float* verts, float eps,
                       u32 owner, u32 type);
  void ConvexHull2Dtor(void* self);

  // ---- Wm3::Query3<float> -------------------------------------------------
  //
  // The class vtable is 0x00D71F38. The RTTI slot - the dword that precedes
  // the vtable - is at 0x00D71F34 and holds the Complete Object Locator
  // 0x00E5B538, whose type descriptor names '.?AV?$Query3@M@Wm3@@'. 'M' is
  // MSVC's mangling for float, so this is the float instantiation; the double
  // one has its own vtable at 0x00D71F58.
  //
  // The slot order, read off the class's *own* vtable (never from a derived
  // one) and confirmed by each body:
  //
  //   +0x00 0x00A53F40  the destructor
  //   +0x04 0x00A53090  GetType() - the body is `mov eax, 3`, the
  //                      Query::QT_REAL enumerator
  //   +0x08 0x00A55CE0  ToPlane(const Vector3<float>&, int, int, int)
  //   +0x0C 0x00A530A0  ToPlane(int, int, int, int)
  //   +0x10 0x00A530D0  ToTetrahedron(const Vector3<float>&, int, int, int, int)
  //   +0x14 0x00A53180  ToTetrahedron(int, int, int, int, int)
  //   +0x18 0x00A55DF0  ToCircumsphere(const Vector3<float>&, int, int, int, int)
  //   +0x1C 0x00A531B0  ToCircumsphere(int, int, int, int, int)
  //
  // The point-taking overload comes *before* the index-taking one in the
  // retail's table - the opposite of the WM4 declaration order the rebuild
  // inherits - so both sides are called by address and never by slot.
  //
  // The retail's ConvexHull3<float> constructor builds this class inline
  // (0x00A6743B writes the vtable pointer, the vertex count follows at
  // [this+0x04] and the vertex array at [this+0x08]), so there is no
  // constructor to call: the object is fabricated with those three fields.
  //
  // Every one of these is thiscall: ECX = `this`, the arguments on the stack,
  // and the callee pops them (`RET 0x10` for four arguments, `RET 0x14` for
  // five). The sign comes back in EAX.
  int Query3ToPlaneIndex(const void* self, int i, int iV0, int iV1, int iV2);
  int Query3ToTetrahedron(const void* self, const float* point, int iV0,
                           int iV1, int iV2, int iV3);
  int Query3ToTetrahedronIndex(const void* self, int i, int iV0, int iV1,
                                int iV2, int iV3);
  int Query3ToCircumsphere(const void* self, const float* point, int iV0,
                            int iV1, int iV2, int iV3);
  int Query3ToCircumsphereIndex(const void* self, int i, int iV0, int iV1,
                                int iV2, int iV3);

  // ---- Wm3::Query2<float> -------------------------------------------------
  //
  // The class vtable is 0x00D72134; the RTTI slot at 0x00D72130 holds the
  // Complete Object Locator 0x00E5BB38, whose type descriptor names
  // '.?AV?$Query2@M@Wm3@@'. The slot order:
  //
  //   +0x00 0x00A756A0  the destructor
  //   +0x04 0x00A74E60  GetType()
  //   +0x08 0x00A75C00  ToLine(const Vector2<float>&, int, int)
  //   +0x0C 0x00A74E70  ToLine(int, int, int)
  //   +0x10 0x00A74EA0  ToTriangle(const Vector2<float>&, int, int, int)
  //   +0x14 0x00A74F30  ToTriangle(int, int, int, int)
  //   +0x18 0x00A75C80  ToCircumcircle(const Vector2<float>&, int, int, int)
  //   +0x1C 0x00A74F60  ToCircumcircle(int, int, int, int)
  //
  // The retail's Query2<float> constructor is at 0x00A74E30 (it writes the
  // vtable pointer at [this+0x00], the vertex count at [this+0x04] and the
  // vertex array at [this+0x08] - the same three fields the Query3 object
  // has), so the object is fabricated with those three fields.
  int Query2ToLine(const void* self, const float* point, int iV0, int iV1);
  int Query2ToLineIndex(const void* self, int i, int iV0, int iV1);
  int Query2ToTriangle(const void* self, const float* point, int iV0, int iV1,
                       int iV2);
  int Query2ToTriangleIndex(const void* self, int i, int iV0, int iV1, int iV2);
  int Query2ToCircumcircle(const void* self, const float* point, int iV0,
                            int iV1, int iV2);
  int Query2ToCircumcircleIndex(const void* self, int i, int iV0, int iV1,
                                int iV2);

  // ---- Vector3<Real>::GenerateOrthonormalBasis ----------------------------
  //
  // Retail 0x00A3D150 (the double instantiation) and 0x00A3CF80 (the float
  // one).  A *static* member, so there is no `this`: four stack arguments -
  // &rkU, &rkV, &rkW and the bUnitLengthW flag - and the body ends in a
  // plain `RET`, so the callee leaves the arguments to the caller.  The
  // caller's own call site settles the order: 0x00A800B4 (the double
  // IntrLine3Capsule3::Find) pushes the flag first and the three pointers
  // after it, and the `ADD ESP,0x10` that follows the call pops all four.
  //
  // The retail writes U and V through the first two pointers and reads W
  // through the third; when bUnitLengthW is false it normalises W in place,
  // so the thunks hand it a copy of the caller's vector.
  void Vector3GenerateOrthonormalBasisd(const double* w, u32 unitLengthW,
                                        double* out);
  void Vector3GenerateOrthonormalBasisf(const float* w, u32 unitLengthW,
                                        float* out);

  // ---- Vector3<double>::Normalize ----------------------------------------
  //
  // Retail 0x00A3D0A0 - the double instantiation of the *member* Normalize.
  // `this` in ECX, no stack arguments, plain RET, and the **length** in ST(0)
  // at the ret: the epilogue leaves the length on the x87 stack (the
  // reciprocal is consumed by the three multiplications), and the degenerate
  // branch (length <= Math<double>::ZERO_TOLERANCE, the 1e-8 baked into
  // 0x00d71c10) leaves 0.0.
  //
  // The body is x87 code, so the length is a 24-bit significand with the
  // extended exponent - a binary64 length would be a different number.
  double Vector3NormalizeD(void* self);

  // ---- Wm3::Eigen<float> ---------------------------------------------------
  //
  // The retail's Eigen<float> lives in Foundation/Numerics/Wm3Eigen.cpp and
  // is reached from Foundation/Approximation/Wm3ApprGaussPointsFit3.cpp:
  // the retail's GaussPointsFit3<float> (0x00A509A0) builds an Eigen<float>
  // of size three (the constructor is 0x00A719E0), fills its nine matrix
  // elements through operator() (0x00A6D2D0), runs IncrSortEigenStuff3
  // (0x00A70DB0) and reads the eigenvalues and the eigenvectors back
  // (0x00A6CE70 and 0x00A6D350).
  //
  // The object is 0x24 bytes: m_iSize at [this+0x00], the GMatrix m_kMat at
  // [this+0x04] (its own m_aafEntry - the array of row pointers - at
  // [this+0x14]), m_afDiag at [this+0x18], m_afSubd at [this+0x1C] and the
  // m_bIsRotation flag at [this+0x20].  The constructor allocates the two
  // Real arrays and the matrix on the retail's heap, so the destructor has
  // to run after every row, or the run's memory grows without bound.
  void EigenfCtor(void* self, int size);
  void EigenfDtor(void* self);

  // Retail 0x00A6D2D0, `Real& operator() (int iRow, int iCol)`: ECX =
  // `this`, two stack arguments (the row and the column, `RET 8`), and the
  // address of the element comes back in EAX.
  float* EigenfElement(void* self, int row, int col);

  // Retail 0x00A70D70 and 0x00A70DB0, IncrSortEigenStuff2 and
  // IncrSortEigenStuff3: ECX = `this`, no stack arguments, void.
  void EigenfIncrSort2(void* self);
  void EigenfIncrSort3(void* self);

  // Retail 0x00A6CE70, `Real GetEigenvalue (int i) const`: ECX = `this`,
  // one stack argument (`RET 4`), and the float comes back in ST(0).
  float EigenfGetEigenvalue(void* self, int i);

  // Retail 0x00A6D350 and 0x00A6D3A0, `void GetEigenvector (int i,
  // Vector3<Real>& rkV)` and the Vector2 overload: ECX = `this`, two stack
  // arguments (the index and the address of the vector, `RET 8`), void.
  void EigenfGetEigenvector3(void* self, int i, float* v);
  void EigenfGetEigenvector2(void* self, int i, float* v);

  // ---- Wm3::Eigen<double> --------------------------------------------------
  //
  // The double instantiation of the same class, in the retail's WM3.8
  // shape.  Its object has the retail's layout as well: m_iSize at
  // [this+0x00], the GMatrix m_kMat at [this+0x04] (its row-pointer array
  // at [this+0x14]), m_afDiag at [this+0x18], m_afSubd at [this+0x1C] and
  // the m_bIsRotation flag at [this+0x20].  Every body is x87 code, so the
  // arithmetic is a 24-bit significand with the extended exponent.
  void EigendCtor(void* self, int size);
  void EigendDtor(void* self);

  // Retail 0x00A6EDC0, `Real& operator() (int iRow, int iCol)`: ECX =
  // `this`, two stack arguments (the row and the column, `RET 8`), and the
  // address of the element comes back in EAX.
  double* EigendElement(void* self, int row, int col);

  // Retail 0x00A71420 and 0x00A71460, IncrSortEigenStuff2 and
  // IncrSortEigenStuff3: ECX = `this`, no stack arguments, void.
  void EigendIncrSort2(void* self);
  void EigendIncrSort3(void* self);

  // Retail 0x00A6CEA0, `Real GetEigenvalue (int i) const`: ECX = `this`,
  // one stack argument (`RET 4`), and the double comes back in ST(0).
  double EigendGetEigenvalue(void* self, int i);

  // Retail 0x00A6EE40 and 0x00A6EE90, `void GetEigenvector (int i,
  // Vector3<Real>& rkV)` and the Vector2 overload: ECX = `this`, two stack
  // arguments (the index and the address of the vector, `RET 8`), void.
  void EigendGetEigenvector3(void* self, int i, double* v);
  void EigendGetEigenvector2(void* self, int i, double* v);

  // ---- Wm3::ConvexHull<double> --------------------------------------------
  //
  // The double instantiation of the hull classes.  The base class's own
  // bodies are shared by all three hull classes (they are not virtual), so
  // these three accessors are the ones the engine calls on a hull of any
  // derived type:
  //
  //   0x00A6BD30  int GetDimension () const          -> EAX
  //   0x00A6BD40  int GetSimplexQuantity () const    -> EAX
  //   0x00A6BD50  const int* GetIndices () const     -> EAX
  //
  // The retail's ConvexHull<double> has its own layout: m_eQueryType at
  // [this+0x08], m_iVertexQuantity at [this+0x0C], m_iDimension at
  // [this+0x10], m_iSimplexQuantity at [this+0x14], m_aiIndex at
  // [this+0x18], m_fEpsilon at [this+0x20] and m_bOwner at [this+0x28].
  int ConvexHulldGetDimension(void* self);
  int ConvexHulldGetSimplexQuantity(void* self);
  const int* ConvexHulldGetIndices(void* self);

  // Retail 0x00A79220, Wm3::ConvexHull2<double>'s constructor: ECX =
  // `this`, five stack arguments - the vertex quantity, the vertex array,
  // the epsilon (a double, so eight bytes), bOwner and the query type -
  // and `RET 0x18`, so the callee pops all twenty-four bytes.
  void ConvexHull2dCtor(void* self, int n, const double* verts, double eps,
                        u32 owner, u32 type);
  // Retail 0x00A74BA0, Wm3::ConvexHull2<double>'s own destructor (the
  // class's, not the vtable's scalar deleting destructor at 0x00A74DF0).
  void ConvexHull2dDtor(void* self);

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
