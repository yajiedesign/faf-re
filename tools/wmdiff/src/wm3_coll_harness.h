#pragma once

// Our side of the Wild Magic 3.8 collision/distance classes: forwards to the
// real headers, so the code exercised is the code the engine compiles.

#include <cstdint>

namespace wm3collharness
{
  using u32 = std::uint32_t;

  /// The sentinel the region helpers' intersection coordinates start from.
  /// The helpers only write the coordinates on the hit path, so both sides
  /// start from this value and compare the raw bits: a path that writes
  /// nothing leaves the sentinel on both sides, and a path that writes a
  /// different coordinate than the other side shows up as a mismatch
  /// instead of as a coincidence of uninitialised memory.
  float UndefCoord();

  /// Wm3::DistVector3Box3<float>::GetSquared.
  /// `vec` is 3 floats; `box` is 15 (Center[3], Axis[3][3], Extent[3]).
  /// Returns the raw float, not its bits, because retail leaves it in ST(0).
  float DistVector3Box3GetSquared(const float* vec, const float* box);

  /// Wm3::DistVector3Segment3<float>::GetSquared.
  /// `vec` is 3 floats; `segment` is 7 (Origin[3], Direction[3], Extent).
  float DistVector3Segment3GetSquared(const float* vec, const float* segment);

  /// Wm3::DistVector3Segment3<float>::Get(). Same inputs as GetSquared: retail
  /// dispatches GetSquared virtually and takes its sqrt, and the rebuilt
  /// `Get()` is written as exactly that composition.
  float DistVector3Segment3Get(const float* vec, const float* segment);

  /// Wm3::DistVector3Segment3<float>::Get(). Same inputs as GetSquared: retail
  /// dispatches GetSquared virtually and takes its sqrt, and the rebuilt
  /// `Get()` is written as exactly that composition.
  float DistVector3Segment3Get(const float* vec, const float* segment);

  /// Wm3::IntrBox2Circle2<float>::Test. `box` is 8 floats
  /// (Center[2], Axis[2][2], Extent[2]), `circle` is 3 (Center[2], Radius).
  /// Returns 1 or 0.
  u32 IntrBox2Circle2Test(const float* box, const float* circle);

/// Wm3::IntrLine3Box3<float>::Test. `line` is 6 floats
  /// (Origin[3], Direction[3]), `box` is 15. Returns 1 or 0.
  u32 IntrLine3Box3Test(const float* line, const float* box);

  /// Wm3::IntrSegment3Box3<float>::Find. `segment` is 7 floats
  /// (Origin[3], Direction[3], Extent), `box` is 15, `solid` is 0 or 1.
  /// Returns 1 or 0.
  u32 IntrSegment3Box3Find(const float* segment, const float* box, u32 solid);

/// Wm3::IntrBox3Sphere3<float>::Test. `box` is 15 floats, `sphere` is 4
  /// (Center[3], Radius). Returns 1 or 0.
  u32 IntrBox3Sphere3Test(const float* box, const float* sphere);

  /// Wm3::IntrBox3Sphere3<float>::GetVertexIntersection. The seven floats are
  /// the Wild Magic ones, in the Wild Magic order: fDx, fDy, fDz, fVx, fVy,
  /// fVz, fRSqr. Returns the raw float - the time of the impact.
  float IntrBox3Sphere3GetVertexIntersection(float fDx, float fDy, float fDz,
                                             float fVx, float fVy, float fVz,
                                             float fRSqr);

  /// Wm3::IntrBox3Sphere3<float>::GetEdgeIntersection. Six floats: fDx, fDz,
  /// fVx, fVz, fVSqr, fRSqr. Returns the raw float.
  float IntrBox3Sphere3GetEdgeIntersection(float fDx, float fDz, float fVx,
                                           float fVz, float fVSqr, float fRSqr);

  /// Wm3::IntrBox3Sphere3<float>::FindFaceRegionIntersection. `box` is 15
  /// floats and `sphere` is 4 (Center[3], Radius) - the helper reads only
  /// the radius, but the object needs both. The nine floats are the helper's
  /// own arguments in the Wild Magic order: fEx, fEy, fEz (the box's
  /// half-extents in the helper's frame), fCx, fCy, fCz (the sphere centre in
  /// that frame) and fVx, fVy, fVz (the velocity in that frame). `above` is
  /// the bAboveFace flag. Writes the three intersection coordinates into
  /// `out3` and the contact time into `outTime`, and returns the helper's own
  /// return value: 0 for "no intersection", 1 for "intersection", -1 for
  /// "already intersecting".
  ///
  /// The three coordinates are only written on the hit path, so the harness
  /// starts them from a sentinel and reports the raw bits; the retail side
  /// starts from the same sentinel.
  int IntrBox3Sphere3FindFaceRegionIntersection(const float* box,
      const float* sphere, float fEx, float fEy, float fEz, float fCx,
      float fCy, float fCz, float fVx, float fVy, float fVz, u32 above,
      float* out3, float* outTime);

