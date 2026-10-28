#include "engine_scope.h"

#include <cstring>

namespace wm3scope
{
  namespace
  {
    const char* const kDead[] = {
      "Wm3::Distance<double, Wm3::Vector2<double>>::Get(double, double, const Wm3::Vector2<double>&, const Wm3::Vector2<double>&)",
      "Wm3::Distance<double, Wm3::Vector2<double>>::GetDerivative(double, const Wm3::Vector2<double>&, const Wm3::Vector2<double>&)",
      "Wm3::Distance<double, Wm3::Vector2<double>>::GetDerivativeSquared(double, const Wm3::Vector2<double>&, const Wm3::Vector2<double>&)",
      "Wm3::Distance<double, Wm3::Vector2<double>>::GetSquared(double, double, const Wm3::Vector2<double>&, const Wm3::Vector2<double>&)",
      "Wm3::Distance<double, Wm3::Vector2<double>>::SetDifferenceStep(double)",
      "Wm3::Distance<double, Wm3::Vector3<double>>::Get(double, double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)",
      "Wm3::Distance<double, Wm3::Vector3<double>>::GetDerivative(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)",
      "Wm3::Distance<double, Wm3::Vector3<double>>::GetDerivativeSquared(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)",
      "Wm3::Distance<double, Wm3::Vector3<double>>::GetSquared(double, double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)",
      "Wm3::Distance<double, Wm3::Vector3<double>>::SetDifferenceStep(double)",
      "Wm3::DistLine3Segment3<double>::Get()",
      "Wm3::DistLine3Segment3<double>::Get(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)",
      "Wm3::DistLine3Segment3<double>::GetSquared()",
      "Wm3::DistLine3Segment3<double>::GetSquared(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)",
      "Wm3::DistSegment3Segment3<double>::Get()",
      "Wm3::DistSegment3Segment3<double>::Get(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)",
      "Wm3::DistSegment3Segment3<double>::GetSquared()",
      "Wm3::DistSegment3Segment3<double>::GetSquared(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)",
      "Wm3::DistVector2Box2<double>::Get()",
      "Wm3::DistVector2Box2<double>::Get(double, const Wm3::Vector2<double>&, const Wm3::Vector2<double>&)",
      "Wm3::DistVector2Box2<double>::GetSquared()",
      "Wm3::DistVector2Box2<double>::GetSquared(double, const Wm3::Vector2<double>&, const Wm3::Vector2<double>&)",
      "Wm3::DistVector3Box3<double>::Get()",
      "Wm3::DistVector3Box3<double>::Get(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)",
      "Wm3::DistVector3Box3<double>::GetSquared()",
      "Wm3::DistVector3Box3<double>::GetSquared(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)",
      "Wm3::DistVector3Segment3<double>::Get()",
      "Wm3::DistVector3Segment3<double>::Get(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)",
      "Wm3::DistVector3Segment3<double>::GetSquared()",
      "Wm3::DistVector3Segment3<double>::GetSquared(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)",
      "Wm3::Eigen<double>::GetEigenvalue(int) const",
      "Wm3::Eigen<double>::GetEigenvector(int, Wm3::Vector2<double>&) const",
      "Wm3::Eigen<double>::GetEigenvector(int, Wm3::Vector3<double>&) const",
      "Wm3::Eigen<double>::IncrSortEigenStuff2()",
      "Wm3::Eigen<double>::IncrSortEigenStuff3()",
      "Wm3::IntrBox2Circle2<double>::Test()",
      "Wm3::IntrBox3Sphere3<double>::Test()",
      "Wm3::IntrLine3Box3<double>::Find()",
      "Wm3::IntrLine3Box3<double>::Test()",
      "Wm3::IntrSegment3Box3<double>::Find()",
      "Wm3::IntrSegment3Box3<double>::Test()",
      "Wm3::IntrSegment3Capsule3<double>::Find()",
      "Wm3::IntrSegment3Sphere3<double>::Find()",
      "Wm3::IntrSegment3Sphere3<double>::Find(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)",
      "Wm3::IntrSegment3Sphere3<double>::Test()",
      "Wm3::Vector3<double>::GenerateOrthonormalBasis(Wm3::Vector3<double>&, Wm3::Vector3<double>&, Wm3::Vector3<double>&, bool)",
      "Wm3::Vector3<double>::Normalize()",
    };
  }  // namespace

  bool IsDeadInEngine(const char* name)
  {
    for (const char* const dead : kDead)
    {
      if (std::strcmp(dead, name) == 0)
      {
        return true;
      }
    }
    return false;
  }
}
