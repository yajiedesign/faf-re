#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

#include "wm3_coll_harness.h"
#include "fuzz.h"

// Forwards to the real Wild Magic 3.8 sources - no copies. Same float-relevant
// switches as main.vcxproj uses for Win32.

#include "Wm3Box2.h"
#include "Wm3Box3.h"
#include "Wm3Capsule3.h"
#include "Wm3Circle2.h"
#include "Wm3ConvexHull1.h"
#include "Wm3ConvexHull2.h"
#include "Wm3ConvexHull3.h"
#include "Wm3ContBox2.h"
#include "Wm3ContBox3.h"
#include "Wm3DistLine3Segment3.h"
#include "Wm3DistSegment3Segment3.h"
#include "Wm3DistVector2Box2.h"
#include "Wm3DistVector3Box3.h"
#include "Wm3DistVector3Segment3.h"
#include "Wm3Eigen.h"
#include "Wm3IntrBox2Box2.h"
#include "Wm3IntrBox2Circle2.h"
#include "Wm3IntrBox3Sphere3.h"
#include "Wm3IntrLine3Box3.h"
#include "Wm3IntrLine3Capsule3.h"
#include "Wm3IntrSegment3Box3.h"
#include "Wm3IntrSegment3Capsule3.h"
#include "Wm3IntrSegment3Sphere3.h"
#include "Wm3Line3.h"
#include "Wm3Query2.h"
#include "Wm3Quaternion.h"
#include "Wm3Query3.h"
#include "Wm3Segment3.h"
#include "Wm3Sphere3.h"
#include "Wm3Vector2.h"
#include "Wm3Vector3.h"

namespace wm3collharness
{
  namespace
  {
    Wm3::Vector3f Load3(const float* p) noexcept
    {
      return Wm3::Vector3f(p[0], p[1], p[2]);
    }

    Wm3::Vector2f Load2(const float* p) noexcept
    {
      return Wm3::Vector2f(p[0], p[1]);
    }

    Wm3::Box3f LoadBox3(const float* p) noexcept
    {
      return Wm3::Box3f(Load3(p),
                        reinterpret_cast<const Wm3::Vector3f*>(p + 3),
                        p + 12);
    }

    Wm3::Box2f LoadBox2(const float* p) noexcept
    {
      return Wm3::Box2f(Load2(p),
                        reinterpret_cast<const Wm3::Vector2f*>(p + 2),
                        p + 6);
    }

    Wm3::Segment3f LoadSegment3(const float* p) noexcept
    {
      return Wm3::Segment3f(Load3(p), Load3(p + 3), p[6]);
    }

    // A Capsule3 is a Segment3 followed by the radius.
    Wm3::Capsule3f LoadCapsule3(const float* p) noexcept
    {
      return Wm3::Capsule3f(LoadSegment3(p), p[7]);
    }

    // A Line3 is an origin and a direction - six floats.
    Wm3::Line3f LoadLine3(const float* p) noexcept
    {
      return Wm3::Line3f(Load3(p), Load3(p + 3));
    }

    // ---- the double instantiation -----------------------------------------
    //
    // A double travels as two words, so the double rows hand the loaders a
    // word array and the loaders read the halves themselves.
    Wm3::Vector3d Load3d(const std::uint32_t* p) noexcept
    {
      return Wm3::Vector3d(difffuzz::LoadDouble(p), difffuzz::LoadDouble(p + 2),
                           difffuzz::LoadDouble(p + 4));
    }

    Wm3::Vector2d Load2d(const std::uint32_t* p) noexcept
    {
      return Wm3::Vector2d(difffuzz::LoadDouble(p), difffuzz::LoadDouble(p + 2));
    }

    // A Box3 is the centre (three), the axes (three of three) and the
    // extents (three) - fifteen doubles, thirty words.
    Wm3::Box3d LoadBox3d(const std::uint32_t* p) noexcept
    {
      Wm3::Vector3d akAxis[3];
      for (int i = 0; i < 3; ++i) {
        akAxis[i] = Load3d(p + 2 * (3 + 3 * i));
      }
      double afExtent[3];
      for (int i = 0; i < 3; ++i) {
        afExtent[i] = difffuzz::LoadDouble(p + 2 * (12 + i));
      }
      return Wm3::Box3d(Load3d(p), akAxis, afExtent);
    }

    // A Box2 is the centre (two), the two axes (two of two) and the two
    // extents - eight doubles, sixteen words.
    Wm3::Box2d LoadBox2d(const std::uint32_t* p) noexcept
    {
      Wm3::Vector2d akAxis[2];
      for (int i = 0; i < 2; ++i) {
        akAxis[i] = Load2d(p + 2 * (2 + 2 * i));
      }
      double afExtent[2];
      for (int i = 0; i < 2; ++i) {
        afExtent[i] = difffuzz::LoadDouble(p + 2 * (6 + i));
      }
      return Wm3::Box2d(Load2d(p), akAxis, afExtent);
    }

    // A Segment3 is the origin, the direction and the extent - seven
    // doubles, fourteen words.
    Wm3::Segment3d LoadSegment3d(const std::uint32_t* p) noexcept
    {
      return Wm3::Segment3d(Load3d(p), Load3d(p + 6),
                            difffuzz::LoadDouble(p + 12));
    }
  } // namespace

  float DistVector3Box3GetSquared(const float* vec, const float* box)
  {
    const Wm3::Vector3f kVector = Load3(vec);
    const Wm3::Box3f kBox = LoadBox3(box);
    Wm3::DistVector3Box3f query(kVector, kBox);
    return query.GetSquared();
  }

  // ---- the double instantiation -----------------------------------------
  //
  // The rebuild's own double instantiations.  The inputs are word arrays:
  // a double is two words, so the vector is six words and the box thirty.
  double DistVector3Box3dGetSquared(const std::uint32_t* vec,
                                    const std::uint32_t* box)
  {
    const Wm3::Vector3d kVector = Load3d(vec);
    const Wm3::Box3d kBox = LoadBox3d(box);
    Wm3::DistVector3Box3d query(kVector, kBox);
    return query.GetSquared();
  }

  double DistVector3Box3dGet(const std::uint32_t* vec,
                             const std::uint32_t* box)
  {
    const Wm3::Vector3d kVector = Load3d(vec);
    const Wm3::Box3d kBox = LoadBox3d(box);
    Wm3::DistVector3Box3d query(kVector, kBox);
    return query.Get();
  }

  double DistVector3Box3dGetSwept(const std::uint32_t* vec,
                                   const std::uint32_t* box, const double fT,
                                   const std::uint32_t* vel0,
                                   const std::uint32_t* vel1)
  {
    const Wm3::Vector3d kVector = Load3d(vec);
    const Wm3::Box3d kBox = LoadBox3d(box);
    const Wm3::Vector3d kVel0 = Load3d(vel0);
    const Wm3::Vector3d kVel1 = Load3d(vel1);
    Wm3::DistVector3Box3d query(kVector, kBox);
    return query.Get(fT, kVel0, kVel1);
  }

  double DistVector3Box3dGetSquaredSwept(const std::uint32_t* vec,
                                         const std::uint32_t* box,
                                         const double fT,
                                         const std::uint32_t* vel0,
                                         const std::uint32_t* vel1)
  {
    const Wm3::Vector3d kVector = Load3d(vec);
    const Wm3::Box3d kBox = LoadBox3d(box);
    const Wm3::Vector3d kVel0 = Load3d(vel0);
    const Wm3::Vector3d kVel1 = Load3d(vel1);
    Wm3::DistVector3Box3d query(kVector, kBox);
    return query.GetSquared(fT, kVel0, kVel1);
  }

  double DistVector3Segment3dGetSquared(const std::uint32_t* vec,
                                        const std::uint32_t* segment)
  {
    const Wm3::Vector3d kVector = Load3d(vec);
    const Wm3::Segment3d kSegment = LoadSegment3d(segment);
    Wm3::DistVector3Segment3d query(kVector, kSegment);
    return query.GetSquared();
  }

  double DistVector3Segment3dGet(const std::uint32_t* vec,
                                 const std::uint32_t* segment)
  {
    const Wm3::Vector3d kVector = Load3d(vec);
    const Wm3::Segment3d kSegment = LoadSegment3d(segment);
    Wm3::DistVector3Segment3d query(kVector, kSegment);
    return query.Get();
  }

  double DistVector3Segment3dGetSwept(const std::uint32_t* vec,
                                       const std::uint32_t* segment,
                                       const double fT,
                                       const std::uint32_t* vel0,
                                       const std::uint32_t* vel1)
  {
    const Wm3::Vector3d kVector = Load3d(vec);
    const Wm3::Segment3d kSegment = LoadSegment3d(segment);
    const Wm3::Vector3d kVel0 = Load3d(vel0);
    const Wm3::Vector3d kVel1 = Load3d(vel1);
    Wm3::DistVector3Segment3d query(kVector, kSegment);
    return query.Get(fT, kVel0, kVel1);
  }

  double DistVector3Segment3dGetSquaredSwept(const std::uint32_t* vec,
                                              const std::uint32_t* segment,
                                              const double fT,
                                              const std::uint32_t* vel0,
                                              const std::uint32_t* vel1)
  {
    const Wm3::Vector3d kVector = Load3d(vec);
    const Wm3::Segment3d kSegment = LoadSegment3d(segment);
    const Wm3::Vector3d kVel0 = Load3d(vel0);
    const Wm3::Vector3d kVel1 = Load3d(vel1);
    Wm3::DistVector3Segment3d query(kVector, kSegment);
    return query.GetSquared(fT, kVel0, kVel1);
  }

  // ---- Wm3::DistVector2Box2<double> --------------------------------------
  //
  // The vector is two doubles (four words) and the box is eight doubles
  // (sixteen words).
  double DistVector2Box2dGetSquared(const std::uint32_t* vec,
                                     const std::uint32_t* box)
  {
    const Wm3::Vector2d kVector = Load2d(vec);
    const Wm3::Box2d kBox = LoadBox2d(box);
    Wm3::DistVector2Box2d query(kVector, kBox);
    return query.GetSquared();
  }

  double DistVector2Box2dGet(const std::uint32_t* vec,
                              const std::uint32_t* box)
  {
    const Wm3::Vector2d kVector = Load2d(vec);
    const Wm3::Box2d kBox = LoadBox2d(box);
    Wm3::DistVector2Box2d query(kVector, kBox);
    return query.Get();
  }

  double DistVector2Box2dGetSwept(const std::uint32_t* vec,
                                   const std::uint32_t* box, const double fT,
                                   const std::uint32_t* vel0,
                                   const std::uint32_t* vel1)
  {
    const Wm3::Vector2d kVector = Load2d(vec);
    const Wm3::Box2d kBox = LoadBox2d(box);
    const Wm3::Vector2d kVel0 = Load2d(vel0);
    const Wm3::Vector2d kVel1 = Load2d(vel1);
    Wm3::DistVector2Box2d query(kVector, kBox);
    return query.Get(fT, kVel0, kVel1);
  }

  double DistVector2Box2dGetSquaredSwept(const std::uint32_t* vec,
                                          const std::uint32_t* box,
                                          const double fT,
                                          const std::uint32_t* vel0,
                                          const std::uint32_t* vel1)
  {
    const Wm3::Vector2d kVector = Load2d(vec);
    const Wm3::Box2d kBox = LoadBox2d(box);
    const Wm3::Vector2d kVel0 = Load2d(vel0);
    const Wm3::Vector2d kVel1 = Load2d(vel1);
    Wm3::DistVector2Box2d query(kVector, kBox);
    return query.GetSquared(fT, kVel0, kVel1);
  }

  // ---- Wm3::DistSegment3Segment3<double> ----------------------------------
  //
  // Each segment is seven doubles: the origin, the direction and the extent.
  double DistSegment3Segment3dGetSquared(const std::uint32_t* seg0,
                                          const std::uint32_t* seg1)
  {
    const Wm3::Segment3d kSegment0 = LoadSegment3d(seg0);
    const Wm3::Segment3d kSegment1 = LoadSegment3d(seg1);
    Wm3::DistSegment3Segment3d query(kSegment0, kSegment1);
    return query.GetSquared();
  }

  double DistSegment3Segment3dGet(const std::uint32_t* seg0,
                                   const std::uint32_t* seg1)
  {
    const Wm3::Segment3d kSegment0 = LoadSegment3d(seg0);
    const Wm3::Segment3d kSegment1 = LoadSegment3d(seg1);
    Wm3::DistSegment3Segment3d query(kSegment0, kSegment1);
    return query.Get();
  }

  double DistSegment3Segment3dGetSwept(const std::uint32_t* seg0,
                                        const std::uint32_t* seg1,
                                        const double fT,
                                        const std::uint32_t* vel0,
                                        const std::uint32_t* vel1)
  {
    const Wm3::Segment3d kSegment0 = LoadSegment3d(seg0);
    const Wm3::Segment3d kSegment1 = LoadSegment3d(seg1);
    const Wm3::Vector3d kVel0 = Load3d(vel0);
    const Wm3::Vector3d kVel1 = Load3d(vel1);
    Wm3::DistSegment3Segment3d query(kSegment0, kSegment1);
    return query.Get(fT, kVel0, kVel1);
  }