  /// Wm3::IntrBox3Sphere3<float>::FindJustEdgeIntersection. The nine floats
  /// are the helper's own arguments in the Wild Magic order: fCy, fEx, fEy,
  /// fEz, fDx, fDz, fVx, fVy, fVz. Writes the three intersection
  /// coordinates into `out3` and the contact time into `outTime`, and returns
  /// the helper's return value.
  int IntrBox3Sphere3FindJustEdgeIntersection(const float* box,
      const float* sphere, float fCy, float fEx, float fEy, float fEz,
      float fDx, float fDz, float fVx, float fVy, float fVz, float* out3,
      float* outTime);

  /// Wm3::IntrBox3Sphere3<float>::FindEdgeRegionIntersection. The nine
  /// floats are the helper's own arguments in the Wild Magic order: fEx,
  /// fEy, fEz, fCx, fCy, fCz, fVx, fVy, fVz. `above` is the bAboveEdge
  /// flag. Writes the three intersection coordinates into `out3` and the
  /// contact time into `outTime`, and returns the helper's return value.
  int IntrBox3Sphere3FindEdgeRegionIntersection(const float* box,
      const float* sphere, float fEx, float fEy, float fEz, float fCx,
      float fCy, float fCz, float fVx, float fVy, float fVz, u32 above,
      float* out3, float* outTime);

  /// Wm3::IntrBox3Sphere3<float>::FindVertexRegionIntersection. The nine
  /// floats are the helper's own arguments in the Wild Magic order: fEx,
  /// fEy, fEz, fCx, fCy, fCz, fVx, fVy, fVz. Writes the three intersection
  /// coordinates into `out3` and the contact time into `outTime`, and returns
  /// the helper's return value.
  int IntrBox3Sphere3FindVertexRegionIntersection(const float* box,
      const float* sphere, float fEx, float fEy, float fEz, float fCx,
      float fCy, float fCz, float fVx, float fVy, float fVz, float* out3,
      float* outTime);

/// Wm3::IntrBox3Sphere3<float>::Find(fTMax, velocity0, velocity1). `box` is
  /// 15 floats, `sphere` is 4 (Center[3], Radius), the velocities are Vector3.
  /// Returns 1 or 0.
  u32 IntrBox3Sphere3Find(const float* box, const float* sphere, float fTMax,
                           const float* vel0, const float* vel1);

  /// Wm3::IntrBox3Sphere3<float>::GetContactPoint. Same inputs as Find; writes
  /// the three floats the returned reference points at into `out3`, or zero when
  /// Find did not report a contact - the member is only written on success.
  /// `out3` receives the three contact-point components and then the contact
  /// time, so a mismatch says which of the two moved.
  void IntrBox3Sphere3GetContactPoint(const float* box, const float* sphere,
                                      float fTMax, const float* vel0,
                                      const float* vel1, float* out3);

  /// Wm3::IntrSegment3Box3<float>::GetPoint(int). Same inputs as Find; writes
  /// the three floats the returned reference points at into `out3`.
  void IntrSegment3Box3GetPoint(const float* segment, const float* box, u32 solid,
                                u32 index, float* out3);

  /// Wm3::IntrSegment3Sphere3<float>::Find(). `segment` is 7 floats
  /// (Origin[3], Direction[3], Extent), `sphere` is 4 (Center[3], Radius).
  /// Returns 1 or 0.
  u32 IntrSegment3Sphere3Find(const float* segment, const float* sphere);

/// Wm3::IntrSegment3Sphere3<float>::GetPoint(int). Same inputs as Find;
  /// writes the three floats the returned reference points at into `out3`.
  /// The point is only defined after a successful Find - the source's own
  /// GetPoint asserts 0 <= i < m_iQuantity - so a failed Find writes zero on
  /// both sides.
  void IntrSegment3Sphere3GetPoint(const float* segment, const float* sphere,
                                   u32 index, float* out3);

  /// Wm3::IntrBox2Box2<float>::Test. `box0` and `box1` are 8 floats each
  /// (Center[2], Axis[2][2], Extent[2]). Returns 1 or 0.
  u32 IntrBox2Box2Test(const float* box0, const float* box1);

/// Wm3::DistVector2Box2<float>::GetSquared. `vec` is 2 floats, `box` is 8
  /// (Center[2], Axis[2][2], Extent[2]). Returns the raw float.
  float DistVector2Box2GetSquared(const float* vec, const float* box);

/// Wm3::DistVector2Box2<float>::Get(). `vec` is 2 floats, `box` is 8
  /// (Center[2], Axis[2][2], Extent[2]). Writes one word: the raw float bits
  /// of the distance.
  void DistVector2Box2Get(const float* vec, const float* box, u32* out);

/// Wm3::InBox<float>(point, box) - the free function the engine's
  /// Moho::CColPrimitiveBase::CollideBox calls. `point` is 3 floats, `box` is
  /// 15 (Center[3], Axis[3][3], Extent[3]). Returns 1 or 0.
  u32 InBox(const float* point, const float* box);

