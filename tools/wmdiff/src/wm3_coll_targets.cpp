// Leaf-function targets for the Wild Magic 3.8 collision / distance classes the
// engine calls. These live outside Foundation/Math, in Foundation/Intersection
// and Foundation/Distance.
//
// Every row is justified by a real rebuild call site, and the caller is
// annotated with the retail address the address walk starts from:
//
//   Moho::CColPrimitive_Box::CollideLine   = 0x004FF2D0  (CColPrimitiveBase.cpp:370)
//   Moho::CColPrimitive_Box::CollideSphere = 0x004FF150  (CColPrimitiveBase.cpp:420)
//   Moho::CColPrimitive_Sphere::CollideBox = 0x004FE860
//   Moho::Projectile::CheckCollision       = 0x0069D1D0  (Projectile.cpp:1388)
//   Moho::VisionDB::Entry::TryAdd          = 0x0081B490  (VisionDB.cpp:396)
//
// The engine builds these objects by constructing them and then querying, so a
// row drives the retail constructor as well as the query - the object holds
// references to caller-owned data, and fabricating its layout by hand would be
// a guess rather than a comparison.
//
// Every row here is on the typed path: the two sides are handed the same typed
// input struct and answer with a typed output struct, and the harness only
// ever sees the byte buffers behind them.  The lanes are plain floats, not
// Wm3:: classes, because the retail thunks and our harness forwards take the
// address of the lanes; the vector is built where the real member is called.

#include "wm3_coll_targets.h"

#include "fuzz.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

#include "wm3_coll_harness.h"
#include "wm3_coll_thunks.h"

using wmdiff::DrawKind;
using wmdiff::FromBits;
using wmdiff::GenEdge;
using wmdiff::GenFloat;
using wmdiff::GenPlain;
using wmdiff::Rng;

using u32 = std::uint32_t;

namespace wm3coll
{
  namespace
  {

    // Retail's DistVector3Box3 object stores its two reference members at
    // [this+0x34] and [this+0x38] and sets a vtable pointer, so it needs at
    // least 0x3C bytes of writable storage. 64 is comfortably above that and
    // keeps the buffer's address layout irrelevant.
    constexpr int kObjectBytes = 64;

    // A unit vector perpendicular to the direction `d`. The generators below
    // place one shape relative to another along `d` and across it, so they
    // need a direction to place it across.
    void MakePerp3(const float* d, float* out)
    {
      float ax = 0.0f, ay = 0.0f, az = 1.0f;
      if (std::fabs(d[2]) > 0.9f) {
        ax = 1.0f; ay = 0.0f; az = 0.0f;
      }
      const float px = ay*d[2] - az*d[1];
      const float py = az*d[0] - ax*d[2];
      const float pz = ax*d[1] - ay*d[0];
      const float len = std::sqrt(px*px + py*py + pz*pz);
      out[0] = px/len; out[1] = py/len; out[2] = pz/len;
    }

    // ---- typed input shapes ------------------------------------------------
    //
    // The lanes are the ones the retail constructor and our harness forward
    // are handed, in the order the call reads them.  A shape that several
    // rows share is written once and the rows forward to it.

    // A vector and a box: Box3 is Center[3], Axis[3][3], Extent[3].
    struct DistVector3Box3In
    {
      float vec[3];
      float box[15];
    };

    // A vector and a segment: Origin[3], Direction[3], Extent.
    struct DistVector3Segment3In
    {
      float vec[3];
      float segment[7];
    };

    // A box (Center[2], Axis[2][2], Extent[2]) and a circle (Center[2], Radius).
    struct IntrBox2Circle2In
    {
      float box[8];
      float circle[3];
    };

    // A line (Origin[3], Direction[3]) and a box.
    struct IntrLine3Box3In
    {
      float line[6];
      float box[15];
    };

    // A segment and a box, plus the bSolid flag Find takes.
    struct IntrSegment3Box3In
    {
      float segment[7];
      float box[15];
      u32 solid;
    };

    // A box and a sphere (Center[3], Radius).
    struct IntrBox3Sphere3In
    {
      float box[15];
      float sphere[4];
    };

    // The same box and sphere, plus the swept Find's own arguments.
    struct IntrBox3Sphere3FindIn
    {
      float box[15];
      float sphere[4];
      float tmax;
      float vel0[3];
      float vel1[3];
    };

    // A segment and a sphere.
    struct IntrSegment3Sphere3In
    {
      float segment[7];
      float sphere[4];
    };

    // Two boxes.
    struct IntrBox2Box2In
    {
      float box0[8];
      float box1[8];
    };

    // A vector and a box, in two dimensions.
    struct DistVector2Box2In
    {
      float vec[2];
      float box[8];
    };

    // A point and a box.
    struct InBoxIn
    {
      float point[3];
      float box[15];
    };

    // The containment rows' points: eight of them, and the count.
    struct ContAlignedBoxIn
    {
      u32 count;
      float points[24];
    };

    // The same, with the mask: the mode word and the eight mask bytes.
    struct ContAlignedBoxMaskIn
    {
      u32 count;
      u32 mode;
      float points[24];
      unsigned char mask[8];
    };

    // The two private statics' own arguments.
    struct VertexIntersectionIn
    {
      float d[3];
      float v[3];
      float r2;
    };

    struct EdgeIntersectionIn
    {
      float d[2];
      float v[2];
      float vsqr;
      float r2;
    };

    // The region helpers: the box and the sphere, the helper's nine arguments
    // and - for the two helpers that have one - the flag.
    struct RegionIn
    {
      float box[15];
      float sphere[4];
      float args[9];
    };

    struct RegionFlagIn
    {
      float box[15];
      float sphere[4];
      float args[9];
      float above;
    };

    // The clipping helper's arguments.
    struct DoClippingIn
    {
      float t0;
      float t1;
      float line[6];
      float box[15];
      float solid;
    };

    struct ClipIn
    {
      float denom;
      float numer;
      float t0;
      float t1;
    };

    // The distance base's step.
    struct StepIn
    {
      float step;
    };

    // The Vector3 / Quaternion / Sphere3 rows.
    struct Vec3In
    {
      float v[3];
    };

    struct PairVec3In
    {
      float a[3];
      float b[3];
    };

    struct PairQuatIn
    {
      float a[4];
      float b[4];
    };

    struct Sphere3CtorIn
    {
      float center[3];
      float radius;
    };

    // ---- typed output shapes -----------------------------------------------

    struct FloatOut
    {
      float ret;      // a scalar answer
    };

    struct BoolOut
    {
      u32 ret;        // a bool or an int answer
    };

    struct PointOut
    {
      float point[3];
    };

    struct PointTimeOut
    {
      float point[3];
      float time;
    };

    struct RegionOut
    {
      u32 ret;
      float point[3];
      float time;
    };

    struct ClipOut
    {
      u32 ret;
      float t0;
      float t1;
    };

    struct Box15Out
    {
      float box[15];
    };

    struct DoClippingOut
    {
      u32 hit;
      u32 quantity;
      u32 type;
      float points[6];
    };

    struct StepOut
    {
      float step;
      float invtwo;
    };

    struct Obj4Out
    {
      float obj[4];
    };

    // ---- generators ---------------------------------------------------------
    //
    // WithExtremes draws exactly what the word path drew, in exactly the same
    // order: the same helper, the same arguments, the same sequence of RNG
    // draws.  The same seed therefore still produces the same input bytes, so
    // the comparison stays bit for bit - that is what makes the conversion
    // provably harmless rather than merely plausible.
    //
    // Ordinary is the --skip-extreme draw: every lane is finite, in range and
    // normal - no NaN, no infinity, no subnormal - so a typed row always has a
    // comparable input and can never be skipped.  It consumes a different
    // number of draws; the two kinds are never mixed in one run.

    void GenDistVector3Box3T(Rng& rng, void* p, DrawKind kind)
    {
      DistVector3Box3In& in = *static_cast<DistVector3Box3In*>(p);
      if (kind == DrawKind::WithExtremes) {
        wmdiff::GenVec3(rng, in.vec);
        for (int i = 0; i < 15; ++i) {
          in.box[i] = rng.Chance(75) ? GenPlain(rng, 1000.0f) : GenFloat(rng);
        }
        return;
      }
      for (int i = 0; i < 3; ++i) {
        in.vec[i] = GenPlain(rng, 100.0f);
      }
      for (int i = 0; i < 15; ++i) {
        in.box[i] = GenPlain(rng, 1000.0f);
      }
    }

    // A point a few times 1e-19 off a segment's line: the squared distance
    // is around 1e-38, which is below the smallest normal float
    // (1.1754944e-38) and therefore lives on the denormal grid, where a
    // float holds far fewer bits than the x87 register does.  The direction
    // is the x axis and the point is off it in y, so the closest point on
    // the segment is the origin and the distance is exactly the offset.
    //
    // This draw is a denormal by construction, so it belongs to the extreme
    // kind only.
    void GenDistVector3Segment3Denormal(Rng& rng, DistVector3Segment3In& in)
    {
      const float d = static_cast<float>(rng.Range(1.0e-20, 1.0e-19));
      in.vec[0] = 0.0f; in.vec[1] = d; in.vec[2] = 0.0f;   // the vector, d off the line
      in.segment[0] = 0.0f; in.segment[1] = 0.0f; in.segment[2] = 0.0f; // the origin
      in.segment[3] = 1.0f; in.segment[4] = 0.0f; in.segment[5] = 0.0f; // the direction
      in.segment[6] = 10.0f;                          // the extent
    }

    void GenDistVector3Segment3Ext(Rng& rng, DistVector3Segment3In& in)
    {
      if (rng.Chance(15)) {
        GenDistVector3Segment3Denormal(rng, in);
        return;
      }
      wmdiff::GenVec3(rng, in.vec);
      for (int i = 0; i < 6; ++i) {
        in.segment[i] = rng.Chance(75) ? GenPlain(rng, 10.0f) : GenFloat(rng);
      }
      in.segment[6] = rng.Chance(80) ? GenPlain(rng, 1000.0f) : GenEdge(rng);
    }

    // Retail's Get() does not take the game's own sqrt: it calls the CRT's,
    // 0x00A91000. That routine branches on the x87 control word
    // (CMP word [ESP], 0x027F at 0x00A91028) and on a non-finite argument takes
    // an error path that reaches an import this harness does not resolve - the
    // run raises the trap code instead of returning NaN, so the row would die
    // rather than compare. The trigger is the result being NaN or +/-Inf, which
    // is visible in every crashing draw and in none of the accepted ones.
    //
    // So this row's scope is finite results. Filtering here rather than with the
    // global --skip-extreme keeps the other rows at full coverage.
    // Same reason as GenDistVector3Box3Get: the retail's CRT sqrt treats a
    // non-positive argument as a fatal math error, so the row is limited to
    // the finite range.
    void GenDistVector3Segment3T(Rng& rng, void* p, DrawKind kind)
    {
      DistVector3Segment3In& in = *static_cast<DistVector3Segment3In*>(p);
      if (kind == DrawKind::WithExtremes) {
        GenDistVector3Segment3Ext(rng, in);
        return;
      }
      // Ordinary: the denormal draw is not made at all, so every lane is a
      // plain number.
      for (int i = 0; i < 3; ++i) {
        in.vec[i] = GenPlain(rng, 100.0f);
      }
      for (int i = 0; i < 6; ++i) {
        in.segment[i] = GenPlain(rng, 10.0f);
      }
      in.segment[6] = GenPlain(rng, 100.0f);
    }

    void GenDistVector3Segment3GetT(Rng& rng, void* p, DrawKind kind)
    {
      DistVector3Segment3In& in = *static_cast<DistVector3Segment3In*>(p);
      if (kind == DrawKind::WithExtremes) {
        for (int attempt = 0; attempt < 32; ++attempt) {
          GenDistVector3Segment3Ext(rng, in);
          const float squared =
            wm3collharness::DistVector3Segment3GetSquared(in.vec, in.segment);
          const u32 bits = wmdiff::Bits(squared);
          if ((bits & 0x7F800000u) != 0x7F800000u) {
            return;
          }
        }
        for (int i = 0; i < 3; ++i) {
          in.vec[i] = GenPlain(rng, 1.0f);
        }
        for (int i = 0; i < 7; ++i) {
          in.segment[i] = GenPlain(rng, 1.0f);
        }
        return;
      }
      // Ordinary: the denormal draw is not made at all, so the squared
      // distance is finite by construction and no filter is needed.
      for (int i = 0; i < 3; ++i) {
        in.vec[i] = GenPlain(rng, 10.0f);
      }
      for (int i = 0; i < 6; ++i) {
        in.segment[i] = GenPlain(rng, 10.0f);
      }
      in.segment[6] = GenPlain(rng, 100.0f);
    }

    void GenIntrBox2Circle2T(Rng& rng, void* p, DrawKind kind)
    {
      IntrBox2Circle2In& in = *static_cast<IntrBox2Circle2In*>(p);
      // The radius is drawn around the box's own extents so that "just
      // touching" and "just missing" are both common: a Test that only ever
      // sees clearly inside or clearly outside answers the same every time.
      for (int i = 0; i < 8; ++i) {
        in.box[i] = GenPlain(rng, 100.0f);
      }
      in.circle[0] = GenPlain(rng, 100.0f);
      in.circle[1] = GenPlain(rng, 100.0f);
      const float extent = (std::fabs(in.box[6]) + std::fabs(in.box[7])) * 0.5f;
      in.circle[2] = rng.Chance(70) ? extent * rng.Range(0.0, 2.5)
                                     : GenPlain(rng, 200.0f);
      (void)kind; // every lane here is plain in both kinds
    }

    void GenIntrLine3Box3T(Rng& rng, void* p, DrawKind kind)
    {
      IntrLine3Box3In& in = *static_cast<IntrLine3Box3In*>(p);
      if (kind == DrawKind::WithExtremes) {
        wmdiff::GenVec3(rng, in.line);
        wmdiff::GenVec3(rng, in.line + 3);
        for (int i = 0; i < 15; ++i) {
          in.box[i] = rng.Chance(75) ? GenPlain(rng, 1000.0f) : GenFloat(rng);
        }
        return;
      }
      for (int i = 0; i < 6; ++i) {
        in.line[i] = GenPlain(rng, 100.0f);
      }
      for (int i = 0; i < 15; ++i) {
        in.box[i] = GenPlain(rng, 1000.0f);
      }
    }

    void GenIntrSegment3Box3T(Rng& rng, void* p, DrawKind kind)
    {
      IntrSegment3Box3In& in = *static_cast<IntrSegment3Box3In*>(p);
      if (kind == DrawKind::WithExtremes) {
        wmdiff::GenVec3(rng, in.segment);
        // The word path's input block held the segment's seven lanes, then the
        // box's fifteen, then three lanes nothing ever read, then the flag.
        // The draws are kept in the same order - including the three that are
        // dropped - so the stream is the same and the flag keeps its value.
        for (int i = 3; i < 9; ++i) {
          const float v = rng.Chance(75) ? GenPlain(rng, 10.0f) : GenFloat(rng);
          if (i < 7) {
            in.segment[i] = v;
          }
          else {
            in.box[i - 7] = v;
          }
        }
        in.box[2] = rng.Chance(80) ? GenPlain(rng, 1000.0f) : GenEdge(rng);
        for (int i = 10; i < 25; ++i) {
          const float v = rng.Chance(75) ? GenPlain(rng, 1000.0f) : GenFloat(rng);
          if (i < 22) {
            in.box[i - 7] = v;
          }
        }
        in.solid = rng.Chance(50) ? 1u : 0u;
        return;
      }
      for (int i = 0; i < 3; ++i) {
        in.segment[i] = GenPlain(rng, 10.0f);
      }
      for (int i = 3; i < 7; ++i) {
        in.segment[i] = GenPlain(rng, 10.0f);
      }
      for (int i = 0; i < 15; ++i) {
        in.box[i] = GenPlain(rng, 1000.0f);
      }
      in.solid = rng.Chance(50) ? 1u : 0u;
    }

    void GenIntrBox3Sphere3T(Rng& rng, void* p, DrawKind kind)
    {
      IntrBox3Sphere3In& in = *static_cast<IntrBox3Sphere3In*>(p);
      if (kind == DrawKind::WithExtremes) {
        for (int i = 0; i < 15; ++i) {
          in.box[i] = rng.Chance(75) ? GenPlain(rng, 1000.0f) : GenFloat(rng);
        }
        for (int i = 0; i < 3; ++i) {
          in.sphere[i] = GenPlain(rng, 1000.0f);
        }
        // The radius is drawn around the box's own extent so that near-touching
        // and clearly-inside cases are both common.
        const float reach =
          (std::fabs(in.box[6]) + std::fabs(in.box[7]) + std::fabs(in.box[8])) / 3.0f;
        in.sphere[3] = rng.Chance(70) ? reach * rng.Range(0.0, 2.5)
                                      : GenPlain(rng, 200.0f);
        return;
      }
      for (int i = 0; i < 15; ++i) {
        in.box[i] = GenPlain(rng, 1000.0f);
      }
      for (int i = 0; i < 3; ++i) {
        in.sphere[i] = GenPlain(rng, 1000.0f);
      }
      in.sphere[3] = GenPlain(rng, 200.0f);
    }

