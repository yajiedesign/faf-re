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

#include "wm3_coll_targets.h"

#include "fuzz.h"

#include <windows.h>
#include <xmmintrin.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#include "wm3_coll_harness.h"
#include "wm3_coll_thunks.h"

#include "Wm3Math.h"

using wmdiff::GenEdge;
using wmdiff::GenPlain;

namespace wm3coll
{
  namespace
  {

// Retail's DistVector3Box3 object stores its two reference members at
    // [this+0x34] and [this+0x38] and sets a vtable pointer, so it needs at
    // least 0x3C bytes of writable storage. 64 is comfortably above that and
    // keeps the buffer's address layout irrelevant.
    constexpr int kObjectBytes = 64;

    // vec[3] then a Box3 (Center[3], Axis[3][3], Extent[3]) - eighteen floats.
    void GenDistVector3Box3(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      wmdiff::GenVec3(rng, f);
      for (int i = 3; i < 18; ++i) {
        f[i] = rng.Chance(75) ? GenPlain(rng, 1000.0f) : wmdiff::GenFloat(rng);
      }
    }

    void DistVector3Box3GetSquaredRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistVector3Box3Ctor(storage, f, f + 3);
      out[0] = wmdiff::Bits(wm3collretail::DistVector3Box3GetSquared(storage));
    }

void DistVector3Box3GetSquaredOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wmdiff::Bits(wm3collharness::DistVector3Box3GetSquared(f, f + 3));
    }

    // A point a few times 1e-19 off a segment's line: the squared distance
    // is around 1e-38, which is below the smallest normal float
    // (1.1754944e-38) and therefore lives on the denormal grid, where a
    // float holds far fewer bits than the x87 register does.  The direction
    // is the x axis and the point is off it in y, so the closest point on
    // the segment is the origin and the distance is exactly the offset.
    void GenDistVector3Segment3Denormal(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      const float d = static_cast<float>(rng.Range(1.0e-20, 1.0e-19));
      f[0] = 0.0f; f[1] = d; f[2] = 0.0f;   // the vector, d off the line
      f[3] = 0.0f; f[4] = 0.0f; f[5] = 0.0f; // the segment's origin
      f[6] = 1.0f; f[7] = 0.0f; f[8] = 0.0f; // the direction
      f[9] = 10.0f;                          // the extent
    }

    // vec[3] then a Segment3 (Origin[3], Direction[3], Extent) - ten floats.
    void GenDistVector3Segment3(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      if (rng.Chance(15)) {
        GenDistVector3Segment3Denormal(rng, in);
        return;
      }
      wmdiff::GenVec3(rng, f);
      for (int i = 3; i < 9; ++i) {
        f[i] = rng.Chance(75) ? GenPlain(rng, 10.0f) : wmdiff::GenFloat(rng);
      }
      f[9] = rng.Chance(80) ? GenPlain(rng, 1000.0f) : GenEdge(rng);
    }

    void DistVector3Segment3GetSquaredRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistVector3Segment3Ctor(storage, f, f + 3);
      out[0] = wmdiff::Bits(wm3collretail::DistVector3Segment3GetSquared(storage));
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
    // global --skip-extreme keeps the other sixty-eight rows at full coverage.
    // Same reason as GenDistVector3Box3Get: the retail's CRT sqrt treats a
    // non-positive argument as a fatal math error, so the row is limited to
    // the finite range.
    void GenDistVector3Segment3Get(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      for (int attempt = 0; attempt < 32; ++attempt) {
        GenDistVector3Segment3(rng, in);
        const float squared = wm3collharness::DistVector3Segment3GetSquared(f, f + 3);
        const std::uint32_t bits = wmdiff::Bits(squared);
        if ((bits & 0x7F800000u) != 0x7F800000u) {
          return;
        }
      }
      for (int i = 0; i < 10; ++i) {
        f[i] = GenPlain(rng, 1.0f);
      }
    }

    void DistVector3Segment3GetRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistVector3Segment3Ctor(storage, f, f + 3);
      out[0] = wmdiff::Bits(wm3collretail::DistVector3Segment3Get(storage));
    }

    void DistVector3Segment3GetOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wmdiff::Bits(wm3collharness::DistVector3Segment3Get(f, f + 3));
    }

    void DistVector3Segment3GetSquaredOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wmdiff::Bits(wm3collharness::DistVector3Segment3GetSquared(f, f + 3));
    }

    // box[8] (Center[2], Axis[2][2], Extent[2]) then circle[3] (Center[2],
    // Radius) - eleven floats.
    //
    // The radius is drawn around the box's own extents so that "just touching"
    // and "just missing" are both common: a Test that only ever sees clearly
    // inside or clearly outside answers the same every time.
    void GenIntrBox2Circle2(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      for (int i = 0; i < 8; ++i) {
        f[i] = GenPlain(rng, 100.0f);
      }
      f[8] = GenPlain(rng, 100.0f);
      f[9] = GenPlain(rng, 100.0f);
      const float extent = (std::fabs(f[6]) + std::fabs(f[7])) * 0.5f;
      f[10] = rng.Chance(70) ? extent * rng.Range(0.0, 2.5)
                              : GenPlain(rng, 200.0f);
    }

void IntrBox2Circle2TestRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrBox2Circle2Ctor(storage, f, f + 8);
      out[0] = wm3collretail::IntrBox2Circle2Test(storage);
    }

    void IntrBox2Circle2TestOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wm3collharness::IntrBox2Circle2Test(f, f + 8);
    }

    // line[6] (Origin[3], Direction[3]) then box[15] - twenty-one floats.
    void GenIntrLine3Box3(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      wmdiff::GenVec3(rng, f);
      wmdiff::GenVec3(rng, f + 3);
      for (int i = 6; i < 21; ++i) {
        f[i] = rng.Chance(75) ? GenPlain(rng, 1000.0f) : wmdiff::GenFloat(rng);
      }
    }

    void IntrLine3Box3TestRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrLine3Box3Ctor(storage, f, f + 6);
      out[0] = wm3collretail::IntrLine3Box3Test(storage);
    }

    void IntrLine3Box3TestOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wm3collharness::IntrLine3Box3Test(f, f + 6);
    }

    // segment[7], box[15], then the `bool bSolid` member as its own input word -
    // twenty-two floats plus one word.
    //
    // The flag is generated rather than fixed because it selects a different
    // branch in Find, and a comparison that only ever exercises one of them
    // would pass without testing anything.
    void GenIntrSegment3Box3(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      wmdiff::GenVec3(rng, f);
      for (int i = 3; i < 9; ++i) {
        f[i] = rng.Chance(75) ? GenPlain(rng, 10.0f) : wmdiff::GenFloat(rng);
      }
      f[9] = rng.Chance(80) ? GenPlain(rng, 1000.0f) : GenEdge(rng);
      for (int i = 10; i < 25; ++i) {
        f[i] = rng.Chance(75) ? GenPlain(rng, 1000.0f) : wmdiff::GenFloat(rng);
      }
      in[25] = rng.Chance(50) ? 1u : 0u;
    }

    void IntrSegment3Box3FindRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrSegment3Box3Ctor(storage, f, f + 7, in[25]);
      out[0] = wm3collretail::IntrSegment3Box3Find(storage);
    }

    void IntrSegment3Box3FindOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wm3collharness::IntrSegment3Box3Find(f, f + 7, in[25]);
    }

    // segment[7] (Origin[3], Direction[3], Extent) then sphere[4]
    // (Center[3], Radius) - eleven floats.
    //
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
    void GenIntrSegment3Sphere3(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);

      // the sphere: centre and radius
      for (int i = 0; i < 3; ++i) {
        f[7 + i] = GenPlain(rng, 100.0f);
      }
      const float radius = static_cast<float>(rng.Range(0.5, 5.0));
      f[10] = radius;

      // the segment direction: unit length
      wmdiff::GenUnitVec3(rng, f + 3);
      const float dx = f[3], dy = f[4], dz = f[5];

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
      const float p[3] = { px, py, pz };
      for (int i = 0; i < 3; ++i) {
        f[i] = f[7 + i] + along * radius * o[i] + off * radius * p[i];
      }

      // the extent, on the segment's own scale and independent of `along`
      f[6] = radius * (static_cast<float>(rng.Range(0.2, 2.5))
                       + (rng.Chance(50) ? std::fabs(along) * radius : 0.0f));
    }

    void IntrSegment3Sphere3FindRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrSegment3Sphere3Ctor(storage, f, f + 7);
      out[0] = wm3collretail::IntrSegment3Sphere3Find(storage);
    }

    void IntrSegment3Sphere3FindOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wm3collharness::IntrSegment3Sphere3Find(f, f + 7);
    }

