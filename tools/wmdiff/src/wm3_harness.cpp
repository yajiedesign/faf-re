#include "wm3_harness.h"

// Forwards to the real Wild Magic 3.8 sources - no copies. Build with the same
// float-relevant switches main.vcxproj uses for Win32 (/arch:SSE2 implied,
// /fp:precise, /O2 /Oi in release), so the code exercised here is the code the
// engine runs.
//
// Foundation/Math/Wm3Vector3.h is a self-contained template header; it needs
// Wm3FoundationLIB.h and Wm3Math.h, both under Foundation/Math or Foundation/.

#include "fuzz.h" // Bits / FromBits

#include "Wm3Vector2.h"
#include "Wm3Vector3.h"

namespace wm3harness
{
  namespace
  {
    // Load/store helpers so the thunks speak plain float lanes and never care
    // about Wm3's tuple layout.
    Wm3::Vector3<float> Load3(const float* p)
    {
      return Wm3::Vector3<float>(p[0], p[1], p[2]);
    }
    void Store3(const Wm3::Vector3<float>& v, float* p)
    {
      p[0] = v.x;
      p[1] = v.y;
      p[2] = v.z;
    }

    Wm3::Vector2<float> Load2(const float* p)
    {
      return Wm3::Vector2<float>(p[0], p[1]);
    }
    void Store2(const Wm3::Vector2<float>& v, float* p)
    {
      p[0] = v.x;
      p[1] = v.y;
    }
  } // namespace

  // ---- Wm3::Vector3<float> -------------------------------------------------

  u32 Dot(const float* a, const float* b)
  {
    return difffuzz::Bits(Wm3::Vector3<float>::Dot(Load3(a), Load3(b)));
  }

  void Cross(const float* a, const float* b, float* out)
  {
    Store3(Wm3::Vector3<float>::Cross(Load3(a), Load3(b)), out);
  }

  u32 LengthSquared(const float* v)
  {
    const Wm3::Vector3<float> t = Load3(v);
    return difffuzz::Bits(Wm3::Vector3<float>::LengthSq(t));
  }

  u32 InstanceDot(const float* a, const float* b)
  {
    return difffuzz::Bits(Load3(a).Dot(Load3(b)));
  }

  u32 InstanceSquaredLength(const float* v)
  {
    return difffuzz::Bits(Load3(v).SquaredLength());
  }

  void InstanceNormalize(const float* v, float* out)
  {
    Wm3::Vector3<float> t = Load3(v);
    (void)t.Normalize();
    Store3(t, out);
  }

  float InstanceNormalizeRet(const float* v, float* out)
  {
    Wm3::Vector3<float> t = Load3(v);
    const float fLength = t.Normalize();
    Store3(t, out);
    return fLength;
  }

  void InstanceCross(const float* a, const float* b, float* out)
  {
    Store3(Load3(a).Cross(Load3(b)), out);
  }

  void Normalize(float* v)
  {
    Wm3::Vector3<float> t = Load3(v);
    // eps is pinned to 1e-6f rather than taken from the input, because the
    // retail build bakes the threshold in as a .rdata constant (0x00d71bfc)
    // instead of taking it as an argument. Feeding a random eps here would
    // report a divergence that is only a signature difference.
    (void)Wm3::Vector3<float>::Normalize(t, 1e-6f);
    Store3(t, v);
  }

  u32 SqrtfBinary(float x)
  {
    return difffuzz::Bits(Wm3::SqrtfBinary(x));
  }

  // ---- Wm3::Vector2<float> -------------------------------------------------

} // namespace wm3harness