    // The box gets an orthonormal axis and a positive extent, the sphere a
    // centre within a couple of box widths and a radius on the box's own
    // scale, and the velocities are large enough that the swept branch is
    // reachable inside a horizon of one or two.
    //
    // The extents have to be positive for the "sphere centre inside the box"
    // path to be reachable at all: the test is `fAx <= Extent[0]`, and with a
    // negative extent even a centre at the origin fails it. GenPlain draws
    // both signs, so the extents are made positive here.
    //
    // No extreme values, and that is a scope limit rather than an accident.
    // The shared GenIntrBox3Sphere3 injects denormals, infinities and NaN
    // through wmdiff::GenFloat, and the region helpers inside Find do not
    // survive those: the rebuild computes them in float, where retail computes
    // in x87, so an overflow becomes an infinity here and a large finite value
    // there, and that changes which branch is taken. It is the same
    // exponent-range family documented on Vector3::Dot and Vector3::LengthSq,
    // and the fix belongs in the region helpers, in the Wm3Detail 24-bit
    // treatment Wm3DistVector3Box3.cpp already got.
    void GenIntrBox3Sphere3FindT(Rng& rng, void* p, DrawKind kind)
    {
      IntrBox3Sphere3FindIn& in = *static_cast<IntrBox3Sphere3FindIn*>(p);
      if (kind == DrawKind::WithExtremes) {
        for (int i = 0; i < 3; ++i) {
          in.box[i] = GenPlain(rng, 1000.0f);        // box centre
        }
        // An orthonormal axis: the algorithm treats the three axes as
        // orthonormal, and a random one would make the box a parallelepiped.
        for (int i = 3; i < 12; ++i) {
          in.box[i] = 0.0f;
        }
        in.box[3] = 1.0f;
        in.box[7] = 1.0f;
        in.box[11] = 1.0f;
        for (int i = 12; i < 15; ++i) {
          in.box[i] = std::fabs(GenPlain(rng, 500.0f));  // positive extents
        }
        for (int i = 0; i < 3; ++i) {
          in.sphere[i] = in.box[i] + GenPlain(rng, 2000.0f); // sphere centre
        }
        // The radius and the horizon are positive. A negative radius is not a
        // shape: the region tests compare against `Radius*fVx*iXSign`, so the
        // sign of the radius flips those comparisons, and the two sides then
        // pick different regions for the same geometry. GenPlain draws both
        // signs, so both of these have to be made positive here.
        const float reach = (in.box[12] + in.box[13] + in.box[14]) / 3.0f;
        in.sphere[3] = std::fabs(rng.Chance(70) ? reach * rng.Range(0.0, 2.5f)
                                                 : GenPlain(rng, 200.0f));
        in.tmax = rng.Chance(60) ? 0.0f : std::fabs(GenPlain(rng, 2.0f));
        for (int i = 0; i < 3; ++i) {
          in.vel0[i] = GenPlain(rng, 1000.0f);
        }
        for (int i = 0; i < 3; ++i) {
          in.vel1[i] = GenPlain(rng, 1000.0f);
        }
        return;
      }
      for (int i = 0; i < 3; ++i) {
        in.box[i] = GenPlain(rng, 1000.0f);
      }
      for (int i = 3; i < 12; ++i) {
        in.box[i] = 0.0f;
      }
      in.box[3] = 1.0f;
      in.box[7] = 1.0f;
      in.box[11] = 1.0f;
      for (int i = 12; i < 15; ++i) {
        in.box[i] = std::fabs(GenPlain(rng, 500.0f));
      }
      for (int i = 0; i < 3; ++i) {
        in.sphere[i] = in.box[i] + GenPlain(rng, 2000.0f);
      }
      in.sphere[3] = std::fabs(GenPlain(rng, 200.0f));
      in.tmax = rng.Chance(60) ? 0.0f : std::fabs(GenPlain(rng, 2.0f));
      for (int i = 0; i < 3; ++i) {
        in.vel0[i] = GenPlain(rng, 1000.0f);
      }
      for (int i = 0; i < 3; ++i) {
        in.vel1[i] = GenPlain(rng, 1000.0f);
      }
    }

    // The segment is placed relative to the sphere's centre rather than drawn
    // blindly, so the three quantities the quadratic in Find() depends on are
    // all controlled:
    //
    //   m  the perpendicular distance from the sphere's centre to the
    //      segment's line: m > R misses, m = R is tangent, m < R cuts.
    //   t  the signed offset of the segment origin along the direction.
    //   e  the extent, drawn independently of t, so "the segment stops short
    //      of the sphere" and "the segment spans it" are both common.
    //
    // The branches that fall out of that:
    //   m > R                -> fDiscr < 0   -> quantity 0
    //   m = R                -> fDiscr ~ 0   -> quantity 1, the tangent case
    //   m < R, e covers it   -> fQM*fQP <= 0 -> quantity 1
    //   m < R, |t| < e       -> fQM > 0      -> quantity 2
    //   m < R, e short of it ->              -> quantity 0
    //
    // The direction is unit length, the way the engine's own segments are
    // (Wm3::MakeSegment3fFromEndpoints), and the radius is positive: a
    // negative radius is not a shape.
    void GenIntrSegment3Sphere3T(Rng& rng, void* p, DrawKind kind)
    {
      IntrSegment3Sphere3In& in = *static_cast<IntrSegment3Sphere3In*>(p);
      if (kind == DrawKind::WithExtremes) {
        // the sphere: centre and radius
        for (int i = 0; i < 3; ++i) {
          in.sphere[i] = GenPlain(rng, 100.0f);
        }
        const float radius = static_cast<float>(rng.Range(0.5, 5.0));
        in.sphere[3] = radius;

        // the segment direction: unit length
        wmdiff::GenUnitVec3(rng, in.segment + 3);
        const float dx = in.segment[3], dy = in.segment[4], dz = in.segment[5];

        // a unit vector perpendicular to the direction
        float ax = 0.0f, ay = 0.0f, az = 1.0f;
        if (std::fabs(dz) > 0.9f) {
          ax = 1.0f; ay = 0.0f; az = 0.0f;
        }
        float px = ay * dz - az * dy;
        float py = az * dx - ax * dz;
        float pz = ax * dy - ay * dx;
        const float plen = std::sqrt(px * px + py * py + pz * pz);
        px /= plen; py /= plen; pz /= plen;

        // the origin: `along` radii along the direction from the centre, and
        // `off` radii away from it. off == 1 is forced now and then: that is the
        // tangent case, where Find() takes the ZeroThreshold branch.
        const float off = rng.Chance(10) ? 1.0f
                                         : static_cast<float>(rng.Range(0.0, 1.6));
        const float along = static_cast<float>(rng.Range(-2.5, 2.5));
        const float o[3] = { dx, dy, dz };
        const float perp[3] = { px, py, pz };
        for (int i = 0; i < 3; ++i) {
          in.segment[i] = in.sphere[i] + along * radius * o[i] + off * radius * perp[i];
        }

        // the extent, on the segment's own scale and independent of `along`
        in.segment[6] = radius * (static_cast<float>(rng.Range(0.2, 2.5))
                                  + (rng.Chance(50) ? std::fabs(along) * radius : 0.0f));
        return;
      }
      for (int i = 0; i < 3; ++i) {
        in.sphere[i] = GenPlain(rng, 100.0f);
      }
      const float radius = static_cast<float>(rng.Range(0.5, 5.0));
      in.sphere[3] = radius;
      wmdiff::GenUnitVec3(rng, in.segment + 3);
      const float dx = in.segment[3], dy = in.segment[4], dz = in.segment[5];
      float ax = 0.0f, ay = 0.0f, az = 1.0f;
      if (std::fabs(dz) > 0.9f) {
        ax = 1.0f; ay = 0.0f; az = 0.0f;
      }
      float px = ay * dz - az * dy;
      float py = az * dx - ax * dz;
      float pz = ax * dy - ay * dx;
      const float plen = std::sqrt(px * px + py * py + pz * pz);
      px /= plen; py /= plen; pz /= plen;
      const float off = rng.Chance(10) ? 1.0f
                                       : static_cast<float>(rng.Range(0.0, 1.6));
      const float along = static_cast<float>(rng.Range(-2.5, 2.5));
      const float o[3] = { dx, dy, dz };
      const float perp[3] = { px, py, pz };
      for (int i = 0; i < 3; ++i) {
        in.segment[i] = in.sphere[i] + along * radius * o[i] + off * radius * perp[i];
      }
      in.segment[6] = radius * (static_cast<float>(rng.Range(0.2, 2.5))
                                + (rng.Chance(50) ? std::fabs(along) * radius : 0.0f));
    }

    // The two axes are orthonormal (the algorithm projects the boxes onto each
    // other's axes, so a non-orthonormal basis would make the test meaningless)
    // and the extents are positive. The second box is placed relative to the
    // first, near the separating distance, so the answer is not always "no".
    void GenIntrBox2Box2T(Rng& rng, void* p, DrawKind kind)
    {
      IntrBox2Box2In& in = *static_cast<IntrBox2Box2In*>(p);
      if (kind == DrawKind::WithExtremes) {
        // box 0: centre, axes, extents
        for (int i = 0; i < 2; ++i) {
          in.box0[i] = GenPlain(rng, 100.0f);
        }
        in.box0[2] = 1.0f; in.box0[3] = 0.0f;               // axis 0
        in.box0[4] = 0.0f; in.box0[5] = 1.0f;               // axis 1
        in.box0[6] = rng.Range(2.0f, 30.0f);                // extent x, positive
        in.box0[7] = rng.Range(2.0f, 30.0f);                // extent y, positive

        // box 1: the same shape, rotated by a random angle, with its centre
        // placed at a distance around the separating distance of the two boxes.
        const float angle = static_cast<float>(rng.Range(0.0, 3.14159265));
        const float ca = std::cos(angle), sa = std::sin(angle);
        in.box1[0] = 0.0f; in.box1[1] = 0.0f;
        in.box1[2] = ca;  in.box1[3] = -sa;                 // axis 0, rotated
        in.box1[4] = sa;  in.box1[5] = ca;                  // axis 1, rotated
        in.box1[6] = in.box0[6] * static_cast<float>(rng.Range(0.3, 1.5));
        in.box1[7] = in.box0[7] * static_cast<float>(rng.Range(0.3, 1.5));

        // the centre, near the point where the two boxes start to touch
        const float e0 = in.box0[6], e1 = in.box0[7];
        const float reach = e0 + e1 + std::fabs(in.box1[6]) + std::fabs(in.box1[7]);
        const float dist = reach * static_cast<float>(rng.Range(0.3, 1.2));
        const float dir = static_cast<float>(rng.Range(0.0, 6.2831853));
        in.box1[0] = in.box0[0] + dist * std::cos(dir);
        in.box1[1] = in.box0[1] + dist * std::sin(dir);
        return;
      }
      for (int i = 0; i < 2; ++i) {
        in.box0[i] = GenPlain(rng, 100.0f);
      }
      in.box0[2] = 1.0f; in.box0[3] = 0.0f;
      in.box0[4] = 0.0f; in.box0[5] = 1.0f;
      in.box0[6] = rng.Range(2.0f, 30.0f);
      in.box0[7] = rng.Range(2.0f, 30.0f);
      const float angle = static_cast<float>(rng.Range(0.0, 3.14159265));
      const float ca = std::cos(angle), sa = std::sin(angle);
      in.box1[0] = 0.0f; in.box1[1] = 0.0f;
      in.box1[2] = ca;  in.box1[3] = -sa;
      in.box1[4] = sa;  in.box1[5] = ca;
      in.box1[6] = in.box0[6] * static_cast<float>(rng.Range(0.3, 1.5));
      in.box1[7] = in.box0[7] * static_cast<float>(rng.Range(0.3, 1.5));
      const float e0 = in.box0[6], e1 = in.box0[7];
      const float reach = e0 + e1 + std::fabs(in.box1[6]) + std::fabs(in.box1[7]);
      const float dist = reach * static_cast<float>(rng.Range(0.3, 1.2));
      const float dir = static_cast<float>(rng.Range(0.0, 6.2831853));
      in.box1[0] = in.box0[0] + dist * std::cos(dir);
      in.box1[1] = in.box0[1] + dist * std::sin(dir);
    }

    // The box gets an orthonormal axis and positive extents: the algorithm
    // projects the vector onto the box's axes and clamps against the
    // extents, so a non-orthonormal basis or a negative extent would make
    // the test meaningless. The vector is drawn around the box, so "inside",
    // "on the edge" and "outside" are all common.
    void GenDistVector2Box2T(Rng& rng, void* p, DrawKind kind)
    {
      DistVector2Box2In& in = *static_cast<DistVector2Box2In*>(p);
      if (kind == DrawKind::WithExtremes) {
        in.box[0] = GenPlain(rng, 40.0f);
        in.box[1] = GenPlain(rng, 40.0f);
        const float angle = static_cast<float>(rng.Range(0.0, 3.14159265));
        const float ca = std::cos(angle), sa = std::sin(angle);
        in.box[2] = ca;  in.box[3] = -sa;                  // axis 0
        in.box[4] = sa;  in.box[5] = ca;                   // axis 1
        in.box[6] = static_cast<float>(rng.Range(0.5, 30.0));   // extent x, positive
        in.box[7] = static_cast<float>(rng.Range(0.5, 30.0));   // extent y, positive
        // the vector, on the box's own scale: sometimes inside, sometimes out
        const float reach = in.box[6] + in.box[7];
        in.vec[0] = GenPlain(rng, 40.0f);
        in.vec[1] = GenPlain(rng, 40.0f);
        if (rng.Chance(40)) {
          // near the box, so the clamped and the unclamped branches are both
          // common
          in.vec[0] = in.box[0] + GenPlain(rng, reach);
          in.vec[1] = in.box[1] + GenPlain(rng, reach);
        }
        return;
      }
      in.box[0] = GenPlain(rng, 40.0f);
      in.box[1] = GenPlain(rng, 40.0f);
      const float angle = static_cast<float>(rng.Range(0.0, 3.14159265));
      const float ca = std::cos(angle), sa = std::sin(angle);
      in.box[2] = ca;  in.box[3] = -sa;
      in.box[4] = sa;  in.box[5] = ca;
      in.box[6] = static_cast<float>(rng.Range(0.5, 30.0));
      in.box[7] = static_cast<float>(rng.Range(0.5, 30.0));
      const float reach = in.box[6] + in.box[7];
      in.vec[0] = GenPlain(rng, 40.0f);
      in.vec[1] = GenPlain(rng, 40.0f);
      if (rng.Chance(40)) {
        in.vec[0] = in.box[0] + GenPlain(rng, reach);
        in.vec[1] = in.box[1] + GenPlain(rng, reach);
      }
    }

    // The box gets an orthonormal axis and positive extents, and the point is
    // drawn around the centre and across the extents, so "inside", "outside"
    // and "just off a face" are all common. One draw in three puts the point
    // exactly on one of the faces, which is the case the comparison can turn
    // on.
    void GenInBoxT(Rng& rng, void* p, DrawKind kind)
    {
      InBoxIn& in = *static_cast<InBoxIn*>(p);
      if (kind == DrawKind::WithExtremes) {
        in.box[0] = GenPlain(rng, 20.0f);                  // the box's centre
        in.box[1] = GenPlain(rng, 20.0f);
        in.box[2] = GenPlain(rng, 20.0f);
        wmdiff::GenUnitVec3(rng, in.box + 3);               // axis 0
        MakePerp3(in.box + 3, in.box + 6);                   // axis 1
        in.box[9] = in.box[4] * in.box[8] - in.box[5] * in.box[7];  // axis 2
        in.box[10] = in.box[5] * in.box[6] - in.box[3] * in.box[8];
        in.box[11] = in.box[3] * in.box[7] - in.box[4] * in.box[6];
        in.box[12] = static_cast<float>(rng.Range(0.5, 10.0));  // extent x
        in.box[13] = static_cast<float>(rng.Range(0.5, 10.0));  // extent y
        in.box[14] = static_cast<float>(rng.Range(0.5, 10.0));  // extent z

        for (int i = 0; i < 3; ++i) {
          in.point[i] = in.box[i] + GenPlain(rng, 20.0f);
        }
        if (rng.Chance(30)) {
          // exactly on one of the faces
          const int i = static_cast<int>(rng.Below(3));
          const float s = rng.Chance(50) ? 1.0f : -1.0f;
          for (int k = 0; k < 3; ++k) {
            in.point[k] = in.box[k] + s * in.box[12 + i] * in.box[3 + 3 * i + k];
          }
        }
        return;
      }
      in.box[0] = GenPlain(rng, 20.0f);
      in.box[1] = GenPlain(rng, 20.0f);
      in.box[2] = GenPlain(rng, 20.0f);
      wmdiff::GenUnitVec3(rng, in.box + 3);
      MakePerp3(in.box + 3, in.box + 6);
      in.box[9] = in.box[4] * in.box[8] - in.box[5] * in.box[7];
      in.box[10] = in.box[5] * in.box[6] - in.box[3] * in.box[8];
      in.box[11] = in.box[3] * in.box[7] - in.box[4] * in.box[6];
      in.box[12] = static_cast<float>(rng.Range(0.5, 10.0));
      in.box[13] = static_cast<float>(rng.Range(0.5, 10.0));
      in.box[14] = static_cast<float>(rng.Range(0.5, 10.0));
      for (int i = 0; i < 3; ++i) {
        in.point[i] = in.box[i] + GenPlain(rng, 20.0f);
      }
    }

    // The count is drawn first and the points follow, so the input reads in
    // the same order as the call. The points are on a modest scale and the
    // count is sometimes one, which makes the box degenerate - the centre
    // and the extent then collapse and the comparison is decided entirely by
    // the rounding of the difference.
    //
    // The count has to stay in 1..8 in both kinds: it is the number of points
    // the retail reads, and a count outside that range would have it read
    // past the eight the input carries.
    void GenContAlignedBoxT(Rng& rng, void* p, DrawKind kind)
    {
      ContAlignedBoxIn& in = *static_cast<ContAlignedBoxIn*>(p);
      in.count = static_cast<u32>(1 + rng.Below(8));
      for (int i = 0; i < 24; ++i) {
        in.points[i] = GenPlain(rng, 10.0f);
      }
      (void)kind; // every lane here is plain in both kinds
    }