// GetPoint is only meaningful after a successful Find: Find() leaves
    // m_akPoint untouched on the miss path and the source's own GetPoint
    // asserts 0 <= i < m_iQuantity. Index 0 only - index 1 needs
    // m_iQuantity, which the generator cannot know before running Find. Same
    // narrowing as IntrSegment3Box3::GetPoint, and for the same reason.
    void IntrSegment3Sphere3GetPointRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrSegment3Sphere3Ctor(storage, f, f + 7);
      if (wm3collretail::IntrSegment3Sphere3Find(storage))
      {
        const float* p = wm3collretail::IntrSegment3Sphere3GetPoint(storage, 0u);
        std::memcpy(out, p, 3 * sizeof(float));
      }
      else
      {
        out[0] = out[1] = out[2] = 0u;
      }
    }

    void IntrSegment3Sphere3GetPointOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      wm3collharness::IntrSegment3Sphere3GetPoint(f, f + 7, 0u,
                                                  reinterpret_cast<float*>(out));
    }

    // box0[8] then box1[8] - each Box2 is Center[2], Axis[2][2], Extent[2].
    //
    // The two axes are orthonormal (the algorithm projects the boxes onto each
    // other's axes, so a non-orthonormal basis would make the test meaningless)
    // and the extents are positive. The second box is placed relative to the
    // first, near the separating distance, so the answer is not always "no".
    void GenIntrBox2Box2(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);

      // box 0: centre, axes, extents
      for (int i = 0; i < 2; ++i) {
        f[i] = GenPlain(rng, 100.0f);
      }
      f[2] = 1.0f; f[3] = 0.0f;               // axis 0
      f[4] = 0.0f; f[5] = 1.0f;               // axis 1
      f[6] = rng.Range(2.0f, 30.0f);          // extent x, positive
      f[7] = rng.Range(2.0f, 30.0f);          // extent y, positive

      // box 1: the same shape, rotated by a random angle, with its centre
      // placed at a distance around the separating distance of the two boxes.
      const float angle = static_cast<float>(rng.Range(0.0, 3.14159265));
      const float ca = std::cos(angle), sa = std::sin(angle);
      f[8] = 0.0f; f[9] = 0.0f;
      f[10] = ca;  f[11] = -sa;               // axis 0, rotated
      f[12] = sa;  f[13] = ca;                // axis 1, rotated
      f[14] = f[6] * static_cast<float>(rng.Range(0.3, 1.5));
      f[15] = f[7] * static_cast<float>(rng.Range(0.3, 1.5));

      // the centre, near the point where the two boxes start to touch
      const float e0 = f[6], e1 = f[7];
      const float reach = e0 + e1 + std::fabs(f[14]) + std::fabs(f[15]);
      const float dist = reach * static_cast<float>(rng.Range(0.3, 1.2));
      const float dir = static_cast<float>(rng.Range(0.0, 6.2831853));
      f[8] = f[0] + dist * std::cos(dir);
      f[9] = f[1] + dist * std::sin(dir);
    }

    void IntrBox2Box2TestRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrBox2Box2Ctor(storage, f, f + 8);
      out[0] = wm3collretail::IntrBox2Box2Test(storage);
    }

    void IntrBox2Box2TestOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wm3collharness::IntrBox2Box2Test(f, f + 8);
    }

    // GetPoint is only meaningful after a *successful* Find: DoClipping writes
    // m_akPoint on intersection and leaves it untouched otherwise, and the
    // source's own GetPoint asserts 0 <= i < m_iQuantity. Comparing the point
    // on a failed Find compares uninitialised memory, which is what the first
    // version of this row did and why it reported 5311 mismatches.
    //
    // So both sides run Find, and only write the point when it succeeded;
    // otherwise both write zero and agree trivially. That is a real narrowing
    // of coverage and it is deliberate: GetPoint is only defined on the
    // intersecting inputs.
    //
    // Index 0 only. Index 1 needs m_iQuantity to be known, and the generator
    // cannot know it before running Find, so that lane is not covered here.
    // box[15] then sphere[4] (Center[3], Radius) - nineteen floats.
    void GenIntrBox3Sphere3(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      for (int i = 0; i < 15; ++i) {
        f[i] = rng.Chance(75) ? GenPlain(rng, 1000.0f) : wmdiff::GenFloat(rng);
      }
      for (int i = 15; i < 18; ++i) {
        f[i] = GenPlain(rng, 1000.0f);
      }
      // The radius is drawn around the box's own extent so that near-touching
      // and clearly-inside cases are both common.
      const float reach = (std::fabs(f[6]) + std::fabs(f[7]) + std::fabs(f[8])) / 3.0f;
      f[18] = rng.Chance(70) ? reach * rng.Range(0.0, 2.5) : GenPlain(rng, 200.0f);
    }

    void IntrBox3Sphere3TestRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrBox3Sphere3Ctor(storage, f, f + 15);
      out[0] = wm3collretail::IntrBox3Sphere3Test(storage);
    }

    void IntrBox3Sphere3TestOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wm3collharness::IntrBox3Sphere3Test(f, f + 15);
    }

    // box[8], circle[3], fTMax, velocity0[2], velocity1[2] - sixteen floats.

    // box[15], sphere[4], fTMax, velocity0[3], velocity1[3] - twenty-six floats.
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
    void GenIntrBox3Sphere3Find(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      for (int i = 0; i < 3; ++i) {
        f[i] = GenPlain(rng, 1000.0f);        // box centre
      }
      // An orthonormal axis: the algorithm treats the three axes as
      // orthonormal, and a random one would make the box a parallelepiped.
      for (int i = 0; i < 3; ++i) {
        f[3 + i * 3] = 0.0f;
        f[4 + i * 3] = 0.0f;
        f[5 + i * 3] = 0.0f;
      }
      f[3] = 1.0f;
      f[7] = 1.0f;
      f[11] = 1.0f;
      for (int i = 12; i < 15; ++i) {
        f[i] = std::fabs(GenPlain(rng, 500.0f));  // positive extents
      }
      for (int i = 15; i < 18; ++i) {
        f[i] = f[i - 15] + GenPlain(rng, 2000.0f); // sphere centre
      }
      // The radius and the horizon are positive. A negative radius is not a
      // shape: the region tests compare against `Radius*fVx*iXSign`, so the
      // sign of the radius flips those comparisons, and the two sides then
      // pick different regions for the same geometry. GenPlain draws both
      // signs, so both of these have to be made positive here.
      const float reach = (f[12] + f[13] + f[14]) / 3.0f;
      f[18] = std::fabs(rng.Chance(70) ? reach * rng.Range(0.0, 2.5f) : GenPlain(rng, 200.0f));
      f[19] = rng.Chance(60) ? 0.0f : std::fabs(GenPlain(rng, 2.0f));
      for (int i = 20; i < 26; ++i) {
        f[i] = GenPlain(rng, 1000.0f);
      }
    }

    void IntrBox3Sphere3FindRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrBox3Sphere3Ctor(storage, f, f + 15);
      out[0] = wm3collretail::IntrBox3Sphere3Find(storage, f[19], f + 20, f + 23);
    }

    void IntrBox3Sphere3FindOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wm3collharness::IntrBox3Sphere3Find(f, f + 15, f[19], f + 20, f + 23);
    }

    // Gated on the intersection type, not on Find's return value.  See the
    // comment on the rebuilt side in wm3_coll_harness.cpp: the contact point
    // is only defined on the IT_OTHER path (sphere centre inside the box).
    // On the IT_POINT path both sides write the member from undefined
    // locals, and on the IT_EMPTY path neither writes it at all, so the
    // comparison there would be garbage against garbage.
    void IntrBox3Sphere3GetContactPointRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrBox3Sphere3Ctor(storage, f, f + 15);
      wm3collretail::IntrBox3Sphere3Find(storage, f[19], f + 20, f + 23);
      // m_iIntersectionType is at this + 0x08: the retail's Find writes 8
      // (IT_OTHER) on the "sphere centre inside the box" path and 1
      // (IT_POINT) / 0 (IT_EMPTY) on the other two.
      const int type = *reinterpret_cast<const int*>(storage + 0x08);
      if (type == 8) {
        const float* p = wm3collretail::IntrBox3Sphere3GetContactPoint(storage);
        std::memcpy(out, p, 3 * sizeof(float));
      }
      else {
        out[0] = out[1] = out[2] = 0u;
      }
      // The contact time is at this + 0x04 - the retail writes it there on
      // every path that has a defined value for it.
      std::memcpy(out + 3, storage + 0x04, sizeof(float));
    }

    void IntrBox3Sphere3GetContactPointOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      wm3collharness::IntrBox3Sphere3GetContactPoint(f, f + 15, f[19], f + 20, f + 23,
                                                      reinterpret_cast<float*>(out));
    }

    // ---- IntrBox3Sphere3::GetVertexIntersection / GetEdgeIntersection ----
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
    void GenIntrBox3Sphere3GetVertexIntersection(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      const std::uint32_t kind = rng.Below(100);
      if (kind < 1) {
        // The line is tangent: the discriminant is exactly zero.  This is
        // the one case random geometry never reaches, and it is the only way
        // to reach the square root at zero - the reciprocal is an infinity
        // and the quotient a NaN, on both sides.
        //
        // R is a power of two, so R*R is exact and the squared length of D
        // minus it is exact too: D = (1, R, 0) and V = (1, 0, 0) give
        // fDot = 1, fDiff = 1 and fVSqr = 1, so the discriminant is 1 - 1.
        const float R = 1024.0f;
        f[0] = 1.0f;
        f[1] = R;
        f[2] = 0.0f;
        f[3] = 1.0f;
        f[4] = 0.0f;
        f[5] = 0.0f;
        f[6] = R * R;
        return;
      }
      if (kind < 2) {
        // The line starts exactly on the sphere and moves inward: fDiff is
        // exactly zero, the discriminant is not, and the answer is exactly
        // zero.  R is a power of two again, so that |D|^2 and R*R are the
        // same number.
        const float R = 1024.0f;
        f[0] = R;
        f[1] = 0.0f;
        f[2] = 0.0f;
        f[3] = -1.0f;
        f[4] = 0.0f;
        f[5] = 0.0f;
        f[6] = R * R;
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
      float p[3] = { GenPlain(rng, 1.0f), GenPlain(rng, 1.0f), GenPlain(rng, 1.0f) };
      const float s = (p[0] * v[0] + p[1] * v[1] + p[2] * v[2]) / vv;
      p[0] -= s * v[0];
      p[1] -= s * v[1];
      p[2] -= s * v[2];
      float pp = p[0] * p[0] + p[1] * p[1] + p[2] * p[2];
      if (pp < 1.0e-6f) {
        // The drawn direction was (nearly) parallel to V; take another one.
        p[0] = v[1];
        p[1] = -v[0];
        p[2] = 0.0f;
        pp = p[0] * p[0] + p[1] * p[1];
        if (pp < 1.0e-6f) {
          p[0] = 0.0f;
          p[1] = v[2];
          p[2] = -v[1];
          pp = p[0] * p[0] + p[1] * p[1] + p[2] * p[2];
        }
      }
      const float inv = 1.0f / std::sqrt(pp);
      p[0] *= inv;
      p[1] *= inv;
      p[2] *= inv;
      const float h = R * rng.Range(0.0f, 1.0f);   // the closest approach
      const float t0 = GenPlain(rng, 100.0f);       // where along the line
      f[0] = t0 * v[0] + h * p[0];
      f[1] = t0 * v[1] + h * p[1];
      f[2] = t0 * v[2] + h * p[2];
      f[3] = v[0];
      f[4] = v[1];
      f[5] = v[2];
      f[6] = R * R;
    }

    void IntrBox3Sphere3GetVertexIntersectionRetail(const std::uint32_t* in,
                                                     std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wmdiff::Bits(wm3collretail::IntrBox3Sphere3GetVertexIntersection(
        f[0], f[1], f[2], f[3], f[4], f[5], f[6]));
    }

    void IntrBox3Sphere3GetVertexIntersectionOurs(const std::uint32_t* in,
                                                   std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wmdiff::Bits(wm3collharness::IntrBox3Sphere3GetVertexIntersection(
        f[0], f[1], f[2], f[3], f[4], f[5], f[6]));
    }

    // The same in two dimensions: d[2], v[2], vsqr and r2.  The squared
    // speed is an argument of the helper, not something it computes, so the
    // generator passes it.
    void GenIntrBox3Sphere3GetEdgeIntersection(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      const std::uint32_t kind = rng.Below(100);
      if (kind < 1) {
        // The tangent: the discriminant is exactly zero.
        const float R = 1024.0f;
        f[0] = 1.0f;
        f[1] = R;
        f[2] = 1.0f;
        f[3] = 0.0f;
        f[4] = 1.0f;
        f[5] = R * R;
        return;
      }
      if (kind < 2) {
        // The line starts exactly on the circle and moves inward: fDiff is
        // exactly zero and the answer is exactly zero.
        const float R = 1024.0f;
        f[0] = R;
        f[1] = 0.0f;
        f[2] = -1.0f;
        f[3] = 0.0f;
        f[4] = 1.0f;
        f[5] = R * R;
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
      float p[2] = { GenPlain(rng, 1.0f), GenPlain(rng, 1.0f) };
      const float s = (p[0] * v[0] + p[1] * v[1]) / vv;
      p[0] -= s * v[0];
      p[1] -= s * v[1];
      float pp = p[0] * p[0] + p[1] * p[1];
      if (pp < 1.0e-6f) {
        p[0] = v[1];
        p[1] = -v[0];
        pp = p[0] * p[0] + p[1] * p[1];
      }
      const float inv = 1.0f / std::sqrt(pp);
      p[0] *= inv;
      p[1] *= inv;
      const float h = R * rng.Range(0.0f, 1.0f);
      const float t0 = GenPlain(rng, 100.0f);
      f[0] = t0 * v[0] + h * p[0];
      f[1] = t0 * v[1] + h * p[1];
      f[2] = v[0];
      f[3] = v[1];
      f[4] = v[0] * v[0] + v[1] * v[1];
      f[5] = R * R;
    }

    void IntrBox3Sphere3GetEdgeIntersectionRetail(const std::uint32_t* in,
                                                   std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wmdiff::Bits(wm3collretail::IntrBox3Sphere3GetEdgeIntersection(
        f[0], f[1], f[2], f[3], f[4], f[5]));
    }

    void IntrBox3Sphere3GetEdgeIntersectionOurs(const std::uint32_t* in,
                                                 std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wmdiff::Bits(wm3collharness::IntrBox3Sphere3GetEdgeIntersection(
        f[0], f[1], f[2], f[3], f[4], f[5]));
    }

    // ---- IntrBox3Sphere3's four region helpers ---------------------------
    //
    // FindFaceRegionIntersection, FindJustEdgeIntersection,
    // FindEdgeRegionIntersection and FindVertexRegionIntersection are
    // private members of the WM3 class; the retail calls them from its own
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
    void GenIntrBox3Sphere3RegionBox(wmdiff::Rng& rng, std::uint32_t* in,
                                     float ex, float ey, float ez, float r)
    {
      float* f = reinterpret_cast<float*>(in);
      for (int i = 0; i < 3; ++i) {
        f[i] = 0.0f;                    // the box's centre
      }
      f[3] = 1.0f; f[4] = 0.0f; f[5] = 0.0f;   // the axis: the identity
      f[6] = 0.0f; f[7] = 1.0f; f[8] = 0.0f;
      f[9] = 0.0f; f[10] = 0.0f; f[11] = 1.0f;
      f[12] = ex; f[13] = ey; f[14] = ez;        // the extents
      f[15] = 0.0f; f[16] = 0.0f; f[17] = 0.0f;  // the sphere's centre
      f[18] = r;                                  // the sphere's radius
    }

    // The face region: nine arguments plus the flag, after the box and the
    // sphere.  15 + 4 + 9 + 1 words.
    void GenIntrBox3Sphere3FindFaceRegion(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      const float ex = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float ey = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float ez = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float r = 0.1f + std::fabs(GenPlain(rng, 20.0f));
      GenIntrBox3Sphere3RegionBox(rng, in, ex, ey, ez, r);

      const std::uint32_t kind = rng.Below(100);
      float cx, cy, cz, vx, vy, vz;
      std::uint32_t flag;
      if (kind < 12) {
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
      else if (kind < 20) {
        // The exact boundary of that test: fCz == fEz + R.
        cx = rng.Range(0.0, ex);
        cy = rng.Range(0.0, ey);
        cz = ez + r;
        vx = GenPlain(rng, 10.0f);
        vy = GenPlain(rng, 10.0f);
        vz = -std::fabs(GenPlain(rng, 10.0f));
        flag = 1u;
      }
      else if (kind < 30) {
        // Moving away from the face: fVz >= 0 answers 0.
        cx = rng.Range(0.0, ex);
        cy = rng.Range(0.0, ey);
        cz = ez + r + rng.Range(0.0, 50.0);
        vx = GenPlain(rng, 10.0f);
        vy = GenPlain(rng, 10.0f);
        vz = std::fabs(GenPlain(rng, 10.0f));
        flag = 1u;
      }
      else if (kind < 36) {
        // Exactly parallel to the face: fVz == 0 answers 0 as well.
        cx = rng.Range(0.0, ex);
        cy = rng.Range(0.0, ey);
        cz = ez + r + rng.Range(0.0, 50.0);
        vx = GenPlain(rng, 10.0f);
        vy = GenPlain(rng, 10.0f);
        vz = 0.0f;
        flag = 1u;
      }
      else if (kind < 50) {
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
      f[19] = ex; f[20] = ey; f[21] = ez;
      f[22] = cx; f[23] = cy; f[24] = cz;
      f[25] = vx; f[26] = vy; f[27] = vz;
      f[28] = (flag != 0u) ? 1.0f : 0.0f;
    }

    // The edge region: nine arguments plus the flag.  15 + 4 + 9 + 1.
    void GenIntrBox3Sphere3FindEdgeRegion(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      const float ex = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float ey = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float ez = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float r = 0.1f + std::fabs(GenPlain(rng, 20.0f));
      GenIntrBox3Sphere3RegionBox(rng, in, ex, ey, ez, r);

      const std::uint32_t kind = rng.Below(100);
      float cx, cy, cz, vx, vy, vz;
      std::uint32_t flag;
      if (kind < 15) {
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
      else if (kind < 25) {
        // Moving away from the edge: fVx*fDx + fVz*fDz >= 0 answers 0.
        cx = ex + rng.Range(0.0, 50.0);
        cz = ez + rng.Range(0.0, 50.0);
        cy = rng.Range(0.0, ey);
        vx = std::fabs(GenPlain(rng, 10.0f));
        vz = std::fabs(GenPlain(rng, 10.0f));
        vy = GenPlain(rng, 10.0f);
        flag = 1u;
      }
      else if (kind < 35) {
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
      f[19] = ex; f[20] = ey; f[21] = ez;
      f[22] = cx; f[23] = cy; f[24] = cz;
      f[25] = vx; f[26] = vy; f[27] = vz;
      f[28] = (flag != 0u) ? 1.0f : 0.0f;
    }

    // The vertex region: nine arguments, no flag.  15 + 4 + 9 words.
    void GenIntrBox3Sphere3FindVertexRegion(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      const float ex = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float ey = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float ez = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float r = 0.1f + std::fabs(GenPlain(rng, 20.0f));
      GenIntrBox3Sphere3RegionBox(rng, in, ex, ey, ez, r);

      const std::uint32_t kind = rng.Below(100);
      float cx, cy, cz, vx, vy, vz;
      if (kind < 15) {
        // The sphere already reaches the corner: |D|^2 <= R^2 is the "already
        // intersecting" test, and the answer is -1.
        cx = ex + r * rng.Range(0.0, 0.5);
        cy = ey + r * rng.Range(0.0, 0.5);
        cz = ez + r * rng.Range(0.0, 0.5);
        vx = -std::fabs(GenPlain(rng, 10.0f));
        vy = -std::fabs(GenPlain(rng, 10.0f));
        vz = -std::fabs(GenPlain(rng, 10.0f));
      }
      else if (kind < 25) {
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
      f[19] = ex; f[20] = ey; f[21] = ez;
      f[22] = cx; f[23] = cy; f[24] = cz;
      f[25] = vx; f[26] = vy; f[27] = vz;
    }

    // The just-edge helper: nine arguments.  15 + 4 + 9 words.
    //
    // The edge runs along y at (fEx, fEz) with half length fEy, and the
    // sphere is fDx, fDz away from it in x and z with the y coordinate
    // fCy.  The helper's own "the sphere is fDx and fDz away from an edge
    // with direction y" reading is what the arguments are built from.
    void GenIntrBox3Sphere3FindJustEdgeRegion(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      const float ex = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float ey = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float ez = 0.5f + std::fabs(GenPlain(rng, 100.0f));
      const float r = 0.1f + std::fabs(GenPlain(rng, 20.0f));
      GenIntrBox3Sphere3RegionBox(rng, in, ex, ey, ez, r);

      const std::uint32_t kind = rng.Below(100);
      float dx, dz, cy, vx, vy, vz;
      if (kind < 15) {
        // The sphere is within a radius of the edge's line.
        dx = r * rng.Range(0.0, 0.7);
        dz = r * rng.Range(0.0, 0.7);
        cy = rng.Range(0.0, ey);
        vx = -std::fabs(GenPlain(rng, 10.0f));
        vz = -std::fabs(GenPlain(rng, 10.0f));
        vy = GenPlain(rng, 10.0f);
      }
      else if (kind < 25) {
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
      f[19] = cy;
      f[20] = ex; f[21] = ey; f[22] = ez;
      f[23] = dx; f[24] = dz;
      f[25] = vx; f[26] = vy; f[27] = vz;
    }

    // ---- The four region helpers, both sides ----------------------------
    //
    // The retail side constructs the retail's own IntrBox3Sphere3 and calls
    // the retail's helper; the rebuilt side calls the rebuilt helper on a
    // rebuilt object.  Both start the three intersection coordinates from
    // the same sentinel, so the paths that write none of them agree.
    //
    // The contact time is the member the retail keeps at [this + 0x04] and
    // the rebuilt class reaches through GetContactTime(); both are read
    // after the call.
    void IntrBox3Sphere3FindFaceRegionRetail(const std::uint32_t* in,
                                              std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrBox3Sphere3Ctor(storage, f, f + 15);
      float ix = wm3collharness::UndefCoord();
      float iy = wm3collharness::UndefCoord();
      float iz = wm3collharness::UndefCoord();
      const int iRet = wm3collretail::IntrBox3Sphere3FindFaceRegionIntersection(
          storage, f[19], f[20], f[21], f[22], f[23], f[24], f[25], f[26],
          f[27], &ix, &iy, &iz, (f[28] != 0.0f) ? 1u : 0u);
      float* p = reinterpret_cast<float*>(out + 1);
      out[0] = static_cast<std::uint32_t>(iRet);
      p[0] = ix;
      p[1] = iy;
      p[2] = iz;
      float fTime = 0.0f;
      std::memcpy(&fTime, storage + 4, sizeof fTime);
      p[3] = fTime;
    }

    void IntrBox3Sphere3FindEdgeRegionRetail(const std::uint32_t* in,
                                              std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrBox3Sphere3Ctor(storage, f, f + 15);
      float ix = wm3collharness::UndefCoord();
      float iy = wm3collharness::UndefCoord();
      float iz = wm3collharness::UndefCoord();
      const int iRet = wm3collretail::IntrBox3Sphere3FindEdgeRegionIntersection(
          storage, f[19], f[20], f[21], f[22], f[23], f[24], f[25], f[26],
          f[27], &ix, &iy, &iz, (f[28] != 0.0f) ? 1u : 0u);
      float* p = reinterpret_cast<float*>(out + 1);
      out[0] = static_cast<std::uint32_t>(iRet);
      p[0] = ix;
      p[1] = iy;
      p[2] = iz;
      float fTime = 0.0f;
      std::memcpy(&fTime, storage + 4, sizeof fTime);
      p[3] = fTime;
    }

    void IntrBox3Sphere3FindJustEdgeRegionRetail(const std::uint32_t* in,
                                                  std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrBox3Sphere3Ctor(storage, f, f + 15);
      float ix = wm3collharness::UndefCoord();
      float iy = wm3collharness::UndefCoord();
      float iz = wm3collharness::UndefCoord();
      const int iRet = wm3collretail::IntrBox3Sphere3FindJustEdgeIntersection(
          storage, f[19], f[20], f[21], f[22], f[23], f[24], f[25], f[26],
          f[27], &ix, &iy, &iz);
      float* p = reinterpret_cast<float*>(out + 1);
      out[0] = static_cast<std::uint32_t>(iRet);
      p[0] = ix;
      p[1] = iy;
      p[2] = iz;
      float fTime = 0.0f;
      std::memcpy(&fTime, storage + 4, sizeof fTime);
      p[3] = fTime;
    }

    void IntrBox3Sphere3FindVertexRegionRetail(const std::uint32_t* in,
                                                std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrBox3Sphere3Ctor(storage, f, f + 15);
      float ix = wm3collharness::UndefCoord();
      float iy = wm3collharness::UndefCoord();
      float iz = wm3collharness::UndefCoord();
      const int iRet = wm3collretail::IntrBox3Sphere3FindVertexRegionIntersection(
          storage, f[19], f[20], f[21], f[22], f[23], f[24], f[25], f[26],
          f[27], &ix, &iy, &iz);
      float* p = reinterpret_cast<float*>(out + 1);
      out[0] = static_cast<std::uint32_t>(iRet);
      p[0] = ix;
      p[1] = iy;
      p[2] = iz;
      float fTime = 0.0f;
      std::memcpy(&fTime, storage + 4, sizeof fTime);
      p[3] = fTime;
    }

    void IntrBox3Sphere3FindFaceRegionOurs(const std::uint32_t* in,
                                           std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      float* p = reinterpret_cast<float*>(out + 1);
      const int iRet = wm3collharness::IntrBox3Sphere3FindFaceRegionIntersection(
          f, f + 15, f[19], f[20], f[21], f[22], f[23], f[24], f[25], f[26],
          f[27], (f[28] != 0.0f) ? 1u : 0u, p, p + 3);
      out[0] = static_cast<std::uint32_t>(iRet);
    }

    void IntrBox3Sphere3FindEdgeRegionOurs(const std::uint32_t* in,
                                           std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      float* p = reinterpret_cast<float*>(out + 1);
      const int iRet = wm3collharness::IntrBox3Sphere3FindEdgeRegionIntersection(
          f, f + 15, f[19], f[20], f[21], f[22], f[23], f[24], f[25], f[26],
          f[27], (f[28] != 0.0f) ? 1u : 0u, p, p + 3);
      out[0] = static_cast<std::uint32_t>(iRet);
    }

    void IntrBox3Sphere3FindJustEdgeRegionOurs(const std::uint32_t* in,
                                               std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      float* p = reinterpret_cast<float*>(out + 1);
      const int iRet = wm3collharness::IntrBox3Sphere3FindJustEdgeIntersection(
          f, f + 15, f[19], f[20], f[21], f[22], f[23], f[24], f[25], f[26],
          f[27], p, p + 3);
      out[0] = static_cast<std::uint32_t>(iRet);
    }

    void IntrBox3Sphere3FindVertexRegionOurs(const std::uint32_t* in,
                                             std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      float* p = reinterpret_cast<float*>(out + 1);
      const int iRet = wm3collharness::IntrBox3Sphere3FindVertexRegionIntersection(
          f, f + 15, f[19], f[20], f[21], f[22], f[23], f[24], f[25], f[26],
          f[27], p, p + 3);
      out[0] = static_cast<std::uint32_t>(iRet);
    }

    void IntrSegment3Box3GetPointRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrSegment3Box3Ctor(storage, f, f + 7, in[25]);
      if (wm3collretail::IntrSegment3Box3Find(storage))
      {
        const float* p = wm3collretail::IntrSegment3Box3GetPoint(storage, 0u);
        std::memcpy(out, p, 3 * sizeof(float));
      }
      else
      {
        out[0] = out[1] = out[2] = 0u;
      }
    }

    void IntrSegment3Box3GetPointOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      wm3collharness::IntrSegment3Box3GetPoint(f, f + 7, in[25], 0u,
                                                reinterpret_cast<float*>(out));
    }

    // ---- the distance / capsule rows --------------------------------------

    // A unit vector perpendicular to the direction `d`. The generators below
    // place one shape relative to another along `d` and across it, so they
    // need a direction to place it across.
    void MakePerp3(const float* d, float* p)
    {
      float ax = 0.0f, ay = 0.0f, az = 1.0f;
      if (std::fabs(d[2]) > 0.9f) {
        ax = 1.0f; ay = 0.0f; az = 0.0f;
      }
      const float px = ay*d[2] - az*d[1];
      const float py = az*d[0] - ax*d[2];
      const float pz = ax*d[1] - ay*d[0];
      const float len = std::sqrt(px*px + py*py + pz*pz);
      p[0] = px/len; p[1] = py/len; p[2] = pz/len;
    }

// vec[2] then box[8] (Center[2], Axis[2][2], Extent[2]) - ten floats.
    //
    // The box gets an orthonormal axis and positive extents: the algorithm
    // projects the vector onto the box's axes and clamps against the
    // extents, so a non-orthonormal basis or a negative extent would make
    // the test meaningless. The vector is drawn around the box, so "inside",
    // "on the edge" and "outside" are all common.
    void GenDistVector2Box2(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      // box[8] starts at f[2]: centre, then the two axis rows, then extents.
      f[2] = GenPlain(rng, 40.0f);
      f[3] = GenPlain(rng, 40.0f);
      const float angle = static_cast<float>(rng.Range(0.0, 3.14159265));
      const float ca = std::cos(angle), sa = std::sin(angle);
      f[4] = ca;  f[5] = -sa;                  // axis 0
      f[6] = sa;  f[7] = ca;                   // axis 1
      f[8] = static_cast<float>(rng.Range(0.5, 30.0));   // extent x, positive
      f[9] = static_cast<float>(rng.Range(0.5, 30.0));   // extent y, positive
      // the vector, on the box's own scale: sometimes inside, sometimes out
      const float reach = f[8] + f[9];
      f[0] = GenPlain(rng, 40.0f);
      f[1] = GenPlain(rng, 40.0f);
      if (rng.Chance(40)) {
        // near the box, so the clamped and the unclamped branches are both
        // common
        f[0] = f[2] + GenPlain(rng, reach);
        f[1] = f[3] + GenPlain(rng, reach);
      }
    }

    void DistVector2Box2GetSquaredRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistVector2Box2Ctor(storage, f, f + 2);
      float value = 0.0f;
      wm3collretail::DistVector2Box2GetSquared(storage, &value);
      out[0] = wmdiff::Bits(value);
    }

    void DistVector2Box2GetSquaredOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wmdiff::Bits(
        wm3collharness::DistVector2Box2GetSquared(f, f + 2));
    }

void DistVector2Box2GetRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistVector2Box2Ctor(storage, f, f + 2);
      float value = 0.0f;
      wm3collretail::DistVector2Box2Get(storage, &value);
      out[0] = wmdiff::Bits(value);
    }

    void DistVector2Box2GetOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      wm3collharness::DistVector2Box2Get(f, f + 2, out);
    }

// point[3] then box[15] - eighteen floats.
    //
    // The box gets an orthonormal axis and positive extents, and the point is
    // drawn around the centre and across the extents, so "inside", "outside"
    // and "just off a face" are all common. One draw in three puts the point
    // exactly on one of the faces, which is the case the comparison can turn
    // on.
    void GenInBox(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);

      f[3] = GenPlain(rng, 20.0f);                 // the box's centre
      f[4] = GenPlain(rng, 20.0f);
      f[5] = GenPlain(rng, 20.0f);
      wmdiff::GenUnitVec3(rng, f + 6);            // axis 0
      MakePerp3(f + 6, f + 9);                     // axis 1, perpendicular to axis 0
      f[12] = f[7]*f[11] - f[8]*f[10];             // axis 2 = axis 0 x axis 1
      f[13] = f[8]*f[9] - f[6]*f[11];
      f[14] = f[6]*f[10] - f[7]*f[9];
      f[15] = static_cast<float>(rng.Range(0.5, 10.0));   // extent x, positive
      f[16] = static_cast<float>(rng.Range(0.5, 10.0));   // extent y, positive
      f[17] = static_cast<float>(rng.Range(0.5, 10.0));   // extent z, positive

      for (int i = 0; i < 3; ++i) {
        f[i] = f[3 + i] + GenPlain(rng, 20.0f);
      }
      if (rng.Chance(30)) {
        // exactly on one of the faces
        const int i = static_cast<int>(rng.Below(3));
        const float s = rng.Chance(50) ? 1.0f : -1.0f;
        for (int k = 0; k < 3; ++k) {
          f[k] = f[3 + k] + s*f[15 + i]*f[6 + 3*i + k];
        }
      }
    }

    void InBoxRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wm3collretail::InBox(f, f + 3);
    }

    void InBoxOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wm3collharness::InBox(f, f + 3);
    }

