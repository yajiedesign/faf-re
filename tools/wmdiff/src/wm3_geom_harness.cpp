#include "wm3_geom_harness.h"

// Forwards to the real Wild Magic 3.8 sources - no copies. Same float-relevant
// switches as main.vcxproj uses for Win32.

#include "Wm3AxisAlignedBox3.h"
#include "Wm3Box3.h"
#include "Wm3Vector3.h"

namespace wm3geomharness
{
  namespace
  {
    Wm3::Vector3f Load3(const float* p) noexcept
    {
      return Wm3::Vector3f(p[0], p[1], p[2]);
    }

    void Store24(const Wm3::Vector3f* v, float* out) noexcept
    {
      for (int i = 0; i < 8; ++i) {
        out[i * 3 + 0] = v[i].x;
        out[i * 3 + 1] = v[i].y;
        out[i * 3 + 2] = v[i].z;
      }
    }
  } // namespace

  u32 Aabb3TestIntersection(const float* a, const float* b)
  {
    const Wm3::AxisAlignedBox3f self(Load3(a), Load3(a + 3));
    const Wm3::AxisAlignedBox3f other(Load3(b), Load3(b + 3));
    return self.TestIntersection(other) ? 1u : 0u;
  }

  void Box3ComputeVertices(const float* self, float* out24)
  {
    // Axis is an array of three Vector3 laid out contiguously, which is exactly
    // the tail of the generated block, so the constructor takes it as-is.
    const Wm3::Box3f box(Load3(self),
                         reinterpret_cast<const Wm3::Vector3f*>(self + 3),
                         self + 12);
    Wm3::Vector3f vertices[8];
    box.ComputeVertices(vertices);
    Store24(vertices, out24);
  }
} // namespace wm3geomharness