  double DistSegment3Segment3dGetSquaredSwept(const std::uint32_t* seg0,
                                               const std::uint32_t* seg1,
                                               const double fT,
                                               const std::uint32_t* vel0,
                                               const std::uint32_t* vel1)
  {
    const Wm3::Segment3d kSegment0 = LoadSegment3d(seg0);
    const Wm3::Segment3d kSegment1 = LoadSegment3d(seg1);
    const Wm3::Vector3d kVel0 = Load3d(vel0);
    const Wm3::Vector3d kVel1 = Load3d(vel1);
    Wm3::DistSegment3Segment3d query(kSegment0, kSegment1);
    return query.GetSquared(fT, kVel0, kVel1);
  }

  // ---- Wm3::DistLine3Segment3<double> -------------------------------------
  //
  // A Line3 is an origin and a direction - six doubles, twelve words.
  Wm3::Line3d LoadLine3d(const std::uint32_t* p) noexcept
  {
    return Wm3::Line3d(Load3d(p), Load3d(p + 6));
  }

  double DistLine3Segment3dGetSquared(const std::uint32_t* line,
                                      const std::uint32_t* segment)
  {
    const Wm3::Line3d kLine = LoadLine3d(line);
    const Wm3::Segment3d kSegment = LoadSegment3d(segment);
    Wm3::DistLine3Segment3d query(kLine, kSegment);
    return query.GetSquared();
  }

  double DistLine3Segment3dGet(const std::uint32_t* line,
                               const std::uint32_t* segment)
  {
    const Wm3::Line3d kLine = LoadLine3d(line);
    const Wm3::Segment3d kSegment = LoadSegment3d(segment);
    Wm3::DistLine3Segment3d query(kLine, kSegment);
    return query.Get();
  }

  double DistLine3Segment3dGetSwept(const std::uint32_t* line,
                                     const std::uint32_t* segment,
                                     const double fT,
                                     const std::uint32_t* vel0,
                                     const std::uint32_t* vel1)
  {
    const Wm3::Line3d kLine = LoadLine3d(line);
    const Wm3::Segment3d kSegment = LoadSegment3d(segment);
    const Wm3::Vector3d kVel0 = Load3d(vel0);
    const Wm3::Vector3d kVel1 = Load3d(vel1);
    Wm3::DistLine3Segment3d query(kLine, kSegment);
    return query.Get(fT, kVel0, kVel1);
  }

  double DistLine3Segment3dGetSquaredSwept(const std::uint32_t* line,
                                            const std::uint32_t* segment,
                                            const double fT,
                                            const std::uint32_t* vel0,
                                            const std::uint32_t* vel1)
  {
    const Wm3::Line3d kLine = LoadLine3d(line);
    const Wm3::Segment3d kSegment = LoadSegment3d(segment);
    const Wm3::Vector3d kVel0 = Load3d(vel0);
    const Wm3::Vector3d kVel1 = Load3d(vel1);
    Wm3::DistLine3Segment3d query(kLine, kSegment);
    return query.GetSquared(fT, kVel0, kVel1);
  }

  // ---- Wm3::IntrBox2Circle2<double> ---------------------------------------
  //
  // A Box2 is the centre (two), the two axes (two of two) and the two
  // extents - eight doubles, sixteen words.  A Circle2 is the centre and the
  // radius - three doubles, six words.
  u32 IntrBox2Circle2dTest(const std::uint32_t* box, const std::uint32_t* circle)
  {
    const Wm3::Box2d kBox = LoadBox2d(box);
    const Wm3::Vector2d kCenter(difffuzz::LoadDouble(circle),
                                difffuzz::LoadDouble(circle + 2));
    const double fRadius = difffuzz::LoadDouble(circle + 4);
    const Wm3::Circle2d kCircle(kCenter, fRadius);
    Wm3::IntrBox2Circle2d query(kBox, kCircle);
    return query.Test() ? 1u : 0u;
  }

  u32 IntrBox2Box2dTest(const std::uint32_t* a, const std::uint32_t* b)
  {
    const Wm3::Box2d kBox0 = LoadBox2d(a);
    const Wm3::Box2d kBox1 = LoadBox2d(b);
    Wm3::IntrBox2Box2d query(kBox0, kBox1);
    return query.Test() ? 1u : 0u;
  }

  u32 IntrLine3Box3dTest(const std::uint32_t* a, const std::uint32_t* b)
  {
    const Wm3::Line3d kLine = LoadLine3d(a);
    const Wm3::Box3d kBox = LoadBox3d(b);
    Wm3::IntrLine3Box3d query(kLine, kBox);
    return query.Test() ? 1u : 0u;
  }

  u32 IntrSegment3Box3dTest(const std::uint32_t* a, const std::uint32_t* b, u32 flag)
  {
    const Wm3::Segment3d kSegment = LoadSegment3d(a);
    const Wm3::Box3d kBox = LoadBox3d(b);
    Wm3::IntrSegment3Box3d query(kSegment, kBox, flag != 0u);
    return query.Test() ? 1u : 0u;
  }

  Wm3::Sphere3d LoadSphere3d(const std::uint32_t* p) noexcept
  {
    return Wm3::Sphere3d(Load3d(p), difffuzz::LoadDouble(p + 6));
  }

  u32 IntrSegment3Sphere3dTest(const std::uint32_t* a, const std::uint32_t* b)
  {
    const Wm3::Segment3d kSegment = LoadSegment3d(a);
    const Wm3::Sphere3d kSphere = LoadSphere3d(b);
    Wm3::IntrSegment3Sphere3d query(kSegment, kSphere);
    return query.Test() ? 1u : 0u;
  }

  // The two double Find rows.  The output layout is the float rows': the hit
  // flag, the intersection type, the quantity and then the two points, each
  // three doubles.  The point is only defined after a successful Find - the
  // retail's Find writes the point array on the hit path only - so on the
  // miss path both sides write zero.
  void IntrLine3Box3dFind(const std::uint32_t* line, const std::uint32_t* box,
                          std::uint32_t* out)
  {
    const Wm3::Line3d kLine = LoadLine3d(line);
    const Wm3::Box3d kBox = LoadBox3d(box);
    Wm3::IntrLine3Box3d query(kLine, kBox);
    const bool hit = query.Find();
    const int iQuantity = query.GetQuantity();
    out[0] = hit ? 1u : 0u;
    out[1] = static_cast<std::uint32_t>(query.GetIntersectionType());
    out[2] = static_cast<std::uint32_t>(iQuantity);
    for (int i = 3; i < 15; ++i) {
      out[i] = 0u;
    }
    if (iQuantity > 0) {
      const Wm3::Vector3d& kPoint0 = query.GetPoint(0);
      difffuzz::StoreDouble(out + 3, kPoint0.x);
      difffuzz::StoreDouble(out + 5, kPoint0.y);
      difffuzz::StoreDouble(out + 7, kPoint0.z);
    }
    if (iQuantity > 1) {
      const Wm3::Vector3d& kPoint1 = query.GetPoint(1);
      difffuzz::StoreDouble(out + 9, kPoint1.x);
      difffuzz::StoreDouble(out + 11, kPoint1.y);
      difffuzz::StoreDouble(out + 13, kPoint1.z);
    }
  }

  void IntrSegment3Box3dFind(const std::uint32_t* segment,
                             const std::uint32_t* box, u32 solid,
                             std::uint32_t* out)
  {
    const Wm3::Segment3d kSegment = LoadSegment3d(segment);
    const Wm3::Box3d kBox = LoadBox3d(box);
    Wm3::IntrSegment3Box3d query(kSegment, kBox, solid != 0u);
    const bool hit = query.Find();
    const int iQuantity = query.GetQuantity();
    out[0] = hit ? 1u : 0u;
    out[1] = static_cast<std::uint32_t>(query.GetIntersectionType());
    out[2] = static_cast<std::uint32_t>(iQuantity);
    for (int i = 3; i < 15; ++i) {
      out[i] = 0u;
    }
    if (iQuantity > 0) {
      const Wm3::Vector3d& kPoint0 = query.GetPoint(0);
      difffuzz::StoreDouble(out + 3, kPoint0.x);
      difffuzz::StoreDouble(out + 5, kPoint0.y);
      difffuzz::StoreDouble(out + 7, kPoint0.z);
    }
    if (iQuantity > 1) {
      const Wm3::Vector3d& kPoint1 = query.GetPoint(1);
      difffuzz::StoreDouble(out + 9, kPoint1.x);
      difffuzz::StoreDouble(out + 11, kPoint1.y);
      difffuzz::StoreDouble(out + 13, kPoint1.z);
    }
  }

  void IntrSegment3Sphere3dFind(const std::uint32_t* segment,
                                const std::uint32_t* sphere,
                                std::uint32_t* out)
  {
    const Wm3::Segment3d kSegment = LoadSegment3d(segment);
    const Wm3::Sphere3d kSphere = LoadSphere3d(sphere);
    Wm3::IntrSegment3Sphere3d query(kSegment, kSphere);
    const bool hit = query.Find();
    const int iQuantity = query.GetQuantity();
    out[0] = hit ? 1u : 0u;
    out[1] = static_cast<std::uint32_t>(iQuantity);
    for (int i = 2; i < 18; ++i) {
      out[i] = 0u;
    }
    if (iQuantity > 0) {
      const Wm3::Vector3d& kPoint0 = query.GetPoint(0);
      difffuzz::StoreDouble(out + 2, kPoint0.x);
      difffuzz::StoreDouble(out + 4, kPoint0.y);
      difffuzz::StoreDouble(out + 6, kPoint0.z);
      difffuzz::StoreDouble(out + 14, query.GetSegmentT(0));
    }
    if (iQuantity > 1) {
      const Wm3::Vector3d& kPoint1 = query.GetPoint(1);
      difffuzz::StoreDouble(out + 8, kPoint1.x);
      difffuzz::StoreDouble(out + 10, kPoint1.y);
      difffuzz::StoreDouble(out + 12, kPoint1.z);
      difffuzz::StoreDouble(out + 16, query.GetSegmentT(1));
    }
  }

  void IntrSegment3Sphere3dFindDynamic(const std::uint32_t* segment,
                                        const std::uint32_t* sphere,
                                        const std::uint32_t* tmax,
                                        const std::uint32_t* vel0,
                                        const std::uint32_t* vel1,
                                        std::uint32_t* out)
  {
    const Wm3::Segment3d kSegment = LoadSegment3d(segment);
    const Wm3::Sphere3d kSphere = LoadSphere3d(sphere);
    Wm3::IntrSegment3Sphere3d query(kSegment, kSphere);
    const bool hit = query.Find(difffuzz::LoadDouble(tmax), Load3d(vel0),
        Load3d(vel1));
    out[0] = hit ? 1u : 0u;
    out[1] = static_cast<std::uint32_t>(query.GetIntersectionType());
    difffuzz::StoreDouble(out + 2, query.GetContactTime());
    for (int i = 4; i < 10; ++i) {
      out[i] = 0u;
    }
    if (hit) {
      // The retail's dynamic Find writes the contact point at [this+0x30]
      // and leaves the quantity at zero when the contact happens on the
      // swept path - the static Find missed, so it wrote zero there.  The
      // harness is a release build (NDEBUG), so GetPoint's assert is
      // compiled out and the point is the one the query wrote.
      const Wm3::Vector3d& kPoint = query.GetPoint(0);
      difffuzz::StoreDouble(out + 4, kPoint.x);
      difffuzz::StoreDouble(out + 6, kPoint.y);
      difffuzz::StoreDouble(out + 8, kPoint.z);
    }
  }

  u32 IntrBox3Sphere3dTest(const std::uint32_t* box,
                            const std::uint32_t* sphere)
  {
    const Wm3::Box3d kBox = LoadBox3d(box);
    const Wm3::Sphere3d kSphere = LoadSphere3d(sphere);
    Wm3::IntrBox3Sphere3d query(kBox, kSphere);
    return query.Test() ? 1u : 0u;
  }

  // A Capsule3 is a Segment3 followed by the radius.
  Wm3::Capsule3d LoadCapsule3d(const std::uint32_t* p) noexcept
  {
    return Wm3::Capsule3d(LoadSegment3d(p), difffuzz::LoadDouble(p + 14));
  }

  u32 IntrSegment3Capsule3dTest(const std::uint32_t* segment,
                                const std::uint32_t* capsule)
  {
    const Wm3::Segment3d kSegment = LoadSegment3d(segment);
    const Wm3::Capsule3d kCapsule = LoadCapsule3d(capsule);
    Wm3::IntrSegment3Capsule3d query(kSegment, kCapsule);
    return query.Test() ? 1u : 0u;
  }

  // The Find: the hit, the intersection type, the quantity and the points.
  // The parameters are left out on purpose: the retail's parameter array
  // holds *every* root the static Find produced, while the rebuild's holds
  // the roots the extent filter kept, and the two differ when the filter
  // drops the first root.
  void IntrSegment3Capsule3dFind(const std::uint32_t* segment,
                                 const std::uint32_t* capsule,
                                 std::uint32_t* out)
  {
    const Wm3::Segment3d kSegment = LoadSegment3d(segment);
    const Wm3::Capsule3d kCapsule = LoadCapsule3d(capsule);
    Wm3::IntrSegment3Capsule3d query(kSegment, kCapsule);
    const bool hit = query.Find();
    const int iQuantity = query.GetQuantity();
    out[0] = hit ? 1u : 0u;
    out[1] = static_cast<std::uint32_t>(query.GetIntersectionType());
    out[2] = static_cast<std::uint32_t>(iQuantity);
    for (int i = 3; i < 15; ++i) {
      out[i] = 0u;
    }
    if (iQuantity > 0) {
      const Wm3::Vector3d& kPoint0 = query.GetPoint(0);
      difffuzz::StoreDouble(out + 3, kPoint0.x);
      difffuzz::StoreDouble(out + 5, kPoint0.y);
      difffuzz::StoreDouble(out + 7, kPoint0.z);
    }
    if (iQuantity > 1) {
      const Wm3::Vector3d& kPoint1 = query.GetPoint(1);
      difffuzz::StoreDouble(out + 9, kPoint1.x);
      difffuzz::StoreDouble(out + 11, kPoint1.y);
      difffuzz::StoreDouble(out + 13, kPoint1.z);
    }
  }