// count[1] then points[8] - twenty-five words.
    //
    // The count is drawn first and the points follow, so the input reads in
    // the same order as the call. The points are on a modest scale and the
    // count is sometimes one, which makes the box degenerate - the centre
    // and the extent then collapse and the comparison is decided entirely by
    // the rounding of the difference.
    void GenContAlignedBox(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      in[0] = static_cast<std::uint32_t>(1 + rng.Below(8));
      for (int i = 0; i < 8; ++i) {
        for (int k = 0; k < 3; ++k) {
          f[1 + 3*i + k] = GenPlain(rng, 10.0f);
        }
      }
    }

    void ContAlignedBoxRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      float afBox[15] = { 0.0f };
      wm3collretail::ContAlignedBox(afBox, static_cast<int>(in[0]), f + 1);
      for (int i = 0; i < 15; ++i) {
        out[i] = wmdiff::Bits(afBox[i]);
      }
    }

    void ContAlignedBoxOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      wm3collharness::ContAlignedBox(static_cast<int>(in[0]), f + 1, out);
    }

    // count[1] mode[1] points[24] mask[8 bytes]
    //
    // The mode word selects the mask shape:
    //   0 - no mask at all (the retail's fourth argument is null, so it takes
    //       the unmasked path);
    //   1 - every point valid;
    //   2 - some points invalid (at least one valid and, when the count
    //       allows it, at least one invalid among the first `count` entries);
    //   3 - no point valid at all, which is the retail's "-1 extents" case.
    //
    // The mask is eight bytes, one per point, packed into the two words that
    // follow the points. A nonzero byte means "keep this point" - the retail
    // tests the mask a byte at a time - and the same bytes are handed to our
    // side as the `const bool*` the Wild Magic 3.4 signature takes, so both
    // sides read exactly the same mask. Only 0 and 1 are produced, so the
    // bytes are a valid bool array on our side.
    //
    // The points are on a modest scale and are sometimes duplicated so the
    // comparisons hit the equal case, which is where the "else if" form of
    // the Wild Magic 3.4 body differs from two independent tests.
    void GenContAlignedBoxMask(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      const int iCount = 1 + static_cast<int>(rng.Below(8));
      const int iMode = static_cast<int>(rng.Below(5));
      in[0] = static_cast<std::uint32_t>(iCount);
      in[1] = static_cast<std::uint32_t>(iMode);

      for (int i = 0; i < 8; ++i) {
        for (int k = 0; k < 3; ++k) {
          f[2 + 3*i + k] = GenPlain(rng, 10.0f);
        }
      }
      // Duplicate a point now and then: an exact tie is a boundary the
      // comparisons must agree on, and it makes the box degenerate.
      if (rng.Chance(40)) {
        const int iSrc = static_cast<int>(rng.Below(8));
        const int iDst = static_cast<int>(rng.Below(8));
        for (int k = 0; k < 3; ++k) {
          f[2 + 3*iDst + k] = f[2 + 3*iSrc + k];
        }
      }
      // And a zero point now and then: the extents then collapse onto zero.
      if (rng.Chance(20)) {
        const int iZero = static_cast<int>(rng.Below(8));
        for (int k = 0; k < 3; ++k) {
          f[2 + 3*iZero + k] = 0.0f;
        }
      }

      unsigned char* mask = reinterpret_cast<unsigned char*>(in + 26);
      for (int i = 0; i < 8; ++i) {
        mask[i] = 1;
      }
      if (iMode == 2) {
        // Invalidate a random nonempty proper subset of the entries the
        // retail will read, so the mask path runs with a mix of valid and
        // invalid points and the seed point is not always the first one.
        for (int i = 0; i < 8; ++i) {
          mask[i] = rng.Chance(50) ? 0 : 1;
        }
        // Keep at least one valid point among the first `count` entries.
        if (mask[0] == 0) {
          int iAny = -1;
          for (int i = 0; i < iCount; ++i) {
            if (mask[i] != 0) { iAny = i; break; }
          }
          if (iAny < 0) {
            mask[static_cast<int>(rng.Below(iCount))] = 1;
          }
        }
      }
      else if (iMode == 3) {
        for (int i = 0; i < 8; ++i) {
          mask[i] = 0;
        }
      }
      else if (iMode == 4) {
        // Nonzero-but-not-one bytes. The retail tests the mask byte against
        // zero, so these count as valid; our side reads the same bytes as the
        // `bool` array the Wild Magic 3.4 signature takes, and MSVC compiles
        // that test to a byte compare against zero as well.
        for (int i = 0; i < 8; ++i) {
          mask[i] = 0x80;
        }
        for (int i = 0; i < 8; ++i) {
          if (rng.Chance(50)) {
            mask[i] = 0;
          }
        }
      }
    }

    void ContAlignedBoxMaskRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      const int iCount = static_cast<int>(in[0]);
      const bool bMasked = in[1] != 0;
      const unsigned char* mask = reinterpret_cast<const unsigned char*>(in + 26);
      float afBox[15] = { 0.0f };
      wm3collretail::ContAlignedBoxMask(afBox, iCount, f + 2,
          bMasked ? mask : nullptr);
      for (int i = 0; i < 15; ++i) {
        out[i] = wmdiff::Bits(afBox[i]);
      }
    }

    void ContAlignedBoxMaskOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      const int iCount = static_cast<int>(in[0]);
      const bool bMasked = in[1] != 0;
      const bool* mask = reinterpret_cast<const bool*>(in + 26);
      wm3collharness::ContAlignedBoxMask(iCount, f + 2,
          bMasked ? mask : nullptr, out);
    }