  /// Wm3::DistVector3Segment3<float>::Get(fT,v0,v1) - the swept distance.
  /// `vec` is 3 floats, `segment` is 7 (Origin[3], Direction[3], Extent),
  /// `fT` is the time of the query and the two velocities are three floats
  /// each. Returns the raw float.
  float DistVector3Segment3Get(const float* vec, const float* segment, float fT,
                                const float* vel0, const float* vel1);

  /// Wm3::DistVector3Segment3<float>::GetSquared(fT,v0,v1) - the swept
  /// squared distance. Same inputs as Get. Returns the raw float.
  float DistVector3Segment3GetSquared(const float* vec, const float* segment,
                                      float fT, const float* vel0,
                                      const float* vel1);

  /// Wm3::ContAlignedBox<float>(quantity, points) - the axis-aligned box
  /// containing the points. `points` is eight vectors of three floats.
  /// Writes fifteen words: the centre (three), the three axes (nine) and the
  /// three extents (three), in that order.
  void ContAlignedBox(int quantity, const float* points, u32* out);

  /// Wm3::ContAlignedBox<float>(quantity, points, mask) - the axis-aligned box
  /// containing the points whose mask entry is true. `mask` is one entry per
  /// point and may be null, which selects the unmasked path. Writes fifteen
  /// words: the centre (three), the three axes (nine) and the three extents
  /// (three), in that order.
  void ContAlignedBoxMask(int quantity, const float* points, const bool* mask,
                          u32* out);

// ---- the swept Distance queries ----------------------------------------
  //
  // The derived class's own Get(fT,v0,v1) and GetSquared(fT,v0,v1) - the two
  // virtuals the class vtable puts at +0x08 and +0x14.

// ---- Wm3::DistLine3Segment3<float> ---------------------------------------

// ---- Wm3::IntrLine3Box3<float>::DoClipping and ::Clip -------------------

  /// Wm3::IntrLine3Box3<float>::DoClipping - the static helper the retail's
  /// Find tail-calls at 0x00A50220. `origin` and `direction` are 3 floats
  /// each, `box` is 15, `points` is the six-float output array (two points).
  /// Writes the quantity, the intersection type and the two points; returns
  /// the helper's bool.
  u32 IntrLine3Box3DoClipping(float fT0, float fT1, const float* origin,
                              const float* direction, const float* box,
                              u32 solid, u32* quantity, float* points,
                              u32* type);

  /// Wm3::IntrLine3Box3<float>::Clip - the *private* static helper the
  /// retail's DoClipping calls at 0x00A4FCC0. Writes the two parameters
  /// through `t0` and `t1` and returns the helper's bool.
  u32 IntrLine3Box3Clip(float denom, float numer, float* t0, float* t1);

/// Wm3::Distance<float,Vector3f>::SetDifferenceStep(step). The base's
  /// constructor is protected, so the object is a derived one; the two
  /// members the row compares are the difference step and its reciprocal.
  /// Writes two words: the step and 0.5/step.
  void DistanceSetDifferenceStep(float step, u32* out);

  /// The same two members straight after construction, with no setter call:
  /// the constructor's own defaults.
  void DistanceDifferenceStepDefault(u32* out);

/// Wm3::Vector3<float>::Length() on the rebuild side: the instance
  /// method, which the engine calls (`velocity.Length()`).  The raw bits of
  /// the returned float are written to `out`.
  u32 Vector3LengthOurs(const float* v);

  /// Wm3::Vector3<float>::CompareArrays on the rebuild side: the raw bits of
  /// the returned int are written to `out`.  The member compares the twelve
  /// bytes of the two vectors.
  u32 Vector3CompareArraysOurs(const float* a, const float* b);

  /// Wm3::Vector3<float>::operator== on the rebuild side.
  u32 Vector3OperatorEquals(const float* a, const float* b);
  /// Wm3::Quaternion<float>::CompareArrays on the rebuild side.
  u32 QuaternionCompareArrays(const float* a, const float* b);
  /// Wm3::Quaternion<float>::operator== on the rebuild side.
  u32 QuaternionOperatorEquals(const float* a, const float* b);

  /// Wm3::Sphere3<float>::Sphere3 on the rebuild side: the object's sixteen
  /// bytes are written to `out` (centre's three lanes, then the radius).
  void Sphere3CtorOurs(std::uint32_t* out, const float* center, float radius);
} // namespace wm3collharness
