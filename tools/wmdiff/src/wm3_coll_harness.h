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

  /// Wm3::Eigen<float>::IncrSortEigenStuff2 (n == 2) and
  /// IncrSortEigenStuff3 (n == 3) on the rebuild side.  `m` is the n-by-n
  /// matrix in row-major order (only the upper triangle is read by the
  /// solver, exactly as the retail reads it).  The eigenvalues (n floats)
  /// and then the eigenvector matrix (n*n floats, row-major, the columns
  /// are the eigenvectors) are written to `out`.
  void EigenfIncrSortOurs(const float* m, int n, u32* out);

  /// Wm3::Eigen<float>::GetEigenvalue(i) on the rebuild side, after the
  /// same solve: the n eigenvalues are written to `out` as raw bits.
  void EigenfGetEigenvaluesOurs(const float* m, int n, u32* out);

  /// Wm3::Eigen<float>::GetEigenvector(i, Vector3<Real>&) on the rebuild
  /// side, after the same solve: the three columns (nine floats) are
  /// written to `out` as raw bits.
  void EigenfGetEigenvectorsOurs(const float* m, int n, u32* out);

  /// Wm3::Eigen<float>::GetEigenvector(i, Vector2<Real>&) on the rebuild
  /// side, after the two-by-two solve: the two columns (four floats) are
  /// written to `out` as raw bits.
  void EigenfGetEigenvectors2Ours(const float* m, u32* out);

  /// Wm3::Eigen<double>::IncrSortEigenStuff2 (n == 2) and
  /// IncrSortEigenStuff3 (n == 3) on the rebuild side.  `m` is the n-by-n
  /// matrix in row-major order; the eigenvalues (n doubles) and then the
  /// eigenvector matrix (n*n doubles, row-major) are written to `out` as
  /// raw bits.
  void EigendIncrSortOurs(const double* m, int n, u32* out);

  /// Wm3::Eigen<double>::GetEigenvalue(i) on the rebuild side, after the
  /// same solve: the n eigenvalues are written to `out` as raw bits.
  void EigendGetEigenvaluesOurs(const double* m, int n, u32* out);

  /// Wm3::Eigen<double>::GetEigenvector(i, Vector3<Real>&) on the rebuild
  /// side, after the same solve: the three columns (nine doubles) are
  /// written to `out` as raw bits.
  void EigendGetEigenvectorsOurs(const double* m, int n, u32* out);

  /// Wm3::Eigen<double>::GetEigenvector(i, Vector2<Real>&) on the rebuild
  /// side, after the two-by-two solve: the two columns (four doubles) are
  /// written to `out` as raw bits.
  void EigendGetEigenvectors2Ours(const double* m, u32* out);

  /// Wm3::IntrLine3Box3<float>::Test. `line` is 6 floats
  /// (Origin[3], Direction[3]), `box` is 15. Returns 1 or 0.
  u32 IntrLine3Box3Test(const float* line, const float* box);

  /// Wm3::IntrSegment3Box3<float>::Find. `segment` is 7 floats
  /// (Origin[3], Direction[3], Extent), `box` is 15, `solid` is 0 or 1.
  /// Returns 1 or 0.
  u32 IntrSegment3Box3Find(const float* segment, const float* box, u32 solid);

  /// Wm3::DistVector3Segment3<float>::GetClosestPoint1. Same inputs as
  /// GetSquared; writes the three floats the returned reference points at into
  /// `out3`. GetSquared must have run - it is what fills the member.
  void DistVector3Segment3GetClosestPoint1(const float* vec, const float* segment,
                                           float* out3);

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

  /// Wm3::DistVector3Box3<float>::Get(fT, velocity0, velocity1). Same
  /// inputs as GetSquared, plus fT and the two velocities.
  /// Wm3::DistVector3Segment3<float>::Get(fT, velocity0, velocity1). Same
  /// inputs as GetSquared, plus fT and the two velocities.
  /// Wm3::IntrBox2Circle2<float>::Find(fTMax, velocity0, velocity1). `box` is
  /// 8 floats, `circle` is 3 (Center[2], Radius), the velocities are Vector2.
  /// Returns 1 or 0.
  /// Wm3::IntrBox2Circle2<float>::Find(fTMax, velocity0, velocity1). `box` is
  /// 8 floats, `circle` is 3 (Center[2], Radius), the velocities are Vector2.
  /// Returns 1 or 0.
  u32 IntrBox2Circle2Find(const float* box, const float* circle, float fTMax,
                           const float* vel0, const float* vel1);

  /// Wm3::DistVector3Box3<float>::Get(). Same inputs as GetSquared.
  float DistVector3Box3Get(const float* vec, const float* box);


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

  /// Wm3::IntrSegment3Sphere3<float>::Test(). Same inputs as Find; returns
  /// 1 or 0.
  u32 IntrSegment3Sphere3Test(const float* segment, const float* sphere);

  /// Wm3::IntrSegment3Sphere3<float>::Test(fTMax, velocity0, velocity1) and
  /// ::Find(fTMax, velocity0, velocity1). Same inputs as the static rows,
  /// plus fTMax and the two velocities.
  u32 IntrSegment3Sphere3TestDynamic(const float* segment, const float* sphere,
                                     float fTMax, const float* vel0,
                                     const float* vel1);
  u32 IntrSegment3Sphere3FindDynamic(const float* segment, const float* sphere,
                                     float fTMax, const float* vel0,
                                     const float* vel1);

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

  /// Wm3::DistSegment3Segment3<float>::GetSquared. `seg0` and `seg1` are 7
  /// floats each (Origin[3], Direction[3], Extent). Returns the raw float.
  float DistSegment3Segment3GetSquared(const float* seg0, const float* seg1);

  /// Wm3::DistSegment3Segment3<float>::Get(). Same inputs as GetSquared:
  /// retail dispatches GetSquared virtually and takes the CRT's square root,
  /// and the rebuilt `Get()` is written as exactly that composition.
  float DistSegment3Segment3Get(const float* seg0, const float* seg1);

  /// Wm3::DistVector2Box2<float>::GetSquared. `vec` is 2 floats, `box` is 8
  /// (Center[2], Axis[2][2], Extent[2]). Returns the raw float.
  float DistVector2Box2GetSquared(const float* vec, const float* box);

  /// Wm3::IntrSegment3Capsule3<float>::Test. `segment` is 7 floats
  /// (Origin[3], Direction[3], Extent), `capsule` is 8 (a Segment3 followed
  /// by the radius). Returns 1 or 0.
  u32 IntrSegment3Capsule3Test(const float* segment, const float* capsule);

  /// Wm3::IntrSegment3Capsule3<float>::Find(). Same inputs as Test. Writes six
  /// words: the bool, the intersection quantity, the first segment parameter
  /// and the first intersection point (three floats). The quantity, the
  /// parameter and the point are only defined after a successful Find, so on
  /// the miss path all six words are zero.
  void IntrSegment3Capsule3Find(const float* segment, const float* capsule,
                                u32* out);

  /// Wm3::IntrLine3Box3<float>::Find(). `line` is 6 floats (Origin[3],
  /// Direction[3]), `box` is 15 (Center[3], Axis[3][3], Extent[3]). Writes
  /// nine words: the bool, the intersection type, the quantity, the first
  /// intersection point (three floats) and the second one (three floats). A
  /// point is only written when the quantity covers it, so on the miss path
  /// all nine words are zero.
  void IntrLine3Box3Find(const float* line, const float* box, u32* out);

  /// Wm3::DistVector2Box2<float>::Get(). `vec` is 2 floats, `box` is 8
  /// (Center[2], Axis[2][2], Extent[2]). Writes one word: the raw float bits
  /// of the distance.
  void DistVector2Box2Get(const float* vec, const float* box, u32* out);

  /// Wm3::IntrSegment3Capsule3<float>::GetPoint(0). `segment` is 7 floats
  /// (Origin[3], Direction[3], Extent), `capsule` is 8 (a Segment3 followed
  /// by the radius). Writes three words: the point's three floats. The
  /// intersection set is only defined after a successful Find, so on the miss
  /// path all three words are zero.
  void IntrSegment3Capsule3GetPoint(const float* segment, const float* capsule,
                                    u32* out);

  /// Wm3::IntrSegment3Capsule3<float>::GetParameter(0). Same inputs as
  /// GetPoint. Writes one word: the parameter's raw float bits, or zero on
  /// the miss path.
  void IntrSegment3Capsule3GetParameter(const float* segment,
                                        const float* capsule, u32* out);

  /// Wm3::Distance<float,Vector3<float>>::GetDerivative, reached through a
  /// DistVector3Segment3. `vec` is 3 floats, `segment` is 7 (Origin[3],
  /// Direction[3], Extent), `fT` is the time of the query and the two
  /// velocities are three floats each. Returns the raw float.
  float DistVector3Segment3GetDerivative(const float* vec, const float* segment,
                                          float fT, const float* vel0,
                                          const float* vel1);

  /// Wm3::Distance<float,Vector3<float>>::GetDerivativeSquared, reached
  /// through a DistVector3Segment3. Same inputs as GetDerivative. Returns
  /// the raw float.
  float DistVector3Segment3GetDerivativeSquared(const float* vec,
                                                  const float* segment, float fT,
                                                  const float* vel0,
                                                  const float* vel1);

  /// Wm3::IntrSegment3Box3<float>::Test(). `segment` is 7 floats
  /// (Origin[3], Direction[3], Extent), `box` is 15 (Center[3], Axis[3][3],
  /// Extent[3]) and `solid` is the bSolid flag. Returns 1 or 0.
  u32 IntrSegment3Box3Test(const float* segment, const float* box, u32 solid);

  /// Wm3::IntrLine3Capsule3<float>::Find(origin, direction, capsule, afT) -
  /// the static one. `line` is 6 floats (Origin[3], Direction[3]), `capsule`
  /// is 8. Writes three words: the number of roots and the two roots. A root
  /// is only written when the returned count covers it, so the missing roots
  /// are zero.
  void IntrLine3Capsule3Find(const float* line, const float* capsule, u32* out);

  /// Wm3::IntrLine3Capsule3<float>::Test() - the member, not the static
  /// Find. `line` is 6 floats (Origin[3], Direction[3]), `capsule` is 8
  /// (the spine Segment3 - Origin[3], Direction[3], Extent - then the
  /// radius). Returns 1 or 0.
  u32 IntrLine3Capsule3Test(const float* line, const float* capsule);

  /// Wm3::ConvexHull3<float>'s constructor - the hull construction. `points`
  /// is `n` vertices of three floats each, `eps` is the epsilon the Mapper3
  /// uses for the dimension test and `type` is the Query::Type value. The
  /// hull's dimension, its simplex quantity, its line/plane data and its
  /// triangles are written to `out` by ConvexHull3Canonical.
  void ConvexHull3Ours(const float* points, int n, float eps, u32 type,
                       u32* out);

  /// Wm3::ConvexHull1<float>'s constructor. `values` is `n` scalars, `eps` is
  /// the epsilon the hull uses for its dimension test and `type` is the
  /// Query::Type value. The comparison words are written by
  /// ConvexHull1Canonical.
  void ConvexHull1Ours(const float* values, int n, float eps, u32 type,
                       u32* out);

  /// Wm3::ConvexHull2<double>'s constructor. `points` is `n` vertices of two
  /// doubles each. The comparison words are written by
  /// ConvexHull2Canonical.
  void ConvexHull2dOurs(const double* points, int n, double eps, u32 type,
                        u32* out);

  /// Wm3::ConvexHull2<float>'s constructor. `points` is `n` vertices of two
  /// floats each. The comparison words are written by ConvexHull2Canonical.
  void ConvexHull2Ours(const float* points, int n, float eps, u32 type,
                       u32* out);

  /// The comparison words of a one-dimensional hull: the dimension, the
  /// simplex quantity and the two indices of the segment.
  ///
  /// The two indices are sorted, because the hull's own order is by value -
  /// the smaller value first - and the sort that produces it is not stable,
  /// so two hulls that agree on the segment can name a different one of two
  /// equal-valued vertices. Sorting the pair removes that tie-break from the
  /// comparison; the value order itself is not compared.
  void ConvexHull1Canonical(int dimension, int simplexQuantity,
                            const int* indices, u32* out);

  /// The comparison words of a two-dimensional hull: the dimension, the
  /// simplex quantity and the hull's edges.
  ///
  /// The edges are canonicalised: each pair is normalised so that the
  /// smaller index comes first and the list of pairs is then sorted. The
  /// hull's own order is the traversal order of its edge list, which is not
  /// comparable between the two sides - and the direction of an edge, which
  /// carries the hull's winding, is dropped with the normalisation.
  void ConvexHull2Canonical(int dimension, int simplexQuantity,
                            const int* indices, u32* out);

  /// The comparison words of a hull, shared by both sides.
  ///
  /// `indices` is the hull's index array (three indices per simplex),
  /// `linePlane` the fifteen floats of the line origin, the line direction,
  /// the plane origin and the two plane directions, in that order. The hull's
  /// own index order comes from a std::set of triangle *pointers*, so it
  /// follows the heap addresses and cannot be compared between the two
  /// sides; the set of triangles can. Each triple is therefore rotated so
  /// that its smallest index comes first - which keeps the winding - and the
  /// triples are sorted.
  ///
  /// The words are: dimension, simplex quantity, the fifteen floats, then
  /// three words per triangle.
  void ConvexHull3Canonical(int dimension, int simplexQuantity,
                            const int* indices, const float* linePlane,
                            u32* out);

  /// Wm3::Query3<float>::ToPlane(point, iV0, iV1, iV2). `verts` is `n`
  /// vertices of three floats each, `point` is three floats. Returns the
  /// sign: +1, -1 or 0.
  int Query3ToPlaneOurs(const float* verts, int n, const float* point, int iV0,
                        int iV1, int iV2);

  /// The rest of the Query3<float> family. Every one of these is a public
  /// member of the rebuild's Wm3::Query3<float>, so they are called by name -
  /// the rebuild's virtual order is the WM4 declaration order, which puts the
  /// index-taking overload first, and that never reaches the retail side.
  ///
  /// `verts` is `n` vertices of three floats each. The index-taking overloads
  /// take the point's index `i` instead of a point.
  int Query3ToPlaneIndexOurs(const float* verts, int n, int i, int iV0,
                              int iV1, int iV2);
  int Query3ToTetrahedronOurs(const float* verts, int n, const float* point,
                               int iV0, int iV1, int iV2, int iV3);
  int Query3ToTetrahedronIndexOurs(const float* verts, int n, int i, int iV0,
                                    int iV1, int iV2, int iV3);
  int Query3ToCircumsphereOurs(const float* verts, int n, const float* point,
                                int iV0, int iV1, int iV2, int iV3);
  int Query3ToCircumsphereIndexOurs(const float* verts, int n, int i, int iV0,
                                     int iV1, int iV2, int iV3);

  /// The Query2<float> family. `verts` is `n` vertices of two floats each.
  int Query2ToLineOurs(const float* verts, int n, const float* point, int iV0,
                        int iV1);
  int Query2ToLineIndexOurs(const float* verts, int n, int i, int iV0,
                             int iV1);
  int Query2ToTriangleOurs(const float* verts, int n, const float* point,
                           int iV0, int iV1, int iV2);
  int Query2ToTriangleIndexOurs(const float* verts, int n, int i, int iV0,
                                int iV1, int iV2);
  int Query2ToCircumcircleOurs(const float* verts, int n, const float* point,
                                int iV0, int iV1, int iV2);
  int Query2ToCircumcircleIndexOurs(const float* verts, int n, int i, int iV0,
                                     int iV1, int iV2);

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

  /// Wm3::ContAlignedBox<float>(quantity, points) - the 2D axis-aligned box
  /// containing the points. `points` is eight vectors of two floats. Writes
  /// eight words: the centre (two), the two axes (four) and the two extents
  /// (two), in that order.
  void ContAlignedBox2(int quantity, const float* points, u32* out);

  /// Wm3::ContAlignedBox<float>(quantity, points, mask) - the 2D axis-aligned
  /// box containing the points whose mask entry is true. `mask` is one entry
  /// per point and may be null, which selects the unmasked path. Writes the
  /// same eight words as the unmasked query.
  void ContAlignedBox2Mask(int quantity, const float* points, const bool* mask,
                           u32* out);

  /// Wm3::IntrBox2Circle2<float>::TestVertexRegion. The six floats are the
  /// helper's own arguments in the Wild Magic order: fCx, fCy, fVx, fVy, fEx,
  /// fEy. Writes the two intersection coordinates into `out2` and the contact
  /// time into `outTime`, and returns the helper's return value.
  int IntrBox2Circle2TestVertexRegion(const float* box, const float* circle,
      float fCx, float fCy, float fVx, float fVy, float fEx, float fEy,
      float* out2, float* outTime);

  /// Wm3::IntrBox2Circle2<float>::TestEdgeRegion. The same shape as the
  /// vertex-region helper.
  int IntrBox2Circle2TestEdgeRegion(const float* box, const float* circle,
      float fCx, float fCy, float fVx, float fVy, float fEx, float fEy,
      float* out2, float* outTime);

  // ---- the swept Distance queries ----------------------------------------
  //
  // The derived class's own Get(fT,v0,v1) and GetSquared(fT,v0,v1) - the two
  // virtuals the class vtable puts at +0x08 and +0x14.

  /// Wm3::DistVector3Box3<float>::Get(fT,v0,v1). `vec` is 3 floats, `box` is
  /// 15, the two velocities are 3 floats each.
  float DistVector3Box3GetSwept(const float* vec, const float* box, float fT,
                                const float* vel0, const float* vel1);

  /// Wm3::DistVector3Box3<float>::GetSquared(fT,v0,v1). Same inputs.
  float DistVector3Box3GetSquaredSwept(const float* vec, const float* box,
                                        float fT, const float* vel0,
                                        const float* vel1);

  /// Wm3::DistVector2Box2<float>::Get(fT,v0,v1). `vec` is 2 floats, `box` is
  /// 8, the two velocities are 2 floats each.
  float DistVector2Box2GetSwept(const float* vec, const float* box, float fT,
                                const float* vel0, const float* vel1);

  /// Wm3::DistVector2Box2<float>::GetSquared(fT,v0,v1). Same inputs.
  float DistVector2Box2GetSquaredSwept(const float* vec, const float* box,
                                        float fT, const float* vel0,
                                        const float* vel1);

  /// Wm3::DistSegment3Segment3<float>::Get(fT,v0,v1). `seg0` and `seg1` are 7
  /// floats each (Origin[3], Direction[3], Extent), the velocities 3 each.
  float DistSegment3Segment3GetSwept(const float* seg0, const float* seg1,
                                     float fT, const float* vel0,
                                     const float* vel1);

  /// Wm3::DistSegment3Segment3<float>::GetSquared(fT,v0,v1). Same inputs.
  float DistSegment3Segment3GetSquaredSwept(const float* seg0,
                                             const float* seg1, float fT,
                                             const float* vel0,
                                             const float* vel1);

  // ---- Wm3::DistLine3Segment3<float> ---------------------------------------

  /// Wm3::DistLine3Segment3<float>::GetSquared. `line` is 6 floats (Origin[3],
  /// Direction[3]), `segment` is 7 (Origin[3], Direction[3], Extent).
  float DistLine3Segment3GetSquared(const float* line, const float* segment);

  /// Wm3::DistLine3Segment3<float>::Get(). Same inputs.
  float DistLine3Segment3Get(const float* line, const float* segment);

  /// Wm3::DistLine3Segment3<float>::Get(fT,v0,v1). Same inputs plus the time
  /// and the two velocities (3 floats each).
  float DistLine3Segment3GetSwept(const float* line, const float* segment,
                                   float fT, const float* vel0,
                                   const float* vel1);

  /// Wm3::DistLine3Segment3<float>::GetSquared(fT,v0,v1). Same inputs.
  float DistLine3Segment3GetSquaredSwept(const float* line,
                                          const float* segment, float fT,
                                          const float* vel0,
                                          const float* vel1);

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

  /// Wm3::Distance<float,Vector3f>::Get(fTMin,fTMax,v0,v1) - the base
  /// class's *window* query, the one the base's own vftable puts at +0x04.
  /// It minimises the distance over the interval [fTMin,fTMax]. `vec` is 3
  /// floats, `segment` is 7, the two velocities are 3 floats each.
  float DistVector3Segment3WindowGet(const float* vec, const float* segment,
                                     float fTMin, float fTMax,
                                     const float* vel0, const float* vel1);

  /// Wm3::Distance<float,Vector3f>::GetSquared(fTMin,fTMax,v0,v1) - the
  /// base class's window query at the base vftable's +0x10 slot. Same
  /// inputs as the distance one.
  float DistVector3Segment3WindowGetSquared(const float* vec,
                                            const float* segment, float fTMin,
                                            float fTMax, const float* vel0,
                                            const float* vel1);

  /// Wm3::Distance<float,Vector3f>::SetDifferenceStep(step). The base's
  /// constructor is protected, so the object is a derived one; the two
  /// members the row compares are the difference step and its reciprocal.
  /// Writes two words: the step and 0.5/step.
  void DistanceSetDifferenceStep(float step, u32* out);

  /// The same two members straight after construction, with no setter call:
  /// the constructor's own defaults.
  void DistanceDifferenceStepDefault(u32* out);

  /// Wm3::Distance<double,Vector3d>::SetDifferenceStep(step) - the double
  /// instantiation of the same base class. The step is a double, so it
  /// travels as two words, and so does the reciprocal: four words in all.
  void Distance3dSetDifferenceStep(double step, u32* out);

  /// The same two members straight after construction, with no setter call.
  void Distance3dDifferenceStepDefault(u32* out);

  /// Wm3::Distance<double,Vector2d>::SetDifferenceStep(step) and the
  /// constructor's own defaults, the Vector2 instantiation.
  void Distance2dSetDifferenceStep(double step, u32* out);
  void Distance2dDifferenceStepDefault(u32* out);

  /// Wm3::Distance<double,Vector3d>'s own four bodies - the two window
  /// queries and the two derivative defaults.  They are the base's members,
  /// so the object is a derived one (a DistVector3Segment3<double>) and the
  /// call goes through the base's reference: the bodies dispatch through the
  /// vtable to the derived class's own swept Get.
  ///
  /// `vec` is six words, `segment` fourteen, the two velocities six words
  /// each; a double is two words.
  double Distance3dWindowGet(const u32* vec, const u32* segment, double fTMin,
                             double fTMax, const u32* vel0, const u32* vel1);
  double Distance3dWindowGetSquared(const u32* vec, const u32* segment,
                                    double fTMin, double fTMax,
                                    const u32* vel0, const u32* vel1);
  double Distance3dGetDerivative(const u32* vec, const u32* segment, double fT,
                                 const u32* vel0, const u32* vel1);
  double Distance3dGetDerivativeSquared(const u32* vec, const u32* segment,
                                        double fT, const u32* vel0,
                                        const u32* vel1);

  /// The same four bodies of Wm3::Distance<double,Vector2d>.  `vec` is four
  /// words, `box` sixteen, the two velocities four words each.
  double Distance2dWindowGet(const u32* vec, const u32* box, double fTMin,
                             double fTMax, const u32* vel0, const u32* vel1);
  double Distance2dWindowGetSquared(const u32* vec, const u32* box,
                                    double fTMin, double fTMax,
                                    const u32* vel0, const u32* vel1);
  double Distance2dGetDerivative(const u32* vec, const u32* box, double fT,
                                 const u32* vel0, const u32* vel1);
  double Distance2dGetDerivativeSquared(const u32* vec, const u32* box,
                                        double fT, const u32* vel0,
                                        const u32* vel1);

  /// Wm3::DistVector3Box3<double>::GetSquared() and ::Get() - the rebuild's
  /// own double instantiation.  The inputs are word arrays: a double is two
  /// words, so the vector is six words and the box (the centre, the three
  /// axes and the three extents) thirty.
  double DistVector3Box3dGetSquared(const u32* vec, const u32* box);
  double DistVector3Box3dGet(const u32* vec, const u32* box);

  /// Wm3::DistVector3Box3<double>'s two swept forms.
  double DistVector3Box3dGetSwept(const u32* vec, const u32* box, double fT,
                                  const u32* vel0, const u32* vel1);
  double DistVector3Box3dGetSquaredSwept(const u32* vec, const u32* box,
                                         double fT, const u32* vel0,
                                         const u32* vel1);

  /// Wm3::DistVector3Segment3<double>::GetSquared(), ::Get() and their two
  /// swept forms - the rebuild's own double instantiation.  `vec` is six
  /// words, `segment` fourteen (the origin, the direction and the extent),
  /// and the two velocities six words each.
  double DistVector3Segment3dGetSquared(const u32* vec, const u32* segment);
  double DistVector3Segment3dGet(const u32* vec, const u32* segment);
  double DistVector3Segment3dGetSwept(const u32* vec, const u32* segment,
                                       double fT, const u32* vel0,
                                       const u32* vel1);
  double DistVector3Segment3dGetSquaredSwept(const u32* vec, const u32* segment,
                                              double fT, const u32* vel0,
                                              const u32* vel1);

  /// Wm3::DistVector2Box2<double>'s static and swept forms.  The vector is
  /// two doubles (four words) and the box is eight doubles (sixteen words).
  double DistVector2Box2dGetSquared(const u32* vec, const u32* box);
  double DistVector2Box2dGet(const u32* vec, const u32* box);
  double DistVector2Box2dGetSwept(const u32* vec, const u32* box, double fT,
                                   const u32* vel0, const u32* vel1);
  double DistVector2Box2dGetSquaredSwept(const u32* vec, const u32* box,
                                          double fT, const u32* vel0,
                                          const u32* vel1);

  /// Wm3::DistSegment3Segment3<double>'s static and swept forms.  Each
  /// segment is seven doubles (fourteen words).
  double DistSegment3Segment3dGetSquared(const u32* seg0, const u32* seg1);
  double DistSegment3Segment3dGet(const u32* seg0, const u32* seg1);
  double DistSegment3Segment3dGetSwept(const u32* seg0, const u32* seg1,
                                        double fT, const u32* vel0,
                                        const u32* vel1);
  double DistSegment3Segment3dGetSquaredSwept(const u32* seg0, const u32* seg1,
                                               double fT, const u32* vel0,
                                               const u32* vel1);

  /// Wm3::DistLine3Segment3<double>'s static and swept forms.  The line is
  /// six doubles (twelve words) and the segment seven (fourteen words).
  double DistLine3Segment3dGetSquared(const u32* line, const u32* segment);
  double DistLine3Segment3dGet(const u32* line, const u32* segment);
  double DistLine3Segment3dGetSwept(const u32* line, const u32* segment,
                                      double fT, const u32* vel0,
                                      const u32* vel1);
  double DistLine3Segment3dGetSquaredSwept(const u32* line, const u32* segment,
                                             double fT, const u32* vel0,
                                             const u32* vel1);

  /// Wm3::IntrBox2Circle2<double>'s Test.  The box is eight doubles (sixteen
  /// words) and the circle three (six words).
  u32 IntrBox2Circle2dTest(const u32* box, const u32* circle);

  /// Wm3::IntrBox2Box2<double>'s Test.  The float row's layout, with the doubles.
  u32 IntrBox2Box2dTest(const u32* a, const u32* b);

  /// Wm3::IntrLine3Box3<double>'s Test.  The float row's layout, with the doubles.
  u32 IntrLine3Box3dTest(const u32* a, const u32* b);

  /// Wm3::IntrSegment3Box3<double>'s Test.  The float row's layout, with the doubles.
  u32 IntrSegment3Box3dTest(const u32* a, const u32* b, u32 flag);

  /// Wm3::IntrSegment3Sphere3<double>'s Test.  The float row's layout, with the doubles.
  u32 IntrSegment3Sphere3dTest(const u32* a, const u32* b);

  /// Wm3::IntrLine3Box3<double>'s Find.  The line is six doubles (twelve
  /// words) and the box fifteen (thirty words).  The output is the hit flag,
  /// the intersection type, the quantity and the two points (each three
  /// doubles), fifteen words.
  void IntrLine3Box3dFind(const u32* line, const u32* box, u32* out);

  /// Wm3::IntrSegment3Box3<double>'s Find.  The segment is seven doubles
  /// (fourteen words), the box fifteen (thirty words) and the solid flag one
  /// word.  The output is the hit flag, the intersection type, the quantity
  /// and the two points (each three doubles), fifteen words.
  void IntrSegment3Box3dFind(const u32* segment, const u32* box, u32 solid,
                             u32* out);

  /// Wm3::IntrSegment3Sphere3<double>'s Find (the static one).  The segment
  /// is seven doubles (fourteen words) and the sphere four (eight words).
  /// The output is the hit flag, the quantity, the two points (each three
  /// doubles) and the two segment parameters, eighteen words.
  void IntrSegment3Sphere3dFind(const u32* segment, const u32* sphere, u32* out);

  /// Wm3::IntrSegment3Sphere3<double>'s dynamic Find,
  /// Find(fTMax, velocity0, velocity1).  The segment is seven doubles
  /// (fourteen words), the sphere four (eight words), the time limit one
  /// double (two words) and the two velocities three doubles each (six
  /// words each).  The output is the hit flag, the intersection type, the
  /// contact time and the contact point (three doubles), ten words.
  void IntrSegment3Sphere3dFindDynamic(const u32* segment, const u32* sphere,
                                       const u32* tmax, const u32* vel0,
                                       const u32* vel1, u32* out);


  /// Wm3::IntrBox3Sphere3<double>'s Test.  The box is fifteen doubles
  /// (thirty words) and the sphere four (eight words).
  u32 IntrBox3Sphere3dTest(const u32* box, const u32* sphere);

  /// Wm3::IntrSegment3Capsule3<double>'s Test.  The segment is seven doubles
  /// (fourteen words) and the capsule eight (sixteen words).
  u32 IntrSegment3Capsule3dTest(const u32* segment, const u32* capsule);

  /// Wm3::IntrSegment3Capsule3<double>'s Find.  The segment is seven doubles
  /// (fourteen words) and the capsule eight (sixteen words); the output is
  /// the hit, the intersection type, the quantity and the two points (the
  /// points are zero when there is no such point).
  void IntrSegment3Capsule3dFind(const u32* segment, const u32* capsule,
                                 u32* out);

  /// Wm3::Vector3<double>::GenerateOrthonormalBasis(U, V, W, bUnitLengthW).
  /// The input is W (three doubles, six words) and the flag (one word); the
  /// output is U (three doubles) followed by V (three doubles), twelve
  /// words.
  void Vector3GenerateOrthonormalBasisd(const std::uint32_t* in,
                                        std::uint32_t* out);

  /// Wm3::Vector3<float>::GenerateOrthonormalBasis(U, V, W, bUnitLengthW).
  /// The float row's layout: W is three words, the flag one, and the output
  /// is six words.
  void Vector3GenerateOrthonormalBasisf(const std::uint32_t* in,
                                        std::uint32_t* out);

  /// Wm3::Vector3<double>::Normalize().  The input is the vector (three
  /// doubles, six words); the output is the normalised vector (three
  /// doubles) followed by the length the member returns (one double), eight
  /// words.
  void Vector3NormalizeD(const std::uint32_t* in, std::uint32_t* out);


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