    // The mode word selects the mask shape:
    //   0 - no mask at all (the retail's fourth argument is null, so it takes
    //       the unmasked path);
    //   1 - every point valid;
    //   2 - some points invalid (at least one valid and, when the count
    //       allows it, at least one invalid among the first `count` entries);
    //   3 - no point valid at all, which is the retail's "-1 extents" case;
    //   4 - nonzero-but-not-one bytes.
    //
    // The mask is eight bytes, one per point. A nonzero byte means "keep this
    // point" - the retail tests the mask a byte at a time - and the same bytes
    // are handed to our side as the `const bool*` the Wild Magic 3.4 signature
    // takes, so both sides read exactly the same mask.
    //
    // The points are on a modest scale and are sometimes duplicated so the
    // comparisons hit the equal case, which is where the "else if" form of
    // the Wild Magic 3.4 body differs from two independent tests.
    void GenContAlignedBoxMaskT(Rng& rng, void* p, DrawKind kind)
    {
      ContAlignedBoxMaskIn& in = *static_cast<ContAlignedBoxMaskIn*>(p);
      const int iCount = 1 + static_cast<int>(rng.Below(8));
      // The extreme kind keeps the fifth mode, whose bytes are 0x80 - a
      // nonzero byte the retail reads as "valid" and our side reads as a
      // `bool` whose representation is not 0 or 1.  The ordinary kind keeps
      // to 0 and 1, which is what a `bool` array actually holds.
      const int iMaxMode = (kind == DrawKind::WithExtremes) ? 5 : 4;
      const int iMode = static_cast<int>(rng.Below(static_cast<u32>(iMaxMode)));
      in.count = static_cast<u32>(iCount);
      in.mode = static_cast<u32>(iMode);

      for (int i = 0; i < 8; ++i) {
        for (int k = 0; k < 3; ++k) {
          in.points[3 * i + k] = GenPlain(rng, 10.0f);
        }
      }
      // Duplicate a point now and then: an exact tie is a boundary the
      // comparisons must agree on, and it makes the box degenerate.
      if (rng.Chance(40)) {
        const int iSrc = static_cast<int>(rng.Below(8));
        const int iDst = static_cast<int>(rng.Below(8));
        for (int k = 0; k < 3; ++k) {
          in.points[3 * iDst + k] = in.points[3 * iSrc + k];
        }
      }
      // And a zero point now and then: the extents then collapse onto zero.
      if (rng.Chance(20)) {
        const int iZero = static_cast<int>(rng.Below(8));
        for (int k = 0; k < 3; ++k) {
          in.points[3 * iZero + k] = 0.0f;
        }
      }

      for (int i = 0; i < 8; ++i) {
        in.mask[i] = 1;
      }
      if (iMode == 2) {
        // Invalidate a random nonempty proper subset of the entries the
        // retail will read, so the mask path runs with a mix of valid and
        // invalid points and the seed point is not always the first one.
        for (int i = 0; i < 8; ++i) {
          in.mask[i] = rng.Chance(50) ? 0 : 1;
        }
        // Keep at least one valid point among the first `count` entries.
        if (in.mask[0] == 0) {
          int iAny = -1;
          for (int i = 0; i < iCount; ++i) {
            if (in.mask[i] != 0) { iAny = i; break; }
          }
          if (iAny < 0) {
            in.mask[static_cast<int>(rng.Below(static_cast<u32>(iCount)))] = 1;
          }
        }
      }
      else if (iMode == 3) {
        for (int i = 0; i < 8; ++i) {
          in.mask[i] = 0;
        }
      }
      else if (iMode == 4) {
        // Nonzero-but-not-one bytes. The retail tests the mask byte against
        // zero, so these count as valid; our side reads the same bytes as the
        // `bool` array the Wild Magic 3.4 signature takes, and MSVC compiles
        // that test to a byte compare against zero as well.
        for (int i = 0; i < 8; ++i) {
          in.mask[i] = 0x80;
        }
        for (int i = 0; i < 8; ++i) {
          if (rng.Chance(50)) {
            in.mask[i] = 0;
          }
        }
      }
    }

    // ---- IntrBox3Sphere3's two private statics ---------------------------
    //
    // Both are private statics of the WM3 class that the retail's Find calls
    // instead of inlining them, and both answer the same question: a line
    // P = D + tV against a sphere (or a circle) of radius^2 fRSqr centred at
    // the origin, and the time of the impact.  The arithmetic is
    //
    //   fDiff  = |D|^2 - R^2
    //   fDot   = D.V
    //   fVSqr  = |V|^2
    //   disc   = fDot^2 - fVSqr*fDiff
    //   fInv   = 1/sqrt(|disc|)
    //   result = fDiff*fInv/(1 - fDot*fInv)
    //
    // so the generator's job is to produce a line and a sphere for which the
    // question has an answer, and to reach the two degenerate cases that
    // random geometry never reaches on its own.
    //
    // The magnitudes stay in the range where the two sides cannot part
    // company on the exponent: the rebuild computes in float, the retail in
    // the x87 register, and the two only agree as long as no intermediate
    // leaves the float range.  That is the same scope limit the Find row
    // records, and for the same reason.
    void GenIntrBox3Sphere3GetVertexIntersectionT(Rng& rng, void* p, DrawKind kind)
    {
      VertexIntersectionIn& in = *static_cast<VertexIntersectionIn*>(p);
      if (kind == DrawKind::WithExtremes) {
        const std::uint32_t k = rng.Below(100);
        if (k < 1) {
          // The line is tangent: the discriminant is exactly zero.  This is
          // the one case random geometry never reaches, and it is the only way
          // to reach the square root at zero - the reciprocal is an infinity
          // and the quotient a NaN, on both sides.
          //
          // R is a power of two, so R*R is exact and the squared length of D
          // minus it is exact too: D = (1, R, 0) and V = (1, 0, 0) give
          // fDot = 1, fDiff = 1 and fVSqr = 1, so the discriminant is 1 - 1.
          const float R = 1024.0f;
          in.d[0] = 1.0f;
          in.d[1] = R;
          in.d[2] = 0.0f;
          in.v[0] = 1.0f;
          in.v[1] = 0.0f;
          in.v[2] = 0.0f;
          in.r2 = R * R;
          return;
        }
        if (k < 2) {
          // The line starts exactly on the sphere and moves inward: fDiff is
          // exactly zero, the discriminant is not, and the answer is exactly
          // zero.  R is a power of two again, so that |D|^2 and R*R are the
          // same number.
          const float R = 1024.0f;
          in.d[0] = R;
          in.d[1] = 0.0f;
          in.d[2] = 0.0f;
          in.v[0] = -1.0f;
          in.v[1] = 0.0f;
          in.v[2] = 0.0f;
          in.r2 = R * R;
          return;
        }
        // An ordinary line that really meets the sphere: the point of closest
        // approach is at distance h from the origin with h <= R, so the
        // discriminant is non-negative and the helper has a real answer.
        const float R = std::fabs(GenPlain(rng, 100.0f)) + 0.5f;
        float v[3];
        for (int i = 0; i < 3; ++i) {
          v[i] = GenPlain(rng, 10.0f);
        }
        float vv = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
        if (vv < 1.0e-4f) {
          v[0] = 1.0f;
          v[1] = 0.0f;
          v[2] = 0.0f;
          vv = 1.0f;
        }
        else {
          // A slow and a fast line, so the discriminant spans a wide range.
          const float scale = rng.Chance(50) ? 1.0e-2f : (rng.Chance(50) ? 1.0e2f : 1.0f);
          for (int i = 0; i < 3; ++i) {
            v[i] *= scale;
          }
          vv *= scale * scale;
        }
        // A direction perpendicular to V.
        float perp[3] = { GenPlain(rng, 1.0f), GenPlain(rng, 1.0f), GenPlain(rng, 1.0f) };
        const float s = (perp[0] * v[0] + perp[1] * v[1] + perp[2] * v[2]) / vv;
        perp[0] -= s * v[0];
        perp[1] -= s * v[1];
        perp[2] -= s * v[2];
        float pp = perp[0] * perp[0] + perp[1] * perp[1] + perp[2] * perp[2];
        if (pp < 1.0e-6f) {
          // The drawn direction was (nearly) parallel to V; take another one.
          perp[0] = v[1];
          perp[1] = -v[0];
          perp[2] = 0.0f;
          pp = perp[0] * perp[0] + perp[1] * perp[1];
          if (pp < 1.0e-6f) {
            perp[0] = 0.0f;
            perp[1] = v[2];
            perp[2] = -v[1];
            pp = perp[0] * perp[0] + perp[1] * perp[1] + perp[2] * perp[2];
          }
        }
        const float inv = 1.0f / std::sqrt(pp);
        perp[0] *= inv;
        perp[1] *= inv;
        perp[2] *= inv;
        const float h = R * rng.Range(0.0f, 1.0f);   // the closest approach
        const float t0 = GenPlain(rng, 100.0f);       // where along the line
        in.d[0] = t0 * v[0] + h * perp[0];
        in.d[1] = t0 * v[1] + h * perp[1];
        in.d[2] = t0 * v[2] + h * perp[2];
        in.v[0] = v[0];
        in.v[1] = v[1];
        in.v[2] = v[2];
        in.r2 = R * R;
        return;
      }
      // Ordinary: the same construction, with plain magnitudes only.
      const float R = std::fabs(GenPlain(rng, 100.0f)) + 0.5f;
      float v[3];
      for (int i = 0; i < 3; ++i) {
        v[i] = GenPlain(rng, 10.0f);
      }
      float vv = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
      if (vv < 1.0e-4f) {
        v[0] = 1.0f;
        v[1] = 0.0f;
        v[2] = 0.0f;
        vv = 1.0f;
      }
      float perp[3] = { GenPlain(rng, 1.0f), GenPlain(rng, 1.0f), GenPlain(rng, 1.0f) };
      const float s = (perp[0] * v[0] + perp[1] * v[1] + perp[2] * v[2]) / vv;
      perp[0] -= s * v[0];
      perp[1] -= s * v[1];
      perp[2] -= s * v[2];
      float pp = perp[0] * perp[0] + perp[1] * perp[1] + perp[2] * perp[2];
      if (pp < 1.0e-6f) {
        perp[0] = v[1];
        perp[1] = -v[0];
        perp[2] = 0.0f;
        pp = perp[0] * perp[0] + perp[1] * perp[1];
      }
      const float inv = 1.0f / std::sqrt(pp);
      perp[0] *= inv;
      perp[1] *= inv;
      perp[2] *= inv;
      const float h = R * rng.Range(0.0f, 1.0f);
      const float t0 = GenPlain(rng, 100.0f);
      in.d[0] = t0 * v[0] + h * perp[0];
      in.d[1] = t0 * v[1] + h * perp[1];
      in.d[2] = t0 * v[2] + h * perp[2];
      in.v[0] = v[0];
      in.v[1] = v[1];
      in.v[2] = v[2];
      in.r2 = R * R;
    }

    // The same in two dimensions: d[2], v[2], vsqr and r2.  The squared
    // speed is an argument of the helper, not something it computes, so the
    // generator passes it.
    void GenIntrBox3Sphere3GetEdgeIntersectionT(Rng& rng, void* p, DrawKind kind)
    {
      EdgeIntersectionIn& in = *static_cast<EdgeIntersectionIn*>(p);
      if (kind == DrawKind::WithExtremes) {
        const std::uint32_t k = rng.Below(100);
        if (k < 1) {
          // The tangent: the discriminant is exactly zero.
          const float R = 1024.0f;
          in.d[0] = 1.0f;
          in.d[1] = R;
          in.v[0] = 1.0f;
          in.v[1] = 0.0f;
          in.vsqr = 1.0f;
          in.r2 = R * R;
          return;
        }
        if (k < 2) {
          // The line starts exactly on the circle and moves inward: fDiff is
          // exactly zero and the answer is exactly zero.
          const float R = 1024.0f;
          in.d[0] = R;
          in.d[1] = 0.0f;
          in.v[0] = -1.0f;
          in.v[1] = 0.0f;
          in.vsqr = 1.0f;
          in.r2 = R * R;
          return;
        }
        const float R = std::fabs(GenPlain(rng, 100.0f)) + 0.5f;
        float v[2];
        for (int i = 0; i < 2; ++i) {
          v[i] = GenPlain(rng, 10.0f);
        }
        float vv = v[0] * v[0] + v[1] * v[1];
        if (vv < 1.0e-4f) {
          v[0] = 1.0f;
          v[1] = 0.0f;
          vv = 1.0f;
        }
        else {
          const float scale = rng.Chance(50) ? 1.0e-2f : (rng.Chance(50) ? 1.0e2f : 1.0f);
          v[0] *= scale;
          v[1] *= scale;
          vv *= scale * scale;
        }
        // A direction perpendicular to V.
        float perp[2] = { GenPlain(rng, 1.0f), GenPlain(rng, 1.0f) };
        const float s = (perp[0] * v[0] + perp[1] * v[1]) / vv;
        perp[0] -= s * v[0];
        perp[1] -= s * v[1];
        float pp = perp[0] * perp[0] + perp[1] * perp[1];
        if (pp < 1.0e-6f) {
          perp[0] = v[1];
          perp[1] = -v[0];
          pp = perp[0] * perp[0] + perp[1] * perp[1];
        }
        const float inv = 1.0f / std::sqrt(pp);
        perp[0] *= inv;
        perp[1] *= inv;
        const float h = R * rng.Range(0.0f, 1.0f);
        const float t0 = GenPlain(rng, 100.0f);
        in.d[0] = t0 * v[0] + h * perp[0];
        in.d[1] = t0 * v[1] + h * perp[1];
        in.v[0] = v[0];
        in.v[1] = v[1];
        in.vsqr = v[0] * v[0] + v[1] * v[1];
        in.r2 = R * R;
        return;
      }
      const float R = std::fabs(GenPlain(rng, 100.0f)) + 0.5f;
      float v[2];
      for (int i = 0; i < 2; ++i) {
        v[i] = GenPlain(rng, 10.0f);
      }
      float vv = v[0] * v[0] + v[1] * v[1];
      if (vv < 1.0e-4f) {
        v[0] = 1.0f;
        v[1] = 0.0f;
        vv = 1.0f;
      }
      float perp[2] = { GenPlain(rng, 1.0f), GenPlain(rng, 1.0f) };
      const float s = (perp[0] * v[0] + perp[1] * v[1]) / vv;
      perp[0] -= s * v[0];
      perp[1] -= s * v[1];
      float pp = perp[0] * perp[0] + perp[1] * perp[1];
      if (pp < 1.0e-6f) {
        perp[0] = v[1];
        perp[1] = -v[0];
        pp = perp[0] * perp[0] + perp[1] * perp[1];
      }
      const float inv = 1.0f / std::sqrt(pp);
      perp[0] *= inv;
      perp[1] *= inv;
      const float h = R * rng.Range(0.0f, 1.0f);
      const float t0 = GenPlain(rng, 100.0f);
      in.d[0] = t0 * v[0] + h * perp[0];
      in.d[1] = t0 * v[1] + h * perp[1];
      in.v[0] = v[0];
      in.v[1] = v[1];
      in.vsqr = v[0] * v[0] + v[1] * v[1];
      in.r2 = R * R;
    }

    // ---- IntrBox3Sphere3's four region helpers ---------------------------
    //
    // FindFaceRegionIntersection, FindJustEdgeIntersection,
    // FindEdgeRegionIntersection and FindVertexRegionIntersection are
    // private members in the WM3 class; the retail calls them from its own
    // Find (0x00A43420 -> 0x00A43C27 / 0x00A43FC7 / 0x00A44016) and the
    // edge-region helper calls the just-edge and face-region helpers in
    // turn (0x00A4208A / 0x00A420EE / 0x00A4216A / 0x00A421CC).
    //
    // All four work in the box's own frame, in the "first octant" the
    // retail's Find builds: the centre coordinates are the distances from
    // the box's centre along the box's axes, and the velocity components
    // are the relative velocity in the same frame.  So the generators
    // produce the arguments of the frame each helper is documented to
    // work in:
    //
    //   face   : the sphere is above the +Z face, i.e. fCz >= fEz, and its
    //            in-plane coordinates fCx, fCy are within the face's
    //            extents fEx, fEy when the bAboveFace flag is set.
    //   edge   : the sphere is above the +X and the +Z faces, i.e.
    //            fCx > fEx and fCz > fEz; the flag says whether fCy is
    //            within the edge's extent fEy.
    //   vertex : the sphere is above the corner, i.e. all three of
    //            fCx > fEx, fCy > fEy, fCz > fEz.
    //   justedge: the edge runs along y at (fEx, fEz) with half length
    //            fEy; the sphere is fDx, fDz away from it in x and z and
    //            its y coordinate is fCy.
    //
    // Every generator also aims at the degenerate cases the algorithm has
    // an answer for: the "already intersecting" tests (fCz <= fEz + R for
    // the face, fDx^2 + fDz^2 <= R^2 for the edge, |D|^2 <= R^2 for the
    // vertex), the "not moving towards" tests, and the exact boundaries
    // of the face and the edge.
    //
    // The box in the input is the one the retail's constructor is given;
    // the region helpers read only the sphere's radius out of the object,
    // so the box is a well-formed one (identity axis, positive extents)
    // and nothing more.
    template <typename T>
    void GenIntrBox3Sphere3RegionBox(T& in, float ex, float ey, float ez, float r)
    {
      for (int i = 0; i < 3; ++i) {
        in.box[i] = 0.0f;                    // the box's centre
      }
      in.box[3] = 1.0f; in.box[4] = 0.0f; in.box[5] = 0.0f;   // the axis: the identity
      in.box[6] = 0.0f; in.box[7] = 1.0f; in.box[8] = 0.0f;
      in.box[9] = 0.0f; in.box[10] = 0.0f; in.box[11] = 1.0f;
      in.box[12] = ex; in.box[13] = ey; in.box[14] = ez;     // the extents
      in.sphere[0] = 0.0f; in.sphere[1] = 0.0f; in.sphere[2] = 0.0f;  // the centre
      in.sphere[3] = r;                                   // the sphere's radius
    }