  u32 IntrBox2Circle2Find(const float* box, const float* circle, float fTMax,
                           const float* vel0, const float* vel1)
  {
    const Wm3::Box2f kBox = LoadBox2(box);
    const Wm3::Circle2f kCircle(Load2(circle), circle[2]);
    Wm3::IntrBox2Circle2f query(kBox, kCircle);
    return query.Find(fTMax, Load2(vel0), Load2(vel1)) ? 1u : 0u;
  }


  float DistVector3Box3Get(const float* vec, const float* box)
  {
    const Wm3::Vector3f kVector = Load3(vec);
    const Wm3::Box3f kBox = LoadBox3(box);
    Wm3::DistVector3Box3f query(kVector, kBox);
    return query.Get();
  }

  float DistVector3Segment3GetSquared(const float* vec, const float* segment)
  {
    const Wm3::Vector3f kVector = Load3(vec);
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    Wm3::DistVector3Segment3f query(kVector, kSegment);
    return query.GetSquared();
  }

  float DistVector3Segment3Get(const float* vec, const float* segment)
  {
    const Wm3::Vector3f kVector = Load3(vec);
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    Wm3::DistVector3Segment3f query(kVector, kSegment);
    return query.Get();
  }

  // ---- Wm3::Eigen<float> ---------------------------------------------------
  //
  // The rebuild's Eigen<float> is the class of Foundation/Numerics/
  // Wm3Eigen.cpp; the retail's is the same class compiled by the 2007
  // compiler.  The solver is fed a symmetric matrix (the engine's Eigen
  // input is a covariance matrix) and only its upper triangle is read.
  static void FillEigenMatrix(Wm3::Eigenf& kES, const float* m, int n)
  {
    for (int r = 0; r < n; ++r)
    {
      for (int c = 0; c < n; ++c)
      {
        kES(r, c) = m[r*n + c];
      }
    }
  }

  void EigenfIncrSortOurs(const float* m, int n, u32* out)
  {
    Wm3::Eigenf kES(n);
    FillEigenMatrix(kES, m, n);
    if (n == 2)
    {
      kES.IncrSortEigenStuff2();
    }
    else
    {
      kES.IncrSortEigenStuff3();
    }

    for (int i = 0; i < n; ++i)
    {
      out[i] = difffuzz::Bits(kES.GetEigenvalue(i));
    }

    const Wm3::GMatrixf& kMat = kES.GetEigenvectors();
    for (int r = 0; r < n; ++r)
    {
      for (int c = 0; c < n; ++c)
      {
        out[n + r*n + c] = difffuzz::Bits(kMat[r][c]);
      }
    }
  }

  void EigenfGetEigenvaluesOurs(const float* m, int n, u32* out)
  {
    Wm3::Eigenf kES(n);
    FillEigenMatrix(kES, m, n);
    if (n == 2)
    {
      kES.IncrSortEigenStuff2();
    }
    else
    {
      kES.IncrSortEigenStuff3();
    }

    for (int i = 0; i < n; ++i)
    {
      out[i] = difffuzz::Bits(kES.GetEigenvalue(i));
    }
  }

  void EigenfGetEigenvectorsOurs(const float* m, int n, u32* out)
  {
    Wm3::Eigenf kES(n);
    FillEigenMatrix(kES, m, n);
    if (n == 2)
    {
      kES.IncrSortEigenStuff2();
    }
    else
    {
      kES.IncrSortEigenStuff3();
    }

    // GetEigenvector(i, Vector3&) writes the i-th column of the matrix.
    Wm3::Vector3f kV;
    for (int i = 0; i < n; ++i)
    {
      kES.GetEigenvector(i, kV);
      out[3*i + 0] = difffuzz::Bits(kV.X());
      out[3*i + 1] = difffuzz::Bits(kV.Y());
      out[3*i + 2] = difffuzz::Bits(kV.Z());
    }
  }

  void EigenfGetEigenvectors2Ours(const float* m, u32* out)
  {
    Wm3::Eigenf kES(2);
    FillEigenMatrix(kES, m, 2);
    kES.IncrSortEigenStuff2();

    Wm3::Vector2f kV;
    for (int i = 0; i < 2; ++i)
    {
      kES.GetEigenvector(i, kV);
      out[2*i + 0] = difffuzz::Bits(kV.X());
      out[2*i + 1] = difffuzz::Bits(kV.Y());
    }
  }

  // ---- Wm3::Eigen<double> --------------------------------------------------
  static void FillEigenMatrix(Wm3::Eigend& kES, const double* m, int n)
  {
    for (int r = 0; r < n; ++r)
    {
      for (int c = 0; c < n; ++c)
      {
        kES(r, c) = m[r*n + c];
      }
    }
  }

  void EigendIncrSortOurs(const double* m, int n, u32* out)
  {
    Wm3::Eigend kES(n);
    FillEigenMatrix(kES, m, n);
    if (n == 2)
    {
      kES.IncrSortEigenStuff2();
    }
    else
    {
      kES.IncrSortEigenStuff3();
    }

    for (int i = 0; i < n; ++i)
    {
      difffuzz::StoreDouble(out + 2*i, kES.GetEigenvalue(i));
    }

    const Wm3::GMatrixd& kMat = kES.GetEigenvectors();
    for (int r = 0; r < n; ++r)
    {
      for (int c = 0; c < n; ++c)
      {
        difffuzz::StoreDouble(out + 2*n + 2*(r*n + c), kMat[r][c]);
      }
    }
  }

  void EigendGetEigenvaluesOurs(const double* m, int n, u32* out)
  {
    Wm3::Eigend kES(n);
    FillEigenMatrix(kES, m, n);
    if (n == 2)
    {
      kES.IncrSortEigenStuff2();
    }
    else
    {
      kES.IncrSortEigenStuff3();
    }

    for (int i = 0; i < n; ++i)
    {
      difffuzz::StoreDouble(out + 2*i, kES.GetEigenvalue(i));
    }
  }

  void EigendGetEigenvectorsOurs(const double* m, int n, u32* out)
  {
    Wm3::Eigend kES(n);
    FillEigenMatrix(kES, m, n);
    if (n == 2)
    {
      kES.IncrSortEigenStuff2();
    }
    else
    {
      kES.IncrSortEigenStuff3();
    }

    // GetEigenvector(i, Vector3&) writes the i-th column of the matrix.
    Wm3::Vector3d kV;
    for (int i = 0; i < n; ++i)
    {
      kES.GetEigenvector(i, kV);
      difffuzz::StoreDouble(out + 6*i + 0, kV.X());
      difffuzz::StoreDouble(out + 6*i + 2, kV.Y());
      difffuzz::StoreDouble(out + 6*i + 4, kV.Z());
    }
  }

  void EigendGetEigenvectors2Ours(const double* m, u32* out)
  {
    Wm3::Eigend kES(2);
    FillEigenMatrix(kES, m, 2);
    kES.IncrSortEigenStuff2();

    Wm3::Vector2d kV;
    for (int i = 0; i < 2; ++i)
    {
      kES.GetEigenvector(i, kV);
      difffuzz::StoreDouble(out + 4*i + 0, kV.X());
      difffuzz::StoreDouble(out + 4*i + 2, kV.Y());
    }
  }

  u32 IntrBox2Circle2Test(const float* box, const float* circle)
  {
    const Wm3::Box2f kBox = LoadBox2(box);
    const Wm3::Circle2f kCircle(Load2(circle), circle[2]);
    Wm3::IntrBox2Circle2f query(kBox, kCircle);
    return query.Test() ? 1u : 0u;
  }


  u32 IntrBox3Sphere3Find(const float* box, const float* sphere, float fTMax,
                           const float* vel0, const float* vel1)
  {
    const Wm3::Box3f kBox = LoadBox3(box);
    const Wm3::Sphere3f kSphere(Load3(sphere), sphere[3]);
    Wm3::IntrBox3Sphere3f query(kBox, kSphere);
    return query.Find(fTMax, Load3(vel0), Load3(vel1)) ? 1u : 0u;
  }

  void IntrBox3Sphere3GetContactPoint(const float* box, const float* sphere,
                                      float fTMax, const float* vel0,
                                      const float* vel1, float* out3)
  {
    const Wm3::Box3f kBox = LoadBox3(box);
    const Wm3::Sphere3f kSphere(Load3(sphere), sphere[3]);
    Wm3::IntrBox3Sphere3f query(kBox, kSphere);
    query.Find(fTMax, Load3(vel0), Load3(vel1));
    // The contact point is only *defined* on one of the three paths Find can
    // take, and the gate is the intersection type, not Find's return value:
    //
    //   IT_OTHER (8)  the sphere centre is inside the box.  Find writes the
    //                 sphere centre into m_kContactPoint and 0 into the
    //                 contact time, and returns true.  Retail 0x00A44150-ish:
    //                 "fstp dword ptr [esi+0x14]" plus "mov [esi+8],8".
    //   IT_EMPTY (0)  Find failed: an intersection was found (this algorithm
    //                 reports the *inverse* of what the name suggests - a
    //                 region helper returns 1 for a hit and Find turns that
    //                 into IT_EMPTY + false).  m_kContactPoint is never
    //                 written at all.
    //   IT_POINT (1)  Find succeeded *because there was no intersection*
    //                 (a region helper returned 0).  m_kContactPoint is
    //                 written, but from the region helpers' fIx/fIy/fIz
    //                 outputs, which that path never sets - so it is
    //                 uninitialised stack on both sides.
    //
    // Comparing the point outside the IT_OTHER case compares uninitialised
    // memory against uninitialised memory: two different pieces of garbage,
    // on every draw.  The contact time is written on every path that has a
    // defined value for it, so it is compared unconditionally.
    if (query.GetIntersectionType() == Wm3::Intersector3f::IT_OTHER) {
      const Wm3::Vector3f& kPoint = query.GetContactPoint();
      out3[0] = kPoint.x;
      out3[1] = kPoint.y;
      out3[2] = kPoint.z;
    }
    else {
      out3[0] = out3[1] = out3[2] = 0.0f;
    }
    out3[3] = query.GetContactTime();
  }

  u32 IntrLine3Box3Test(const float* line, const float* box)
  {
    const Wm3::Line3f kLine(Load3(line), Load3(line + 3));
    const Wm3::Box3f kBox = LoadBox3(box);
    Wm3::IntrLine3Box3f query(kLine, kBox);
    return query.Test() ? 1u : 0u;
  }

  u32 IntrSegment3Box3Find(const float* segment, const float* box, u32 solid)
  {
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    const Wm3::Box3f kBox = LoadBox3(box);
    Wm3::IntrSegment3Box3f query(kSegment, kBox, solid != 0u);
    return query.Find() ? 1u : 0u;
  }

  void DistVector3Segment3GetClosestPoint1(const float* vec, const float* segment,
                                           float* out3)
  {
    const Wm3::Vector3f kVector = Load3(vec);
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    Wm3::DistVector3Segment3f query(kVector, kSegment);
    query.GetSquared(); // fills m_kClosestPoint1 on every branch
    const Wm3::Vector3f& kPoint = query.GetClosestPoint1();
    out3[0] = kPoint.x;
    out3[1] = kPoint.y;
    out3[2] = kPoint.z;
  }

  u32 IntrBox3Sphere3Test(const float* box, const float* sphere)
  {
    const Wm3::Box3f kBox = LoadBox3(box);
    const Wm3::Sphere3f kSphere(Load3(sphere), sphere[3]);
    Wm3::IntrBox3Sphere3f query(kBox, kSphere);
    return query.Test() ? 1u : 0u;
  }

  // The two helpers are private statics of the WM3 class, so the harness
  // cannot name them directly.  They are reached through the "Rob" idiom: an
  // explicit instantiation, whose template argument is not access checked
  // ([temp.spec]/6), hands the private member's address over.  The address
  // taken is the private one, so the call names the same symbol the class's
  // own .cpp instantiates - the engine's own code, not a copy of it.
  namespace
  {
    using GetVertexFn = float (*)(float, float, float, float, float, float, float);
    using GetEdgeFn = float (*)(float, float, float, float, float, float);

    struct GetVertexTag
    {
      using type = GetVertexFn;
      friend type get(GetVertexTag);
    };

    struct GetEdgeTag
    {
      using type = GetEdgeFn;
      friend type get(GetEdgeTag);
    };

    template <typename Tag, typename Tag::type M>
    struct Rob
    {
      friend typename Tag::type get(Tag) { return M; }
    };

    template struct Rob<GetVertexTag, &Wm3::IntrBox3Sphere3f::GetVertexIntersection>;
    template struct Rob<GetEdgeTag, &Wm3::IntrBox3Sphere3f::GetEdgeIntersection>;