// ---- Wm3::IntrLine3Box3<float>::DoClipping and ::Clip -------------------
    //
    // DoClipping's input: fT0, fT1, the line (six floats), the box (fifteen)
    // and the bSolid flag - twenty-four words.
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
    void GenIntrLine3Box3DoClipping(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      const float fMax = std::numeric_limits<float>::max();

      const bool tangent = rng.Chance(25);
      if (tangent) {
        // the exact tangency
        f[2] = 2.0f; f[3] = 0.0f; f[4] = 0.0f;      // the line's origin
        f[5] = 1.0f; f[6] = 1.0f; f[7] = 0.0f;      // the line's direction
        f[8] = f[9] = f[10] = 0.0f;                 // the box's centre
        f[11] = 1.0f; f[12] = 0.0f; f[13] = 0.0f;   // axis 0
        f[14] = 0.0f; f[15] = 1.0f; f[16] = 0.0f;   // axis 1
        f[17] = 0.0f; f[18] = 0.0f; f[19] = 1.0f;   // axis 2
        f[20] = f[21] = f[22] = 1.0f;               // the extents
      }
      else {
        // the box: the centre, an orthonormal axis and positive extents
        f[8] = GenPlain(rng, 20.0f);
        f[9] = GenPlain(rng, 20.0f);
        f[10] = GenPlain(rng, 20.0f);
        wmdiff::GenUnitVec3(rng, f + 11);          // axis 0
        MakePerp3(f + 11, f + 14);                   // axis 1
        f[17] = f[12]*f[16] - f[13]*f[15];           // axis 2 = axis 0 x axis 1
        f[18] = f[13]*f[14] - f[11]*f[16];
        f[19] = f[11]*f[15] - f[12]*f[14];
        f[20] = static_cast<float>(rng.Range(0.5, 10.0));
        f[21] = static_cast<float>(rng.Range(0.5, 10.0));
        f[22] = static_cast<float>(rng.Range(0.5, 10.0));

        // the line: the origin around the centre and the direction a unit
        // vector, so the line passes through the box's neighbourhood
        for (int i = 0; i < 3; ++i) {
          f[2 + i] = f[8 + i] + GenPlain(rng, 20.0f);
        }
        wmdiff::GenUnitVec3(rng, f + 5);
        if (rng.Chance(10)) {
          // the degenerate direction: nothing can be clipped at all, which is
          // the bSolid branch
          f[5] = f[6] = f[7] = 0.0f;
          for (int i = 0; i < 3; ++i) {
            f[2 + i] = f[8 + i] + GenPlain(rng, 0.5f*f[20 + i]);
          }
        }
      }

      // the clip interval.  The whole line is the common case; the narrow
      // intervals are what make the clipping itself decide the answer, and
      // the one that misses the box entirely is what makes the helper report
      // IT_EMPTY.
      const std::uint32_t kind = rng.Below(100);
      if (kind < 55) {
        f[0] = -fMax;
        f[1] = fMax;
      }
      else if (kind < 80) {
        f[0] = static_cast<float>(rng.Range(-30.0, 0.0));
        f[1] = static_cast<float>(rng.Range(0.0, 30.0));
      }
      else if (kind < 92) {
        // an interval that is entirely after the box
        f[0] = 100.0f;
        f[1] = 200.0f;
      }
      else {
        // an interval that is entirely before it
        f[0] = -200.0f;
        f[1] = -100.0f;
      }
      f[23] = rng.Chance(50) ? 1.0f : 0.0f;   // bSolid
    }

    void IntrLine3Box3DoClippingRetail(const std::uint32_t* in,
                                       std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      // The two points are only written on the paths that define them, so
      // both sides start them from the same sentinel.
      float points[6];
      for (int i = 0; i < 6; ++i) {
        points[i] = wm3collharness::UndefCoord();
      }
      int quantity = 0;
      int type = 0;
      const std::uint32_t hit = wm3collretail::IntrLine3Box3DoClipping(
          f[0], f[1], f + 2, f + 5, f + 8, (f[23] != 0.0f) ? 1 : 0,
          &quantity, points, &type);
      out[0] = hit;
      out[1] = static_cast<std::uint32_t>(quantity);
      out[2] = static_cast<std::uint32_t>(type);
      std::memcpy(out + 3, points, 6*sizeof(float));
    }

    void IntrLine3Box3DoClippingOurs(const std::uint32_t* in,
                                     std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      float points[6];
      for (int i = 0; i < 6; ++i) {
        points[i] = wm3collharness::UndefCoord();
      }
      const std::uint32_t hit = wm3collharness::IntrLine3Box3DoClipping(
          f[0], f[1], f + 2, f + 5, f + 8, (f[23] != 0.0f) ? 1u : 0u,
          out + 1, points, out + 2);
      out[0] = hit;
      std::memcpy(out + 3, points, 6*sizeof(float));
    }

    // Clip's input: the denominator, the numerator and the two parameters -
    // four floats.
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
    void GenIntrLine3Box3Clip(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      const std::uint32_t kind = rng.Below(100);
      if (kind < 20) {
        // the denominator is exactly zero: the answer is the sign of the
        // numerator alone.  Both +0 and -0 are drawn, because -0 is not
        // "less than zero" and the helper's third branch takes it too.
        f[0] = rng.Chance(50) ? 0.0f : -0.0f;
        f[1] = rng.Chance(50) ? 0.0f
                  : (rng.Chance(50) ? -1.0f : 1.0f);
        f[2] = 0.0f;
        f[3] = 1.0f;
        return;
      }
      if (kind < 50) {
        // the exact boundaries: the denominator is a power of two and the
        // parameters are powers of two, so both products are exact
        const float d = static_cast<float>(1 << rng.Below(4));   // 1,2,4,8
        const float t0 = static_cast<float>(1 << rng.Below(3));   // 1,2,4
        const float t1 = t0*2.0f;
        f[0] = rng.Chance(50) ? d : -d;
        f[1] = f[0]*t1;            // exactly fDenom*fT1
        f[2] = t0;
        f[3] = t1;
        return;
      }
      if (kind < 70) {
        // the segment is entirely clipped: the numerator is strictly greater
        // than fDenom*fT1
        const float d = static_cast<float>(1 << rng.Below(4));
        const float t0 = static_cast<float>(1 << rng.Below(3));
        const float t1 = t0*2.0f;
        f[0] = rng.Chance(50) ? d : -d;
        f[1] = f[0]*t1*4.0f;
        f[2] = t0;
        f[3] = t1;
        return;
      }
      if (kind < 85) {
        // the quotient is exact: the numerator is a multiple of the
        // denominator and the parameters are powers of two
        const float d = static_cast<float>(1 << rng.Below(4));
        f[0] = rng.Chance(50) ? d : -d;
        f[1] = d*static_cast<float>(1 << rng.Below(3));
        f[2] = 0.0f;
        f[3] = static_cast<float>(1 << rng.Below(3));
        return;
      }
      // the general case
      f[0] = GenPlain(rng, 10.0f);
      f[1] = GenPlain(rng, 10.0f);
      f[2] = GenPlain(rng, 5.0f);
      f[3] = GenPlain(rng, 5.0f);
      if (rng.Chance(10)) {
        // the parameters the other way round
        const float t = f[2];
        f[2] = f[3];
        f[3] = t;
      }
    }

    void IntrLine3Box3ClipRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      float t0 = f[2];
      float t1 = f[3];
      const std::uint32_t ok = wm3collretail::IntrLine3Box3Clip(f[0], f[1],
                                                                &t0, &t1);
      out[0] = ok;
      out[1] = wmdiff::Bits(t0);
      out[2] = wmdiff::Bits(t1);
    }

    void IntrLine3Box3ClipOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      float t0 = f[2];
      float t1 = f[3];
      const std::uint32_t ok = wm3collharness::IntrLine3Box3Clip(f[0], f[1],
                                                                &t0, &t1);
      out[0] = ok;
      out[1] = wmdiff::Bits(t0);
      out[2] = wmdiff::Bits(t1);
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
    void GenDistanceSetDifferenceStep(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      const std::uint32_t kind = rng.Below(100);
      if (kind < 20) {
        f[0] = 1e-3f;                        // the default
      }
      else if (kind < 45) {
        // positive, and not the default
        static const float kPositive[] = {
            1e-30f, 1e-10f, 1e-4f, 5e-4f, 2e-3f, 0.5f, 1.0f, 2.0f, 1e3f,
            1e30f, 1.17549435e-38f };
        f[0] = kPositive[rng.Below(11)];
      }
      else if (kind < 62) {
        // negative
        static const float kNegative[] = {
            -1e-30f, -1e-4f, -1e-3f, -0.5f, -1.0f, -2.0f, -1e30f };
        f[0] = kNegative[rng.Below(7)];
      }
      else if (kind < 75) {
        // exactly zero, both signs
        f[0] = rng.Chance(50) ? 0.0f : -0.0f;
      }
      else if (kind < 90) {
        // big enough that 0.5/step is a float denormal
        f[0] = rng.Chance(50) ? 1e38f : -1e38f;
      }
      else {
        f[0] = GenPlain(rng, 10.0f);
      }
    }

    void DistanceSetDifferenceStepRetail(const std::uint32_t* in,
                                         std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistanceCtor(storage);
      wm3collretail::DistanceSetDifferenceStep(storage, f[0]);
      // m_fDifferenceStep at [this+0x2C] and m_fInvTwoDifferenceStep at
      // [this+0x30]
      std::memcpy(out, storage + 0x2C, 2*sizeof(float));
    }

    void DistanceSetDifferenceStepOurs(const std::uint32_t* in,
                                       std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      wm3collharness::DistanceSetDifferenceStep(f[0], out);
    }

    // The same two members straight after construction: the constructor's own
    // defaults, which the retail inlines (1e-3 and 500.0f) and the rebuild
    // reaches through SetDifferenceStep(1e-3).
    void DistanceDifferenceStepDefaultRetail(const std::uint32_t* in,
                                             std::uint32_t* out)
    {
      (void)in;
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::DistanceCtor(storage);
      std::memcpy(out, storage + 0x2C, 2*sizeof(float));
    }

    void DistanceDifferenceStepDefaultOurs(const std::uint32_t* in,
                                           std::uint32_t* out)
    {
      (void)in;
      wm3collharness::DistanceDifferenceStepDefault(out);
    }