    // The face region: nine arguments plus the flag.
    void GenIntrBox3Sphere3FindFaceRegionT(Rng& rng, void* p, DrawKind kind)
    {
      RegionFlagIn& in = *static_cast<RegionFlagIn*>(p);
      const float ex = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float ey = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float ez = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float r = 0.1f + std::fabs(GenPlain(rng, 20.0f));
      GenIntrBox3Sphere3RegionBox(in, ex, ey, ez, r);

      const std::uint32_t k = rng.Below(100);
      float cx, cy, cz, vx, vy, vz;
      u32 flag;
      if (k < 12) {
        // The sphere already reaches the face: fCz <= fEz + R is the
        // "already intersecting" test, and this is the case where the
        // answer is -1.
        cx = rng.Range(0.0, ex);
        cy = rng.Range(0.0, ey);
        cz = ez + r * rng.Range(-0.5, 1.0);
        vx = GenPlain(rng, 10.0f);
        vy = GenPlain(rng, 10.0f);
        vz = -std::fabs(GenPlain(rng, 10.0f));
        flag = 1u;
      }
      else if (k < 20) {
        // The exact boundary of that test: fCz == fEz + R.
        cx = rng.Range(0.0, ex);
        cy = rng.Range(0.0, ey);
        cz = ez + r;
        vx = GenPlain(rng, 10.0f);
        vy = GenPlain(rng, 10.0f);
        vz = -std::fabs(GenPlain(rng, 10.0f));
        flag = 1u;
      }
      else if (k < 30) {
        // Moving away from the face: fVz >= 0 answers 0.
        cx = rng.Range(0.0, ex);
        cy = rng.Range(0.0, ey);
        cz = ez + r + rng.Range(0.0, 50.0);
        vx = GenPlain(rng, 10.0f);
        vy = GenPlain(rng, 10.0f);
        vz = std::fabs(GenPlain(rng, 10.0f));
        flag = 1u;
      }
      else if (k < 36) {
        // Exactly parallel to the face: fVz == 0 answers 0 as well.
        cx = rng.Range(0.0, ex);
        cy = rng.Range(0.0, ey);
        cz = ez + r + rng.Range(0.0, 50.0);
        vx = GenPlain(rng, 10.0f);
        vy = GenPlain(rng, 10.0f);
        vz = 0.0f;
        flag = 1u;
      }
      else if (k < 50) {
        // Not above the face: the in-plane coordinate is outside the face's
        // extent, so the flag is off.  This is the shape the edge-region
        // helper calls the face-region helper with.
        cx = ex + rng.Range(0.0, 50.0);
        cy = rng.Range(0.0, ey);
        cz = ez + r + rng.Range(0.0, 50.0);
        vx = -std::fabs(GenPlain(rng, 10.0f));
        vy = GenPlain(rng, 10.0f);
        vz = -std::fabs(GenPlain(rng, 10.0f));
        flag = 0u;
      }
      else {
        // The ordinary case: the sphere is above the face and heading for
        // it.  The in-plane coordinates are near the face's corners often
        // enough for the edge and vertex branches to be reachable.
        cx = rng.Range(0.0, ex);
        cy = rng.Range(0.0, ey);
        cz = ez + r + rng.Range(0.0, 100.0);
        vx = GenPlain(rng, 20.0f);
        vy = GenPlain(rng, 20.0f);
        vz = -std::fabs(GenPlain(rng, 20.0f));
        flag = 1u;
      }
      in.args[0] = ex; in.args[1] = ey; in.args[2] = ez;
      in.args[3] = cx; in.args[4] = cy; in.args[5] = cz;
      in.args[6] = vx; in.args[7] = vy; in.args[8] = vz;
      in.above = (flag != 0u) ? 1.0f : 0.0f;
      (void)kind; // no extreme lane is drawn on this row in either kind
    }

    // The edge region: nine arguments plus the flag.
    void GenIntrBox3Sphere3FindEdgeRegionT(Rng& rng, void* p, DrawKind kind)
    {
      RegionFlagIn& in = *static_cast<RegionFlagIn*>(p);
      const float ex = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float ey = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float ez = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float r = 0.1f + std::fabs(GenPlain(rng, 20.0f));
      GenIntrBox3Sphere3RegionBox(in, ex, ey, ez, r);

      const std::uint32_t k = rng.Below(100);
      float cx, cy, cz, vx, vy, vz;
      u32 flag;
      if (k < 15) {
        // The sphere already reaches the edge: fDx^2 + fDz^2 <= R^2 is the
        // "already intersecting" test, and the answer is -1.
        cx = ex + r * rng.Range(0.0, 0.5);
        cz = ez + r * rng.Range(0.0, 0.5);
        cy = rng.Range(0.0, ey);
        vx = -std::fabs(GenPlain(rng, 10.0f));
        vz = -std::fabs(GenPlain(rng, 10.0f));
        vy = GenPlain(rng, 10.0f);
        flag = 1u;
      }
      else if (k < 25) {
        // Moving away from the edge: fVx*fDx + fVz*fDz >= 0 answers 0.
        cx = ex + rng.Range(0.0, 50.0);
        cz = ez + rng.Range(0.0, 50.0);
        cy = rng.Range(0.0, ey);
        vx = std::fabs(GenPlain(rng, 10.0f));
        vz = std::fabs(GenPlain(rng, 10.0f));
        vy = GenPlain(rng, 10.0f);
        flag = 1u;
      }
      else if (k < 35) {
        // The edge's extent does not contain the sphere's y coordinate:
        // the flag is off.
        cx = ex + rng.Range(0.0, 50.0);
        cz = ez + rng.Range(0.0, 50.0);
        cy = ey + rng.Range(0.0, 50.0);
        vx = -std::fabs(GenPlain(rng, 10.0f));
        vz = -std::fabs(GenPlain(rng, 10.0f));
        vy = GenPlain(rng, 10.0f);
        flag = 0u;
      }
      else {
        // The ordinary case: the sphere is above the edge and heading for
        // it.
        cx = ex + rng.Range(0.0, 50.0);
        cz = ez + rng.Range(0.0, 50.0);
        cy = rng.Range(0.0, ey);
        vx = -std::fabs(GenPlain(rng, 10.0f));
        vz = -std::fabs(GenPlain(rng, 10.0f));
        vy = GenPlain(rng, 10.0f);
        flag = 1u;
      }
      in.args[0] = ex; in.args[1] = ey; in.args[2] = ez;
      in.args[3] = cx; in.args[4] = cy; in.args[5] = cz;
      in.args[6] = vx; in.args[7] = vy; in.args[8] = vz;
      in.above = (flag != 0u) ? 1.0f : 0.0f;
      (void)kind;
    }

    // The vertex region: nine arguments, no flag.
    void GenIntrBox3Sphere3FindVertexRegionT(Rng& rng, void* p, DrawKind kind)
    {
      RegionIn& in = *static_cast<RegionIn*>(p);
      const float ex = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float ey = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float ez = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float r = 0.1f + std::fabs(GenPlain(rng, 20.0f));
      GenIntrBox3Sphere3RegionBox(in, ex, ey, ez, r);

      const std::uint32_t k = rng.Below(100);
      float cx, cy, cz, vx, vy, vz;
      if (k < 15) {
        // The sphere already reaches the corner: |D|^2 <= R^2 is the "already
        // intersecting" test, and the answer is -1.
        cx = ex + r * rng.Range(0.0, 0.5);
        cy = ey + r * rng.Range(0.0, 0.5);
        cz = ez + r * rng.Range(0.0, 0.5);
        vx = -std::fabs(GenPlain(rng, 10.0f));
        vy = -std::fabs(GenPlain(rng, 10.0f));
        vz = -std::fabs(GenPlain(rng, 10.0f));
      }
      else if (k < 25) {
        // Moving away from the corner: the dot product with the offset is
        // non-negative, and the answer is 0.
        cx = ex + rng.Range(0.0, 50.0);
        cy = ey + rng.Range(0.0, 50.0);
        cz = ez + rng.Range(0.0, 50.0);
        vx = std::fabs(GenPlain(rng, 10.0f));
        vy = std::fabs(GenPlain(rng, 10.0f));
        vz = std::fabs(GenPlain(rng, 10.0f));
      }
      else {
        // The ordinary case: the sphere is above the corner and heading for
        // it.
        cx = ex + rng.Range(0.0, 50.0);
        cy = ey + rng.Range(0.0, 50.0);
        cz = ez + rng.Range(0.0, 50.0);
        vx = -std::fabs(GenPlain(rng, 10.0f));
        vy = -std::fabs(GenPlain(rng, 10.0f));
        vz = -std::fabs(GenPlain(rng, 10.0f));
      }
      in.args[0] = ex; in.args[1] = ey; in.args[2] = ez;
      in.args[3] = cx; in.args[4] = cy; in.args[5] = cz;
      in.args[6] = vx; in.args[7] = vy; in.args[8] = vz;
      (void)kind;
    }

    // The just-edge helper: nine arguments.
    //
    // The edge runs along y at (fEx, fEz) with half length fEy, and the
    // sphere is fDx, fDz away from it in x and z with the y coordinate
    // fCy.  The helper's own "the sphere is fDx and fDz away from an edge
    // with direction y" reading is what the arguments are built from.
    void GenIntrBox3Sphere3FindJustEdgeRegionT(Rng& rng, void* p, DrawKind kind)
    {
      RegionIn& in = *static_cast<RegionIn*>(p);
      const float ex = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float ey = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float ez = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float r = 0.1f + std::fabs(GenPlain(rng, 20.0f));
      GenIntrBox3Sphere3RegionBox(in, ex, ey, ez, r);

      const std::uint32_t k = rng.Below(100);
      float dx, dz, cy, vx, vy, vz;
      if (k < 15) {
        // The sphere is within a radius of the edge's line.
        dx = r * rng.Range(0.0, 0.7);
        dz = r * rng.Range(0.0, 0.7);
        cy = rng.Range(0.0, ey);
        vx = -std::fabs(GenPlain(rng, 10.0f));
        vz = -std::fabs(GenPlain(rng, 10.0f));
        vy = GenPlain(rng, 10.0f);
      }
      else if (k < 25) {
        // The sphere is past the end of the edge in y.
        dx = rng.Range(0.0, 50.0);
        dz = rng.Range(0.0, 50.0);
        cy = ey + rng.Range(0.0, 50.0);
        vx = -std::fabs(GenPlain(rng, 10.0f));
        vz = -std::fabs(GenPlain(rng, 10.0f));
        vy = GenPlain(rng, 10.0f);
      }
      else {
        dx = rng.Range(0.0, 50.0);
        dz = rng.Range(0.0, 50.0);
        cy = rng.Range(-ey, ey);
        vx = -std::fabs(GenPlain(rng, 10.0f));
        vz = -std::fabs(GenPlain(rng, 10.0f));
        vy = GenPlain(rng, 10.0f);
      }
      in.args[0] = cy;
      in.args[1] = ex; in.args[2] = ey; in.args[3] = ez;
      in.args[4] = dx; in.args[5] = dz;
      in.args[6] = vx; in.args[7] = vy; in.args[8] = vz;
      (void)kind;
    }

    // ---- Wm3::IntrLine3Box3<float>::DoClipping and ::Clip -------------------
    //
    // DoClipping's input: fT0, fT1, the line (six floats), the box (fifteen)
    // and the bSolid flag.
    //
    // The interesting branches are:
    //
    //   * the six slab clips, and with them the Clip helper's three signs -
    //     a positive denominator, a negative one and an exactly zero one;
    //   * the "nothing was clipped" case, which with bSolid reports the
    //     segment anyway (the unclipped -MAX_REAL / +MAX_REAL parameters)
    //     and without it reports nothing;
    //   * the tangency, where the clipped interval collapses to a point and
    //     the answer is IT_POINT rather than IT_SEGMENT;
    //   * the case where the clip interval [fT0,fT1] does not reach the box
    //     at all, which is IT_EMPTY.
    //
    // The exact tangency is constructed in the box's own frame, so the
    // arithmetic is exact: the box is the unit cube at the origin and the
    // line starts at (2,0,0) along (1,1,0).  In the box's coordinates that is
    // o = (2,0,0) and d = (1,1,0), so the x slab gives [-3,-1] and the y slab
    // gives [-1,1]: they touch at t = -1 exactly, and fT0 == fT1 is what makes
    // the retail take the IT_POINT branch.
    void GenIntrLine3Box3DoClippingT(Rng& rng, void* p, DrawKind kind)
    {
      DoClippingIn& in = *static_cast<DoClippingIn*>(p);
      const bool extreme = (kind == DrawKind::WithExtremes);
      const float fMax = extreme ? std::numeric_limits<float>::max() : 1.0e6f;

      const bool tangent = rng.Chance(25);
      if (tangent) {
        // the exact tangency
        in.line[0] = 2.0f; in.line[1] = 0.0f; in.line[2] = 0.0f;   // the origin
        in.line[3] = 1.0f; in.line[4] = 1.0f; in.line[5] = 0.0f;   // the direction
        in.box[0] = in.box[1] = in.box[2] = 0.0f;                  // the centre
        in.box[3] = 1.0f; in.box[4] = 0.0f; in.box[5] = 0.0f;      // axis 0
        in.box[6] = 0.0f; in.box[7] = 1.0f; in.box[8] = 0.0f;      // axis 1
        in.box[9] = 0.0f; in.box[10] = 0.0f; in.box[11] = 1.0f;     // axis 2
        in.box[12] = in.box[13] = in.box[14] = 1.0f;               // the extents
      }
      else {
        // the box: the centre, an orthonormal axis and positive extents
        in.box[0] = GenPlain(rng, 20.0f);
        in.box[1] = GenPlain(rng, 20.0f);
        in.box[2] = GenPlain(rng, 20.0f);
        wmdiff::GenUnitVec3(rng, in.box + 3);          // axis 0
        MakePerp3(in.box + 3, in.box + 6);              // axis 1
        in.box[9] = in.box[4] * in.box[7] - in.box[5] * in.box[6];   // axis 2
        in.box[10] = in.box[5] * in.box[3] - in.box[4] * in.box[8];
        in.box[11] = in.box[3] * in.box[6] - in.box[4] * in.box[7];
        in.box[12] = static_cast<float>(rng.Range(0.5, 10.0));
        in.box[13] = static_cast<float>(rng.Range(0.5, 10.0));
        in.box[14] = static_cast<float>(rng.Range(0.5, 10.0));

        // the line: the origin around the centre and the direction a unit
        // vector, so the line passes through the box's neighbourhood
        for (int i = 0; i < 3; ++i) {
          in.line[i] = in.box[i] + GenPlain(rng, 20.0f);
        }
        wmdiff::GenUnitVec3(rng, in.line + 3);
        if (rng.Chance(10)) {
          // the degenerate direction: nothing can be clipped at all, which is
          // the bSolid branch
          in.line[3] = in.line[4] = in.line[5] = 0.0f;
          for (int i = 0; i < 3; ++i) {
            in.line[i] = in.box[i] + GenPlain(rng, 0.5f * in.box[12 + i]);
          }
        }
      }

      // the clip interval.  The whole line is the common case; the narrow
      // intervals are what make the clipping itself decide the answer, and
      // the one that misses the box entirely is what makes the helper report
      // IT_EMPTY.
      const std::uint32_t k = rng.Below(100);
      if (k < 55) {
        in.t0 = -fMax;
        in.t1 = fMax;
      }
      else if (k < 80) {
        in.t0 = static_cast<float>(rng.Range(-30.0, 0.0));
        in.t1 = static_cast<float>(rng.Range(0.0, 30.0));
      }
      else if (k < 92) {
        // an interval that is entirely after the box
        in.t0 = 100.0f;
        in.t1 = 200.0f;
      }
      else {
        // an interval that is entirely before it
        in.t0 = -200.0f;
        in.t1 = -100.0f;
      }
      in.solid = rng.Chance(50) ? 1.0f : 0.0f;   // bSolid
    }

    // Clip's input: the denominator, the numerator and the two parameters.
    //
    // The branches are the three signs of the denominator and, for each of
    // the two non-zero signs, the three answers: the segment is entirely
    // clipped (false), the interval is unchanged (true) or one of the two
    // parameters is replaced by the quotient.
    //
    // The exact cases are built from powers of two, so that the products
    // fDenom*fT0 and fDenom*fT1 are exact and the comparison lands exactly
    // on its boundary.  A random draw would be a few ulp away from it and
    // would take the other branch.
    void GenIntrLine3Box3ClipT(Rng& rng, void* p, DrawKind kind)
    {
      ClipIn& in = *static_cast<ClipIn*>(p);
      const std::uint32_t k = rng.Below(100);
      if (k < 20) {
        // the denominator is exactly zero: the answer is the sign of the
        // numerator alone.  Both +0 and -0 are drawn, because -0 is not
        // "less than zero" and the helper's third branch takes it too.
        in.denom = rng.Chance(50) ? 0.0f : -0.0f;
        in.numer = rng.Chance(50) ? 0.0f
                  : (rng.Chance(50) ? -1.0f : 1.0f);
        in.t0 = 0.0f;
        in.t1 = 1.0f;
        return;
      }
      if (k < 50) {
        // the exact boundaries: the denominator is a power of two and the
        // parameters are powers of two, so both products are exact
        const float d = static_cast<float>(1 << rng.Below(4));   // 1,2,4,8
        const float t0 = static_cast<float>(1 << rng.Below(3));   // 1,2,4
        const float t1 = t0 * 2.0f;
        in.denom = rng.Chance(50) ? d : -d;
        in.numer = in.denom * t1;            // exactly fDenom*fT1
        in.t0 = t0;
        in.t1 = t1;
        return;
      }
      if (k < 70) {
        // the segment is entirely clipped: the numerator is strictly greater
        // than fDenom*fT1
        const float d = static_cast<float>(1 << rng.Below(4));
        const float t0 = static_cast<float>(1 << rng.Below(3));
        const float t1 = t0 * 2.0f;
        in.denom = rng.Chance(50) ? d : -d;
        in.numer = in.denom * t1 * 4.0f;
        in.t0 = t0;
        in.t1 = t1;
        return;
      }
      if (k < 85) {
        // the quotient is exact: the numerator is a multiple of the
        // denominator and the parameters are powers of two
        const float d = static_cast<float>(1 << rng.Below(4));
        in.denom = rng.Chance(50) ? d : -d;
        in.numer = d * static_cast<float>(1 << rng.Below(3));
        in.t0 = 0.0f;
        in.t1 = static_cast<float>(1 << rng.Below(3));
        return;
      }
      // the general case
      in.denom = GenPlain(rng, 10.0f);
      in.numer = GenPlain(rng, 10.0f);
      in.t0 = GenPlain(rng, 5.0f);
      in.t1 = GenPlain(rng, 5.0f);
      if (rng.Chance(10)) {
        // the parameters the other way round
        const float t = in.t0;
        in.t0 = in.t1;
        in.t1 = t;
      }
      (void)kind;
    }