    // The four region helpers are private *member* functions, so what the
    // idiom hands over is a pointer to member rather than a plain function
    // pointer.  The explicit instantiation still bypasses the access check
    // ([temp.spec]/6), and the pointer names the same member the class's own
    // .cpp instantiates - the engine's code, not a copy of it.
    //
    // FindFaceRegionIntersection and FindEdgeRegionIntersection have the same
    // shape, so their pointers have the same type; the tags still differ,
    // because the members differ.
    using FaceRegionFn = int (Wm3::IntrBox3Sphere3f::*)(float, float, float,
        float, float, float, float, float, float, float&, float&, float&,
        bool);
    using JustEdgeFn = int (Wm3::IntrBox3Sphere3f::*)(float, float, float,
        float, float, float, float, float, float, float&, float&, float&);
    using EdgeRegionFn = FaceRegionFn;
    using VertexRegionFn = JustEdgeFn;

    struct FaceRegionTag
    {
      using type = FaceRegionFn;
      friend type get(FaceRegionTag);
    };

    struct JustEdgeTag
    {
      using type = JustEdgeFn;
      friend type get(JustEdgeTag);
    };

    struct EdgeRegionTag
    {
      using type = EdgeRegionFn;
      friend type get(EdgeRegionTag);
    };

    struct VertexRegionTag
    {
      using type = VertexRegionFn;
      friend type get(VertexRegionTag);
    };

    template struct Rob<FaceRegionTag, &Wm3::IntrBox3Sphere3f::FindFaceRegionIntersection>;
    template struct Rob<JustEdgeTag, &Wm3::IntrBox3Sphere3f::FindJustEdgeIntersection>;
    template struct Rob<EdgeRegionTag, &Wm3::IntrBox3Sphere3f::FindEdgeRegionIntersection>;
    template struct Rob<VertexRegionTag, &Wm3::IntrBox3Sphere3f::FindVertexRegionIntersection>;

    // The intersection coordinates of the region helpers are only defined on
    // the hit path: the "no hit" and the "already intersecting" paths write
    // none of them.  Both sides therefore start them from this sentinel and
    // compare the raw bits - a path that writes nothing leaves the sentinel
    // on both sides, and a path that writes a different coordinate than the
    // other side shows up as a mismatch instead of as a coincidence of
    // uninitialised memory.
    const float kUndefCoord = difffuzz::FromBits(0x7FC00001u);

    // ---- IntrBox2Circle2's two region helpers ----------------------------
    //
    // TestVertexRegion and TestEdgeRegion are private *member* functions of
    // the WM3 class, so what the "Rob" idiom hands over is a pointer to
    // member - the explicit instantiation bypasses the access check
    // ([temp.spec]/6), and the pointer names the member the class's own .cpp
    // instantiates, so the call runs the engine's code and not a copy.
    //
    // The two members have the same shape - eight arguments, the last two
    // being the coordinates they write - so their pointers have the same
    // type; the tags still differ, because the members differ.
    using RegionFn = int (Wm3::IntrBox2Circle2f::*)(float, float, float, float,
        float, float, float&, float&);

    struct B2CVertexRegionTag
    {
      using type = RegionFn;
      friend type get(B2CVertexRegionTag);
    };

    struct B2CEdgeRegionTag
    {
      using type = RegionFn;
      friend type get(B2CEdgeRegionTag);
    };

    template struct Rob<B2CVertexRegionTag,
        &Wm3::IntrBox2Circle2f::TestVertexRegion>;
    template struct Rob<B2CEdgeRegionTag,
        &Wm3::IntrBox2Circle2f::TestEdgeRegion>;

    // ---- Wm3::IntrLine3Box3<float>::Clip --------------------------------
    //
    // Clip is a *private static* member of the WM3 class, so the harness
    // cannot name it directly.  The "Rob" idiom reaches it the same way the
    // two static helpers above are reached: an explicit instantiation, whose
    // template argument is not access checked ([temp.spec]/6), hands the
    // private member's address over.  The address taken is the private one,
    // so the call names the same symbol the class's own .cpp instantiates -
    // the engine's own code, not a copy of it.
    using ClipFn = bool (*)(float, float, float&, float&);

    struct ClipTag
    {
      using type = ClipFn;
      friend type get(ClipTag);
    };

    template struct Rob<ClipTag, &Wm3::IntrLine3Box3f::Clip>;

    // ---- Wm3::Distance<float,Vector3f>'s difference step --------------------
    //
    // The two members are protected, so the "Rob" idiom reaches them the same
    // way it reaches a private static member: an explicit instantiation,
    // whose template argument is not access checked ([temp.spec]/6), hands
    // the member's address over.  Here the "address" is a pointer to member,
    // so the idiom carries a pointer-to-member type.
    using DiffStepMember = float Wm3::Distance3f::*;

    struct DiffStepTag
    {
      using type = DiffStepMember;
      friend type get(DiffStepTag);
    };

    struct DiffInvTag
    {
      using type = DiffStepMember;
      friend type get(DiffInvTag);
    };

    template struct Rob<DiffStepTag, &Wm3::Distance3f::m_fDifferenceStep>;
    template struct Rob<DiffInvTag, &Wm3::Distance3f::m_fInvTwoDifferenceStep>;

    // ---- Wm3::Distance<double,Vector3d>'s difference step ------------------
    //
    // The same two members in the double instantiation.  The pointer-to-
    // member type is the double one, so the tags are separate: a tag names
    // one member type, and the "Rob" idiom's explicit instantiation is what
    // hands the protected member's address over.
    using DiffStep3dMember = double Wm3::Distance3d::*;
    using DiffStep2dMember = double Wm3::Distance2d::*;

    struct DiffStep3dTag
    {
      using type = DiffStep3dMember;
      friend type get(DiffStep3dTag);
    };

    struct DiffInv3dTag
    {
      using type = DiffStep3dMember;
      friend type get(DiffInv3dTag);
    };

    struct DiffStep2dTag
    {
      using type = DiffStep2dMember;
      friend type get(DiffStep2dTag);
    };

    struct DiffInv2dTag
    {
      using type = DiffStep2dMember;
      friend type get(DiffInv2dTag);
    };

    template struct Rob<DiffStep3dTag, &Wm3::Distance3d::m_fDifferenceStep>;
    template struct Rob<DiffInv3dTag, &Wm3::Distance3d::m_fInvTwoDifferenceStep>;
    template struct Rob<DiffStep2dTag, &Wm3::Distance2d::m_fDifferenceStep>;
    template struct Rob<DiffInv2dTag, &Wm3::Distance2d::m_fInvTwoDifferenceStep>;
  } // namespace

  float UndefCoord()
  {
    return kUndefCoord;
  }

  float IntrBox3Sphere3GetVertexIntersection(float fDx, float fDy, float fDz,
                                             float fVx, float fVy, float fVz,
                                             float fRSqr)
  {
    return get(GetVertexTag{})(fDx, fDy, fDz, fVx, fVy, fVz, fRSqr);
  }

  float IntrBox3Sphere3GetEdgeIntersection(float fDx, float fDz, float fVx,
                                           float fVz, float fVSqr, float fRSqr)
  {
    return get(GetEdgeTag{})(fDx, fDz, fVx, fVz, fVSqr, fRSqr);
  }

  // ---- The four region helpers -------------------------------------------
  //
  // They are private member functions of the WM3 class, so the harness
  // reaches them through the same "Rob" idiom as the two static helpers
  // above, but with a pointer to member: the pointer names the member the
  // class's own .cpp instantiates, and the call runs that member on a
  // live object.
  //
  // The object carries a box and a sphere, and the helpers read exactly one
  // thing from it: the sphere's radius.  The contact time they write is
  // readable through the public GetContactTime().
  //
  // The three intersection coordinates are only written on the hit path, so
  // the harness starts them from a sentinel and hands the caller the raw
  // bits; the retail side starts from the same sentinel, and the two then
  // agree on the paths that write nothing and disagree on the paths that
  // write different things.
  int IntrBox3Sphere3FindFaceRegionIntersection(const float* box,
      const float* sphere, float fEx, float fEy, float fEz, float fCx,
      float fCy, float fCz, float fVx, float fVy, float fVz, u32 above,
      float* out3, float* outTime)
  {
    Wm3::Box3f kBox = LoadBox3(box);
    Wm3::Sphere3f kSphere(Load3(sphere), sphere[3]);
    Wm3::IntrBox3Sphere3f kQuery(kBox, kSphere);
    float fIx = kUndefCoord;
    float fIy = kUndefCoord;
    float fIz = kUndefCoord;
    const int iRet = (kQuery.*get(FaceRegionTag{}))(fEx, fEy, fEz, fCx, fCy,
        fCz, fVx, fVy, fVz, fIx, fIy, fIz, above != 0u);
    out3[0] = fIx;
    out3[1] = fIy;
    out3[2] = fIz;
    *outTime = kQuery.GetContactTime();
    return iRet;
  }

  int IntrBox3Sphere3FindJustEdgeIntersection(const float* box,
      const float* sphere, float fCy, float fEx, float fEy, float fEz,
      float fDx, float fDz, float fVx, float fVy, float fVz, float* out3,
      float* outTime)
  {
    Wm3::Box3f kBox = LoadBox3(box);
    Wm3::Sphere3f kSphere(Load3(sphere), sphere[3]);
    Wm3::IntrBox3Sphere3f kQuery(kBox, kSphere);
    float fIx = kUndefCoord;
    float fIy = kUndefCoord;
    float fIz = kUndefCoord;
    const int iRet = (kQuery.*get(JustEdgeTag{}))(fCy, fEx, fEy, fEz, fDx, fDz,
        fVx, fVy, fVz, fIx, fIy, fIz);
    out3[0] = fIx;
    out3[1] = fIy;
    out3[2] = fIz;
    *outTime = kQuery.GetContactTime();
    return iRet;
  }

  int IntrBox3Sphere3FindEdgeRegionIntersection(const float* box,
      const float* sphere, float fEx, float fEy, float fEz, float fCx,
      float fCy, float fCz, float fVx, float fVy, float fVz, u32 above,
      float* out3, float* outTime)
  {
    Wm3::Box3f kBox = LoadBox3(box);
    Wm3::Sphere3f kSphere(Load3(sphere), sphere[3]);
    Wm3::IntrBox3Sphere3f kQuery(kBox, kSphere);
    float fIx = kUndefCoord;
    float fIy = kUndefCoord;
    float fIz = kUndefCoord;
    const int iRet = (kQuery.*get(EdgeRegionTag{}))(fEx, fEy, fEz, fCx, fCy,
        fCz, fVx, fVy, fVz, fIx, fIy, fIz, above != 0u);
    out3[0] = fIx;
    out3[1] = fIy;
    out3[2] = fIz;
    *outTime = kQuery.GetContactTime();
    return iRet;
  }

  int IntrBox3Sphere3FindVertexRegionIntersection(const float* box,
      const float* sphere, float fEx, float fEy, float fEz, float fCx,
      float fCy, float fCz, float fVx, float fVy, float fVz, float* out3,
      float* outTime)
  {
    Wm3::Box3f kBox = LoadBox3(box);
    Wm3::Sphere3f kSphere(Load3(sphere), sphere[3]);
    Wm3::IntrBox3Sphere3f kQuery(kBox, kSphere);
    float fIx = kUndefCoord;
    float fIy = kUndefCoord;
    float fIz = kUndefCoord;
    const int iRet = (kQuery.*get(VertexRegionTag{}))(fEx, fEy, fEz, fCx,
        fCy, fCz, fVx, fVy, fVz, fIx, fIy, fIz);
    out3[0] = fIx;
    out3[1] = fIy;
    out3[2] = fIz;
    *outTime = kQuery.GetContactTime();
    return iRet;
  }

  void IntrSegment3Box3GetPoint(const float* segment, const float* box, u32 solid,
                                u32 index, float* out3)
  {
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    const Wm3::Box3f kBox = LoadBox3(box);
    Wm3::IntrSegment3Box3f query(kSegment, kBox, solid != 0u);
    if (!query.Find()) {
      // m_akPoint is only written when the segment actually intersects, so
      // there is nothing defined to read otherwise.
      out3[0] = out3[1] = out3[2] = 0.0f;
      return;
    }
    const Wm3::Vector3f& kPoint = query.GetPoint(static_cast<int>(index));
    out3[0] = kPoint.x;
    out3[1] = kPoint.y;
    out3[2] = kPoint.z;
  }

  u32 IntrSegment3Sphere3Find(const float* segment, const float* sphere)
  {
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    const Wm3::Sphere3f kSphere(Load3(sphere), sphere[3]);
    Wm3::IntrSegment3Sphere3f query(kSegment, kSphere);
    return query.Find() ? 1u : 0u;
  }

  u32 IntrSegment3Sphere3Test(const float* segment, const float* sphere)
  {
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    const Wm3::Sphere3f kSphere(Load3(sphere), sphere[3]);
    Wm3::IntrSegment3Sphere3f query(kSegment, kSphere);
    return query.Test() ? 1u : 0u;
  }

  u32 IntrSegment3Sphere3TestDynamic(const float* segment, const float* sphere,
                                     float fTMax, const float* vel0,
                                     const float* vel1)
  {
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    const Wm3::Sphere3f kSphere(Load3(sphere), sphere[3]);
    Wm3::IntrSegment3Sphere3f query(kSegment, kSphere);
    return query.Test(fTMax, Load3(vel0), Load3(vel1)) ? 1u : 0u;
  }

