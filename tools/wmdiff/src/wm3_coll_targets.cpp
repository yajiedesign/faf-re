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

using difffuzz::GenEdge;
using difffuzz::GenPlain;

namespace wm3coll
{
  namespace
  {


    // Retail's DistVector3Box3 object stores its two reference members at
    // [this+0x34] and [this+0x38] and sets a vtable pointer, so it needs at
    // least 0x3C bytes of writable storage. 64 is comfortably above that and
    // keeps the buffer's address layout irrelevant.
    constexpr int kObjectBytes = 64;

    // The double instantiation of the same classes is a different animal:
    // the retail's Distance<double,Vector3d> puts the difference step at
    // [this+0x58] and its reciprocal at [this+0x60], and the derived
    // DistVector3Box3<double> puts its two references at [this+0x68] and
    // [this+0x6C].  The object therefore needs 0x70 bytes, and the double
    // rows use their own storage size.
    constexpr int kDoubleObjectBytes = 128;

    // vec[3] then a Box3 (Center[3], Axis[3][3], Extent[3]) - eighteen floats.
    void GenDistVector3Box3(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      difffuzz::GenVec3(rng, f);
      for (int i = 3; i < 18; ++i) {
        f[i] = rng.Chance(75) ? GenPlain(rng, 1000.0f) : difffuzz::GenFloat(rng);
      }
    }

    void DistVector3Box3GetSquaredRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistVector3Box3Ctor(storage, f, f + 3);
      out[0] = difffuzz::Bits(wm3collretail::DistVector3Box3GetSquared(storage));
    }

    // Retail's Get() takes the CRT sqrt at 0x00A91000, not the game's own
    // Math<Real>::Sqrt, and on a non-finite argument that path reaches an
    // import the harness does not resolve - the run raises the trap code
    // instead of returning NaN. The same ceiling as
    // DistVector3Segment3::Get, so the same bound: finite results.
    // Retail's Get() takes the CRT's sqrt at 0x00A91000, and the CRT's slow
    // path for a non-positive argument is a fatal math error - in the retail as
    // well, not only in this harness. The engine never produces such a
    // distance, so the row measures the range the engine actually sees.
    void GenDistVector3Box3Get(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      for (int attempt = 0; attempt < 32; ++attempt) {
        GenDistVector3Box3(rng, in);
        const float squared = wm3collharness::DistVector3Box3GetSquared(f, f + 3);
        const std::uint32_t bits = difffuzz::Bits(squared);
        if ((bits & 0x7F800000u) != 0x7F800000u) {
          return; // exponent is not all ones, so this is finite
        }
      }
      for (int i = 0; i < 18; ++i) {
        f[i] = GenPlain(rng, 1.0f);
      }
    }

    void DistVector3Box3GetRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistVector3Box3Ctor(storage, f, f + 3);
      out[0] = difffuzz::Bits(wm3collretail::DistVector3Box3Get(storage));
    }

    void DistVector3Box3GetOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(wm3collharness::DistVector3Box3Get(f, f + 3));
    }

    void DistVector3Box3GetSquaredOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(wm3collharness::DistVector3Box3GetSquared(f, f + 3));
    }

    // A point a few times 1e-19 off a segment's line: the squared distance
    // is around 1e-38, which is below the smallest normal float
    // (1.1754944e-38) and therefore lives on the denormal grid, where a
    // float holds far fewer bits than the x87 register does.  The direction
    // is the x axis and the point is off it in y, so the closest point on
    // the segment is the origin and the distance is exactly the offset.
    void GenDistVector3Segment3Denormal(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      const float d = static_cast<float>(rng.Range(1.0e-20, 1.0e-19));
      f[0] = 0.0f; f[1] = d; f[2] = 0.0f;   // the vector, d off the line
      f[3] = 0.0f; f[4] = 0.0f; f[5] = 0.0f; // the segment's origin
      f[6] = 1.0f; f[7] = 0.0f; f[8] = 0.0f; // the direction
      f[9] = 10.0f;                          // the extent
    }

    // vec[3] then a Segment3 (Origin[3], Direction[3], Extent) - ten floats.
    void GenDistVector3Segment3(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      if (rng.Chance(15)) {
        GenDistVector3Segment3Denormal(rng, in);
        return;
      }
      difffuzz::GenVec3(rng, f);
      for (int i = 3; i < 9; ++i) {
        f[i] = rng.Chance(75) ? GenPlain(rng, 10.0f) : difffuzz::GenFloat(rng);
      }
      f[9] = rng.Chance(80) ? GenPlain(rng, 1000.0f) : GenEdge(rng);
    }

    void DistVector3Segment3GetSquaredRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistVector3Segment3Ctor(storage, f, f + 3);
      out[0] = difffuzz::Bits(wm3collretail::DistVector3Segment3GetSquared(storage));
    }

    // The retail GetSquared has to run before GetClosestPoint1 is worth
    // reading, exactly as on the rebuilt side, so both objects go through the
    // same sequence. No gate on the result: GetSquared assigns m_kClosestPoint1
    // on all three of its branches.
    void DistVector3Segment3GetClosestPoint1Retail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistVector3Segment3Ctor(storage, f, f + 3);
      (void)wm3collretail::DistVector3Segment3GetSquared(storage);
      const float* p = wm3collretail::DistVector3Segment3GetClosestPoint1(storage);
      std::memcpy(out, p, 3 * sizeof(float));
    }

    void DistVector3Segment3GetClosestPoint1Ours(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      wm3collharness::DistVector3Segment3GetClosestPoint1(f, f + 3,
                                                           reinterpret_cast<float*>(out));
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
    void GenDistVector3Segment3Get(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      for (int attempt = 0; attempt < 32; ++attempt) {
        GenDistVector3Segment3(rng, in);
        const float squared = wm3collharness::DistVector3Segment3GetSquared(f, f + 3);
        const std::uint32_t bits = difffuzz::Bits(squared);
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
      out[0] = difffuzz::Bits(wm3collretail::DistVector3Segment3Get(storage));
    }

    void DistVector3Segment3GetOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(wm3collharness::DistVector3Segment3Get(f, f + 3));
    }

    void DistVector3Segment3GetSquaredOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(wm3collharness::DistVector3Segment3GetSquared(f, f + 3));
    }

    // box[8] (Center[2], Axis[2][2], Extent[2]) then circle[3] (Center[2],
    // Radius) - eleven floats.
    //
    // The radius is drawn around the box's own extents so that "just touching"
    // and "just missing" are both common: a Test that only ever sees clearly
    // inside or clearly outside answers the same every time.
    void GenIntrBox2Circle2(difffuzz::Rng& rng, std::uint32_t* in)
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

    

    
    
    // box[8], circle[3], fTMax, velocity0[2], velocity1[2] - sixteen floats.
    //
    // A well-conditioned box on purpose. GenIntrBox2Circle2 draws the extents
    // with GenPlain, which signs them, and the axis is arbitrary rather than
    // orthonormal - so the projections and the extents are not comparable, the
    // region dispatch compares against nonsense, and both sides answer the same
    // thing for the wrong reason. The same trap is already written up for
    // IntrBox3Sphere3::Find. Here the axis is the identity, the extents are
    // positive and small, the circle sits within a couple of box widths, and the
    // velocities are on the box's scale, so the region tests actually decide.
    void GenIntrBox2Circle2Find(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      f[0] = GenPlain(rng, 10.0f);            // box centre x
      f[1] = GenPlain(rng, 10.0f);            // box centre y
      f[2] = 1.0f; f[3] = 0.0f;               // axis[0] = (1, 0)
      f[4] = 0.0f; f[5] = 1.0f;               // axis[1] = (0, 1)
      f[6] = rng.Range(2.0, 12.0);            // extent x, positive
      f[7] = rng.Range(2.0, 12.0);            // extent y, positive
      f[8] = GenPlain(rng, 20.0f);            // circle centre x
      f[9] = GenPlain(rng, 20.0f);            // circle centre y
      f[10] = rng.Range(0.5, 8.0);            // radius, comparable to the extents
      f[11] = rng.Chance(50) ? 0.0f : rng.Range(0.0, 2.0);
      for (int i = 12; i < 16; ++i) {
        f[i] = GenPlain(rng, 6.0f);           // velocities on the box's scale
      }

      // Two forced inputs whose answers the geometry fixes, so the address can
      // be checked before any of its numbers are trusted. The lesson is already
      // written up under IntrBox3Sphere3::Find: a control has to reach the path
      // it claims to test, and the earlier one there did not.
      //   WMDIFF_B2C_CONTROL=inside -> centre at the box centre: Find must be true
      //                                (the Rzz early return)
      //   WMDIFF_B2C_CONTROL=far    -> centre one box away, static, fTMax 0:
      //                                Find must be false
      if (const char* const ctl = std::getenv("WMDIFF_B2C_CONTROL")) {
        for (int i = 12; i < 16; ++i) {
          f[i] = 0.0f;
        }
        f[11] = 0.0f;
        f[10] = 1.0f;
        if (ctl[0] == 'i') {
          f[8] = f[0];
          f[9] = f[1];
        }
        else if (ctl[0] == 'f') {
          f[8] = f[0] + 200.0f;
          f[9] = f[1];
        }
      }
    }

    void IntrBox2Circle2FindRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrBox2Circle2Ctor(storage, f, f + 8);
      out[0] = wm3collretail::IntrBox2Circle2Find(storage, f[11], f + 12, f + 14);
    }

    void IntrBox2Circle2FindOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wm3collharness::IntrBox2Circle2Find(f, f + 8, f[11], f + 12, f + 14);
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
    void GenIntrLine3Box3(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      difffuzz::GenVec3(rng, f);
      difffuzz::GenVec3(rng, f + 3);
      for (int i = 6; i < 21; ++i) {
        f[i] = rng.Chance(75) ? GenPlain(rng, 1000.0f) : difffuzz::GenFloat(rng);
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
    void GenIntrSegment3Box3(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      difffuzz::GenVec3(rng, f);
      for (int i = 3; i < 9; ++i) {
        f[i] = rng.Chance(75) ? GenPlain(rng, 10.0f) : difffuzz::GenFloat(rng);
      }
      f[9] = rng.Chance(80) ? GenPlain(rng, 1000.0f) : GenEdge(rng);
      for (int i = 10; i < 25; ++i) {
        f[i] = rng.Chance(75) ? GenPlain(rng, 1000.0f) : difffuzz::GenFloat(rng);
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
    void GenIntrSegment3Sphere3(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);

      // the sphere: centre and radius
      for (int i = 0; i < 3; ++i) {
        f[7 + i] = GenPlain(rng, 100.0f);
      }
      const float radius = static_cast<float>(rng.Range(0.5, 5.0));
      f[10] = radius;

      // the segment direction: unit length
      difffuzz::GenUnitVec3(rng, f + 3);
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

    void IntrSegment3Sphere3TestRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrSegment3Sphere3Ctor(storage, f, f + 7);
      out[0] = wm3collretail::IntrSegment3Sphere3Test(storage);
    }

    void IntrSegment3Sphere3TestOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wm3collharness::IntrSegment3Sphere3Test(f, f + 7);
    }

    // The swept rows: the same geometry, plus fTMax and the two velocities.
    // segment[7] sphere[4] tmax vel0[3] vel1[3] - eighteen floats.
    //
    // The swept query only sees the *relative* velocity, velocity1 -
    // velocity0: the rebuilt Test() builds its capsule from exactly that
    // difference. So the generator draws one direction and one speed and
    // splits it evenly between the two velocities, which keeps the two
    // sides' streams independent of the geometry.
    //
    // The speed is on the radius's scale, so the capsule the sphere sweeps
    // out is comparable to the sphere itself, and fTMax is either zero (the
    // contact has to be immediate) or a few times the time the sphere needs
    // to cross its own radius.
    void GenIntrSegment3Sphere3Dynamic(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      GenIntrSegment3Sphere3(rng, in);

      const float radius = f[10];
      difffuzz::GenUnitVec3(rng, f + 15);
      const float speed = radius * static_cast<float>(rng.Range(0.0, 3.0));
      for (int i = 0; i < 3; ++i) {
        f[15 + i] = f[15 + i] * speed;   // velocity1
        f[12 + i] = -0.5f * f[15 + i];   // velocity0, so the relative velocity
      }                                   // is exactly the drawn one
      f[11] = rng.Chance(50) || speed <= 0.0f
             ? 0.0f
             : 4.0f * radius / speed;
    }

    void IntrSegment3Sphere3TestDynamicRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrSegment3Sphere3Ctor(storage, f, f + 7);
      out[0] = wm3collretail::IntrSegment3Sphere3TestDynamic(storage, f[11], f + 12, f + 15);
    }

    void IntrSegment3Sphere3TestDynamicOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wm3collharness::IntrSegment3Sphere3TestDynamic(f, f + 7, f[11], f + 12, f + 15);
    }

    void IntrSegment3Sphere3FindDynamicRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrSegment3Sphere3Ctor(storage, f, f + 7);
      out[0] = wm3collretail::IntrSegment3Sphere3FindDynamic(storage, f[11], f + 12, f + 15);
    }

    void IntrSegment3Sphere3FindDynamicOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wm3collharness::IntrSegment3Sphere3FindDynamic(f, f + 7, f[11], f + 12, f + 15);
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
    void GenIntrBox2Box2(difffuzz::Rng& rng, std::uint32_t* in)
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
    void GenIntrBox3Sphere3(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      for (int i = 0; i < 15; ++i) {
        f[i] = rng.Chance(75) ? GenPlain(rng, 1000.0f) : difffuzz::GenFloat(rng);
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
    // through difffuzz::GenFloat, and the region helpers inside Find do not
    // survive those: the rebuild computes them in float, where retail computes
    // in x87, so an overflow becomes an infinity here and a large finite value
    // there, and that changes which branch is taken. It is the same
    // exponent-range family documented on Vector3::Dot and Vector3::LengthSq,
    // and the fix belongs in the region helpers, in the Wm3Detail 24-bit
    // treatment Wm3DistVector3Box3.cpp already got.
    void GenIntrBox3Sphere3Find(difffuzz::Rng& rng, std::uint32_t* in)
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
    void GenIntrBox3Sphere3GetVertexIntersection(difffuzz::Rng& rng, std::uint32_t* in)
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
      out[0] = difffuzz::Bits(wm3collretail::IntrBox3Sphere3GetVertexIntersection(
        f[0], f[1], f[2], f[3], f[4], f[5], f[6]));
    }

    void IntrBox3Sphere3GetVertexIntersectionOurs(const std::uint32_t* in,
                                                   std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(wm3collharness::IntrBox3Sphere3GetVertexIntersection(
        f[0], f[1], f[2], f[3], f[4], f[5], f[6]));
    }

    // The same in two dimensions: d[2], v[2], vsqr and r2.  The squared
    // speed is an argument of the helper, not something it computes, so the
    // generator passes it.
    void GenIntrBox3Sphere3GetEdgeIntersection(difffuzz::Rng& rng, std::uint32_t* in)
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
      out[0] = difffuzz::Bits(wm3collretail::IntrBox3Sphere3GetEdgeIntersection(
        f[0], f[1], f[2], f[3], f[4], f[5]));
    }

    void IntrBox3Sphere3GetEdgeIntersectionOurs(const std::uint32_t* in,
                                                 std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(wm3collharness::IntrBox3Sphere3GetEdgeIntersection(
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
    void GenIntrBox3Sphere3RegionBox(difffuzz::Rng& rng, std::uint32_t* in,
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
    void GenIntrBox3Sphere3FindFaceRegion(difffuzz::Rng& rng, std::uint32_t* in)
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
    void GenIntrBox3Sphere3FindEdgeRegion(difffuzz::Rng& rng, std::uint32_t* in)
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
    void GenIntrBox3Sphere3FindVertexRegion(difffuzz::Rng& rng, std::uint32_t* in)
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
    void GenIntrBox3Sphere3FindJustEdgeRegion(difffuzz::Rng& rng, std::uint32_t* in)
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

    // A Segment3: Origin[3], Direction[3], Extent. The direction is unit
    // length (the way the engine's own segments are), the origin is on a
    // moderate scale and the extent is positive - a negative extent is not a
    // shape - with the occasional zero.
    void GenSegment3Data(difffuzz::Rng& rng, float* f)
    {
      for (int i = 0; i < 3; ++i) {
        f[i] = GenPlain(rng, 100.0f);
      }
      difffuzz::GenUnitVec3(rng, f + 3);
      f[6] = rng.Chance(3) ? 0.0f : static_cast<float>(rng.Range(0.1, 20.0));
    }

    // Two segments: seg0[7] then seg1[7].
    //
    // The second segment is placed relative to the first - `along` the first
    // direction and `off` across it - so the distance formula's branches are
    // reached instead of only the far-away case. About one draw in eight has
    // the second direction parallel or antiparallel to the first, which is
    // the fDet < ZERO_TOLERANCE branch (the segments are parallel), and a few
    // percent have a zero extent.
    void GenDistSegment3Segment3(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      GenSegment3Data(rng, f);                   // segment 0

      float perp[3];
      MakePerp3(f + 3, perp);
      const float along = static_cast<float>(rng.Range(-40.0, 40.0));
      const float off = static_cast<float>(rng.Range(-25.0, 25.0));
      for (int i = 0; i < 3; ++i) {
        f[7 + i] = f[i] + along*f[3 + i] + off*perp[i];
      }
      if (rng.Chance(12)) {
        // parallel or antiparallel to the first segment
        const float s = rng.Chance(50) ? 1.0f : -1.0f;
        for (int i = 0; i < 3; ++i) {
          f[10 + i] = s*f[3 + i];
        }
      }
      else {
        difffuzz::GenUnitVec3(rng, f + 10);
      }
      f[13] = rng.Chance(3) ? 0.0f : static_cast<float>(rng.Range(0.1, 20.0));
    }

    // The Get() row's scope is finite, non-negative results: retail's Get
    // takes the CRT's square root at 0x00A91000, whose slow path for a
    // negative argument is a fatal math error - in the retail as well, not
    // only in this harness. The same bound as the other two Get() rows, and
    // the filter lives in this row's generator rather than behind the global
    // --skip-extreme, so the other rows keep full coverage.
    void GenDistSegment3Segment3Get(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      for (int attempt = 0; attempt < 32; ++attempt) {
        GenDistSegment3Segment3(rng, in);
        const float squared =
          wm3collharness::DistSegment3Segment3GetSquared(f, f + 7);
        const std::uint32_t bits = difffuzz::Bits(squared);
        const bool finite = (bits & 0x7F800000u) != 0x7F800000u;
        const bool negative =
          (bits & 0x80000000u) != 0u && (bits & 0x7FFFFFFFu) != 0u;
        if (finite && !negative) {
          return;
        }
      }
      for (int i = 0; i < 14; ++i) {
        f[i] = GenPlain(rng, 1.0f);
      }
    }

    void DistSegment3Segment3GetSquaredRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistSegment3Segment3Ctor(storage, f, f + 7);
      float value = 0.0f;
      wm3collretail::DistSegment3Segment3GetSquared(storage, &value);
      out[0] = difffuzz::Bits(value);
    }

    void DistSegment3Segment3GetSquaredOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(
        wm3collharness::DistSegment3Segment3GetSquared(f, f + 7));
    }

    void DistSegment3Segment3GetRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistSegment3Segment3Ctor(storage, f, f + 7);
      out[0] = difffuzz::Bits(wm3collretail::DistSegment3Segment3Get(storage));
    }

    void DistSegment3Segment3GetOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(
        wm3collharness::DistSegment3Segment3Get(f, f + 7));
    }

    // vec[2] then box[8] (Center[2], Axis[2][2], Extent[2]) - ten floats.
    //
    // The box gets an orthonormal axis and positive extents: the algorithm
    // projects the vector onto the box's axes and clamps against the
    // extents, so a non-orthonormal basis or a negative extent would make
    // the test meaningless. The vector is drawn around the box, so "inside",
    // "on the edge" and "outside" are all common.
    void GenDistVector2Box2(difffuzz::Rng& rng, std::uint32_t* in)
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
      out[0] = difffuzz::Bits(value);
    }

    void DistVector2Box2GetSquaredOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(
        wm3collharness::DistVector2Box2GetSquared(f, f + 2));
    }

    // segment[7] then capsule[8] (a Segment3 followed by the radius) -
    // fifteen floats.
    //
    // The capsule's spine is placed near the query segment: `along` the
    // query direction and `off` across it, with `off` drawn around the
    // radius half the time, so "the segment just grazes the capsule" and
    // "the segment misses it" are both common. The capsule's own spine
    // direction is parallel to the query direction a quarter of the time.
    void GenIntrSegment3Capsule3(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);

      // One draw in ten is exactly tangent on the non-parallel path, which is
      // the only path that owns the discriminant's ZERO_TOLERANCE branch. The
      // geometry is built in the capsule's own frame so the arithmetic is
      // exact: capsule spine at the origin along +z, query at (r,0,0) along
      // (0,1/2,sqrt(3)/2). Then kP = (r,0,0), kD = (0,1/2,0) in that frame,
      // fA1 is zero and fA0 = r*r - r*r is exactly zero, so the discriminant is
      // exactly zero - a random near-tangent draw would be a few ulp away and
      // would take the two-root or no-root branch instead.
      if (rng.Chance(10)) {
        const float radius = static_cast<float>(rng.Range(0.2, 5.0));
        f[0] = f[1] = f[2] = 0.0f;
        f[3] = 0.0f; f[4] = 0.0f; f[5] = 1.0f;
        f[6] = static_cast<float>(rng.Range(0.1, 20.0));
        f[7] = radius; f[8] = 0.0f; f[9] = 0.0f;
        f[10] = 0.0f; f[11] = 0.5f; f[12] = 0.8660254f;
        f[13] = static_cast<float>(rng.Range(0.1, 20.0));
        f[14] = radius;
        return;
      }

      GenSegment3Data(rng, f);                   // the query segment

      const float radius = static_cast<float>(rng.Range(0.2, 5.0));
      float perp[3];
      MakePerp3(f + 3, perp);
      const float along = static_cast<float>(rng.Range(-30.0, 30.0));
      const float off = rng.Chance(50)
        ? static_cast<float>(rng.Range(-2.0*radius, 2.0*radius))
        : static_cast<float>(rng.Range(-25.0, 25.0));
      for (int i = 0; i < 3; ++i) {
        f[7 + i] = f[i] + along*f[3 + i] + off*perp[i];
      }
      if (rng.Chance(25)) {
        for (int i = 0; i < 3; ++i) {
          f[10 + i] = f[3 + i];
        }
      }
      else {
        difffuzz::GenUnitVec3(rng, f + 10);
      }
      f[13] = static_cast<float>(rng.Range(0.1, 20.0));   // capsule extent
      f[14] = radius;
    }

    void IntrSegment3Capsule3TestRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrSegment3Capsule3Ctor(storage, f, f + 7);
      out[0] = wm3collretail::IntrSegment3Capsule3Test(storage);
    }

    void IntrSegment3Capsule3TestOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wm3collharness::IntrSegment3Capsule3Test(f, f + 7);
    }

    // The intersection set is only defined after a successful Find: the
    // quantity is zero and there is no parameter and no point otherwise, so
    // both sides write zero on the miss path. The retail's members are read
    // out of its object storage: m_iQuantity at [this+0x14], the first point
    // at [this+0x18] and the first parameter at [this+0x30].
    void IntrSegment3Capsule3FindRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrSegment3Capsule3Ctor(storage, f, f + 7);
      const std::uint32_t hit = wm3collretail::IntrSegment3Capsule3Find(storage);
      out[0] = hit;
      if (!hit) {
        out[1] = out[2] = out[3] = out[4] = out[5] = 0u;
        return;
      }
      std::memcpy(out + 1, storage + 0x14, sizeof(std::uint32_t));
      std::memcpy(out + 2, storage + 0x30, sizeof(float));   // parameter 0
      std::memcpy(out + 3, storage + 0x18, 3*sizeof(float));   // point 0
    }

    void IntrSegment3Capsule3FindOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      wm3collharness::IntrSegment3Capsule3Find(f, f + 7, out);
    }

    // line[6] (Origin[3], Direction[3]) then capsule[8] - fourteen floats.
    //
    // This is a line query, so there is no extent filter on the query: the
    // line has to pass within the radius, and the returned parameters are the
    // roots. The capsule's spine is placed near the line, with the
    // perpendicular offset drawn around the radius half the time, so the hit
    // and the miss are both common.
    void GenIntrLine3Capsule3(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);

      // The same exact tangent case as GenIntrSegment3Capsule3: capsule spine
      // on the z-axis at the origin, the line through (r,0,0) along
      // (0,1/2,sqrt(3)/2). It is not parallel to the axis, and its
      // discriminant is exactly zero, which is the ZERO_TOLERANCE branch.
      if (rng.Chance(10)) {
        const float radius = static_cast<float>(rng.Range(0.2, 5.0));
        f[0] = radius; f[1] = 0.0f; f[2] = 0.0f;
        f[3] = 0.0f; f[4] = 0.5f; f[5] = 0.8660254f;
        f[6] = f[7] = f[8] = 0.0f;
        f[9] = 0.0f; f[10] = 0.0f; f[11] = 1.0f;
        f[12] = static_cast<float>(rng.Range(0.1, 20.0));
        f[13] = radius;
        return;
      }

      for (int i = 0; i < 3; ++i) {
        f[i] = GenPlain(rng, 100.0f);
      }
      difffuzz::GenUnitVec3(rng, f + 3);

      const float radius = static_cast<float>(rng.Range(0.2, 5.0));
      float perp[3];
      MakePerp3(f + 3, perp);
      const float along = static_cast<float>(rng.Range(-30.0, 30.0));
      const float off = rng.Chance(50)
        ? static_cast<float>(rng.Range(-2.0*radius, 2.0*radius))
        : static_cast<float>(rng.Range(-25.0, 25.0));
      for (int i = 0; i < 3; ++i) {
        f[6 + i] = f[i] + along*f[3 + i] + off*perp[i];
      }
      if (rng.Chance(25)) {
        for (int i = 0; i < 3; ++i) {
          f[9 + i] = f[3 + i];
        }
      }
      else {
        difffuzz::GenUnitVec3(rng, f + 9);
      }
      f[12] = static_cast<float>(rng.Range(0.1, 20.0));   // capsule extent
      f[13] = radius;
    }

    void IntrLine3Capsule3FindRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      float afT[2] = { 0.0f, 0.0f };
      const int iQuantity =
        wm3collretail::IntrLine3Capsule3Find(f, f + 3, f + 6, afT);
      out[0] = static_cast<std::uint32_t>(iQuantity);
      out[1] = (iQuantity > 0) ? difffuzz::Bits(afT[0]) : 0u;
      out[2] = (iQuantity > 1) ? difffuzz::Bits(afT[1]) : 0u;
    }

    void IntrLine3Capsule3FindOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      wm3collharness::IntrLine3Capsule3Find(f, f + 6, out);
    }

    // The member Test of the same class, at the class vtable's +0x08 slot.
    // The inputs are the same fourteen words the Find row uses - line[6]
    // (Origin[3], Direction[3]) then capsule[8] (the spine Segment3 followed
    // by the radius) - so the same generator serves both rows: the geometry
    // that makes "the line grazes the capsule" and "the line misses it" both
    // common is exactly what a boolean row needs.
    void IntrLine3Capsule3TestRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrLine3Capsule3Ctor(storage, f, f + 6);
      out[0] = wm3collretail::IntrLine3Capsule3Test(storage);
    }

    void IntrLine3Capsule3TestOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wm3collharness::IntrLine3Capsule3Test(f, f + 6);
    }

    // ---- Wm3::ConvexHull3<float> -----------------------------------------
    //
    // The input is a point set with a known hull, not a random cloud:
    //   word 0        the number of points (1..6)
    //   words 1..18   the points, three floats each (six points' worth)
    //   word 19       the epsilon (the Mapper3's relative tolerance)
    //   word 20       the Query::Type value
    //
    // The hull's own index order comes from a std::set of triangle pointers,
    // so it follows the heap addresses and cannot be compared between the two
    // sides; the set of triangles can, and the canonicalisation in
    // wm3collharness::ConvexHull3Canonical does that.
    void GenConvexHull3(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      for (int i = 0; i < 19; ++i) {
        f[i] = 0.0f;
      }
      in[0] = 0;                  // the point count, filled in below
      in[20] = 3;                 // Wm3::Query::QT_REAL

      // The epsilon is the Mapper3's relative tolerance for the dimension
      // test. It has to be positive: with epsilon zero a single point would
      // be taken for a zero-length line, and the direction's normalisation
      // would divide by zero.
      in[19] = difffuzz::Bits(1.0e-6f);

      int n = 0;
      switch (rng.Below(10)) {
      case 0: {
        // A triangle: three points, dimension 2.
        n = 3;
        f[1] = 0.0f; f[2] = 0.0f; f[3] = 0.0f;
        f[4] = 1.0f; f[5] = 0.0f; f[6] = 0.0f;
        f[7] = 0.0f; f[8] = 1.0f; f[9] = 0.0f;
        break;
      }
      case 1: {
        // A square in the z = 0 plane: dimension 2.
        n = 4;
        f[1] = 0.0f; f[2] = 0.0f; f[3] = 0.0f;
        f[4] = 1.0f; f[5] = 0.0f; f[6] = 0.0f;
        f[7] = 1.0f; f[8] = 1.0f; f[9] = 0.0f;
        f[10] = 0.0f; f[11] = 1.0f; f[12] = 0.0f;
        break;
      }
      case 2: {
        // A regular pentagon on the unit circle: dimension 2.
        n = 5;
        for (int i = 0; i < 5; ++i) {
          const double fAngle = 2.0*3.14159265358979323846*i/5.0;
          f[1 + 3*i + 0] = static_cast<float>(std::cos(fAngle));
          f[1 + 3*i + 1] = static_cast<float>(std::sin(fAngle));
          f[1 + 3*i + 2] = 0.0f;
        }
        break;
      }
      case 3: {
        // A regular hexagon on the unit circle: dimension 2, six points.
        n = 6;
        for (int i = 0; i < 6; ++i) {
          const double fAngle = 2.0*3.14159265358979323846*i/6.0;
          f[1 + 3*i + 0] = static_cast<float>(std::cos(fAngle));
          f[1 + 3*i + 1] = static_cast<float>(std::sin(fAngle));
          f[1 + 3*i + 2] = 0.0f;
        }
        break;
      }
      case 4: {
        // A tetrahedron: dimension 3, four faces.
        n = 4;
        f[1] = 0.0f; f[2] = 0.0f; f[3] = 0.0f;
        f[4] = 1.0f; f[5] = 0.0f; f[6] = 0.0f;
        f[7] = 0.0f; f[8] = 1.0f; f[9] = 0.0f;
        f[10] = 0.0f; f[11] = 0.0f; f[12] = 1.0f;
        break;
      }
      case 5: {
        // The eight corners of a cube, thinned to six: dimension 3.
        n = 6;
        const float akCorner[8][3] = {
          {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
          {1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f},
          {0.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f} };
        const int aiPick[6] = {0, 6, 2, 4, 1, 5};
        for (int i = 0; i < 6; ++i) {
          for (int c = 0; c < 3; ++c) {
            f[1 + 3*i + c] = akCorner[aiPick[i]][c];
          }
        }
        break;
      }
      case 6: {
        // Collinear points: dimension 1.
        n = 4;
        for (int i = 0; i < 4; ++i) {
          f[1 + 3*i + 0] = static_cast<float>(i);
          f[1 + 3*i + 1] = 0.0f;
          f[1 + 3*i + 2] = 0.0f;
        }
        break;
      }
      case 7: {
        // Duplicate points: the same point twice plus a third one, so the
        // set is a point set of two distinct points - dimension 1.
        n = 3;
        f[1] = 0.25f; f[2] = 0.5f; f[3] = 0.75f;
        f[4] = 0.25f; f[5] = 0.5f; f[6] = 0.75f;
        f[7] = 1.25f; f[8] = 0.5f; f[9] = 0.75f;
        break;
      }
      case 8: {
        // Every point the same: the set is a single point, dimension 0.
        n = 3;
        for (int i = 0; i < 3; ++i) {
          f[1 + 3*i + 0] = 0.5f;
          f[1 + 3*i + 1] = -0.25f;
          f[1 + 3*i + 2] = 0.125f;
        }
        break;
      }
      default: {
        // A generic point set in a cube: dimension 3, and the hull is not
        // known in advance - this is the draw that finds the disagreement
        // when the two sides' predicates differ.
        n = 6;
        for (int i = 0; i < 6; ++i) {
          for (int c = 0; c < 3; ++c) {
            f[1 + 3*i + c] = GenPlain(rng, 10.0f);
          }
        }
        break;
      }
      }
      in[0] = static_cast<std::uint32_t>(n);
    }

    void ConvexHull3Retail(const std::uint32_t* in, std::uint32_t* out)
    {
      const int n = static_cast<int>(in[0]);
      const float* points = reinterpret_cast<const float*>(in + 1);
      const float eps = difffuzz::FromBits(in[19]);
      const std::uint32_t type = in[20];

      // The retail's ConvexHull3<float> is 0x78 bytes: the hull's std::set
      // header sits at [this+0x68] and runs to [this+0x78].
      std::uint32_t storage[64];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::ConvexHull3Ctor(storage, n, points, eps, 0u, type);

      // The retail's member offsets are the rebuild's: the base class
      // ConvexHull<float> holds m_eQueryType at [this+0x04],
      // m_iVertexQuantity at [this+0x08], m_iDimension at [this+0x0C],
      // m_iSimplexQuantity at [this+0x10] and m_aiIndex at [this+0x14]; the
      // derived class holds m_akVertex at [this+0x20], m_akSVertex at
      // [this+0x24], m_pkQuery at [this+0x28], the line origin at
      // [this+0x2C], the line direction at [this+0x38], the plane origin at
      // [this+0x44] and the two plane directions at [this+0x50] and
      // [this+0x5C].
      const int dimension = static_cast<int>(storage[3]);
      const int quantity = static_cast<int>(storage[4]);
      const int* indices = reinterpret_cast<const int*>(storage[5]);
      float akLinePlane[15];
      std::memcpy(akLinePlane, storage + 11, sizeof akLinePlane);

      wm3collharness::ConvexHull3Canonical(dimension, quantity, indices,
                                           akLinePlane, out);

      // The retail's hull lives in the retail's heap; its destructor frees
      // the triangles, the scaled vertices and the query, so the run's
      // memory stays flat.
      wm3collretail::ConvexHull3Dtor(storage);
    }

    void ConvexHull3Ours(const std::uint32_t* in, std::uint32_t* out)
    {
      const int n = static_cast<int>(in[0]);
      const float* points = reinterpret_cast<const float*>(in + 1);
      const float eps = difffuzz::FromBits(in[19]);
      const std::uint32_t type = in[20];
      wm3collharness::ConvexHull3Ours(points, n, eps, type, out);
    }

    // ---- Wm3::Eigen<float> -------------------------------------------------
    //
    // The engine's Eigen input is a covariance matrix: symmetric, of
    // moderate magnitude.  The retail's solver is x87 code, so it keeps the
    // extended exponent where a float cannot, and the rebuild's float
    // arithmetic cannot follow it out of the float's range; the generator
    // therefore stays inside the range the engine's GaussPointsFit2/3 can
    // produce (the covariance of points whose coordinates are bounded by
    // the game world), which is also the range where the two agree bit for
    // bit.
    //
    // The shapes are chosen to reach the solver's branches:
    //   * a diagonal matrix - the eigenvalues are already known;
    //   * a matrix with a zero (0,2) element - the "already tridiagonal"
    //     branch of Tridiagonal3;
    //   * a matrix with a tiny but non-zero (0,2) element - the branch the
    //     rebuild's ZERO_TOLERANCE test used to miss;
    //   * a matrix with a negative (0,2) element of ordinary size;
    //   * a matrix with repeated eigenvalues;
    //   * the zero matrix.
    void GenEigenSym(difffuzz::Rng& rng, float* f, int n)
    {
      float ak[9];
      for (int i = 0; i < 9; ++i)
      {
        ak[i] = 0.0f;
      }

      const float fScale = rng.Chance(50) ? 1.0f : 10.0f;

      // the diagonal
      for (int i = 0; i < n; ++i)
      {
        ak[i*n + i] = difffuzz::GenPlain(rng, fScale);
      }
      // the upper triangle
      for (int i = 0; i < n; ++i)
      {
        for (int j = i + 1; j < n; ++j)
        {
          ak[i*n + j] = difffuzz::GenPlain(rng, fScale);
        }
      }

      switch (rng.Below(7))
      {
      case 0:
        // a diagonal matrix
        for (int i = 0; i < n; ++i)
        {
          for (int j = 0; j < n; ++j)
          {
            if (i != j)
            {
              ak[i*n + j] = 0.0f;
            }
          }
        }
        break;
      case 1:
        // the (0,2) element is exactly zero
        if (n == 3)
        {
          ak[2] = 0.0f;
        }
        break;
      case 2:
        // the (0,2) element is tiny but not zero: the rebuild's tolerance
        // test used to send this down the other branch
        if (n == 3)
        {
          ak[2] = 1e-8f * (rng.Chance(50) ? 1.0f : -1.0f);
        }
        break;
      case 3:
        // a negative (0,2) element of ordinary size
        if (n == 3)
        {
          ak[2] = -std::fabs(ak[2]);
        }
        break;
      case 4:
        // repeated eigenvalues
        for (int i = 1; i < n; ++i)
        {
          ak[i*n + i] = ak[0];
        }
        for (int i = 0; i < n; ++i)
        {
          for (int j = i + 1; j < n; ++j)
          {
            ak[i*n + j] = 0.0f;
          }
        }
        break;
      case 5:
        // the zero matrix
        for (int i = 0; i < 9; ++i)
        {
          ak[i] = 0.0f;
        }
        break;
      default:
        // a general symmetric matrix
        break;
      }

      // symmetrise
      for (int i = 0; i < n; ++i)
      {
        for (int j = i + 1; j < n; ++j)
        {
          ak[j*n + i] = ak[i*n + j];
        }
      }

      for (int i = 0; i < n*n; ++i)
      {
        f[i] = ak[i];
      }
    }

    void GenEigen3(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      GenEigenSym(rng, f, 3);
    }

    void GenEigen2(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      GenEigenSym(rng, f, 2);
    }

    // The same shapes for the double instantiation.  A double travels as two
    // words, so the matrix is written with StoreDouble.
    void GenEigenSymd(difffuzz::Rng& rng, std::uint32_t* in, int n)
    {
      double ak[9];
      for (int i = 0; i < 9; ++i)
      {
        ak[i] = 0.0;
      }

      const double fScale = rng.Chance(50) ? 1.0 : 10.0;

      for (int i = 0; i < n; ++i)
      {
        ak[i*n + i] = rng.Range(-fScale, fScale);
      }
      for (int i = 0; i < n; ++i)
      {
        for (int j = i + 1; j < n; ++j)
        {
          ak[i*n + j] = rng.Range(-fScale, fScale);
        }
      }

      switch (rng.Below(7))
      {
      case 0:
        for (int i = 0; i < n; ++i)
        {
          for (int j = 0; j < n; ++j)
          {
            if (i != j)
            {
              ak[i*n + j] = 0.0;
            }
          }
        }
        break;
      case 1:
        if (n == 3)
        {
          ak[2] = 0.0;
        }
        break;
      case 2:
        if (n == 3)
        {
          // tiny but not zero: the branch the rebuild's tolerance test
          // used to miss
          ak[2] = 1e-8 * (rng.Chance(50) ? 1.0 : -1.0);
        }
        break;
      case 3:
        if (n == 3)
        {
          ak[2] = -std::fabs(ak[2]);
        }
        break;
      case 4:
        for (int i = 1; i < n; ++i)
        {
          ak[i*n + i] = ak[0];
        }
        for (int i = 0; i < n; ++i)
        {
          for (int j = i + 1; j < n; ++j)
          {
            ak[i*n + j] = 0.0;
          }
        }
        break;
      case 5:
        for (int i = 0; i < 9; ++i)
        {
          ak[i] = 0.0;
        }
        break;
      default:
        break;
      }

      for (int i = 0; i < n; ++i)
      {
        for (int j = i + 1; j < n; ++j)
        {
          ak[j*n + i] = ak[i*n + j];
        }
      }

      for (int i = 0; i < n*n; ++i)
      {
        difffuzz::StoreDouble(in + 2*i, ak[i]);
      }
    }

    void GenEigen3d(difffuzz::Rng& rng, std::uint32_t* in)
    {
      GenEigenSymd(rng, in, 3);
    }

    void GenEigen2d(difffuzz::Rng& rng, std::uint32_t* in)
    {
      GenEigenSymd(rng, in, 2);
    }

    // The retail's Eigen<float> is 0x24 bytes: m_iSize at [this+0x00], the
    // GMatrix m_kMat at [this+0x04] (its row-pointer array at
    // [this+0x14]), m_afDiag at [this+0x18], m_afSubd at [this+0x1C] and
    // the m_bIsRotation flag at [this+0x20].
    constexpr int kEigenObjectBytes = 0x28;

    void EigenfSetup(unsigned char* storage, const float* m, int n)
    {
      std::memset(storage, 0, kEigenObjectBytes);
      wm3collretail::EigenfCtor(storage, n);
      for (int r = 0; r < n; ++r)
      {
        for (int c = 0; c < n; ++c)
        {
          *wm3collretail::EigenfElement(storage, r, c) = m[r*n + c];
        }
      }
    }

    void EigenfIncrSort3Retail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kEigenObjectBytes];
      const float* m = reinterpret_cast<const float*>(in);
      EigenfSetup(storage, m, 3);
      wm3collretail::EigenfIncrSort3(storage);

      const float* akDiag =
          *reinterpret_cast<const float* const*>(storage + 0x18);
      for (int i = 0; i < 3; ++i)
      {
        out[i] = difffuzz::Bits(akDiag[i]);
      }
      float* const* apkRow =
          *reinterpret_cast<float* const* const*>(storage + 0x14);
      for (int r = 0; r < 3; ++r)
      {
        for (int c = 0; c < 3; ++c)
        {
          out[3 + r*3 + c] = difffuzz::Bits(apkRow[r][c]);
        }
      }

      wm3collretail::EigenfDtor(storage);
    }

    void EigenfIncrSort3Ours(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* m = reinterpret_cast<const float*>(in);
      wm3collharness::EigenfIncrSortOurs(m, 3, out);
    }

    void EigenfIncrSort2Retail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kEigenObjectBytes];
      const float* m = reinterpret_cast<const float*>(in);
      EigenfSetup(storage, m, 2);
      wm3collretail::EigenfIncrSort2(storage);

      const float* akDiag =
          *reinterpret_cast<const float* const*>(storage + 0x18);
      for (int i = 0; i < 2; ++i)
      {
        out[i] = difffuzz::Bits(akDiag[i]);
      }
      float* const* apkRow =
          *reinterpret_cast<float* const* const*>(storage + 0x14);
      for (int r = 0; r < 2; ++r)
      {
        for (int c = 0; c < 2; ++c)
        {
          out[2 + r*2 + c] = difffuzz::Bits(apkRow[r][c]);
        }
      }

      wm3collretail::EigenfDtor(storage);
    }

    void EigenfIncrSort2Ours(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* m = reinterpret_cast<const float*>(in);
      wm3collharness::EigenfIncrSortOurs(m, 2, out);
    }

    void EigenfGetEigenvaluesRetail(const std::uint32_t* in,
                                    std::uint32_t* out)
    {
      unsigned char storage[kEigenObjectBytes];
      const float* m = reinterpret_cast<const float*>(in);
      EigenfSetup(storage, m, 3);
      wm3collretail::EigenfIncrSort3(storage);
      for (int i = 0; i < 3; ++i)
      {
        out[i] = difffuzz::Bits(
            wm3collretail::EigenfGetEigenvalue(storage, i));
      }
      wm3collretail::EigenfDtor(storage);
    }

    void EigenfGetEigenvaluesOurs(const std::uint32_t* in,
                                   std::uint32_t* out)
    {
      const float* m = reinterpret_cast<const float*>(in);
      wm3collharness::EigenfGetEigenvaluesOurs(m, 3, out);
    }

    void EigenfGetEigenvectorsRetail(const std::uint32_t* in,
                                     std::uint32_t* out)
    {
      unsigned char storage[kEigenObjectBytes];
      const float* m = reinterpret_cast<const float*>(in);
      EigenfSetup(storage, m, 3);
      wm3collretail::EigenfIncrSort3(storage);
      float akV[3];
      for (int i = 0; i < 3; ++i)
      {
        wm3collretail::EigenfGetEigenvector3(storage, i, akV);
        out[3*i + 0] = difffuzz::Bits(akV[0]);
        out[3*i + 1] = difffuzz::Bits(akV[1]);
        out[3*i + 2] = difffuzz::Bits(akV[2]);
      }
      wm3collretail::EigenfDtor(storage);
    }

    void EigenfGetEigenvectorsOurs(const std::uint32_t* in,
                                    std::uint32_t* out)
    {
      const float* m = reinterpret_cast<const float*>(in);
      wm3collharness::EigenfGetEigenvectorsOurs(m, 3, out);
    }

    void EigenfGetEigenvectors2Retail(const std::uint32_t* in,
                                      std::uint32_t* out)
    {
      unsigned char storage[kEigenObjectBytes];
      const float* m = reinterpret_cast<const float*>(in);
      EigenfSetup(storage, m, 2);
      wm3collretail::EigenfIncrSort2(storage);
      float akV[2];
      for (int i = 0; i < 2; ++i)
      {
        wm3collretail::EigenfGetEigenvector2(storage, i, akV);
        out[2*i + 0] = difffuzz::Bits(akV[0]);
        out[2*i + 1] = difffuzz::Bits(akV[1]);
      }
      wm3collretail::EigenfDtor(storage);
    }

    void EigenfGetEigenvectors2Ours(const std::uint32_t* in,
                                     std::uint32_t* out)
    {
      const float* m = reinterpret_cast<const float*>(in);
      wm3collharness::EigenfGetEigenvectors2Ours(m, out);
    }

    // ---- Wm3::Eigen<double> -------------------------------------------------
    //
    // The retail's Eigen<double> has the retail's layout: the eigenvalues
    // are at [this+0x18] and the matrix rows through the GMatrix's
    // row-pointer array at [this+0x14].
    void EigendSetup(unsigned char* storage, const double* m, int n)
    {
      std::memset(storage, 0, kEigenObjectBytes);
      wm3collretail::EigendCtor(storage, n);
      for (int r = 0; r < n; ++r)
      {
        for (int c = 0; c < n; ++c)
        {
          *wm3collretail::EigendElement(storage, r, c) = m[r*n + c];
        }
      }
    }

    void EigendIncrSort3Retail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kEigenObjectBytes];
      const double* m = reinterpret_cast<const double*>(in);
      EigendSetup(storage, m, 3);
      wm3collretail::EigendIncrSort3(storage);

      const double* akDiag =
          *reinterpret_cast<const double* const*>(storage + 0x18);
      for (int i = 0; i < 3; ++i)
      {
        difffuzz::StoreDouble(out + 2*i, akDiag[i]);
      }
      double* const* apkRow =
          *reinterpret_cast<double* const* const*>(storage + 0x14);
      for (int r = 0; r < 3; ++r)
      {
        for (int c = 0; c < 3; ++c)
        {
          difffuzz::StoreDouble(out + 6 + 2*(r*3 + c), apkRow[r][c]);
        }
      }

      wm3collretail::EigendDtor(storage);
    }

    void EigendIncrSort3Ours(const std::uint32_t* in, std::uint32_t* out)
    {
      const double* m = reinterpret_cast<const double*>(in);
      wm3collharness::EigendIncrSortOurs(m, 3, out);
    }

    void EigendIncrSort2Retail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kEigenObjectBytes];
      const double* m = reinterpret_cast<const double*>(in);
      EigendSetup(storage, m, 2);
      wm3collretail::EigendIncrSort2(storage);

      const double* akDiag =
          *reinterpret_cast<const double* const*>(storage + 0x18);
      for (int i = 0; i < 2; ++i)
      {
        difffuzz::StoreDouble(out + 2*i, akDiag[i]);
      }
      double* const* apkRow =
          *reinterpret_cast<double* const* const*>(storage + 0x14);
      for (int r = 0; r < 2; ++r)
      {
        for (int c = 0; c < 2; ++c)
        {
          difffuzz::StoreDouble(out + 4 + 2*(r*2 + c), apkRow[r][c]);
        }
      }

      wm3collretail::EigendDtor(storage);
    }

    void EigendIncrSort2Ours(const std::uint32_t* in, std::uint32_t* out)
    {
      const double* m = reinterpret_cast<const double*>(in);
      wm3collharness::EigendIncrSortOurs(m, 2, out);
    }

    void EigendGetEigenvaluesRetail(const std::uint32_t* in,
                                    std::uint32_t* out)
    {
      unsigned char storage[kEigenObjectBytes];
      const double* m = reinterpret_cast<const double*>(in);
      EigendSetup(storage, m, 3);
      wm3collretail::EigendIncrSort3(storage);
      for (int i = 0; i < 3; ++i)
      {
        difffuzz::StoreDouble(out + 2*i,
            wm3collretail::EigendGetEigenvalue(storage, i));
      }
      wm3collretail::EigendDtor(storage);
    }

    void EigendGetEigenvaluesOurs(const std::uint32_t* in,
                                   std::uint32_t* out)
    {
      const double* m = reinterpret_cast<const double*>(in);
      wm3collharness::EigendGetEigenvaluesOurs(m, 3, out);
    }

    void EigendGetEigenvectorsRetail(const std::uint32_t* in,
                                     std::uint32_t* out)
    {
      unsigned char storage[kEigenObjectBytes];
      const double* m = reinterpret_cast<const double*>(in);
      EigendSetup(storage, m, 3);
      wm3collretail::EigendIncrSort3(storage);
      double akV[3];
      for (int i = 0; i < 3; ++i)
      {
        wm3collretail::EigendGetEigenvector3(storage, i, akV);
        difffuzz::StoreDouble(out + 6*i + 0, akV[0]);
        difffuzz::StoreDouble(out + 6*i + 2, akV[1]);
        difffuzz::StoreDouble(out + 6*i + 4, akV[2]);
      }
      wm3collretail::EigendDtor(storage);
    }

    void EigendGetEigenvectorsOurs(const std::uint32_t* in,
                                    std::uint32_t* out)
    {
      const double* m = reinterpret_cast<const double*>(in);
      wm3collharness::EigendGetEigenvectorsOurs(m, 3, out);
    }

    void EigendGetEigenvectors2Retail(const std::uint32_t* in,
                                      std::uint32_t* out)
    {
      unsigned char storage[kEigenObjectBytes];
      const double* m = reinterpret_cast<const double*>(in);
      EigendSetup(storage, m, 2);
      wm3collretail::EigendIncrSort2(storage);
      double akV[2];
      for (int i = 0; i < 2; ++i)
      {
        wm3collretail::EigendGetEigenvector2(storage, i, akV);
        difffuzz::StoreDouble(out + 4*i + 0, akV[0]);
        difffuzz::StoreDouble(out + 4*i + 2, akV[1]);
      }
      wm3collretail::EigendDtor(storage);
    }

    void EigendGetEigenvectors2Ours(const std::uint32_t* in,
                                     std::uint32_t* out)
    {
      const double* m = reinterpret_cast<const double*>(in);
      wm3collharness::EigendGetEigenvectors2Ours(m, out);
    }

    // ---- Wm3::Query3<float>::ToPlane --------------------------------------
    //
    // The input is four vertices (twelve floats), the point (three floats)
    // and the three vertex indices (three words): eighteen words.
    //
    // The generator builds the configurations where the sign is decided by
    // the last bit of the determinant: a point exactly on the plane (the
    // determinant is exactly zero), a point one ulp off the plane, a
    // degenerate triangle (two of the three vertices equal) and a general
    // point away from the plane.
    void GenQuery3ToPlane(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      for (int i = 0; i < 12; ++i) {
        f[i] = 0.0f;
      }
      for (int i = 0; i < 3; ++i) {
        in[15 + i] = static_cast<std::uint32_t>(i);
      }

      switch (rng.Below(6)) {
      case 0: {
        // The unit triangle in the z = 0 plane and a point exactly on it:
        // the determinant is exactly zero on both sides.
        f[0] = 0.0f; f[1] = 0.0f; f[2] = 0.0f;
        f[3] = 1.0f; f[4] = 0.0f; f[5] = 0.0f;
        f[6] = 0.0f; f[7] = 1.0f; f[8] = 0.0f;
        f[9] = 0.0f; f[10] = 0.0f; f[11] = 1.0f;
        f[12] = 0.5f; f[13] = 0.5f; f[14] = 0.0f;
        break;
      }
      case 1: {
        // The same triangle, but the point is one ulp off the plane: the
        // determinant is a denormal-sized number and its sign is the whole
        // answer.
        f[0] = 0.0f; f[1] = 0.0f; f[2] = 0.0f;
        f[3] = 1.0f; f[4] = 0.0f; f[5] = 0.0f;
        f[6] = 0.0f; f[7] = 1.0f; f[8] = 0.0f;
        f[9] = 0.0f; f[10] = 0.0f; f[11] = 1.0f;
        f[12] = 0.5f; f[13] = 0.5f;
        f[14] = std::numeric_limits<float>::denorm_min();
        break;
      }
      case 2: {
        // A degenerate triangle: the first two vertices are the same, so
        // the determinant is exactly zero whatever the point is.
        f[0] = 0.25f; f[1] = 0.5f; f[2] = -0.75f;
        f[3] = 0.25f; f[4] = 0.5f; f[5] = -0.75f;
        f[6] = 1.25f; f[7] = -0.5f; f[8] = 0.25f;
        f[9] = 0.0f; f[10] = 0.0f; f[11] = 1.0f;
        f[12] = GenPlain(rng, 10.0f);
        f[13] = GenPlain(rng, 10.0f);
        f[14] = GenPlain(rng, 10.0f);
        break;
      }
      case 3: {
        // The point is one of the vertices: the determinant is exactly
        // zero.
        f[0] = 0.0f; f[1] = 0.0f; f[2] = 0.0f;
        f[3] = 1.0f; f[4] = 0.0f; f[5] = 0.0f;
        f[6] = 0.0f; f[7] = 1.0f; f[8] = 0.0f;
        f[9] = 0.0f; f[10] = 0.0f; f[11] = 1.0f;
        f[12] = f[3]; f[13] = f[4]; f[14] = f[5];
        break;
      }
      case 4: {
        // A general tetrahedron and a general point.
        for (int i = 0; i < 12; ++i) {
          f[i] = GenPlain(rng, 10.0f);
        }
        for (int i = 0; i < 3; ++i) {
          f[12 + i] = GenPlain(rng, 10.0f);
        }
        break;
      }
      default: {
        // A general tetrahedron and a point on its face plane, built in
        // exact arithmetic so that the determinant is zero in the
        // mathematical sense but not necessarily in the bits.
        f[0] = GenPlain(rng, 10.0f);
        f[1] = GenPlain(rng, 10.0f);
        f[2] = GenPlain(rng, 10.0f);
        f[3] = GenPlain(rng, 10.0f);
        f[4] = GenPlain(rng, 10.0f);
        f[5] = GenPlain(rng, 10.0f);
        f[6] = GenPlain(rng, 10.0f);
        f[7] = GenPlain(rng, 10.0f);
        f[8] = GenPlain(rng, 10.0f);
        f[9] = 0.0f; f[10] = 0.0f; f[11] = 1.0f;
        // A point of the form V0 + a*(V1-V0) + b*(V2-V0).
        const float a = static_cast<float>(rng.Range(-2.0, 2.0));
        const float b = static_cast<float>(rng.Range(-2.0, 2.0));
        for (int c = 0; c < 3; ++c) {
          f[12 + c] = f[c] + a*(f[3 + c] - f[c]) + b*(f[6 + c] - f[c]);
        }
        break;
      }
      }
    }

    void Query3ToPlaneRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* verts = reinterpret_cast<const float*>(in);
      const float* point = reinterpret_cast<const float*>(in + 12);
      const int iV0 = static_cast<int>(in[15]);
      const int iV1 = static_cast<int>(in[16]);
      const int iV2 = static_cast<int>(in[17]);

      // The retail builds this class inline (the ConvexHull3<float>
      // constructor at 0x00A67433..0x00A67442 writes the vtable pointer,
      // the vertex count and the vertex array), so the object is built the
      // same way here: the vtable at [this+0x00], the count at [this+0x04]
      // and the array at [this+0x08].
      std::uint32_t storage[4];
      storage[0] = 0x00D71F38u;         // the class vtable
      storage[1] = 4u;                  // the vertex count
      storage[2] = reinterpret_cast<std::uint32_t>(verts);
      storage[3] = 0u;

      const int iSign = wm3collretail::Query3ToPlane(storage, point, iV0, iV1,
                                                      iV2);
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    void Query3ToPlaneOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* verts = reinterpret_cast<const float*>(in);
      const float* point = reinterpret_cast<const float*>(in + 12);
      const int iV0 = static_cast<int>(in[15]);
      const int iV1 = static_cast<int>(in[16]);
      const int iV2 = static_cast<int>(in[17]);
      const int iSign = wm3collharness::Query3ToPlaneOurs(verts, 4, point, iV0,
                                                           iV1, iV2);
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    // line[6] (Origin[3], Direction[3]) then box[15] - twenty-one floats.
    //
    // The Find row needs a box the algorithm can actually clip against, so
    // the box gets an orthonormal axis and positive extents, and the line is
    // placed through the box's neighbourhood: the origin is drawn around the
    // centre and the direction is a unit vector, so "hits the box" and
    // "misses the box" are both common.
    //
    // One draw in twenty is the degenerate one - a zero direction. That is
    // the only geometry that leaves the line unclipped, and therefore the
    // only one that reaches the bSolid branch of DoClipping: with bSolid the
    // retail reports IT_SEGMENT with the unclipped -MAX_REAL / +MAX_REAL
    // parameters.
    //
    // One draw in ten is the exact tangency, built in the box's own frame so
    // the arithmetic is exact: the box is the unit cube at the origin and the
    // line starts at (2,0,0) along (1,1,0). In the box's coordinates that is
    // o = (2,0,0) and d = (1,1,0), so the x slab gives [-3,-1] and the y slab
    // gives [-1,1]: they touch at t = -1 exactly, and fT0 == fT1 is what makes
    // the retail take the IT_POINT branch. A random near-tangent draw would be
    // a few ulp away and would take the IT_SEGMENT branch instead, so the
    // IT_POINT path is otherwise never reached.
    void GenIntrLine3Box3Find(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);

      if (rng.Chance(10)) {
        f[0] = 2.0f; f[1] = 0.0f; f[2] = 0.0f;      // the line's origin
        f[3] = 1.0f; f[4] = 1.0f; f[5] = 0.0f;      // the line's direction
        f[6] = f[7] = f[8] = 0.0f;                  // the box's centre
        f[9]  = 1.0f; f[10] = 0.0f; f[11] = 0.0f;   // axis 0
        f[12] = 0.0f; f[13] = 1.0f; f[14] = 0.0f;   // axis 1
        f[15] = 0.0f; f[16] = 0.0f; f[17] = 1.0f;   // axis 2
        f[18] = f[19] = f[20] = 1.0f;               // the extents
        return;
      }

      // the box: the centre, an orthonormal axis and positive extents
      f[6] = GenPlain(rng, 20.0f);
      f[7] = GenPlain(rng, 20.0f);
      f[8] = GenPlain(rng, 20.0f);
      difffuzz::GenUnitVec3(rng, f + 9);            // axis 0
      MakePerp3(f + 9, f + 12);                     // axis 1, perpendicular to axis 0
      f[15] = f[10]*f[14] - f[11]*f[13];            // axis 2 = axis 0 x axis 1
      f[16] = f[11]*f[12] - f[9]*f[14];
      f[17] = f[9]*f[13] - f[10]*f[12];
      f[18] = static_cast<float>(rng.Range(0.5, 10.0));   // extent x, positive
      f[19] = static_cast<float>(rng.Range(0.5, 10.0));   // extent y, positive
      f[20] = static_cast<float>(rng.Range(0.5, 10.0));   // extent z, positive

      // the line: the origin around the centre, the direction a unit vector
      for (int i = 0; i < 3; ++i) {
        f[i] = f[6 + i] + GenPlain(rng, 20.0f);
      }
      difffuzz::GenUnitVec3(rng, f + 3);
      if (rng.Chance(5)) {
        // The degenerate direction: the line cannot be clipped at all, which
        // is the bSolid branch. The origin is put inside the box so the branch
        // is actually taken instead of the line simply missing.
        f[3] = f[4] = f[5] = 0.0f;
        for (int i = 0; i < 3; ++i) {
          f[i] = f[6 + i] + GenPlain(rng, 0.5f*f[18 + i]);
        }
      }
    }

    // The intersection set is only defined after a successful Find: the
    // quantity is zero and there is no point on the miss path, so both sides
    // write zero there. The retail's members are read out of its object
    // storage: m_iIntersectionType at [this+0x08], m_iQuantity at [this+0x14]
    // and the two points at [this+0x18] and [this+0x24].
    void IntrLine3Box3FindRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrLine3Box3Ctor(storage, f, f + 6);
      const std::uint32_t hit = wm3collretail::IntrLine3Box3Find(storage);
      std::uint32_t quantity = 0u;
      std::memcpy(&quantity, storage + 0x14, sizeof quantity);
      out[0] = hit;
      std::memcpy(out + 1, storage + 0x08, sizeof(std::uint32_t));  // the type
      out[2] = quantity;
      out[3] = out[4] = out[5] = out[6] = out[7] = out[8] = 0u;
      if (quantity > 0) {
        std::memcpy(out + 3, storage + 0x18, 3*sizeof(float));
      }
      if (quantity > 1) {
        std::memcpy(out + 6, storage + 0x24, 3*sizeof(float));
      }
    }

    void IntrLine3Box3FindOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      wm3collharness::IntrLine3Box3Find(f, f + 6, out);
    }

    void DistVector2Box2GetRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistVector2Box2Ctor(storage, f, f + 2);
      float value = 0.0f;
      wm3collretail::DistVector2Box2Get(storage, &value);
      out[0] = difffuzz::Bits(value);
    }

    void DistVector2Box2GetOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      wm3collharness::DistVector2Box2Get(f, f + 2, out);
    }

    // The point is only defined after a successful Find - the retail's Find
    // writes the point array on the hit path only - so on the miss path both
    // sides write zero. The retail's own accessor is called, so this row
    // compares the accessor body rather than a raw memory read.
    void IntrSegment3Capsule3GetPointRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrSegment3Capsule3Ctor(storage, f, f + 7);
      out[0] = out[1] = out[2] = 0u;
      if (!wm3collretail::IntrSegment3Capsule3Find(storage)) {
        return;
      }
      const float* point = wm3collretail::IntrSegment3Capsule3GetPoint(storage, 0);
      out[0] = difffuzz::Bits(point[0]);
      out[1] = difffuzz::Bits(point[1]);
      out[2] = difffuzz::Bits(point[2]);
    }

    void IntrSegment3Capsule3GetPointOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      wm3collharness::IntrSegment3Capsule3GetPoint(f, f + 7, out);
    }

    void IntrSegment3Capsule3GetParameterRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrSegment3Capsule3Ctor(storage, f, f + 7);
      out[0] = 0u;
      if (!wm3collretail::IntrSegment3Capsule3Find(storage)) {
        return;
      }
      float value = 0.0f;
      wm3collretail::IntrSegment3Capsule3GetParameter(storage, 0, &value);
      out[0] = difffuzz::Bits(value);
    }

    void IntrSegment3Capsule3GetParameterOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      wm3collharness::IntrSegment3Capsule3GetParameter(f, f + 7, out);
    }

    // vec[3] then segment[7], then fT, then the two velocities - seventeen
    // floats. The velocities are on the scale of the shapes, so the swept
    // query is a real motion rather than a jump, and the time is drawn around
    // zero because the derivative is a central difference around it.
    //
    // The finite retry is the one the static Get row uses: the retail's swept
    // Get takes the CRT's square root, and a non-finite argument is a fatal
    // math error in the retail as well as in the harness.
    void GenDistVector3Segment3Swept(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      if (rng.Chance(15)) {
        // The denormal range: the point is a few times 1e-19 off the
        // segment's line, so the squared distance is around 1e-38 and lives
        // on the float denormal grid.  The time and the velocities are
        // zero, so the *moved* pair is exactly the pair below and the
        // relative geometry - the one that decides the squared distance -
        // is untouched by the sweep.
        GenDistVector3Segment3Denormal(rng, in);
        f[10] = 0.0f;                          // fT
        for (int i = 11; i < 17; ++i) {
          f[i] = 0.0f;                         // the velocities
        }
        return;
      }
      for (int attempt = 0; attempt < 32; ++attempt) {
        GenDistVector3Segment3(rng, in);
        const float squared = wm3collharness::DistVector3Segment3GetSquared(f, f + 3);
        const std::uint32_t bits = difffuzz::Bits(squared);
        if ((bits & 0x7F800000u) != 0x7F800000u) {
          break;
        }
      }
      f[10] = GenPlain(rng, 10.0f);          // fT
      for (int i = 0; i < 3; ++i) {
        f[11 + i] = GenPlain(rng, 5.0f);     // the velocity of the point
        f[14 + i] = GenPlain(rng, 5.0f);     // the velocity of the segment
      }
    }

    void DistVector3Segment3GetDerivativeRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistVector3Segment3Ctor(storage, f, f + 3);
      float value = 0.0f;
      wm3collretail::DistanceVector3GetDerivative(storage, f[10], f + 11, f + 14, &value);
      out[0] = difffuzz::Bits(value);
    }

    void DistVector3Segment3GetDerivativeOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(
        wm3collharness::DistVector3Segment3GetDerivative(f, f + 3, f[10], f + 11, f + 14));
    }

    void DistVector3Segment3GetDerivativeSquaredRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistVector3Segment3Ctor(storage, f, f + 3);
      float value = 0.0f;
      wm3collretail::DistanceVector3GetDerivativeSquared(storage, f[10], f + 11, f + 14, &value);
      out[0] = difffuzz::Bits(value);
    }

    void DistVector3Segment3GetDerivativeSquaredOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(
        wm3collharness::DistVector3Segment3GetDerivativeSquared(f, f + 3, f[10], f + 11, f + 14));
    }

    void IntrSegment3Box3TestRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrSegment3Box3Ctor(storage, f, f + 7, in[25]);
      out[0] = wm3collretail::IntrSegment3Box3Test(storage);
    }

    void IntrSegment3Box3TestOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = wm3collharness::IntrSegment3Box3Test(f, f + 7, in[25]);
    }

    // point[3] then box[15] - eighteen floats.
    //
    // The box gets an orthonormal axis and positive extents, and the point is
    // drawn around the centre and across the extents, so "inside", "outside"
    // and "just off a face" are all common. One draw in three puts the point
    // exactly on one of the faces, which is the case the comparison can turn
    // on.
    void GenInBox(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);

      f[3] = GenPlain(rng, 20.0f);                 // the box's centre
      f[4] = GenPlain(rng, 20.0f);
      f[5] = GenPlain(rng, 20.0f);
      difffuzz::GenUnitVec3(rng, f + 6);            // axis 0
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

    void DistVector3Segment3GetSweptRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistVector3Segment3Ctor(storage, f, f + 3);
      float value = 0.0f;
      wm3collretail::DistVector3Segment3Get(storage, f[10], f + 11, f + 14, &value);
      out[0] = difffuzz::Bits(value);
    }

    void DistVector3Segment3GetSweptOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(
        wm3collharness::DistVector3Segment3Get(f, f + 3, f[10], f + 11, f + 14));
    }

    void DistVector3Segment3GetSquaredSweptRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistVector3Segment3Ctor(storage, f, f + 3);
      float value = 0.0f;
      wm3collretail::DistVector3Segment3GetSquared(storage, f[10], f + 11, f + 14, &value);
      out[0] = difffuzz::Bits(value);
    }

    void DistVector3Segment3GetSquaredSweptOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(
        wm3collharness::DistVector3Segment3GetSquared(f, f + 3, f[10], f + 11, f + 14));
    }

    // count[1] then points[8] - twenty-five words.
    //
    // The count is drawn first and the points follow, so the input reads in
    // the same order as the call. The points are on a modest scale and the
    // count is sometimes one, which makes the box degenerate - the centre
    // and the extent then collapse and the comparison is decided entirely by
    // the rounding of the difference.
    void GenContAlignedBox(difffuzz::Rng& rng, std::uint32_t* in)
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
        out[i] = difffuzz::Bits(afBox[i]);
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
    void GenContAlignedBoxMask(difffuzz::Rng& rng, std::uint32_t* in)
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
        out[i] = difffuzz::Bits(afBox[i]);
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

    // ---- the 2D ContAlignedBox --------------------------------------------
    //
    // count[1] then points[16] - seventeen words, the 3D query in two
    // dimensions: the count is drawn first and the points follow, so the
    // input reads in the same order as the call. The count is sometimes
    // one, which makes the box degenerate - the centre and the extent then
    // collapse and the comparison is decided entirely by the rounding of
    // the difference.
    void GenContAlignedBox2(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      in[0] = static_cast<std::uint32_t>(1 + rng.Below(8));
      for (int i = 0; i < 16; ++i) {
        f[1 + i] = GenPlain(rng, 10.0f);
      }
    }

    void ContAlignedBox2Retail(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      float afBox[8] = { 0.0f };
      wm3collretail::ContAlignedBox2(afBox, static_cast<int>(in[0]), f + 1);
      for (int i = 0; i < 8; ++i) {
        out[i] = difffuzz::Bits(afBox[i]);
      }
    }

    void ContAlignedBox2Ours(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      wm3collharness::ContAlignedBox2(static_cast<int>(in[0]), f + 1, out);
    }

    // count[1] mode[1] points[16] mask[8 bytes] - twenty words.
    //
    // The same mode word as the 3D masked query selects the mask shape:
    //   0 - no mask at all (the retail's fourth argument is null, so it takes
    //       the unmasked path);
    //   1 - every point valid;
    //   2 - some points invalid (at least one valid and, when the count
    //       allows it, at least one invalid among the first `count` entries);
    //   3 - no point valid at all, which is the retail's "-1 extents" case;
    //   4 - nonzero-but-not-one bytes, which the retail counts as valid.
    //
    // The mask is eight bytes, one per point, packed into the two words that
    // follow the points. A nonzero byte means "keep this point" - the retail
    // tests the mask a byte at a time - and the same bytes are handed to our
    // side as the `const bool*` the Wild Magic 3.4 signature takes, so both
    // sides read exactly the same mask.
    void GenContAlignedBox2Mask(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      const int iCount = 1 + static_cast<int>(rng.Below(8));
      const int iMode = static_cast<int>(rng.Below(5));
      in[0] = static_cast<std::uint32_t>(iCount);
      in[1] = static_cast<std::uint32_t>(iMode);

      for (int i = 0; i < 16; ++i) {
        f[2 + i] = GenPlain(rng, 10.0f);
      }
      // Duplicate a point now and then: an exact tie is a boundary the
      // comparisons must agree on, and it makes the box degenerate.
      if (rng.Chance(40)) {
        const int iSrc = static_cast<int>(rng.Below(8));
        const int iDst = static_cast<int>(rng.Below(8));
        for (int k = 0; k < 2; ++k) {
          f[2 + 2*iDst + k] = f[2 + 2*iSrc + k];
        }
      }
      // And a zero point now and then: the extents then collapse onto zero.
      if (rng.Chance(20)) {
        const int iZero = static_cast<int>(rng.Below(8));
        for (int k = 0; k < 2; ++k) {
          f[2 + 2*iZero + k] = 0.0f;
        }
      }

      unsigned char* mask = reinterpret_cast<unsigned char*>(in + 18);
      for (int i = 0; i < 8; ++i) {
        mask[i] = 1;
      }
      if (iMode == 2) {
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

    void ContAlignedBox2MaskRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      const int iCount = static_cast<int>(in[0]);
      const bool bMasked = in[1] != 0;
      const unsigned char* mask = reinterpret_cast<const unsigned char*>(in + 18);
      float afBox[8] = { 0.0f };
      wm3collretail::ContAlignedBox2Mask(afBox, iCount, f + 2,
          bMasked ? mask : nullptr);
      for (int i = 0; i < 8; ++i) {
        out[i] = difffuzz::Bits(afBox[i]);
      }
    }

    void ContAlignedBox2MaskOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      const int iCount = static_cast<int>(in[0]);
      const bool bMasked = in[1] != 0;
      const bool* mask = reinterpret_cast<const bool*>(in + 18);
      wm3collharness::ContAlignedBox2Mask(iCount, f + 2,
          bMasked ? mask : nullptr, out);
    }

    // ---- IntrBox2Circle2's two region helpers ----------------------------
    //
    // The two helpers work in the box's own frame: fCx and fCy are the
    // circle centre's coordinates there, fVx and fVy the relative velocity
    // in the same frame, and fEx and fEy the box's extents. The retail's
    // Find hands the vertex helper a centre that is beyond the corner
    // (-fEx,-fEy) in both directions - the other regions arrive with the
    // signs flipped - and the edge helper a centre that is beyond the edge
    // (fCx <= -fEx) with its coordinate along the edge inside the edge's
    // extent.
    //
    // The generators below construct that geometry exactly, so the helper's
    // own comparisons decide the answer:
    //
    //   vertex: the distance to the corner is compared against the radius
    //           (the circle that covers the corner answers -1), the dot
    //           product of the velocity and the vector to the corner is
    //           compared against zero (a circle that is not moving towards
    //           the box answers 0), the cross product decides which of the
    //           two edges is reached first, and the distance from the
    //           corner to the line of motion decides whether the circle
    //           misses the box altogether.
    //   edge:   the signed distance from the circle centre to the edge is
    //           compared against the radius (a circle that reaches the edge
    //           answers -1), the velocity's x component against zero (not
    //           moving towards the edge answers 0), and the distance from
    //           the edge's endpoint to the line of motion decides between a
    //           hit on the edge and a hit on the corner.
    //
    // The box and the circle in the input are the ones the retail's
    // constructor is given; the helpers read only the circle's radius out of
    // the object, so the box is a well-formed one (the identity axis and
    // positive extents) and nothing more.
    void GenIntrBox2Circle2Region(difffuzz::Rng& rng, std::uint32_t* in,
                                   float ex, float ey, float r, float cx,
                                   float cy, float vx, float vy)
    {
      float* f = reinterpret_cast<float*>(in);
      f[0] = 0.0f; f[1] = 0.0f;      // the box's centre
      f[2] = 1.0f; f[3] = 0.0f;      // the axis: the identity
      f[4] = 0.0f; f[5] = 1.0f;
      f[6] = ex; f[7] = ey;          // the extents
      f[8] = 0.0f; f[9] = 0.0f;      // the circle's centre (not read)
      f[10] = r;                     // the radius
      f[11] = cx; f[12] = cy;        // fCx, fCy
      f[13] = vx; f[14] = vy;        // fVx, fVy
      f[15] = ex; f[16] = ey;        // fEx, fEy
    }

    // The vertex region: the circle centre is beyond the corner (-fEx,-fEy).
    void GenIntrBox2Circle2RegionVertex(difffuzz::Rng& rng, std::uint32_t* in)
    {
      const float ex = rng.Range(0.5f, 8.0f);
      const float ey = rng.Range(0.5f, 8.0f);
      const float r = rng.Range(0.25f, 4.0f);
      const std::uint32_t mode = rng.Below(100);

      float a = 0.0f;   // the distance in x from the corner
      float b = 0.0f;   // the distance in y from the corner
      float vx = 0.0f;
      float vy = 0.0f;

      if (mode < 15) {
        // The circle already covers the corner: its distance to the corner
        // is not greater than the radius. That is the helper's -1.
        a = r * rng.Range(0.0f, 0.9f);
        b = r * rng.Range(0.0f, 0.5f);
        vx = rng.Range(-2.0f, 2.0f);
        vy = rng.Range(-2.0f, 2.0f);
      }
      else if (mode < 25) {
        // Exactly on that boundary: the distance to the corner is the
        // radius, so the helper's fDiff is exactly zero.
        a = r;
        b = 0.0f;
        vx = rng.Range(-2.0f, 2.0f);
        vy = rng.Range(-2.0f, 2.0f);
      }
      else if (mode < 38) {
        // Not moving towards the box: the dot product of the velocity and
        // the vector to the corner is not negative. That is the helper's 0.
        a = r + rng.Range(0.0f, 4.0f);
        b = rng.Range(0.0f, 4.0f);
        vx = -rng.Range(0.0f, 3.0f);
        vy = -rng.Range(0.0f, 3.0f);
      }
      else if (mode < 46) {
        // Exactly on that boundary: the velocity is zero.
        a = r + rng.Range(0.0f, 4.0f);
        b = rng.Range(0.0f, 4.0f);
        vx = 0.0f;
        vy = 0.0f;
      }
      else if (mode < 62) {
        // The corner is the first point of contact: the velocity has no x
        // component, so the cross product is the distance in x times the y
        // speed, and that distance is inside the radius - which is the
        // helper's corner branch, with the answer -fEx, -fEy.
        a = r * rng.Range(0.05f, 0.9f);
        b = r + rng.Range(0.1f, 4.0f);
        vx = 0.0f;
        vy = rng.Range(0.5f, 3.0f);
      }
      else if (mode < 74) {
        // The other corner branch: the velocity has no y component and the
        // distance in y is inside the radius.
        b = r * rng.Range(0.05f, 0.9f);
        a = r + rng.Range(0.1f, 4.0f);
        vx = rng.Range(0.5f, 3.0f);
        vy = 0.0f;
      }
      else if (mode < 84) {
        // The circle touches the left edge of the box, not the corner. The
        // construction makes the helper's own tests land on that branch:
        // the cross product has to be past the corner test but inside the
        // radius times the y speed, and the recomputed cross product - the
        // one the helper makes against the far corner - has to stay inside
        // the radius times the speed.
        const float e = rng.Range(0.1f, 2.0f * ey);
        vx = rng.Range(0.2f, 2.0f);
        vy = vx * (2.0f * ey + e) / r * rng.Range(1.2f, 2.0f);
        a = r + 1.5f * vx * e / vy;
        b = e;
      }
      else if (mode < 91) {
        // The circle misses the box: it passes the corner too far out. The
        // numbers are fixed so that the helper's miss test is reached.
        const float r0 = 1.0f;
        const float ey0 = 0.5f;
        const float vx0 = rng.Range(0.01f, 0.05f);
        const float vy0 = rng.Range(1.0f, 2.0f);
        const float a0 = r0 * rng.Range(3.0f, 5.0f);
        GenIntrBox2Circle2Region(rng, in, 1.0f, ey0, r0, -(1.0f + a0),
            -(ey0 + 0.5f), vx0, vy0);
        return;
      }
      else {
        // The general case.
        a = rng.Range(0.0f, 4.0f * r + 2.0f);
        b = rng.Range(0.0f, 4.0f * r + 2.0f);
        vx = rng.Range(-3.0f, 3.0f);
        vy = rng.Range(-3.0f, 3.0f);
      }

      GenIntrBox2Circle2Region(rng, in, ex, ey, r, -(ex + a), -(ey + b),
          vx, vy);
    }

    // The edge region: the circle centre is beyond the edge (fCx <= -fEx)
    // and its coordinate along the edge is inside the edge's extent.
    void GenIntrBox2Circle2RegionEdge(difffuzz::Rng& rng, std::uint32_t* in)
    {
      const float ex = rng.Range(0.5f, 8.0f);
      const float ey = rng.Range(0.5f, 8.0f);
      const float r = rng.Range(0.25f, 4.0f);
      const std::uint32_t mode = rng.Below(100);

      float a = 0.0f;    // the distance in x from the edge
      float cy = 0.0f;   // the coordinate along the edge
      float vx = 0.0f;
      float vy = 0.0f;

      if (mode < 18) {
        // The circle already reaches the edge: its distance to the edge is
        // not greater than the radius. That is the helper's -1.
        a = r * rng.Range(0.0f, 0.9f);
        cy = rng.Range(-ey, ey);
        vx = rng.Range(-2.0f, 2.0f);
        vy = rng.Range(-2.0f, 2.0f);
      }
      else if (mode < 28) {
        // Exactly on that boundary: the distance to the edge is the radius.
        a = r;
        cy = rng.Range(-ey, ey);
        vx = rng.Range(-2.0f, 2.0f);
        vy = rng.Range(-2.0f, 2.0f);
      }
      else if (mode < 42) {
        // Not moving towards the edge: the x component of the velocity is
        // not positive. That is the helper's 0.
        a = r + rng.Range(0.0f, 4.0f);
        cy = rng.Range(-ey, ey);
        vx = -rng.Range(0.0f, 3.0f);
        vy = rng.Range(-2.0f, 2.0f);
      }
      else if (mode < 50) {
        // Exactly on that boundary: the x component of the velocity is zero.
        a = r + rng.Range(0.0f, 4.0f);
        cy = rng.Range(-ey, ey);
        vx = 0.0f;
        vy = rng.Range(-2.0f, 2.0f);
      }
      else if (mode < 66) {
        // The circle touches the interior of the edge. The construction
        // makes the helper's cross product land inside the radius times the
        // y speed, which is the edge branch.
        const float c = rng.Range(0.1f, 2.0f * ey);
        vy = rng.Range(0.5f, 2.0f);
        a = r * rng.Range(1.05f, 2.0f);
        const float perp = vy * rng.Range(0.05f, 0.95f);
        vx = (vy * a - perp) / c;
        cy = ey - c;
      }
      else if (mode < 76) {
        // The circle touches the corner at the far end of the edge.
        vy = rng.Range(0.5f, 2.0f);
        a = r * rng.Range(1.05f, 2.0f);
        vx = vy * a / r * rng.Range(1.1f, 2.0f);
        cy = ey;
      }
      else if (mode < 83) {
        // The circle misses the box: it passes the corner too far out.
        const float r0 = 1.0f;
        const float ey0 = 0.5f;
        const float vx0 = rng.Range(0.01f, 0.05f);
        const float vy0 = rng.Range(1.0f, 2.0f);
        const float a0 = r0 * rng.Range(3.0f, 5.0f);
        GenIntrBox2Circle2Region(rng, in, 1.0f, ey0, r0, -(1.0f + a0),
            ey0, vx0, vy0);
        return;
      }
      else if (mode < 92) {
        // The circle touches the edge from below: the velocity's y component
        // is negative, and the construction makes the helper's cross product
        // land between minus the radius times the speed and zero.
        const float d = rng.Range(0.1f, 2.0f * ey);
        vy = -rng.Range(0.5f, 2.0f);
        a = r * rng.Range(1.05f, 2.0f);
        vx = -vy * (a - 0.5f * r) / d;
        cy = -ey + d;
      }
      else {
        // The general case.
        a = rng.Range(0.0f, 4.0f * r + 2.0f);
        cy = rng.Range(-ey, ey);
        vx = rng.Range(-3.0f, 3.0f);
        vy = rng.Range(-3.0f, 3.0f);
      }

      GenIntrBox2Circle2Region(rng, in, ex, ey, r, -(ex + a), cy, vx, vy);
    }

    void IntrBox2Circle2TestVertexRegionRetail(const std::uint32_t* in,
                                                std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrBox2Circle2Ctor(storage, f, f + 8);
      float ix = wm3collharness::UndefCoord();
      float iy = wm3collharness::UndefCoord();
      const int iRet = wm3collretail::IntrBox2Circle2TestVertexRegion(
          storage, f[11], f[12], f[13], f[14], f[15], f[16], &ix, &iy);
      float* p = reinterpret_cast<float*>(out + 1);
      out[0] = static_cast<std::uint32_t>(iRet);
      p[0] = ix;
      p[1] = iy;
      float fTime = 0.0f;
      std::memcpy(&fTime, storage + 4, sizeof fTime);
      p[2] = fTime;
    }

    void IntrBox2Circle2TestVertexRegionOurs(const std::uint32_t* in,
                                              std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      float* p = reinterpret_cast<float*>(out + 1);
      const int iRet = wm3collharness::IntrBox2Circle2TestVertexRegion(
          f, f + 8, f[11], f[12], f[13], f[14], f[15], f[16], p, p + 2);
      out[0] = static_cast<std::uint32_t>(iRet);
    }

    void IntrBox2Circle2TestEdgeRegionRetail(const std::uint32_t* in,
                                              std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::IntrBox2Circle2Ctor(storage, f, f + 8);
      float ix = wm3collharness::UndefCoord();
      float iy = wm3collharness::UndefCoord();
      const int iRet = wm3collretail::IntrBox2Circle2TestEdgeRegion(
          storage, f[11], f[12], f[13], f[14], f[15], f[16], &ix, &iy);
      float* p = reinterpret_cast<float*>(out + 1);
      out[0] = static_cast<std::uint32_t>(iRet);
      p[0] = ix;
      p[1] = iy;
      float fTime = 0.0f;
      std::memcpy(&fTime, storage + 4, sizeof fTime);
      p[2] = fTime;
    }

    void IntrBox2Circle2TestEdgeRegionOurs(const std::uint32_t* in,
                                            std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      float* p = reinterpret_cast<float*>(out + 1);
      const int iRet = wm3collharness::IntrBox2Circle2TestEdgeRegion(
          f, f + 8, f[11], f[12], f[13], f[14], f[15], f[16], p, p + 2);
      out[0] = static_cast<std::uint32_t>(iRet);
    }

    // ---- the swept Distance queries --------------------------------------
    //
    // The six swept rows ask the static question one time step later: the
    // two shapes are moved by fT*velocity and the distance between the moved
    // pair is measured.
    //
    // The generators give the row three things the static rows do not need:
    //
    //   * a time.  It is exactly zero a quarter of the time - then the moved
    //     pair is the original pair, bit for bit, because 0*velocity is
    //     exactly zero and origin+0 is exactly origin - and otherwise it is
    //     drawn around zero, so the row is both a motion and a no-motion.
    //   * two velocities that are sometimes equal.  Then the pair moves
    //     rigidly and the *relative* geometry - and with it the branch the
    //     static query takes - is the one the static row already reaches.
    //   * one velocity zero.  Then one shape stands still while the other
    //     sweeps past it, which is the case the engine's own collision code
    //     produces and the one that moves the answer between the clamped and
    //     the unclamped branch of the box queries.
    //
    // The velocities are finite and on the scale of the shapes: the moved
    // pair has to stay inside the float range, or the row would measure the
    // overflow behaviour of the two compilers rather than the algorithm.
    // That is a scope limit, and it is the same one the static rows record.
    void GenDistVector3Box3Swept(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      GenDistVector3Box3(rng, in);            // vec[3] box[15]
      f[18] = rng.Chance(25) ? 0.0f
                              : static_cast<float>(rng.Range(-2.0, 2.0));
      const std::uint32_t kind = rng.Below(100);
      if (kind < 20) {
        // rigid: both shapes move together
        for (int i = 0; i < 3; ++i) {
          f[19 + i] = GenPlain(rng, 100.0f);
          f[22 + i] = f[19 + i];
        }
      }
      else if (kind < 45) {
        // the box stands still and the vector sweeps past it
        for (int i = 0; i < 3; ++i) {
          f[19 + i] = GenPlain(rng, 100.0f);
          f[22 + i] = 0.0f;
        }
      }
      else if (kind < 60) {
        // the vector stands still and the box sweeps past it
        for (int i = 0; i < 3; ++i) {
          f[19 + i] = 0.0f;
          f[22 + i] = GenPlain(rng, 100.0f);
        }
      }
      else {
        for (int i = 0; i < 3; ++i) {
          f[19 + i] = GenPlain(rng, 100.0f);
          f[22 + i] = GenPlain(rng, 100.0f);
        }
      }
    }

    // The ordinary geometry, without the denormal case. `scale` multiplies
    // the two origins and the extent - the directions stay unit - so the
    // whole configuration can be pushed into the denormal range.
    void GenDistLine3Segment3Body(difffuzz::Rng& rng, std::uint32_t* in,
                                  float scale)
    {
      float* f = reinterpret_cast<float*>(in);
      for (int i = 0; i < 3; ++i) {
        f[i] = GenPlain(rng, 20.0f) * scale;   // the line's origin
        f[6 + i] = GenPlain(rng, 20.0f) * scale; // the segment's origin
      }
      difffuzz::GenUnitVec3(rng, f + 3);        // the line's direction
      if (rng.Chance(20)) {
        for (int i = 0; i < 3; ++i) {
          f[9 + i] = f[3 + i];                  // parallel: the degenerate branch
        }
      }
      else {
        difffuzz::GenUnitVec3(rng, f + 9);      // the segment's direction
      }
      // The extent is a length: it is drawn positive (a zero extent is a
      // degenerate case worth a draw), because a negative one puts the
      // algorithm's own branch tests outside the range the retail and the
      // rebuild agree on - the retail returns an infinity there and the
      // rebuild a NaN, and that is an overflow of the *inputs*, not a
      // difference in the arithmetic.
      float extent = 0.0f;
      if (rng.Chance(70)) {
        extent = GenPlain(rng, 100.0f);
      }
      else if (rng.Chance(80)) {
        extent = static_cast<float>(rng.Range(0.0, 1.0));
      }
      f[12] = extent * scale;
    }

    // line[6] (Origin[3], Direction[3]) then segment[7] (Origin[3],
    // Direction[3], Extent) - thirteen floats.
    //
    // The two directions are unit vectors, and one draw in five makes the
    // segment's direction *exactly* the line's: that is the degenerate
    // branch the algorithm tests with `fDet = |1 - fA01*fA01|`.
    void GenDistLine3Segment3(difffuzz::Rng& rng, std::uint32_t* in)
    {
      if (rng.Chance(15)) {
        // The denormal range: the whole configuration - the two origins and
        // the extent - is scaled down by a factor around 1e-21, so the
        // squared distance is around 1e-40 and every product the algorithm
        // forms on the way there lives on the float denormal grid, where a
        // float holds far fewer bits than the x87 register does.  The
        // directions are not scaled: they decide which branch the algorithm
        // takes, and that decision must not change with the scale.
        GenDistLine3Segment3Body(rng, in,
                                 static_cast<float>(rng.Range(1.0e-21, 1.0e-20)));
        return;
      }
      GenDistLine3Segment3Body(rng, in, 1.0f);
    }

    // The swept rows take the same thirteen floats, then fT and the two
    // velocities - seventeen floats.
    void GenDistLine3Segment3Swept(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      if (rng.Chance(15)) {
        // The denormal range, with the sweep switched off: the time and the
        // velocities are zero, so the *moved* pair is exactly the pair
        // below and the relative geometry - the one that decides the
        // squared distance - is untouched.
        GenDistLine3Segment3Body(rng, in,
                                 static_cast<float>(rng.Range(1.0e-21, 1.0e-20)));
        f[13] = 0.0f;                       // fT
        for (int i = 14; i < 20; ++i) {
          f[i] = 0.0f;                      // the velocities
        }
        return;
      }
      GenDistLine3Segment3Body(rng, in, 1.0f);
      f[13] = rng.Chance(25) ? 0.0f
                              : static_cast<float>(rng.Range(-2.0, 2.0));
      const std::uint32_t kind = rng.Below(100);
      if (kind < 25) {
        // rigid: the relative geometry is untouched by the sweep
        for (int i = 0; i < 3; ++i) {
          f[14 + i] = GenPlain(rng, 20.0f);
          f[17 + i] = f[14 + i];
        }
      }
      else if (kind < 50) {
        for (int i = 0; i < 3; ++i) {
          f[14 + i] = GenPlain(rng, 20.0f);
          f[17 + i] = 0.0f;
        }
      }
      else if (kind < 75) {
        for (int i = 0; i < 3; ++i) {
          f[14 + i] = 0.0f;
          f[17 + i] = GenPlain(rng, 20.0f);
        }
      }
      else {
        for (int i = 0; i < 3; ++i) {
          f[14 + i] = GenPlain(rng, 20.0f);
          f[17 + i] = GenPlain(rng, 20.0f);
        }
      }
    }

    void DistLine3Segment3GetSquaredRetail(const std::uint32_t* in,
                                            std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistLine3Segment3Ctor(storage, f, f + 6);
      float value = 0.0f;
      wm3collretail::DistLine3Segment3GetSquared(storage, &value);
      out[0] = difffuzz::Bits(value);
    }

    void DistLine3Segment3GetSquaredOurs(const std::uint32_t* in,
                                          std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(
        wm3collharness::DistLine3Segment3GetSquared(f, f + 6));
    }

    void DistLine3Segment3GetRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistLine3Segment3Ctor(storage, f, f + 6);
      out[0] = difffuzz::Bits(wm3collretail::DistLine3Segment3Get(storage));
    }

    void DistLine3Segment3GetOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(
        wm3collharness::DistLine3Segment3Get(f, f + 6));
    }

    void DistLine3Segment3GetSquaredSweptRetail(const std::uint32_t* in,
                                                 std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistLine3Segment3Ctor(storage, f, f + 6);
      float value = 0.0f;
      wm3collretail::DistLine3Segment3GetSquaredSwept(storage, f[13],
                                                        f + 14, f + 17,
                                                        &value);
      out[0] = difffuzz::Bits(value);
    }

    void DistLine3Segment3GetSquaredSweptOurs(const std::uint32_t* in,
                                               std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(
        wm3collharness::DistLine3Segment3GetSquaredSwept(f, f + 6, f[13],
                                                          f + 14, f + 17));
    }

    void DistLine3Segment3GetSweptRetail(const std::uint32_t* in,
                                          std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistLine3Segment3Ctor(storage, f, f + 6);
      float value = 0.0f;
      wm3collretail::DistLine3Segment3GetSwept(storage, f[13], f + 14, f + 17,
                                                &value);
      out[0] = difffuzz::Bits(value);
    }

    void DistLine3Segment3GetSweptOurs(const std::uint32_t* in,
                                        std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(
        wm3collharness::DistLine3Segment3GetSwept(f, f + 6, f[13], f + 14,
                                                    f + 17));
    }

    // A box so small that the squared distance lands in the float *denormal*
    // range - the case the x87 exponent range is about.  The extent and the
    // vector's offset are both around 1e-19, so the squared distance is
    // around 1e-38, which is below the smallest normal float (1.1754944e-38)
    // and therefore lives on the denormal grid, where a float holds far fewer
    // bits than the x87 register does.
    //
    // The velocities are zero and the time is zero, so the *moved* pair is
    // exactly the pair below: the relative geometry - the one that decides
    // the squared distance - is untouched by the sweep.
    void GenDistVector2Box2Denormal(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      // the extent and the offset, both a few times 1e-19
      const float e = static_cast<float>(rng.Range(1.0e-20, 1.0e-19));
      f[0] = 2.0f*e;                           // the vector, just outside the face
      f[1] = 0.0f;
      f[2] = 0.0f; f[3] = 0.0f;               // the box's centre
      f[4] = 1.0f; f[5] = 0.0f;               // the axis: the identity
      f[6] = 0.0f; f[7] = 1.0f;
      f[8] = e; f[9] = e;                      // the extents
      f[10] = 0.0f;                           // fT
      for (int i = 11; i < 15; ++i) {
        f[i] = 0.0f;                          // the velocities
      }
    }

    void GenDistVector2Box2Swept(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      if (rng.Chance(15)) {
        GenDistVector2Box2Denormal(rng, in);
        return;
      }
      GenDistVector2Box2(rng, in);            // vec[2] box[8]
      f[10] = rng.Chance(25) ? 0.0f
                              : static_cast<float>(rng.Range(-2.0, 2.0));
      const std::uint32_t kind = rng.Below(100);
      if (kind < 20) {
        for (int i = 0; i < 2; ++i) {
          f[11 + i] = GenPlain(rng, 40.0f);
          f[13 + i] = f[11 + i];
        }
      }
      else if (kind < 45) {
        for (int i = 0; i < 2; ++i) {
          f[11 + i] = GenPlain(rng, 40.0f);
          f[13 + i] = 0.0f;
        }
      }
      else if (kind < 60) {
        for (int i = 0; i < 2; ++i) {
          f[11 + i] = 0.0f;
          f[13 + i] = GenPlain(rng, 40.0f);
        }
      }
      else {
        for (int i = 0; i < 2; ++i) {
          f[11 + i] = GenPlain(rng, 40.0f);
          f[13 + i] = GenPlain(rng, 40.0f);
        }
      }
    }

    // Two parallel segments a few times 1e-19 apart: the squared distance is
    // around 1e-38, which is below the smallest normal float and therefore
    // lives on the denormal grid.  The time and the velocities are zero, so
    // the moved pair is exactly the pair below.
    void GenDistSegment3Segment3Denormal(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      const float d = static_cast<float>(rng.Range(1.0e-20, 1.0e-19));
      for (int i = 0; i < 3; ++i) {
        f[i] = 0.0f;                          // segment 0's origin
        f[7 + i] = (i == 1) ? d : 0.0f;        // segment 1's origin
      }
      f[3] = 1.0f; f[4] = 0.0f; f[5] = 0.0f;   // segment 0's direction
      f[10] = 1.0f; f[11] = 0.0f; f[12] = 0.0f; // segment 1's direction
      f[6] = 10.0f;                           // segment 0's extent
      f[13] = 10.0f;                          // segment 1's extent
      f[14] = 0.0f;                           // fT
      for (int i = 15; i < 21; ++i) {
        f[i] = 0.0f;                          // the velocities
      }
    }

    void GenDistSegment3Segment3Swept(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      if (rng.Chance(15)) {
        GenDistSegment3Segment3Denormal(rng, in);
        return;
      }
      GenDistSegment3Segment3(rng, in);        // seg0[7] seg1[7]
      f[14] = rng.Chance(25) ? 0.0f
                             : static_cast<float>(rng.Range(-2.0, 2.0));
      const std::uint32_t kind = rng.Below(100);
      if (kind < 25) {
        // rigid: the relative geometry - the parallel case included - is
        // exactly the one the static row reaches
        for (int i = 0; i < 3; ++i) {
          f[15 + i] = GenPlain(rng, 20.0f);
          f[18 + i] = f[15 + i];
        }
      }
      else if (kind < 50) {
        for (int i = 0; i < 3; ++i) {
          f[15 + i] = GenPlain(rng, 20.0f);
          f[18 + i] = 0.0f;
        }
      }
      else if (kind < 65) {
        for (int i = 0; i < 3; ++i) {
          f[15 + i] = 0.0f;
          f[18 + i] = GenPlain(rng, 20.0f);
        }
      }
      else {
        for (int i = 0; i < 3; ++i) {
          f[15 + i] = GenPlain(rng, 20.0f);
          f[18 + i] = GenPlain(rng, 20.0f);
        }
      }
    }

    // The Get() rows' scope is the finite, non-negative results, for the
    // reason the static Get() rows record: retail's Get() takes the CRT's
    // square root at 0x00A91000, and the CRT's slow path for a non-positive
    // argument is a fatal math error - in the retail as well, not only in
    // this harness.  The filter lives in this row's generator rather than
    // behind the global --skip-extreme, so the other rows keep full coverage.
    void GenDistVector3Box3SweptGet(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      for (int attempt = 0; attempt < 32; ++attempt) {
        GenDistVector3Box3Swept(rng, in);
        const float squared = wm3collharness::DistVector3Box3GetSquaredSwept(
            f, f + 3, f[18], f + 19, f + 22);
        const std::uint32_t bits = difffuzz::Bits(squared);
        if ((bits & 0x7F800000u) != 0x7F800000u) {
          return; // the exponent is not all ones, so this is finite
        }
      }
      for (int i = 0; i < 18; ++i) {
        f[i] = GenPlain(rng, 1.0f);
      }
      f[18] = 0.0f;
      for (int i = 19; i < 25; ++i) {
        f[i] = 0.0f;
      }
    }

    void GenDistVector2Box2SweptGet(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      for (int attempt = 0; attempt < 32; ++attempt) {
        GenDistVector2Box2Swept(rng, in);
        const float squared = wm3collharness::DistVector2Box2GetSquaredSwept(
            f, f + 2, f[10], f + 11, f + 13);
        const std::uint32_t bits = difffuzz::Bits(squared);
        if ((bits & 0x7F800000u) != 0x7F800000u) {
          return;
        }
      }
      for (int i = 0; i < 10; ++i) {
        f[i] = GenPlain(rng, 1.0f);
      }
      for (int i = 10; i < 15; ++i) {
        f[i] = 0.0f;
      }
    }

    void GenDistSegment3Segment3SweptGet(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      for (int attempt = 0; attempt < 32; ++attempt) {
        GenDistSegment3Segment3Swept(rng, in);
        const float squared =
          wm3collharness::DistSegment3Segment3GetSquaredSwept(
              f, f + 7, f[14], f + 15, f + 18);
        const std::uint32_t bits = difffuzz::Bits(squared);
        const bool finite = (bits & 0x7F800000u) != 0x7F800000u;
        const bool negative =
          (bits & 0x80000000u) != 0u && (bits & 0x7FFFFFFFu) != 0u;
        if (finite && !negative) {
          return;
        }
      }
      for (int i = 0; i < 14; ++i) {
        f[i] = GenPlain(rng, 1.0f);
      }
      for (int i = 14; i < 21; ++i) {
        f[i] = 0.0f;
      }
    }

    void DistVector3Box3GetSweptRetail(const std::uint32_t* in, std::uint32_t* out)    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistVector3Box3Ctor(storage, f, f + 3);
      float value = 0.0f;
      wm3collretail::DistVector3Box3GetSwept(storage, f[18], f + 19, f + 22,
                                              &value);
      out[0] = difffuzz::Bits(value);
    }

    void DistVector3Box3GetSweptOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(wm3collharness::DistVector3Box3GetSwept(
          f, f + 3, f[18], f + 19, f + 22));
    }

    void DistVector3Box3GetSquaredSweptRetail(const std::uint32_t* in,
                                               std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistVector3Box3Ctor(storage, f, f + 3);
      float value = 0.0f;
      wm3collretail::DistVector3Box3GetSquaredSwept(storage, f[18], f + 19,
                                                     f + 22, &value);
      out[0] = difffuzz::Bits(value);
    }

    void DistVector3Box3GetSquaredSweptOurs(const std::uint32_t* in,
                                             std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(wm3collharness::DistVector3Box3GetSquaredSwept(
          f, f + 3, f[18], f + 19, f + 22));
    }

    void DistVector2Box2GetSweptRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistVector2Box2Ctor(storage, f, f + 2);
      float value = 0.0f;
      wm3collretail::DistVector2Box2GetSwept(storage, f[10], f + 11, f + 13,
                                              &value);
      out[0] = difffuzz::Bits(value);
    }

    void DistVector2Box2GetSweptOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(wm3collharness::DistVector2Box2GetSwept(
          f, f + 2, f[10], f + 11, f + 13));
    }

    void DistVector2Box2GetSquaredSweptRetail(const std::uint32_t* in,
                                               std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistVector2Box2Ctor(storage, f, f + 2);
      float value = 0.0f;
      wm3collretail::DistVector2Box2GetSquaredSwept(storage, f[10], f + 11,
                                                     f + 13, &value);
      out[0] = difffuzz::Bits(value);
    }

    void DistVector2Box2GetSquaredSweptOurs(const std::uint32_t* in,
                                             std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(wm3collharness::DistVector2Box2GetSquaredSwept(
          f, f + 2, f[10], f + 11, f + 13));
    }

    void DistSegment3Segment3GetSweptRetail(const std::uint32_t* in,
                                            std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistSegment3Segment3Ctor(storage, f, f + 7);
      float value = 0.0f;
      wm3collretail::DistSegment3Segment3GetSwept(storage, f[14], f + 15,
                                                   f + 18, &value);
      out[0] = difffuzz::Bits(value);
    }

    void DistSegment3Segment3GetSweptOurs(const std::uint32_t* in,
                                          std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(wm3collharness::DistSegment3Segment3GetSwept(
          f, f + 7, f[14], f + 15, f + 18));
    }

    void DistSegment3Segment3GetSquaredSweptRetail(const std::uint32_t* in,
                                                   std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistSegment3Segment3Ctor(storage, f, f + 7);
      float value = 0.0f;
      wm3collretail::DistSegment3Segment3GetSquaredSwept(storage, f[14],
                                                          f + 15, f + 18,
                                                          &value);
      out[0] = difffuzz::Bits(value);
    }

    void DistSegment3Segment3GetSquaredSweptOurs(const std::uint32_t* in,
                                                 std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(
        wm3collharness::DistSegment3Segment3GetSquaredSwept(
            f, f + 7, f[14], f + 15, f + 18));
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
    void GenIntrLine3Box3DoClipping(difffuzz::Rng& rng, std::uint32_t* in)
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
        difffuzz::GenUnitVec3(rng, f + 11);          // axis 0
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
        difffuzz::GenUnitVec3(rng, f + 5);
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
    void GenIntrLine3Box3Clip(difffuzz::Rng& rng, std::uint32_t* in)
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
      out[1] = difffuzz::Bits(t0);
      out[2] = difffuzz::Bits(t1);
    }

    void IntrLine3Box3ClipOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      float t0 = f[2];
      float t1 = f[3];
      const std::uint32_t ok = wm3collharness::IntrLine3Box3Clip(f[0], f[1],
                                                                &t0, &t1);
      out[0] = ok;
      out[1] = difffuzz::Bits(t0);
      out[2] = difffuzz::Bits(t1);
    }

    // ---- the base class's window queries -----------------------------------
    //
    // vec[3] then segment[7], then fTMin, fTMax and the two velocities -
    // eighteen floats.
    //
    // The window query minimises the distance over [fTMin,fTMax], and the
    // branches it has are:
    //
    //   * the objects are already in contact at fTMin (the distance is
    //     effectively zero) - the answer is zero;
    //   * the distance is increasing over the whole interval - the answer is
    //     the distance at fTMin;
    //   * the distance is decreasing over the whole interval - the answer is
    //     the distance at fTMax;
    //   * the distance has a minimum inside the interval - the answer is the
    //     distance at that minimum, found by Newton's method and then by
    //     bisection.
    //
    // The last one is the interesting one, and it is the one the generator
    // aims at: the relative velocity is pointed at the segment, so the two
    // shapes approach each other and then separate, and the minimum lies
    // inside the window.  The receding case - the relative velocity pointed
    // away - is drawn as well, because that is the "the distance is
    // increasing" branch.
    void GenDistVector3Segment3Window(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      GenDistVector3Segment3(rng, in);          // vec[3] segment[7]

      // the direction from the vector to the segment's origin
      float d[3];
      for (int i = 0; i < 3; ++i) {
        d[i] = f[3 + i] - f[i];
      }
      const float len = std::sqrt(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
      if (len > 1.0e-6f) {
        for (int i = 0; i < 3; ++i) {
          d[i] /= len;
        }
      }
      else {
        d[0] = 1.0f; d[1] = 0.0f; d[2] = 0.0f;
      }
      const float speed = static_cast<float>(rng.Range(1.0, 20.0));
      const std::uint32_t kind = rng.Below(100);
      for (int i = 0; i < 3; ++i) {
        // the relative velocity: the vector's minus the segment's
        const float rel = (kind < 40) ? speed
                          : (kind < 70 ? -speed
                                       : static_cast<float>(rng.Range(-20.0, 20.0)));
        f[12 + i] = d[i]*rel;              // the velocity of the vector
        f[15 + i] = 0.0f;                  // the segment stands still
      }
      // the window
      const std::uint32_t win = rng.Below(100);
      if (win < 40) {
        f[10] = 0.0f;  f[11] = 1.0f;
      }
      else if (win < 70) {
        f[10] = -1.0f; f[11] = 1.0f;
      }
      else if (win < 85) {
        f[10] = 0.0f;  f[11] = 0.5f;
      }
      else {
        f[10] = 0.0f;  f[11] = 4.0f;
      }
    }

    void DistVector3Segment3WindowGetRetail(const std::uint32_t* in,
                                            std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistVector3Segment3Ctor(storage, f, f + 3);
      float value = 0.0f;
      wm3collretail::DistVector3Segment3WindowGet(storage, f[10], f[11], f + 12,
                                                   f + 15, &value);
      out[0] = difffuzz::Bits(value);
    }

    void DistVector3Segment3WindowGetOurs(const std::uint32_t* in,
                                          std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(wm3collharness::DistVector3Segment3WindowGet(
          f, f + 3, f[10], f[11], f + 12, f + 15));
    }

    void DistVector3Segment3WindowGetSquaredRetail(const std::uint32_t* in,
                                                   std::uint32_t* out)
    {
      unsigned char storage[kObjectBytes];
      std::memset(storage, 0, sizeof storage);
      const float* f = reinterpret_cast<const float*>(in);
      wm3collretail::DistVector3Segment3Ctor(storage, f, f + 3);
      float value = 0.0f;
      wm3collretail::DistVector3Segment3WindowGetSquared(storage, f[10], f[11],
                                                          f + 12, f + 15,
                                                          &value);
      out[0] = difffuzz::Bits(value);
    }

    void DistVector3Segment3WindowGetSquaredOurs(const std::uint32_t* in,
                                                 std::uint32_t* out)
    {
      const float* f = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(
        wm3collharness::DistVector3Segment3WindowGetSquared(
            f, f + 3, f[10], f[11], f + 12, f + 15));
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
    void GenDistanceSetDifferenceStep(difffuzz::Rng& rng, std::uint32_t* in)
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

    // ---- Wm3::Distance<double,Vector3d>'s difference step ------------------
    //
    // The same argument as the float row, but a double: the step travels as
    // two words.
    //
    // The retail's body (0x00A39DB0) compares the argument with zero and
    // takes one branch or the other - a step that is not positive is
    // replaced by the default 1e-3 - so the row has to draw both polarities
    // and several magnitudes.  1e-3 is drawn on its own: it is the default,
    // and the only value the engine ever passes.
    //
    // The magnitudes at the ends of the range are there for the second
    // member: the reciprocal 0.5/step is a 24-bit significand on the retail
    // (the engine runs under `_PC_24`) and a binary64 quotient in a plain
    // rebuild, so a step whose reciprocal is not representable in 24 bits is
    // where the two part company.  Every step here is a double, so the
    // x87's extended exponent range covers all of them - a double never
    // overflows on the retail side.
    void GenDistanceSetDifferenceStepDouble(difffuzz::Rng& rng,
                                            std::uint32_t* in)
    {
      const std::uint32_t kind = rng.Below(100);
      double step = 0.0;
      if (kind < 20) {
        step = 1e-3;                             // the default
      }
      else if (kind < 45) {
        // positive, and not the default
        static const double kPositive[] = {
            1e-30, 1e-10, 1e-4, 5e-4, 2e-3, 0.5, 1.0, 2.0, 1e3, 1e30,
            2.2250738585072014e-308, 1e-300 };
        step = kPositive[rng.Below(12)];
      }
      else if (kind < 62) {
        // negative
        static const double kNegative[] = {
            -1e-30, -1e-4, -1e-3, -0.5, -1.0, -2.0, -1e30, -1e300 };
        step = kNegative[rng.Below(8)];
      }
      else if (kind < 75) {
        // exactly zero, both signs
        step = rng.Chance(50) ? 0.0 : -0.0;
      }
      else if (kind < 90) {
        // big enough that 0.5/step is a double denormal
        step = rng.Chance(50) ? 1e308 : -1e308;
      }
      else {
        step = difffuzz::GenDouble(rng);
      }
      difffuzz::StoreDouble(in, step);
    }

    void Distance3dSetDifferenceStepRetail(const std::uint32_t* in,
                                           std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::Distance3dCtor(storage);
      wm3collretail::Distance3dSetDifferenceStep(storage,
                                                  difffuzz::LoadDouble(in));
      // m_fDifferenceStep at [this+0x58] and m_fInvTwoDifferenceStep at
      // [this+0x60], a double each.
      std::memcpy(out, storage + 0x58, 2*sizeof(double));
    }

    void Distance3dSetDifferenceStepOurs(const std::uint32_t* in,
                                         std::uint32_t* out)
    {
      wm3collharness::Distance3dSetDifferenceStep(difffuzz::LoadDouble(in),
                                                   out);
    }

    void Distance3dDifferenceStepDefaultRetail(const std::uint32_t* in,
                                               std::uint32_t* out)
    {
      (void)in;
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::Distance3dCtor(storage);
      std::memcpy(out, storage + 0x58, 2*sizeof(double));
    }

    void Distance3dDifferenceStepDefaultOurs(const std::uint32_t* in,
                                             std::uint32_t* out)
    {
      (void)in;
      wm3collharness::Distance3dDifferenceStepDefault(out);
    }

    void Distance2dSetDifferenceStepRetail(const std::uint32_t* in,
                                           std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::Distance2dCtor(storage);
      wm3collretail::Distance2dSetDifferenceStep(storage,
                                                  difffuzz::LoadDouble(in));
      // m_fDifferenceStep at [this+0x48] and m_fInvTwoDifferenceStep at
      // [this+0x50].
      std::memcpy(out, storage + 0x48, 2*sizeof(double));
    }

    void Distance2dSetDifferenceStepOurs(const std::uint32_t* in,
                                         std::uint32_t* out)
    {
      wm3collharness::Distance2dSetDifferenceStep(difffuzz::LoadDouble(in),
                                                   out);
    }

    void Distance2dDifferenceStepDefaultRetail(const std::uint32_t* in,
                                               std::uint32_t* out)
    {
      (void)in;
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::Distance2dCtor(storage);
      std::memcpy(out, storage + 0x48, 2*sizeof(double));
    }

    void Distance2dDifferenceStepDefaultOurs(const std::uint32_t* in,
                                             std::uint32_t* out)
    {
      (void)in;
      wm3collharness::Distance2dDifferenceStepDefault(out);
    }

    // ---- Wm3::Distance<double,Vector3d>'s and ::Distance<double,Vector2d>'s
    // own bodies ------------------------------------------------------
    //
    // The base class's window search has four early exits and two loops:
    //
    //   fF0 <= ZeroThreshold   the objects are in contact at fTMin
    //   fDF0 >= 0              the distance grows from fTMin
    //   fF1 <= ZeroThreshold   the objects are in contact at fTMax
    //   fDF1 <= 0              the distance still shrinks at fTMax
    //   the Newton loop        fT = fT0 - fF0/fDF0
    //   the bisection loop     fTm = 0.5*(fT0 + fT1)
    //
    // and the derivative defaults have their own shape: the finite
    // difference f'(t) = (f(t+h)-f(t-h))/(2*h).
    //
    // The generator reaches the four early exits *exactly*, with the
    // segment on the x axis and the vector either on it (the contact case)
    // or off it by a whole number (the two monotone cases).  The general
    // family carries the two loops and the arithmetic.
    //
    // The magnitudes stay inside [1e-100,1e100]: the emulation models the
    // x87's 24-bit significand but not its extended exponent, and the
    // rebuild's double path carries the values in binary64.
    void GenDistance3dWindow(difffuzz::Rng& rng, std::uint32_t* in)
    {
      const auto Mag = [&rng]() -> double
      {
        const int e = static_cast<int>(rng.Below(201)) - 100;
        return std::ldexp(rng.Range(1.0, 10.0), e);
      };
      double v[3], origin[3], dir[3], extent;
      double vel0[3], vel1[3];
      double fTMin = 0.0, fTMax = 1.0;
      const std::uint32_t kind = rng.Below(100);
      if (kind < 20) {
        // in contact at fTMin: the vector sits on the segment
        dir[0] = 1.0; dir[1] = 0.0; dir[2] = 0.0;
        origin[0] = origin[1] = origin[2] = 0.0;
        extent = rng.Range(1.0, 100.0);
        v[0] = rng.Range(-extent, extent); v[1] = 0.0; v[2] = 0.0;
      }
      else if (kind < 40) {
        // the distance grows from fTMin
        dir[0] = 1.0; dir[1] = 0.0; dir[2] = 0.0;
        origin[0] = origin[1] = origin[2] = 0.0;
        extent = rng.Range(1.0, 100.0);
        v[0] = 0.0; v[1] = rng.Range(1.0, 100.0); v[2] = 0.0;
        vel0[1] = rng.Range(1.0, 20.0);
      }
      else if (kind < 55) {
        // the distance still shrinks at fTMax
        dir[0] = 1.0; dir[1] = 0.0; dir[2] = 0.0;
        origin[0] = origin[1] = origin[2] = 0.0;
        extent = rng.Range(1.0, 100.0);
        v[0] = 0.0; v[1] = rng.Range(10.0, 100.0); v[2] = 0.0;
        vel0[1] = -rng.Range(1.0, 5.0);
        fTMax = 1.0;
      }
      else if (kind < 70) {
        // the minimum is inside the window
        dir[0] = 1.0; dir[1] = 0.0; dir[2] = 0.0;
        origin[0] = origin[1] = origin[2] = 0.0;
        extent = rng.Range(1.0, 100.0);
        v[0] = 0.0; v[1] = rng.Range(10.0, 100.0); v[2] = 0.0;
        const double speed = rng.Range(1.0, 5.0);
        vel0[1] = -speed;
        // the window reaches past the closest approach
        fTMax = 2.0*v[1]/speed + 1.0;
      }
      else {
        // the general family
        difffuzz::GenUnitVec3d(rng, dir);
        for (int i = 0; i < 3; ++i) {
          origin[i] = rng.Chance(60) ? rng.Range(-100.0, 100.0) : Mag();
        }
        for (int i = 0; i < 3; ++i) {
          v[i] = rng.Chance(60) ? rng.Range(-100.0, 100.0) : Mag();
        }
        extent = rng.Chance(60) ? rng.Range(0.0, 100.0) : Mag();
        for (int i = 0; i < 3; ++i) {
          vel0[i] = rng.Chance(50) ? rng.Range(-20.0, 20.0) : Mag();
          vel1[i] = rng.Chance(50) ? rng.Range(-20.0, 20.0) : Mag();
        }
        fTMin = rng.Chance(60) ? 0.0 : rng.Range(-4.0, 4.0);
        fTMax = fTMin + (rng.Chance(50) ? rng.Range(0.5, 40.0)
                                         : rng.Range(0.0, 4.0));
      }
      double d[18];
      for (int i = 0; i < 3; ++i) d[i] = v[i];
      for (int i = 0; i < 3; ++i) d[3 + i] = origin[i];
      for (int i = 0; i < 3; ++i) d[6 + i] = dir[i];
      d[9] = extent;
      d[10] = fTMin;
      d[11] = fTMax;
      for (int i = 0; i < 3; ++i) d[12 + i] = vel0[i];
      for (int i = 0; i < 3; ++i) d[15 + i] = vel1[i];
      for (int i = 0; i < 18; ++i) {
        difffuzz::StoreDouble(in + 2*i, d[i]);
      }
    }

    // The derivative rows take one time instead of the window's two.
    void GenDistance3dDerivative(difffuzz::Rng& rng, std::uint32_t* in)
    {
      GenDistance3dWindow(rng, in);
      // the second time of the window becomes the row's own time
      difffuzz::StoreDouble(in + 20, difffuzz::LoadDouble(in + 22));
    }

    // The Vector2 base's rows: the same four exits, with a box instead of a
    // segment.  The box is axis aligned and centred on the origin, so the
    // point's distance to it is a plain ramp and the branches are reached
    // exactly.
    void GenDistance2dWindow(difffuzz::Rng& rng, std::uint32_t* in)
    {
      const auto Mag = [&rng]() -> double
      {
        const int e = static_cast<int>(rng.Below(201)) - 100;
        return std::ldexp(rng.Range(1.0, 10.0), e);
      };
      double d[16];
      // the centre
      d[2] = 0.0; d[3] = 0.0;
      // the two axes
      d[4] = 1.0; d[5] = 0.0;
      d[6] = 0.0; d[7] = 1.0;
      // the extents
      const double extent = rng.Range(1.0, 30.0);
      d[8] = extent; d[9] = extent;
      double vel0[2], vel1[2];
      double fTMin = 0.0, fTMax = 1.0;
      const std::uint32_t kind = rng.Below(100);
      if (kind < 20) {
        // in contact at fTMin: the point sits on the box's face
        d[0] = 0.0; d[1] = extent;
      }
      else if (kind < 40) {
        // the distance grows from fTMin
        d[0] = 0.0; d[1] = extent + rng.Range(1.0, 100.0);
        vel0[1] = rng.Range(1.0, 20.0);
      }
      else if (kind < 55) {
        // the distance still shrinks at fTMax
        d[0] = 0.0; d[1] = extent + rng.Range(10.0, 100.0);
        vel0[1] = -rng.Range(1.0, 5.0);
        fTMax = 1.0;
      }
      else if (kind < 70) {
        // the minimum is inside the window
        d[0] = 0.0; d[1] = extent + rng.Range(10.0, 100.0);
        const double speed = rng.Range(1.0, 5.0);
        vel0[1] = -speed;
        fTMax = 2.0*(d[1] - extent)/speed + 1.0;
      }
      else {
        // the general family: the box is rotated and the point is anywhere
        const double angle = rng.Range(0.0, 3.1415926535897932);
        const double ca = std::cos(angle), sa = std::sin(angle);
        d[4] = ca;  d[5] = -sa;
        d[6] = sa;  d[7] = ca;
        d[2] = rng.Chance(60) ? rng.Range(-100.0, 100.0) : Mag();
        d[3] = rng.Chance(60) ? rng.Range(-100.0, 100.0) : Mag();
        d[8] = rng.Chance(60) ? rng.Range(0.5, 30.0) : Mag();
        d[9] = rng.Chance(60) ? rng.Range(0.5, 30.0) : Mag();
        for (int i = 0; i < 2; ++i) {
          d[i] = d[2 + i] + (rng.Chance(60) ? rng.Range(-100.0, 100.0) : Mag());
        }
        for (int i = 0; i < 2; ++i) {
          vel0[i] = rng.Chance(50) ? rng.Range(-20.0, 20.0) : Mag();
          vel1[i] = rng.Chance(50) ? rng.Range(-20.0, 20.0) : Mag();
        }
        fTMin = rng.Chance(60) ? 0.0 : rng.Range(-4.0, 4.0);
        fTMax = fTMin + (rng.Chance(50) ? rng.Range(0.5, 40.0)
                                         : rng.Range(0.0, 4.0));
      }
      d[10] = fTMin;
      d[11] = fTMax;
      d[12] = vel0[0]; d[13] = vel0[1];
      d[14] = vel1[0]; d[15] = vel1[1];
      for (int i = 0; i < 16; ++i) {
        difffuzz::StoreDouble(in + 2*i, d[i]);
      }
    }

    void GenDistance2dDerivative(difffuzz::Rng& rng, std::uint32_t* in)
    {
      GenDistance2dWindow(rng, in);
      difffuzz::StoreDouble(in + 20, difffuzz::LoadDouble(in + 22));
    }

    // ---- the eight rows ---------------------------------------------------
    //
    // The retail side builds the object with the retail's own constructor
    // and then calls the base's own body on it, so the body's vtable
    // dispatch reaches the derived class's swept Get.
    void Distance3dWindowGetRetail(const std::uint32_t* in,
                                   std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double vec[3], segment[7], vel0[3], vel1[3];
      for (int i = 0; i < 3; ++i) {
        vec[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 7; ++i) {
        segment[i] = difffuzz::LoadDouble(in + 2*(3 + i));
      }
      const double fTMin = difffuzz::LoadDouble(in + 20);
      const double fTMax = difffuzz::LoadDouble(in + 22);
      for (int i = 0; i < 3; ++i) {
        vel0[i] = difffuzz::LoadDouble(in + 2*(12 + i));
      }
      for (int i = 0; i < 3; ++i) {
        vel1[i] = difffuzz::LoadDouble(in + 2*(15 + i));
      }
      wm3collretail::DistVector3Segment3dCtor(storage, vec, segment);
      double value = 0.0;
      wm3collretail::Distance3dWindowGet(storage, fTMin, fTMax, vel0, vel1,
                                         &value);
      difffuzz::StoreDouble(out, value);
    }

    void Distance3dWindowGetOurs(const std::uint32_t* in,
                                 std::uint32_t* out)
    {
      difffuzz::StoreDouble(out,
          wm3collharness::Distance3dWindowGet(in, in + 6,
              difffuzz::LoadDouble(in + 20), difffuzz::LoadDouble(in + 22),
              in + 24, in + 30));
    }

    void Distance3dWindowGetSquaredRetail(const std::uint32_t* in,
                                          std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double vec[3], segment[7], vel0[3], vel1[3];
      for (int i = 0; i < 3; ++i) {
        vec[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 7; ++i) {
        segment[i] = difffuzz::LoadDouble(in + 2*(3 + i));
      }
      const double fTMin = difffuzz::LoadDouble(in + 20);
      const double fTMax = difffuzz::LoadDouble(in + 22);
      for (int i = 0; i < 3; ++i) {
        vel0[i] = difffuzz::LoadDouble(in + 2*(12 + i));
      }
      for (int i = 0; i < 3; ++i) {
        vel1[i] = difffuzz::LoadDouble(in + 2*(15 + i));
      }
      wm3collretail::DistVector3Segment3dCtor(storage, vec, segment);
      double value = 0.0;
      wm3collretail::Distance3dWindowGetSquared(storage, fTMin, fTMax, vel0,
                                                 vel1, &value);
      difffuzz::StoreDouble(out, value);
    }

    void Distance3dWindowGetSquaredOurs(const std::uint32_t* in,
                                        std::uint32_t* out)
    {
      difffuzz::StoreDouble(out,
          wm3collharness::Distance3dWindowGetSquared(in, in + 6,
              difffuzz::LoadDouble(in + 20), difffuzz::LoadDouble(in + 22),
              in + 24, in + 30));
    }

    void Distance3dGetDerivativeRetail(const std::uint32_t* in,
                                       std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double vec[3], segment[7], vel0[3], vel1[3];
      for (int i = 0; i < 3; ++i) {
        vec[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 7; ++i) {
        segment[i] = difffuzz::LoadDouble(in + 2*(3 + i));
      }
      const double fT = difffuzz::LoadDouble(in + 20);
      for (int i = 0; i < 3; ++i) {
        vel0[i] = difffuzz::LoadDouble(in + 2*(11 + i));
      }
      for (int i = 0; i < 3; ++i) {
        vel1[i] = difffuzz::LoadDouble(in + 2*(14 + i));
      }
      wm3collretail::DistVector3Segment3dCtor(storage, vec, segment);
      double value = 0.0;
      wm3collretail::Distance3dGetDerivative(storage, fT, vel0, vel1, &value);
      difffuzz::StoreDouble(out, value);
    }

    void Distance3dGetDerivativeOurs(const std::uint32_t* in,
                                     std::uint32_t* out)
    {
      difffuzz::StoreDouble(out,
          wm3collharness::Distance3dGetDerivative(in, in + 6,
              difffuzz::LoadDouble(in + 20), in + 22, in + 28));
    }

    void Distance3dGetDerivativeSquaredRetail(const std::uint32_t* in,
                                              std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double vec[3], segment[7], vel0[3], vel1[3];
      for (int i = 0; i < 3; ++i) {
        vec[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 7; ++i) {
        segment[i] = difffuzz::LoadDouble(in + 2*(3 + i));
      }
      const double fT = difffuzz::LoadDouble(in + 20);
      for (int i = 0; i < 3; ++i) {
        vel0[i] = difffuzz::LoadDouble(in + 2*(11 + i));
      }
      for (int i = 0; i < 3; ++i) {
        vel1[i] = difffuzz::LoadDouble(in + 2*(14 + i));
      }
      wm3collretail::DistVector3Segment3dCtor(storage, vec, segment);
      double value = 0.0;
      wm3collretail::Distance3dGetDerivativeSquared(storage, fT, vel0, vel1,
                                                     &value);
      difffuzz::StoreDouble(out, value);
    }

    void Distance3dGetDerivativeSquaredOurs(const std::uint32_t* in,
                                            std::uint32_t* out)
    {
      difffuzz::StoreDouble(out,
          wm3collharness::Distance3dGetDerivativeSquared(in, in + 6,
              difffuzz::LoadDouble(in + 20), in + 22, in + 28));
    }

    void Distance2dWindowGetRetail(const std::uint32_t* in,
                                   std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double vec[2], box[8], vel0[2], vel1[2];
      for (int i = 0; i < 2; ++i) {
        vec[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 8; ++i) {
        box[i] = difffuzz::LoadDouble(in + 2*(2 + i));
      }
      const double fTMin = difffuzz::LoadDouble(in + 20);
      const double fTMax = difffuzz::LoadDouble(in + 22);
      for (int i = 0; i < 2; ++i) {
        vel0[i] = difffuzz::LoadDouble(in + 2*(12 + i));
      }
      for (int i = 0; i < 2; ++i) {
        vel1[i] = difffuzz::LoadDouble(in + 2*(14 + i));
      }
      wm3collretail::DistVector2Box2dCtor(storage, vec, box);
      double value = 0.0;
      wm3collretail::Distance2dWindowGet(storage, fTMin, fTMax, vel0, vel1,
                                         &value);
      difffuzz::StoreDouble(out, value);
    }

    void Distance2dWindowGetOurs(const std::uint32_t* in,
                                 std::uint32_t* out)
    {
      difffuzz::StoreDouble(out,
          wm3collharness::Distance2dWindowGet(in, in + 4,
              difffuzz::LoadDouble(in + 20), difffuzz::LoadDouble(in + 22),
              in + 24, in + 28));
    }

    void Distance2dWindowGetSquaredRetail(const std::uint32_t* in,
                                          std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double vec[2], box[8], vel0[2], vel1[2];
      for (int i = 0; i < 2; ++i) {
        vec[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 8; ++i) {
        box[i] = difffuzz::LoadDouble(in + 2*(2 + i));
      }
      const double fTMin = difffuzz::LoadDouble(in + 20);
      const double fTMax = difffuzz::LoadDouble(in + 22);
      for (int i = 0; i < 2; ++i) {
        vel0[i] = difffuzz::LoadDouble(in + 2*(12 + i));
      }
      for (int i = 0; i < 2; ++i) {
        vel1[i] = difffuzz::LoadDouble(in + 2*(14 + i));
      }
      wm3collretail::DistVector2Box2dCtor(storage, vec, box);
      double value = 0.0;
      wm3collretail::Distance2dWindowGetSquared(storage, fTMin, fTMax, vel0,
                                                 vel1, &value);
      difffuzz::StoreDouble(out, value);
    }

    void Distance2dWindowGetSquaredOurs(const std::uint32_t* in,
                                        std::uint32_t* out)
    {
      difffuzz::StoreDouble(out,
          wm3collharness::Distance2dWindowGetSquared(in, in + 4,
              difffuzz::LoadDouble(in + 20), difffuzz::LoadDouble(in + 22),
              in + 24, in + 28));
    }

    void Distance2dGetDerivativeRetail(const std::uint32_t* in,
                                       std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double vec[2], box[8], vel0[2], vel1[2];
      for (int i = 0; i < 2; ++i) {
        vec[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 8; ++i) {
        box[i] = difffuzz::LoadDouble(in + 2*(2 + i));
      }
      const double fT = difffuzz::LoadDouble(in + 20);
      for (int i = 0; i < 2; ++i) {
        vel0[i] = difffuzz::LoadDouble(in + 2*(11 + i));
      }
      for (int i = 0; i < 2; ++i) {
        vel1[i] = difffuzz::LoadDouble(in + 2*(13 + i));
      }
      wm3collretail::DistVector2Box2dCtor(storage, vec, box);
      double value = 0.0;
      wm3collretail::Distance2dGetDerivative(storage, fT, vel0, vel1, &value);
      difffuzz::StoreDouble(out, value);
    }

    void Distance2dGetDerivativeOurs(const std::uint32_t* in,
                                     std::uint32_t* out)
    {
      difffuzz::StoreDouble(out,
          wm3collharness::Distance2dGetDerivative(in, in + 4,
              difffuzz::LoadDouble(in + 20), in + 22, in + 26));
    }

    void Distance2dGetDerivativeSquaredRetail(const std::uint32_t* in,
                                              std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double vec[2], box[8], vel0[2], vel1[2];
      for (int i = 0; i < 2; ++i) {
        vec[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 8; ++i) {
        box[i] = difffuzz::LoadDouble(in + 2*(2 + i));
      }
      const double fT = difffuzz::LoadDouble(in + 20);
      for (int i = 0; i < 2; ++i) {
        vel0[i] = difffuzz::LoadDouble(in + 2*(11 + i));
      }
      for (int i = 0; i < 2; ++i) {
        vel1[i] = difffuzz::LoadDouble(in + 2*(13 + i));
      }
      wm3collretail::DistVector2Box2dCtor(storage, vec, box);
      double value = 0.0;
      wm3collretail::Distance2dGetDerivativeSquared(storage, fT, vel0, vel1,
                                                     &value);
      difffuzz::StoreDouble(out, value);
    }

    void Distance2dGetDerivativeSquaredOurs(const std::uint32_t* in,
                                            std::uint32_t* out)
    {
      difffuzz::StoreDouble(out,
          wm3collharness::Distance2dGetDerivativeSquared(in, in + 4,
              difffuzz::LoadDouble(in + 20), in + 22, in + 26));
    }

    // ---- Wm3::DistVector3Box3<double> -------------------------------------
    //
    // The double instantiation of the class the float rows already compare.
    //
    // The input is the vector (three doubles) and the box (the centre, the
    // three axes and the three extents - fifteen doubles), so thirty-six
    // words.
    //
    // The generator keeps the box a *box*: the axes are unit vectors and the
    // extents are non-negative, because that is what the engine hands the
    // class and because the clamping branches - the ones that make the
    // squared distance more than the projection - are only reached when the
    // point is outside the slab the axis spans.
    //
    // The magnitudes stay inside [1e-100, 1e100], and that is a limit of
    // the rebuild's double path rather than of the row: the emulation in
    // Foundation/Math/Wm3Vector3.h models the x87's 24-bit significand but
    // not its extended exponent, so a value that leaves the binary64 range
    // in the middle of the computation would come back as an infinity where
    // the retail keeps a finite number on its stack.  Inside the range
    // every intermediate is a binary64 number and the two agree bit for bit.
    void GenDistVector3Box3Double(difffuzz::Rng& rng, std::uint32_t* in)
    {
      // a magnitude in [1e-100, 1e100]
      const auto Mag = [&rng]() -> double
      {
        const int e = static_cast<int>(rng.Below(201)) - 100;
        return std::ldexp(rng.Range(1.0, 10.0), e);
      };
      double d[18];
      // the point: a direction and a magnitude from the safe range
      {
        double v[3];
        difffuzz::GenUnitVec3d(rng, v);
        const double m = rng.Chance(60) ? rng.Range(0.0, 100.0) : Mag();
        for (int c = 0; c < 3; ++c) {
          d[c] = v[c]*m;
        }
      }
      for (int i = 3; i < 6; ++i) {                // the centre
        d[i] = rng.Chance(70) ? rng.Range(-100.0, 100.0) : Mag();
      }
      for (int a = 0; a < 3; ++a) {                // the three axes
        double v[3];
        difffuzz::GenUnitVec3d(rng, v);
        for (int c = 0; c < 3; ++c) {
          d[6 + 3*a + c] = v[c];
        }
      }
      for (int e = 0; e < 3; ++e) {                // the three extents
        d[15 + e] = rng.Chance(70) ? rng.Range(0.0, 100.0) : Mag();
      }
      for (int i = 0; i < 18; ++i) {
        difffuzz::StoreDouble(in + 2*i, d[i]);
      }
    }

    void DistVector3Box3dGetSquaredRetail(const std::uint32_t* in,
                                          std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double vec[3], box[15];
      for (int i = 0; i < 3; ++i) {
        vec[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 15; ++i) {
        box[i] = difffuzz::LoadDouble(in + 2*(3 + i));
      }
      wm3collretail::DistVector3Box3dCtor(storage, vec, box);
      difffuzz::StoreDouble(out,
          wm3collretail::DistVector3Box3dGetSquared(storage));
    }

    void DistVector3Box3dGetSquaredOurs(const std::uint32_t* in,
                                        std::uint32_t* out)
    {
      difffuzz::StoreDouble(out,
          wm3collharness::DistVector3Box3dGetSquared(in, in + 6));
    }

    // The retail's Get() takes the CRT's square root at 0x00A91000, and the
    // CRT's slow path for a non-finite argument reaches an import the
    // harness does not resolve - the run raises the trap code instead of
    // returning NaN.  The same ceiling as the float row, so the same bound:
    // a finite squared distance.
    void GenDistVector3Box3dGet(difffuzz::Rng& rng, std::uint32_t* in)
    {
      for (int attempt = 0; attempt < 32; ++attempt) {
        GenDistVector3Box3Double(rng, in);
        const double squared =
            wm3collharness::DistVector3Box3dGetSquared(in, in + 6);
        if (std::isfinite(squared)) {
          return;
        }
      }
      for (int i = 0; i < 18; ++i) {
        difffuzz::StoreDouble(in + 2*i, 1.0);
      }
    }

    void DistVector3Box3dGetRetail(const std::uint32_t* in,
                                   std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double vec[3], box[15];
      for (int i = 0; i < 3; ++i) {
        vec[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 15; ++i) {
        box[i] = difffuzz::LoadDouble(in + 2*(3 + i));
      }
      wm3collretail::DistVector3Box3dCtor(storage, vec, box);
      difffuzz::StoreDouble(out, wm3collretail::DistVector3Box3dGet(storage));
    }

    void DistVector3Box3dGetOurs(const std::uint32_t* in,
                                 std::uint32_t* out)
    {
      difffuzz::StoreDouble(out,
          wm3collharness::DistVector3Box3dGet(in, in + 6));
    }

    // The swept forms of the box: the vector, the box, the time and the two
    // velocities - fifty words.
    //
    // The time is zero in a third of the draws and the velocities are zero
    // with it, so the moved pair is exactly the pair below; in the rest the
    // point moves towards the box's centre and the box moves away from the
    // point, which is a real relative motion.  The retry is the one the
    // static rows use - the retail's swept Get takes the CRT's square root,
    // and a non-finite argument is a fatal math error in the retail too.
    void GenDistVector3Box3dSwept(difffuzz::Rng& rng, std::uint32_t* in)
    {
      for (int attempt = 0; attempt < 32; ++attempt) {
        GenDistVector3Box3Double(rng, in);
        const bool still = rng.Chance(35);
        difffuzz::StoreDouble(in + 36,
            still ? 0.0
                  : (rng.Chance(50) ? rng.Range(0.0, 1.0) : rng.Range(-4.0, 4.0)));
        // the point's velocity points at the box's centre
        double toCentre[3];
        for (int i = 0; i < 3; ++i) {
          toCentre[i] = difffuzz::LoadDouble(in + 2*(3 + i))
                      - difffuzz::LoadDouble(in + 2*i);
        }
        for (int i = 0; i < 3; ++i) {
          double v = 0.0;
          if (!still) {
            v = rng.Chance(60) ? toCentre[i]*rng.Range(0.1, 2.0)
                               : rng.Range(-20.0, 20.0);
          }
          difffuzz::StoreDouble(in + 2*(19 + i), v);
        }
        for (int i = 0; i < 3; ++i) {
          double v = 0.0;
          if (!still && rng.Chance(35)) {
            v = rng.Range(-20.0, 20.0);
          }
          difffuzz::StoreDouble(in + 2*(22 + i), v);
        }
        const double fT = difffuzz::LoadDouble(in + 36);
        const double squared =
            wm3collharness::DistVector3Box3dGetSquaredSwept(in, in + 6, fT,
                in + 38, in + 44);
        if (std::isfinite(squared)) {
          return;
        }
      }
    }

    void DistVector3Box3dGetSweptRetail(const std::uint32_t* in,
                                        std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double vec[3], box[15], vel0[3], vel1[3];
      for (int i = 0; i < 3; ++i) {
        vec[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 15; ++i) {
        box[i] = difffuzz::LoadDouble(in + 2*(3 + i));
      }
      const double fT = difffuzz::LoadDouble(in + 36);
      for (int i = 0; i < 3; ++i) {
        vel0[i] = difffuzz::LoadDouble(in + 2*(19 + i));
        vel1[i] = difffuzz::LoadDouble(in + 2*(22 + i));
      }
      wm3collretail::DistVector3Box3dCtor(storage, vec, box);
      double value = 0.0;
      wm3collretail::DistVector3Box3dGetSwept(storage, fT, vel0, vel1, &value);
      difffuzz::StoreDouble(out, value);
    }

    void DistVector3Box3dGetSweptOurs(const std::uint32_t* in,
                                      std::uint32_t* out)
    {
      const double fT = difffuzz::LoadDouble(in + 36);
      difffuzz::StoreDouble(out,
          wm3collharness::DistVector3Box3dGetSwept(in, in + 6, fT, in + 38,
              in + 44));
    }

    void DistVector3Box3dGetSquaredSweptRetail(const std::uint32_t* in,
                                               std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double vec[3], box[15], vel0[3], vel1[3];
      for (int i = 0; i < 3; ++i) {
        vec[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 15; ++i) {
        box[i] = difffuzz::LoadDouble(in + 2*(3 + i));
      }
      const double fT = difffuzz::LoadDouble(in + 36);
      for (int i = 0; i < 3; ++i) {
        vel0[i] = difffuzz::LoadDouble(in + 2*(19 + i));
        vel1[i] = difffuzz::LoadDouble(in + 2*(22 + i));
      }
      wm3collretail::DistVector3Box3dCtor(storage, vec, box);
      double value = 0.0;
      wm3collretail::DistVector3Box3dGetSquaredSwept(storage, fT, vel0, vel1,
                                                      &value);
      difffuzz::StoreDouble(out, value);
    }

    void DistVector3Box3dGetSquaredSweptOurs(const std::uint32_t* in,
                                             std::uint32_t* out)
    {
      const double fT = difffuzz::LoadDouble(in + 36);
      difffuzz::StoreDouble(out,
          wm3collharness::DistVector3Box3dGetSquaredSwept(in, in + 6, fT,
              in + 38, in + 44));
    }

    // ---- Wm3::IntrSegment3Capsule3<double> ----------------------------------
    //
    // The double instantiation: the segment is seven doubles and the capsule
    // eight - the segment's seven and the radius.  Thirty words.
    //
    // The magnitudes stay inside [1e-100,1e100], which is what keeps the
    // modelled arithmetic exact; what that leaves out of reach is the x87's
    // extended exponent range itself, i.e. the overflow and the underflow of
    // the products.
    //
    // Half of the draws are an exact collision, so that the row witnesses
    // the precision of the distance class under it: both segments
    // degenerate to points, one 1 + 5 * 2^-26 above the other along y, and
    // the capsule's radius is 1 + 6 * 2^-26.
    //
    // The x87 rounds the difference of the two origins to its 24-bit
    // significand, which is 1 + 2^-23 (the gap is 0.625 of an ulp above
    // one), and the squared distance is then (1 + 2^-23)^2 = 1 + 2^-22, so
    // the distance is 1 + 2^-23 - which is above the radius and the test
    // is false.  The binary64 chain keeps the difference exact, so its
    // square root is the gap itself, 1 + 5 * 2^-26, which is below the
    // radius and the test is true: the same draw is a disagreement.
    //
    // The two values are built from their bits: the harness runs under
    // _PC_24, so 1 + ldexp(1.0,-26) would be 1.0 in the generator itself.
    void GenIntrSegment3Capsule3Double(difffuzz::Rng& rng, std::uint32_t* in)
    {
      // a magnitude in [1e-100, 1e100]
      const auto Mag = [&rng]() -> double
      {
        const int e = static_cast<int>(rng.Below(201)) - 100;
        return std::ldexp(rng.Range(1.0, 10.0), e);
      };
      double d[15];
      if (rng.Chance(50)) {
        // the collision: two points, one 1 + 5 * 2^-26 above the other
        const std::uint64_t kGap = 0x3FF0000014000000ull;    // 1 + 5 * 2^-26
        const std::uint64_t kRadius = 0x3FF0000018000000ull; // 1 + 6 * 2^-26
        double fGap = 0.0, fRadius = 0.0;
        std::memcpy(&fGap, &kGap, sizeof fGap);
        std::memcpy(&fRadius, &kRadius, sizeof fRadius);
        for (int i = 0; i < 15; ++i) {
          d[i] = 0.0;
        }
        d[3] = 1.0;              // the segment's direction
        d[8] = fGap;             // the capsule's segment origin, along y
        d[11] = 1.0;             // the capsule's segment direction
        d[14] = fRadius;         // the capsule's radius
      }
      else {
        // the segment: the origin, the direction and the extent
        for (int i = 0; i < 3; ++i) {
          d[i] = rng.Chance(75) ? rng.Range(-10.0, 10.0) : Mag();
        }
        for (int i = 3; i < 6; ++i) {
          d[i] = rng.Chance(60) ? rng.Range(-1.0, 1.0) : Mag();
        }
        d[6] = rng.Chance(80) ? rng.Range(-1000.0, 1000.0) : Mag();
        // the capsule: its own segment and the radius
        for (int i = 7; i < 13; ++i) {
          d[i] = rng.Chance(75) ? rng.Range(-10.0, 10.0) : Mag();
        }
        d[13] = rng.Chance(80) ? rng.Range(-1000.0, 1000.0) : Mag();
        d[14] = rng.Chance(60) ? rng.Range(0.1, 20.0) : Mag();
      }
      for (int i = 0; i < 15; ++i) {
        difffuzz::StoreDouble(in + 2*i, d[i]);
      }
    }

    void IntrSegment3Capsule3dTestRetail(const std::uint32_t* in,
                                         std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double segment[7], capsule[8];
      for (int i = 0; i < 7; ++i) {
        segment[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 8; ++i) {
        capsule[i] = difffuzz::LoadDouble(in + 2*(7 + i));
      }
      wm3collretail::IntrSegment3Capsule3dCtor(storage, segment, capsule);
      out[0] = wm3collretail::IntrSegment3Capsule3dTest(storage);
    }

    void IntrSegment3Capsule3dTestOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      out[0] = wm3collharness::IntrSegment3Capsule3dTest(in, in + 14);
    }

// The Find's generator.  The magnitudes stay in a moderate range: the
    // rebuild's double branch carries its intermediates through memory (a
    // binary64) where the retail keeps them on the x87 stack, and the two
    // agree only while the values fit a binary64 - so the products have to
    // stay far below 1e308.
    //
    // Three families of draws:
    //   - the exact tangent: the capsule's spine on the z-axis at the origin
    //     and the line through (-r,0,0) along (0,1,0).  In the capsule's
    //     frame (the basis is (-1,0,0) and (0,-1,0) for that axis) the
    //     line's origin is (r,0,0) and its direction is (0,-1,0), so fA1 is
    //     zero and fA0 is r*r - r*r, exactly zero: the discriminant is
    //     exactly zero, which is the ZERO_TOLERANCE branch;
    //   - the parallel line: the query's direction is the capsule's
    //     direction, so the line runs along the spine's axis, which is the
    //     other branch of the body's first test;
    //   - the ordinary draws: a random segment against a random capsule.
    void GenIntrSegment3Capsule3DoubleFind(difffuzz::Rng& rng,
                                           std::uint32_t* in)
    {
      double d[15];
      if (rng.Chance(10)) {
        // the exact tangent
        const double radius = rng.Range(0.2, 5.0);
        for (int i = 0; i < 15; ++i) {
          d[i] = 0.0;
        }
        d[0] = -radius;          // the query's origin
        d[4] = 1.0;              // the query's direction, along y
        d[6] = rng.Range(0.1, 20.0);
        d[10] = 1.0;             // the capsule spine's direction, along z
        d[13] = rng.Range(0.1, 20.0);
        d[14] = radius;
      }
      else {
        // the query segment: the origin, the direction and the extent
        for (int i = 0; i < 3; ++i) {
          d[i] = rng.Range(-10.0, 10.0);
        }
        if (rng.Chance(10)) {
          // the line runs along the capsule's axis: the parallel branch
          d[3] = 0.0; d[4] = 0.0; d[5] = 1.0;
        }
        else {
          // a unit direction
          const double u = rng.Range(-1.0, 1.0);
          const double v = rng.Range(-1.0, 1.0);
          double w = 1.0 - u*u - v*v;
          w = (w > 0.0) ? std::sqrt(w) : 0.0;
          d[3] = u; d[4] = v; d[5] = w;
        }
        d[6] = rng.Range(0.1, 50.0);
        // the capsule: its own spine and the radius
        for (int i = 7; i < 10; ++i) {
          d[i] = rng.Range(-10.0, 10.0);
        }
        const double u = rng.Range(-1.0, 1.0);
        const double v = rng.Range(-1.0, 1.0);
        double w = 1.0 - u*u - v*v;
        w = (w > 0.0) ? std::sqrt(w) : 0.0;
        d[10] = u; d[11] = v; d[12] = w;
        d[13] = rng.Range(0.1, 50.0);
        d[14] = rng.Range(0.1, 20.0);
      }
      for (int i = 0; i < 15; ++i) {
        difffuzz::StoreDouble(in + 2*i, d[i]);
      }
    }

    // The retail's double Find (0x00A6C3C0).  It writes the intersection
    // type at [this+0x10], the quantity at [this+0x20] and the two points at
    // [this+0x28] and [this+0x40].  The parameters are not compared: the
    // retail's array holds every root the static Find produced, while the
    // rebuild's holds the roots the extent filter kept.
    void IntrSegment3Capsule3dFindRetail(const std::uint32_t* in,
                                         std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double segment[7], capsule[8];
      for (int i = 0; i < 7; ++i) {
        segment[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 8; ++i) {
        capsule[i] = difffuzz::LoadDouble(in + 2*(7 + i));
      }
      wm3collretail::IntrSegment3Capsule3dCtor(storage, segment, capsule);
      const std::uint32_t hit =
          wm3collretail::IntrSegment3Capsule3dFind(storage);
      std::uint32_t quantity = 0u;
      std::memcpy(&quantity, storage + 0x20, sizeof quantity);
      out[0] = hit;
      std::memcpy(out + 1, storage + 0x10, sizeof(std::uint32_t));
      out[2] = quantity;
      for (int i = 3; i < 15; ++i) {
        out[i] = 0u;
      }
      if (quantity > 0) {
        double point[3];
        std::memcpy(point, storage + 0x28, 3*sizeof(double));
        for (int i = 0; i < 3; ++i) {
          difffuzz::StoreDouble(out + 3 + 2*i, point[i]);
        }
      }
      if (quantity > 1) {
        double point[3];
        std::memcpy(point, storage + 0x28 + 24, 3*sizeof(double));
        for (int i = 0; i < 3; ++i) {
          difffuzz::StoreDouble(out + 9 + 2*i, point[i]);
        }
      }
    }

    void IntrSegment3Capsule3dFindOurs(const std::uint32_t* in,
                                        std::uint32_t* out)
    {
      wm3collharness::IntrSegment3Capsule3dFind(in, in + 14, out);
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
      // The three values are built from their *bit patterns*, not from
      // arithmetic.  That matters: the generator runs inside the harness's
      // loop, and the harness leaves the x87 control word at `_PC_24`
      // (0x007F) between the two sides - so an expression like
      // `1.0 + std::ldexp(1.0,-30)` is *evaluated at 24 bits* and comes out
      // as 1.0, which is exactly the value the construction must not have.
      // A memcpy from a literal cannot be rounded.
      inline double FromBits64(const std::uint64_t b) noexcept
      {
        double d = 0.0;
        std::memcpy(&d, &b, sizeof d);
        return d;
      }
      // 1 + 2^-30: exact in binary64, rounds to 1.0 in 24 bits.
      inline double CollideValue() { return FromBits64(0x3FF0000000400000ull); }
      // 1 - 2^-32: exact in binary64; the x87 loads it without rounding.
      inline double CollideExtent() { return FromBits64(0x3FEFFFFFFFE00000ull); }
      // 3 * 2^-32: between the two sides' results.
      inline double CollideRadius() { return FromBits64(0x3E08000000000000ull); }
      // A unit axis: the box's three axes are the identity.
      inline void IdentityAxes(double* d, const int first)
      {
        d[first + 0] = 1.0; d[first + 1] = 0.0; d[first + 2] = 0.0;
        d[first + 3] = 0.0; d[first + 4] = 1.0; d[first + 5] = 0.0;
        d[first + 6] = 0.0; d[first + 7] = 0.0; d[first + 8] = 1.0;
      }
    } // namespace

    // ---- Wm3::IntrBox3Sphere3<double> --------------------------------------
    //
    // The double instantiation: the box is fifteen doubles and the sphere
    // four - the centre and the radius.  Thirty-eight words.
    //
    // The magnitudes stay inside [1e-100,1e100], which is what keeps the
    // modelled arithmetic exact; what that leaves out of reach is the x87's
    // extended exponent range itself, i.e. the overflow and the underflow of
    // the products.
    //
    // Half of the draws are the exact-collision family above: the box's axes
    // are the identity, the sphere's centre sits one 2^-30 past one of the
    // faces and that face is 2^-32 short of the coordinate, so the two sides
    // compute different distances to the face and the radius (3 * 2^-32)
    // sits between them.  The other half is the random geometry, which keeps
    // the other regions covered.
    void GenIntrBox3Sphere3Double(difffuzz::Rng& rng, std::uint32_t* in)
    {
      const auto Mag = [&rng]() -> double
      {
        const int e = static_cast<int>(rng.Below(201)) - 100;
        return std::ldexp(rng.Range(1.0, 10.0), e);
      };
      double d[19];
      if (rng.Chance(50))
      {
        for (int i = 0; i < 19; ++i) {
          d[i] = 0.0;
        }
        IdentityAxes(d, 3);              // the box's axes
        d[12] = 1.0; d[13] = 1.0; d[14] = 1.0;   // the extents
        const int k = static_cast<int>(rng.Below(3));   // the offending axis
        d[12 + k] = CollideExtent();     // the face is 2^-32 short
        d[15 + k] = CollideValue();      // the centre is 2^-30 past it
        for (int j = 0; j < 3; ++j) {
          if (j != k) {
            d[15 + j] = rng.Range(-0.5, 0.5);   // still inside the box
          }
        }
        d[18] = CollideRadius();         // between the two sides' results
      }
      else
      {
      for (int i = 0; i < 18; ++i) {
        d[i] = rng.Chance(75) ? rng.Range(-1000.0, 1000.0) : Mag();
      }
      // the radius is drawn around the box's own extent, so that the
      // near-touching and the clearly-inside cases are both common
      const double reach = (std::fabs(d[6]) + std::fabs(d[7]) + std::fabs(d[8])) / 3.0;
      d[18] = rng.Chance(70) ? reach * rng.Range(0.0, 2.5) : Mag();
      }
      for (int i = 0; i < 19; ++i) {
        difffuzz::StoreDouble(in + 2*i, d[i]);
      }
    }

    void IntrBox3Sphere3dTestRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double box[15], sphere[4];
      for (int i = 0; i < 15; ++i) {
        box[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 4; ++i) {
        sphere[i] = difffuzz::LoadDouble(in + 2*(15 + i));
      }
      wm3collretail::IntrBox3Sphere3dCtor(storage, box, sphere);
      out[0] = wm3collretail::IntrBox3Sphere3dTest(storage);
    }

    void IntrBox3Sphere3dTestOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      out[0] = wm3collharness::IntrBox3Sphere3dTest(in, in + 30);
    }

    // ---- the double instantiation of the Intr* classes --------------------
    //
    // The magnitudes stay inside [1e-100,1e100], which is what keeps the
    // modelled arithmetic exact; what that leaves out of reach is the x87's
    // extended exponent range itself, i.e. the overflow and the underflow of
    // the products.
    namespace
    {
      // a magnitude in [1e-100, 1e100]
      inline double IntrMag(difffuzz::Rng& rng)
      {
        const int e = static_cast<int>(rng.Below(201)) - 100;
        return std::ldexp(rng.Range(1.0, 10.0), e);
      }
    }

    void GenIntrBox2Box2Double(difffuzz::Rng& rng, std::uint32_t* in)
    {
      double d[16];
      // box 0: the centre, the axes and the extents
      for (int i = 0; i < 2; ++i) {
        d[i] = rng.Range(-100.0, 100.0);
      }
      d[2] = 1.0; d[3] = 0.0;              // axis 0
      d[4] = 0.0; d[5] = 1.0;              // axis 1
      d[6] = rng.Range(2.0, 30.0);          // extent x, positive
      d[7] = rng.Range(2.0, 30.0);          // extent y, positive
      // box 1: the same shape, rotated, with its centre near the point where
      // the two boxes start to touch
      const double angle = rng.Range(0.0, 3.1415926535897932);
      const double ca = std::cos(angle), sa = std::sin(angle);
      d[8] = 0.0; d[9] = 0.0;
      d[10] = ca;  d[11] = -sa;            // axis 0, rotated
      d[12] = sa;  d[13] = ca;             // axis 1, rotated
      d[14] = d[6] * rng.Range(0.3, 1.5);
      d[15] = d[7] * rng.Range(0.3, 1.5);
      const double reach = d[6] + d[7] + std::fabs(d[14]) + std::fabs(d[15]);
      const double dist = reach * rng.Range(0.3, 1.2);
      const double dir = rng.Range(0.0, 6.2831853071795864);
      d[8] = d[0] + dist * std::cos(dir);
      d[9] = d[1] + dist * std::sin(dir);
      for (int i = 0; i < 16; ++i) {
        difffuzz::StoreDouble(in + 2*i, d[i]);
      }
    }

    // The double instantiation of IntrLine3Box3: the line is six doubles (the
    // origin and the direction) and the box fifteen - forty-two words.
    //
    // Half of the draws are the exact-collision family: the direction is a
    // unit axis, the box's axes are the identity and its extents are one,
    // and the line's origin is one 2^-30 away from the box's centre along a
    // coordinate that is not the direction's.  The cross product of the two
    // then has a component of 1.0 on the x87 and 1 + 2^-30 in binary64, and
    // the right-hand side of that test is exactly 1.0 - so the retail
    // answers true and binary64 answers false.
    void GenIntrLine3Box3Double(difffuzz::Rng& rng, std::uint32_t* in)
    {
      double d[21];
      if (rng.Chance(50))
      {
        for (int i = 0; i < 21; ++i) {
          d[i] = 0.0;
        }
        const int k = static_cast<int>(rng.Below(3));    // the direction's axis
        const int j = (k + 1 + static_cast<int>(rng.Below(2))) % 3;
        d[3 + k] = 1.0;                 // the direction is a unit axis
        d[j] = CollideValue();          // the origin is 2^-30 off the centre
        IdentityAxes(d, 9);             // the box's axes
        d[18] = 1.0; d[19] = 1.0; d[20] = 1.0;   // the extents
      }
      else
      {
      for (int i = 0; i < 6; ++i) {
        d[i] = rng.Chance(60) ? rng.Range(-1000.0, 1000.0) : IntrMag(rng);
      }
      for (int i = 6; i < 21; ++i) {
        d[i] = rng.Chance(75) ? rng.Range(-1000.0, 1000.0) : IntrMag(rng);
      }
      }
      for (int i = 0; i < 21; ++i) {
        difffuzz::StoreDouble(in + 2*i, d[i]);
      }
    }

    // The double instantiation of IntrSegment3Box3: the segment is seven
    // doubles (the origin, the direction and the extent) and the box fifteen
    // - forty-four words, plus the `solid` flag in a word of its own.
    //
    // Half of the draws are the exact-collision family: the direction is a
    // unit axis, the box's axes are the identity and its extents are one, and
    // the segment's origin is one 2^-30 off the box's centre along a
    // coordinate that is not the direction's.  The first test then compares
    // the retail's 1.0 against a right-hand side of exactly 1.0 (true) and
    // the binary64 1 + 2^-30 against the same 1.0 (false).
    void GenIntrSegment3Box3Double(difffuzz::Rng& rng, std::uint32_t* in)
    {
      double d[22];
      if (rng.Chance(50))
      {
        for (int i = 0; i < 22; ++i) {
          d[i] = 0.0;
        }
        const int k = static_cast<int>(rng.Below(3));    // the direction's axis
        const int j = (k + 1 + static_cast<int>(rng.Below(2))) % 3;
        d[3 + k] = 1.0;                 // the direction is a unit axis
        d[j] = CollideValue();          // the origin is 2^-30 off the centre
        d[6] = 1.0;                     // the segment's extent
        IdentityAxes(d, 10);            // the box's axes
        d[19] = 1.0; d[20] = 1.0; d[21] = 1.0;   // the extents
      }
      else
      {
      for (int i = 0; i < 9; ++i) {
        d[i] = rng.Chance(75) ? rng.Range(-10.0, 10.0) : IntrMag(rng);
      }
      d[9] = rng.Chance(80) ? rng.Range(-1000.0, 1000.0) : IntrMag(rng);
      for (int i = 10; i < 22; ++i) {
        d[i] = rng.Chance(75) ? rng.Range(-1000.0, 1000.0) : IntrMag(rng);
      }
      }
      for (int i = 0; i < 22; ++i) {
        difffuzz::StoreDouble(in + 2*i, d[i]);
      }
      in[44] = rng.Chance(50) ? 1u : 0u;    // the `solid` flag, its own word
    }

    // The double instantiation of IntrSegment3Sphere3: the segment is seven
    // doubles (the origin, the direction and the extent) and the sphere four
    // (the centre and the radius) - twenty-two words.
    //
    // Half of the draws are the exact-collision family: the sphere's centre
    // is the origin, its radius is one, the segment's direction is a unit
    // axis and its origin is one 2^-30 off the centre along a coordinate
    // that is not the direction's.  The retail's fA0 is then 0 - the x87
    // rounds (1 + 2^-30)^2 to 1.0 - and its fDiscr is 0, so it goes on and
    // answers true; the binary64 fA0 is 2^-29 and its fDiscr is negative,
    // so it answers false.
    void GenIntrSegment3Sphere3Double(difffuzz::Rng& rng, std::uint32_t* in)
    {
      double d[11];
      if (rng.Chance(50))
      {
        for (int i = 0; i < 11; ++i) {
          d[i] = 0.0;
        }
        const int k = static_cast<int>(rng.Below(3));    // the direction's axis
        const int j = (k + 1 + static_cast<int>(rng.Below(2))) % 3;
        d[3 + k] = 1.0;                 // the direction is a unit axis
        d[j] = CollideValue();          // the origin is 2^-30 off the centre
        d[6] = 1.0;                     // the segment's extent
        d[10] = 1.0;                    // the sphere's radius
      }
      else
      {
      for (int i = 0; i < 9; ++i) {
        d[i] = rng.Chance(75) ? rng.Range(-10.0, 10.0) : IntrMag(rng);
      }
      d[9] = rng.Chance(80) ? rng.Range(-1000.0, 1000.0) : IntrMag(rng);
      const double reach = (std::fabs(d[3]) + std::fabs(d[4]) + std::fabs(d[5])) / 3.0;
      d[10] = rng.Chance(70) ? reach * rng.Range(0.0, 2.5) : IntrMag(rng);
      }
      for (int i = 0; i < 11; ++i) {
        difffuzz::StoreDouble(in + 2*i, d[i]);
      }
    }

    void IntrBox2Box2dTestRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double a[8], b[8];
      for (int i = 0; i < 8; ++i) {
        a[i] = difffuzz::LoadDouble(in + 2*i);
        b[i] = difffuzz::LoadDouble(in + 2*(8 + i));
      }
      wm3collretail::IntrBox2Box2dCtor(storage, a, b);
      out[0] = wm3collretail::IntrBox2Box2dTest(storage);
    }

    void IntrBox2Box2dTestOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      out[0] = wm3collharness::IntrBox2Box2dTest(in, in + 16);;
    }

    void IntrLine3Box3dTestRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double a[6], b[15];
      for (int i = 0; i < 6; ++i) {
        a[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 15; ++i) {
        b[i] = difffuzz::LoadDouble(in + 2*(6 + i));
      }
      wm3collretail::IntrLine3Box3dCtor(storage, a, b);
      out[0] = wm3collretail::IntrLine3Box3dTest(storage);
    }

    void IntrLine3Box3dTestOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      out[0] = wm3collharness::IntrLine3Box3dTest(in, in + 12);;
    }

    void IntrSegment3Box3dTestRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double a[7], b[15];
      for (int i = 0; i < 7; ++i) {
        a[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 15; ++i) {
        b[i] = difffuzz::LoadDouble(in + 2*(7 + i));
      }
      wm3collretail::IntrSegment3Box3dCtor(storage, a, b, in[44]);
      out[0] = wm3collretail::IntrSegment3Box3dTest(storage);
    }

    void IntrSegment3Box3dTestOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      out[0] = wm3collharness::IntrSegment3Box3dTest(in, in + 14, in[44]);;
    }

    void IntrSegment3Sphere3dTestRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double a[7], b[4];
      for (int i = 0; i < 7; ++i) {
        a[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 4; ++i) {
        b[i] = difffuzz::LoadDouble(in + 2*(7 + i));
      }
      wm3collretail::IntrSegment3Sphere3dCtor(storage, a, b);
      out[0] = wm3collretail::IntrSegment3Sphere3dTest(storage);
    }

    void IntrSegment3Sphere3dTestOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      out[0] = wm3collharness::IntrSegment3Sphere3dTest(in, in + 14);;
    }

    // ---- Wm3::IntrLine3Box3<double>::Find and Wm3::IntrSegment3Box3<double>::Find ---
    //
    // The two rows share the retail's double DoClipping (0x00A505F0) and its
    // double Clip (0x00A4FD90): the two Find bodies are thin wrappers that
    // call it with the parameter range and the solid flag.
    //
    // The output is the float rows' layout with the doubles: the hit flag,
    // the intersection type, the quantity and the two points.  The retail's
    // own members are read out of its object storage, and the double
    // instantiation's layout is not the float one's:
    //
    //   IntrLine3Box3<double>     the type at [this+0x10], the quantity at
    //                              [this+0x20] and the points at [this+0x28];
    //   IntrSegment3Box3<double>  the type at [this+0x10], the quantity at
    //                              [this+0x24] and the points at [this+0x28].
    //
    // The offsets come from the two Find bodies themselves (the `lea` of the
    // three out-parameters) and from the retail's own accessors:
    // 0x00A46370 is the double GetQuantity (`mov eax,[ecx+0x24]`) and
    // 0x00A46380 the double GetPoint (`lea eax,[ecx+eax*8+0x28]`).
    void IntrLine3Box3dFindRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double line[6], box[15];
      for (int i = 0; i < 6; ++i) {
        line[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 15; ++i) {
        box[i] = difffuzz::LoadDouble(in + 2*(6 + i));
      }
      wm3collretail::IntrLine3Box3dCtor(storage, line, box);
      const std::uint32_t hit = wm3collretail::IntrLine3Box3dFind(storage);
      std::uint32_t quantity = 0u;
      std::memcpy(&quantity, storage + 0x20, sizeof quantity);
      out[0] = hit;
      std::memcpy(out + 1, storage + 0x10, sizeof(std::uint32_t));
      out[2] = quantity;
      for (int i = 3; i < 15; ++i) {
        out[i] = 0u;
      }
      if (quantity > 0) {
        double point[3];
        std::memcpy(point, storage + 0x28, 3*sizeof(double));
        for (int i = 0; i < 3; ++i) {
          difffuzz::StoreDouble(out + 3 + 2*i, point[i]);
        }
      }
      if (quantity > 1) {
        double point[3];
        std::memcpy(point, storage + 0x28 + 24, 3*sizeof(double));
        for (int i = 0; i < 3; ++i) {
          difffuzz::StoreDouble(out + 9 + 2*i, point[i]);
        }
      }
    }

    void IntrLine3Box3dFindOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      wm3collharness::IntrLine3Box3dFind(in, in + 12, out);
    }

    void IntrSegment3Box3dFindRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double segment[7], box[15];
      for (int i = 0; i < 7; ++i) {
        segment[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 15; ++i) {
        box[i] = difffuzz::LoadDouble(in + 2*(7 + i));
      }
      wm3collretail::IntrSegment3Box3dCtor(storage, segment, box, in[44]);
      const std::uint32_t hit = wm3collretail::IntrSegment3Box3dFind(storage);
      std::uint32_t quantity = 0u;
      std::memcpy(&quantity, storage + 0x24, sizeof quantity);
      out[0] = hit;
      std::memcpy(out + 1, storage + 0x10, sizeof(std::uint32_t));
      out[2] = quantity;
      for (int i = 3; i < 15; ++i) {
        out[i] = 0u;
      }
      if (quantity > 0) {
        double point[3];
        std::memcpy(point, storage + 0x28, 3*sizeof(double));
        for (int i = 0; i < 3; ++i) {
          difffuzz::StoreDouble(out + 3 + 2*i, point[i]);
        }
      }
      if (quantity > 1) {
        double point[3];
        std::memcpy(point, storage + 0x28 + 24, 3*sizeof(double));
        for (int i = 0; i < 3; ++i) {
          difffuzz::StoreDouble(out + 9 + 2*i, point[i]);
        }
      }
    }

    void IntrSegment3Box3dFindOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      wm3collharness::IntrSegment3Box3dFind(in, in + 14, in[44], out);
    }

    // ---- Wm3::IntrSegment3Sphere3<double>::Find -------------------------------
    //
    // The retail's static Find (0x00A479B0) writes the quantity at
    // [this+0x28], the two points at [this+0x30] and [this+0x48] and the
    // two segment parameters at [this+0x60] and [this+0x68].  The
    // parameters are only written on the hit paths, so the row writes zero
    // for them when the quantity says there is no such point.
    void IntrSegment3Sphere3dFindRetail(const std::uint32_t* in,
                                        std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double segment[7], sphere[4];
      for (int i = 0; i < 7; ++i) {
        segment[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 4; ++i) {
        sphere[i] = difffuzz::LoadDouble(in + 2*(7 + i));
      }
      wm3collretail::IntrSegment3Sphere3dCtor(storage, segment, sphere);
      const std::uint32_t hit = wm3collretail::IntrSegment3Sphere3dFind(storage);
      std::uint32_t quantity = 0u;
      std::memcpy(&quantity, storage + 0x28, sizeof quantity);
      out[0] = hit;
      out[1] = quantity;
      for (int i = 2; i < 18; ++i) {
        out[i] = 0u;
      }
      if (quantity > 0) {
        double point[3], param;
        std::memcpy(point, storage + 0x30, 3*sizeof(double));
        for (int i = 0; i < 3; ++i) {
          difffuzz::StoreDouble(out + 2 + 2*i, point[i]);
        }
        std::memcpy(&param, storage + 0x60, sizeof param);
        difffuzz::StoreDouble(out + 14, param);
      }
      if (quantity > 1) {
        double point[3], param;
        std::memcpy(point, storage + 0x48, 3*sizeof(double));
        for (int i = 0; i < 3; ++i) {
          difffuzz::StoreDouble(out + 8 + 2*i, point[i]);
        }
        std::memcpy(&param, storage + 0x68, sizeof param);
        difffuzz::StoreDouble(out + 16, param);
      }
    }

    void IntrSegment3Sphere3dFindOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      wm3collharness::IntrSegment3Sphere3dFind(in, in + 14, out);
    }

    // ---- Wm3::IntrSegment3Sphere3<double>::Find(fTMax, v0, v1) ------------
    //
    // The dynamic Find, the double instantiation.  The input is the static
    // Find's (the segment and the sphere) plus the time limit and the two
    // velocities - eighteen doubles, thirty-six words.
    //
    // The generator draws the four cases the retail's body branches on:
    //
    //   * the sphere already touches the segment - the query's own static
    //     Find answers true and the row sees the early-hit path: the
    //     contact time zero and the intersection type IT_OTHER;
    //   * the sphere is aimed at a point of the segment's interior - the
    //     contact is on the swept path and the point is the interior one;
    //   * the sphere is aimed past one of the segment's ends - the point is
    //     that end point;
    //   * the sphere is aimed at random - most of those miss.
    //
    // The two velocities are drawn so that their difference - the only
    // thing the swept query sees - is the drawn relative velocity: the
    // first velocity is drawn on its own and the second one is that plus
    // the relative velocity.
    void GenIntrSegment3Sphere3DoubleDyn(difffuzz::Rng& rng,
                                          std::uint32_t* in)
    {
      double d[18];
      // the segment: the origin, the direction and the extent
      for (int i = 0; i < 3; ++i) {
        d[i] = rng.Range(-10.0, 10.0);
      }
      double u = rng.Range(-1.0, 1.0);
      double v = rng.Range(-1.0, 1.0);
      double w = 1.0 - u*u - v*v;
      w = (w > 0.0) ? std::sqrt(w) : 0.0;
      d[3] = u; d[4] = v; d[5] = w;
      d[6] = rng.Range(0.5, 20.0);
      // the sphere: the centre and the radius
      for (int i = 7; i < 10; ++i) {
        d[i] = rng.Range(-10.0, 10.0);
      }
      d[10] = rng.Range(0.1, 5.0);

      const int iCase = static_cast<int>(rng.Below(4));
      double fSpeed = 0.0;
      double fReach = 0.0;
      if (iCase == 0) {
        // the sphere already touches the segment: its centre sits on it
        const double s = rng.Range(-d[6], d[6]);
        d[7] = d[0] + s*d[3];
        d[8] = d[1] + s*d[4];
        d[9] = d[2] + s*d[5];
        u = rng.Range(-1.0, 1.0);
        v = rng.Range(-1.0, 1.0);
        w = 1.0 - u*u - v*v;
        w = (w > 0.0) ? std::sqrt(w) : 0.0;
        fSpeed = d[10] * rng.Range(0.5, 3.0);
        d[15] = u*fSpeed; d[16] = v*fSpeed; d[17] = w*fSpeed;
      }
      else if (iCase == 1 || iCase == 2) {
        // the sphere is aimed at a point of the segment's line: one of its
        // interior points, or one past one of its ends
        double s;
        if (iCase == 1) {
          s = rng.Range(-d[6], d[6]);
        }
        else {
          const double extra = rng.Range(0.5, 5.0);
          s = rng.Chance(50) ? -(d[6] + extra) : (d[6] + extra);
        }
        const double tx = d[0] + s*d[3];
        const double ty = d[1] + s*d[4];
        const double tz = d[2] + s*d[5];
        double dx = tx - d[7];
        double dy = ty - d[8];
        double dz = tz - d[9];
        const double fLen = std::sqrt(dx*dx + dy*dy + dz*dz);
        fSpeed = d[10] * rng.Range(0.5, 3.0);
        fReach = fLen;
        if (fLen > 0.0) {
          dx = dx/fLen*fSpeed;
          dy = dy/fLen*fSpeed;
          dz = dz/fLen*fSpeed;
        }
        else {
          dx = 0.0; dy = 0.0; dz = 0.0;
        }
        d[15] = dx; d[16] = dy; d[17] = dz;
      }
      else {
        // a random relative velocity: most of those miss
        u = rng.Range(-1.0, 1.0);
        v = rng.Range(-1.0, 1.0);
        w = 1.0 - u*u - v*v;
        w = (w > 0.0) ? std::sqrt(w) : 0.0;
        fSpeed = d[10] * rng.Range(0.0, 3.0);
        d[15] = u*fSpeed; d[16] = v*fSpeed; d[17] = w*fSpeed;
      }
      // the first velocity is drawn on its own, so that the difference -
      // the relative velocity the swept query sees - is the drawn one
      for (int i = 0; i < 3; ++i) {
        d[12 + i] = rng.Range(-2.0, 2.0);
        d[15 + i] = d[12 + i] + d[15 + i];
      }
      // the time limit: zero (the contact has to be immediate) or twice the
      // time the sphere needs to reach the point it was aimed at
      d[11] = rng.Chance(30) ? 0.0
            : (fSpeed > 0.0 ? 2.0*fReach/fSpeed
                            : (d[10] > 0.0 ? 4.0*d[10]/fSpeed : 0.0));
      for (int i = 0; i < 18; ++i) {
        difffuzz::StoreDouble(in + 2*i, d[i]);
      }
    }

    // The retail's dynamic double Find (0x00A47690).  It writes the contact
    // time at [this+0x08], the intersection type at [this+0x10] and the
    // contact point at [this+0x30].  The point is only written on the hit
    // path, so the row writes zero for it when there is no hit.
    void IntrSegment3Sphere3dFindDynamicRetail(const std::uint32_t* in,
                                                std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double segment[7], sphere[4], vel0[3], vel1[3];
      for (int i = 0; i < 7; ++i) {
        segment[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 4; ++i) {
        sphere[i] = difffuzz::LoadDouble(in + 2*(7 + i));
      }
      for (int i = 0; i < 3; ++i) {
        vel0[i] = difffuzz::LoadDouble(in + 24 + 2*i);
        vel1[i] = difffuzz::LoadDouble(in + 30 + 2*i);
      }
      wm3collretail::IntrSegment3Sphere3dCtor(storage, segment, sphere);
      const std::uint32_t hit = wm3collretail::IntrSegment3Sphere3dFindDynamic(
          storage, difffuzz::LoadDouble(in + 22), vel0, vel1);
      out[0] = hit;
      std::memcpy(out + 1, storage + 0x10, sizeof(std::uint32_t));
      double contact = 0.0;
      std::memcpy(&contact, storage + 0x08, sizeof contact);
      difffuzz::StoreDouble(out + 2, contact);
      for (int i = 4; i < 10; ++i) {
        out[i] = 0u;
      }
      if (hit) {
        double point[3];
        std::memcpy(point, storage + 0x30, 3*sizeof(double));
        for (int i = 0; i < 3; ++i) {
          difffuzz::StoreDouble(out + 4 + 2*i, point[i]);
        }
      }
    }

    void IntrSegment3Sphere3dFindDynamicOurs(const std::uint32_t* in,
                                               std::uint32_t* out)
    {
      wm3collharness::IntrSegment3Sphere3dFindDynamic(in, in + 14, in + 22,
          in + 24, in + 30, out);
    }

    // ---- Wm3::IntrBox2Circle2<double> --------------------------------------
    //
    // The double instantiation: the box is eight doubles and the circle three
    // - twenty-two words.
    //
    // The box's extents are drawn with GenPlain, which signs them, and the
    // axes are arbitrary rather than orthonormal - so the projections and the
    // extents are not comparable and the region dispatch compares against
    // nonsense.  That is deliberate: both sides then answer the same, and
    // the test is a comparison rather than a geometry.
    void GenIntrBox2Circle2Double(difffuzz::Rng& rng, std::uint32_t* in)
    {
      double d[11];
      if (rng.Chance(50))
      {
        // The exact-collision family: the box's axes are the identity, its
        // first extent is 2^-32 short of one, and the circle's centre is
        // 2^-30 past that face.  The retail's distance to the face is then
        // 2^-32 and the rebuild's is 5 * 2^-32, and the radius (3 * 2^-32)
        // sits between them: the retail answers true, binary64 answers
        // false.  The class's Test delegates to DistVector2Box2<double>,
        // whose Get() is the modelled x87 one, so this is a witness of the
        // distance class's double branch as well.
        for (int i = 0; i < 11; ++i) {
          d[i] = 0.0;
        }
        d[2] = 1.0; d[3] = 0.0;              // the box's axis 0
        d[4] = 0.0; d[5] = 1.0;              // the box's axis 1
        d[6] = CollideExtent();             // the x extent is 2^-32 short
        d[7] = 1.0;                         // the y extent
        d[8] = CollideValue();              // the circle's centre
        d[9] = rng.Range(-0.5, 0.5);         // still inside the y extent
        d[10] = CollideRadius();            // between the two sides' results
      }
      else
      {
      for (int i = 0; i < 8; ++i) {
        d[i] = GenPlain(rng, 100.0);
      }
      d[8] = GenPlain(rng, 100.0);
      d[9] = GenPlain(rng, 100.0);
      const double extent = (std::fabs(d[6]) + std::fabs(d[7])) * 0.5;
      d[10] = rng.Chance(70) ? extent * rng.Range(0.0, 2.5) : GenPlain(rng, 200.0);
      }
      for (int i = 0; i < 11; ++i) {
        difffuzz::StoreDouble(in + 2*i, d[i]);
      }
    }

    void IntrBox2Circle2dTestRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double box[8], circle[3];
      for (int i = 0; i < 8; ++i) {
        box[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 3; ++i) {
        circle[i] = difffuzz::LoadDouble(in + 2*(8 + i));
      }
      wm3collretail::IntrBox2Circle2dCtor(storage, box, circle);
      out[0] = wm3collretail::IntrBox2Circle2dTest(storage);
    }

    void IntrBox2Circle2dTestOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      out[0] = wm3collharness::IntrBox2Circle2dTest(in, in + 16);
    }

    // ---- Wm3::DistLine3Segment3<double> ------------------------------------
    //
    // The double instantiation: the line is six doubles (the origin and the
    // direction) and the segment seven (the origin, the direction and the
    // extent).  Twenty-six words.
    //
    // The directions are unit vectors, and in a fifth of the draws the
    // segment's direction is the line's - the degenerate branch the retail
    // takes when the determinant vanishes.  The extent is drawn positive,
    // because a negative one puts the algorithm's own branch tests outside
    // the range the retail and the rebuild agree on.
    void GenDistLine3Segment3Double(difffuzz::Rng& rng, std::uint32_t* in)
    {
      // a magnitude in [1e-100, 1e100]
      const auto Mag = [&rng]() -> double
      {
        const int e = static_cast<int>(rng.Below(201)) - 100;
        return std::ldexp(rng.Range(1.0, 10.0), e);
      };
      double d[13];
      for (int i = 0; i < 3; ++i) {
        d[i] = rng.Chance(60) ? rng.Range(-20.0, 20.0) : Mag();   // the line's origin
        d[6 + i] = rng.Chance(60) ? rng.Range(-20.0, 20.0) : Mag(); // the segment's origin
      }
      difffuzz::GenUnitVec3d(rng, d + 3);            // the line's direction
      if (rng.Chance(20)) {
        for (int i = 0; i < 3; ++i) {
          d[9 + i] = d[3 + i];                       // parallel
        }
      }
      else {
        difffuzz::GenUnitVec3d(rng, d + 9);         // the segment's direction
      }
      double extent = 0.0;
      if (rng.Chance(70)) {
        extent = rng.Chance(60) ? rng.Range(-100.0, 100.0) : Mag();
        if (extent < 0.0) {
          extent = -extent;
        }
      }
      else if (rng.Chance(80)) {
        extent = rng.Range(0.0, 1.0);
      }
      d[12] = extent;
      for (int i = 0; i < 13; ++i) {
        difffuzz::StoreDouble(in + 2*i, d[i]);
      }
    }

    // The swept forms: the line, the segment, the time and the two
    // velocities - forty words.  The time is zero in a third of the draws
    // and the velocities are zero with it, so the moved pair is exactly the
    // pair below.
    void GenDistLine3Segment3DoubleSwept(difffuzz::Rng& rng, std::uint32_t* in)
    {
      for (int attempt = 0; attempt < 32; ++attempt) {
        GenDistLine3Segment3Double(rng, in);
        const bool still = rng.Chance(35);
        difffuzz::StoreDouble(in + 26,
            still ? 0.0
                  : (rng.Chance(50) ? rng.Range(0.0, 1.0) : rng.Range(-4.0, 4.0)));
        for (int i = 0; i < 3; ++i) {
          double v = 0.0;
          if (!still) {
            v = rng.Range(-20.0, 20.0);
          }
          difffuzz::StoreDouble(in + 2*(14 + i), v);
        }
        for (int i = 0; i < 3; ++i) {
          double v = 0.0;
          if (!still && rng.Chance(35)) {
            v = rng.Range(-20.0, 20.0);
          }
          difffuzz::StoreDouble(in + 2*(17 + i), v);
        }
        const double fT = difffuzz::LoadDouble(in + 26);
        const double squared =
            wm3collharness::DistLine3Segment3dGetSquaredSwept(in, in + 12, fT,
                in + 28, in + 34);
        if (std::isfinite(squared)) {
          return;
        }
      }
    }

    void DistLine3Segment3dGetSquaredRetail(const std::uint32_t* in,
                                             std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double line[6], segment[7];
      for (int i = 0; i < 6; ++i) {
        line[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 7; ++i) {
        segment[i] = difffuzz::LoadDouble(in + 2*(6 + i));
      }
      wm3collretail::DistLine3Segment3dCtor(storage, line, segment);
      difffuzz::StoreDouble(out,
          wm3collretail::DistLine3Segment3dGetSquared(storage));
    }

    void DistLine3Segment3dGetSquaredOurs(const std::uint32_t* in,
                                          std::uint32_t* out)
    {
      difffuzz::StoreDouble(out,
          wm3collharness::DistLine3Segment3dGetSquared(in, in + 12));
    }

    void DistLine3Segment3dGetRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double line[6], segment[7];
      for (int i = 0; i < 6; ++i) {
        line[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 7; ++i) {
        segment[i] = difffuzz::LoadDouble(in + 2*(6 + i));
      }
      wm3collretail::DistLine3Segment3dCtor(storage, line, segment);
      difffuzz::StoreDouble(out,
          wm3collretail::DistLine3Segment3dGet(storage));
    }

    void DistLine3Segment3dGetOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      difffuzz::StoreDouble(out,
          wm3collharness::DistLine3Segment3dGet(in, in + 12));
    }

    void DistLine3Segment3dGetSweptRetail(const std::uint32_t* in,
                                          std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double line[6], segment[7], vel0[3], vel1[3];
      for (int i = 0; i < 6; ++i) {
        line[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 7; ++i) {
        segment[i] = difffuzz::LoadDouble(in + 2*(6 + i));
      }
      const double fT = difffuzz::LoadDouble(in + 26);
      for (int i = 0; i < 3; ++i) {
        vel0[i] = difffuzz::LoadDouble(in + 2*(14 + i));
        vel1[i] = difffuzz::LoadDouble(in + 2*(17 + i));
      }
      wm3collretail::DistLine3Segment3dCtor(storage, line, segment);
      double value = 0.0;
      wm3collretail::DistLine3Segment3dGetSwept(storage, fT, vel0, vel1, &value);
      difffuzz::StoreDouble(out, value);
    }

    void DistLine3Segment3dGetSweptOurs(const std::uint32_t* in,
                                         std::uint32_t* out)
    {
      const double fT = difffuzz::LoadDouble(in + 26);
      difffuzz::StoreDouble(out,
          wm3collharness::DistLine3Segment3dGetSwept(in, in + 12, fT,
              in + 28, in + 34));
    }

    void DistLine3Segment3dGetSquaredSweptRetail(const std::uint32_t* in,
                                                  std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double line[6], segment[7], vel0[3], vel1[3];
      for (int i = 0; i < 6; ++i) {
        line[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 7; ++i) {
        segment[i] = difffuzz::LoadDouble(in + 2*(6 + i));
      }
      const double fT = difffuzz::LoadDouble(in + 26);
      for (int i = 0; i < 3; ++i) {
        vel0[i] = difffuzz::LoadDouble(in + 2*(14 + i));
        vel1[i] = difffuzz::LoadDouble(in + 2*(17 + i));
      }
      wm3collretail::DistLine3Segment3dCtor(storage, line, segment);
      double value = 0.0;
      wm3collretail::DistLine3Segment3dGetSquaredSwept(storage, fT, vel0, vel1,
                                                      &value);
      difffuzz::StoreDouble(out, value);
    }

    void DistLine3Segment3dGetSquaredSweptOurs(const std::uint32_t* in,
                                                std::uint32_t* out)
    {
      const double fT = difffuzz::LoadDouble(in + 26);
      difffuzz::StoreDouble(out,
          wm3collharness::DistLine3Segment3dGetSquaredSwept(in, in + 12, fT,
              in + 28, in + 34));
    }

    // ---- Wm3::DistSegment3Segment3<double> ----------------------------------
    //
    // The double instantiation: two segments, seven doubles each - the
    // origin, the direction and the extent.  Twenty-eight words.
    //
    // The second segment's origin is put on the first segment's line and
    // offset sideways, so the two segments meet at a shallow angle most of
    // the time; in a twelfth of the draws the second segment is parallel or
    // antiparallel to the first, which is the degenerate branch the retail
    // takes when the determinant vanishes.
    void GenDistSegment3Segment3Double(difffuzz::Rng& rng, std::uint32_t* in)
    {
      // a magnitude in [1e-100, 1e100]
      const auto Mag = [&rng]() -> double
      {
        const int e = static_cast<int>(rng.Below(201)) - 100;
        return std::ldexp(rng.Range(1.0, 10.0), e);
      };
      double d[14];
      // segment 0: the origin, the direction, the extent
      for (int i = 0; i < 3; ++i) {
        d[i] = rng.Chance(60) ? rng.Range(-40.0, 40.0) : Mag();
      }
      difffuzz::GenUnitVec3d(rng, d + 3);
      d[6] = rng.Chance(3) ? 0.0
              : (rng.Chance(70) ? rng.Range(0.1, 20.0) : Mag());
      // segment 1: the origin is on the first segment's line, offset sideways
      double perp[3];
      {
        // a vector perpendicular to the first direction
        const double ax = d[3], ay = d[4], az = d[5];
        double bx = 0.0, by = 0.0, bz = 0.0;
        if (std::fabs(ax) < std::fabs(ay) && std::fabs(ax) < std::fabs(az)) {
          bx = 0.0; by = -az; bz = ay;
        }
        else if (std::fabs(ay) < std::fabs(az)) {
          bx = -az; by = 0.0; bz = ax;
        }
        else {
          bx = -ay; by = ax; bz = 0.0;
        }
        const double len = std::sqrt(bx*bx + by*by + bz*bz);
        perp[0] = bx/len; perp[1] = by/len; perp[2] = bz/len;
      }
      const double along = rng.Range(-40.0, 40.0);
      const double off = rng.Range(-25.0, 25.0);
      for (int i = 0; i < 3; ++i) {
        d[7 + i] = d[i] + along*d[3 + i] + off*perp[i];
      }
      if (rng.Chance(12)) {
        // parallel or antiparallel to the first segment
        const double s = rng.Chance(50) ? 1.0 : -1.0;
        for (int i = 0; i < 3; ++i) {
          d[10 + i] = s*d[3 + i];
        }
      }
      else {
        difffuzz::GenUnitVec3d(rng, d + 10);
      }
      d[13] = rng.Chance(3) ? 0.0
              : (rng.Chance(70) ? rng.Range(0.1, 20.0) : Mag());
      for (int i = 0; i < 14; ++i) {
        difffuzz::StoreDouble(in + 2*i, d[i]);
      }
    }

    // The Get() row's scope is finite results: retail's Get takes the CRT's
    // square root at 0x00A91000, whose slow path for a negative argument is
    // a fatal math error - in the retail as well, not only in this harness.
    // The squared distance is an absolute value, so it is never negative;
    // a NaN is the only thing to keep out, and the filter lives in this
    // row's generator.
    void GenDistSegment3Segment3DoubleGet(difffuzz::Rng& rng,
                                           std::uint32_t* in)
    {
      for (int attempt = 0; attempt < 32; ++attempt) {
        GenDistSegment3Segment3Double(rng, in);
        const double squared =
          wm3collharness::DistSegment3Segment3dGetSquared(in, in + 14);
        if (std::isfinite(squared)) {
          return;
        }
      }
      for (int i = 0; i < 14; ++i) {
        difffuzz::StoreDouble(in + 2*i, 1.0);
      }
    }

    // The swept forms: the two segments, the time and the two velocities -
    // forty-two words.  The time is zero in a third of the draws and the
    // velocities are zero with it, so the moved pair is exactly the pair
    // below.
    void GenDistSegment3Segment3DoubleSwept(difffuzz::Rng& rng,
                                            std::uint32_t* in)
    {
      for (int attempt = 0; attempt < 32; ++attempt) {
        GenDistSegment3Segment3Double(rng, in);
        const bool still = rng.Chance(35);
        difffuzz::StoreDouble(in + 28,
            still ? 0.0
                  : (rng.Chance(50) ? rng.Range(0.0, 1.0) : rng.Range(-4.0, 4.0)));
        // the first segment's velocity points at the second segment's origin
        double toOther[3];
        for (int i = 0; i < 3; ++i) {
          toOther[i] = difffuzz::LoadDouble(in + 2*(7 + i))
                     - difffuzz::LoadDouble(in + 2*i);
        }
        for (int i = 0; i < 3; ++i) {
          double v = 0.0;
          if (!still) {
            v = rng.Chance(60) ? toOther[i]*rng.Range(0.1, 2.0)
                               : rng.Range(-20.0, 20.0);
          }
          difffuzz::StoreDouble(in + 2*(15 + i), v);
        }
        for (int i = 0; i < 3; ++i) {
          double v = 0.0;
          if (!still && rng.Chance(35)) {
            v = rng.Range(-20.0, 20.0);
          }
          difffuzz::StoreDouble(in + 2*(18 + i), v);
        }
        const double fT = difffuzz::LoadDouble(in + 28);
        const double squared =
            wm3collharness::DistSegment3Segment3dGetSquaredSwept(in, in + 14,
                fT, in + 30, in + 36);
        if (std::isfinite(squared)) {
          return;
        }
      }
    }

    void DistSegment3Segment3dGetSquaredRetail(const std::uint32_t* in,
                                                std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double seg0[7], seg1[7];
      for (int i = 0; i < 7; ++i) {
        seg0[i] = difffuzz::LoadDouble(in + 2*i);
        seg1[i] = difffuzz::LoadDouble(in + 2*(7 + i));
      }
      wm3collretail::DistSegment3Segment3dCtor(storage, seg0, seg1);
      difffuzz::StoreDouble(out,
          wm3collretail::DistSegment3Segment3dGetSquared(storage));
    }

    void DistSegment3Segment3dGetSquaredOurs(const std::uint32_t* in,
                                             std::uint32_t* out)
    {
      difffuzz::StoreDouble(out,
          wm3collharness::DistSegment3Segment3dGetSquared(in, in + 14));
    }

    void DistSegment3Segment3dGetRetail(const std::uint32_t* in,
                                         std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double seg0[7], seg1[7];
      for (int i = 0; i < 7; ++i) {
        seg0[i] = difffuzz::LoadDouble(in + 2*i);
        seg1[i] = difffuzz::LoadDouble(in + 2*(7 + i));
      }
      wm3collretail::DistSegment3Segment3dCtor(storage, seg0, seg1);
      difffuzz::StoreDouble(out,
          wm3collretail::DistSegment3Segment3dGet(storage));
    }

    void DistSegment3Segment3dGetOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      difffuzz::StoreDouble(out,
          wm3collharness::DistSegment3Segment3dGet(in, in + 14));
    }

    void DistSegment3Segment3dGetSweptRetail(const std::uint32_t* in,
                                              std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double seg0[7], seg1[7], vel0[3], vel1[3];
      for (int i = 0; i < 7; ++i) {
        seg0[i] = difffuzz::LoadDouble(in + 2*i);
        seg1[i] = difffuzz::LoadDouble(in + 2*(7 + i));
      }
      const double fT = difffuzz::LoadDouble(in + 28);
      for (int i = 0; i < 3; ++i) {
        vel0[i] = difffuzz::LoadDouble(in + 2*(15 + i));
        vel1[i] = difffuzz::LoadDouble(in + 2*(18 + i));
      }
      wm3collretail::DistSegment3Segment3dCtor(storage, seg0, seg1);
      double value = 0.0;
      wm3collretail::DistSegment3Segment3dGetSwept(storage, fT, vel0, vel1,
                                                    &value);
      difffuzz::StoreDouble(out, value);
    }

    void DistSegment3Segment3dGetSweptOurs(const std::uint32_t* in,
                                             std::uint32_t* out)
    {
      const double fT = difffuzz::LoadDouble(in + 28);
      difffuzz::StoreDouble(out,
          wm3collharness::DistSegment3Segment3dGetSwept(in, in + 14, fT,
              in + 30, in + 36));
    }

    void DistSegment3Segment3dGetSquaredSweptRetail(const std::uint32_t* in,
                                                     std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double seg0[7], seg1[7], vel0[3], vel1[3];
      for (int i = 0; i < 7; ++i) {
        seg0[i] = difffuzz::LoadDouble(in + 2*i);
        seg1[i] = difffuzz::LoadDouble(in + 2*(7 + i));
      }
      const double fT = difffuzz::LoadDouble(in + 28);
      for (int i = 0; i < 3; ++i) {
        vel0[i] = difffuzz::LoadDouble(in + 2*(15 + i));
        vel1[i] = difffuzz::LoadDouble(in + 2*(18 + i));
      }
      wm3collretail::DistSegment3Segment3dCtor(storage, seg0, seg1);
      double value = 0.0;
      wm3collretail::DistSegment3Segment3dGetSquaredSwept(storage, fT, vel0,
                                                          vel1, &value);
      difffuzz::StoreDouble(out, value);
    }

    void DistSegment3Segment3dGetSquaredSweptOurs(const std::uint32_t* in,
                                                  std::uint32_t* out)
    {
      const double fT = difffuzz::LoadDouble(in + 28);
      difffuzz::StoreDouble(out,
          wm3collharness::DistSegment3Segment3dGetSquaredSwept(in, in + 14, fT,
              in + 30, in + 36));
    }

    // ---- Wm3::DistVector2Box2<double> --------------------------------------
    //
    // The double instantiation: the vector is two doubles and the box is
    // eight - the centre, the two axes and the two extents.  Twenty words.
    //
    // The extents are positive and the axes are an orthonormal pair, as the
    // class requires.  In the aligned family the axes are the x and the y
    // axis and the centre is the origin, so the projection of the point onto
    // an axis is the point's own coordinate minus zero - exactly - and a
    // point placed at +-extent lands exactly on the boundary between the
    // clamped and the unclamped branch.
    void GenDistVector2Box2Double(difffuzz::Rng& rng, std::uint32_t* in)
    {
      // a magnitude in [1e-100, 1e100]
      const auto Mag = [&rng]() -> double
      {
        const int e = static_cast<int>(rng.Below(201)) - 100;
        return std::ldexp(rng.Range(1.0, 10.0), e);
      };
      double d[10];
      const bool aligned = rng.Chance(40);
      // the centre
      for (int i = 0; i < 2; ++i) {
        d[2 + i] = (aligned || !rng.Chance(60)) ? rng.Range(-100.0, 100.0) : Mag();
      }
      // the two axes
      if (aligned) {
        d[4] = 1.0; d[5] = 0.0;
        d[6] = 0.0; d[7] = 1.0;
      }
      else {
        const double angle = rng.Range(0.0, 3.1415926535897932);
        const double ca = std::cos(angle), sa = std::sin(angle);
        d[4] = ca;  d[5] = -sa;
        d[6] = sa;  d[7] = ca;
      }
      // the extents, positive
      for (int i = 0; i < 2; ++i) {
        d[8 + i] = rng.Chance(70) ? rng.Range(0.5, 30.0) : Mag();
      }
      // the point, on the box's own scale
      const double reach = d[8] + d[9];
      for (int i = 0; i < 2; ++i) {
        d[i] = d[2 + i] + GenPlain(rng, reach);
      }
      if (aligned && rng.Chance(40)) {
        // exactly on one of the faces: the projection onto that axis is
        // exactly the extent
        const int i = static_cast<int>(rng.Below(2));
        const double s = rng.Chance(50) ? 1.0 : -1.0;
        d[2 + i] = 0.0;
        d[i] = s*d[8 + i];
      }
      for (int i = 0; i < 10; ++i) {
        difffuzz::StoreDouble(in + 2*i, d[i]);
      }
    }

    // The swept forms: the vector, the box, the time and the two velocities -
    // thirty words.  The time is zero in a third of the draws and the
    // velocities are zero with it, so the moved pair is exactly the pair
    // below and the exact-boundary family is carried into the swept body.
    void GenDistVector2Box2DoubleSwept(difffuzz::Rng& rng, std::uint32_t* in)
    {
      for (int attempt = 0; attempt < 32; ++attempt) {
        GenDistVector2Box2Double(rng, in);
        const bool still = rng.Chance(35);
        difffuzz::StoreDouble(in + 20,
            still ? 0.0
                  : (rng.Chance(50) ? rng.Range(0.0, 1.0) : rng.Range(-4.0, 4.0)));
        // the point's velocity points at the box's centre
        double toCentre[2];
        for (int i = 0; i < 2; ++i) {
          toCentre[i] = difffuzz::LoadDouble(in + 2*(2 + i))
                      - difffuzz::LoadDouble(in + 2*i);
        }
        for (int i = 0; i < 2; ++i) {
          double v = 0.0;
          if (!still) {
            v = rng.Chance(60) ? toCentre[i]*rng.Range(0.1, 2.0)
                               : rng.Range(-20.0, 20.0);
          }
          difffuzz::StoreDouble(in + 2*(11 + i), v);
        }
        for (int i = 0; i < 2; ++i) {
          double v = 0.0;
          if (!still && rng.Chance(35)) {
            v = rng.Range(-20.0, 20.0);
          }
          difffuzz::StoreDouble(in + 2*(13 + i), v);
        }
        const double fT = difffuzz::LoadDouble(in + 20);
        const double squared =
            wm3collharness::DistVector2Box2dGetSquaredSwept(in, in + 4, fT,
                in + 22, in + 26);
        if (std::isfinite(squared)) {
          return;
        }
      }
    }

    void DistVector2Box2dGetSquaredRetail(const std::uint32_t* in,
                                           std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double vec[2], box[8];
      for (int i = 0; i < 2; ++i) {
        vec[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 8; ++i) {
        box[i] = difffuzz::LoadDouble(in + 2*(2 + i));
      }
      wm3collretail::DistVector2Box2dCtor(storage, vec, box);
      difffuzz::StoreDouble(out,
          wm3collretail::DistVector2Box2dGetSquared(storage));
    }

    void DistVector2Box2dGetSquaredOurs(const std::uint32_t* in,
                                          std::uint32_t* out)
    {
      difffuzz::StoreDouble(out,
          wm3collharness::DistVector2Box2dGetSquared(in, in + 4));
    }

    void DistVector2Box2dGetRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double vec[2], box[8];
      for (int i = 0; i < 2; ++i) {
        vec[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 8; ++i) {
        box[i] = difffuzz::LoadDouble(in + 2*(2 + i));
      }
      wm3collretail::DistVector2Box2dCtor(storage, vec, box);
      difffuzz::StoreDouble(out, wm3collretail::DistVector2Box2dGet(storage));
    }

    void DistVector2Box2dGetOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      difffuzz::StoreDouble(out,
          wm3collharness::DistVector2Box2dGet(in, in + 4));
    }

    void DistVector2Box2dGetSweptRetail(const std::uint32_t* in,
                                         std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double vec[2], box[8], vel0[2], vel1[2];
      for (int i = 0; i < 2; ++i) {
        vec[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 8; ++i) {
        box[i] = difffuzz::LoadDouble(in + 2*(2 + i));
      }
      const double fT = difffuzz::LoadDouble(in + 20);
      for (int i = 0; i < 2; ++i) {
        vel0[i] = difffuzz::LoadDouble(in + 2*(11 + i));
        vel1[i] = difffuzz::LoadDouble(in + 2*(13 + i));
      }
      wm3collretail::DistVector2Box2dCtor(storage, vec, box);
      double value = 0.0;
      wm3collretail::DistVector2Box2dGetSwept(storage, fT, vel0, vel1, &value);
      difffuzz::StoreDouble(out, value);
    }

    void DistVector2Box2dGetSweptOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const double fT = difffuzz::LoadDouble(in + 20);
      difffuzz::StoreDouble(out,
          wm3collharness::DistVector2Box2dGetSwept(in, in + 4, fT, in + 22, in + 26));
    }

    void DistVector2Box2dGetSquaredSweptRetail(const std::uint32_t* in,
                                                 std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double vec[2], box[8], vel0[2], vel1[2];
      for (int i = 0; i < 2; ++i) {
        vec[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 8; ++i) {
        box[i] = difffuzz::LoadDouble(in + 2*(2 + i));
      }
      const double fT = difffuzz::LoadDouble(in + 20);
      for (int i = 0; i < 2; ++i) {
        vel0[i] = difffuzz::LoadDouble(in + 2*(11 + i));
        vel1[i] = difffuzz::LoadDouble(in + 2*(13 + i));
      }
      wm3collretail::DistVector2Box2dCtor(storage, vec, box);
      double value = 0.0;
      wm3collretail::DistVector2Box2dGetSquaredSwept(storage, fT, vel0, vel1,
                                                     &value);
      difffuzz::StoreDouble(out, value);
    }

    void DistVector2Box2dGetSquaredSweptOurs(const std::uint32_t* in,
                                              std::uint32_t* out)
    {
      const double fT = difffuzz::LoadDouble(in + 20);
      difffuzz::StoreDouble(out,
          wm3collharness::DistVector2Box2dGetSquaredSwept(in, in + 4, fT,
              in + 22, in + 26));
    }

    // ---- Wm3::DistVector3Segment3<double> ----------------------------------
    //
    // The input is the vector (three doubles) and the segment (the origin,
    // the direction and the extent - seven doubles), so twenty words; the
    // swept rows add the time (one double) and the two velocities (three
    // doubles each), so thirty-four.
    //
    // The degenerate branch here is the segment parameter: it gates the three
    // closest-point branches, and the boundaries are t == -extent, t == 0 and
    // t == +extent.  The float generator reaches them by luck; in double
    // they are reached *exactly*, with no denormal trick: the direction is
    // the x axis, the origin is the origin and the vector is on the axis, so
    // the parameter is the vector's x - and the extent is set to that very
    // number, so t lands exactly on the boundary.  Every operation in that
    // family is exact (a multiply by 1.0 and a subtraction of 0.0), so the
    // comparison sees the equality it is meant to see.
    void GenDistVector3Segment3Double(difffuzz::Rng& rng, std::uint32_t* in)
    {
      // the magnitudes stay inside [1e-100,1e100] - the limit inside which
      // the rebuild's double path is exact, because the emulation models the
      // x87's 24-bit significand but not its extended exponent.
      const auto Mag = [&rng]() -> double
      {
        const int e = static_cast<int>(rng.Below(201)) - 100;
        return std::ldexp(rng.Range(1.0, 10.0), e);
      };
      double v[3], origin[3], dir[3], extent;
      const std::uint32_t kind = rng.Below(100);
      if (kind < 40) {
        // the exact-boundary family
        dir[0] = 1.0; dir[1] = 0.0; dir[2] = 0.0;
        origin[0] = origin[1] = origin[2] = 0.0;
        v[1] = v[2] = 0.0;
        const std::uint32_t which = rng.Below(6);
        double t;
        if (which < 2) {
          t = 0.0;                                  // t == 0
          extent = (which == 0) ? 0.0 : 1.0;
        }
        else if (which < 4) {
          t = (which == 2) ? 1.0 : Mag();           // t == +extent
          extent = t;
        }
        else {
          t = -(which == 4 ? 1.0 : Mag());          // t == -extent
          extent = -t;
        }
        v[0] = t;
      }
      else {
        // the ordinary family
        difffuzz::GenUnitVec3d(rng, dir);
        for (int i = 0; i < 3; ++i) {
          origin[i] = rng.Chance(60) ? rng.Range(-100.0, 100.0) : Mag();
        }
        for (int i = 0; i < 3; ++i) {
          v[i] = rng.Chance(60) ? rng.Range(-100.0, 100.0) : Mag();
        }
        extent = rng.Chance(60) ? rng.Range(0.0, 100.0) : Mag();
      }
      double d[10];
      for (int i = 0; i < 3; ++i) d[i] = v[i];
      for (int i = 0; i < 3; ++i) d[3 + i] = origin[i];
      for (int i = 0; i < 3; ++i) d[6 + i] = dir[i];
      d[9] = extent;
      for (int i = 0; i < 10; ++i) {
        difffuzz::StoreDouble(in + 2*i, d[i]);
      }
    }

    void DistVector3Segment3dGetSquaredRetail(const std::uint32_t* in,
                                               std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double vec[3], segment[7];
      for (int i = 0; i < 3; ++i) {
        vec[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 7; ++i) {
        segment[i] = difffuzz::LoadDouble(in + 2*(3 + i));
      }
      wm3collretail::DistVector3Segment3dCtor(storage, vec, segment);
      difffuzz::StoreDouble(out,
          wm3collretail::DistVector3Segment3dGetSquared(storage));
    }

    void DistVector3Segment3dGetSquaredOurs(const std::uint32_t* in,
                                             std::uint32_t* out)
    {
      difffuzz::StoreDouble(out,
          wm3collharness::DistVector3Segment3dGetSquared(in, in + 6));
    }

    void DistVector3Segment3dGetRetail(const std::uint32_t* in,
                                       std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double vec[3], segment[7];
      for (int i = 0; i < 3; ++i) {
        vec[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 7; ++i) {
        segment[i] = difffuzz::LoadDouble(in + 2*(3 + i));
      }
      wm3collretail::DistVector3Segment3dCtor(storage, vec, segment);
      difffuzz::StoreDouble(out,
          wm3collretail::DistVector3Segment3dGet(storage));
    }

    void DistVector3Segment3dGetOurs(const std::uint32_t* in,
                                     std::uint32_t* out)
    {
      difffuzz::StoreDouble(out,
          wm3collharness::DistVector3Segment3dGet(in, in + 6));
    }

    // The swept forms: the vector, the segment, the time and the two
    // velocities - thirty-four words.
    //
    // The time is zero in a third of the draws and the velocities are zero
    // with it, so the moved pair is exactly the pair below - which is what
    // carries the exact-boundary family of the static generator (the point
    // at +-extent on the segment's own line) into the swept body.  In the
    // rest the point moves along the segment's direction in half the draws,
    // so the relative motion is along the segment and the segment parameter
    // lands on the extent boundary; the segment itself moves in a third.
    //
    // The retry is the one the static rows use: the retail's swept Get takes
    // the CRT's square root, and a non-finite argument is a fatal math error
    // in the retail as well as in the harness.
    void GenDistVector3Segment3dSwept(difffuzz::Rng& rng, std::uint32_t* in)
    {
      for (int attempt = 0; attempt < 32; ++attempt) {
        GenDistVector3Segment3Double(rng, in);
        const bool still = rng.Chance(35);
        // the time
        difffuzz::StoreDouble(in + 20,
            still ? 0.0
                  : (rng.Chance(50) ? rng.Range(0.0, 1.0) : rng.Range(-4.0, 4.0)));
        // the two velocities
        double dir[3];
        for (int i = 0; i < 3; ++i) {
          dir[i] = difffuzz::LoadDouble(in + 2*(6 + i));
        }
        const double speed = rng.Range(1.0, 20.0);
        const bool along = rng.Chance(50);
        const bool segMoves = rng.Chance(35);
        for (int i = 0; i < 3; ++i) {
          double v = 0.0;
          if (!still) {
            v = along ? dir[i]*speed : rng.Range(-20.0, 20.0);
          }
          difffuzz::StoreDouble(in + 2*(11 + i), v);
        }
        for (int i = 0; i < 3; ++i) {
          double v = 0.0;
          if (!still && segMoves) {
            v = rng.Range(-20.0, 20.0);
          }
          difffuzz::StoreDouble(in + 2*(14 + i), v);
        }
        const double fT = difffuzz::LoadDouble(in + 20);
        const double squared =
            wm3collharness::DistVector3Segment3dGetSquaredSwept(in, in + 6, fT,
                in + 22, in + 28);
        if (std::isfinite(squared)) {
          return;
        }
      }
    }

    void DistVector3Segment3dGetSweptRetail(const std::uint32_t* in,
                                            std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double vec[3], segment[7], vel0[3], vel1[3];
      for (int i = 0; i < 3; ++i) {
        vec[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 7; ++i) {
        segment[i] = difffuzz::LoadDouble(in + 2*(3 + i));
      }
      const double fT = difffuzz::LoadDouble(in + 20);
      for (int i = 0; i < 3; ++i) {
        vel0[i] = difffuzz::LoadDouble(in + 2*(11 + i));
        vel1[i] = difffuzz::LoadDouble(in + 2*(14 + i));
      }
      wm3collretail::DistVector3Segment3dCtor(storage, vec, segment);
      double value = 0.0;
      wm3collretail::DistVector3Segment3dGetSwept(storage, fT, vel0, vel1,
                                                   &value);
      difffuzz::StoreDouble(out, value);
    }

    void DistVector3Segment3dGetSweptOurs(const std::uint32_t* in,
                                          std::uint32_t* out)
    {
      const double fT = difffuzz::LoadDouble(in + 20);
      difffuzz::StoreDouble(out,
          wm3collharness::DistVector3Segment3dGetSwept(in, in + 6, fT,
              in + 22, in + 28));
    }

    void DistVector3Segment3dGetSquaredSweptRetail(const std::uint32_t* in,
                                                   std::uint32_t* out)
    {
      unsigned char storage[kDoubleObjectBytes];
      std::memset(storage, 0, sizeof storage);
      double vec[3], segment[7], vel0[3], vel1[3];
      for (int i = 0; i < 3; ++i) {
        vec[i] = difffuzz::LoadDouble(in + 2*i);
      }
      for (int i = 0; i < 7; ++i) {
        segment[i] = difffuzz::LoadDouble(in + 2*(3 + i));
      }
      const double fT = difffuzz::LoadDouble(in + 20);
      for (int i = 0; i < 3; ++i) {
        vel0[i] = difffuzz::LoadDouble(in + 2*(11 + i));
        vel1[i] = difffuzz::LoadDouble(in + 2*(14 + i));
      }
      wm3collretail::DistVector3Segment3dCtor(storage, vec, segment);
      double value = 0.0;
      wm3collretail::DistVector3Segment3dGetSquaredSwept(storage, fT, vel0,
                                                          vel1, &value);
      difffuzz::StoreDouble(out, value);
    }

    void DistVector3Segment3dGetSquaredSweptOurs(const std::uint32_t* in,
                                                 std::uint32_t* out)
    {
      const double fT = difffuzz::LoadDouble(in + 20);
      difffuzz::StoreDouble(out,
          wm3collharness::DistVector3Segment3dGetSquaredSwept(in, in + 6, fT,
              in + 22, in + 28));
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

    // The point is (0,1,0) and the segment runs along the y axis from the
    // origin, so the point is the segment's far end: the distance is 0.
    // ---- Wm3::Query3<float>: the rest of the class ------------------------
    //
    // The input is eight vertices (three floats each), the query point (three
    // floats) and the five index words - the point's own index `i` and the
    // simplex's four indices. Thirty-two words.
    //
    //   words  0..23  the vertices
    //   words 24..26  the query point
    //   word  27      `i` - the point's index in the vertex array
    //   words 28..31  iV0, iV1, iV2, iV3
    //
    // The vertices 0..3 are the simplex and the vertices 4..7 all carry the
    // query point, so the index-taking overloads (which use m_akVertex[i] as
    // the point) ask the same question as the point-taking ones.
    //
    // The generator builds configurations whose answer the geometry fixes,
    // and the degenerate ones are the point of the row: a signed-volume
    // predicate has to decide between "on the simplex" and "on one side of
    // it", and the exact zero is where the two sides can part company.
    void GenQuery3(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      for (int i = 0; i < 32; ++i) {
        f[i] = 0.0f;
      }

      const std::uint32_t kind = rng.Below(16);
      float p[3] = { 0.0f, 0.0f, 0.0f };

      // The simplex: vertices 0, 1, 2 and 3.
      switch (kind) {
      case 0: {
        // The unit tetrahedron and a point in the plane of the face
        // (0, 1, 2) - the z = 0 plane. The determinant is exactly zero on
        // both sides, because all three of the face's z coordinates are
        // zero and so every term of it is.
        f[0] = 0.0f; f[1] = 0.0f; f[2] = 0.0f;
        f[3] = 1.0f; f[4] = 0.0f; f[5] = 0.0f;
        f[6] = 0.0f; f[7] = 1.0f; f[8] = 0.0f;
        f[9] = 0.0f; f[10] = 0.0f; f[11] = 1.0f;
        p[0] = static_cast<float>(rng.Range(0.0, 1.0));
        p[1] = static_cast<float>(rng.Range(0.0, 1.0));
        p[2] = 0.0f;
        break;
      }
      case 1: {
        // The unit tetrahedron and a point strictly inside it: all four
        // face signs agree, so the tetrahedron answer is -1.
        f[0] = 0.0f; f[1] = 0.0f; f[2] = 0.0f;
        f[3] = 1.0f; f[4] = 0.0f; f[5] = 0.0f;
        f[6] = 0.0f; f[7] = 1.0f; f[8] = 0.0f;
        f[9] = 0.0f; f[10] = 0.0f; f[11] = 1.0f;
        p[0] = static_cast<float>(rng.Range(0.0, 0.3));
        p[1] = static_cast<float>(rng.Range(0.0, 0.3));
        p[2] = static_cast<float>(rng.Range(0.0, 0.3));
        break;
      }
      case 2: {
        // The unit tetrahedron and a point beyond the face (0, 1, 2): the
        // tetrahedron answer is +1.
        f[0] = 0.0f; f[1] = 0.0f; f[2] = 0.0f;
        f[3] = 1.0f; f[4] = 0.0f; f[5] = 0.0f;
        f[6] = 0.0f; f[7] = 1.0f; f[8] = 0.0f;
        f[9] = 0.0f; f[10] = 0.0f; f[11] = 1.0f;
        p[0] = static_cast<float>(rng.Range(0.0, 1.0));
        p[1] = static_cast<float>(rng.Range(0.0, 1.0));
        p[2] = static_cast<float>(rng.Range(1.5, 4.0));
        break;
      }
      case 3: {
        // The point is one of the vertices: every predicate that uses that
        // vertex has a zero difference vector, so the determinant is
        // exactly zero.
        f[0] = 0.0f; f[1] = 0.0f; f[2] = 0.0f;
        f[3] = 1.0f; f[4] = 0.0f; f[5] = 0.0f;
        f[6] = 0.0f; f[7] = 1.0f; f[8] = 0.0f;
        f[9] = 0.0f; f[10] = 0.0f; f[11] = 1.0f;
        p[0] = 1.0f; p[1] = 0.0f; p[2] = 0.0f;
        break;
      }
      case 4: {
        // The unit tetrahedron and a point one ulp off the plane of the
        // face (0, 1, 2): the determinant is a denormal-sized number and
        // its sign is the whole answer.
        f[0] = 0.0f; f[1] = 0.0f; f[2] = 0.0f;
        f[3] = 1.0f; f[4] = 0.0f; f[5] = 0.0f;
        f[6] = 0.0f; f[7] = 1.0f; f[8] = 0.0f;
        f[9] = 0.0f; f[10] = 0.0f; f[11] = 1.0f;
        p[0] = static_cast<float>(rng.Range(0.0, 1.0));
        p[1] = static_cast<float>(rng.Range(0.0, 1.0));
        p[2] = std::numeric_limits<float>::denorm_min();
        break;
      }
      case 5: {
        // A degenerate simplex: the first two vertices are the same, so
        // the determinant of the face is exactly zero whatever the point
        // is.
        f[0] = 0.25f; f[1] = 0.5f; f[2] = -0.75f;
        f[3] = 0.25f; f[4] = 0.5f; f[5] = -0.75f;
        f[6] = 1.25f; f[7] = -0.5f; f[8] = 0.25f;
        f[9] = 0.0f; f[10] = 0.0f; f[11] = 1.0f;
        p[0] = GenPlain(rng, 10.0f);
        p[1] = GenPlain(rng, 10.0f);
        p[2] = GenPlain(rng, 10.0f);
        break;
      }
      case 6: {
        // Three collinear vertices: the face has no area, so the
        // determinant is exactly zero.
        f[0] = 0.0f; f[1] = 0.0f; f[2] = 0.0f;
        f[3] = 1.0f; f[4] = 0.0f; f[5] = 0.0f;
        f[6] = 2.0f; f[7] = 0.0f; f[8] = 0.0f;
        f[9] = 0.0f; f[10] = 0.0f; f[11] = 1.0f;
        p[0] = GenPlain(rng, 10.0f);
        p[1] = GenPlain(rng, 10.0f);
        p[2] = GenPlain(rng, 10.0f);
        break;
      }
      case 7: {
        // Four coplanar vertices: the circumsphere of a coplanar set is
        // degenerate, and the 4x4 determinant is exactly zero because every
        // one of its third rows is zero.
        f[0] = 0.0f; f[1] = 0.0f; f[2] = 0.0f;
        f[3] = 1.0f; f[4] = 0.0f; f[5] = 0.0f;
        f[6] = 0.0f; f[7] = 1.0f; f[8] = 0.0f;
        f[9] = 1.0f; f[10] = 1.0f; f[11] = 0.0f;
        p[0] = static_cast<float>(rng.Range(-1.0, 2.0));
        p[1] = static_cast<float>(rng.Range(-1.0, 2.0));
        p[2] = 0.0f;
        break;
      }
      case 8: {
        // Four points on the unit circle in the z = 0 plane, and the point
        // at the circle's centre: the point is strictly inside the
        // circumsphere.
        for (int i = 0; i < 4; ++i) {
          const double a = 2.0*3.14159265358979323846*i/4.0;
          f[3*i + 0] = static_cast<float>(std::cos(a));
          f[3*i + 1] = static_cast<float>(std::sin(a));
          f[3*i + 2] = 0.0f;
        }
        p[0] = 0.0f; p[1] = 0.0f; p[2] = 0.0f;
        break;
      }
      case 9: {
        // The eight corners of the unit cube and its centre: the centre is
        // strictly inside the circumsphere of any four of the corners.
        const float akCorner[8][3] = {
          {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
          {1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f},
          {0.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f} };
        for (int i = 0; i < 4; ++i) {
          f[3*i + 0] = akCorner[i][0];
          f[3*i + 1] = akCorner[i][1];
          f[3*i + 2] = akCorner[i][2];
        }
        p[0] = 0.5f; p[1] = 0.5f; p[2] = 0.5f;
        break;
      }
      case 10: {
        // A general tetrahedron and a general point.
        for (int i = 0; i < 12; ++i) {
          f[i] = GenPlain(rng, 10.0f);
        }
        p[0] = GenPlain(rng, 10.0f);
        p[1] = GenPlain(rng, 10.0f);
        p[2] = GenPlain(rng, 10.0f);
        break;
      }
      case 11: {
        // A general tetrahedron with large coordinates and a general point:
        // the intermediates leave the small range, where the float chain
        // and the x87 chain can differ.
        for (int i = 0; i < 12; ++i) {
          f[i] = GenPlain(rng, 100000.0f);
        }
        p[0] = GenPlain(rng, 100000.0f);
        p[1] = GenPlain(rng, 100000.0f);
        p[2] = GenPlain(rng, 100000.0f);
        break;
      }
      case 12: {
        // A general tetrahedron and a point far away from it.
        for (int i = 0; i < 12; ++i) {
          f[i] = GenPlain(rng, 10.0f);
        }
        p[0] = GenPlain(rng, 1.0e6f);
        p[1] = GenPlain(rng, 1.0e6f);
        p[2] = GenPlain(rng, 1.0e6f);
        break;
      }
      case 13: {
        // A general tetrahedron and the point at its first vertex: the
        // circumsphere's determinant has a zero row and is exactly zero.
        for (int i = 0; i < 12; ++i) {
          f[i] = GenPlain(rng, 10.0f);
        }
        p[0] = f[0]; p[1] = f[1]; p[2] = f[2];
        break;
      }
      case 14: {
        // Every vertex the same point: the set has no dimension and every
        // determinant is exactly zero.
        for (int i = 0; i < 12; ++i) {
          f[i] = 0.5f;
        }
        p[0] = 0.5f; p[1] = 0.5f; p[2] = 0.5f;
        break;
      }
      default: {
        // A tilted face and a point on it in exact arithmetic. The face's
        // plane is x*(-2.5) + y*(-1.5) + z*(3.75) = 0, and the point
        // satisfies it, but the determinant is a sum of three products
        // that cancels - so the sign of the tiny residue is decided by the
        // order of the operations.
        f[0] = 0.0f; f[1] = 0.0f; f[2] = 0.0f;
        f[3] = 1.5f; f[4] = 0.0f; f[5] = 1.0f;
        f[6] = 0.0f; f[7] = 2.5f; f[8] = 1.0f;
        f[9] = 0.0f; f[10] = 0.0f; f[11] = 1.0f;
        p[0] = static_cast<float>(rng.Range(-4.0, 4.0));
        p[1] = static_cast<float>(rng.Range(-4.0, 4.0));
        p[2] = static_cast<float>(rng.Range(-4.0, 4.0));
        // Put the point on the plane: z = (2.5*x + 1.5*y)/3.75.
        p[2] = (2.5f*p[0] + 1.5f*p[1])/3.75f;
        break;
      }
      }

      // The vertices 4..7 all carry the query point, so that the
      // index-taking overloads ask the same question.
      for (int i = 4; i < 8; ++i) {
        f[3*i + 0] = p[0];
        f[3*i + 1] = p[1];
        f[3*i + 2] = p[2];
      }
      f[24] = p[0];
      f[25] = p[1];
      f[26] = p[2];

      // The point's own index, and the simplex's indices. The simplex is
      // sometimes permuted and sometimes degenerate - a repeated index
      // makes the determinant exactly zero again.
      in[27] = 4u + rng.Below(4u);
      const std::uint32_t kindH1 = rng.Below(8);
      switch (kindH1) {
      case 0: in[28] = 1u; in[29] = 2u; in[30] = 0u; in[31] = 3u; break;
      case 1: in[28] = 2u; in[29] = 3u; in[30] = 1u; in[31] = 0u; break;
      case 2: in[28] = 0u; in[29] = 0u; in[30] = 1u; in[31] = 2u; break;
      case 3: in[28] = 0u; in[29] = 1u; in[30] = 1u; in[31] = 2u; break;
      default: in[28] = 0u; in[29] = 1u; in[30] = 2u; in[31] = 3u; break;
      }
    }

    // The retail's Query3<float> object is built inline by its
    // ConvexHull3<float> constructor, so it has three fields: the vtable
    // pointer at [this+0x00], the vertex count at [this+0x04] and the
    // vertex array at [this+0x08].
    void Query3RetailObject(const std::uint32_t* in, std::uint32_t* storage)
    {
      storage[0] = 0x00D71F38u;         // the class vtable
      storage[1] = 8u;                  // the vertex count
      storage[2] = reinterpret_cast<std::uint32_t>(in);
      storage[3] = 0u;
    }

    void Query3ToPlaneIndexRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      std::uint32_t storage[4];
      Query3RetailObject(in, storage);
      const int iSign = wm3collretail::Query3ToPlaneIndex(
        storage, static_cast<int>(in[27]), static_cast<int>(in[28]),
        static_cast<int>(in[29]), static_cast<int>(in[30]));
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    void Query3ToPlaneIndexOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* verts = reinterpret_cast<const float*>(in);
      const int iSign = wm3collharness::Query3ToPlaneIndexOurs(
        verts, 8, static_cast<int>(in[27]), static_cast<int>(in[28]),
        static_cast<int>(in[29]), static_cast<int>(in[30]));
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    void Query3ToTetrahedronRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      std::uint32_t storage[4];
      Query3RetailObject(in, storage);
      const float* point = reinterpret_cast<const float*>(in + 24);
      const int iSign = wm3collretail::Query3ToTetrahedron(
        storage, point, static_cast<int>(in[28]), static_cast<int>(in[29]),
        static_cast<int>(in[30]), static_cast<int>(in[31]));
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    void Query3ToTetrahedronOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* verts = reinterpret_cast<const float*>(in);
      const float* point = reinterpret_cast<const float*>(in + 24);
      const int iSign = wm3collharness::Query3ToTetrahedronOurs(
        verts, 8, point, static_cast<int>(in[28]), static_cast<int>(in[29]),
        static_cast<int>(in[30]), static_cast<int>(in[31]));
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    void Query3ToTetrahedronIndexRetail(const std::uint32_t* in,
                                         std::uint32_t* out)
    {
      std::uint32_t storage[4];
      Query3RetailObject(in, storage);
      const int iSign = wm3collretail::Query3ToTetrahedronIndex(
        storage, static_cast<int>(in[27]), static_cast<int>(in[28]),
        static_cast<int>(in[29]), static_cast<int>(in[30]),
        static_cast<int>(in[31]));
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    void Query3ToTetrahedronIndexOurs(const std::uint32_t* in,
                                       std::uint32_t* out)
    {
      const float* verts = reinterpret_cast<const float*>(in);
      const int iSign = wm3collharness::Query3ToTetrahedronIndexOurs(
        verts, 8, static_cast<int>(in[27]), static_cast<int>(in[28]),
        static_cast<int>(in[29]), static_cast<int>(in[30]),
        static_cast<int>(in[31]));
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    void Query3ToCircumsphereRetail(const std::uint32_t* in,
                                     std::uint32_t* out)
    {
      std::uint32_t storage[4];
      Query3RetailObject(in, storage);
      const float* point = reinterpret_cast<const float*>(in + 24);
      const int iSign = wm3collretail::Query3ToCircumsphere(
        storage, point, static_cast<int>(in[28]), static_cast<int>(in[29]),
        static_cast<int>(in[30]), static_cast<int>(in[31]));
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    void Query3ToCircumsphereOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* verts = reinterpret_cast<const float*>(in);
      const float* point = reinterpret_cast<const float*>(in + 24);
      const int iSign = wm3collharness::Query3ToCircumsphereOurs(
        verts, 8, point, static_cast<int>(in[28]), static_cast<int>(in[29]),
        static_cast<int>(in[30]), static_cast<int>(in[31]));
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    void Query3ToCircumsphereIndexRetail(const std::uint32_t* in,
                                          std::uint32_t* out)
    {
      std::uint32_t storage[4];
      Query3RetailObject(in, storage);
      const int iSign = wm3collretail::Query3ToCircumsphereIndex(
        storage, static_cast<int>(in[27]), static_cast<int>(in[28]),
        static_cast<int>(in[29]), static_cast<int>(in[30]),
        static_cast<int>(in[31]));
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    void Query3ToCircumsphereIndexOurs(const std::uint32_t* in,
                                        std::uint32_t* out)
    {
      const float* verts = reinterpret_cast<const float*>(in);
      const int iSign = wm3collharness::Query3ToCircumsphereIndexOurs(
        verts, 8, static_cast<int>(in[27]), static_cast<int>(in[28]),
        static_cast<int>(in[29]), static_cast<int>(in[30]),
        static_cast<int>(in[31]));
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    // ---- Wm3::Query2<float> ------------------------------------------------
    //
    // The input is eight vertices (two floats each), the query point (two
    // floats) and the four index words. Twenty-two words.
    //
    //   words  0..15  the vertices
    //   words 16..17  the query point
    //   word  18      `i` - the point's index in the vertex array
    //   words 19..21  iV0, iV1, iV2
    //
    // The vertices 0..2 are the triangle and the vertices 3..7 all carry the
    // query point.
    void GenQuery2(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      for (int i = 0; i < 22; ++i) {
        f[i] = 0.0f;
      }

      const std::uint32_t kind = rng.Below(16);
      float p[2] = { 0.0f, 0.0f };

      switch (kind) {
      case 0: {
        // The unit right triangle and a point on the edge (0, 1) - the
        // y = 0 line. The determinant is exactly zero, because both of the
        // triangle's y differences are zero.
        f[0] = 0.0f; f[1] = 0.0f;
        f[2] = 1.0f; f[3] = 0.0f;
        f[4] = 0.0f; f[5] = 1.0f;
        p[0] = static_cast<float>(rng.Range(0.0, 1.0));
        p[1] = 0.0f;
        break;
      }
      case 1: {
        // The unit right triangle and a point strictly inside it.
        f[0] = 0.0f; f[1] = 0.0f;
        f[2] = 1.0f; f[3] = 0.0f;
        f[4] = 0.0f; f[5] = 1.0f;
        p[0] = static_cast<float>(rng.Range(0.0, 0.9));
        p[1] = static_cast<float>(rng.Range(0.0, 0.9));
        break;
      }
      case 2: {
        // The unit right triangle and a point outside it.
        f[0] = 0.0f; f[1] = 0.0f;
        f[2] = 1.0f; f[3] = 0.0f;
        f[4] = 0.0f; f[5] = 1.0f;
        p[0] = static_cast<float>(rng.Range(-2.0, 2.0));
        p[1] = static_cast<float>(rng.Range(-2.0, 2.0));
        break;
      }
      case 3: {
        // The point is the first vertex: the difference vector is zero and
        // the determinant is exactly zero.
        f[0] = 0.0f; f[1] = 0.0f;
        f[2] = 1.0f; f[3] = 0.0f;
        f[4] = 0.0f; f[5] = 1.0f;
        p[0] = 0.0f; p[1] = 0.0f;
        break;
      }
      case 4: {
        // The point is the second vertex.
        f[0] = 0.0f; f[1] = 0.0f;
        f[2] = 1.0f; f[3] = 0.0f;
        f[4] = 0.0f; f[5] = 1.0f;
        p[0] = 1.0f; p[1] = 0.0f;
        break;
      }
      case 5: {
        // Three collinear vertices: the triangle has no area and every
        // line determinant is exactly zero.
        f[0] = 0.0f; f[1] = 0.0f;
        f[2] = 1.0f; f[3] = 0.0f;
        f[4] = 2.0f; f[5] = 0.0f;
        p[0] = GenPlain(rng, 10.0f);
        p[1] = GenPlain(rng, 10.0f);
        break;
      }
      case 6: {
        // Two coincident vertices.
        f[0] = 0.25f; f[1] = 0.5f;
        f[2] = 0.25f; f[3] = 0.5f;
        f[4] = 1.25f; f[5] = -0.5f;
        p[0] = GenPlain(rng, 10.0f);
        p[1] = GenPlain(rng, 10.0f);
        break;
      }
      case 7: {
        // Three points on the unit circle and the point on the circle as
        // well: the circumcircle determinant is exactly zero, because each
        // of the three z terms is a difference of equal products.
        f[0] = 1.0f; f[1] = 0.0f;
        f[2] = 0.0f; f[3] = 1.0f;
        f[4] = -1.0f; f[5] = 0.0f;
        p[0] = 0.0f; p[1] = -1.0f;
        break;
      }
      case 8: {
        // The unit right triangle and the point at the circumcentre, which
        // is strictly inside the circumcircle.
        f[0] = 0.0f; f[1] = 0.0f;
        f[2] = 1.0f; f[3] = 0.0f;
        f[4] = 0.0f; f[5] = 1.0f;
        p[0] = 0.5f; p[1] = 0.5f;
        break;
      }
      case 9: {
        // A general triangle and a general point.
        for (int i = 0; i < 6; ++i) {
          f[i] = GenPlain(rng, 10.0f);
        }
        p[0] = GenPlain(rng, 10.0f);
        p[1] = GenPlain(rng, 10.0f);
        break;
      }
      case 10: {
        // A general triangle with large coordinates.
        for (int i = 0; i < 6; ++i) {
          f[i] = GenPlain(rng, 100000.0f);
        }
        p[0] = GenPlain(rng, 100000.0f);
        p[1] = GenPlain(rng, 100000.0f);
        break;
      }
      case 11: {
        // A general triangle and a point far away from it.
        for (int i = 0; i < 6; ++i) {
          f[i] = GenPlain(rng, 10.0f);
        }
        p[0] = GenPlain(rng, 1.0e6f);
        p[1] = GenPlain(rng, 1.0e6f);
        break;
      }
      case 12: {
        // A general triangle and the point at its first vertex: the
        // circumcircle determinant has a zero row.
        for (int i = 0; i < 6; ++i) {
          f[i] = GenPlain(rng, 10.0f);
        }
        p[0] = f[0]; p[1] = f[1];
        break;
      }
      case 13: {
        // Every vertex the same point.
        for (int i = 0; i < 6; ++i) {
          f[i] = 0.5f;
        }
        p[0] = 0.5f; p[1] = -0.25f;
        break;
      }
      case 14: {
        // A tilted line and a point on it in exact arithmetic: the
        // determinant cancels, so the sign of the residue is decided by
        // the order of the operations.
        f[0] = 0.0f; f[1] = 0.0f;
        f[2] = 1.5f; f[3] = 2.5f;
        f[4] = 0.0f; f[5] = 1.0f;
        p[0] = static_cast<float>(rng.Range(-4.0, 4.0));
        p[1] = static_cast<float>(rng.Range(-4.0, 4.0));
        p[1] = (1.5f/2.5f)*p[0];
        break;
      }
      default: {
        // The point one ulp off the line through the first two vertices:
        // the determinant is a denormal-sized number and its sign is the
        // whole answer.
        f[0] = 0.0f; f[1] = 0.0f;
        f[2] = 1.0f; f[3] = 0.0f;
        f[4] = 0.0f; f[5] = 1.0f;
        p[0] = static_cast<float>(rng.Range(0.0, 1.0));
        p[1] = std::numeric_limits<float>::denorm_min();
        break;
      }
      }

      // The vertices 3..7 all carry the query point.
      for (int i = 3; i < 8; ++i) {
        f[2*i + 0] = p[0];
        f[2*i + 1] = p[1];
      }
      f[16] = p[0];
      f[17] = p[1];

      in[18] = 3u + rng.Below(5u);
      switch (rng.Below(6)) {
      case 0: in[19] = 1u; in[20] = 2u; in[21] = 0u; break;
      case 1: in[19] = 2u; in[20] = 0u; in[21] = 1u; break;
      case 2: in[19] = 0u; in[20] = 0u; in[21] = 1u; break;
      default: in[19] = 0u; in[20] = 1u; in[21] = 2u; break;
      }
    }

    void Query2RetailObject(const std::uint32_t* in, std::uint32_t* storage)
    {
      storage[0] = 0x00D72134u;         // the class vtable
      storage[1] = 8u;                  // the vertex count
      storage[2] = reinterpret_cast<std::uint32_t>(in);
      storage[3] = 0u;
    }

    void Query2ToLineRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      std::uint32_t storage[4];
      Query2RetailObject(in, storage);
      const float* point = reinterpret_cast<const float*>(in + 16);
      const int iSign = wm3collretail::Query2ToLine(
        storage, point, static_cast<int>(in[19]), static_cast<int>(in[20]));
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    void Query2ToLineOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* verts = reinterpret_cast<const float*>(in);
      const float* point = reinterpret_cast<const float*>(in + 16);
      const int iSign = wm3collharness::Query2ToLineOurs(
        verts, 8, point, static_cast<int>(in[19]), static_cast<int>(in[20]));
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    void Query2ToLineIndexRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      std::uint32_t storage[4];
      Query2RetailObject(in, storage);
      const int iSign = wm3collretail::Query2ToLineIndex(
        storage, static_cast<int>(in[18]), static_cast<int>(in[19]),
        static_cast<int>(in[20]));
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    void Query2ToLineIndexOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* verts = reinterpret_cast<const float*>(in);
      const int iSign = wm3collharness::Query2ToLineIndexOurs(
        verts, 8, static_cast<int>(in[18]), static_cast<int>(in[19]),
        static_cast<int>(in[20]));
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    void Query2ToTriangleRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      std::uint32_t storage[4];
      Query2RetailObject(in, storage);
      const float* point = reinterpret_cast<const float*>(in + 16);
      const int iSign = wm3collretail::Query2ToTriangle(
        storage, point, static_cast<int>(in[19]), static_cast<int>(in[20]),
        static_cast<int>(in[21]));
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    void Query2ToTriangleOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* verts = reinterpret_cast<const float*>(in);
      const float* point = reinterpret_cast<const float*>(in + 16);
      const int iSign = wm3collharness::Query2ToTriangleOurs(
        verts, 8, point, static_cast<int>(in[19]), static_cast<int>(in[20]),
        static_cast<int>(in[21]));
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    void Query2ToTriangleIndexRetail(const std::uint32_t* in,
                                      std::uint32_t* out)
    {
      std::uint32_t storage[4];
      Query2RetailObject(in, storage);
      const int iSign = wm3collretail::Query2ToTriangleIndex(
        storage, static_cast<int>(in[18]), static_cast<int>(in[19]),
        static_cast<int>(in[20]), static_cast<int>(in[21]));
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    void Query2ToTriangleIndexOurs(const std::uint32_t* in,
                                    std::uint32_t* out)
    {
      const float* verts = reinterpret_cast<const float*>(in);
      const int iSign = wm3collharness::Query2ToTriangleIndexOurs(
        verts, 8, static_cast<int>(in[18]), static_cast<int>(in[19]),
        static_cast<int>(in[20]), static_cast<int>(in[21]));
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    void Query2ToCircumcircleRetail(const std::uint32_t* in,
                                     std::uint32_t* out)
    {
      std::uint32_t storage[4];
      Query2RetailObject(in, storage);
      const float* point = reinterpret_cast<const float*>(in + 16);
      const int iSign = wm3collretail::Query2ToCircumcircle(
        storage, point, static_cast<int>(in[19]), static_cast<int>(in[20]),
        static_cast<int>(in[21]));
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    void Query2ToCircumcircleOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* verts = reinterpret_cast<const float*>(in);
      const float* point = reinterpret_cast<const float*>(in + 16);
      const int iSign = wm3collharness::Query2ToCircumcircleOurs(
        verts, 8, point, static_cast<int>(in[19]), static_cast<int>(in[20]),
        static_cast<int>(in[21]));
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    void Query2ToCircumcircleIndexRetail(const std::uint32_t* in,
                                          std::uint32_t* out)
    {
      std::uint32_t storage[4];
      Query2RetailObject(in, storage);
      const int iSign = wm3collretail::Query2ToCircumcircleIndex(
        storage, static_cast<int>(in[18]), static_cast<int>(in[19]),
        static_cast<int>(in[20]), static_cast<int>(in[21]));
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    void Query2ToCircumcircleIndexOurs(const std::uint32_t* in,
                                        std::uint32_t* out)
    {
      const float* verts = reinterpret_cast<const float*>(in);
      const int iSign = wm3collharness::Query2ToCircumcircleIndexOurs(
        verts, 8, static_cast<int>(in[18]), static_cast<int>(in[19]),
        static_cast<int>(in[20]), static_cast<int>(in[21]));
      out[0] = static_cast<std::uint32_t>(iSign);
    }

    // ---- Wm3::ConvexHull1<float> and Wm3::ConvexHull2<float> --------------
    //
    // The input is a point set with a known hull, not a random cloud:
    //   word 0        the number of points (1..8)
    //   words 1..8    ConvexHull1: the eight scalars' worth
    //   words 1..16   ConvexHull2: the eight points' worth, two floats each
    //   word 9/17     the epsilon (the Mapper's relative tolerance)
    //   word 10/18    the Query::Type value
    //
    // The epsilon has to be positive: with epsilon zero a single point
    // would be taken for a zero-length line and the direction's
    // normalisation would divide by zero.
    void GenConvexHull1(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      for (int i = 0; i < 11; ++i) {
        f[i] = 0.0f;
      }
      in[10] = 3u;                // Wm3::Query::QT_REAL
      in[9] = difffuzz::Bits(1.0e-6f);

      int n = 0;
      switch (rng.Below(8)) {
      case 0: {
        // A single point: the set has dimension zero.
        n = 1;
        f[1] = 0.5f;
        break;
      }
      case 1: {
        // Two points: the set has dimension one.
        n = 2;
        f[1] = 0.0f;
        f[2] = 1.0f;
        break;
      }
      case 2: {
        // Four distinct values.
        n = 4;
        f[1] = 0.0f; f[2] = 1.0f; f[3] = 2.0f; f[4] = 3.0f;
        break;
      }
      case 3: {
        // Duplicated values: the same value twice, so the sort that names
        // the two ends of the segment has a tie to break.
        n = 4;
        f[1] = 0.0f; f[2] = 0.0f; f[3] = 1.0f; f[4] = 1.0f;
        break;
      }
      case 4: {
        // Every value the same: dimension zero.
        n = 3;
        f[1] = 0.5f; f[2] = 0.5f; f[3] = 0.5f;
        break;
      }
      case 5: {
        // A range below the epsilon: the hull's dimension test has to
        // reject it, and the answer is dimension zero.
        n = 4;
        f[1] = 0.0f;
        f[2] = 1.0e-7f;
        f[3] = 2.0e-7f;
        f[4] = 3.0e-7f;
        break;
      }
      case 6: {
        // A general set of distinct values.
        n = 6;
        for (int i = 0; i < 6; ++i) {
          f[1 + i] = static_cast<float>(i) +
                     static_cast<float>(rng.Range(0.0, 1.0));
        }
        break;
      }
      default: {
        // A general set of values, duplicates included.
        n = 8;
        for (int i = 0; i < 8; ++i) {
          f[1 + i] = GenPlain(rng, 100.0f);
        }
        break;
      }
      }
      in[0] = static_cast<std::uint32_t>(n);
    }

    void ConvexHull1Retail(const std::uint32_t* in, std::uint32_t* out)
    {
      const int n = static_cast<int>(in[0]);
      const float* values = reinterpret_cast<const float*>(in + 1);
      const float eps = difffuzz::FromBits(in[9]);
      const std::uint32_t type = in[10];

      std::uint32_t storage[64];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::ConvexHull1Ctor(storage, n, values, eps, 0u, type);

      // The base class ConvexHull<float> holds m_eQueryType at [this+0x04],
      // m_iVertexQuantity at [this+0x08], m_iDimension at [this+0x0C],
      // m_iSimplexQuantity at [this+0x10] and m_aiIndex at [this+0x14] -
      // the same offsets the ConvexHull3<float> row documents.
      const int dimension = static_cast<int>(storage[3]);
      const int quantity = static_cast<int>(storage[4]);
      const int* indices = reinterpret_cast<const int*>(storage[5]);
      wm3collharness::ConvexHull1Canonical(dimension, quantity, indices, out);

      wm3collretail::ConvexHull1Dtor(storage);
    }

    void ConvexHull1Ours(const std::uint32_t* in, std::uint32_t* out)
    {
      const int n = static_cast<int>(in[0]);
      const float* values = reinterpret_cast<const float*>(in + 1);
      const float eps = difffuzz::FromBits(in[9]);
      const std::uint32_t type = in[10];
      wm3collharness::ConvexHull1Ours(values, n, eps, type, out);
    }

    void GenConvexHull2(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      for (int i = 0; i < 19; ++i) {
        f[i] = 0.0f;
      }
      in[18] = 3u;               // Wm3::Query::QT_REAL
      in[17] = difffuzz::Bits(1.0e-6f);

      int n = 0;
      const std::uint32_t kindH2 = rng.Below(11);
      switch (kindH2) {
      case 0: {
        // A single point: dimension zero.
        n = 1;
        f[1] = 0.5f; f[2] = -0.25f;
        break;
      }
      case 1: {
        // Two points: dimension one.
        n = 2;
        f[1] = 0.0f; f[2] = 0.0f;
        f[3] = 1.0f; f[4] = 1.0f;
        break;
      }
      case 2: {
        // Three collinear points: dimension one.
        n = 3;
        f[1] = 0.0f; f[2] = 0.0f;
        f[3] = 1.0f; f[4] = 2.0f;
        f[5] = 2.0f; f[6] = 4.0f;
        break;
      }
      case 3: {
        // A triangle: dimension two, three edges.
        n = 3;
        f[1] = 0.0f; f[2] = 0.0f;
        f[3] = 1.0f; f[4] = 0.0f;
        f[5] = 0.0f; f[6] = 1.0f;
        break;
      }
      case 4: {
        // A square: dimension two, four edges.
        n = 4;
        f[1] = 0.0f; f[2] = 0.0f;
        f[3] = 1.0f; f[4] = 0.0f;
        f[5] = 1.0f; f[6] = 1.0f;
        f[7] = 0.0f; f[8] = 1.0f;
        break;
      }
      case 5: {
        // A regular pentagon on the unit circle: five edges.
        n = 5;
        for (int i = 0; i < 5; ++i) {
          const double a = 2.0*3.14159265358979323846*i/5.0;
          f[1 + 2*i + 0] = static_cast<float>(std::cos(a));
          f[1 + 2*i + 1] = static_cast<float>(std::sin(a));
        }
        break;
      }
      case 6: {
        // A square with one corner repeated: still four edges.
        n = 5;
        f[1] = 0.0f; f[2] = 0.0f;
        f[3] = 1.0f; f[4] = 0.0f;
        f[5] = 1.0f; f[6] = 1.0f;
        f[7] = 0.0f; f[8] = 1.0f;
        f[9] = 0.0f; f[10] = 0.0f;
        break;
      }
      case 7: {
        // Every point the same: dimension zero.
        n = 4;
        for (int i = 0; i < 4; ++i) {
          f[1 + 2*i + 0] = 0.5f;
          f[1 + 2*i + 1] = -0.25f;
        }
        break;
      }
      case 8: {
        // A triangle with a point strictly inside it: the interior point
        // is not a hull vertex, so the hull still has three edges.
        n = 4;
        f[1] = 0.0f; f[2] = 0.0f;
        f[3] = 4.0f; f[4] = 0.0f;
        f[5] = 0.0f; f[6] = 4.0f;
        f[7] = 1.0f; f[8] = 1.0f;
        break;
      }
      case 9: {
        // A square with a point exactly on one of its edges: the point on
        // the edge is not a hull vertex.
        n = 5;
        f[1] = 0.0f; f[2] = 0.0f;
        f[3] = 1.0f; f[4] = 0.0f;
        f[5] = 1.0f; f[6] = 1.0f;
        f[7] = 0.0f; f[8] = 1.0f;
        f[9] = 0.5f; f[10] = 0.0f;
        break;
      }
      default: {
        // A general point set.
        n = 8;
        for (int i = 0; i < 8; ++i) {
          f[1 + 2*i + 0] = GenPlain(rng, 10.0f);
          f[1 + 2*i + 1] = GenPlain(rng, 10.0f);
        }
        break;
      }
      }
      in[0] = static_cast<std::uint32_t>(n);
    }

    void ConvexHull2Retail(const std::uint32_t* in, std::uint32_t* out)
    {
      const int n = static_cast<int>(in[0]);
      const float* points = reinterpret_cast<const float*>(in + 1);
      const float eps = difffuzz::FromBits(in[17]);
      const std::uint32_t type = in[18];

      std::uint32_t storage[64];
      std::memset(storage, 0, sizeof storage);
      wm3collretail::ConvexHull2Ctor(storage, n, points, eps, 0u, type);

      const int dimension = static_cast<int>(storage[3]);
      const int quantity = static_cast<int>(storage[4]);
      const int* indices = reinterpret_cast<const int*>(storage[5]);
      wm3collharness::ConvexHull2Canonical(dimension, quantity, indices, out);

      wm3collretail::ConvexHull2Dtor(storage);
    }

    void ConvexHull2Ours(const std::uint32_t* in, std::uint32_t* out)
    {
      const int n = static_cast<int>(in[0]);
      const float* points = reinterpret_cast<const float*>(in + 1);
      const float eps = difffuzz::FromBits(in[17]);
      const std::uint32_t type = in[18];
      wm3collharness::ConvexHull2Ours(points, n, eps, type, out);
    }

    // ---- Wm3::ConvexHull<double> ------------------------------------------
    //
    // The double instantiation.  The vertex array is built to the retail's
    // stride - a Vector3<double> is twenty-four bytes (three doubles), where
    // a float vertex is twelve - so the harness hands the retail's
    // constructor an array of doubles, three per vertex and contiguous.
    // The shared Vector3 generator (fuzz.h), so the row sees the same
    // magnitudes as the Dot and LengthSq rows - including the family whose
    // squares leave the float's normal range.
    void GenVec3Lanes(difffuzz::Rng& rng, std::uint32_t* in)
    {
      difffuzz::GenVec3(rng, reinterpret_cast<float*>(in));
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
      out[0] = difffuzz::Bits(wm3collretail::Vector3Length(v));
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
    void GenVec3Compare(difffuzz::Rng& rng, std::uint32_t* in)
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
    void GenVec3Equals(difffuzz::Rng& rng, std::uint32_t* in)
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
    void GenQuatEquals(difffuzz::Rng& rng, std::uint32_t* in)
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
    void GenSphere3Ctor(difffuzz::Rng& rng, std::uint32_t* in)
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
      const float radius = difffuzz::FromBits(in[3]);
      wm3collretail::Sphere3Ctor(out, center, radius);
    }

    void Sphere3CtorOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const float* center = reinterpret_cast<const float*>(in);
      const float radius = difffuzz::FromBits(in[3]);
      wm3collharness::Sphere3CtorOurs(out, center, radius);
    }

    // ---- Wm3::ConvexHull<double> (groundwork, not a row yet) --------------
    //
    // The retail's ConvexHull2<double> is at 0x00A79220 and the base's three
    // accessors - the ones the engine calls - are at 0x00A6BD30 (GetDimension),
    // 0x00A6BD40 (GetSimplexQuantity) and 0x00A6BD50 (GetIndices).  The
    // row built from these agrees with the retail bit for bit - it passed
    // 200000/200000 - but the retail's ConvexHull2<double> *leaks* the hull's
    // std::set: its destructor (0x00A74BA0) frees the vertices, the scaled
    // vertices and the query, and not the set, so the retail's heap grows by
    // about three kilobytes per hull and the run dies of a bad_alloc after
    // some 350000 constructions.  A row cannot be run at the closing gate
    // (1,000,000) on a body that leaks, so this is left as groundwork.
    void GenConvexHull2d(difffuzz::Rng& rng, std::uint32_t* in)
    {
      double* d = reinterpret_cast<double*>(in + 1);
      for (int i = 0; i < 16; ++i) {
        d[i] = 0.0;
      }
      difffuzz::StoreDouble(in + 17, 1.0e-3);   // the engine's epsilon
      in[19] = 0u;               // Wm3::Query::QT_INT64 (the engine's type)

      int n = 0;
      const std::uint32_t kind = rng.Below(11);
      switch (kind) {
      case 0: {
        // A single point: dimension zero.
        n = 1;
        d[0] = 0.5; d[1] = -0.25;
        break;
      }
      case 1: {
        // Two points: dimension one.
        n = 2;
        d[0] = 0.0; d[1] = 0.0;
        d[2] = 1.0; d[3] = 1.0;
        break;
      }
      case 2: {
        // Three collinear points: dimension one.
        n = 3;
        d[0] = 0.0; d[1] = 0.0;
        d[2] = 1.0; d[3] = 2.0;
        d[4] = 2.0; d[5] = 4.0;
        break;
      }
      case 3: {
        // A triangle: dimension two, three edges.
        n = 3;
        d[0] = 0.0; d[1] = 0.0;
        d[2] = 1.0; d[3] = 0.0;
        d[4] = 0.0; d[5] = 1.0;
        break;
      }
      case 4: {
        // A square: dimension two, four edges.
        n = 4;
        d[0] = 0.0; d[1] = 0.0;
        d[2] = 1.0; d[3] = 0.0;
        d[4] = 1.0; d[5] = 1.0;
        d[6] = 0.0; d[7] = 1.0;
        break;
      }
      case 5: {
        // A pentagon with a point on one of its edges.
        n = 6;
        d[0] = 0.0; d[1] = 0.0;
        d[2] = 2.0; d[3] = 0.0;
        d[4] = 3.0; d[5] = 1.0;
        d[6] = 2.0; d[7] = 2.0;
        d[8] = 0.0; d[9] = 2.0;
        d[10] = 1.0; d[11] = 0.0;
        break;
      }
      case 6: {
        // A point far from the rest.
        n = 5;
        d[0] = 0.0; d[1] = 0.0;
        d[2] = 1.0; d[3] = 0.0;
        d[4] = 0.0; d[5] = 1.0;
        d[6] = 1.0; d[7] = 1.0;
        d[8] = 1000.0; d[9] = 1000.0;
        break;
      }
      case 7: {
        // A duplicated point.
        n = 4;
        d[0] = 0.0; d[1] = 0.0;
        d[2] = 0.0; d[3] = 0.0;
        d[4] = 1.0; d[5] = 0.0;
        d[6] = 0.0; d[7] = 1.0;
        break;
      }
      case 8: {
        // A general polygon.
        n = 7;
        for (int i = 0; i < 7; ++i) {
          const double a = 2.0*3.14159265358979323846*i/7.0;
          d[2*i + 0] = std::cos(a);
          d[2*i + 1] = std::sin(a);
        }
        break;
      }
      case 9: {
        // A polygon with an interior point.
        n = 6;
        d[0] = 0.0; d[1] = 0.0;
        d[2] = 4.0; d[3] = 0.0;
        d[4] = 4.0; d[5] = 4.0;
        d[6] = 0.0; d[7] = 4.0;
        d[8] = 2.0; d[9] = 2.0;
        d[10] = 2.0; d[11] = 0.5;
        break;
      }
      default: {
        // A polygon with tiny and huge coordinates mixed.
        n = 5;
        d[0] = 0.0; d[1] = 0.0;
        d[2] = 1.0; d[3] = 0.0;
        d[4] = 1.0; d[5] = 1.0;
        d[6] = 0.0; d[7] = 1.0;
        d[8] = 1.0e-6; d[9] = 1.0e-6;
        break;
      }
      }
      in[0] = static_cast<std::uint32_t>(n);
    }

    void ConvexHull2dRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      const int n = static_cast<int>(in[0]);
      const double* points = reinterpret_cast<const double*>(in + 1);
      const double eps = difffuzz::LoadDouble(in + 17);
      const std::uint32_t type = in[19];

      // The retail's ConvexHull3<double> is 0x2C bytes for the base class
      // and the derived data runs to [this+0xB8]; 0x100 bytes of storage
      // is comfortably above that.
      std::uint32_t* storage = new std::uint32_t[1024];
      std::memset(storage, 0, 1024*sizeof(std::uint32_t));
      wm3collretail::ConvexHull2dCtor(storage, n, points, eps, 0u, type);

      const int dimension =
          wm3collretail::ConvexHulldGetDimension(storage);
      const int quantity =
          wm3collretail::ConvexHulldGetSimplexQuantity(storage);
      const int* indices = wm3collretail::ConvexHulldGetIndices(storage);
      wm3collharness::ConvexHull2Canonical(dimension, quantity, indices, out);

      wm3collretail::ConvexHull2dDtor(storage);
    }

    void ConvexHull2dOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      const int n = static_cast<int>(in[0]);
      const double* points = reinterpret_cast<const double*>(in + 1);
      const double eps = difffuzz::LoadDouble(in + 17);
      const std::uint32_t type = in[19];
      wm3collharness::ConvexHull2dOurs(points, n, eps, type, out);
    }

    // ---- Wm3::Vector3<Real>::GenerateOrthonormalBasis ----------------------
    //
    // The generator draws W and the magnitudes from the range the row can
    // carry: the squares, the sum and the square root all have to stay
    // inside the binary64 range, because the rebuild's x87 emulation
    // (Foundation/Math/Wm3Vector3.h) models the retail's 24-bit significand
    // but not its extended exponent - a value that leaves the binary64 range
    // in the middle of the computation would come back as an infinity where
    // the retail keeps a finite number on its stack.
    //
    // The float row keeps the magnitudes in [1e-8, 1e8] instead, because the
    // *float* instantiation computes the sum in a float: the retail keeps it
    // on the x87 (the extended exponent), the rebuild rounds it to binary32,
    // and the two part company once the sum leaves the binary32 normal
    // range.  That is the rebuild's float path, which the engine uses, and
    // it is left exactly as it is - the row only covers the range where the
    // two agree.
    //
    // The shapes are a direction and a magnitude, an axis-aligned vector
    // (which reaches the |W.x| == |W.y| tie and the zero components) and a
    // vector whose components have different magnitudes.
    void GenVector3OrthonormalBasisd(difffuzz::Rng& rng, std::uint32_t* in)
    {
      double d[3];
      const std::uint32_t mode = rng.Below(100);
      if (mode < 55) {                 // a direction and a magnitude
        difffuzz::GenUnitVec3d(rng, d);
        const int e = static_cast<int>(rng.Below(201)) - 100;
        const double m = std::ldexp(rng.Range(1.0, 10.0), e);
        for (int c = 0; c < 3; ++c) {
          d[c] *= m;
        }
      } else if (mode < 80) {          // axis-aligned, with signed zeros
        static const double kLane[] = {0.0, -0.0, 1.0, -1.0, 0.5, -0.5};
        for (int c = 0; c < 3; ++c) {
          d[c] = kLane[rng.Below(6)];
        }
        const int e = static_cast<int>(rng.Below(201)) - 100;
        const double m = std::ldexp(rng.Range(1.0, 10.0), e);
        for (int c = 0; c < 3; ++c) {
          d[c] *= m;
        }
      } else {                         // components of different magnitudes
        for (int c = 0; c < 3; ++c) {
          const int e = static_cast<int>(rng.Below(201)) - 100;
          d[c] = std::ldexp(rng.Range(-10.0, 10.0), e);
        }
      }
      for (int i = 0; i < 3; ++i) {
        difffuzz::StoreDouble(in + 2*i, d[i]);
      }
      in[6] = 1u;                     // bUnitLengthW
    }

    void GenVector3OrthonormalBasisf(difffuzz::Rng& rng, std::uint32_t* in)
    {
      float f[3];
      const std::uint32_t mode = rng.Below(100);
      if (mode < 55) {                 // a direction and a magnitude
        difffuzz::GenUnitVec3(rng, f);
        const int e = static_cast<int>(rng.Below(17)) - 8;
        const float m = static_cast<float>(std::ldexp(rng.Range(1.0, 10.0), e));
        for (int c = 0; c < 3; ++c) {
          f[c] *= m;
        }
      } else if (mode < 80) {          // axis-aligned, with signed zeros
        static const float kLane[] = {0.0f, -0.0f, 1.0f, -1.0f, 0.5f, -0.5f};
        for (int c = 0; c < 3; ++c) {
          f[c] = kLane[rng.Below(6)];
        }
        const int e = static_cast<int>(rng.Below(17)) - 8;
        const float m = static_cast<float>(std::ldexp(rng.Range(1.0, 10.0), e));
        for (int c = 0; c < 3; ++c) {
          f[c] *= m;
        }
      } else {                         // components of different magnitudes
        for (int c = 0; c < 3; ++c) {
          const int e = static_cast<int>(rng.Below(17)) - 8;
          f[c] = static_cast<float>(std::ldexp(rng.Range(-10.0, 10.0), e));
        }
      }
      for (int i = 0; i < 3; ++i) {
        in[i] = difffuzz::Bits(f[i]);
      }
      in[3] = 1u;                     // bUnitLengthW
    }

    void Vector3GenerateOrthonormalBasisdRetail(const std::uint32_t* in,
                                                std::uint32_t* out)
    {
      double w[3];
      for (int i = 0; i < 3; ++i) {
        w[i] = difffuzz::LoadDouble(in + 2*i);
      }
      double outv[6];
      wm3collretail::Vector3GenerateOrthonormalBasisd(w, in[6], outv);
      for (int i = 0; i < 6; ++i) {
        difffuzz::StoreDouble(out + 2*i, outv[i]);
      }
    }

    void Vector3GenerateOrthonormalBasisdOurs(const std::uint32_t* in,
                                              std::uint32_t* out)
    {
      wm3collharness::Vector3GenerateOrthonormalBasisd(in, out);
    }

    // ---- Vector3<double>::Normalize ----------------------------------------
    //
    // The generator draws full-precision binary64 components: the significand
    // is 53 bits, so a product of two of them needs 106 bits and the x87's
    // single rounding (against binary64's two) is exactly what the row
    // measures.  The exponent stays inside [-60, 60] so that the squares, the
    // sum and the length all stay inside the binary64 range: the rebuild
    // carries its intermediates in doubles, while the retail keeps them on the
    // x87 with the extended exponent.
    //
    // The two branches are both reached on purpose: the degenerate one
    // (length <= 1e-8, the tolerance the retail bakes into 0x00d71c10) zeroes
    // the vector and returns 0.0, and the normal one scales it and returns the
    // length.
    void GenVector3Normalized(difffuzz::Rng& rng, std::uint32_t* in)
    {
      double d[3];
      const std::uint32_t mode = rng.Below(100);
      if (mode < 45) {                 // full precision, the normal branch
        const int e = static_cast<int>(rng.Below(64)) - 3;
        for (int c = 0; c < 3; ++c) {
          const std::uint64_t sig = rng.Next() >> 11;    // 53 bits, exact
          d[c] = std::ldexp(static_cast<double>(sig), e);
        }
      } else if (mode < 75) {          // full precision, the degenerate branch
        const int e = static_cast<int>(rng.Below(34)) - 60;
        for (int c = 0; c < 3; ++c) {
          const std::uint64_t sig = rng.Next() >> 11;
          d[c] = std::ldexp(static_cast<double>(sig), e);
        }
      } else if (mode < 90) {          // exact lanes, with signed zeros
        static const double kLane[] = {0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 2.0,
                                       -2.0};
        const int e = static_cast<int>(rng.Below(121)) - 60;
        const double m = std::ldexp(rng.Range(1.0, 2.0), e);
        for (int c = 0; c < 3; ++c) {
          d[c] = kLane[rng.Below(8)] * m;
        }
      } else {                         // a direction and a magnitude
        difffuzz::GenUnitVec3d(rng, d);
        const int e = static_cast<int>(rng.Below(121)) - 60;
        const double m = std::ldexp(rng.Range(1.0, 2.0), e);
        for (int c = 0; c < 3; ++c) {
          d[c] *= m;
        }
      }
      for (int i = 0; i < 3; ++i) {
        difffuzz::StoreDouble(in + 2*i, d[i]);
      }
    }

    void Vector3NormalizeDRetail(const std::uint32_t* in,
                                  std::uint32_t* out)
    {
      double v[3];
      for (int i = 0; i < 3; ++i) {
        v[i] = difffuzz::LoadDouble(in + 2*i);
      }
      const double dLength = wm3collretail::Vector3NormalizeD(v);
      for (int i = 0; i < 3; ++i) {
        difffuzz::StoreDouble(out + 2*i, v[i]);
      }
      difffuzz::StoreDouble(out + 6, dLength);
    }

    void Vector3NormalizeDOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      wm3collharness::Vector3NormalizeD(in, out);
    }

    void Vector3GenerateOrthonormalBasisfRetail(const std::uint32_t* in,
                                                std::uint32_t* out)
    {
      float w[3];
      for (int i = 0; i < 3; ++i) {
        w[i] = difffuzz::FromBits(in[i]);
      }
      float outv[6];
      wm3collretail::Vector3GenerateOrthonormalBasisf(w, in[3], outv);
      for (int i = 0; i < 6; ++i) {
        out[i] = difffuzz::Bits(outv[i]);
      }
    }

    void Vector3GenerateOrthonormalBasisfOurs(const std::uint32_t* in,
                                              std::uint32_t* out)
    {
      wm3collharness::Vector3GenerateOrthonormalBasisf(in, out);
    }

  } // namespace

  const difffuzz::Target kTargets[] = {
    { "Wm3::DistVector3Box3<float>::GetSquared()", 0x00A45C00,
      "Foundation/Distance/Wm3DistVector3Box3.cpp:45 (retail thiscall, result in ST(0))",
      18, 1, "vec[3] box[15]", "ret", GenDistVector3Box3,
      DistVector3Box3GetSquaredRetail, DistVector3Box3GetSquaredOurs },
    { "Wm3::DistVector3Box3<float>::Get()", 0x00A457E0,
      "Foundation/Distance/Wm3DistVector3Box3.cpp:40 (retail thiscall, result in ST(0))",
      18, 1, "vec[3] box[15]", "ret", GenDistVector3Box3Get,
      DistVector3Box3GetRetail, DistVector3Box3GetOurs },
    { "Wm3::DistVector3Segment3<float>::GetSquared()", 0x00A484F0,
      "Foundation/Distance/Wm3DistVector3Segment3.cpp:46 (retail thiscall, result in ST(0))",
      10, 1, "vec[3] segment[7]", "ret", GenDistVector3Segment3,
      DistVector3Segment3GetSquaredRetail, DistVector3Segment3GetSquaredOurs },
    { "Wm3::DistVector3Segment3<float>::Get()", 0x00A480E0,
      "Foundation/Distance/Wm3DistVector3Segment3.cpp:39 (retail thiscall, result in ST(0))",
      10, 1, "vec[3] segment[7]", "ret", GenDistVector3Segment3Get,
      DistVector3Segment3GetRetail, DistVector3Segment3GetOurs },
    { "Wm3::IntrBox2Circle2<float>::Find(float, const Wm3::Vector2<float>&, const Wm3::Vector2<float>&)", 0x00A4F580,
      "Foundation/Intersection/Wm3IntrBox2Circle2.cpp:49 (retail thiscall, RET 0xC, bool in AL)"
      " -- the retail's CRT runs now, so this row answers",
      16, 1, "box[8] circle[3] tmax vel0[2] vel1[2]", "ret", GenIntrBox2Circle2Find,
      IntrBox2Circle2FindRetail, IntrBox2Circle2FindOurs },
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
    { "Wm3::Distance<float, Wm3::Vector3<float>>::GetClosestPoint1() const", 0x00A39230,
      "Foundation/Distance/Wm3Distance.h:72 (retail thiscall, returns &m_kClosestPoint1 in EAX)",
      10, 3, "vec[3] segment[7]", "point[3]", GenDistVector3Segment3,
      DistVector3Segment3GetClosestPoint1Retail, DistVector3Segment3GetClosestPoint1Ours },
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
    { "Wm3::IntrSegment3Sphere3<float>::Test()", 0x00A46C80,
      "Foundation/Intersection/Wm3IntrSegment3Sphere3.cpp:42 (retail thiscall, bool in AL;"
      " the class vtable's +0x08 slot)",
      11, 1, "segment[7] sphere[4]", "ret", GenIntrSegment3Sphere3,
      IntrSegment3Sphere3TestRetail, IntrSegment3Sphere3TestOurs },
    { "Wm3::IntrSegment3Sphere3<float>::Test(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)", 0x00A46B10,
      "Foundation/Intersection/Wm3IntrSegment3Sphere3.cpp:149 (retail thiscall, RET 0xC, bool in AL;"
      " the class vtable's +0x04 slot, Test(fTMax, velocity0, velocity1))",
      18, 1, "segment[7] sphere[4] tmax vel0[3] vel1[3]", "ret", GenIntrSegment3Sphere3Dynamic,
      IntrSegment3Sphere3TestDynamicRetail, IntrSegment3Sphere3TestDynamicOurs },
    { "Wm3::IntrSegment3Sphere3<float>::Find(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)", 0x00A46DB0,
      "Foundation/Intersection/Wm3IntrSegment3Sphere3.cpp:176 (retail thiscall, RET 0xC, bool in AL;"
      " the class vtable's +0x0C slot, Find(fTMax, velocity0, velocity1))",
      18, 1, "segment[7] sphere[4] tmax vel0[3] vel1[3]", "ret", GenIntrSegment3Sphere3Dynamic,
      IntrSegment3Sphere3FindDynamicRetail, IntrSegment3Sphere3FindDynamicOurs },
    { "Wm3::IntrBox2Box2<float>::Test()", 0x00A47D40,
      "Foundation/Intersection/Wm3IntrBox2Box2.h (retail thiscall, bool in AL;"
      " direct CALL from the AI steering test at 0x00596DC3)",
      16, 1, "box0[8] box1[8]", "ret", GenIntrBox2Box2,
      IntrBox2Box2TestRetail, IntrBox2Box2TestOurs },
    { "Wm3::DistSegment3Segment3<float>::GetSquared()", 0x00A7E660,
      "Foundation/Distance/Wm3DistSegment3Segment3.cpp:124 (retail thiscall, no stack"
      " arguments, result in ST(0); the class vtable's +0x18 slot)",
      14, 1, "seg0[7] seg1[7]", "ret", GenDistSegment3Segment3,
      DistSegment3Segment3GetSquaredRetail, DistSegment3Segment3GetSquaredOurs },
    { "Wm3::DistSegment3Segment3<float>::Get()", 0x00A7E230,
      "Foundation/Distance/Wm3DistSegment3Segment3.cpp:68 (retail thiscall, no stack"
      " arguments, result in ST(0); dispatches GetSquared through the vtable's"
      " +0x18 slot and takes the CRT's square root at 0x00A91000)",
      14, 1, "seg0[7] seg1[7]", "ret", GenDistSegment3Segment3Get,
      DistSegment3Segment3GetRetail, DistSegment3Segment3GetOurs },
    { "Wm3::DistVector2Box2<float>::GetSquared()", 0x00A6C9D0,
      "Foundation/Distance/Wm3DistVector2Box2.cpp:45 (retail thiscall, no stack"
      " arguments, result in ST(0); the class vtable's +0x18 slot)",
      10, 1, "vec[2] box[8]", "ret", GenDistVector2Box2,
      DistVector2Box2GetSquaredRetail, DistVector2Box2GetSquaredOurs },
    { "Wm3::IntrSegment3Capsule3<float>::Test()", 0x00A6BEB0,
      "Foundation/Intersection/Wm3IntrSegment3Capsule3.cpp:41 (retail thiscall, no"
      " stack arguments, bool in EAX; the class vtable's +0x08 slot)",
      15, 1, "segment[7] capsule[8]", "ret", GenIntrSegment3Capsule3,
      IntrSegment3Capsule3TestRetail, IntrSegment3Capsule3TestOurs },
    { "Wm3::IntrSegment3Capsule3<float>::Find()", 0x00A6C070,
      "Foundation/Intersection/Wm3IntrSegment3Capsule3.cpp:56 (retail thiscall, no"
      " stack arguments, bool in AL; the class vtable's +0x10 slot, the static"
      " Find - the swept Find at +0x0C is the base's stub)"
      " -- the intersection set is compared only after a successful Find",
      15, 6, "segment[7] capsule[8]", "ret quantity parameter0 point0[3]",
      GenIntrSegment3Capsule3,
      IntrSegment3Capsule3FindRetail, IntrSegment3Capsule3FindOurs },
    { "Wm3::IntrLine3Capsule3<float>::Find(const Wm3::Vector3<float>&, const Wm3::Vector3<float>&, const Wm3::Capsule3<float>&, float*)", 0x00A7F920,
      "Foundation/Intersection/Wm3IntrLine3Capsule3.cpp:86 (retail static, four"
      " pointer arguments - origin, direction, capsule, afT - and the number of"
      " roots in EAX)",
      14, 3, "line[6] capsule[8]", "ret afT[2]", GenIntrLine3Capsule3,
      IntrLine3Capsule3FindRetail, IntrLine3Capsule3FindOurs },
    { "Wm3::IntrLine3Box3<float>::Find()", 0x00A508F0,
      "Foundation/Intersection/Wm3IntrLine3Box3.cpp:153 (retail thiscall, no"
      " stack arguments, bool in AL; the class vtable's +0x10 slot, the static"
      " Find - +0x0C is the base's swept stub)"
      " -- the intersection set is compared only when the quantity covers it",
      21, 9, "line[6] box[15]", "ret type quantity point0[3] point1[3]",
      GenIntrLine3Box3Find,
      IntrLine3Box3FindRetail, IntrLine3Box3FindOurs },
    { "Wm3::DistVector2Box2<float>::Get()", 0x00A6C680,
      "Foundation/Distance/Wm3DistVector2Box2.cpp:39 (retail thiscall, no stack"
      " arguments, the float in ST(0); the class vtable's +0x0C slot, which"
      " IntrBox2Circle2::Test at 0x00A4E620 dispatches through)",
      10, 1, "vec[2] box[8]", "ret", GenDistVector2Box2,
      DistVector2Box2GetRetail, DistVector2Box2GetOurs },
    { "Wm3::IntrSegment3Capsule3<float>::GetPoint(int) const", 0x00A6BDF0,
      "Foundation/Intersection/Wm3IntrSegment3Capsule3.cpp:94 (retail thiscall,"
      " the index as a dword stack argument, RET 4, &m_akPoint[index] in EAX)"
      " -- the retail's own accessor body, which has no code xref in the"
      " retail image because the compiler inlined it; see the README",
      15, 3, "segment[7] capsule[8]", "point0[3]", GenIntrSegment3Capsule3,
      IntrSegment3Capsule3GetPointRetail, IntrSegment3Capsule3GetPointOurs },
    { "Wm3::IntrSegment3Capsule3<float>::GetParameter(int) const", 0x00A6BE00,
      "Foundation/Intersection/Wm3IntrSegment3Capsule3.cpp:101 (retail thiscall,"
      " the index as a dword stack argument, RET 4, the float in ST(0))"
      " -- the retail's own accessor body, which has no code xref in the"
      " retail image because the compiler inlined it; see the README",
      15, 1, "segment[7] capsule[8]", "ret", GenIntrSegment3Capsule3,
      IntrSegment3Capsule3GetParameterRetail, IntrSegment3Capsule3GetParameterOurs },
    { "Wm3::Distance<float, Wm3::Vector3<float>>::GetDerivative(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)", 0x00A39110,
      "Foundation/Distance/Wm3Distance.h:87 (the base class's generic body,"
      " at the class vtable's +0x1C slot; retail thiscall, three stack"
      " arguments - fT, rkVelocity0, rkVelocity1 - RET 0xC, the float in ST(0))",
      17, 1, "vec[3] segment[7] fT vel0[3] vel1[3]", "ret",
      GenDistVector3Segment3Swept,
      DistVector3Segment3GetDerivativeRetail, DistVector3Segment3GetDerivativeOurs },
    { "Wm3::Distance<float, Wm3::Vector3<float>>::GetDerivativeSquared(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)", 0x00A39180,
      "Foundation/Distance/Wm3Distance.h:98 (the base class's generic body,"
      " at the class vtable's +0x20 slot; retail thiscall, three stack"
      " arguments - fT, rkVelocity0, rkVelocity1 - RET 0xC, the float in ST(0))",
      17, 1, "vec[3] segment[7] fT vel0[3] vel1[3]", "ret",
      GenDistVector3Segment3Swept,
      DistVector3Segment3GetDerivativeSquaredRetail, DistVector3Segment3GetDerivativeSquaredOurs },
    { "Wm3::IntrSegment3Box3<float>::Test()", 0x00A46440,
      "Foundation/Intersection/Wm3IntrSegment3Box3.h (retail thiscall, bool in"
      " AL; the class vtable's +0x08 slot, the same slot the already-wired"
      " IntrLine3Box3::Test occupies)",
      26, 1, "segment[7] box[15] solid", "ret", GenIntrSegment3Box3,
      IntrSegment3Box3TestRetail, IntrSegment3Box3TestOurs },
    { "Wm3::InBox<float>(const Wm3::Vector3<float>&, const Wm3::Box3<float>&)", 0x00A3BC10,
      "Foundation/Containment/Wm3ContBox3.cpp:355 (retail free function, cdecl:"
      " the point and the box are two stack pointers, the caller removes them,"
      " the bool in AL)",
      18, 1, "point[3] box[15]", "ret", GenInBox,
      InBoxRetail, InBoxOurs },
    { "Wm3::DistVector3Segment3<float>::Get(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)", 0x00A48190,
      "Foundation/Distance/Wm3DistVector3Segment3.cpp:100 (retail thiscall, three"
      " stack arguments - fT, rkVelocity0, rkVelocity1 - RET 0xC, the float in"
      " ST(0); the class vtable's +0x08 slot)",
      17, 1, "vec[3] segment[7] fT vel0[3] vel1[3]", "ret",
      GenDistVector3Segment3Swept,
      DistVector3Segment3GetSweptRetail, DistVector3Segment3GetSweptOurs },
    { "Wm3::DistVector3Segment3<float>::GetSquared(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)", 0x00A48340,
      "Foundation/Distance/Wm3DistVector3Segment3.cpp:111 (retail thiscall, three"
      " stack arguments - fT, rkVelocity0, rkVelocity1 - RET 0xC, the float in"
      " ST(0); the class vtable's +0x14 slot)",
      17, 1, "vec[3] segment[7] fT vel0[3] vel1[3]", "ret",
      GenDistVector3Segment3Swept,
      DistVector3Segment3GetSquaredSweptRetail, DistVector3Segment3GetSquaredSweptOurs },
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
    { "Wm3::ContAlignedBox<float>(int, const Wm3::Vector2<float>*)", 0x00A685F0,
      "Foundation/Containment/Wm3ContBox2.cpp:24 (retail free function returning"
      " a Box2 by value, so the hidden return pointer is the first stack"
      " argument; cdecl, the callee leaves its four arguments; the box is"
      " written as the centre, the two axes and the two extents. Found by its"
      " shape - the body writes the 2D identity and the two unit extents"
      " before it computes the extremes - the engine never calls it, so it"
      " has no code xref in the retail image)",
      17, 8, "count[1] points[16]", "center[2] axis[4] extent[2]",
      GenContAlignedBox2, ContAlignedBox2Retail, ContAlignedBox2Ours },
    { "Wm3::ContAlignedBox<float>(int, const Wm3::Vector2<float>*, const bool*)", 0x00A685F0,
      "Foundation/Containment/Wm3ContBox2.cpp:24 (the same retail entry point,"
      " but the fourth stack argument - the abValid mask, which the retail"
      " tests a byte at a time - is passed through instead of being a"
      " literal zero)",
      20, 8, "count[1] mode[1] points[16] mask[8]",
      "center[2] axis[4] extent[2]",
      GenContAlignedBox2Mask, ContAlignedBox2MaskRetail,
      ContAlignedBox2MaskOurs },
    { "Wm3::IntrBox2Circle2<float>::TestVertexRegion(float, float, float, float, float, float, float&, float&)", 0x00A4E6C0,
      "Foundation/Intersection/Wm3IntrBox2Circle2.cpp:152 (retail thiscall, a"
      " private member: ECX is `this`, eight stack arguments - fCx, fCy, fVx,"
      " fVy, fEx, fEy, &rfIx, &rfIy - RET 0x20, the callee removes them; the"
      " retail calls it out of line from Find at 0x00A4F67D, 0x00A4F815 and"
      " 0x00A4F8AF, so the helper has a body of its own)",
      17, 4, "box[8] circle[3] f[6]", "ret point[2] time",
      GenIntrBox2Circle2RegionVertex,
      IntrBox2Circle2TestVertexRegionRetail,
      IntrBox2Circle2TestVertexRegionOurs },
    { "Wm3::IntrBox2Circle2<float>::TestEdgeRegion(float, float, float, float, float, float, float&, float&)", 0x00A4EB60,
      "Foundation/Intersection/Wm3IntrBox2Circle2.cpp:283 (retail thiscall, a"
      " private member: the same eight stack arguments and the same RET 0x20;"
      " the retail calls it out of line from Find at 0x00A4F75E, 0x00A4F7D2"
      " and 0x00A4F866)",
      17, 4, "box[8] circle[3] f[6]", "ret point[2] time",
      GenIntrBox2Circle2RegionEdge,
      IntrBox2Circle2TestEdgeRegionRetail,
      IntrBox2Circle2TestEdgeRegionOurs },
    { "Wm3::DistVector3Box3<float>::Get(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)", 0x00A458C0,
      "Foundation/Distance/Wm3DistVector3Box3.cpp:96 (retail thiscall, three"
      " stack arguments - fT, rkVelocity0, rkVelocity1 - RET 0xC, the float in"
      " ST(0); the class vtable 0x00D71D8C's +0x08 slot, the derived class's own"
      " Get and not the base's)",
      25, 1, "vec[3] box[15] fT vel0[3] vel1[3]", "ret",
      GenDistVector3Box3SweptGet,
      DistVector3Box3GetSweptRetail, DistVector3Box3GetSweptOurs },
    { "Wm3::DistVector3Box3<float>::GetSquared(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)", 0x00A45A60,
      "Foundation/Distance/Wm3DistVector3Box3.cpp:107 (retail thiscall, three"
      " stack arguments - fT, rkVelocity0, rkVelocity1 - RET 0xC, the float in"
      " ST(0); the class vtable 0x00D71D8C's +0x14 slot, the derived class's own"
      " GetSquared. The base's +0x10 slot is not a generic three-argument"
      " GetSquared - the retail's base declares that one pure virtual - but the"
      " base's own window GetSquared(fTMin,fTMax,v0,v1))",
      25, 1, "vec[3] box[15] fT vel0[3] vel1[3]", "ret",
      GenDistVector3Box3Swept,
      DistVector3Box3GetSquaredSweptRetail, DistVector3Box3GetSquaredSweptOurs },
    { "Wm3::DistVector2Box2<float>::Get(float, const Wm3::Vector2<float>&, const Wm3::Vector2<float>&)", 0x00A6C730,
      "Foundation/Distance/Wm3DistVector2Box2.cpp:70 (retail thiscall, three"
      " stack arguments - fT, rkVelocity0, rkVelocity1 - RET 0xC, the float in"
      " ST(0); the class vtable 0x00D720C4's +0x08 slot)",
      15, 1, "vec[2] box[8] fT vel0[2] vel1[2]", "ret",
      GenDistVector2Box2SweptGet,
      DistVector2Box2GetSweptRetail, DistVector2Box2GetSweptOurs },
    { "Wm3::DistVector2Box2<float>::GetSquared(float, const Wm3::Vector2<float>&, const Wm3::Vector2<float>&)", 0x00A6C880,
      "Foundation/Distance/Wm3DistVector2Box2.cpp:81 (retail thiscall, three"
      " stack arguments - fT, rkVelocity0, rkVelocity1 - RET 0xC, the float in"
      " ST(0); the class vtable 0x00D720C4's +0x14 slot)",
      15, 1, "vec[2] box[8] fT vel0[2] vel1[2]", "ret",
      GenDistVector2Box2Swept,
      DistVector2Box2GetSquaredSweptRetail, DistVector2Box2GetSquaredSweptOurs },
    { "Wm3::DistSegment3Segment3<float>::Get(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)", 0x00A7E2A0,
      "Foundation/Distance/Wm3DistSegment3Segment3.cpp:334 (retail thiscall,"
      " three stack arguments - fT, rkVelocity0, rkVelocity1 - RET 0xC, the"
      " float in ST(0); the class vtable 0x00D72254's +0x08 slot)",
      21, 1, "seg0[7] seg1[7] fT vel0[3] vel1[3]", "ret",
      GenDistSegment3Segment3SweptGet,
      DistSegment3Segment3GetSweptRetail, DistSegment3Segment3GetSweptOurs },
    { "Wm3::DistSegment3Segment3<float>::GetSquared(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)", 0x00A7E480,
      "Foundation/Distance/Wm3DistSegment3Segment3.cpp:345 (retail thiscall,"
      " three stack arguments - fT, rkVelocity0, rkVelocity1 - RET 0xC, the"
      " float in ST(0); the class vtable 0x00D72254's +0x14 slot)",
      21, 1, "seg0[7] seg1[7] fT vel0[3] vel1[3]", "ret",
      GenDistSegment3Segment3Swept,
      DistSegment3Segment3GetSquaredSweptRetail,
      DistSegment3Segment3GetSquaredSweptOurs },
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
    { "Wm3::Distance<float, Wm3::Vector3<float>>::Get(float, float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)", 0x00A38C50,
      "Foundation/Distance/Wm3Distance.cpp:150 (retail thiscall, four stack"
      " arguments - fTMin, fTMax, &rkVelocity0, &rkVelocity1 - RET 0x10, the"
      " float in ST(0). The base class's own vftable is 0x00D71C54 - the base"
      " constructor at 0x00A38C40 installs it - and this body is its +0x04"
      " slot: the base's window query, not a destructor. The row runs on a"
      " DistVector3Segment3f so the body's vtable dispatch reaches the"
      " derived class's own swept Get)",
      18, 1, "vec[3] segment[7] tmin tmax vel0[3] vel1[3]", "ret",
      GenDistVector3Segment3Window,
      DistVector3Segment3WindowGetRetail, DistVector3Segment3WindowGetOurs },
    { "Wm3::Distance<float, Wm3::Vector3<float>>::GetSquared(float, float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)", 0x00A38EB0,
      "Foundation/Distance/Wm3Distance.cpp:245 (retail thiscall, four stack"
      " arguments - fTMin, fTMax, &rkVelocity0, &rkVelocity1 - RET 0x10, the"
      " ST(0). The base class's own vftable is 0x00D71C54 - the base"
      " constructor at 0x00A38C40 installs it - and this body is its +0x10"
      " slot: the base's window query for the squared distance. The"
      " retail's base has no generic three-argument GetSquared at all: the"
      " slot is pure virtual, so +0x10 is this window body and not one)",
      18, 1, "vec[3] segment[7] tmin tmax vel0[3] vel1[3]", "ret",
      GenDistVector3Segment3Window,
      DistVector3Segment3WindowGetSquaredRetail,
      DistVector3Segment3WindowGetSquaredOurs },
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
    { "Wm3::DistLine3Segment3<float>::GetSquared()", 0x00A80DE0,
      "Foundation/Distance/Wm3DistLine3Segment3.cpp:51 (retail thiscall, no"
      " stack arguments, the result in ST(0); the class vtable 0x00D722D4's"
      " +0x18 slot. The Complete Object Locator at 0x00E5BEE0 carries the"
      " type descriptor '.?AV?$DistLine3Segment3@M@Wm3@@', and the"
      " constructor at 0x00A80900 writes the vtable pointer)",
      13, 1, "line[6] segment[7]", "ret", GenDistLine3Segment3,
      DistLine3Segment3GetSquaredRetail, DistLine3Segment3GetSquaredOurs },
    { "Wm3::DistLine3Segment3<float>::Get()", 0x00A809A0,
      "Foundation/Distance/Wm3DistLine3Segment3.cpp:39 (retail thiscall, no"
      " stack arguments, the float in ST(0); the class vtable 0x00D722D4's"
      " +0x0C slot - it dispatches the +0x18 slot and jumps to the CRT's"
      " square root at 0x00A91000)",
      13, 1, "line[6] segment[7]", "ret", GenDistLine3Segment3,
      DistLine3Segment3GetRetail, DistLine3Segment3GetOurs },
    { "Wm3::DistLine3Segment3<float>::GetSquared(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)", 0x00A80C10,
      "Foundation/Distance/Wm3DistLine3Segment3.cpp:126 (retail thiscall,"
      " three stack arguments - fT, rkVelocity0, rkVelocity1 - RET 0xC, the"
      " float in ST(0); the class vtable 0x00D722D4's +0x14 slot)",
      20, 1, "line[6] segment[7] fT vel0[3] vel1[3]", "ret",
      GenDistLine3Segment3Swept,
      DistLine3Segment3GetSquaredSweptRetail,
      DistLine3Segment3GetSquaredSweptOurs },
    { "Wm3::DistLine3Segment3<float>::Get(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)", 0x00A80A40,
      "Foundation/Distance/Wm3DistLine3Segment3.cpp:115 (retail thiscall,"
      " three stack arguments - fT, rkVelocity0, rkVelocity1 - RET 0xC, the"
      " float in ST(0); the class vtable 0x00D722D4's +0x08 slot)",
      20, 1, "line[6] segment[7] fT vel0[3] vel1[3]", "ret",
      GenDistLine3Segment3Swept,
      DistLine3Segment3GetSweptRetail, DistLine3Segment3GetSweptOurs },
    { "Wm3::IntrLine3Capsule3<float>::Test()", 0x00A7F760,
      "Foundation/Intersection/Wm3IntrLine3Capsule3.h (retail thiscall, no"
      " stack arguments, the bool in EAX; located via the class vtable"
      " 0x00D722A4's +0x08 slot - the constructor at 0x00A7F730 installs"
      " that vtable, and the body builds a DistLine3Segment3<float> from the"
      " line and the capsule's spine, dispatches its +0x0C slot (the"
      " distance, 0x00A809A0) and compares the result with the capsule's"
      " radius)",
      14, 1, "line[6] capsule[8]", "ret", GenIntrLine3Capsule3,
      IntrLine3Capsule3TestRetail, IntrLine3Capsule3TestOurs },
    { "Wm3::ConvexHull3<float>::ConvexHull3(int, Wm3::Vector3<float>*, float, bool, int)", 0x00A67120,
      "Foundation/Containment/Wm3ConvexHull3.cpp:22 (retail thiscall, five"
      " stack arguments - iVertexQuantity, akVertex, fEpsilon, bOwner,"
      " eQueryType - RET 0x14; the class has one virtual slot, the"
      " destructor, so the hull construction is reached by address. The"
      " argument order is the base ConvexHull<float> constructor's call at"
      " 0x00A6BBC0)",
      21, 41, "count points[18] eps type",
      "dim quantity lineplane[15] tri[24]", GenConvexHull3,
      ConvexHull3Retail, ConvexHull3Ours },
    { "Wm3::Query3<float>::ToPlane(const Wm3::Vector3<float>&, int, int, int) const", 0x00A55CE0,
      "Foundation/RationalArithmetic/Wm3Query3.inl (retail thiscall, four"
      " stack arguments - the point's address and the three vertex indices -"
      " RET 0x10, the sign in EAX; the Query3<float> class vtable 0x00D71F38's"
      " +0x08 slot, and the Complete Object Locator at 0x00E5B538 names the"
      " type descriptor '.?AV?$Query3@M@Wm3@@')",
      18, 1, "verts[12] point[3] iV0 iV1 iV2", "ret", GenQuery3ToPlane,
      Query3ToPlaneRetail, Query3ToPlaneOurs },
    { "Wm3::Query3<float>::ToPlane(int, int, int, int) const", 0x00A530A0,
      "Foundation/RationalArithmetic/Wm3Query3.inl (retail thiscall, four"
      " stack arguments - the point's index and the three vertex indices -"
      " RET 0x10, the sign in EAX; the Query3<float> class vtable 0x00D71F38's"
      " +0x0C slot. The body computes &m_akVertex[i] and forwards to the"
      " +0x08 slot, so the retail's table puts the point-taking overload"
      " first - the opposite of the rebuild's WM4 order)",
      32, 1, "verts[24] point[3] i iV0 iV1 iV2 iV3", "ret", GenQuery3,
      Query3ToPlaneIndexRetail, Query3ToPlaneIndexOurs },
    { "Wm3::Query3<float>::ToTetrahedron(const Wm3::Vector3<float>&, int, int, int, int) const", 0x00A530D0,
      "Foundation/RationalArithmetic/Wm3Query3.inl (retail thiscall, five"
      " stack arguments - the point's address and the four vertex indices -"
      " RET 0x14, the sign in EAX; the Query3<float> class vtable 0x00D71F38's"
      " +0x10 slot. The body calls the +0x08 slot four times and combines the"
      " four signs)",
      32, 1, "verts[24] point[3] i iV0 iV1 iV2 iV3", "ret", GenQuery3,
      Query3ToTetrahedronRetail, Query3ToTetrahedronOurs },
    { "Wm3::Query3<float>::ToTetrahedron(int, int, int, int, int) const", 0x00A53180,
      "Foundation/RationalArithmetic/Wm3Query3.inl (retail thiscall, five"
      " stack arguments - the point's index and the four vertex indices -"
      " RET 0x14, the sign in EAX; the Query3<float> class vtable 0x00D71F38's"
      " +0x14 slot)",
      32, 1, "verts[24] point[3] i iV0 iV1 iV2 iV3", "ret", GenQuery3,
      Query3ToTetrahedronIndexRetail, Query3ToTetrahedronIndexOurs },
    { "Wm3::Query3<float>::ToCircumsphere(const Wm3::Vector3<float>&, int, int, int, int) const", 0x00A55DF0,
      "Foundation/RationalArithmetic/Wm3Query3.inl (retail thiscall, five"
      " stack arguments - the point's address and the four vertex indices -"
      " RET 0x14, the sign in EAX; the Query3<float> class vtable 0x00D71F38's"
      " +0x18 slot)",
      32, 1, "verts[24] point[3] i iV0 iV1 iV2 iV3", "ret", GenQuery3,
      Query3ToCircumsphereRetail, Query3ToCircumsphereOurs },
    { "Wm3::Query3<float>::ToCircumsphere(int, int, int, int, int) const", 0x00A531B0,
      "Foundation/RationalArithmetic/Wm3Query3.inl (retail thiscall, five"
      " stack arguments - the point's index and the four vertex indices -"
      " RET 0x14, the sign in EAX; the Query3<float> class vtable 0x00D71F38's"
      " +0x1C slot)",
      32, 1, "verts[24] point[3] i iV0 iV1 iV2 iV3", "ret", GenQuery3,
      Query3ToCircumsphereIndexRetail, Query3ToCircumsphereIndexOurs },
    { "Wm3::Query2<float>::ToLine(const Wm3::Vector2<float>&, int, int) const", 0x00A75C00,
      "Foundation/RationalArithmetic/Wm3Query2.inl (retail thiscall, three"
      " stack arguments - the point's address and the two vertex indices -"
      " RET 0x0C, the sign in EAX; the Query2<float> class vtable 0x00D72134's"
      " +0x08 slot, and the Complete Object Locator at 0x00E5BB38 names the"
      " type descriptor '.?AV?$Query2@M@Wm3@@')",
      22, 1, "verts[16] point[2] i iV0 iV1 iV2", "ret", GenQuery2,
      Query2ToLineRetail, Query2ToLineOurs },
    { "Wm3::Query2<float>::ToLine(int, int, int) const", 0x00A74E70,
      "Foundation/RationalArithmetic/Wm3Query2.inl (retail thiscall, three"
      " stack arguments - the point's index and the two vertex indices -"
      " RET 0x0C, the sign in EAX; the Query2<float> class vtable 0x00D72134's"
      " +0x0C slot)",
      22, 1, "verts[16] point[2] i iV0 iV1 iV2", "ret", GenQuery2,
      Query2ToLineIndexRetail, Query2ToLineIndexOurs },
    { "Wm3::Query2<float>::ToTriangle(const Wm3::Vector2<float>&, int, int, int) const", 0x00A74EA0,
      "Foundation/RationalArithmetic/Wm3Query2.inl (retail thiscall, four"
      " stack arguments - the point's address and the three vertex indices -"
      " RET 0x10, the sign in EAX; the Query2<float> class vtable 0x00D72134's"
      " +0x10 slot. The body calls the +0x08 slot three times)",
      22, 1, "verts[16] point[2] i iV0 iV1 iV2", "ret", GenQuery2,
      Query2ToTriangleRetail, Query2ToTriangleOurs },
    { "Wm3::Query2<float>::ToTriangle(int, int, int, int) const", 0x00A74F30,
      "Foundation/RationalArithmetic/Wm3Query2.inl (retail thiscall, four"
      " stack arguments - the point's index and the three vertex indices -"
      " RET 0x10, the sign in EAX; the Query2<float> class vtable 0x00D72134's"
      " +0x14 slot)",
      22, 1, "verts[16] point[2] i iV0 iV1 iV2", "ret", GenQuery2,
      Query2ToTriangleIndexRetail, Query2ToTriangleIndexOurs },
    { "Wm3::Query2<float>::ToCircumcircle(const Wm3::Vector2<float>&, int, int, int) const", 0x00A75C80,
      "Foundation/RationalArithmetic/Wm3Query2.inl (retail thiscall, four"
      " stack arguments - the point's address and the three vertex indices -"
      " RET 0x10, the sign in EAX; the Query2<float> class vtable 0x00D72134's"
      " +0x18 slot)",
      22, 1, "verts[16] point[2] i iV0 iV1 iV2", "ret", GenQuery2,
      Query2ToCircumcircleRetail, Query2ToCircumcircleOurs },
    { "Wm3::Query2<float>::ToCircumcircle(int, int, int, int) const", 0x00A74F60,
      "Foundation/RationalArithmetic/Wm3Query2.inl (retail thiscall, four"
      " stack arguments - the point's index and the three vertex indices -"
      " RET 0x10, the sign in EAX; the Query2<float> class vtable 0x00D72134's"
      " +0x1C slot)",
      22, 1, "verts[16] point[2] i iV0 iV1 iV2", "ret", GenQuery2,
      Query2ToCircumcircleIndexRetail, Query2ToCircumcircleIndexOurs },
    { "Wm3::ConvexHull1<float>::ConvexHull1(int, const float*, float, bool, int)", 0x00A74560,
      "Foundation/Containment/Wm3ConvexHull1.cpp:20 (retail thiscall, five"
      " stack arguments - iVertexQuantity, afVertex, fEpsilon, bOwner,"
      " eQueryType - RET 0x14; the class has one virtual slot, the"
      " destructor, so the construction is reached by address. The argument"
      " order is the base ConvexHull<float> constructor's call at 0x00A745A5,"
      " which takes (iVertexQuantity, fEpsilon, bOwner, eQueryType). The"
      " class vtable is 0x00D72114 - the RTTI slot at 0x00D72110 holds the"
      " Complete Object Locator of '.?AV?$ConvexHull1@M@Wm3@@')",
      11, 4, "count values[8] eps type", "dim quantity index0 index1",
      GenConvexHull1, ConvexHull1Retail, ConvexHull1Ours },
    { "Wm3::ConvexHull2<float>::ConvexHull2(int, Wm3::Vector2<float>*, float, bool, int)", 0x00A78E30,
      "Foundation/Containment/Wm3ConvexHull2.cpp:24 (retail thiscall, five"
      " stack arguments - iVertexQuantity, akVertex, fEpsilon, bOwner,"
      " eQueryType - RET 0x14; the class has one virtual slot, the"
      " destructor, so the construction is reached by address. The argument"
      " order is the base ConvexHull<float> constructor's call at 0x00A78E76."
      " The class vtable is 0x00D72124 - the RTTI slot at 0x00D72120 holds"
      " the Complete Object Locator of '.?AV?$ConvexHull2@M@Wm3@@')",
      19, 20, "count points[16] eps type",
      "dim quantity edge0[2] ... edge8[2]", GenConvexHull2,
      ConvexHull2Retail, ConvexHull2Ours },
    // ---- the double instantiation ----------------------------------------
    //
    // These four rows come last on purpose: the seed of a row is derived
    // from its index in the merged table, so a row inserted in the middle
    // would move every seed behind it.  Appending keeps the existing rows'
    // seeds where they are.
    { "Wm3::Distance<double, Wm3::Vector3<double>>::SetDifferenceStep(double)", 0x00A39DB0,
      "Foundation/Distance/Wm3Distance.cpp:71 (retail thiscall, one double"
      " argument on the stack and the callee pops it - RET 8 - the step lands"
      " at [this+0x58] and 0.5/step at [this+0x60]. The base class's own"
      " vftable is 0x00D71CA4 - the base constructor at 0x00A39FA0 installs"
      " it - and this body is a member of the base, so the row runs on an"
      " object the retail's own base constructor built. The retail's body is"
      " x87 code and the engine runs under _PC_24, so 0.5/step is a 24-bit"
      " significand, which is why the rebuild's double branch emulates it)",
      2, 4, "step", "step invtwo", GenDistanceSetDifferenceStepDouble,
      Distance3dSetDifferenceStepRetail, Distance3dSetDifferenceStepOurs },
    { "Wm3::Distance<double, Wm3::Vector3<double>>::Distance()", 0x00A39FA0,
      "Foundation/Distance/Wm3Distance.cpp:22 (retail thiscall, no stack"
      " arguments: the base constructor of the double instantiation, which"
      " inlines the defaults - the step 1e-3 at [this+0x58] and its"
      " reciprocal 500.0 at [this+0x60]. The rebuild's constructor calls the"
      " setter, so this row is what says the two agree there)",
      2, 4, "step", "step invtwo", GenDistanceSetDifferenceStepDouble,
      Distance3dDifferenceStepDefaultRetail, Distance3dDifferenceStepDefaultOurs },
    { "Wm3::Distance<double, Wm3::Vector2<double>>::SetDifferenceStep(double)", 0x00A397C0,
      "Foundation/Distance/Wm3Distance.cpp:71 (the Vector2 instantiation of"
      " the same base: the class vtable is 0x00D71C7C, the base constructor"
      " is 0x00A39F60, the step lands at [this+0x48] and 0.5/step at"
      " [this+0x50])",
      2, 4, "step", "step invtwo", GenDistanceSetDifferenceStepDouble,
      Distance2dSetDifferenceStepRetail, Distance2dSetDifferenceStepOurs },
    { "Wm3::Distance<double, Wm3::Vector2<double>>::Distance()", 0x00A39F60,
      "Foundation/Distance/Wm3Distance.cpp:22 (the Vector2 instantiation of"
      " the base constructor, which inlines the defaults - the step 1e-3 at"
      " [this+0x48] and its reciprocal 500.0 at [this+0x50])",
      2, 4, "step", "step invtwo", GenDistanceSetDifferenceStepDouble,
      Distance2dDifferenceStepDefaultRetail, Distance2dDifferenceStepDefaultOurs },
    { "Wm3::DistVector3Box3<double>::GetSquared()", 0x00A460D0,
      "Foundation/Distance/Wm3DistVector3Box3.cpp:45 (the double"
      " instantiation: the class vtable 0x00D71DB4's +0x18 slot, ECX ="
      " `this`, no stack arguments, the double in ST(0). The retail's body is"
      " x87 code and the engine runs under _PC_24, so every product and every"
      " sum is a 24-bit significand with the extended exponent - which is why"
      " the rebuild's double branch spells its arithmetic out in x87"
      " instructions)",
      36, 2, "vec[3] box[15]", "ret", GenDistVector3Box3Double,
      DistVector3Box3dGetSquaredRetail, DistVector3Box3dGetSquaredOurs },
    { "Wm3::DistVector3Box3<double>::Get()", 0x00A45800,
      "Foundation/Distance/Wm3DistVector3Box3.cpp:40 (the double"
      " instantiation: the class vtable 0x00D71DB4's +0x0C slot. It"
      " dispatches the +0x18 slot and jumps to the CRT's square root at"
      " 0x00A91000, which normalises the control word to 0x027F before the"
      " FSQRT - so the square root itself is a 53-bit significand even"
      " though the distance below it is a 24-bit one)",
      36, 2, "vec[3] box[15]", "ret", GenDistVector3Box3dGet,
      DistVector3Box3dGetRetail, DistVector3Box3dGetOurs },
    { "Wm3::DistVector3Segment3<double>::GetSquared()", 0x00A48910,
      "Foundation/Distance/Wm3DistVector3Segment3.cpp:46 (the double"
      " instantiation: the class vtable 0x00D71E94's +0x18 slot, ECX ="
      " `this`, no stack arguments, the double in ST(0). The retail's body is"
      " x87 code under _PC_24, so the differences, the parameter, the closest"
      " point and the squared distance are all 24-bit significands)",
      20, 2, "vec[3] segment[7]", "ret", GenDistVector3Segment3Double,
      DistVector3Segment3dGetSquaredRetail, DistVector3Segment3dGetSquaredOurs },
    { "Wm3::DistVector3Segment3<double>::Get()", 0x00A48100,
      "Foundation/Distance/Wm3DistVector3Segment3.cpp:39 (the double"
      " instantiation: the class vtable 0x00D71E94's +0x0C slot; it dispatches"
      " the +0x18 slot and jumps to the CRT's square root at 0x00A91000)",
      20, 2, "vec[3] segment[7]", "ret", GenDistVector3Segment3Double,
      DistVector3Segment3dGetRetail, DistVector3Segment3dGetOurs },
    { "Wm3::DistVector3Segment3<double>::GetSquared(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)", 0x00A487E0,
      "Foundation/Distance/Wm3DistVector3Segment3.cpp:57 (the double"
      " instantiation's swept form: the class vtable 0x00D71E94's +0x14 slot,"
      " ECX = `this`, three stack arguments - the double fT and the two"
      " velocity pointers - and `RET 0x10`; the double comes back in ST(0)."
      " The body moves the point and the segment's origin by fT times the two"
      " velocities on the x87, builds the moved segment and asks the class's"
      " own static GetSquared (the +0x18 slot, 0x00A48910) for the distance)",
      34, 2, "vec[3] segment[7] t vel0[3] vel1[3]", "ret",
      GenDistVector3Segment3dSwept,
      DistVector3Segment3dGetSquaredSweptRetail, DistVector3Segment3dGetSquaredSweptOurs },
    { "Wm3::DistVector3Segment3<double>::Get(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)", 0x00A486B0,
      "Foundation/Distance/Wm3DistVector3Segment3.cpp:39 (the double"
      " instantiation's swept form: the class vtable 0x00D71E94's +0x08 slot,"
      " same ABI as the swept GetSquared; it dispatches the +0x14 slot and"
      " jumps to the CRT's square root at 0x00A91000)",
      34, 2, "vec[3] segment[7] t vel0[3] vel1[3]", "ret",
      GenDistVector3Segment3dSwept,
      DistVector3Segment3dGetSweptRetail, DistVector3Segment3dGetSweptOurs },
    { "Wm3::DistVector3Box3<double>::GetSquared(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)", 0x00A45FB0,
      "Foundation/Distance/Wm3DistVector3Box3.cpp:50 (the double"
      " instantiation's swept form: the class vtable 0x00D71DB4's +0x14 slot,"
      " ECX = `this`, three stack arguments - the double fT and the two"
      " velocity pointers - and `RET 0x10`; the double comes back in ST(0))",
      50, 2, "vec[3] box[15] t vel0[3] vel1[3]", "ret",
      GenDistVector3Box3dSwept,
      DistVector3Box3dGetSquaredSweptRetail, DistVector3Box3dGetSquaredSweptOurs },
    { "Wm3::DistVector3Box3<double>::Get(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)", 0x00A45E90,
      "Foundation/Distance/Wm3DistVector3Box3.cpp:45 (the double"
      " instantiation's swept form: the class vtable 0x00D71DB4's +0x08 slot,"
      " same ABI as the swept GetSquared; it dispatches the +0x14 slot and"
      " jumps to the CRT's square root at 0x00A91000)",
      50, 2, "vec[3] box[15] t vel0[3] vel1[3]", "ret",
      GenDistVector3Box3dSwept,
      DistVector3Box3dGetSweptRetail, DistVector3Box3dGetSweptOurs },
    { "Wm3::DistVector2Box2<double>::GetSquared()", 0x00A6CD70,
      "Foundation/Distance/Wm3DistVector2Box2.cpp:45 (the double"
      " instantiation: the class vtable 0x00D720EC's +0x18 slot, ECX ="
      " `this`, no stack arguments, the double in ST(0). The retail's body"
      " is x87 code and the engine runs under _PC_24, so every product and"
      " every sum is a 24-bit significand - which is why the rebuild's"
      " double branch spells the projections and the deltas out in the"
      " Wm3Detail::X87 helpers)",
      20, 2, "vec[2] box[8]", "ret", GenDistVector2Box2Double,
      DistVector2Box2dGetSquaredRetail, DistVector2Box2dGetSquaredOurs },
    { "Wm3::DistVector2Box2<double>::Get()", 0x00A6C6A0,
      "Foundation/Distance/Wm3DistVector2Box2.cpp:39 (the double"
      " instantiation: the class vtable 0x00D720EC's +0x0C slot; it"
      " dispatches the +0x18 slot and jumps to the CRT's square root at"
      " 0x00A91000, which keeps the ambient precision control)",
      20, 2, "vec[2] box[8]", "ret", GenDistVector2Box2Double,
      DistVector2Box2dGetRetail, DistVector2Box2dGetOurs },
    { "Wm3::DistVector2Box2<double>::GetSquared(double, const Wm3::Vector2<double>&, const Wm3::Vector2<double>&)", 0x00A6CC60,
      "Foundation/Distance/Wm3DistVector2Box2.cpp:50 (the double"
      " instantiation's swept form: the class vtable 0x00D720EC's +0x14"
      " slot, ECX = `this`, three stack arguments - the double fT and the"
      " two velocity pointers - and `RET 0x10`; the double comes back in"
      " ST(0). The body moves the point and the box's centre on the x87 and"
      " asks the class's own static GetSquared for the distance)",
      30, 2, "vec[2] box[8] t vel0[2] vel1[2]", "ret",
      GenDistVector2Box2DoubleSwept,
      DistVector2Box2dGetSquaredSweptRetail, DistVector2Box2dGetSquaredSweptOurs },
    { "Wm3::DistVector2Box2<double>::Get(double, const Wm3::Vector2<double>&, const Wm3::Vector2<double>&)", 0x00A6CB50,
      "Foundation/Distance/Wm3DistVector2Box2.cpp:39 (the double"
      " instantiation's swept form: the class vtable 0x00D720EC's +0x08"
      " slot, same ABI as the swept GetSquared; it dispatches the +0x14 slot"
      " and jumps to the CRT's square root at 0x00A91000)",
      30, 2, "vec[2] box[8] t vel0[2] vel1[2]", "ret",
      GenDistVector2Box2DoubleSwept,
      DistVector2Box2dGetSweptRetail, DistVector2Box2dGetSweptOurs },
    { "Wm3::DistLine3Segment3<double>::GetSquared()", 0x00A81330,
      "Foundation/Distance/Wm3DistLine3Segment3.cpp:51 (the double"
      " instantiation: the class vtable 0x00D722FC's +0x18 slot, ECX ="
      " `this`, no stack arguments, the double in ST(0). The retail's body"
      " is x87 code and the engine runs under _PC_24, so every product and"
      " every sum is a 24-bit significand - and the instruction sequence"
      " gives the same orderings as the float body's, the inline dots"
      " included, so the rebuild's double branch runs the same modelled"
      " arithmetic)",
      26, 2, "line[6] segment[7]", "ret", GenDistLine3Segment3Double,
      DistLine3Segment3dGetSquaredRetail, DistLine3Segment3dGetSquaredOurs },
    { "Wm3::DistLine3Segment3<double>::Get()", 0x00A809C0,
      "Foundation/Distance/Wm3DistLine3Segment3.cpp:38 (the double"
      " instantiation: the class vtable 0x00D722FC's +0x0C slot; it"
      " dispatches the +0x18 slot and jumps to the CRT's square root at"
      " 0x00A91000, which keeps the ambient precision control)",
      26, 2, "line[6] segment[7]", "ret", GenDistLine3Segment3Double,
      DistLine3Segment3dGetRetail, DistLine3Segment3dGetOurs },
    { "Wm3::DistLine3Segment3<double>::GetSquared(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)", 0x00A811F0,
      "Foundation/Distance/Wm3DistLine3Segment3.cpp:206 (the double"
      " instantiation's swept form: the class vtable 0x00D722FC's +0x14"
      " slot, ECX = `this`, three stack arguments - the double fT and the"
      " two velocity pointers - and `RET 0x10`; the double comes back in"
      " ST(0))",
      40, 2, "line[6] segment[7] t vel0[3] vel1[3]", "ret",
      GenDistLine3Segment3DoubleSwept,
      DistLine3Segment3dGetSquaredSweptRetail, DistLine3Segment3dGetSquaredSweptOurs },
    { "Wm3::DistLine3Segment3<double>::Get(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)", 0x00A810B0,
      "Foundation/Distance/Wm3DistLine3Segment3.cpp:187 (the double"
      " instantiation's swept form: the class vtable 0x00D722FC's +0x08"
      " slot, same ABI as the swept GetSquared; it dispatches the +0x14 slot"
      " and jumps to the CRT's square root at 0x00A91000)",
      40, 2, "line[6] segment[7] t vel0[3] vel1[3]", "ret",
      GenDistLine3Segment3DoubleSwept,
      DistLine3Segment3dGetSweptRetail, DistLine3Segment3dGetSweptOurs },
    { "Wm3::IntrBox2Circle2<double>::Test()", 0x00A4EE50,
      "Foundation/Intersection/Wm3IntrBox2Circle2.h (the double"
      " instantiation: the class vtable 0x00D71EDC's +0x08 slot - ECX = `this`, no"
      " stack arguments, the bool in EAX. The class's Test delegates to"
      " DistVector2Box2<double>::Get(), whose double branch IS the modelled"
      " x87 one, so this body inherits the modelling)"
      " The row compares a bool, so it witnesses the precision only through"
      " an exact collision: the generator builds one - the box's axes are"
      " the identity, its first extent is 2^-32 short of one and the"
      " circle's centre is 2^-30 past that face, with the radius 3 * 2^-32"
      " between the two sides' distances (the retail's 2^-32, the binary64"
      " 5 * 2^-32).  The mutation check - the double branch put back on the"
      " plain binary64 source - fails 99960/200000 (seed 0x5EED2007, 200000"
      " draws); with the transcription in place the row passes 200000/200000"
      " and 1000000/1000000 on all five seeds",
      22, 1, "box[8] circle[3]", "ret", GenIntrBox2Circle2Double,
      IntrBox2Circle2dTestRetail, IntrBox2Circle2dTestOurs },
    { "Wm3::IntrSegment3Capsule3<double>::Test()", 0x00A6BF80,
      "Foundation/Intersection/Wm3IntrSegment3Capsule3.h (the double"
      " instantiation: the class vtable 0x00D720AC's +0x08 slot, ECX ="
      " `this`, no stack arguments, the bool in EAX. The class's Test"
      " delegates to DistSegment3Segment3<double>::Get(), whose double"
      " branch is now transcribed - the intermediates run on the hardware's"
      " x87 and the square root is the x87's as well)"
      " The row compares a bool, so it witnesses the precision only through"
      " an exact collision: the generator builds one - both segments"
      " degenerate to points, one 1 + 5 * 2^-26 above the other along y,"
      " and the capsule's radius is 1 + 6 * 2^-26.  The x87 rounds the"
      " difference of the origins to 1 + 2^-23, so the distance is"
      " 1 + 2^-23 - above the radius, and the test is false; the binary64"
      " chain keeps the difference exact, so its distance is the gap"
      " itself, 1 + 5 * 2^-26, below the radius, and the test is true."
      " The mutation check - the distance class's double branch put back"
      " on the plain binary64 source - fails 100088/200000 (seed"
      " 0x5EED2007, 200000 draws); with the transcription in place the row"
      " passes 200000/200000 and 1000000/1000000 on all five seeds)",
      30, 1, "segment[7] capsule[8]", "ret", GenIntrSegment3Capsule3Double,
      IntrSegment3Capsule3dTestRetail, IntrSegment3Capsule3dTestOurs },
    { "Wm3::IntrBox3Sphere3<double>::Test()", 0x00A425C0,
      "Foundation/Intersection/Wm3IntrBox3Sphere3.h (the double"
      " instantiation: the class vtable 0x00D71D14's +0x08 slot, ECX ="
      " `this`, no stack arguments, the bool in EAX. The retail's double body"
      " is x87 code and its inline dot is formed as (y*y' + x*x') + z*z' -"
      " the y term first, unlike the shared Vector3::Dot - so the"
      " transcription has to spell the difference, the three dots and the"
      " sums out in the Wm3Detail::X87 helpers)"
      " The rebuild's double branch is transcribed in the Wm3Detail::X87"
      " helpers, with the retail's own groupings.  The row compares a"
      " bool, so it witnesses the precision only through an exact"
      " collision: the generator builds one - the box's axes are the"
      " identity, the sphere's centre is one 2^-30 past one of the"
      " faces and that face is 2^-32 short of the coordinate, and the"
      " radius 3 * 2^-32 sits between the two sides' distances to the"
      " face (the retail's 2^-32, the binary64 5 * 2^-32).  The mutation"
      " check - the double branch put back on the plain binary64 source"
      " - fails 100317/200000 (seed 0x5EED2007, 200000 draws); with the"
      " transcription in place the row passes 200000/200000 and"
      " 1000000/1000000 on all five seeds)",
      38, 1, "box[15] sphere[4]", "ret", GenIntrBox3Sphere3Double,
      IntrBox3Sphere3dTestRetail, IntrBox3Sphere3dTestOurs },
    { "Wm3::IntrBox2Box2<double>::Test()", 0x00A47F00,
      "Foundation/Intersection/Wm3IntrBox2Box2.h (the double instantiation: the class"
      " vtable's +0x08 slot, ECX = `this`, no stack arguments, the bool in"
      " AL. The retail's double body is x87 code, so the rebuild's double"
      " branch models the arithmetic in the Wm3Detail::X87 helpers)",
      32, 1, "box0[8] box1[8]", "ret", GenIntrBox2Box2Double,
      IntrBox2Box2dTestRetail, IntrBox2Box2dTestOurs },
    { "Wm3::IntrLine3Box3<double>::Test()", 0x00A50090,
      "Foundation/Intersection/Wm3IntrLine3Box3.h (the double instantiation: the class"
      " vtable's +0x08 slot, ECX = `this`, no stack arguments, the bool in"
      " AL. The retail's double body is x87 code; this class's float path"
      " is already transcribed, and so is its double path now)"
      " The rebuild's double branch is transcribed in the Wm3Detail::X87"
      " helpers, with the retail's own groupings.  The row compares a"
      " bool, so it witnesses the precision only through an exact"
      " collision: the generator builds one - the direction is a unit"
      " axis, the box's axes are the identity with unit extents, and the"
      " line's origin is one 2^-30 off the box's centre along a"
      " coordinate that is not the direction's, so the cross product's"
      " component is 1.0 on the x87 and 1 + 2^-30 in binary64 against a"
      " right-hand side of exactly 1.0.  The mutation check - the double"
      " branch put back on the plain binary64 source - fails"
      " 100001/200000 (seed 0x5EED2007, 200000 draws); with the"
      " transcription in place the row passes 200000/200000 and"
      " 1000000/1000000 on all five seeds)",
      42, 1, "line[6] box[15]", "ret", GenIntrLine3Box3Double,
      IntrLine3Box3dTestRetail, IntrLine3Box3dTestOurs },
    { "Wm3::IntrSegment3Box3<double>::Test()", 0x00A46720,
      "Foundation/Intersection/Wm3IntrSegment3Box3.h (the double instantiation: the class"
      " vtable's +0x08 slot, ECX = `this`, no stack arguments, the bool in"
      " AL. The retail's double body is x87 code; this class's float path"
      " is already transcribed, and so is its double path now)"
      " The rebuild's double branch is transcribed in the Wm3Detail::X87"
      " helpers, with the retail's own groupings.  The row compares a"
      " bool, so it witnesses the precision only through an exact"
      " collision: the generator builds one - the direction is a unit"
      " axis, the box's axes are the identity with unit extents, and the"
      " segment's origin is one 2^-30 off the box's centre along a"
      " coordinate that is not the direction's, so the first test"
      " compares the retail's 1.0 with a right-hand side of exactly"
      " 1.0.  The mutation check - the double branch put back on the"
      " plain binary64 source - fails 99619/200000 (seed 0x5EED2007,"
      " 200000 draws); with the transcription in place the row passes"
      " 200000/200000 and 1000000/1000000 on all five seeds)",
      45, 1, "segment[7] box[15] solid", "ret", GenIntrSegment3Box3Double,
      IntrSegment3Box3dTestRetail, IntrSegment3Box3dTestOurs },
    { "Wm3::IntrSegment3Sphere3<double>::Test()", 0x00A475E0,
      "Foundation/Intersection/Wm3IntrSegment3Sphere3.h (the double instantiation: the class"
      " vtable's +0x08 slot, ECX = `this`, no stack arguments, the bool in"
      " AL. The rebuild's source already has the retail's grouping - the"
      " three terms x, y, z - and the double path now has the precision too)"
      " The rebuild's double branch is transcribed in the Wm3Detail::X87"
      " helpers, with the retail's own groupings.  The row compares a"
      " bool, so it witnesses the precision only through an exact"
      " collision: the generator builds one - the sphere's centre is the"
      " origin, its radius is one, the segment's direction is a unit"
      " axis and its origin is one 2^-30 off the centre along a"
      " coordinate that is not the direction's, so the retail's fA0 is 0"
      " (it rounds the squared distance to 1.0) and its discriminant is"
      " 0, while the binary64 fA0 is 2^-29 and its discriminant is"
      " negative.  The mutation check - the double branch put back on the"
      " plain binary64 source - fails 99956/200000 (seed 0x5EED2007,"
      " 200000 draws); with the transcription in place the row passes"
      " 200000/200000 and 1000000/1000000 on all five seeds)",
      22, 1, "segment[7] sphere[4]", "ret", GenIntrSegment3Sphere3Double,
      IntrSegment3Sphere3dTestRetail, IntrSegment3Sphere3dTestOurs },
    { "Wm3::Distance<double, Wm3::Vector3<double>>::Get(double, double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)", 0x00A39860,
      "Foundation/Distance/Wm3Distance.cpp:125 (retail thiscall, four stack"
      " arguments - fTMin, fTMax, &rkVelocity0, &rkVelocity1, the two doubles"
      " first - and RET 0x18, so the callee pops all 24 bytes; the double"
      " comes back in ST(0). The base class's own vftable is 0x00D71CA4 - the"
      " base constructor at 0x00A39FA0 installs it - and this body is its"
      " +0x04 slot: the base's window search, not a destructor. The row runs"
      " on a DistVector3Segment3<double> so the body's vtable dispatch"
      " reaches the derived class's own swept Get. The retail's body is x87"
      " code, so the rebuild's double branch models the arithmetic in the"
      " Wm3Detail::X87 helpers)",
      36, 2, "vec[3] segment[7] tmin tmax vel0[3] vel1[3]", "ret",
      GenDistance3dWindow,
      Distance3dWindowGetRetail, Distance3dWindowGetOurs },
    { "Wm3::Distance<double, Wm3::Vector3<double>>::GetSquared(double, double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)", 0x00A39AB0,
      "Foundation/Distance/Wm3Distance.cpp:245 (retail thiscall, four stack"
      " arguments - fTMin, fTMax, &rkVelocity0, &rkVelocity1 - and RET 0x18,"
      " the double in ST(0). The base class's own vftable is 0x00D71CA4 and"
      " this body is its +0x10 slot: the base's window search for the"
      " squared distance. The retail's base has no generic three-argument"
      " GetSquared at all - the slot is pure virtual, so +0x10 is this"
      " window body and not one)",
      36, 2, "vec[3] segment[7] tmin tmax vel0[3] vel1[3]", "ret",
      GenDistance3dWindow,
      Distance3dWindowGetSquaredRetail, Distance3dWindowGetSquaredOurs },
    { "Wm3::Distance<double, Wm3::Vector3<double>>::GetDerivative(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)", 0x00A39D00,
      "Foundation/Distance/Wm3Distance.cpp:102 (retail thiscall, three stack"
      " arguments - the double fT and the two velocity pointers - and"
      " RET 0x10, the double in ST(0). The base class's own vftable is"
      " 0x00D71CA4 and this body is its +0x1C slot: the base's finite"
      " difference default, the one DistVector3Segment3 does not override."
      " The body computes the two sample times on the x87 - `fld"
      " m_fDifferenceStep; fadd fT` and `fld fT; fsub m_fDifferenceStep` -"
      " so the rebuild's double branch models the arithmetic in the"
      " Wm3Detail::X87 helpers)",
      34, 2, "vec[3] segment[7] t vel0[3] vel1[3]", "ret",
      GenDistance3dDerivative,
      Distance3dGetDerivativeRetail, Distance3dGetDerivativeOurs },
    { "Wm3::Distance<double, Wm3::Vector3<double>>::GetDerivativeSquared(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)", 0x00A39D60,
      "Foundation/Distance/Wm3Distance.cpp:113 (retail thiscall, three stack"
      " arguments - the double fT and the two velocity pointers - and"
      " RET 0x10, the double in ST(0). The base class's own vftable is"
      " 0x00D71CA4 and this body is its +0x20 slot. The body doubles the"
      " distance on the x87 (`fadd st(0), st(0)`) and multiplies it by the"
      " derivative (`fmulp st(1)`), so the rebuild's double branch models"
      " the arithmetic in the Wm3Detail::X87 helpers)",
      34, 2, "vec[3] segment[7] t vel0[3] vel1[3]", "ret",
      GenDistance3dDerivative,
      Distance3dGetDerivativeSquaredRetail, Distance3dGetDerivativeSquaredOurs },
    { "Wm3::Distance<double, Wm3::Vector2<double>>::Get(double, double, const Wm3::Vector2<double>&, const Wm3::Vector2<double>&)", 0x00A39270,
      "Foundation/Distance/Wm3Distance.cpp:125 (retail thiscall, four stack"
      " arguments - fTMin, fTMax, &rkVelocity0, &rkVelocity1 - and RET 0x18,"
      " the double in ST(0). The base class's own vftable is 0x00D71C7C - the"
      " RTTI in front of it names the Vector2 instantiation - and this body"
      " is its +0x04 slot. The row runs on a DistVector2Box2<double> so the"
      " body's vtable dispatch reaches the derived class's own swept Get."
      " The retail's body is x87 code, so the rebuild's double branch"
      " models the arithmetic in the Wm3Detail::X87 helpers)",
      32, 2, "vec[2] box[8] tmin tmax vel0[2] vel1[2]", "ret",
      GenDistance2dWindow,
      Distance2dWindowGetRetail, Distance2dWindowGetOurs },
    { "Wm3::Distance<double, Wm3::Vector2<double>>::GetSquared(double, double, const Wm3::Vector2<double>&, const Wm3::Vector2<double>&)", 0x00A394C0,
      "Foundation/Distance/Wm3Distance.cpp:245 (retail thiscall, four stack"
      " arguments - fTMin, fTMax, &rkVelocity0, &rkVelocity1 - and RET 0x18,"
      " the double in ST(0). The base class's own vftable is 0x00D71C7C and"
      " this body is its +0x10 slot. The retail's body is x87 code, so the"
      " rebuild's double branch models the arithmetic in the Wm3Detail::X87"
      " helpers)",
      32, 2, "vec[2] box[8] tmin tmax vel0[2] vel1[2]", "ret",
      GenDistance2dWindow,
      Distance2dWindowGetSquaredRetail, Distance2dWindowGetSquaredOurs },
    { "Wm3::Distance<double, Wm3::Vector2<double>>::GetDerivative(double, const Wm3::Vector2<double>&, const Wm3::Vector2<double>&)", 0x00A39710,
      "Foundation/Distance/Wm3Distance.cpp:102 (retail thiscall, three stack"
      " arguments - the double fT and the two velocity pointers - and"
      " RET 0x10, the double in ST(0). The base class's own vftable is"
      " 0x00D71C7C and this body is its +0x1C slot; the difference step"
      " lives at [this+0x48] and its reciprocal at [this+0x50]. The retail's"
      " body is x87 code, so the rebuild's double branch models the"
      " arithmetic in the Wm3Detail::X87 helpers)",
      30, 2, "vec[2] box[8] t vel0[2] vel1[2]", "ret",
      GenDistance2dDerivative,
      Distance2dGetDerivativeRetail, Distance2dGetDerivativeOurs },
    { "Wm3::Distance<double, Wm3::Vector2<double>>::GetDerivativeSquared(double, const Wm3::Vector2<double>&, const Wm3::Vector2<double>&)", 0x00A39770,
      "Foundation/Distance/Wm3Distance.cpp:113 (retail thiscall, three stack"
      " arguments - the double fT and the two velocity pointers - and"
      " RET 0x10, the double in ST(0). The base class's own vftable is"
      " 0x00D71C7C and this body is its +0x20 slot. The retail's body is x87"
      " code, so the rebuild's double branch models the arithmetic in the"
      " Wm3Detail::X87 helpers)",
      30, 2, "vec[2] box[8] t vel0[2] vel1[2]", "ret",
      GenDistance2dDerivative,
      Distance2dGetDerivativeSquaredRetail, Distance2dGetDerivativeSquaredOurs },
    { "Wm3::DistSegment3Segment3<double>::GetSquared()", 0x00A7F100,
      "Foundation/Distance/Wm3DistSegment3Segment3.cpp:143 (the double"
      " instantiation: the class vtable 0x00D7227C's +0x18 slot, ECX ="
      " `this`, no stack arguments, the double in ST(0). The retail's body"
      " is x87 code and the engine runs under _PC_24, so every product and"
      " every sum is a 24-bit significand with the extended exponent."
      " The rebuild's double branch therefore carries its intermediates"
      " in Wm3Detail::X87Asm, which runs the operations on the hardware's"
      " x87: the operands are 53-bit doubles, so a product needs 106 bits"
      " and the software model (which rounds the binary64 product and then"
      " the significand) would land an ulp of the 24-bit significand away"
      " from the retail. The instruction sequence of the retail's double"
      " body is the same as its float body's - the two agree bit for bit"
      " on every input whose segments are float-representable - so the"
      " double branch is the same source with the x87's rounding)",
      28, 2, "seg0[7] seg1[7]", "ret", GenDistSegment3Segment3Double,
      DistSegment3Segment3dGetSquaredRetail,
      DistSegment3Segment3dGetSquaredOurs },
    { "Wm3::DistSegment3Segment3<double>::Get()", 0x00A7E250,
      "Foundation/Distance/Wm3DistSegment3Segment3.cpp:86 (the double"
      " instantiation: the class vtable 0x00D7227C's +0x0C slot; it"
      " dispatches the +0x18 slot and jumps to the CRT's square root at"
      " 0x00A91000, which keeps the ambient precision control - so under"
      " the engine's _PC_24 the square root is a 24-bit one and not the"
      " correctly rounded binary64 square root Math<double>::Sqrt"
      " computes)",
      28, 2, "seg0[7] seg1[7]", "ret", GenDistSegment3Segment3DoubleGet,
      DistSegment3Segment3dGetRetail, DistSegment3Segment3dGetOurs },
    { "Wm3::DistSegment3Segment3<double>::GetSquared(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)", 0x00A7EFB0,
      "Foundation/Distance/Wm3DistSegment3Segment3.cpp:596 (the double"
      " instantiation's swept form: the class vtable 0x00D7227C's +0x14"
      " slot, ECX = `this`, three stack arguments - the double fT and the"
      " two velocity pointers - and `RET 0x10`; the double comes back in"
      " ST(0). The body moves the two shapes on the x87 as well, so the"
      " rebuild's double branch moves them on the x87 too)",
      42, 2, "seg0[7] seg1[7] t vel0[3] vel1[3]", "ret",
      GenDistSegment3Segment3DoubleSwept,
      DistSegment3Segment3dGetSquaredSweptRetail,
      DistSegment3Segment3dGetSquaredSweptOurs },
    { "Wm3::DistSegment3Segment3<double>::Get(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)", 0x00A7EE60,
      "Foundation/Distance/Wm3DistSegment3Segment3.cpp:561 (the double"
      " instantiation's swept form: the class vtable 0x00D7227C's +0x08"
      " slot, same ABI as the swept GetSquared; it dispatches the +0x14"
      " slot and jumps to the CRT's square root at 0x00A91000)",
      42, 2, "seg0[7] seg1[7] t vel0[3] vel1[3]", "ret",
      GenDistSegment3Segment3DoubleSwept,
      DistSegment3Segment3dGetSweptRetail, DistSegment3Segment3dGetSweptOurs },
    { "Wm3::IntrLine3Box3<double>::Find()", 0x00A50940,
      "Foundation/Intersection/Wm3IntrLine3Box3.cpp:223 (the double"
      " instantiation: the class vtable 0x00D71F0C's +0x0C slot, ECX ="
      " `this`, no stack arguments, the bool in AL. The body is a thin"
      " wrapper around the shared double DoClipping (0x00A505F0): it"
      " passes the parameter range [-MAX_REAL, +MAX_REAL] and the solid"
      " flag, so the rebuild's double branch of DoClipping and Clip is"
      " the transcription of those two bodies. The retail's body is x87"
      " code and the engine runs under _PC_24, so every product and"
      " every sum is a 24-bit significand with the extended exponent;"
      " the rebuild's double branch carries its intermediates in"
      " Wm3Detail::X87Asm, which runs the operations on the hardware's"
      " x87)",
      42, 15, "line[6] box[15]", "hit type quantity point0[3] point1[3]",
      GenIntrLine3Box3Double,
      IntrLine3Box3dFindRetail, IntrLine3Box3dFindOurs },
    { "Wm3::IntrSegment3Box3<double>::Find()", 0x00A46330,
      "Foundation/Intersection/Wm3IntrSegment3Box3.cpp:283 (the double"
      " instantiation: the class vtable 0x00D71DF4's +0x0C slot, ECX ="
      " `this`, no stack arguments, the bool in AL. The body is a thin"
      " wrapper around the shared double DoClipping (0x00A505F0): it"
      " passes the parameter range [-Extent, +Extent] and the object's"
      " own solid flag, so the rebuild's double branch of DoClipping and"
      " Clip is the transcription of those two bodies. The retail's body"
      " is x87 code and the engine runs under _PC_24, so every product"
      " and every sum is a 24-bit significand with the extended"
      " exponent; the rebuild's double branch carries its intermediates"
      " in Wm3Detail::X87Asm, which runs the operations on the hardware's"
      " x87)",
      45, 15, "segment[7] box[15] solid",
      "hit type quantity point0[3] point1[3]",
      GenIntrSegment3Box3Double,
      IntrSegment3Box3dFindRetail, IntrSegment3Box3dFindOurs },
    { "Wm3::IntrSegment3Sphere3<double>::Find()", 0x00A479B0,
      "Foundation/Intersection/Wm3IntrSegment3Sphere3.cpp:231 (the double"
      " instantiation: the class vtable 0x00D71E24's +0x0C slot - the"
      " *static* Find, the dynamic one is 0x00A47690 - ECX = `this`, no"
      " stack arguments, the bool in AL. The retail's body is x87 code and"
      " the engine runs under _PC_24, so every product and every sum is a"
      " 24-bit significand with the extended exponent, and the square root"
      " is the CRT's (0x00A91000), which under _PC_24 is a 24-bit one; the"
      " rebuild's double branch therefore carries its intermediates in"
      " Wm3Detail::X87Asm, which runs the operations on the hardware's x87)",
      22, 18, "segment[7] sphere[4]",
      "hit quantity point0[3] point1[3] t0 t1",
      GenIntrSegment3Sphere3Double,
      IntrSegment3Sphere3dFindRetail, IntrSegment3Sphere3dFindOurs },
    { "Wm3::Vector3<double>::GenerateOrthonormalBasis(Wm3::Vector3<double>&, Wm3::Vector3<double>&, Wm3::Vector3<double>&, bool)", 0x00A3D150,
      "Foundation/Math/Wm3Vector3.inl:618 (the retail's double instantiation:"
      " a *static* member, so there is no `this` - four stack arguments,"
      " &rkU, &rkV, &rkW and the bUnitLengthW flag - and the body ends in a"
      " plain RET, so the caller pops them. The order is settled by the"
      " retail's own call site at 0x00A800B4 (the double"
      " IntrLine3Capsule3::Find), which pushes the flag first and the three"
      " pointers after it and pops all four with `ADD ESP,0x10`. The body is"
      " x87 code and the engine runs under _PC_24, so every product, every"
      " sum and the square root carry a 24-bit significand; the rebuild's"
      " double branch carries its intermediates in Wm3Detail::X87, which"
      " runs the operations on the hardware's x87. The algorithm is the two"
      " branch one - the same one the WM3.4 copy has - and the row is a"
      " value row: it compares U and V, six doubles)",
      7, 12, "w[3] unit", "u[3] v[3]", GenVector3OrthonormalBasisd,
      Vector3GenerateOrthonormalBasisdRetail,
      Vector3GenerateOrthonormalBasisdOurs },
    { "Wm3::Vector3<float>::GenerateOrthonormalBasis(Wm3::Vector3<float>&, Wm3::Vector3<float>&, Wm3::Vector3<float>&, bool)", 0x00A3CF80,
      "Foundation/Math/Wm3Vector3.inl:618 (the retail's float instantiation:"
      " the same cdecl static member, the same four stack arguments, and the"
      " same two branch algorithm. The row's magnitudes stay in the range"
      " where the rebuild's float path and the retail's x87 body agree: the"
      " retail keeps the sum on the x87 with its extended exponent, the"
      " rebuild rounds it to binary32, and the two part company once the sum"
      " leaves the binary32 normal range. The row is a value row: it"
      " compares U and V, six floats)",
      4, 6, "w[3] unit", "u[3] v[3]", GenVector3OrthonormalBasisf,
      Vector3GenerateOrthonormalBasisfRetail,
      Vector3GenerateOrthonormalBasisfOurs },
    { "Wm3::Vector3<double>::Normalize()", 0x00A3D0A0,
      "Foundation/Math/Wm3Vector3.inl:309 (the retail's double instantiation of"
      " the member Normalize: `this` in ECX, no stack arguments, plain RET, and"
      " the LENGTH in ST(0) at the ret - the reciprocal is consumed by the"
      " three multiplications, and the degenerate branch leaves 0.0. The body"
      " is x87 code and the engine runs under _PC_24, so the length, the"
      " inverse length and the three products are 24-bit significands with the"
      " extended exponent; the rebuild's double branch carries its"
      " intermediates in Wm3Detail::X87, which runs the operations on the"
      " hardware's x87. The return value is the length and not the inverse"
      " length - the WM3.4 copy returns the length as well, so this is the"
      " source's own semantics. The row is a value row: it compares the"
      " normalised vector and the returned length, four doubles)",
      6, 8, "v[3]", "v[3] len", GenVector3Normalized,
      Vector3NormalizeDRetail, Vector3NormalizeDOurs },
    { "Wm3::IntrSegment3Capsule3<double>::Find()", 0x00A6C3C0,
      "Foundation/Intersection/Wm3IntrSegment3Capsule3.cpp:56 (the double"
      " instantiation: the class vtable 0x00D720AC's +0x0C slot, ECX ="
      " `this`, no stack arguments, the bool in AL. The body calls the"
      " static Find (0x00A80080), whose double branch is the"
      " transcription in Foundation/Intersection/Wm3IntrLine3Capsule3.cpp,"
      " keeps the roots the query segment's extent admits and computes"
      " their points. The retail's body is x87 code and the engine runs"
      " under _PC_24, so every product and every sum is a 24-bit"
      " significand with the extended exponent; the rebuild's double"
      " branch carries its intermediates in Wm3Detail::X87, which runs the"
      " operations on the hardware's x87. The row compares the hit, the"
      " intersection type, the quantity and the two points; the parameters"
      " are left out because the retail's array holds every root the"
      " static Find produced and the rebuild's holds the roots the extent"
      " filter kept)",
      30, 15, "segment[7] capsule[8]",
      "hit type quantity point0[3] point1[3]",
      GenIntrSegment3Capsule3DoubleFind,
      IntrSegment3Capsule3dFindRetail, IntrSegment3Capsule3dFindOurs },
    { "Wm3::IntrSegment3Sphere3<double>::Find(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)", 0x00A47690,
      "Foundation/Intersection/Wm3IntrSegment3Sphere3.cpp:443 (the double"
      " instantiation: the class vtable 0x00D71E24's +0x0C slot - the"
      " *dynamic* Find, Find(fTMax, velocity0, velocity1) - ECX = `this`,"
      " three stack arguments (the time limit, a 64-bit double, and the two"
      " velocity pointers) and `RET 0x10`, so the callee pops all sixteen"
      " bytes, the bool in AL. The body asks the class's own static Find"
      " (0x00A479B0) first, then builds the path the sphere's centre travels"
      " along - the direction is the difference of the two velocities, the"
      " origin is the centre plus half of that difference, the direction is"
      " normalised in place and the extent is half the length - and meets"
      " the segment grown by the sphere's radius through"
      " IntrSegment3Capsule3<double> (0x00A6C3C0). The retail's body is"
      " x87 code and the engine runs under _PC_24, so every product and"
      " every sum is a 24-bit significand with the extended exponent; the"
      " rebuild's double branch carries its intermediates in"
      " Wm3Detail::X87Asm, which runs the operations on the hardware's x87."
      " The row compares the hit, the intersection type, the contact time"
      " and the contact point; the point is only written on the hit path,"
      " so both sides write zero for it when there is no hit)",
      36, 10, "segment[7] sphere[4] tmax vel0[3] vel1[3]",
      "hit type time point0[3]",
      GenIntrSegment3Sphere3DoubleDyn,
      IntrSegment3Sphere3dFindDynamicRetail,
      IntrSegment3Sphere3dFindDynamicOurs },
    { "Wm3::Eigen<float>::IncrSortEigenStuff3()", 0x00A70DB0,
      "Foundation/Numerics/Wm3Eigen.cpp:613 (retail thiscall, ECX ="
      " `this`, no stack arguments, void. The body is Tridiagonal3"
      " (0x00A6D480), QLAlgorithm (0x00A6E7C0), IncreasingSort (0x00A6ED00)"
      " and the GuaranteeRotation inlined. The retail's Eigen<float> is"
      " reached from the retail's GaussPointsFit3<float> (0x00A509A0),"
      " which is what the engine's ContBox3 uses. The row builds the"
      " Eigen<float> through the retail's own constructor (0x00A719E0),"
      " fills the matrix through the retail's operator() (0x00A6D2D0),"
      " runs the solver and compares the three eigenvalues and the nine"
      " entries of the eigenvector matrix. The retail's body is x87 code,"
      " so its arithmetic is a 24-bit significand with the extended"
      " exponent; the generator stays inside the float range the engine's"
      " covariance matrices live in, where that is a float's own"
      " arithmetic. The mutation check - the rebuild's Tridiagonal3 put"
      " back on the ZERO_TOLERANCE test of the WM4 form - fails"
      " 23640/200000 (seed 0x5EED2007, 200000 draws); with the"
      " transcription in place the row passes 200000/200000 and"
      " 1000000/1000000 on all five seeds)",
      9, 12, "m[9]", "diag[3] mat[9]", GenEigen3,
      EigenfIncrSort3Retail, EigenfIncrSort3Ours },
    { "Wm3::Eigen<float>::IncrSortEigenStuff2()", 0x00A70D70,
      "Foundation/Numerics/Wm3Eigen.cpp:604 (retail thiscall, ECX ="
      " `this`, no stack arguments, void. The body is Tridiagonal2"
      " (0x00A6D420), QLAlgorithm, IncreasingSort and the GuaranteeRotation"
      " inlined; the retail's GaussPointsFit2<float> reaches it the same way"
      " GaussPointsFit3 reaches the three-by-three one. The row compares"
      " the two eigenvalues and the four entries of the eigenvector"
      " matrix. The mutation check - the rebuild's IncreasingSort put back"
      " on the decreasing order - fails 142830/200000 (seed 0x5EED2007,"
      " 200000 draws); with the rebuild as it is the row passes"
      " 200000/200000 and 1000000/1000000 on all five seeds)",
      4, 6, "m[4]", "diag[2] mat[4]", GenEigen2,
      EigenfIncrSort2Retail, EigenfIncrSort2Ours },
    { "Wm3::Eigen<float>::GetEigenvalue(int) const", 0x00A6CE70,
      "Foundation/Numerics/Wm3Eigen.cpp:108 (retail thiscall, ECX ="
      " `this`, one stack argument - the index - and `RET 4`, so the callee"
      " pops it; the float comes back in ST(0), the body ends in FLD/RET)"
      " and never writes EAX. The row runs the retail's own solver first"
      " and then reads the three eigenvalues through the retail's"
      " accessor. The mutation check - the rebuild's Tridiagonal3 put back"
      " on the ZERO_TOLERANCE test - fails 12117/200000 (seed"
      " 0x5EED2007, 200000 draws); with the transcription in place the row"
      " passes 200000/200000 and 1000000/1000000 on all five seeds)",
      9, 3, "m[9]", "ret[3]", GenEigen3,
      EigenfGetEigenvaluesRetail, EigenfGetEigenvaluesOurs },
    { "Wm3::Eigen<float>::GetEigenvector(int, Wm3::Vector3<float>&) const", 0x00A6D350,
      "Foundation/Numerics/Wm3Eigen.cpp:137 (retail thiscall, ECX ="
      " `this`, two stack arguments - the index and the address of the"
      " Vector3 - and `RET 8`, void. The body compares m_iSize with three"
      " and copies the column out of the matrix, or writes the zero vector"
      " when the size is not three. The row runs the retail's own solver"
      " first and then reads the three columns through the retail's"
      " accessor. The mutation check - the rebuild's Tridiagonal3 put back"
      " on the ZERO_TOLERANCE test - fails 23744/200000 (seed"
      " 0x5EED2007, 200000 draws); with the transcription in place the row"
      " passes 200000/200000 and 1000000/1000000 on all five seeds)",
      9, 9, "m[9]", "vec[9]", GenEigen3,
      EigenfGetEigenvectorsRetail, EigenfGetEigenvectorsOurs },
    { "Wm3::Eigen<float>::GetEigenvector(int, Wm3::Vector2<float>&) const", 0x00A6D3A0,
      "Foundation/Numerics/Wm3Eigen.cpp:120 (retail thiscall, ECX ="
      " `this`, two stack arguments - the index and the address of the"
      " Vector2 - and `RET 8`, void. The body compares m_iSize with two"
      " and copies the column out of the matrix, or writes the zero vector"
      " when the size is not two. This is the accessor the retail's"
      " GaussPointsFit2<float> uses, the path the engine's ContBox2"
      " reaches. The row runs the retail's own two-by-two solver first"
      " and then reads the two columns through the retail's accessor. The"
      " mutation check - the rebuild's IncreasingSort put back on the"
      " decreasing order - fails 143236/200000 (seed 0x5EED2007, 200000"
      " draws); with the rebuild as it is the row passes 200000/200000"
      " and 1000000/1000000 on all five seeds)",
      4, 4, "m[4]", "vec[4]", GenEigen2,
      EigenfGetEigenvectors2Retail, EigenfGetEigenvectors2Ours },
    { "Wm3::Eigen<double>::IncrSortEigenStuff3()", 0x00A71460,
      "Foundation/Numerics/Wm3Eigen.cpp:819 (the double instantiation:"
      " retail thiscall, ECX = `this`, no stack arguments, void. The body"
      " is Tridiagonal3 (0x00A6EF70), QLAlgorithm (0x00A6FF60),"
      " IncreasingSort (0x00A70370) and the GuaranteeRotation inlined. The"
      " retail's double body is x87 code and the engine runs under _PC_24,"
      " so every product, every sum, every division and the square root"
      " carry a 24-bit significand with the extended exponent - a"
      " binary64 expression cannot reproduce that, so the rebuild's double"
      " branch carries its intermediates in Wm3Detail::X87Asm and takes"
      " its square root from Wm3Detail::X87Sqrt (the CRT's routine at"
      " 0x00A91000 re-sets the control word to (CW & 0x300) | 0x7F, which"
      " under _PC_24 leaves the precision control at 24 bits). The row"
      " builds the Eigen<double> through the retail's own constructor"
      " (0x00A71DC0), fills the matrix through the retail's operator()"
      " (0x00A6EDC0), runs the solver and compares the three eigenvalues"
      " and the nine entries of the eigenvector matrix. The mutation"
      " check - the double branch put back on the plain binary64 source -"
      " fails 114495/200000 (seed 0x5EED2007, 200000 draws); with the"
      " transcription in place the row passes 200000/200000 and"
      " 1000000/1000000 on all five seeds)",
      18, 24, "m[9]", "diag[3] mat[9]", GenEigen3d,
      EigendIncrSort3Retail, EigendIncrSort3Ours },
    { "Wm3::Eigen<double>::IncrSortEigenStuff2()", 0x00A71420,
      "Foundation/Numerics/Wm3Eigen.cpp:810 (the double instantiation:"
      " retail thiscall, ECX = `this`, no stack arguments, void. The body"
      " is Tridiagonal2 (0x00A6EF10), QLAlgorithm, IncreasingSort and the"
      " GuaranteeRotation inlined; the retail's GaussPointsFit2<double>"
      " reaches it the same way GaussPointsFit3 reaches the three-by-three"
      " one. The row compares the two eigenvalues and the four entries of"
      " the eigenvector matrix. The mutation check - the double branch put"
      " back on the plain binary64 source - fails 114434/200000 (seed"
      " 0x5EED2007, 200000 draws); with the transcription in place the row"
      " passes 200000/200000 and 1000000/1000000 on all five seeds)",
      8, 12, "m[4]", "diag[2] mat[4]", GenEigen2d,
      EigendIncrSort2Retail, EigendIncrSort2Ours },
    { "Wm3::Eigen<double>::GetEigenvalue(int) const", 0x00A6CEA0,
      "Foundation/Numerics/Wm3Eigen.cpp:295 (the double instantiation:"
      " retail thiscall, ECX = `this`, one stack argument - the index -"
      " and `RET 4`, so the callee pops it; the double comes back in"
      " ST(0), the body ends in FLD/RET) and never writes EAX. The row"
      " runs the retail's own solver first and then reads the three"
      " eigenvalues through the retail's accessor. The mutation check -"
      " the double branch put back on the plain binary64 source - fails"
      " 114544/200000 (seed 0x5EED2007, 200000 draws); with the"
      " transcription in place the row passes 200000/200000 and"
      " 1000000/1000000 on all five seeds)",
      18, 6, "m[9]", "ret[3]", GenEigen3d,
      EigendGetEigenvaluesRetail, EigendGetEigenvaluesOurs },
    { "Wm3::Eigen<double>::GetEigenvector(int, Wm3::Vector3<double>&) const", 0x00A6EE40,
      "Foundation/Numerics/Wm3Eigen.cpp:324 (the double instantiation:"
      " retail thiscall, ECX = `this`, two stack arguments - the index and"
      " the address of the Vector3 - and `RET 8`, void. The body compares"
      " m_iSize with three and copies the column out of the matrix, or"
      " writes the zero vector when the size is not three. The row runs"
      " the retail's own solver first and then reads the three columns"
      " through the retail's accessor. The mutation check - the double"
      " branch put back on the plain binary64 source - fails 114290/200000"
      " (seed 0x5EED2007, 200000 draws); with the transcription in place"
      " the row passes 200000/200000 and 1000000/1000000 on all five"
      " seeds)",
      18, 18, "m[9]", "vec[9]", GenEigen3d,
      EigendGetEigenvectorsRetail, EigendGetEigenvectorsOurs },
    { "Wm3::Eigen<double>::GetEigenvector(int, Wm3::Vector2<double>&) const", 0x00A6EE90,
      "Foundation/Numerics/Wm3Eigen.cpp:307 (the double instantiation:"
      " retail thiscall, ECX = `this`, two stack arguments - the index and"
      " the address of the Vector2 - and `RET 8`, void. This is the"
      " accessor the retail's GaussPointsFit2<double> uses. The row runs"
      " the retail's own two-by-two solver first and then reads the two"
      " columns through the retail's accessor. The mutation check - the"
      " double branch put back on the plain binary64 source - fails"
      " 114177/200000 (seed 0x5EED2007, 200000 draws); with the"
      " transcription in place the row passes 200000/200000 and"
      " 1000000/1000000 on all five seeds)",
      8, 8, "m[4]", "vec[4]", GenEigen2d,
      EigendGetEigenvectors2Retail, EigendGetEigenvectors2Ours },
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

  const difffuzz::Target* Targets(int& count)
  {
    count = static_cast<int>(sizeof(kTargets) / sizeof(kTargets[0]));
    return kTargets;
  }

} // namespace wm3coll