    // ---- Wm3::Distance<float,Vector3f>'s difference step --------------------
    //
    // The argument the setter is given - one float.
    //
    // The retail's body (0x00A391D0) compares the argument with zero and
    // takes one branch or the other, so the row has to draw both polarities
    // and several magnitudes.  1e-3 is drawn on its own: it is the default,
    // and the only value the engine ever passes.
    //
    // The magnitudes at the ends of the range are there for the second
    // member: the reciprocal 0.5/step is computed on the x87 stack by the
    // retail (an 80-bit division rounded to 24 bits and then stored to a
    // float) and in float by the rebuild, so a step big enough to make the
    // reciprocal a float denormal is where the two could part company.
    void GenDistanceSetDifferenceStepT(Rng& rng, void* p, DrawKind kind)
    {
      StepIn& in = *static_cast<StepIn*>(p);
      if (kind == DrawKind::WithExtremes) {
        const std::uint32_t k = rng.Below(100);
        if (k < 20) {
          in.step = 1e-3f;                        // the default
        }
        else if (k < 45) {
          // positive, and not the default
          static const float kPositive[] = {
              1e-30f, 1e-10f, 1e-4f, 5e-4f, 2e-3f, 0.5f, 1.0f, 2.0f, 1e3f,
              1e30f, 1.17549435e-38f };
          in.step = kPositive[rng.Below(11)];
        }
        else if (k < 62) {
          // negative
          static const float kNegative[] = {
              -1e-30f, -1e-4f, -1e-3f, -0.5f, -1.0f, -2.0f, -1e30f };
          in.step = kNegative[rng.Below(7)];
        }
        else if (k < 75) {
          // exactly zero, both signs
          in.step = rng.Chance(50) ? 0.0f : -0.0f;
        }
        else if (k < 90) {
          // big enough that 0.5/step is a float denormal
          in.step = rng.Chance(50) ? 1e38f : -1e38f;
        }
        else {
          in.step = GenPlain(rng, 10.0f);
        }
        return;
      }
      // Ordinary: the same magnitudes, but neither the denormal nor the
      // overflow tail - a step of zero would make the reciprocal an infinity.
      in.step = GenPlain(rng, 10.0f);
    }

    // ---- the Vector3 / Quaternion / Sphere3 rows --------------------------
    //
    // These rows' generators draw bit patterns rather than numbers: the
    // interesting cases are exactly the ones a float expression cannot
    // produce (a NaN payload, a negative zero, a difference of one in the
    // last bit of a lane).

    void GenVec3T(Rng& rng, void* p, DrawKind kind)
    {
      Vec3In& in = *static_cast<Vec3In*>(p);
      if (kind == DrawKind::WithExtremes) {
        wmdiff::GenVec3(rng, in.v);
        return;
      }
      for (int i = 0; i < 3; ++i) {
        in.v[i] = GenPlain(rng, 100.0f);
      }
    }

    // The comparison is bitwise over the twelve bytes of the two vectors, so
    // the generator has to produce the cases a component-wise comparison
    // would get wrong: the same bits with a NaN in a lane, +0.0f against
    // -0.0f, and two vectors that differ in one bit of one lane.  The
    // patterns are bits, never floats: the generator's own arithmetic runs
    // under _PC_24.
    void GenVec3CompareT(Rng& rng, void* p, DrawKind kind)
    {
      PairVec3In& in = *static_cast<PairVec3In*>(p);
      static constexpr u32 kZero = 0x00000000u;   // +0.0f
      static constexpr u32 kNegZero = 0x80000000u; // -0.0f
      static constexpr u32 kNaN = 0x7FC00000u;     // the default quiet NaN
      static constexpr u32 kNaN2 = 0x7FFFFFFFu;   // a NaN with a far payload
      static constexpr u32 kOne = 0x3F800000u;     // 1.0f
      static constexpr u32 kOneUp = 0x3F800001u;   // the next float above 1.0f
      static constexpr u32 kNegOne = 0xBF800000u;  // -1.0f

      u32 a[3], b[3];
      for (int i = 0; i < 3; ++i) {
        a[i] = rng.U32();
      }
      for (int i = 0; i < 3; ++i) {
        b[i] = rng.U32();
      }

      if (kind == DrawKind::WithExtremes) {
        const u32 mode = rng.Below(100);
        const int lane = static_cast<int>(rng.Below(3));
        if (mode < 25) {
          // the two vectors are bit-identical: the answer is 0.  A NaN in a
          // lane is still bit-identical - a component-wise comparison would
          // call these different.
          for (int i = 0; i < 3; ++i) {
            b[i] = a[i];
          }
          if (rng.Chance(40)) {
            a[lane] = kNaN;
            b[lane] = kNaN;
          }
        }
        else if (mode < 40) {
          // +0.0f against -0.0f in one lane: the bits differ, the values do
          // not.  The answer is -1 or +1, never 0.
          a[lane] = kZero;
          b[lane] = kNegZero;
        }
        else if (mode < 55) {
          // one lane one ulp apart, and the other two identical
          a[lane] = kOne;
          b[lane] = kOneUp;
          for (int i = 0; i < 3; ++i) {
            if (i != lane) {
              b[i] = a[i];
            }
          }
        }
        else if (mode < 65) {
          // the same NaN in both vectors, the other lanes identical
          a[lane] = kNaN;
          b[lane] = kNaN;
          for (int i = 0; i < 3; ++i) {
            if (i != lane) {
              b[i] = a[i];
            }
          }
        }
        else if (mode < 75) {
          // two NaNs with different payloads: the last byte differs by 0xFF.
          // A raw byte difference would be -255; both memcmps answer -1.
          a[lane] = kNaN;
          b[lane] = kNaN2;
          for (int i = 0; i < 3; ++i) {
            if (i != lane) {
              b[i] = a[i];
            }
          }
        }
        else if (mode < 85) {
          // a lane whose first byte differs by 0x40: a raw byte difference
          // would be 64; both memcmps answer -1 or +1
          a[lane] = kOne;
          b[lane] = kNegOne;
          for (int i = 0; i < 3; ++i) {
            if (i != lane) {
              b[i] = a[i];
            }
          }
        }
        else {
          // a lane that differs in a random bit, the other two identical
          a[lane] ^= (1u << rng.Below(32));
          for (int i = 0; i < 3; ++i) {
            if (i != lane) {
              b[i] = a[i];
            }
          }
        }
      }
      else {
        // Ordinary: the same shapes, but every lane is a plain number.  The
        // "equal" and "one ulp apart" cases are kept, because they are the
        // ones the comparison turns on.
        const u32 mode = rng.Below(100);
        const int lane = static_cast<int>(rng.Below(3));
        for (int i = 0; i < 3; ++i) {
          b[i] = a[i];
        }
        if (mode < 50) {
          // one ulp apart in one lane
          a[lane] = wmdiff::Bits(1.0f);
          b[lane] = wmdiff::Bits(1.0f) + 1u;
        }
        else if (mode < 70) {
          // the two lanes differ in sign
          a[lane] = wmdiff::Bits(1.0f);
          b[lane] = wmdiff::Bits(-1.0f);
        }
        else if (mode < 85) {
          // one ulp apart in another lane
          a[(lane + 1) % 3] = wmdiff::Bits(2.0f);
          b[(lane + 1) % 3] = wmdiff::Bits(2.0f) + 1u;
        }
      }

      for (int i = 0; i < 3; ++i) {
        in.a[i] = FromBits(a[i]);
        in.b[i] = FromBits(b[i]);
      }
    }

    // The same idea over the twelve bytes of the two vectors, for the
    // operator== row: the same bits with a NaN in a lane (a float comparison
    // says those differ), +0.0f against -0.0f (the values are equal, the bits
    // are not), and one bit of one lane changed.
    void GenVec3EqualsT(Rng& rng, void* p, DrawKind kind)
    {
      PairVec3In& in = *static_cast<PairVec3In*>(p);
      static constexpr u32 kZero = 0x00000000u;
      static constexpr u32 kNegZero = 0x80000000u;
      static constexpr u32 kNaN = 0x7FC00000u;
      static constexpr u32 kOne = 0x3F800000u;
      static constexpr u32 kOneUp = 0x3F800001u;
      static constexpr u32 kNegOne = 0xBF800000u;

      u32 a[3], b[3];
      for (int i = 0; i < 3; ++i) {
        a[i] = rng.U32();
      }
      for (int i = 0; i < 3; ++i) {
        b[i] = rng.U32();
      }

      if (kind == DrawKind::WithExtremes) {
        const u32 mode = rng.Below(100);
        const int lane = static_cast<int>(rng.Below(3));
        if (mode < 25) {
          // bit-identical, sometimes with a NaN in a lane
          for (int i = 0; i < 3; ++i) {
            b[i] = a[i];
          }
          if (rng.Chance(40)) {
            a[lane] = kNaN;
            b[lane] = kNaN;
          }
        }
        else if (mode < 40) {
          // +0.0f against -0.0f in one lane
          a[lane] = kZero;
          b[lane] = kNegZero;
        }
        else if (mode < 55) {
          // one lane one ulp apart
          a[lane] = kOne;
          b[lane] = kOneUp;
          for (int i = 0; i < 3; ++i) {
            if (i != lane) {
              b[i] = a[i];
            }
          }
        }
        else if (mode < 65) {
          // one lane whose first byte differs by 0x40
          a[lane] = kOne;
          b[lane] = kNegOne;
          for (int i = 0; i < 3; ++i) {
            if (i != lane) {
              b[i] = a[i];
            }
          }
        }
        else if (mode < 75) {
          // one bit different in one lane
          a[lane] ^= (1u << rng.Below(32));
          for (int i = 0; i < 3; ++i) {
            if (i != lane) {
              b[i] = a[i];
            }
          }
        }
      }
      else {
        const u32 mode = rng.Below(100);
        const int lane = static_cast<int>(rng.Below(3));
        for (int i = 0; i < 3; ++i) {
          b[i] = a[i];
        }
        if (mode < 50) {
          a[lane] = wmdiff::Bits(1.0f);
          b[lane] = wmdiff::Bits(1.0f) + 1u;
        }
        else if (mode < 70) {
          a[lane] = wmdiff::Bits(1.0f);
          b[lane] = wmdiff::Bits(-1.0f);
        }
      }

      for (int i = 0; i < 3; ++i) {
        in.a[i] = FromBits(a[i]);
        in.b[i] = FromBits(b[i]);
      }
    }

    // The same idea over the sixteen bytes of the two quaternions.  The
    // quaternion's CompareArrays answers -1, 0 or +1, so it says which byte
    // differed first and not merely that one did - that is why it is the
    // strong one of the two.
    void GenQuatEqualsT(Rng& rng, void* p, DrawKind kind)
    {
      PairQuatIn& in = *static_cast<PairQuatIn*>(p);
      static constexpr u32 kZero = 0x00000000u;
      static constexpr u32 kNegZero = 0x80000000u;
      static constexpr u32 kNaN = 0x7FC00000u;
      static constexpr u32 kNaN2 = 0x7FFFFFFFu;
      static constexpr u32 kOne = 0x3F800000u;
      static constexpr u32 kOneUp = 0x3F800001u;

      u32 a[4], b[4];
      for (int i = 0; i < 4; ++i) {
        a[i] = rng.U32();
      }
      for (int i = 0; i < 4; ++i) {
        b[i] = rng.U32();
      }

      if (kind == DrawKind::WithExtremes) {
        const u32 mode = rng.Below(100);
        const int lane = static_cast<int>(rng.Below(4));
        if (mode < 25) {
          // bit-identical, sometimes with a NaN in a lane
          for (int i = 0; i < 4; ++i) {
            b[i] = a[i];
          }
          if (rng.Chance(40)) {
            a[lane] = kNaN;
            b[lane] = kNaN;
          }
        }
        else if (mode < 40) {
          // +0.0f against -0.0f in one lane
          a[lane] = kZero;
          b[lane] = kNegZero;
        }
        else if (mode < 55) {
          // one lane one ulp apart
          a[lane] = kOne;
          b[lane] = kOneUp;
          for (int i = 0; i < 4; ++i) {
            if (i != lane) {
              b[i] = a[i];
            }
          }
        }
        else if (mode < 70) {
          // two NaNs with different payloads
          a[lane] = kNaN;
          b[lane] = kNaN2;
          for (int i = 0; i < 4; ++i) {
            if (i != lane) {
              b[i] = a[i];
            }
          }
        }
        else if (mode < 80) {
          // one bit different in one lane
          a[lane] ^= (1u << rng.Below(32));
          for (int i = 0; i < 4; ++i) {
            if (i != lane) {
              b[i] = a[i];
            }
          }
        }
      }
      else {
        const u32 mode = rng.Below(100);
        const int lane = static_cast<int>(rng.Below(4));
        for (int i = 0; i < 4; ++i) {
          b[i] = a[i];
        }
        if (mode < 60) {
          a[lane] = wmdiff::Bits(1.0f);
          b[lane] = wmdiff::Bits(1.0f) + 1u;
        }
      }

      for (int i = 0; i < 4; ++i) {
        in.a[i] = FromBits(a[i]);
        in.b[i] = FromBits(b[i]);
      }
    }

    // The one constructor in this library whose compared bytes are all
    // values: the object is a Vector3 and a float, sixteen bytes, no padding,
    // no references, and the retail's body writes all sixteen.
    //
    // The retail copies the centre's lanes through the x87, so a quiet NaN
    // keeps its payload; the generator therefore uses quiet NaNs only - a
    // signalling one would be quieted by the retail and copied unchanged by
    // the rebuild, which is the harness's known divergence class for pure
    // copies and not a property of the rebuild.
    void GenSphere3CtorT(Rng& rng, void* p, DrawKind kind)
    {
      Sphere3CtorIn& in = *static_cast<Sphere3CtorIn*>(p);
      static constexpr u32 kZero = 0x00000000u;    // +0.0f
      static constexpr u32 kNegZero = 0x80000000u;  // -0.0f
      static constexpr u32 kNaN = 0x7FC00000u;      // the default quiet NaN
      static constexpr u32 kNaN2 = 0x7FC00001u;    // a quiet NaN with a payload
      static constexpr u32 kDenorm = 0x00000001u;   // the smallest denormal
      static constexpr u32 kOne = 0x3F800000u;      // 1.0f
      static constexpr u32 kNegOne = 0xBF800000u;   // -1.0f

      u32 c[3];
      u32 r;
      if (kind == DrawKind::WithExtremes) {
        // the radius: +0.0f, -0.0f, a denormal, a NaN, and the rest random
        const u32 rmode = rng.Below(100);
        if (rmode < 15) {
          r = kZero;
        }
        else if (rmode < 30) {
          r = kNegZero;
        }
        else if (rmode < 40) {
          r = kDenorm;
        }
        else if (rmode < 50) {
          r = kNaN;
        }
        else if (rmode < 60) {
          r = kNaN2;
        }
        else {
          r = rng.U32();
        }

        // the centre: one lane at a time, the rest random
        const u32 cmode = rng.Below(100);
        const int lane = static_cast<int>(rng.Below(3));
        for (int i = 0; i < 3; ++i) {
          c[i] = rng.U32();
        }
        if (cmode < 15) {
          c[lane] = kZero;
        }
        else if (cmode < 30) {
          c[lane] = kNegZero;
        }
        else if (cmode < 40) {
          c[lane] = kDenorm;
        }
        else if (cmode < 50) {
          c[lane] = kNaN;
        }
        else if (cmode < 60) {
          c[lane] = kNaN2;
        }
        else if (cmode < 70) {
          c[lane] = kOne;
        }
        else if (cmode < 80) {
          c[lane] = kNegOne;
        }
      }
      else {
        // Ordinary: plain numbers only - no NaN, no infinity, no subnormal.
        for (int i = 0; i < 3; ++i) {
          c[i] = wmdiff::Bits(GenPlain(rng, 10.0f));
        }
        r = wmdiff::Bits(GenPlain(rng, 10.0f));
      }

      // A random draw can be a *signalling* NaN, and the retail's x87 copy
      // would quiet it while the rebuild's plain copy would not - that is the
      // harness's known divergence class for pure copies, not a property of
      // the rebuild.  Every NaN the generator draws is quieted here.
      const u32 q[4] = { c[0], c[1], c[2], r };
      for (int i = 0; i < 4; ++i) {
        const u32 u = q[i];
        if ((u & 0x7F800000u) == 0x7F800000u && (u & 0x007FFFFFu) != 0
            && (u & 0x00400000u) == 0)
        {
          if (i < 3) {
            c[i] = u | 0x00400000u;
          }
          else {
            r = u | 0x00400000u;
          }
        }
      }

      in.center[0] = FromBits(c[0]);
      in.center[1] = FromBits(c[1]);
      in.center[2] = FromBits(c[2]);
      in.radius = FromBits(r);
    }

    // ---- retail side (the existing thunks) ---------------------------------

    void DistVector3Box3GetSquaredRetailT(const void* p, void* q)
    {
      const DistVector3Box3In& in = *static_cast<const DistVector3Box3In*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::DistVector3Box3Ctor(storage, in.vec, in.box);
      out.ret = wm3collretail::DistVector3Box3GetSquared(storage);
    }

    void DistVector3Segment3GetSquaredRetailT(const void* p, void* q)
    {
      const DistVector3Segment3In& in = *static_cast<const DistVector3Segment3In*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::DistVector3Segment3Ctor(storage, in.vec, in.segment);
      out.ret = wm3collretail::DistVector3Segment3GetSquared(storage);
    }

    void DistVector3Segment3GetRetailT(const void* p, void* q)
    {
      const DistVector3Segment3In& in = *static_cast<const DistVector3Segment3In*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::DistVector3Segment3Ctor(storage, in.vec, in.segment);
      out.ret = wm3collretail::DistVector3Segment3Get(storage);
    }