  u32 IntrSegment3Sphere3FindDynamic(const float* segment, const float* sphere,
                                     float fTMax, const float* vel0,
                                     const float* vel1)
  {
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    const Wm3::Sphere3f kSphere(Load3(sphere), sphere[3]);
    Wm3::IntrSegment3Sphere3f query(kSegment, kSphere);
    return query.Find(fTMax, Load3(vel0), Load3(vel1)) ? 1u : 0u;
  }

  void IntrSegment3Sphere3GetPoint(const float* segment, const float* sphere,
                                   u32 index, float* out3)
  {
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    const Wm3::Sphere3f kSphere(Load3(sphere), sphere[3]);
    Wm3::IntrSegment3Sphere3f query(kSegment, kSphere);
    if (!query.Find()) {
      // Find() leaves m_akPoint untouched on the miss path, and the source's
      // own GetPoint asserts 0 <= i < m_iQuantity, so there is nothing
      // defined to read. Both sides write zero and agree trivially.
      out3[0] = out3[1] = out3[2] = 0.0f;
      return;
    }
    // Find() returning true means m_iQuantity is 1 or 2, so index 0 is always
    // valid. Index 1 would need m_iQuantity, which the generator cannot know
    // before running Find - the same narrowing as IntrSegment3Box3::GetPoint.
    const Wm3::Vector3f& kPoint = query.GetPoint(static_cast<int>(index));
    out3[0] = kPoint.x;
    out3[1] = kPoint.y;
    out3[2] = kPoint.z;
  }

  u32 IntrBox2Box2Test(const float* box0, const float* box1)
  {
    const Wm3::Box2f kBox0 = LoadBox2(box0);
    const Wm3::Box2f kBox1 = LoadBox2(box1);
    Wm3::IntrBox2Box2f query(kBox0, kBox1);
    return query.Test() ? 1u : 0u;
  }

  float DistSegment3Segment3GetSquared(const float* seg0, const float* seg1)
  {
    const Wm3::Segment3f kSeg0 = LoadSegment3(seg0);
    const Wm3::Segment3f kSeg1 = LoadSegment3(seg1);
    Wm3::DistSegment3Segment3f query(kSeg0, kSeg1);
    return query.GetSquared();
  }

  float DistSegment3Segment3Get(const float* seg0, const float* seg1)
  {
    const Wm3::Segment3f kSeg0 = LoadSegment3(seg0);
    const Wm3::Segment3f kSeg1 = LoadSegment3(seg1);
    Wm3::DistSegment3Segment3f query(kSeg0, kSeg1);
    return query.Get();
  }

  float DistLine3Segment3GetSquared(const float* line, const float* segment)
  {
    const Wm3::Line3f kLine = LoadLine3(line);
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    Wm3::DistLine3Segment3f query(kLine, kSegment);
    return query.GetSquared();
  }

  float DistLine3Segment3Get(const float* line, const float* segment)
  {
    const Wm3::Line3f kLine = LoadLine3(line);
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    Wm3::DistLine3Segment3f query(kLine, kSegment);
    return query.Get();
  }

  float DistVector2Box2GetSquared(const float* vec, const float* box)
  {
    const Wm3::Vector2f kVector = Load2(vec);
    const Wm3::Box2f kBox = LoadBox2(box);
    Wm3::DistVector2Box2f query(kVector, kBox);
    return query.GetSquared();
  }

  u32 IntrSegment3Capsule3Test(const float* segment, const float* capsule)
  {
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    const Wm3::Capsule3f kCapsule = LoadCapsule3(capsule);
    Wm3::IntrSegment3Capsule3f query(kSegment, kCapsule);
    return query.Test() ? 1u : 0u;
  }

  void IntrSegment3Capsule3Find(const float* segment, const float* capsule,
                                u32* out)
  {
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    const Wm3::Capsule3f kCapsule = LoadCapsule3(capsule);
    Wm3::IntrSegment3Capsule3f query(kSegment, kCapsule);
    const bool hit = query.Find();
    out[0] = hit ? 1u : 0u;
    if (!hit) {
      // On the miss path the intersection set is empty: the quantity is zero
      // and there is no parameter and no point to read. Both sides write zero.
      out[1] = out[2] = out[3] = out[4] = out[5] = 0u;
      return;
    }
    out[1] = static_cast<u32>(query.GetQuantity());
    out[2] = difffuzz::Bits(query.GetParameter(0));
    const Wm3::Vector3f& kPoint = query.GetPoint(0);
    out[3] = difffuzz::Bits(kPoint.x);
    out[4] = difffuzz::Bits(kPoint.y);
    out[5] = difffuzz::Bits(kPoint.z);
  }

  void IntrLine3Box3Find(const float* line, const float* box, u32* out)
  {
    const Wm3::Line3f kLine(Load3(line), Load3(line + 3));
    const Wm3::Box3f kBox = LoadBox3(box);
    Wm3::IntrLine3Box3f query(kLine, kBox);
    const bool hit = query.Find();
    const int iQuantity = query.GetQuantity();
    out[0] = hit ? 1u : 0u;
    out[1] = static_cast<u32>(query.GetIntersectionType());
    out[2] = static_cast<u32>(iQuantity);
    out[3] = out[4] = out[5] = out[6] = out[7] = out[8] = 0u;
    if (iQuantity > 0) {
      const Wm3::Vector3f& kPoint0 = query.GetPoint(0);
      out[3] = difffuzz::Bits(kPoint0.x);
      out[4] = difffuzz::Bits(kPoint0.y);
      out[5] = difffuzz::Bits(kPoint0.z);
    }
    if (iQuantity > 1) {
      const Wm3::Vector3f& kPoint1 = query.GetPoint(1);
      out[6] = difffuzz::Bits(kPoint1.x);
      out[7] = difffuzz::Bits(kPoint1.y);
      out[8] = difffuzz::Bits(kPoint1.z);
    }
  }

  void DistVector2Box2Get(const float* vec, const float* box, u32* out)
  {
    const Wm3::Vector2f kVector = Load2(vec);
    const Wm3::Box2f kBox = LoadBox2(box);
    Wm3::DistVector2Box2f query(kVector, kBox);
    out[0] = difffuzz::Bits(query.Get());
  }

  void IntrSegment3Capsule3GetPoint(const float* segment, const float* capsule,
                                    u32* out)
  {
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    const Wm3::Capsule3f kCapsule = LoadCapsule3(capsule);
    Wm3::IntrSegment3Capsule3f query(kSegment, kCapsule);
    out[0] = out[1] = out[2] = 0u;
    if (!query.Find()) {
      // On the miss path the intersection set is empty: there is no point to
      // read. Both sides write zero.
      return;
    }
    const Wm3::Vector3f& kPoint = query.GetPoint(0);
    out[0] = difffuzz::Bits(kPoint.x);
    out[1] = difffuzz::Bits(kPoint.y);
    out[2] = difffuzz::Bits(kPoint.z);
  }

  void IntrSegment3Capsule3GetParameter(const float* segment,
                                        const float* capsule, u32* out)
  {
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    const Wm3::Capsule3f kCapsule = LoadCapsule3(capsule);
    Wm3::IntrSegment3Capsule3f query(kSegment, kCapsule);
    out[0] = 0u;
    if (!query.Find()) {
      // On the miss path the intersection set is empty: there is no parameter
      // to read. Both sides write zero.
      return;
    }
    out[0] = difffuzz::Bits(query.GetParameter(0));
  }

  void IntrLine3Capsule3Find(const float* line, const float* capsule, u32* out)
  {
    const Wm3::Vector3f kOrigin = Load3(line);
    const Wm3::Vector3f kDir = Load3(line + 3);
    const Wm3::Capsule3f kCapsule = LoadCapsule3(capsule);
    float afT[2] = { 0.0f, 0.0f };
    const int iQuantity =
      Wm3::IntrLine3Capsule3f::Find(kOrigin, kDir, kCapsule, afT);
    out[0] = static_cast<u32>(iQuantity);
    // A root is only written when the returned count covers it.
    out[1] = (iQuantity > 0) ? difffuzz::Bits(afT[0]) : 0u;
    out[2] = (iQuantity > 1) ? difffuzz::Bits(afT[1]) : 0u;
  }

  u32 IntrLine3Capsule3Test(const float* line, const float* capsule)
  {
    const Wm3::Line3f kLine(Load3(line), Load3(line + 3));
    const Wm3::Capsule3f kCapsule = LoadCapsule3(capsule);
    Wm3::IntrLine3Capsule3f query(kLine, kCapsule);
    return query.Test() ? 1u : 0u;
  }

  void ConvexHull3Canonical(int dimension, int simplexQuantity,
                            const int* indices, const float* linePlane,
                            u32* out)
  {
    out[0] = static_cast<u32>(dimension);
    out[1] = static_cast<u32>(simplexQuantity);
    for (int i = 0; i < 15; ++i) {
      out[2 + i] = difffuzz::Bits(linePlane[i]);
    }

    std::vector<std::array<int,3>> akTriangle;
    akTriangle.reserve(static_cast<std::size_t>(simplexQuantity > 0
        ? simplexQuantity : 0));
    for (int t = 0; t < simplexQuantity; ++t) {
      const int a = indices[3*t + 0];
      const int b = indices[3*t + 1];
      const int c = indices[3*t + 2];
      // Rotate so that the smallest index comes first. The rotation keeps the
      // winding, so two hulls that agree on the triangles agree on their
      // orientation as well.
      std::array<int,3> kTriangle;
      if (a <= b && a <= c) {
        kTriangle = {a, b, c};
      }
      else if (b <= c) {
        kTriangle = {b, c, a};
      }
      else {
        kTriangle = {c, a, b};
      }
      akTriangle.push_back(kTriangle);
    }
    std::sort(akTriangle.begin(), akTriangle.end());

    for (std::size_t i = 0; i < akTriangle.size(); ++i) {
      const int iWord = 17 + 3*static_cast<int>(i);
      if (iWord + 2 >= 48) {
        break;
      }
      out[iWord + 0] = static_cast<u32>(akTriangle[i][0]);
      out[iWord + 1] = static_cast<u32>(akTriangle[i][1]);
      out[iWord + 2] = static_cast<u32>(akTriangle[i][2]);
    }
  }

  int Query3ToPlaneOurs(const float* verts, int n, const float* point, int iV0,
                        int iV1, int iV2)
  {
    Wm3::Vector3f* akVertex = new Wm3::Vector3f[n];
    for (int i = 0; i < n; ++i) {
      akVertex[i] = Wm3::Vector3f(verts[3*i + 0], verts[3*i + 1],
                                   verts[3*i + 2]);
    }
    {
      Wm3::Query3f kQuery(n, akVertex);
      const Wm3::Vector3f kPoint(point[0], point[1], point[2]);
      const int iSign = kQuery.ToPlane(kPoint, iV0, iV1, iV2);
      delete[] akVertex;
      return iSign;
    }
  }

  // The Query3<float> family, forwarded to the rebuild's own members. The
  // rebuild keeps the WM4 declaration order, so the index-taking overload
  // comes first in its vtable and the point-taking one second; the retail's
  // table is the other way round. Nothing here depends on the order - every
  // call is by name.
  int Query3ToPlaneIndexOurs(const float* verts, int n, int i, int iV0,
                              int iV1, int iV2)
  {
    Wm3::Vector3f* akVertex = new Wm3::Vector3f[n];
    for (int k = 0; k < n; ++k) {
      akVertex[k] = Wm3::Vector3f(verts[3*k + 0], verts[3*k + 1],
                                   verts[3*k + 2]);
    }
    {
      Wm3::Query3f kQuery(n, akVertex);
      const int iSign = kQuery.ToPlane(i, iV0, iV1, iV2);
      delete[] akVertex;
      return iSign;
    }
  }

  int Query3ToTetrahedronOurs(const float* verts, int n, const float* point,
                               int iV0, int iV1, int iV2, int iV3)
  {
    Wm3::Vector3f* akVertex = new Wm3::Vector3f[n];
    for (int k = 0; k < n; ++k) {
      akVertex[k] = Wm3::Vector3f(verts[3*k + 0], verts[3*k + 1],
                                   verts[3*k + 2]);
    }
    {
      Wm3::Query3f kQuery(n, akVertex);
      const Wm3::Vector3f kPoint(point[0], point[1], point[2]);
      const int iSign = kQuery.ToTetrahedron(kPoint, iV0, iV1, iV2, iV3);
      delete[] akVertex;
      return iSign;
    }
  }

  int Query3ToTetrahedronIndexOurs(const float* verts, int n, int i, int iV0,
                                    int iV1, int iV2, int iV3)
  {
    Wm3::Vector3f* akVertex = new Wm3::Vector3f[n];
    for (int k = 0; k < n; ++k) {
      akVertex[k] = Wm3::Vector3f(verts[3*k + 0], verts[3*k + 1],
                                   verts[3*k + 2]);
    }
    {
      Wm3::Query3f kQuery(n, akVertex);
      const int iSign = kQuery.ToTetrahedron(i, iV0, iV1, iV2, iV3);
      delete[] akVertex;
      return iSign;
    }
  }

  int Query3ToCircumsphereOurs(const float* verts, int n, const float* point,
                                int iV0, int iV1, int iV2, int iV3)
  {
    Wm3::Vector3f* akVertex = new Wm3::Vector3f[n];
    for (int k = 0; k < n; ++k) {
      akVertex[k] = Wm3::Vector3f(verts[3*k + 0], verts[3*k + 1],
                                   verts[3*k + 2]);
    }
    {
      Wm3::Query3f kQuery(n, akVertex);
      const Wm3::Vector3f kPoint(point[0], point[1], point[2]);
      const int iSign = kQuery.ToCircumsphere(kPoint, iV0, iV1, iV2, iV3);
      delete[] akVertex;
      return iSign;
    }
  }

