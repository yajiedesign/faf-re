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

} // namespace

  float DistVector3Box3GetSquared(const float* vec, const float* box)
  {
    const Wm3::Vector3f kVector = Load3(vec);
    const Wm3::Box3f kBox = LoadBox3(box);
    Wm3::DistVector3Box3f query(kVector, kBox);
    return query.GetSquared();
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
    const float kUndefCoord = wmdiff::FromBits(0x7FC00001u);

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

float DistVector2Box2GetSquared(const float* vec, const float* box)
  {
    const Wm3::Vector2f kVector = Load2(vec);
    const Wm3::Box2f kBox = LoadBox2(box);
    Wm3::DistVector2Box2f query(kVector, kBox);
    return query.GetSquared();
  }

void DistVector2Box2Get(const float* vec, const float* box, u32* out)
  {
    const Wm3::Vector2f kVector = Load2(vec);
    const Wm3::Box2f kBox = LoadBox2(box);
    Wm3::DistVector2Box2f query(kVector, kBox);
    out[0] = wmdiff::Bits(query.Get());
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
      out[i] = wmdiff::Bits(f[i]);
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
      out[i] = wmdiff::Bits(f[i]);
    }
  }

// ---- the swept Distance queries ----------------------------------------
  //
  // The derived class's own virtuals, the ones the class vtable puts at
  // +0x08 (Get) and +0x14 (GetSquared).  The retail's body moves the two
  // shapes by fT*velocity and then asks the *static* query of the moved
  // pair, which is what the rebuilt source does too.

// ---- Wm3::DistLine3Segment3<float> ---------------------------------------

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
    out[0] = wmdiff::Bits(query.GetDifferenceStep());
    out[1] = wmdiff::Bits(query.*get(DiffInvTag{}));
  }

  void DistanceDifferenceStepDefault(u32* out)
  {
    Wm3::DistVector3Segment3f query = MakeDistanceObject();
    out[0] = wmdiff::Bits(query.GetDifferenceStep());
    out[1] = wmdiff::Bits(query.*get(DiffInvTag{}));
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

  } // namespace

u32 Vector3LengthOurs(const float* v)
  {
    Wm3::Vector3f kV(v[0], v[1], v[2]);
    return wmdiff::Bits(kV.Length());
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