    void IntrBox2Circle2TestRetailT(const void* p, void* q)
    {
      const IntrBox2Circle2In& in = *static_cast<const IntrBox2Circle2In*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::IntrBox2Circle2Ctor(storage, in.box, in.circle);
      out.ret = wm3collretail::IntrBox2Circle2Test(storage);
    }

    void IntrLine3Box3TestRetailT(const void* p, void* q)
    {
      const IntrLine3Box3In& in = *static_cast<const IntrLine3Box3In*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::IntrLine3Box3Ctor(storage, in.line, in.box);
      out.ret = wm3collretail::IntrLine3Box3Test(storage);
    }

    void IntrSegment3Box3FindRetailT(const void* p, void* q)
    {
      const IntrSegment3Box3In& in = *static_cast<const IntrSegment3Box3In*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::IntrSegment3Box3Ctor(storage, in.segment, in.box, in.solid);
      out.ret = wm3collretail::IntrSegment3Box3Find(storage);
    }

    void IntrBox3Sphere3TestRetailT(const void* p, void* q)
    {
      const IntrBox3Sphere3In& in = *static_cast<const IntrBox3Sphere3In*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::IntrBox3Sphere3Ctor(storage, in.box, in.sphere);
      out.ret = wm3collretail::IntrBox3Sphere3Test(storage);
    }

    void IntrBox3Sphere3FindRetailT(const void* p, void* q)
    {
      const IntrBox3Sphere3FindIn& in = *static_cast<const IntrBox3Sphere3FindIn*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::IntrBox3Sphere3Ctor(storage, in.box, in.sphere);
      out.ret = wm3collretail::IntrBox3Sphere3Find(storage, in.tmax, in.vel0, in.vel1);
    }

    void IntrBox3Sphere3GetContactPointRetailT(const void* p, void* q)
    {
      const IntrBox3Sphere3FindIn& in = *static_cast<const IntrBox3Sphere3FindIn*>(p);
      PointTimeOut& out = *static_cast<PointTimeOut*>(q);
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::IntrBox3Sphere3Ctor(storage, in.box, in.sphere);
      wm3collretail::IntrBox3Sphere3Find(storage, in.tmax, in.vel0, in.vel1);
      // m_iIntersectionType is at this + 0x08: the retail's Find writes 8
      // (IT_OTHER) on the "sphere centre inside the box" path and 1
      // (IT_POINT) / 0 (IT_EMPTY) on the other two.
      const int type = *reinterpret_cast<const int*>(storage + 0x08);
      if (type == 8) {
        const float* pt = wm3collretail::IntrBox3Sphere3GetContactPoint(storage);
        std::memcpy(out.point, pt, 3 * sizeof(float));
      }
      else {
        out.point[0] = out.point[1] = out.point[2] = 0.0f;
      }
      // The contact time is at this + 0x04 - the retail writes it there on
      // every path that has a defined value for it.
      std::memcpy(&out.time, storage + 0x04, sizeof(float));
    }

    // GetPoint is only meaningful after a successful Find: Find() leaves
    // m_akPoint untouched on the miss path and the source's own GetPoint
    // asserts 0 <= i < m_iQuantity. Index 0 only - index 1 needs
    // m_iQuantity, which the generator cannot know before running Find. Same
    // narrowing as IntrSegment3Box3::GetPoint, and for the same reason.
    void IntrSegment3Box3GetPointRetailT(const void* p, void* q)
    {
      const IntrSegment3Box3In& in = *static_cast<const IntrSegment3Box3In*>(p);
      PointOut& out = *static_cast<PointOut*>(q);
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::IntrSegment3Box3Ctor(storage, in.segment, in.box, in.solid);
      if (wm3collretail::IntrSegment3Box3Find(storage))
      {
        const float* pt = wm3collretail::IntrSegment3Box3GetPoint(storage, 0u);
        std::memcpy(out.point, pt, 3 * sizeof(float));
      }
      else
      {
        out.point[0] = out.point[1] = out.point[2] = 0.0f;
      }
    }

    void IntrSegment3Sphere3FindRetailT(const void* p, void* q)
    {
      const IntrSegment3Sphere3In& in = *static_cast<const IntrSegment3Sphere3In*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::IntrSegment3Sphere3Ctor(storage, in.segment, in.sphere);
      out.ret = wm3collretail::IntrSegment3Sphere3Find(storage);
    }

    void IntrSegment3Sphere3GetPointRetailT(const void* p, void* q)
    {
      const IntrSegment3Sphere3In& in = *static_cast<const IntrSegment3Sphere3In*>(p);
      PointOut& out = *static_cast<PointOut*>(q);
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::IntrSegment3Sphere3Ctor(storage, in.segment, in.sphere);
      if (wm3collretail::IntrSegment3Sphere3Find(storage))
      {
        const float* pt = wm3collretail::IntrSegment3Sphere3GetPoint(storage, 0u);
        std::memcpy(out.point, pt, 3 * sizeof(float));
      }
      else
      {
        out.point[0] = out.point[1] = out.point[2] = 0.0f;
      }
    }

    void IntrBox2Box2TestRetailT(const void* p, void* q)
    {
      const IntrBox2Box2In& in = *static_cast<const IntrBox2Box2In*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::IntrBox2Box2Ctor(storage, in.box0, in.box1);
      out.ret = wm3collretail::IntrBox2Box2Test(storage);
    }

    void DistVector2Box2GetSquaredRetailT(const void* p, void* q)
    {
      const DistVector2Box2In& in = *static_cast<const DistVector2Box2In*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::DistVector2Box2Ctor(storage, in.vec, in.box);
      float value = 0.0f;
      wm3collretail::DistVector2Box2GetSquared(storage, &value);
      out.ret = value;
    }

    void DistVector2Box2GetRetailT(const void* p, void* q)
    {
      const DistVector2Box2In& in = *static_cast<const DistVector2Box2In*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::DistVector2Box2Ctor(storage, in.vec, in.box);
      float value = 0.0f;
      wm3collretail::DistVector2Box2Get(storage, &value);
      out.ret = value;
    }

    void InBoxRetailT(const void* p, void* q)
    {
      const InBoxIn& in = *static_cast<const InBoxIn*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      out.ret = wm3collretail::InBox(in.point, in.box);
    }

    void ContAlignedBoxRetailT(const void* p, void* q)
    {
      const ContAlignedBoxIn& in = *static_cast<const ContAlignedBoxIn*>(p);
      Box15Out& out = *static_cast<Box15Out*>(q);
      float afBox[15] = { 0.0f };
      wm3collretail::ContAlignedBox(afBox, static_cast<int>(in.count), in.points);
      for (int i = 0; i < 15; ++i) {
        out.box[i] = afBox[i];
      }
    }

    void IntrBox3Sphere3GetVertexIntersectionRetailT(const void* p, void* q)
    {
      const VertexIntersectionIn& in = *static_cast<const VertexIntersectionIn*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      out.ret = wm3collretail::IntrBox3Sphere3GetVertexIntersection(
        in.d[0], in.d[1], in.d[2], in.v[0], in.v[1], in.v[2], in.r2);
    }

    void IntrBox3Sphere3GetEdgeIntersectionRetailT(const void* p, void* q)
    {
      const EdgeIntersectionIn& in = *static_cast<const EdgeIntersectionIn*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      out.ret = wm3collretail::IntrBox3Sphere3GetEdgeIntersection(
        in.d[0], in.d[1], in.v[0], in.v[1], in.vsqr, in.r2);
    }

    // The four region helpers: the retail side constructs the retail's own
    // IntrBox3Sphere3 and calls the retail's helper; the rebuilt side calls
    // the rebuilt helper on a rebuilt object.  Both start the three
    // intersection coordinates from the same sentinel, so the paths that
    // write none of them agree.
    //
    // The contact time is the member the retail keeps at [this + 0x04] and
    // the rebuilt class reaches through GetContactTime(); both are read
    // after the call.
    void IntrBox3Sphere3FindFaceRegionRetailT(const void* p, void* q)
    {
      const RegionFlagIn& in = *static_cast<const RegionFlagIn*>(p);
      RegionOut& out = *static_cast<RegionOut*>(q);
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::IntrBox3Sphere3Ctor(storage, in.box, in.sphere);
      float ix = wm3collharness::UndefCoord();
      float iy = wm3collharness::UndefCoord();
      float iz = wm3collharness::UndefCoord();
      const int iRet = wm3collretail::IntrBox3Sphere3FindFaceRegionIntersection(
          storage, in.args[0], in.args[1], in.args[2], in.args[3], in.args[4],
          in.args[5], in.args[6], in.args[7], in.args[8], &ix, &iy, &iz,
          (in.above != 0.0f) ? 1u : 0u);
      out.ret = static_cast<u32>(iRet);
      out.point[0] = ix;
      out.point[1] = iy;
      out.point[2] = iz;
      float fTime = 0.0f;
      std::memcpy(&fTime, storage + 4, sizeof fTime);
      out.time = fTime;
    }

    void IntrBox3Sphere3FindEdgeRegionRetailT(const void* p, void* q)
    {
      const RegionFlagIn& in = *static_cast<const RegionFlagIn*>(p);
      RegionOut& out = *static_cast<RegionOut*>(q);
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::IntrBox3Sphere3Ctor(storage, in.box, in.sphere);
      float ix = wm3collharness::UndefCoord();
      float iy = wm3collharness::UndefCoord();
      float iz = wm3collharness::UndefCoord();
      const int iRet = wm3collretail::IntrBox3Sphere3FindEdgeRegionIntersection(
          storage, in.args[0], in.args[1], in.args[2], in.args[3], in.args[4],
          in.args[5], in.args[6], in.args[7], in.args[8], &ix, &iy, &iz,
          (in.above != 0.0f) ? 1u : 0u);
      out.ret = static_cast<u32>(iRet);
      out.point[0] = ix;
      out.point[1] = iy;
      out.point[2] = iz;
      float fTime = 0.0f;
      std::memcpy(&fTime, storage + 4, sizeof fTime);
      out.time = fTime;
    }

    void IntrBox3Sphere3FindJustEdgeRegionRetailT(const void* p, void* q)
    {
      const RegionIn& in = *static_cast<const RegionIn*>(p);
      RegionOut& out = *static_cast<RegionOut*>(q);
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::IntrBox3Sphere3Ctor(storage, in.box, in.sphere);
      float ix = wm3collharness::UndefCoord();
      float iy = wm3collharness::UndefCoord();
      float iz = wm3collharness::UndefCoord();
      const int iRet = wm3collretail::IntrBox3Sphere3FindJustEdgeIntersection(
          storage, in.args[0], in.args[1], in.args[2], in.args[3], in.args[4],
          in.args[5], in.args[6], in.args[7], in.args[8], &ix, &iy, &iz);
      out.ret = static_cast<u32>(iRet);
      out.point[0] = ix;
      out.point[1] = iy;
      out.point[2] = iz;
      float fTime = 0.0f;
      std::memcpy(&fTime, storage + 4, sizeof fTime);
      out.time = fTime;
    }

    void IntrBox3Sphere3FindVertexRegionRetailT(const void* p, void* q)
    {
      const RegionIn& in = *static_cast<const RegionIn*>(p);
      RegionOut& out = *static_cast<RegionOut*>(q);
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::IntrBox3Sphere3Ctor(storage, in.box, in.sphere);
      float ix = wm3collharness::UndefCoord();
      float iy = wm3collharness::UndefCoord();
      float iz = wm3collharness::UndefCoord();
      const int iRet = wm3collretail::IntrBox3Sphere3FindVertexRegionIntersection(
          storage, in.args[0], in.args[1], in.args[2], in.args[3], in.args[4],
          in.args[5], in.args[6], in.args[7], in.args[8], &ix, &iy, &iz);
      out.ret = static_cast<u32>(iRet);
      out.point[0] = ix;
      out.point[1] = iy;
      out.point[2] = iz;
      float fTime = 0.0f;
      std::memcpy(&fTime, storage + 4, sizeof fTime);
      out.time = fTime;
    }

    void ContAlignedBoxMaskRetailT(const void* p, void* q)
    {
      const ContAlignedBoxMaskIn& in = *static_cast<const ContAlignedBoxMaskIn*>(p);
      Box15Out& out = *static_cast<Box15Out*>(q);
      const int iCount = static_cast<int>(in.count);
      const bool bMasked = in.mode != 0;
      float afBox[15] = { 0.0f };
      wm3collretail::ContAlignedBoxMask(afBox, iCount, in.points,
          bMasked ? in.mask : nullptr);
      for (int i = 0; i < 15; ++i) {
        out.box[i] = afBox[i];
      }
    }

    void IntrLine3Box3DoClippingRetailT(const void* p, void* q)
    {
      const DoClippingIn& in = *static_cast<const DoClippingIn*>(p);
      DoClippingOut& out = *static_cast<DoClippingOut*>(q);
      // The two points are only written on the paths that define them, so
      // both sides start them from the same sentinel.
      float points[6];
      for (int i = 0; i < 6; ++i) {
        points[i] = wm3collharness::UndefCoord();
      }
      int quantity = 0;
      int type = 0;
      const u32 hit = wm3collretail::IntrLine3Box3DoClipping(
          in.t0, in.t1, in.line, in.line + 3, in.box, (in.solid != 0.0f) ? 1 : 0,
          &quantity, points, &type);
      out.hit = hit;
      out.quantity = static_cast<u32>(quantity);
      out.type = static_cast<u32>(type);
      std::memcpy(out.points, points, 6 * sizeof(float));
    }

    void IntrLine3Box3ClipRetailT(const void* p, void* q)
    {
      const ClipIn& in = *static_cast<const ClipIn*>(p);
      ClipOut& out = *static_cast<ClipOut*>(q);
      float t0 = in.t0;
      float t1 = in.t1;
      const u32 ok = wm3collretail::IntrLine3Box3Clip(in.denom, in.numer, &t0, &t1);
      out.ret = ok;
      out.t0 = t0;
      out.t1 = t1;
    }

    void DistanceSetDifferenceStepRetailT(const void* p, void* q)
    {
      const StepIn& in = *static_cast<const StepIn*>(p);
      StepOut& out = *static_cast<StepOut*>(q);
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::DistanceCtor(storage);
      wm3collretail::DistanceSetDifferenceStep(storage, in.step);
      // m_fDifferenceStep at [this+0x2C] and m_fInvTwoDifferenceStep at
      // [this+0x30]
      std::memcpy(&out.step, storage + 0x2C, 2 * sizeof(float));
    }

    // The same two members straight after construction: the constructor's own
    // defaults, which the retail inlines (1e-3 and 500.0f) and the rebuild
    // reaches through SetDifferenceStep(1e-3).
    void DistanceDifferenceStepDefaultRetailT(const void* p, void* q)
    {
      (void)p;
      StepOut& out = *static_cast<StepOut*>(q);
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::DistanceCtor(storage);
      std::memcpy(&out.step, storage + 0x2C, 2 * sizeof(float));
    }

    void Vector3LengthRetailT(const void* p, void* q)
    {
      const Vec3In& in = *static_cast<const Vec3In*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      out.ret = wm3collretail::Vector3Length(in.v);
    }

    void Vector3CompareArraysRetailT(const void* p, void* q)
    {
      const PairVec3In& in = *static_cast<const PairVec3In*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      out.ret = static_cast<u32>(
        wm3collretail::Vector3CompareArrays(in.a, in.b));
    }

    void Vector3OperatorEqualsRetailT(const void* p, void* q)
    {
      const PairVec3In& in = *static_cast<const PairVec3In*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      out.ret = wm3collretail::Vector3OperatorEquals(in.a, in.b) ? 1u : 0u;
    }

    void QuaternionCompareArraysRetailT(const void* p, void* q)
    {
      const PairQuatIn& in = *static_cast<const PairQuatIn*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      out.ret = static_cast<u32>(
        wm3collretail::QuaternionCompareArrays(in.a, in.b));
    }

    void QuaternionOperatorEqualsRetailT(const void* p, void* q)
    {
      const PairQuatIn& in = *static_cast<const PairQuatIn*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      out.ret = wm3collretail::QuaternionOperatorEquals(in.a, in.b) ? 1u : 0u;
    }

    void Sphere3CtorRetailT(const void* p, void* q)
    {
      const Sphere3CtorIn& in = *static_cast<const Sphere3CtorIn*>(p);
      Obj4Out& out = *static_cast<Obj4Out*>(q);
      wm3collretail::Sphere3Ctor(&out.obj[0], in.center, in.radius);
    }

    // ---- our side (the harness forwards to the real Wild Magic 3.8) --------

    void DistVector3Box3GetSquaredOursT(const void* p, void* q)
    {
      const DistVector3Box3In& in = *static_cast<const DistVector3Box3In*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      out.ret = wm3collharness::DistVector3Box3GetSquared(in.vec, in.box);
    }

    void DistVector3Segment3GetSquaredOursT(const void* p, void* q)
    {
      const DistVector3Segment3In& in = *static_cast<const DistVector3Segment3In*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      out.ret = wm3collharness::DistVector3Segment3GetSquared(in.vec, in.segment);
    }

    void DistVector3Segment3GetOursT(const void* p, void* q)
    {
      const DistVector3Segment3In& in = *static_cast<const DistVector3Segment3In*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      out.ret = wm3collharness::DistVector3Segment3Get(in.vec, in.segment);
    }

    void IntrBox2Circle2TestOursT(const void* p, void* q)
    {
      const IntrBox2Circle2In& in = *static_cast<const IntrBox2Circle2In*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      out.ret = wm3collharness::IntrBox2Circle2Test(in.box, in.circle);
    }

    void IntrLine3Box3TestOursT(const void* p, void* q)
    {
      const IntrLine3Box3In& in = *static_cast<const IntrLine3Box3In*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      out.ret = wm3collharness::IntrLine3Box3Test(in.line, in.box);
    }

    void IntrSegment3Box3FindOursT(const void* p, void* q)
    {
      const IntrSegment3Box3In& in = *static_cast<const IntrSegment3Box3In*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      out.ret = wm3collharness::IntrSegment3Box3Find(in.segment, in.box, in.solid);
    }

    void IntrBox3Sphere3TestOursT(const void* p, void* q)
    {
      const IntrBox3Sphere3In& in = *static_cast<const IntrBox3Sphere3In*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      out.ret = wm3collharness::IntrBox3Sphere3Test(in.box, in.sphere);
    }