  int Query3ToCircumsphereIndexOurs(const float* verts, int n, int i, int iV0,
                                     int iV1, int iV2, int iV3)
  {
    Wm3::Vector3f* akVertex = new Wm3::Vector3f[n];
    for (int k = 0; k < n; ++k) {
      akVertex[k] = Wm3::Vector3f(verts[3*k + 0], verts[3*k + 1],
                                   verts[3*k + 2]);
    }
    {
      Wm3::Query3f kQuery(n, akVertex);
      const int iSign = kQuery.ToCircumsphere(i, iV0, iV1, iV2, iV3);
      delete[] akVertex;
      return iSign;
    }
  }

  // The Query2<float> family, likewise forwarded by name.
  int Query2ToLineOurs(const float* verts, int n, const float* point, int iV0,
                        int iV1)
  {
    Wm3::Vector2f* akVertex = new Wm3::Vector2f[n];
    for (int k = 0; k < n; ++k) {
      akVertex[k] = Wm3::Vector2f(verts[2*k + 0], verts[2*k + 1]);
    }
    {
      Wm3::Query2f kQuery(n, akVertex);
      const Wm3::Vector2f kPoint(point[0], point[1]);
      const int iSign = kQuery.ToLine(kPoint, iV0, iV1);
      delete[] akVertex;
      return iSign;
    }
  }

  int Query2ToLineIndexOurs(const float* verts, int n, int i, int iV0,
                             int iV1)
  {
    Wm3::Vector2f* akVertex = new Wm3::Vector2f[n];
    for (int k = 0; k < n; ++k) {
      akVertex[k] = Wm3::Vector2f(verts[2*k + 0], verts[2*k + 1]);
    }
    {
      Wm3::Query2f kQuery(n, akVertex);
      const int iSign = kQuery.ToLine(i, iV0, iV1);
      delete[] akVertex;
      return iSign;
    }
  }

  int Query2ToTriangleOurs(const float* verts, int n, const float* point,
                           int iV0, int iV1, int iV2)
  {
    Wm3::Vector2f* akVertex = new Wm3::Vector2f[n];
    for (int k = 0; k < n; ++k) {
      akVertex[k] = Wm3::Vector2f(verts[2*k + 0], verts[2*k + 1]);
    }
    {
      Wm3::Query2f kQuery(n, akVertex);
      const Wm3::Vector2f kPoint(point[0], point[1]);
      const int iSign = kQuery.ToTriangle(kPoint, iV0, iV1, iV2);
      delete[] akVertex;
      return iSign;
    }
  }

  int Query2ToTriangleIndexOurs(const float* verts, int n, int i, int iV0,
                                int iV1, int iV2)
  {
    Wm3::Vector2f* akVertex = new Wm3::Vector2f[n];
    for (int k = 0; k < n; ++k) {
      akVertex[k] = Wm3::Vector2f(verts[2*k + 0], verts[2*k + 1]);
    }
    {
      Wm3::Query2f kQuery(n, akVertex);
      const int iSign = kQuery.ToTriangle(i, iV0, iV1, iV2);
      delete[] akVertex;
      return iSign;
    }
  }

  int Query2ToCircumcircleOurs(const float* verts, int n, const float* point,
                                int iV0, int iV1, int iV2)
  {
    Wm3::Vector2f* akVertex = new Wm3::Vector2f[n];
    for (int k = 0; k < n; ++k) {
      akVertex[k] = Wm3::Vector2f(verts[2*k + 0], verts[2*k + 1]);
    }
    {
      Wm3::Query2f kQuery(n, akVertex);
      const Wm3::Vector2f kPoint(point[0], point[1]);
      const int iSign = kQuery.ToCircumcircle(kPoint, iV0, iV1, iV2);
      delete[] akVertex;
      return iSign;
    }
  }

  int Query2ToCircumcircleIndexOurs(const float* verts, int n, int i, int iV0,
                                     int iV1, int iV2)
  {
    Wm3::Vector2f* akVertex = new Wm3::Vector2f[n];
    for (int k = 0; k < n; ++k) {
      akVertex[k] = Wm3::Vector2f(verts[2*k + 0], verts[2*k + 1]);
    }
    {
      Wm3::Query2f kQuery(n, akVertex);
      const int iSign = kQuery.ToCircumcircle(i, iV0, iV1, iV2);
      delete[] akVertex;
      return iSign;
    }
  }

  // The comparison words of the one-dimensional hull.
  void ConvexHull1Canonical(int dimension, int simplexQuantity,
                            const int* indices, u32* out)
  {
    out[0] = static_cast<u32>(dimension);
    out[1] = static_cast<u32>(simplexQuantity);
    int i0 = 0;
    int i1 = 0;
    if (indices != 0 && simplexQuantity >= 2) {
      i0 = indices[0];
      i1 = indices[1];
      if (i1 < i0) {
        const int iTmp = i0;
        i0 = i1;
        i1 = iTmp;
      }
    }
    out[2] = static_cast<u32>(i0);
    out[3] = static_cast<u32>(i1);
  }

  // The comparison words of the two-dimensional hull.
  //
  // The hull's index array holds *one* index per edge - the first vertex of
  // the edge - and the edges are the cyclic pairs of that array, because the
  // edge list is walked in the polygon's order and each edge ends where the
  // next one starts.
  void ConvexHull2Canonical(int dimension, int simplexQuantity,
                            const int* indices, u32* out)
  {
    out[0] = static_cast<u32>(dimension);
    out[1] = static_cast<u32>(simplexQuantity);

    std::vector<std::array<int,2>> akEdge;
    if (indices != 0 && dimension == 2 && simplexQuantity > 0) {
      akEdge.reserve(static_cast<std::size_t>(simplexQuantity));
      for (int i = 0; i < simplexQuantity; ++i) {
        int a = indices[i];
        int b = indices[(i + 1) % simplexQuantity];
        if (b < a) {
          const int iTmp = a;
          a = b;
          b = iTmp;
        }
        akEdge.push_back({a, b});
      }
    }
    std::sort(akEdge.begin(), akEdge.end());

    for (std::size_t i = 0; i < akEdge.size(); ++i) {
      const int iWord = 2 + 2*static_cast<int>(i);
      if (iWord + 1 >= 20) {
        break;
      }
      out[iWord + 0] = static_cast<u32>(akEdge[i][0]);
      out[iWord + 1] = static_cast<u32>(akEdge[i][1]);
    }
    for (int i = 2 + 2*static_cast<int>(akEdge.size()); i < 20; ++i) {
      out[i] = 0u;
    }
  }

  void ConvexHull1Ours(const float* values, int n, float eps, u32 type,
                       u32* out)
  {
    float* afVertex = new float[n > 0 ? n : 1];
    for (int i = 0; i < n; ++i) {
      afVertex[i] = values[i];
    }
    {
      // The hull does not own the values (bOwner is false), so the array
      // outlives the hull and is deleted below.
      Wm3::ConvexHull1f kHull(n, afVertex, eps, false,
                              static_cast<Wm3::Query::Type>(type));
      ConvexHull1Canonical(kHull.GetDimension(), kHull.GetSimplexQuantity(),
                           kHull.GetIndices(), out);
    }
    delete[] afVertex;
  }

  void ConvexHull2Ours(const float* points, int n, float eps, u32 type,
                       u32* out)
  {
    Wm3::Vector2f* akVertex = new Wm3::Vector2f[n > 0 ? n : 1];
    for (int i = 0; i < n; ++i) {
      akVertex[i] = Wm3::Vector2f(points[2*i + 0], points[2*i + 1]);
    }
    {
      Wm3::ConvexHull2f kHull(n, akVertex, eps, false,
                              static_cast<Wm3::Query::Type>(type));
      ConvexHull2Canonical(kHull.GetDimension(), kHull.GetSimplexQuantity(),
                           kHull.GetIndices(), out);
    }
    delete[] akVertex;
  }

  void ConvexHull3Ours(const float* points, int n, float eps, u32 type,
                       u32* out)  {
    Wm3::Vector3f* akVertex = new Wm3::Vector3f[n];
    for (int i = 0; i < n; ++i) {
      akVertex[i] = Wm3::Vector3f(points[3*i + 0], points[3*i + 1],
                                   points[3*i + 2]);
    }
    {
      // The hull does not own the vertices (bOwner is false), so the array
      // outlives the hull and is deleted below.
      Wm3::ConvexHull3f kHull(n, akVertex, eps, false,
                              static_cast<Wm3::Query::Type>(type));
      float akLinePlane[15];
      const Wm3::Vector3f& kLineOrigin = kHull.GetLineOrigin();
      const Wm3::Vector3f& kLineDirection = kHull.GetLineDirection();
      const Wm3::Vector3f& kPlaneOrigin = kHull.GetPlaneOrigin();
      const Wm3::Vector3f& kPlaneDirection0 = kHull.GetPlaneDirection(0);
      const Wm3::Vector3f& kPlaneDirection1 = kHull.GetPlaneDirection(1);
      const Wm3::Vector3f* apkSource[5] = {
        &kLineOrigin, &kLineDirection, &kPlaneOrigin, &kPlaneDirection0,
        &kPlaneDirection1 };
      for (int v = 0; v < 5; ++v) {
        for (int c = 0; c < 3; ++c) {
          akLinePlane[3*v + c] = (*apkSource[v])[c];
        }
      }
      ConvexHull3Canonical(kHull.GetDimension(), kHull.GetSimplexQuantity(),
                           kHull.GetIndices(), akLinePlane, out);
    }
    delete[] akVertex;
  }

  float DistVector3Segment3GetDerivative(const float* vec, const float* segment,
                                          float fT, const float* vel0,
                                          const float* vel1)
  {
    const Wm3::Vector3f kVector = Load3(vec);
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    const Wm3::Vector3f kVel0 = Load3(vel0);
    const Wm3::Vector3f kVel1 = Load3(vel1);
    Wm3::DistVector3Segment3f query(kVector, kSegment);
    return query.GetDerivative(fT, kVel0, kVel1);
  }

  float DistVector3Segment3GetDerivativeSquared(const float* vec,
                                                  const float* segment, float fT,
                                                  const float* vel0,
                                                  const float* vel1)
  {
    const Wm3::Vector3f kVector = Load3(vec);
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    const Wm3::Vector3f kVel0 = Load3(vel0);
    const Wm3::Vector3f kVel1 = Load3(vel1);
    Wm3::DistVector3Segment3f query(kVector, kSegment);
    return query.GetDerivativeSquared(fT, kVel0, kVel1);
  }

  u32 IntrSegment3Box3Test(const float* segment, const float* box, u32 solid)
  {
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    const Wm3::Box3f kBox = LoadBox3(box);
    Wm3::IntrSegment3Box3f query(kSegment, kBox, solid != 0u);
    return query.Test() ? 1u : 0u;
  }

  float DistVector3Segment3Get(const float* vec, const float* segment, float fT,
                                const float* vel0, const float* vel1)
  {
    const Wm3::Vector3f kVector = Load3(vec);
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    const Wm3::Vector3f kVel0 = Load3(vel0);
    const Wm3::Vector3f kVel1 = Load3(vel1);
    Wm3::DistVector3Segment3f query(kVector, kSegment);
    return query.Get(fT, kVel0, kVel1);
  }

  float DistVector3Segment3GetSquared(const float* vec, const float* segment,
                                      float fT, const float* vel0,
                                      const float* vel1)
  {
    const Wm3::Vector3f kVector = Load3(vec);
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    const Wm3::Vector3f kVel0 = Load3(vel0);
    const Wm3::Vector3f kVel1 = Load3(vel1);
    Wm3::DistVector3Segment3f query(kVector, kSegment);
    return query.GetSquared(fT, kVel0, kVel1);
  }

  u32 InBox(const float* point, const float* box)
  {
    const Wm3::Vector3f kPoint = Load3(point);
    const Wm3::Box3f kBox = LoadBox3(box);
    return Wm3::InBox(kPoint, kBox) ? 1u : 0u;
  }

  void ContAlignedBox(int quantity, const float* points, u32* out)
  {
    Wm3::Vector3f akPoint[8];
    for (int i = 0; i < 8; ++i) {
      akPoint[i] = Load3(points + 3*i);
    }
    const Wm3::Box3f kBox = Wm3::ContAlignedBox(quantity, akPoint);
    const float* f = reinterpret_cast<const float*>(&kBox);
    for (int i = 0; i < 15; ++i) {
      out[i] = difffuzz::Bits(f[i]);
    }
  }

  void ContAlignedBoxMask(int quantity, const float* points, const bool* mask,
                          u32* out)
  {
    Wm3::Vector3f akPoint[8];
    for (int i = 0; i < 8; ++i) {
      akPoint[i] = Load3(points + 3*i);
    }
    const Wm3::Box3f kBox = Wm3::ContAlignedBox(quantity, akPoint, mask);
    const float* f = reinterpret_cast<const float*>(&kBox);
    for (int i = 0; i < 15; ++i) {
      out[i] = difffuzz::Bits(f[i]);
    }
  }