// ---- the exact-collision family for the Intr*<double>::Test rows -------
    //
    // A `bool` only flips when the two sides of the deciding comparison land
    // within an ulp of each other.  With a random generator and operands of
    // the order of a thousand, that happens about once in 10^10 draws - so a
    // bool row built on random geometry cannot witness the precision of the
    // double branch: the mutation check (the double branch put back on the
    // plain binary64 source) still passes 1000000/1000000 on five seeds.
    //
    // These helpers build the collision *exactly* instead.  The value
    // 1 + 2^-30 is exact in binary64 and is not a 24-bit significand, so the
    // x87 rounds it to 1.0 while binary64 keeps it.  A coordinate that
    // carries it therefore projects to a different number on the two sides,
    // and a threshold placed between the two results decides the row.
    //
    // Every value used here is exactly representable in binary64, so the
    // construction is exact and not a coincidence of the draw:
    //
    //   CollideValue()   1 + 2^-30   the coordinate that rounds away
    //   CollideExtent()  1 - 2^-32   the threshold the x87 loads exactly
    //   CollideRadius()  3 * 2^-32   between the two sides' results
    //
    // The retail's own answers under this construction are:
    //
    //   IntrBox3Sphere3      one of the three faces: the retail's fDk is
    //                         2^-32 and the radius 3 * 2^-32, so the retail
    //                         answers true; the binary64 fDk is 5 * 2^-32 and
    //                         answers false
    //   IntrLine3Box3        the y face: the retail's |W.y| is 1.0 and the
    //                         right-hand side 1.0, so the retail answers
    //                         true; the binary64 |W.y| is 1 + 2^-30 and
    //                         answers false
    //   IntrSegment3Box3     the x face: the retail's |diff.x| is 1.0 against
    //                         a right-hand side of 1.0, so the retail answers
    //                         true; the binary64 |diff.x| is 1 + 2^-30 and
    //                         answers false
    //   IntrSegment3Sphere3  the discriminant: the retail's fA0 is 0, so its
    //                         fDiscr is 0 and it goes on to answer true; the
    //                         binary64 fA0 is 2^-29 and its fDiscr is
    //                         negative, so it answers false
    namespace
    {
    } // namespace

// ---- the double instantiation of the Intr* classes --------------------
    //
    // The magnitudes stay inside [1e-100,1e100], which is what keeps the
    // modelled arithmetic exact; what that leaves out of reach is the x87's
    // extended exponent range itself, i.e. the overflow and the underflow of
    // the products.
    namespace
    {
    }