    void IntrBox3Sphere3FindOursT(const void* p, void* q)
    {
      const IntrBox3Sphere3FindIn& in = *static_cast<const IntrBox3Sphere3FindIn*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      out.ret = wm3collharness::IntrBox3Sphere3Find(in.box, in.sphere, in.tmax,
                                                    in.vel0, in.vel1);
    }

    void IntrBox3Sphere3GetContactPointOursT(const void* p, void* q)
    {
      const IntrBox3Sphere3FindIn& in = *static_cast<const IntrBox3Sphere3FindIn*>(p);
      PointTimeOut& out = *static_cast<PointTimeOut*>(q);
      wm3collharness::IntrBox3Sphere3GetContactPoint(in.box, in.sphere, in.tmax,
                                                      in.vel0, in.vel1, out.point);
    }

    void IntrSegment3Box3GetPointOursT(const void* p, void* q)
    {
      const IntrSegment3Box3In& in = *static_cast<const IntrSegment3Box3In*>(p);
      PointOut& out = *static_cast<PointOut*>(q);
      wm3collharness::IntrSegment3Box3GetPoint(in.segment, in.box, in.solid, 0u,
                                                out.point);
    }

    void IntrSegment3Sphere3FindOursT(const void* p, void* q)
    {
      const IntrSegment3Sphere3In& in = *static_cast<const IntrSegment3Sphere3In*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      out.ret = wm3collharness::IntrSegment3Sphere3Find(in.segment, in.sphere);
    }

    void IntrSegment3Sphere3GetPointOursT(const void* p, void* q)
    {
      const IntrSegment3Sphere3In& in = *static_cast<const IntrSegment3Sphere3In*>(p);
      PointOut& out = *static_cast<PointOut*>(q);
      wm3collharness::IntrSegment3Sphere3GetPoint(in.segment, in.sphere, 0u,
                                                   out.point);
    }

    void IntrBox2Box2TestOursT(const void* p, void* q)
    {
      const IntrBox2Box2In& in = *static_cast<const IntrBox2Box2In*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      out.ret = wm3collharness::IntrBox2Box2Test(in.box0, in.box1);
    }

    void DistVector2Box2GetSquaredOursT(const void* p, void* q)
    {
      const DistVector2Box2In& in = *static_cast<const DistVector2Box2In*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      out.ret = wm3collharness::DistVector2Box2GetSquared(in.vec, in.box);
    }

    void DistVector2Box2GetOursT(const void* p, void* q)
    {
      const DistVector2Box2In& in = *static_cast<const DistVector2Box2In*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      u32 bits = 0;
      wm3collharness::DistVector2Box2Get(in.vec, in.box, &bits);
      out.ret = FromBits(bits);
    }

    void InBoxOursT(const void* p, void* q)
    {
      const InBoxIn& in = *static_cast<const InBoxIn*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      out.ret = wm3collharness::InBox(in.point, in.box);
    }

    void ContAlignedBoxOursT(const void* p, void* q)
    {
      const ContAlignedBoxIn& in = *static_cast<const ContAlignedBoxIn*>(p);
      Box15Out& out = *static_cast<Box15Out*>(q);
      u32 words[15] = { 0 };
      wm3collharness::ContAlignedBox(static_cast<int>(in.count), in.points, words);
      for (int i = 0; i < 15; ++i) {
        out.box[i] = FromBits(words[i]);
      }
    }

    void IntrBox3Sphere3GetVertexIntersectionOursT(const void* p, void* q)
    {
      const VertexIntersectionIn& in = *static_cast<const VertexIntersectionIn*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      out.ret = wm3collharness::IntrBox3Sphere3GetVertexIntersection(
        in.d[0], in.d[1], in.d[2], in.v[0], in.v[1], in.v[2], in.r2);
    }

    void IntrBox3Sphere3GetEdgeIntersectionOursT(const void* p, void* q)
    {
      const EdgeIntersectionIn& in = *static_cast<const EdgeIntersectionIn*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      out.ret = wm3collharness::IntrBox3Sphere3GetEdgeIntersection(
        in.d[0], in.d[1], in.v[0], in.v[1], in.vsqr, in.r2);
    }

    void IntrBox3Sphere3FindFaceRegionOursT(const void* p, void* q)
    {
      const RegionFlagIn& in = *static_cast<const RegionFlagIn*>(p);
      RegionOut& out = *static_cast<RegionOut*>(q);
      const int iRet = wm3collharness::IntrBox3Sphere3FindFaceRegionIntersection(
          in.box, in.sphere, in.args[0], in.args[1], in.args[2], in.args[3],
          in.args[4], in.args[5], in.args[6], in.args[7], in.args[8],
          (in.above != 0.0f) ? 1u : 0u, out.point, &out.time);
      out.ret = static_cast<u32>(iRet);
    }

    void IntrBox3Sphere3FindEdgeRegionOursT(const void* p, void* q)
    {
      const RegionFlagIn& in = *static_cast<const RegionFlagIn*>(p);
      RegionOut& out = *static_cast<RegionOut*>(q);
      const int iRet = wm3collharness::IntrBox3Sphere3FindEdgeRegionIntersection(
          in.box, in.sphere, in.args[0], in.args[1], in.args[2], in.args[3],
          in.args[4], in.args[5], in.args[6], in.args[7], in.args[8],
          (in.above != 0.0f) ? 1u : 0u, out.point, &out.time);
      out.ret = static_cast<u32>(iRet);
    }

    void IntrBox3Sphere3FindJustEdgeRegionOursT(const void* p, void* q)
    {
      const RegionIn& in = *static_cast<const RegionIn*>(p);
      RegionOut& out = *static_cast<RegionOut*>(q);
      const int iRet = wm3collharness::IntrBox3Sphere3FindJustEdgeIntersection(
          in.box, in.sphere, in.args[0], in.args[1], in.args[2], in.args[3],
          in.args[4], in.args[5], in.args[6], in.args[7], in.args[8],
          out.point, &out.time);
      out.ret = static_cast<u32>(iRet);
    }

    void IntrBox3Sphere3FindVertexRegionOursT(const void* p, void* q)
    {
      const RegionIn& in = *static_cast<const RegionIn*>(p);
      RegionOut& out = *static_cast<RegionOut*>(q);
      const int iRet = wm3collharness::IntrBox3Sphere3FindVertexRegionIntersection(
          in.box, in.sphere, in.args[0], in.args[1], in.args[2], in.args[3],
          in.args[4], in.args[5], in.args[6], in.args[7], in.args[8],
          out.point, &out.time);
      out.ret = static_cast<u32>(iRet);
    }

    void ContAlignedBoxMaskOursT(const void* p, void* q)
    {
      const ContAlignedBoxMaskIn& in = *static_cast<const ContAlignedBoxMaskIn*>(p);
      Box15Out& out = *static_cast<Box15Out*>(q);
      const int iCount = static_cast<int>(in.count);
      const bool bMasked = in.mode != 0;
      const bool* mask = reinterpret_cast<const bool*>(in.mask);
      u32 words[15] = { 0 };
      wm3collharness::ContAlignedBoxMask(iCount, in.points,
          bMasked ? mask : nullptr, words);
      for (int i = 0; i < 15; ++i) {
        out.box[i] = FromBits(words[i]);
      }
    }

    void IntrLine3Box3DoClippingOursT(const void* p, void* q)
    {
      const DoClippingIn& in = *static_cast<const DoClippingIn*>(p);
      DoClippingOut& out = *static_cast<DoClippingOut*>(q);
      float points[6];
      for (int i = 0; i < 6; ++i) {
        points[i] = wm3collharness::UndefCoord();
      }
      const u32 hit = wm3collharness::IntrLine3Box3DoClipping(
          in.t0, in.t1, in.line, in.line + 3, in.box, (in.solid != 0.0f) ? 1u : 0u,
          &out.quantity, points, &out.type);
      out.hit = hit;
      std::memcpy(out.points, points, 6 * sizeof(float));
    }

    void IntrLine3Box3ClipOursT(const void* p, void* q)
    {
      const ClipIn& in = *static_cast<const ClipIn*>(p);
      ClipOut& out = *static_cast<ClipOut*>(q);
      float t0 = in.t0;
      float t1 = in.t1;
      const u32 ok = wm3collharness::IntrLine3Box3Clip(in.denom, in.numer, &t0, &t1);
      out.ret = ok;
      out.t0 = t0;
      out.t1 = t1;
    }

    void DistanceSetDifferenceStepOursT(const void* p, void* q)
    {
      const StepIn& in = *static_cast<const StepIn*>(p);
      StepOut& out = *static_cast<StepOut*>(q);
      u32 words[2] = { 0 };
      wm3collharness::DistanceSetDifferenceStep(in.step, words);
      out.step = FromBits(words[0]);
      out.invtwo = FromBits(words[1]);
    }

    void DistanceDifferenceStepDefaultOursT(const void* p, void* q)
    {
      (void)p;
      StepOut& out = *static_cast<StepOut*>(q);
      u32 words[2] = { 0 };
      wm3collharness::DistanceDifferenceStepDefault(words);
      out.step = FromBits(words[0]);
      out.invtwo = FromBits(words[1]);
    }

    void Vector3LengthOursT(const void* p, void* q)
    {
      const Vec3In& in = *static_cast<const Vec3In*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      out.ret = FromBits(wm3collharness::Vector3LengthOurs(in.v));
    }

    void Vector3CompareArraysOursT(const void* p, void* q)
    {
      const PairVec3In& in = *static_cast<const PairVec3In*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      out.ret = wm3collharness::Vector3CompareArraysOurs(in.a, in.b);
    }

    void Vector3OperatorEqualsOursT(const void* p, void* q)
    {
      const PairVec3In& in = *static_cast<const PairVec3In*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      out.ret = wm3collharness::Vector3OperatorEquals(in.a, in.b);
    }

    void QuaternionCompareArraysOursT(const void* p, void* q)
    {
      const PairQuatIn& in = *static_cast<const PairQuatIn*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      out.ret = wm3collharness::QuaternionCompareArrays(in.a, in.b);
    }

    void QuaternionOperatorEqualsOursT(const void* p, void* q)
    {
      const PairQuatIn& in = *static_cast<const PairQuatIn*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      out.ret = wm3collharness::QuaternionOperatorEquals(in.a, in.b);
    }

    void Sphere3CtorOursT(const void* p, void* q)
    {
      const Sphere3CtorIn& in = *static_cast<const Sphere3CtorIn*>(p);
      Obj4Out& out = *static_cast<Obj4Out*>(q);
      u32 words[4] = { 0 };
      wm3collharness::Sphere3CtorOurs(words, in.center, in.radius);
      for (int i = 0; i < 4; ++i) {
        out.obj[i] = FromBits(words[i]);
      }
    }

  } // namespace