  // ---- the 2D ContAlignedBox ---------------------------------------------
  //
  // The same function in two dimensions: the box is written as the centre
  // (two), the two axes (four) and the two extents (two) - eight floats.
  void ContAlignedBox2(int quantity, const float* points, u32* out)
  {
    Wm3::Vector2f akPoint[8];
    for (int i = 0; i < 8; ++i) {
      akPoint[i] = Load2(points + 2*i);
    }
    const Wm3::Box2f kBox = Wm3::ContAlignedBox(quantity, akPoint);
    const float* f = reinterpret_cast<const float*>(&kBox);
    for (int i = 0; i < 8; ++i) {
      out[i] = difffuzz::Bits(f[i]);
    }
  }

  void ContAlignedBox2Mask(int quantity, const float* points, const bool* mask,
                           u32* out)
  {
    Wm3::Vector2f akPoint[8];
    for (int i = 0; i < 8; ++i) {
      akPoint[i] = Load2(points + 2*i);
    }
    const Wm3::Box2f kBox = Wm3::ContAlignedBox(quantity, akPoint, mask);
    const float* f = reinterpret_cast<const float*>(&kBox);
    for (int i = 0; i < 8; ++i) {
      out[i] = difffuzz::Bits(f[i]);
    }
  }

  // ---- IntrBox2Circle2's two region helpers ------------------------------
  //
  // They are private member functions of the WM3 class, so the harness
  // reaches them through the same "Rob" idiom as IntrBox3Sphere3's region
  // helpers, but with a pointer to member: the pointer names the member the
  // class's own .cpp instantiates, and the call runs that member on a live
  // object.
  //
  // The object carries a box and a circle, and the helpers read exactly one
  // thing from it: the circle's radius. The contact time they write is
  // readable through the public GetContactTime().
  //
  // The two intersection coordinates are only written on the hit path, so
  // the harness starts them from a sentinel and hands the caller the raw
  // bits; the retail side starts from the same sentinel, and the two then
  // agree on the paths that write nothing and disagree on the paths that
  // write different things.
  int IntrBox2Circle2TestVertexRegion(const float* box, const float* circle,
      float fCx, float fCy, float fVx, float fVy, float fEx, float fEy,
      float* out2, float* outTime)
  {
    Wm3::Box2f kBox = LoadBox2(box);
    Wm3::Circle2f kCircle(Load2(circle), circle[2]);
    Wm3::IntrBox2Circle2f kQuery(kBox, kCircle);
    float fIx = kUndefCoord;
    float fIy = kUndefCoord;
    const int iRet = (kQuery.*get(B2CVertexRegionTag{}))(fCx, fCy, fVx, fVy,
        fEx, fEy, fIx, fIy);
    out2[0] = fIx;
    out2[1] = fIy;
    *outTime = kQuery.GetContactTime();
    return iRet;
  }

  int IntrBox2Circle2TestEdgeRegion(const float* box, const float* circle,
      float fCx, float fCy, float fVx, float fVy, float fEx, float fEy,
      float* out2, float* outTime)
  {
    Wm3::Box2f kBox = LoadBox2(box);
    Wm3::Circle2f kCircle(Load2(circle), circle[2]);
    Wm3::IntrBox2Circle2f kQuery(kBox, kCircle);
    float fIx = kUndefCoord;
    float fIy = kUndefCoord;
    const int iRet = (kQuery.*get(B2CEdgeRegionTag{}))(fCx, fCy, fVx, fVy,
        fEx, fEy, fIx, fIy);
    out2[0] = fIx;
    out2[1] = fIy;
    *outTime = kQuery.GetContactTime();
    return iRet;
  }

  // ---- the swept Distance queries ----------------------------------------
  //
  // The derived class's own virtuals, the ones the class vtable puts at
  // +0x08 (Get) and +0x14 (GetSquared).  The retail's body moves the two
  // shapes by fT*velocity and then asks the *static* query of the moved
  // pair, which is what the rebuilt source does too.

  float DistVector3Box3GetSwept(const float* vec, const float* box, float fT,
                                const float* vel0, const float* vel1)
  {
    const Wm3::Vector3f kVector = Load3(vec);
    const Wm3::Box3f kBox = LoadBox3(box);
    const Wm3::Vector3f kVel0 = Load3(vel0);
    const Wm3::Vector3f kVel1 = Load3(vel1);
    Wm3::DistVector3Box3f query(kVector, kBox);
    return query.Get(fT, kVel0, kVel1);
  }

  float DistVector3Box3GetSquaredSwept(const float* vec, const float* box,
                                        float fT, const float* vel0,
                                        const float* vel1)
  {
    const Wm3::Vector3f kVector = Load3(vec);
    const Wm3::Box3f kBox = LoadBox3(box);
    const Wm3::Vector3f kVel0 = Load3(vel0);
    const Wm3::Vector3f kVel1 = Load3(vel1);
    Wm3::DistVector3Box3f query(kVector, kBox);
    return query.GetSquared(fT, kVel0, kVel1);
  }

  float DistVector2Box2GetSwept(const float* vec, const float* box, float fT,
                                const float* vel0, const float* vel1)
  {
    const Wm3::Vector2f kVector = Load2(vec);
    const Wm3::Box2f kBox = LoadBox2(box);
    const Wm3::Vector2f kVel0 = Load2(vel0);
    const Wm3::Vector2f kVel1 = Load2(vel1);
    Wm3::DistVector2Box2f query(kVector, kBox);
    return query.Get(fT, kVel0, kVel1);
  }

  float DistVector2Box2GetSquaredSwept(const float* vec, const float* box,
                                        float fT, const float* vel0,
                                        const float* vel1)
  {
    const Wm3::Vector2f kVector = Load2(vec);
    const Wm3::Box2f kBox = LoadBox2(box);
    const Wm3::Vector2f kVel0 = Load2(vel0);
    const Wm3::Vector2f kVel1 = Load2(vel1);
    Wm3::DistVector2Box2f query(kVector, kBox);
    return query.GetSquared(fT, kVel0, kVel1);
  }

  float DistSegment3Segment3GetSwept(const float* seg0, const float* seg1,
                                     float fT, const float* vel0,
                                     const float* vel1)
  {
    const Wm3::Segment3f kSeg0 = LoadSegment3(seg0);
    const Wm3::Segment3f kSeg1 = LoadSegment3(seg1);
    const Wm3::Vector3f kVel0 = Load3(vel0);
    const Wm3::Vector3f kVel1 = Load3(vel1);
    Wm3::DistSegment3Segment3f query(kSeg0, kSeg1);
    return query.Get(fT, kVel0, kVel1);
  }

  float DistSegment3Segment3GetSquaredSwept(const float* seg0,
                                             const float* seg1, float fT,
                                             const float* vel0,
                                             const float* vel1)
  {
    const Wm3::Segment3f kSeg0 = LoadSegment3(seg0);
    const Wm3::Segment3f kSeg1 = LoadSegment3(seg1);
    const Wm3::Vector3f kVel0 = Load3(vel0);
    const Wm3::Vector3f kVel1 = Load3(vel1);
    Wm3::DistSegment3Segment3f query(kSeg0, kSeg1);
    return query.GetSquared(fT, kVel0, kVel1);
  }

  // ---- Wm3::DistLine3Segment3<float> ---------------------------------------

  float DistLine3Segment3GetSwept(const float* line, const float* segment,
                                   float fT, const float* vel0,
                                   const float* vel1)
  {
    const Wm3::Line3f kLine = LoadLine3(line);
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    const Wm3::Vector3f kVel0 = Load3(vel0);
    const Wm3::Vector3f kVel1 = Load3(vel1);
    Wm3::DistLine3Segment3f query(kLine, kSegment);
    return query.Get(fT, kVel0, kVel1);
  }

  float DistLine3Segment3GetSquaredSwept(const float* line,
                                          const float* segment, float fT,
                                          const float* vel0,
                                          const float* vel1)
  {
    const Wm3::Line3f kLine = LoadLine3(line);
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    const Wm3::Vector3f kVel0 = Load3(vel0);
    const Wm3::Vector3f kVel1 = Load3(vel1);
    Wm3::DistLine3Segment3f query(kLine, kSegment);
    return query.GetSquared(fT, kVel0, kVel1);
  }

  // ---- Wm3::IntrLine3Box3<float>::DoClipping and ::Clip -------------------

  u32 IntrLine3Box3DoClipping(float fT0, float fT1, const float* origin,
                              const float* direction, const float* box,
                              u32 solid, u32* quantity, float* points,
                              u32* type)
  {
    const Wm3::Vector3f kOrigin = Load3(origin);
    const Wm3::Vector3f kDirection = Load3(direction);
    const Wm3::Box3f kBox = LoadBox3(box);
    // The two points are only written on the paths that define them, so both
    // sides start them from the same sentinel and the raw bits are compared.
    Wm3::Vector3f akPoint[2] = { Wm3::Vector3f(kUndefCoord, kUndefCoord,
                                               kUndefCoord),
                                 Wm3::Vector3f(kUndefCoord, kUndefCoord,
                                               kUndefCoord) };
    int iQuantity = 0;
    int iType = 0;
    const bool hit = Wm3::IntrLine3Box3f::DoClipping(fT0, fT1, kOrigin,
        kDirection, kBox, solid != 0u, iQuantity, akPoint, iType);
    quantity[0] = static_cast<u32>(iQuantity);
    type[0] = static_cast<u32>(iType);
    points[0] = akPoint[0].x;
    points[1] = akPoint[0].y;
    points[2] = akPoint[0].z;
    points[3] = akPoint[1].x;
    points[4] = akPoint[1].y;
    points[5] = akPoint[1].z;
    return hit ? 1u : 0u;
  }

  u32 IntrLine3Box3Clip(float denom, float numer, float* t0, float* t1)
  {
    float fT0 = *t0;
    float fT1 = *t1;
    const bool ok = get(ClipTag{})(denom, numer, fT0, fT1);
    *t0 = fT0;
    *t1 = fT1;
    return ok ? 1u : 0u;
  }

  // ---- the base class's window queries -----------------------------------
  //
  // The base's own vftable - the one the base constructor installs - puts
  // Get(fTMin,fTMax,v0,v1) at +0x04 and GetSquared(fTMin,fTMax,v0,v1) at
  // +0x10.  Both are the base's minimisation over the interval, and both
  // reach the derived class's own swept Get through the vtable - which is
  // why the row runs on a DistVector3Segment3f object and not on the base.

  float DistVector3Segment3WindowGet(const float* vec, const float* segment,
                                     float fTMin, float fTMax,
                                     const float* vel0, const float* vel1)
  {
    const Wm3::Vector3f kVector = Load3(vec);
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    const Wm3::Vector3f kVel0 = Load3(vel0);
    const Wm3::Vector3f kVel1 = Load3(vel1);
    Wm3::DistVector3Segment3f query(kVector, kSegment);
    // The derived class declares its own three-argument Get, which hides the
    // base's four-argument one, so the base's window query is reached
    // through a base reference.  It is still a virtual call: the derived
    // class does not override it, so the base's body runs - and the body's
    // own calls to Get(fT,v0,v1) reach the derived class's swept Get.
    Wm3::Distance3f& kBase = query;
    return kBase.Get(fTMin, fTMax, kVel0, kVel1);
  }

  // ---- Wm3::Distance<float,Vector3f>'s difference step --------------------
  //
  // The base's constructor is protected, so the object is a derived one -
  // DistVector3Segment3f - whose Distance<float,Vector3f> base subobject is
  // the class the row is about.  The two members the row compares are the
  // difference step and its reciprocal.
  namespace
  {
    Wm3::DistVector3Segment3f MakeDistanceObject()
    {
      return Wm3::DistVector3Segment3f(
          Wm3::Vector3f(0.0f, 0.0f, 0.0f),
          Wm3::Segment3f(Wm3::Vector3f(0.0f, 0.0f, 0.0f),
                         Wm3::Vector3f(1.0f, 0.0f, 0.0f), 1.0f));
    }
  }

  void DistanceSetDifferenceStep(float step, u32* out)
  {
    Wm3::DistVector3Segment3f query = MakeDistanceObject();
    query.SetDifferenceStep(step);
    out[0] = difffuzz::Bits(query.GetDifferenceStep());
    out[1] = difffuzz::Bits(query.*get(DiffInvTag{}));
  }

  void DistanceDifferenceStepDefault(u32* out)
  {
    Wm3::DistVector3Segment3f query = MakeDistanceObject();
    out[0] = difffuzz::Bits(query.GetDifferenceStep());
    out[1] = difffuzz::Bits(query.*get(DiffInvTag{}));
  }

  // ---- the double instantiation -----------------------------------------
  //
  // The base class's constructor is protected, so the object is a derived
  // one: DistVector3Segment3<double> for the Vector3 base and
  // DistVector2Box2<double> for the Vector2 one - the two double
  // instantiations the harness already compiles.  The two members the rows
  // compare are the difference step and its reciprocal, and a double travels
  // as two words.
  namespace
  {
    Wm3::DistVector3Segment3d MakeDistanceObject3d()
    {
      return Wm3::DistVector3Segment3d(
          Wm3::Vector3d(0.0, 0.0, 0.0),
          Wm3::Segment3d(Wm3::Vector3d(0.0, 0.0, 0.0),
                         Wm3::Vector3d(1.0, 0.0, 0.0), 1.0));
    }

    Wm3::DistVector2Box2d MakeDistanceObject2d()
    {
      Wm3::Box2d kBox(Wm3::Vector2d(0.0, 0.0),
                      Wm3::Vector2d(1.0, 0.0), Wm3::Vector2d(0.0, 1.0),
                      1.0, 1.0);
      return Wm3::DistVector2Box2d(Wm3::Vector2d(0.0, 0.0), kBox);
    }
  } // namespace