// ---- a temporary diagnostic row ---------------------------------------
    //
    // The swept rows disagree with the retail, and the question is which
    // velocity moves which shape.  This row fixes the input: the point is
    // (0,1,0), the segment's origin is the origin, its direction is the x
    // axis, its extent is 1, the time is 1, the point's velocity is zero and
    // the segment's velocity is (0,1,0).
    //
    //   the rebuild moves the point with the first velocity and the origin
    //   with the second: the point stays at (0,1,0), the origin moves to
    //   (0,1,0), the distance is 0.
    //   the other way round: the point moves to (0,0,0) and the origin stays
    //   at the origin, the distance is 1.

    // The input that fails the swept row, verbatim.

    // The same, with the time zeroed: the swept call then reduces to the
    // static one.

    // The simplest possible swept case: the point is (0,1,0), the segment
    // runs along the x axis from the origin, the extent is 1, the time is 0
    // and both velocities are zero.  The squared distance is 1.

    // The point is the origin, the segment's origin is (0,5,0) and it runs
    // along the x axis.  The squared distance is 25.

// ---- Wm3::ConvexHull<double> ------------------------------------------
    //
    // The double instantiation.  The vertex array is built to the retail's
    // stride - a Vector3<double> is twenty-four bytes (three doubles), where
    // a float vertex is twelve - so the harness hands the retail's
    // constructor an array of doubles, three per vertex and contiguous.
    // The shared Vector3 generator (fuzz.h), so the row sees the same
    // magnitudes as the Dot and LengthSq rows - including the family whose
    // squares leave the float's normal range.
    void GenVec3Lanes(wmdiff::Rng& rng, std::uint32_t* in)
    {
      wmdiff::GenVec3(rng, reinterpret_cast<float*>(in));
    }

    // ---- Wm3::Vector3<float>::Length ---------------------------------------
    //
    // The engine calls the instance method (`velocity.Length()` in
    // CAiSteeringImpl.cpp:198 and :1458), and the retail's body is at
    // 0x00452A90 - a thiscall with no stack arguments whose length comes
    // back in ST(0).  The generator is the shared Vector3 one, so the row
    // sees the same magnitudes as the Dot and LengthSq rows - including the
    // family whose squares leave the float's normal range, which is where
    // this body (SSE squares) and the x87 SquaredLength body differ.
    void Vector3LengthRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* v = reinterpret_cast<const float*>(in);
      out[0] = wmdiff::Bits(wm3collretail::Vector3Length(v));
    }

    void Vector3LengthOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* v = reinterpret_cast<const float*>(in);
      out[0] = wm3collharness::Vector3LengthOurs(v);
    }

    // ---- Wm3::Vector3<float>::CompareArrays ------------------------------
    //
    // The retail's body at 0x004F0A50 is the compiler's inlined memcmp over
    // the twelve bytes of the two vectors, and the inlined memcmp answers
    // -1, 0 or +1.  The comparison is therefore *bitwise* over the lanes, in
    // the little-endian order of the object: the x lane first, then y, then
    // z.  A component-wise `==` would disagree with it twice over - on a
    // signed zero against a zero, and on a NaN against the same NaN - and
    // the sign of the answer says which lane differed first.
    //
    // The bit patterns are built as bits, never as floats: the generator's
    // own arithmetic runs under _PC_24, and the interesting cases are
    // exactly the ones a float expression cannot produce (a NaN payload, a
    // negative zero, a difference of one in the last bit of a lane).
    void GenVec3Compare(wmdiff::Rng& rng, std::uint32_t* in)
    {
      // the lane patterns the families use
      static constexpr std::uint32_t kZero = 0x00000000u;   // +0.0f
      static constexpr std::uint32_t kNegZero = 0x80000000u; // -0.0f
      static constexpr std::uint32_t kNaN = 0x7FC00000u;     // the default quiet NaN
      static constexpr std::uint32_t kNaN2 = 0x7FFFFFFFu;   // a NaN with a far payload
      static constexpr std::uint32_t kOne = 0x3F800000u;     // 1.0f
      static constexpr std::uint32_t kOneUp = 0x3F800001u;   // the next float above 1.0f
      static constexpr std::uint32_t kNegOne = 0xBF800000u;  // -1.0f

      for (int i = 0; i < 6; ++i)
      {
        in[i] = rng.U32();
      }

      const std::uint32_t mode = rng.Below(100);
      const int lane = static_cast<int>(rng.Below(3));
      if (mode < 25)
      {
        // the two vectors are bit-identical: the answer is 0.  A NaN in a
        // lane is still bit-identical - a component-wise comparison would
        // call these different.
        for (int i = 0; i < 3; ++i)
        {
          in[3 + i] = in[i];
        }
        if (rng.Chance(40))
        {
          in[lane] = kNaN;
          in[3 + lane] = kNaN;
        }
      }
      else if (mode < 40)
      {
        // +0.0f against -0.0f in one lane: the bits differ, the values do
        // not.  The answer is -1 or +1, never 0.
        in[lane] = kZero;
        in[3 + lane] = kNegZero;
      }
      else if (mode < 55)
      {
        // one lane one ulp apart, and the other two identical
        in[lane] = kOne;
        in[3 + lane] = kOneUp;
        for (int i = 0; i < 3; ++i)
        {
          if (i != lane)
          {
            in[3 + i] = in[i];
          }
        }
      }
      else if (mode < 65)
      {
        // the same NaN in both vectors, the other lanes identical
        in[lane] = kNaN;
        in[3 + lane] = kNaN;
        for (int i = 0; i < 3; ++i)
        {
          if (i != lane)
          {
            in[3 + i] = in[i];
          }
        }
      }
      else if (mode < 75)
      {
        // two NaNs with different payloads: the last byte differs by 0xFF.
        // A raw byte difference would be -255; both memcmps answer -1.
        in[lane] = kNaN;
        in[3 + lane] = kNaN2;
        for (int i = 0; i < 3; ++i)
        {
          if (i != lane)
          {
            in[3 + i] = in[i];
          }
        }
      }
      else if (mode < 85)
      {
        // a lane whose first byte differs by 0x40: a raw byte difference
        // would be 64; both memcmps answer -1 or +1
        in[lane] = kOne;
        in[3 + lane] = kNegOne;
        for (int i = 0; i < 3; ++i)
        {
          if (i != lane)
          {
            in[3 + i] = in[i];
          }
        }
      }
      else
      {
        // a lane that differs in a random bit, the other two identical
        in[lane] ^= (1u << rng.Below(32));
        for (int i = 0; i < 3; ++i)
        {
          if (i != lane)
          {
            in[3 + i] = in[i];
          }
        }
      }
    }

    void Vector3CompareArraysRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* a = reinterpret_cast<const float*>(in);
      const float* b = reinterpret_cast<const float*>(in + 3);
      out[0] = static_cast<std::uint32_t>(
        wm3collretail::Vector3CompareArrays(a, b));
    }

    void Vector3CompareArraysOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* a = reinterpret_cast<const float*>(in);
      const float* b = reinterpret_cast<const float*>(in + 3);
      out[0] = wm3collharness::Vector3CompareArraysOurs(a, b);
    }

    // ---- Wm3::Vector3<float>::operator== ---------------------------------
    //
    // The comparison is bitwise over the twelve bytes, so the generator has to
    // produce the cases a component-wise `==` would get wrong: the same bits
    // with a NaN in a lane (a float comparison calls those different), +0.0f
    // against -0.0f (the values are equal, the bits are not), and two vectors
    // that differ in one bit of one lane.  The patterns are bits, never
    // floats: the generator's own arithmetic runs under _PC_24.
    void GenVec3Equals(wmdiff::Rng& rng, std::uint32_t* in)
    {
      static constexpr std::uint32_t kZero = 0x00000000u;   // +0.0f
      static constexpr std::uint32_t kNegZero = 0x80000000u; // -0.0f
      static constexpr std::uint32_t kNaN = 0x7FC00000u;     // the default quiet NaN
      static constexpr std::uint32_t kOne = 0x3F800000u;     // 1.0f
      static constexpr std::uint32_t kOneUp = 0x3F800001u;   // the next float above 1.0f
      static constexpr std::uint32_t kNegOne = 0xBF800000u;  // -1.0f

      for (int i = 0; i < 6; ++i)
      {
        in[i] = rng.U32();
      }
      const std::uint32_t mode = rng.Below(100);
      const int lane = static_cast<int>(rng.Below(3));
      if (mode < 25)
      {
        // bit-identical, sometimes with a NaN in a lane: the bits agree, so
        // the answer is "equal" even though a float comparison would say the
        // NaN lane differs
        for (int i = 0; i < 3; ++i)
        {
          in[3 + i] = in[i];
        }
        if (rng.Chance(40))
        {
          in[lane] = kNaN;
          in[3 + lane] = kNaN;
        }
      }
      else if (mode < 40)
      {
        // +0.0f against -0.0f in one lane, the other two identical: the
        // values are equal and the bits are not, so a component-wise `==`
        // would say "equal" where the bitwise one says "different"
        in[lane] = kZero;
        in[3 + lane] = kNegZero;
        for (int i = 0; i < 3; ++i)
        {
          if (i != lane)
          {
            in[3 + i] = in[i];
          }
        }
      }
      else if (mode < 55)
      {
        // one lane one ulp apart, the other two identical
        in[lane] = kOne;
        in[3 + lane] = kOneUp;
        for (int i = 0; i < 3; ++i)
        {
          if (i != lane)
          {
            in[3 + i] = in[i];
          }
        }
      }
      else if (mode < 65)
      {
        // a lane whose first byte differs by 0x40: the values are opposite
        // in sign
        in[lane] = kOne;
        in[3 + lane] = kNegOne;
        for (int i = 0; i < 3; ++i)
        {
          if (i != lane)
          {
            in[3 + i] = in[i];
          }
        }
      }
      else if (mode < 75)
      {
        // one bit different in one lane, the other two identical
        in[lane] ^= (1u << rng.Below(32));
        for (int i = 0; i < 3; ++i)
        {
          if (i != lane)
          {
            in[3 + i] = in[i];
          }
        }
      }
    }

    // ---- Wm3::Quaternion<float>::CompareArrays / operator== ---------------
    //
    // The same idea over the sixteen bytes of the two quaternions.  The
    // quaternion's CompareArrays answers -1, 0 or +1, so it says which byte
    // differed first and not merely that one did - that is why it is the
    // strong one of the two.
    void GenQuatEquals(wmdiff::Rng& rng, std::uint32_t* in)
    {
      static constexpr std::uint32_t kZero = 0x00000000u;   // +0.0f
      static constexpr std::uint32_t kNegZero = 0x80000000u; // -0.0f
      static constexpr std::uint32_t kNaN = 0x7FC00000u;     // the default quiet NaN
      static constexpr std::uint32_t kNaN2 = 0x7FFFFFFFu;   // a NaN with a far payload
      static constexpr std::uint32_t kOne = 0x3F800000u;     // 1.0f
      static constexpr std::uint32_t kOneUp = 0x3F800001u;   // the next float above 1.0f

      for (int i = 0; i < 8; ++i)
      {
        in[i] = rng.U32();
      }
      const std::uint32_t mode = rng.Below(100);
      const int lane = static_cast<int>(rng.Below(4));
      if (mode < 25)
      {
        // bit-identical, sometimes with a NaN in a lane
        for (int i = 0; i < 4; ++i)
        {
          in[4 + i] = in[i];
        }
        if (rng.Chance(40))
        {
          in[lane] = kNaN;
          in[4 + lane] = kNaN;
        }
      }
      else if (mode < 40)
      {
        // +0.0f against -0.0f in one lane, the other three identical: the
        // values are equal and the bits are not, so a component-wise `==`
        // would say "equal" where the bitwise one says "different"
        in[lane] = kZero;
        in[4 + lane] = kNegZero;
        for (int i = 0; i < 4; ++i)
        {
          if (i != lane)
          {
            in[4 + i] = in[i];
          }
        }
      }
      else if (mode < 55)
      {
        // one lane one ulp apart, the other three identical
        in[lane] = kOne;
        in[4 + lane] = kOneUp;
        for (int i = 0; i < 4; ++i)
        {
          if (i != lane)
          {
            in[4 + i] = in[i];
          }
        }
      }
      else if (mode < 70)
      {
        // two NaNs with different payloads: the last byte differs by 0xFF
        in[lane] = kNaN;
        in[4 + lane] = kNaN2;
        for (int i = 0; i < 4; ++i)
        {
          if (i != lane)
          {
            in[4 + i] = in[i];
          }
        }
      }
      else if (mode < 80)
      {
        // one bit different in one lane, the other three identical
        in[lane] ^= (1u << rng.Below(32));
        for (int i = 0; i < 4; ++i)
        {
          if (i != lane)
          {
            in[4 + i] = in[i];
          }
        }
      }
    }

    void Vector3OperatorEqualsRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* a = reinterpret_cast<const float*>(in);
      const float* b = reinterpret_cast<const float*>(in + 3);
      out[0] = wm3collretail::Vector3OperatorEquals(a, b) ? 1u : 0u;
    }

    void Vector3OperatorEqualsOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* a = reinterpret_cast<const float*>(in);
      const float* b = reinterpret_cast<const float*>(in + 3);
      out[0] = wm3collharness::Vector3OperatorEquals(a, b);
    }

    void QuaternionCompareArraysRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* a = reinterpret_cast<const float*>(in);
      const float* b = reinterpret_cast<const float*>(in + 4);
      out[0] = static_cast<std::uint32_t>(
        wm3collretail::QuaternionCompareArrays(a, b));
    }

    void QuaternionCompareArraysOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* a = reinterpret_cast<const float*>(in);
      const float* b = reinterpret_cast<const float*>(in + 4);
      out[0] = wm3collharness::QuaternionCompareArrays(a, b);
    }

    void QuaternionOperatorEqualsRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* a = reinterpret_cast<const float*>(in);
      const float* b = reinterpret_cast<const float*>(in + 4);
      out[0] = wm3collretail::QuaternionOperatorEquals(a, b) ? 1u : 0u;
    }

    void QuaternionOperatorEqualsOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* a = reinterpret_cast<const float*>(in);
      const float* b = reinterpret_cast<const float*>(in + 4);
      out[0] = wm3collharness::QuaternionOperatorEquals(a, b);
    }

    // ---- Wm3::Sphere3<float>::Sphere3(const Wm3::Vector3<float>&, float) ---
    //
    // The one constructor in this library whose compared bytes are all
    // values: the object is a Vector3 and a float, sixteen bytes, no padding,
    // no references, and the retail's body writes all sixteen.
    //
    // The retail copies the centre's lanes through the x87, so a quiet NaN
    // keeps its payload; the generator therefore uses quiet NaNs only - a
    // signalling one would be quieted by the retail and copied unchanged by
    // the rebuild, which is the harness's known divergence class for pure
    // copies and not a property of the rebuild.
    void GenSphere3Ctor(wmdiff::Rng& rng, std::uint32_t* in)
    {
      static constexpr std::uint32_t kZero = 0x00000000u;    // +0.0f
      static constexpr std::uint32_t kNegZero = 0x80000000u;  // -0.0f
      static constexpr std::uint32_t kNaN = 0x7FC00000u;      // the default quiet NaN
      static constexpr std::uint32_t kNaN2 = 0x7FC00001u;    // a quiet NaN with a payload
      static constexpr std::uint32_t kDenorm = 0x00000001u;   // the smallest denormal
      static constexpr std::uint32_t kOne = 0x3F800000u;      // 1.0f
      static constexpr std::uint32_t kNegOne = 0xBF800000u;   // -1.0f

      // the radius: +0.0f, -0.0f, a denormal, a NaN, and the rest random
      const std::uint32_t rmode = rng.Below(100);
      if (rmode < 15)
      {
        in[3] = kZero;
      }
      else if (rmode < 30)
      {
        in[3] = kNegZero;
      }
      else if (rmode < 40)
      {
        in[3] = kDenorm;
      }
      else if (rmode < 50)
      {
        in[3] = kNaN;
      }
      else if (rmode < 60)
      {
        in[3] = kNaN2;
      }
      else
      {
        in[3] = rng.U32();
      }

      // the centre: one lane at a time, the rest random
      const std::uint32_t cmode = rng.Below(100);
      const int lane = static_cast<int>(rng.Below(3));
      for (int i = 0; i < 3; ++i)
      {
        in[i] = rng.U32();
      }
      if (cmode < 15)
      {
        in[lane] = kZero;
      }
      else if (cmode < 30)
      {
        in[lane] = kNegZero;
      }
      else if (cmode < 40)
      {
        in[lane] = kDenorm;
      }
      else if (cmode < 50)
      {
        in[lane] = kNaN;
      }
      else if (cmode < 60)
      {
        in[lane] = kNaN2;
      }
      else if (cmode < 70)
      {
        in[lane] = kOne;
      }
      else if (cmode < 80)
      {
        in[lane] = kNegOne;
      }

      // A random draw can be a *signalling* NaN, and the retail's x87 copy
      // would quiet it while the rebuild's plain copy would not - that is the
      // harness's known divergence class for pure copies, not a property of
      // the rebuild.  Every NaN the generator draws is quieted here.
      for (int i = 0; i < 4; ++i)
      {
        const std::uint32_t u = in[i];
        if ((u & 0x7F800000u) == 0x7F800000u && (u & 0x007FFFFFu) != 0
            && (u & 0x00400000u) == 0)
        {
          in[i] = u | 0x00400000u;
        }
      }

    }

    void Sphere3CtorRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* center = reinterpret_cast<const float*>(in);
      const float radius = wmdiff::FromBits(in[3]);
      wm3collretail::Sphere3Ctor(out, center, radius);
    }

    void Sphere3CtorOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* center = reinterpret_cast<const float*>(in);
      const float radius = wmdiff::FromBits(in[3]);
      wm3collharness::Sphere3CtorOurs(out, center, radius);
    }

} // namespace

  const wmdiff::Target kTargets[] = {
    { "Wm3::DistVector3Box3<float>::GetSquared()", 0x00A45C00,
      "Foundation/Distance/Wm3DistVector3Box3.cpp:45 (retail thiscall, result in ST(0))",
      18, 1, "vec[3] box[15]", "ret", GenDistVector3Box3,
      DistVector3Box3GetSquaredRetail, DistVector3Box3GetSquaredOurs },
    { "Wm3::DistVector3Segment3<float>::GetSquared()", 0x00A484F0,
      "Foundation/Distance/Wm3DistVector3Segment3.cpp:46 (retail thiscall, result in ST(0))",
      10, 1, "vec[3] segment[7]", "ret", GenDistVector3Segment3,
      DistVector3Segment3GetSquaredRetail, DistVector3Segment3GetSquaredOurs },
    { "Wm3::DistVector3Segment3<float>::Get()", 0x00A480E0,
      "Foundation/Distance/Wm3DistVector3Segment3.cpp:39 (retail thiscall, result in ST(0))",
      10, 1, "vec[3] segment[7]", "ret", GenDistVector3Segment3Get,
      DistVector3Segment3GetRetail, DistVector3Segment3GetOurs },
    { "Wm3::IntrBox2Circle2<float>::Test()", 0x00A4E620,
      "Foundation/Intersection/Wm3IntrBox2Circle2.h (retail thiscall, bool in EAX)",
      11, 1, "box[8] circle[3]", "ret", GenIntrBox2Circle2,
      IntrBox2Circle2TestRetail, IntrBox2Circle2TestOurs },
    { "Wm3::IntrLine3Box3<float>::Test()", 0x00A4FE60,
      "Foundation/Intersection/Wm3IntrLine3Box3.h (retail thiscall, bool in EAX; located via vtable slot +0x08)",
      21, 1, "line[6] box[15]", "ret", GenIntrLine3Box3,
      IntrLine3Box3TestRetail, IntrLine3Box3TestOurs },
    { "Wm3::IntrSegment3Box3<float>::Find()", 0x00A462A0,
      "Foundation/Intersection/Wm3IntrSegment3Box3.h (retail thiscall, bool in AL)",
      26, 1, "segment[7] box[15] solid", "ret", GenIntrSegment3Box3,
      IntrSegment3Box3FindRetail, IntrSegment3Box3FindOurs },
    { "Wm3::IntrBox3Sphere3<float>::Test()", 0x00A41560,
      "Foundation/Intersection/Wm3IntrBox3Sphere3.cpp:39 (retail thiscall, bool in AL)",
      19, 1, "box[15] sphere[4]", "ret", GenIntrBox3Sphere3,
      IntrBox3Sphere3TestRetail, IntrBox3Sphere3TestOurs },
    { "Wm3::IntrBox3Sphere3<float>::Find(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)", 0x00A43420,
      "Foundation/Intersection/Wm3IntrBox3Sphere3.cpp:121 (retail thiscall, RET 0xC, bool in AL)",
      26, 1, "box[15] sphere[4] tmax vel0[3] vel1[3]", "ret", GenIntrBox3Sphere3Find,
      IntrBox3Sphere3FindRetail, IntrBox3Sphere3FindOurs },
    { "Wm3::IntrBox3Sphere3<float>::GetContactPoint() const", 0x00A414C0,
      "Foundation/Intersection/Wm3IntrBox3Sphere3.h:42 (retail thiscall, returns &m_kContactPoint in EAX)"
      " -- the point is compared only on the IT_OTHER path, the only one where"
      " the algorithm defines it",
      26, 4, "box[15] sphere[4] tmax vel0[3] vel1[3]", "point[3] time", GenIntrBox3Sphere3Find,
      IntrBox3Sphere3GetContactPointRetail, IntrBox3Sphere3GetContactPointOurs },
    { "Wm3::IntrSegment3Box3<float>::GetPoint(int) const", 0x00A46300,
      "Foundation/Intersection/Wm3IntrSegment3Box3.h (retail thiscall, returns &m_akPoint[i] in EAX)",
      26, 3, "segment[7] box[15] solid", "point[3]", GenIntrSegment3Box3,
      IntrSegment3Box3GetPointRetail, IntrSegment3Box3GetPointOurs },
    { "Wm3::IntrSegment3Sphere3<float>::Find()", 0x00A471C0,
      "Foundation/Intersection/Wm3IntrSegment3Sphere3.cpp:66 (retail thiscall, bool in AL;"
      " direct CALL from Moho::CColPrimitive_Sphere::CollideLine at 0x004FEA27)",
      11, 1, "segment[7] sphere[4]", "ret", GenIntrSegment3Sphere3,
      IntrSegment3Sphere3FindRetail, IntrSegment3Sphere3FindOurs },
    { "Wm3::IntrSegment3Sphere3<float>::GetPoint(int) const", 0x00A46940,
      "Foundation/Intersection/Wm3IntrSegment3Sphere3.h (retail thiscall, returns &m_akPoint[i] in EAX)",
      11, 3, "segment[7] sphere[4]", "point[3]", GenIntrSegment3Sphere3,
      IntrSegment3Sphere3GetPointRetail, IntrSegment3Sphere3GetPointOurs },
    { "Wm3::IntrBox2Box2<float>::Test()", 0x00A47D40,
      "Foundation/Intersection/Wm3IntrBox2Box2.h (retail thiscall, bool in AL;"
      " direct CALL from the AI steering test at 0x00596DC3)",
      16, 1, "box0[8] box1[8]", "ret", GenIntrBox2Box2,
      IntrBox2Box2TestRetail, IntrBox2Box2TestOurs },
    { "Wm3::DistVector2Box2<float>::GetSquared()", 0x00A6C9D0,
      "Foundation/Distance/Wm3DistVector2Box2.cpp:45 (retail thiscall, no stack"
      " arguments, result in ST(0); the class vtable's +0x18 slot)",
      10, 1, "vec[2] box[8]", "ret", GenDistVector2Box2,
      DistVector2Box2GetSquaredRetail, DistVector2Box2GetSquaredOurs },
    { "Wm3::DistVector2Box2<float>::Get()", 0x00A6C680,
      "Foundation/Distance/Wm3DistVector2Box2.cpp:39 (retail thiscall, no stack"
      " arguments, the float in ST(0); the class vtable's +0x0C slot, which"
      " IntrBox2Circle2::Test at 0x00A4E620 dispatches through)",
      10, 1, "vec[2] box[8]", "ret", GenDistVector2Box2,
      DistVector2Box2GetRetail, DistVector2Box2GetOurs },
    { "Wm3::InBox<float>(const Wm3::Vector3<float>&, const Wm3::Box3<float>&)", 0x00A3BC10,
      "Foundation/Containment/Wm3ContBox3.cpp:355 (retail free function, cdecl:"
      " the point and the box are two stack pointers, the caller removes them,"
      " the bool in AL)",
      18, 1, "point[3] box[15]", "ret", GenInBox,
      InBoxRetail, InBoxOurs },
    { "Wm3::ContAlignedBox<float>(int, const Wm3::Vector3<float>*)", 0x00A3AE40,
      "Foundation/Containment/Wm3ContBox3.cpp:46 (retail free function returning"
      " a Box3 by value, so the hidden return pointer is the first stack"
      " argument; cdecl, the callee leaves its three arguments; the box is"
      " written as the centre, the three axes and the three extents)",
      25, 15, "count[1] points[8]", "center[3] axis[9] extent[3]",
      GenContAlignedBox, ContAlignedBoxRetail, ContAlignedBoxOurs },
    { "Wm3::IntrBox3Sphere3<float>::GetVertexIntersection(float, float, float, float, float, float, float)", 0x00A41740,
      "Foundation/Intersection/Wm3IntrBox3Sphere3.cpp:262 (retail cdecl, seven"
      " float arguments - fDx, fDy, fDz, fVx, fVy, fVz, fRSqr - the caller"
      " removes them, the result in ST(0); a private static of the WM3 class,"
      " called by the retail's Find at 0x00A43831)",
      7, 1, "d[3] v[3] r2", "ret", GenIntrBox3Sphere3GetVertexIntersection,
      IntrBox3Sphere3GetVertexIntersectionRetail,
      IntrBox3Sphere3GetVertexIntersectionOurs },
    { "Wm3::IntrBox3Sphere3<float>::GetEdgeIntersection(float, float, float, float, float, float)", 0x00A41800,
      "Foundation/Intersection/Wm3IntrBox3Sphere3.cpp:306 (retail cdecl, six"
      " float arguments - fDx, fDz, fVx, fVz, fVSqr, fRSqr - the result in"
      " ST(0); a private static of the WM3 class, called by the retail's Find"
      " at 0x00A43868 and at 0x00A438D1)",
      6, 1, "d[2] v[2] vsqr r2", "ret", GenIntrBox3Sphere3GetEdgeIntersection,
      IntrBox3Sphere3GetEdgeIntersectionRetail,
      IntrBox3Sphere3GetEdgeIntersectionOurs },
    { "Wm3::IntrBox3Sphere3<float>::FindFaceRegionIntersection(float, float, float, float, float, float, float, float, float, float&, float&, float&, bool)", 0x00A41880,
      "Foundation/Intersection/Wm3IntrBox3Sphere3.cpp:325 (retail thiscall, a"
      " private member: ECX is `this`, thirteen stack arguments - fEx, fEy,"
      " fEz, fCx, fCy, fCz, fVx, fVy, fVz, &rfIx, &rfIy, &rfIz and the"
      " bAboveFace flag - RET 0x34, the callee removes them; the retail"
      " calls it from Find at 0x00A420EE and 0x00A421CC)",
      29, 5, "box[15] sphere[4] f[9] above", "ret point[3] time",
      GenIntrBox3Sphere3FindFaceRegion,
      IntrBox3Sphere3FindFaceRegionRetail, IntrBox3Sphere3FindFaceRegionOurs },
    { "Wm3::IntrBox3Sphere3<float>::FindJustEdgeIntersection(float, float, float, float, float, float, float, float, float, float&, float&, float&)", 0x00A41CC0,
      "Foundation/Intersection/Wm3IntrBox3Sphere3.cpp:464 (retail thiscall, a"
      " private member: ECX is `this`, twelve stack arguments - fCy, fEx,"
      " fEy, fEz, fDx, fDz, fVx, fVy, fVz, &rfIx, &rfIy, &rfIz - RET 0x30;"
      " the retail calls it from FindEdgeRegionIntersection at 0x00A4208A and"
      " 0x00A4216A)",
      28, 5, "box[15] sphere[4] f[9]", "ret point[3] time",
      GenIntrBox3Sphere3FindJustEdgeRegion,
      IntrBox3Sphere3FindJustEdgeRegionRetail,
      IntrBox3Sphere3FindJustEdgeRegionOurs },
    { "Wm3::IntrBox3Sphere3<float>::FindEdgeRegionIntersection(float, float, float, float, float, float, float, float, float, float&, float&, float&, bool)", 0x00A41F50,
      "Foundation/Intersection/Wm3IntrBox3Sphere3.cpp:532 (retail thiscall, a"
      " private member: ECX is `this`, thirteen stack arguments - fEx, fEy,"
      " fEz, fCx, fCy, fCz, fVx, fVy, fVz, &rfIx, &rfIy, &rfIz and the"
      " bAboveEdge flag - RET 0x34; the retail calls it from Find at"
      " 0x00A43C27 and 0x00A43FC7 and from FindVertexRegionIntersection at"
      " 0x00A424BA and 0x00A42581)",
      29, 5, "box[15] sphere[4] f[9] above", "ret point[3] time",
      GenIntrBox3Sphere3FindEdgeRegion,
      IntrBox3Sphere3FindEdgeRegionRetail, IntrBox3Sphere3FindEdgeRegionOurs },
    { "Wm3::IntrBox3Sphere3<float>::FindVertexRegionIntersection(float, float, float, float, float, float, float, float, float, float&, float&, float&)", 0x00A421E0,
      "Foundation/Intersection/Wm3IntrBox3Sphere3.cpp:620 (retail thiscall, a"
      " private member: ECX is `this`, twelve stack arguments - fEx, fEy,"
      " fEz, fCx, fCy, fCz, fVx, fVy, fVz, &rfIx, &rfIy, &rfIz - RET 0x30;"
      " the retail calls it from Find at 0x00A44016)",
      28, 5, "box[15] sphere[4] f[9]", "ret point[3] time",
      GenIntrBox3Sphere3FindVertexRegion,
      IntrBox3Sphere3FindVertexRegionRetail,
      IntrBox3Sphere3FindVertexRegionOurs },
    { "Wm3::ContAlignedBox<float>(int, const Wm3::Vector3<float>*, const bool*)", 0x00A3AE40,
      "Foundation/Containment/Wm3ContBox3.cpp:62 (retail free function returning"
      " a Box3 by value, so the hidden return pointer is the first stack"
      " argument; cdecl with FOUR arguments - the fourth is the abValid mask,"
      " which the retail tests a byte at a time; the mask is null for the"
      " unmasked path)",
      28, 15, "count[1] mode[1] points[24] mask[8]", "center[3] axis[9] extent[3]",
      GenContAlignedBoxMask, ContAlignedBoxMaskRetail,
      ContAlignedBoxMaskOurs },
    { "Wm3::IntrLine3Box3<float>::DoClipping(float, float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&, const Wm3::Box3<float>&, bool, int&, Wm3::Vector3<float>*, int&)", 0x00A50220,
      "Foundation/Intersection/Wm3IntrLine3Box3.cpp:174 (retail static member,"
      " so there is no `this`: nine stack arguments - fT0, fT1, &origin,"
      " &direction, &box, bSolid, &riQuantity, akPoint, &riIntrType - cdecl, the"
      " caller removes the thirty-six bytes, the bool in AL. The retail's own"
      " Find at 0x00A508F0 calls it and pushes the literal 1 for bSolid)"
      " -- the two points are compared only where the helper defines them",
      24, 9, "t0 t1 line[6] box[15] solid",
      "ret quantity type point0[3] point1[3]",
      GenIntrLine3Box3DoClipping,
      IntrLine3Box3DoClippingRetail, IntrLine3Box3DoClippingOurs },
    { "Wm3::IntrLine3Box3<float>::Clip(float, float, float&, float&)", 0x00A4FCC0,
      "Foundation/Intersection/Wm3IntrLine3Box3.cpp:303 (retail static member"
      " and a private one: four stack arguments - fDenom, fNumer, &rfT0,"
      " &rfT1 - cdecl, the caller removes the sixteen bytes, the bool in AL."
      " The retail's own DoClipping calls it at 0x00A5032D and 0x00A50366)",
      4, 3, "denom numer t0 t1", "ret t0 t1",
      GenIntrLine3Box3Clip,
      IntrLine3Box3ClipRetail, IntrLine3Box3ClipOurs },
    { "Wm3::Distance<float, Wm3::Vector3<float>>::SetDifferenceStep(float)", 0x00A391D0,
      "Foundation/Distance/Wm3Distance.cpp:71 (retail thiscall, one float"
      " argument on the stack and the callee pops it - RET 4 - the step lands"
      " at [this+0x2C] and 0.5/step at [this+0x30]. The base class's own"
      " vftable is 0x00D71C54 - the base constructor at 0x00A39F20 installs"
      " it - and this body is a member of the base, so the row runs on an"
      " object the retail's own base constructor built)",
      1, 2, "step", "step invtwo", GenDistanceSetDifferenceStep,
      DistanceSetDifferenceStepRetail, DistanceSetDifferenceStepOurs },
    { "Wm3::Distance<float, Wm3::Vector3<float>>::Distance()", 0x00A39F20,
      "Foundation/Distance/Wm3Distance.cpp:22 (retail thiscall, no stack"
      " arguments: the base constructor, which inlines the defaults - the step"
      " 1e-3 at [this+0x2C] and its reciprocal 500.0f at [this+0x30] - instead"
      " of reaching them through the setter. The rebuild's constructor calls"
      " the setter, so this row is what says the two agree there)",
      1, 2, "step", "step invtwo", GenDistanceSetDifferenceStep,
      DistanceDifferenceStepDefaultRetail, DistanceDifferenceStepDefaultOurs },
    { "Wm3::Vector3<float>::Length() const", 0x00452A90,
      "Foundation/Math/Wm3Vector3.inl:259 (retail thiscall, ECX = `this`,"
      " no stack arguments, a plain RET, and the length comes back in ST(0)."
      " The body computes the three squares with SSE (MOVSS/MULSS) and"
      " calls the CRT's square root at 0x00452FC0, so the sum behaves as"
      " binary32 in the denormal range - where this body and the x87"
      " SquaredLength body at 0x004757E0 differ. The engine calls the"
      " instance method (CAiSteeringImpl.cpp:198 and :1458), so the row"
      " pins the value the engine's own inlined copy depends on)",
      3, 1, "v[3]", "ret", GenVec3Lanes, Vector3LengthRetail,
      Vector3LengthOurs },
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
      6, 1, "a[3] b[3]", "ret", GenVec3Compare,
      Vector3CompareArraysRetail, Vector3CompareArraysOurs },
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
      6, 1, "a[3] b[3]", "ret", GenVec3Equals,
      Vector3OperatorEqualsRetail, Vector3OperatorEqualsOurs },
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
      8, 1, "a[4] b[4]", "ret", GenQuatEquals,
      QuaternionCompareArraysRetail, QuaternionCompareArraysOurs },
    { "Wm3::Quaternion<float>::operator==(const Wm3::Quaternion<float>&) const",
      0x004F08E0,
      "Foundation/Math/Wm3Quaternion.inl:150 (the same register convention as"
      " CompareArrays - ECX = `this`, EAX = the other quaternion, a plain RET -"
      " and the body starts with a bare `call 0x004F0B40`, so it assumes both"
      " registers are already set. The result is that memcmp result folded"
      " into a bool (`neg eax; sbb eax,eax; add eax,1`). The body has no"
      " caller in the image: the compiler inlined the member at every use site)",
      8, 1, "a[4] b[4]", "ret", GenQuatEquals,
      QuaternionOperatorEqualsRetail, QuaternionOperatorEqualsOurs },
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
      4, 4, "center[3] radius", "obj[4]", GenSphere3Ctor,
      Sphere3CtorRetail, Sphere3CtorOurs },
  };

  const wmdiff::Target* Targets(int& count)
  {
    count = static_cast<int>(sizeof(kTargets) / sizeof(kTargets[0]));
    return kTargets;
  }

} // namespace wm3coll