  const wmdiff::Target kTargets[] = {
    { "Wm3::DistVector3Box3<float>::GetSquared()", 0x00A45C00,
      "Foundation/Distance/Wm3DistVector3Box3.cpp:45 (retail thiscall, result in ST(0))",
      static_cast<std::uint32_t>(sizeof(DistVector3Box3In)),
      static_cast<std::uint32_t>(sizeof(FloatOut)),
      GenDistVector3Box3T, DistVector3Box3GetSquaredRetailT, DistVector3Box3GetSquaredOursT },
    { "Wm3::DistVector3Segment3<float>::GetSquared()", 0x00A484F0,
      "Foundation/Distance/Wm3DistVector3Segment3.cpp:46 (retail thiscall, result in ST(0))",
      static_cast<std::uint32_t>(sizeof(DistVector3Segment3In)),
      static_cast<std::uint32_t>(sizeof(FloatOut)),
      GenDistVector3Segment3T, DistVector3Segment3GetSquaredRetailT, DistVector3Segment3GetSquaredOursT },
    { "Wm3::DistVector3Segment3<float>::Get()", 0x00A480E0,
      "Foundation/Distance/Wm3DistVector3Segment3.cpp:39 (retail thiscall, result in ST(0))",
      static_cast<std::uint32_t>(sizeof(DistVector3Segment3In)),
      static_cast<std::uint32_t>(sizeof(FloatOut)),
      GenDistVector3Segment3GetT, DistVector3Segment3GetRetailT, DistVector3Segment3GetOursT },
    { "Wm3::IntrBox2Circle2<float>::Test()", 0x00A4E620,
      "Foundation/Intersection/Wm3IntrBox2Circle2.h (retail thiscall, bool in EAX)",
      static_cast<std::uint32_t>(sizeof(IntrBox2Circle2In)),
      static_cast<std::uint32_t>(sizeof(BoolOut)),
      GenIntrBox2Circle2T, IntrBox2Circle2TestRetailT, IntrBox2Circle2TestOursT },
    { "Wm3::IntrLine3Box3<float>::Test()", 0x00A4FE60,
      "Foundation/Intersection/Wm3IntrLine3Box3.h (retail thiscall, bool in EAX; located via vtable slot +0x08)",
      static_cast<std::uint32_t>(sizeof(IntrLine3Box3In)),
      static_cast<std::uint32_t>(sizeof(BoolOut)),
      GenIntrLine3Box3T, IntrLine3Box3TestRetailT, IntrLine3Box3TestOursT },
    { "Wm3::IntrSegment3Box3<float>::Find()", 0x00A462A0,
      "Foundation/Intersection/Wm3IntrSegment3Box3.h (retail thiscall, bool in AL)",
      static_cast<std::uint32_t>(sizeof(IntrSegment3Box3In)),
      static_cast<std::uint32_t>(sizeof(BoolOut)),
      GenIntrSegment3Box3T, IntrSegment3Box3FindRetailT, IntrSegment3Box3FindOursT },
    { "Wm3::IntrBox3Sphere3<float>::Test()", 0x00A41560,
      "Foundation/Intersection/Wm3IntrBox3Sphere3.cpp:39 (retail thiscall, bool in AL)",
      static_cast<std::uint32_t>(sizeof(IntrBox3Sphere3In)),
      static_cast<std::uint32_t>(sizeof(BoolOut)),
      GenIntrBox3Sphere3T, IntrBox3Sphere3TestRetailT, IntrBox3Sphere3TestOursT },
    { "Wm3::IntrBox3Sphere3<float>::Find(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)", 0x00A43420,
      "Foundation/Intersection/Wm3IntrBox3Sphere3.cpp:121 (retail thiscall, RET 0xC, bool in AL)",
      static_cast<std::uint32_t>(sizeof(IntrBox3Sphere3FindIn)),
      static_cast<std::uint32_t>(sizeof(BoolOut)),
      GenIntrBox3Sphere3FindT, IntrBox3Sphere3FindRetailT, IntrBox3Sphere3FindOursT },
    { "Wm3::IntrBox3Sphere3<float>::GetContactPoint() const", 0x00A414C0,
      "Foundation/Intersection/Wm3IntrBox3Sphere3.h:42 (retail thiscall, returns &m_kContactPoint in EAX)"
      " -- the point is compared only on the IT_OTHER path, the only one where"
      " the algorithm defines it",
      static_cast<std::uint32_t>(sizeof(IntrBox3Sphere3FindIn)),
      static_cast<std::uint32_t>(sizeof(PointTimeOut)),
      GenIntrBox3Sphere3FindT, IntrBox3Sphere3GetContactPointRetailT, IntrBox3Sphere3GetContactPointOursT },
    { "Wm3::IntrSegment3Box3<float>::GetPoint(int) const", 0x00A46300,
      "Foundation/Intersection/Wm3IntrSegment3Box3.h (retail thiscall, returns &m_akPoint[i] in EAX)",
      static_cast<std::uint32_t>(sizeof(IntrSegment3Box3In)),
      static_cast<std::uint32_t>(sizeof(PointOut)),
      GenIntrSegment3Box3T, IntrSegment3Box3GetPointRetailT, IntrSegment3Box3GetPointOursT },
    { "Wm3::IntrSegment3Sphere3<float>::Find()", 0x00A471C0,
      "Foundation/Intersection/Wm3IntrSegment3Sphere3.cpp:66 (retail thiscall, bool in AL;"
      " direct CALL from Moho::CColPrimitive_Sphere::CollideLine at 0x004FEA27)",
      static_cast<std::uint32_t>(sizeof(IntrSegment3Sphere3In)),
      static_cast<std::uint32_t>(sizeof(BoolOut)),
      GenIntrSegment3Sphere3T, IntrSegment3Sphere3FindRetailT, IntrSegment3Sphere3FindOursT },
    { "Wm3::IntrSegment3Sphere3<float>::GetPoint(int) const", 0x00A46940,
      "Foundation/Intersection/Wm3IntrSegment3Sphere3.h (retail thiscall, returns &m_akPoint[i] in EAX)",
      static_cast<std::uint32_t>(sizeof(IntrSegment3Sphere3In)),
      static_cast<std::uint32_t>(sizeof(PointOut)),
      GenIntrSegment3Sphere3T, IntrSegment3Sphere3GetPointRetailT, IntrSegment3Sphere3GetPointOursT },
    { "Wm3::IntrBox2Box2<float>::Test()", 0x00A47D40,
      "Foundation/Intersection/Wm3IntrBox2Box2.h (retail thiscall, bool in AL;"
      " direct CALL from the AI steering test at 0x00596DC3)",
      static_cast<std::uint32_t>(sizeof(IntrBox2Box2In)),
      static_cast<std::uint32_t>(sizeof(BoolOut)),
      GenIntrBox2Box2T, IntrBox2Box2TestRetailT, IntrBox2Box2TestOursT },
    { "Wm3::DistVector2Box2<float>::GetSquared()", 0x00A6C9D0,
      "Foundation/Distance/Wm3DistVector2Box2.cpp:45 (retail thiscall, no stack"
      " arguments, result in ST(0); the class vtable's +0x18 slot)",
      static_cast<std::uint32_t>(sizeof(DistVector2Box2In)),
      static_cast<std::uint32_t>(sizeof(FloatOut)),
      GenDistVector2Box2T, DistVector2Box2GetSquaredRetailT, DistVector2Box2GetSquaredOursT },
    { "Wm3::DistVector2Box2<float>::Get()", 0x00A6C680,
      "Foundation/Distance/Wm3DistVector2Box2.cpp:39 (retail thiscall, no stack"
      " arguments, the float in ST(0); the class vtable's +0x0C slot, which"
      " IntrBox2Circle2::Test at 0x00A4E620 dispatches through)",
      static_cast<std::uint32_t>(sizeof(DistVector2Box2In)),
      static_cast<std::uint32_t>(sizeof(FloatOut)),
      GenDistVector2Box2T, DistVector2Box2GetRetailT, DistVector2Box2GetOursT },
    { "Wm3::InBox<float>(const Wm3::Vector3<float>&, const Wm3::Box3<float>&)", 0x00A3BC10,
      "Foundation/Containment/Wm3ContBox3.cpp:355 (retail free function, cdecl:"
      " the point and the box are two stack pointers, the caller removes them,"
      " the bool in AL)",
      static_cast<std::uint32_t>(sizeof(InBoxIn)),
      static_cast<std::uint32_t>(sizeof(BoolOut)),
      GenInBoxT, InBoxRetailT, InBoxOursT },
    { "Wm3::ContAlignedBox<float>(int, const Wm3::Vector3<float>*)", 0x00A3AE40,
      "Foundation/Containment/Wm3ContBox3.cpp:46 (retail free function returning"
      " a Box3 by value, so the hidden return pointer is the first stack"
      " argument; cdecl, the callee leaves its three arguments; the box is"
      " written as the centre, the three axes and the three extents)",
      static_cast<std::uint32_t>(sizeof(ContAlignedBoxIn)),
      static_cast<std::uint32_t>(sizeof(Box15Out)),
      GenContAlignedBoxT, ContAlignedBoxRetailT, ContAlignedBoxOursT },
    { "Wm3::IntrBox3Sphere3<float>::GetVertexIntersection(float, float, float, float, float, float, float)", 0x00A41740,
      "Foundation/Intersection/Wm3IntrBox3Sphere3.cpp:262 (retail cdecl, seven"
      " float arguments - fDx, fDy, fDz, fVx, fVy, fVz, fRSqr - the caller"
      " removes them, the result in ST(0); a private static of the WM3 class,"
      " called by the retail's Find at 0x00A43831)",
      static_cast<std::uint32_t>(sizeof(VertexIntersectionIn)),
      static_cast<std::uint32_t>(sizeof(FloatOut)),
      GenIntrBox3Sphere3GetVertexIntersectionT,
      IntrBox3Sphere3GetVertexIntersectionRetailT,
      IntrBox3Sphere3GetVertexIntersectionOursT },
    { "Wm3::IntrBox3Sphere3<float>::GetEdgeIntersection(float, float, float, float, float, float)", 0x00A41800,
      "Foundation/Intersection/Wm3IntrBox3Sphere3.cpp:306 (retail cdecl, six"
      " float arguments - fDx, fDz, fVx, fVz, fVSqr, fRSqr - the result in"
      " ST(0); a private static of the WM3 class, called by the retail's Find"
      " at 0x00A43868 and at 0x00A438D1)",
      static_cast<std::uint32_t>(sizeof(EdgeIntersectionIn)),
      static_cast<std::uint32_t>(sizeof(FloatOut)),
      GenIntrBox3Sphere3GetEdgeIntersectionT,
      IntrBox3Sphere3GetEdgeIntersectionRetailT,
      IntrBox3Sphere3GetEdgeIntersectionOursT },
    { "Wm3::IntrBox3Sphere3<float>::FindFaceRegionIntersection(float, float, float, float, float, float, float, float, float, float&, float&, float&, bool)", 0x00A41880,
      "Foundation/Intersection/Wm3IntrBox3Sphere3.cpp:325 (retail thiscall, a"
      " private member: ECX is `this`, thirteen stack arguments - fEx, fEy,"
      " fEz, fCx, fCy, fCz, fVx, fVy, fVz, &rfIx, &rfIy, &rfIz and the"
      " bAboveFace flag - RET 0x34, the callee removes them; the retail"
      " calls it from Find at 0x00A420EE and 0x00A421CC)",
      static_cast<std::uint32_t>(sizeof(RegionFlagIn)),
      static_cast<std::uint32_t>(sizeof(RegionOut)),
      GenIntrBox3Sphere3FindFaceRegionT,
      IntrBox3Sphere3FindFaceRegionRetailT, IntrBox3Sphere3FindFaceRegionOursT },
    { "Wm3::IntrBox3Sphere3<float>::FindJustEdgeIntersection(float, float, float, float, float, float, float, float, float, float&, float&, float&)", 0x00A41CC0,
      "Foundation/Intersection/Wm3IntrBox3Sphere3.cpp:464 (retail thiscall, a"
      " private member: ECX is `this`, twelve stack arguments - fCy, fEx,"
      " fEy, fEz, fDx, fDz, fVx, fVy, fVz, &rfIx, &rfIy, &rfIz - RET 0x30;"
      " the retail calls it from FindEdgeRegionIntersection at 0x00A4208A and"
      " 0x00A4216A)",
      static_cast<std::uint32_t>(sizeof(RegionIn)),
      static_cast<std::uint32_t>(sizeof(RegionOut)),
      GenIntrBox3Sphere3FindJustEdgeRegionT,
      IntrBox3Sphere3FindJustEdgeRegionRetailT,
      IntrBox3Sphere3FindJustEdgeRegionOursT },
    { "Wm3::IntrBox3Sphere3<float>::FindEdgeRegionIntersection(float, float, float, float, float, float, float, float, float, float&, float&, float&, bool)", 0x00A41F50,
      "Foundation/Intersection/Wm3IntrBox3Sphere3.cpp:532 (retail thiscall, a"
      " private member: ECX is `this`, thirteen stack arguments - fEx, fEy,"
      " fEz, fCx, fCy, fCz, fVx, fVy, fVz, &rfIx, &rfIy, &rfIz and the"
      " bAboveEdge flag - RET 0x34; the retail calls it from Find at"
      " 0x00A43C27 and 0x00A43FC7 and from FindVertexRegionIntersection at"
      " 0x00A424BA and 0x00A42581)",
      static_cast<std::uint32_t>(sizeof(RegionFlagIn)),
      static_cast<std::uint32_t>(sizeof(RegionOut)),
      GenIntrBox3Sphere3FindEdgeRegionT,
      IntrBox3Sphere3FindEdgeRegionRetailT, IntrBox3Sphere3FindEdgeRegionOursT },
    { "Wm3::IntrBox3Sphere3<float>::FindVertexRegionIntersection(float, float, float, float, float, float, float, float, float, float&, float&, float&)", 0x00A421E0,
      "Foundation/Intersection/Wm3IntrBox3Sphere3.cpp:620 (retail thiscall, a"
      " private member: ECX is `this`, twelve stack arguments - fEx, fEy,"
      " fEz, fCx, fCy, fCz, fVx, fVy, fVz, &rfIx, &rfIy, &rfIz - RET 0x30;"
      " the retail calls it from Find at 0x00A44016)",
      static_cast<std::uint32_t>(sizeof(RegionIn)),
      static_cast<std::uint32_t>(sizeof(RegionOut)),
      GenIntrBox3Sphere3FindVertexRegionT,
      IntrBox3Sphere3FindVertexRegionRetailT, IntrBox3Sphere3FindVertexRegionOursT },
    { "Wm3::ContAlignedBox<float>(int, const Wm3::Vector3<float>*, const bool*)", 0x00A3AE40,
      "Foundation/Containment/Wm3ContBox3.cpp:62 (retail free function returning"
      " a Box3 by value, so the hidden return pointer is the first stack"
      " argument; cdecl with FOUR arguments - the fourth is the abValid mask,"
      " which the retail tests a byte at a time; the mask is null for the"
      " unmasked path)",
      static_cast<std::uint32_t>(sizeof(ContAlignedBoxMaskIn)),
      static_cast<std::uint32_t>(sizeof(Box15Out)),
      GenContAlignedBoxMaskT, ContAlignedBoxMaskRetailT,
      ContAlignedBoxMaskOursT },
    { "Wm3::IntrLine3Box3<float>::DoClipping(float, float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&, const Wm3::Box3<float>&, bool, int&, Wm3::Vector3<float>*, int&)", 0x00A50220,
      "Foundation/Intersection/Wm3IntrLine3Box3.cpp:174 (retail static member,"
      " so there is no `this`: nine stack arguments - fT0, fT1, &origin,"
      " &direction, &box, bSolid, &riQuantity, akPoint, &riIntrType - cdecl, the"
      " caller removes the thirty-six bytes, the bool in AL. The retail's own"
      " Find at 0x00A508F0 calls it and pushes the literal 1 for bSolid)"
      " -- the two points are compared only where the helper defines them",
      static_cast<std::uint32_t>(sizeof(DoClippingIn)),
      static_cast<std::uint32_t>(sizeof(DoClippingOut)),
      GenIntrLine3Box3DoClippingT,
      IntrLine3Box3DoClippingRetailT, IntrLine3Box3DoClippingOursT },
    { "Wm3::IntrLine3Box3<float>::Clip(float, float, float&, float&)", 0x00A4FCC0,
      "Foundation/Intersection/Wm3IntrLine3Box3.cpp:303 (retail static member"
      " and a private one: four stack arguments - fDenom, fNumer, &rfT0,"
      " &rfT1 - cdecl, the caller removes the sixteen bytes, the bool in AL."
      " The retail's own DoClipping calls it at 0x00A5032D and 0x00A50366)",
      static_cast<std::uint32_t>(sizeof(ClipIn)),
      static_cast<std::uint32_t>(sizeof(ClipOut)),
      GenIntrLine3Box3ClipT,
      IntrLine3Box3ClipRetailT, IntrLine3Box3ClipOursT },
    { "Wm3::Distance<float, Wm3::Vector3<float>>::SetDifferenceStep(float)", 0x00A391D0,
      "Foundation/Distance/Wm3Distance.cpp:71 (retail thiscall, one float"
      " argument on the stack and the callee pops it - RET 4 - the step lands"
      " at [this+0x2C] and 0.5/step at [this+0x30]. The base class's own"
      " vftable is 0x00D71C54 - the base constructor at 0x00A39F20 installs"
      " it - and this body is a member of the base, so the row runs on an"
      " object the retail's own base constructor built)",
      static_cast<std::uint32_t>(sizeof(StepIn)),
      static_cast<std::uint32_t>(sizeof(StepOut)),
      GenDistanceSetDifferenceStepT,
      DistanceSetDifferenceStepRetailT, DistanceSetDifferenceStepOursT },
    { "Wm3::Distance<float, Wm3::Vector3<float>>::Distance()", 0x00A39F20,
      "Foundation/Distance/Wm3Distance.cpp:22 (retail thiscall, no stack"
      " arguments: the base constructor, which inlines the defaults - the step"
      " 1e-3 at [this+0x2C] and its reciprocal 500.0f at [this+0x30] - instead"
      " of reaching them through the setter. The rebuild's constructor calls"
      " the setter, so this row is what says the two agree there)",
      static_cast<std::uint32_t>(sizeof(StepIn)),
      static_cast<std::uint32_t>(sizeof(StepOut)),
      GenDistanceSetDifferenceStepT,
      DistanceDifferenceStepDefaultRetailT, DistanceDifferenceStepDefaultOursT },
    { "Wm3::Vector3<float>::Length() const", 0x00452A90,
      "Foundation/Math/Wm3Vector3.inl:259 (retail thiscall, ECX = `this`,"
      " no stack arguments, a plain RET, and the length comes back in ST(0)."
      " The body computes the three squares with SSE (MOVSS/MULSS) and"
      " calls the CRT's square root at 0x00452FC0, so the sum behaves as"
      " binary32 in the denormal range - where this body and the x87"
      " SquaredLength body at 0x004757E0 differ. The engine calls the"
      " instance method (CAiSteeringImpl.cpp:198 and :1458), so the row"
      " pins the value the engine's own inlined copy depends on)",
      static_cast<std::uint32_t>(sizeof(Vec3In)),
      static_cast<std::uint32_t>(sizeof(FloatOut)),
      GenVec3T, Vector3LengthRetailT,
      Vector3LengthOursT },
    { "Wm3::Vector3<float>::CompareArrays(const Wm3::Vector3<float>&) const",
      0x004F0A50,
      "Foundation/Math/Wm3Vector3.inl:103 (retail thiscall, ECX = `this`,"
      " one stack argument - the address of the other vector - and `RET 4`,"
      " so the callee pops it; the result is an int in EAX. The body is the"
      " compiler's inlined memcmp over the twelve bytes of the two vectors,"
      " and the inlined memcmp answers -1, 0 or +1. MSVC's memcmp answers"
      " the same three values - measured with this toolchain, and at /O2 it"
      " is the same inlined shape - so what this row settles is not the"
      " normalisation but what precedes it: the comparison is bitwise over"
      " the lanes in the object's little-endian order, so the x lane is"
      " compared first. The retail's own engine calls the member - named at"
      " `src/sdk/moho/entity/intel/CIntel.cpp:62` and"
      " `src/sdk/moho/render/camera/VTransform.cpp:178`, which both give"
      " the address 0x004F0A50 - and in the WM3.8 retail the member is"
      " public, so the rebuild's is public too; the body is unchanged. The"
      " WM3.4 copy has it private and spells it `memcmp(...)`; both answer"
      " -1/0/+1, so a component-wise comparison is what this row would"
      " catch, and it does - the mutation fails 72% of the draws)",
      static_cast<std::uint32_t>(sizeof(PairVec3In)),
      static_cast<std::uint32_t>(sizeof(BoolOut)),
      GenVec3CompareT,
      Vector3CompareArraysRetailT, Vector3CompareArraysOursT },
    { "Wm3::Vector3<float>::operator==(const Wm3::Vector3<float>&) const",
      0x004F08B0,
      "Foundation/Math/Wm3Vector3.inl:109 (retail thiscall, ECX = `this`, the"
      " other vector's address on the stack - the body loads it with `mov"
      " eax,[esp+4]` - and `RET 4`, so the callee pops it. The body pushes that"
      " address for the memcmp at 0x004F0A50 and folds the result into a bool"
      " (`neg eax; sbb eax,eax; add eax,1`), so EAX comes back 0 or 1. The body"
      " has no caller in the image - the compiler inlined the member at every"
      " use site - so this row compares a body nothing calls, the way the"
      " `Vector3<float>::Length` row does. The comparison is bitwise over the"
      " twelve bytes, so the generator's cases are the ones a component-wise"
      " `==` would get wrong: the same bits with a NaN in a lane, +0.0f against"
      " -0.0f, and one bit of one lane changed)",
      static_cast<std::uint32_t>(sizeof(PairVec3In)),
      static_cast<std::uint32_t>(sizeof(BoolOut)),
      GenVec3EqualsT,
      Vector3OperatorEqualsRetailT, Vector3OperatorEqualsOursT },
    { "Wm3::Quaternion<float>::CompareArrays(const Wm3::Quaternion<float>&) const",
      0x004F0B40,
      "Foundation/Math/Wm3Quaternion.inl:143 (the retail's body takes **ECX ="
      " `this` and EAX = the other quaternion** - it reads `mov esi,[ecx]` and"
      " `cmp esi,[eax]` and never touches the stack - and ends in a plain RET,"
      " so a thunk that pushed the argument would change the stack the body"
      " returns to. The body is the compiler's out-of-line memcmp over the"
      " sixteen bytes and answers -1, 0 or +1, so it says which byte differed"
      " first and not merely that one did. It has 24 callers in the image, all"
      " of them the compiler's inlined uses)",
      static_cast<std::uint32_t>(sizeof(PairQuatIn)),
      static_cast<std::uint32_t>(sizeof(BoolOut)),
      GenQuatEqualsT,
      QuaternionCompareArraysRetailT, QuaternionCompareArraysOursT },
    { "Wm3::Quaternion<float>::operator==(const Wm3::Quaternion<float>&) const",
      0x004F08E0,
      "Foundation/Math/Wm3Quaternion.inl:150 (the same register convention as"
      " CompareArrays - ECX = `this`, EAX = the other quaternion, a plain RET -"
      " and the body starts with a bare `call 0x004F0B40`, so it assumes both"
      " registers are already set. The result is that memcmp result folded"
      " into a bool (`neg eax; sbb eax,eax; add eax,1`). The body has no"
      " caller in the image: the compiler inlined the member at every use site)",
      static_cast<std::uint32_t>(sizeof(PairQuatIn)),
      static_cast<std::uint32_t>(sizeof(BoolOut)),
      GenQuatEqualsT,
      QuaternionOperatorEqualsRetailT, QuaternionOperatorEqualsOursT },
    { "Wm3::Sphere3<float>::Sphere3(const Wm3::Vector3<float>&, float)",
      0x004740C0,
      "Foundation/Math/Wm3Sphere3.inl:23 (the retail's body takes **ECX as the"
      " hidden return pointer** - the object the constructor writes - the centre's"
      " address at [esp+4] and the radius at [esp+8], and ends in `RET 8`, so"
      " the callee pops the two arguments. It writes all sixteen bytes of the"
      " object: the three centre lanes through the x87 (so a quiet NaN keeps"
      " its payload) at +0, +4 and +8, and the radius with a plain `movss` at"
      " +0xC. The object is a Vector3 and a float - no padding, no references,"
      " and no byte either side leaves unwritten - which makes this the one"
      " constructor in the library whose compared bytes are all values. The"
      " generator uses quiet NaNs only: a signalling one would be quieted by"
      " the retail's x87 and copied unchanged by the rebuild, which is the"
      " harness's known divergence class for pure copies)",
      static_cast<std::uint32_t>(sizeof(Sphere3CtorIn)),
      static_cast<std::uint32_t>(sizeof(Obj4Out)),
      GenSphere3CtorT,
      Sphere3CtorRetailT, Sphere3CtorOursT },
  };

  const wmdiff::Target* Targets(int& count)
  {
    count = static_cast<int>(sizeof(kTargets) / sizeof(kTargets[0]));
    return kTargets;
  }

} // namespace wm3coll