  void Distance3dSetDifferenceStep(double step, u32* out)
  {
    Wm3::DistVector3Segment3d query = MakeDistanceObject3d();
    query.SetDifferenceStep(step);
    difffuzz::StoreDouble(out, query.GetDifferenceStep());
    difffuzz::StoreDouble(out + 2, query.*get(DiffInv3dTag{}));
  }

  void Distance3dDifferenceStepDefault(u32* out)
  {
    Wm3::DistVector3Segment3d query = MakeDistanceObject3d();
    difffuzz::StoreDouble(out, query.GetDifferenceStep());
    difffuzz::StoreDouble(out + 2, query.*get(DiffInv3dTag{}));
  }

  void Distance2dSetDifferenceStep(double step, u32* out)
  {
    Wm3::DistVector2Box2d query = MakeDistanceObject2d();
    query.SetDifferenceStep(step);
    difffuzz::StoreDouble(out, query.GetDifferenceStep());
    difffuzz::StoreDouble(out + 2, query.*get(DiffInv2dTag{}));
  }

  void Distance2dDifferenceStepDefault(u32* out)
  {
    Wm3::DistVector2Box2d query = MakeDistanceObject2d();
    difffuzz::StoreDouble(out, query.GetDifferenceStep());
    difffuzz::StoreDouble(out + 2, query.*get(DiffInv2dTag{}));
  }

  float DistVector3Segment3WindowGetSquared(const float* vec,
                                            const float* segment, float fTMin,
                                            float fTMax, const float* vel0,
                                            const float* vel1)
  {
    const Wm3::Vector3f kVector = Load3(vec);
    const Wm3::Segment3f kSegment = LoadSegment3(segment);
    const Wm3::Vector3f kVel0 = Load3(vel0);
    const Wm3::Vector3f kVel1 = Load3(vel1);
    Wm3::DistVector3Segment3f query(kVector, kSegment);
    Wm3::Distance3f& kBase = query;
    return kBase.GetSquared(fTMin, fTMax, kVel0, kVel1);
  }

  // ---- the double base's own four bodies ---------------------------------
  //
  // The four bodies are the base class's members, so the object is a derived
  // one - the same two the difference-step rows use - and the call goes
  // through a reference to the base: the bodies dispatch through the
  // vtable, so the derived class's own swept Get is what answers.
  double Distance3dWindowGet(const std::uint32_t* vec,
                             const std::uint32_t* segment,
                             const double fTMin, const double fTMax,
                             const std::uint32_t* vel0,
                             const std::uint32_t* vel1)
  {
    const Wm3::Vector3d kVector = Load3d(vec);
    const Wm3::Segment3d kSegment = LoadSegment3d(segment);
    const Wm3::Vector3d kVel0 = Load3d(vel0);
    const Wm3::Vector3d kVel1 = Load3d(vel1);
    Wm3::DistVector3Segment3d query(kVector, kSegment);
    Wm3::Distance3d& kBase = query;
    return kBase.Get(fTMin, fTMax, kVel0, kVel1);
  }

  double Distance3dWindowGetSquared(const std::uint32_t* vec,
                                    const std::uint32_t* segment,
                                    const double fTMin, const double fTMax,
                                    const std::uint32_t* vel0,
                                    const std::uint32_t* vel1)
  {
    const Wm3::Vector3d kVector = Load3d(vec);
    const Wm3::Segment3d kSegment = LoadSegment3d(segment);
    const Wm3::Vector3d kVel0 = Load3d(vel0);
    const Wm3::Vector3d kVel1 = Load3d(vel1);
    Wm3::DistVector3Segment3d query(kVector, kSegment);
    Wm3::Distance3d& kBase = query;
    return kBase.GetSquared(fTMin, fTMax, kVel0, kVel1);
  }

  double Distance3dGetDerivative(const std::uint32_t* vec,
                                 const std::uint32_t* segment, const double fT,
                                 const std::uint32_t* vel0,
                                 const std::uint32_t* vel1)
  {
    const Wm3::Vector3d kVector = Load3d(vec);
    const Wm3::Segment3d kSegment = LoadSegment3d(segment);
    const Wm3::Vector3d kVel0 = Load3d(vel0);
    const Wm3::Vector3d kVel1 = Load3d(vel1);
    Wm3::DistVector3Segment3d query(kVector, kSegment);
    Wm3::Distance3d& kBase = query;
    return kBase.GetDerivative(fT, kVel0, kVel1);
  }

  double Distance3dGetDerivativeSquared(const std::uint32_t* vec,
                                        const std::uint32_t* segment,
                                        const double fT,
                                        const std::uint32_t* vel0,
                                        const std::uint32_t* vel1)
  {
    const Wm3::Vector3d kVector = Load3d(vec);
    const Wm3::Segment3d kSegment = LoadSegment3d(segment);
    const Wm3::Vector3d kVel0 = Load3d(vel0);
    const Wm3::Vector3d kVel1 = Load3d(vel1);
    Wm3::DistVector3Segment3d query(kVector, kSegment);
    Wm3::Distance3d& kBase = query;
    return kBase.GetDerivativeSquared(fT, kVel0, kVel1);
  }

  double Distance2dWindowGet(const std::uint32_t* vec,
                             const std::uint32_t* box,
                             const double fTMin, const double fTMax,
                             const std::uint32_t* vel0,
                             const std::uint32_t* vel1)
  {
    const Wm3::Vector2d kVector = Load2d(vec);
    const Wm3::Box2d kBox = LoadBox2d(box);
    const Wm3::Vector2d kVel0 = Load2d(vel0);
    const Wm3::Vector2d kVel1 = Load2d(vel1);
    Wm3::DistVector2Box2d query(kVector, kBox);
    Wm3::Distance2d& kBase = query;
    return kBase.Get(fTMin, fTMax, kVel0, kVel1);
  }

  double Distance2dWindowGetSquared(const std::uint32_t* vec,
                                    const std::uint32_t* box,
                                    const double fTMin, const double fTMax,
                                    const std::uint32_t* vel0,
                                    const std::uint32_t* vel1)
  {
    const Wm3::Vector2d kVector = Load2d(vec);
    const Wm3::Box2d kBox = LoadBox2d(box);
    const Wm3::Vector2d kVel0 = Load2d(vel0);
    const Wm3::Vector2d kVel1 = Load2d(vel1);
    Wm3::DistVector2Box2d query(kVector, kBox);
    Wm3::Distance2d& kBase = query;
    return kBase.GetSquared(fTMin, fTMax, kVel0, kVel1);
  }

  double Distance2dGetDerivative(const std::uint32_t* vec,
                                 const std::uint32_t* box, const double fT,
                                 const std::uint32_t* vel0,
                                 const std::uint32_t* vel1)
  {
    const Wm3::Vector2d kVector = Load2d(vec);
    const Wm3::Box2d kBox = LoadBox2d(box);
    const Wm3::Vector2d kVel0 = Load2d(vel0);
    const Wm3::Vector2d kVel1 = Load2d(vel1);
    Wm3::DistVector2Box2d query(kVector, kBox);
    Wm3::Distance2d& kBase = query;
    return kBase.GetDerivative(fT, kVel0, kVel1);
  }

  double Distance2dGetDerivativeSquared(const std::uint32_t* vec,
                                        const std::uint32_t* box,
                                        const double fT,
                                        const std::uint32_t* vel0,
                                        const std::uint32_t* vel1)
  {
    const Wm3::Vector2d kVector = Load2d(vec);
    const Wm3::Box2d kBox = LoadBox2d(box);
    const Wm3::Vector2d kVel0 = Load2d(vel0);
    const Wm3::Vector2d kVel1 = Load2d(vel1);
    Wm3::DistVector2Box2d query(kVector, kBox);
    Wm3::Distance2d& kBase = query;
    return kBase.GetDerivativeSquared(fT, kVel0, kVel1);
  }
  // ---- Wm3::Vector3<Real>::GenerateOrthonormalBasis ------------------------
  //
  // The static member of the Vector3 template.  The rebuild's float
  // instantiation is the one the engine uses and is left exactly as it was;
  // the double instantiation is the one the retail runs as x87 code under
  // the engine's `_PC_24`, and the rebuild's double branch spells its
  // arithmetic in x87 instructions.
  void Vector3GenerateOrthonormalBasisd(const std::uint32_t* in,
                                        std::uint32_t* out)
  {
    Wm3::Vector3d kW(difffuzz::LoadDouble(in), difffuzz::LoadDouble(in + 2),
                      difffuzz::LoadDouble(in + 4));
    Wm3::Vector3d kU, kV;
    Wm3::Vector3d::GenerateOrthonormalBasis(kU, kV, kW, in[6] != 0);
    for (int i = 0; i < 3; ++i) {
      difffuzz::StoreDouble(out + 2*i, kU.m_afTuple[i]);
      difffuzz::StoreDouble(out + 6 + 2*i, kV.m_afTuple[i]);
    }
  }

  void Vector3GenerateOrthonormalBasisf(const std::uint32_t* in,
                                        std::uint32_t* out)
  {
    Wm3::Vector3f kW(difffuzz::FromBits(in[0]), difffuzz::FromBits(in[1]),
                      difffuzz::FromBits(in[2]));
    Wm3::Vector3f kU, kV;
    Wm3::Vector3f::GenerateOrthonormalBasis(kU, kV, kW, in[3] != 0);
    for (int i = 0; i < 3; ++i) {
      out[i] = difffuzz::Bits(kU.m_afTuple[i]);
      out[3 + i] = difffuzz::Bits(kV.m_afTuple[i]);
    }
  }

  // ---- Wm3::Vector3<double>::Normalize ------------------------------------
  //
  // The member of the double instantiation.  The rebuild's double branch is
  // spelled in x87 instructions (Foundation/Math/Wm3Vector3.inl), so the
  // length it returns is the retail's 24-bit one and the vector it leaves
  // behind is scaled by the x87's reciprocal.
  void Vector3NormalizeD(const std::uint32_t* in, std::uint32_t* out)
  {
    Wm3::Vector3d kV(difffuzz::LoadDouble(in), difffuzz::LoadDouble(in + 2),
                     difffuzz::LoadDouble(in + 4));
    const double dLength = kV.Normalize();
    for (int i = 0; i < 3; ++i) {
      difffuzz::StoreDouble(out + 2*i, kV.m_afTuple[i]);
    }
    difffuzz::StoreDouble(out + 6, dLength);
  }

  // ---- Wm3::ConvexHull<double> --------------------------------------------
  void ConvexHull2dOurs(const double* points, int n, double eps, u32 type,
                        u32* out)
  {
    Wm3::Vector2d* akVertex = new Wm3::Vector2d[n > 0 ? n : 1];
    for (int i = 0; i < n; ++i)
    {
      akVertex[i] = Wm3::Vector2d(points[2*i + 0], points[2*i + 1]);
    }
    {
      // The hull does not own the vertices (bOwner is false), so the array
      // outlives the hull and is deleted below.
      Wm3::ConvexHull2d kHull(n, akVertex, eps, false,
                              static_cast<Wm3::Query::Type>(type));
      ConvexHull2Canonical(kHull.GetDimension(), kHull.GetSimplexQuantity(),
                           kHull.GetIndices(), out);
    }
    delete[] akVertex;
  }


  u32 Vector3LengthOurs(const float* v)
  {
    Wm3::Vector3f kV(v[0], v[1], v[2]);
    return difffuzz::Bits(kV.Length());
  }

  u32 Vector3CompareArraysOurs(const float* a, const float* b)
  {
    // The bytes go in untouched: the member compares the twelve bytes of the
    // two vectors, so a NaN payload and a signed zero have to survive the
    // trip into the object.
    Wm3::Vector3f kA;
    Wm3::Vector3f kB;
    std::memcpy(&kA, a, 3 * sizeof(float));
    std::memcpy(&kB, b, 3 * sizeof(float));
    return static_cast<std::uint32_t>(kA.CompareArrays(kB));
  }

  u32 Vector3OperatorEquals(const float* a, const float* b)
  {
    Wm3::Vector3f kA;
    Wm3::Vector3f kB;
    std::memcpy(&kA, a, 3 * sizeof(float));
    std::memcpy(&kB, b, 3 * sizeof(float));
    return (kA == kB) ? 1u : 0u;
  }

  u32 QuaternionCompareArrays(const float* a, const float* b)
  {
    Wm3::Quaternionf kA;
    Wm3::Quaternionf kB;
    std::memcpy(&kA, a, 4 * sizeof(float));
    std::memcpy(&kB, b, 4 * sizeof(float));
    return static_cast<u32>(kA.CompareArrays(kB));
  }

  u32 QuaternionOperatorEquals(const float* a, const float* b)
  {
    Wm3::Quaternionf kA;
    Wm3::Quaternionf kB;
    std::memcpy(&kA, a, 4 * sizeof(float));
    std::memcpy(&kB, b, 4 * sizeof(float));
    return (kA == kB) ? 1u : 0u;
  }

  void Sphere3CtorOurs(std::uint32_t* out, const float* center, float radius)
  {
    Wm3::Vector3f kCenter(center[0], center[1], center[2]);
    const Wm3::Sphere3f kSphere(kCenter, radius);
    std::memcpy(out, &kSphere, sizeof kSphere);
  }
} // namespace wm3collharness
