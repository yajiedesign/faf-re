# `double` 可达性审计 —— wmdiff 的 47 个 `double` 失败比对项

**日期**：2026-10-28
**范围**：`tools/wmdiff` 比对表里 **47 个 `[FAIL]` 且是 `double` 的目标**，逐个判定
"引擎（`src/`）会不会真的执行它"。
**结论一句话**：**47 个全部是 `dead`** —— 没有一个 `src/` 调用点把 `double`（或
`Vector3d` / `Vector2d` / `double` 表达式）传给 Wild Magic 模板；`src/` 只实例化了
**float** 一侧。

本轮是**只读审计**：没有改 harness、没有改 Wild Magic 树、没有改目标表；唯一的写入是
本文件（以及 `.tmp/dbl/` 下的草稿输出）。

---

## 1. 失败清单是怎么来的

```
./build/clang-x86-release/tools/wmdiff/wmdiff.exe --iters 200000 --seed 0x5EED2007 --max-mismatch 0
```

（`build/clang-x86-release/tools/wmdiff/wmdiff.exe` 已是最新构建，未重建。）

```bash
grep '^\[FAIL\]' run1.txt | grep -i 'double' | wc -l   # -> 47
```

同一份输出里总共有 81 个 `[FAIL]`（156 项中 75 过 81 败），其中 **47 个是 `double`**，
与任务说明一致。清单按零售地址排序后的完整表在 §3。

---

## 2. 判定规则（沿用本仓库已经定下的三条）

1. **模板实例化由调用处的实参静态类型决定** —— 不是"类名里有没有 `double`"，
   也不是"库里有没有显式实例化"，也不是链接探针。
2. **调用点若在 `Foundation/**` 里，要继续往上追**，直到落到 `src/`，或者把链走完
   （链断在半路 ⇒ `dead`）。
3. **链接探针只能筛，不能结案** —— 它分不清"没人用"和"人人都在用"
   （头文件内联模板由使用者实例化）。所以本文件的结论**只用调用点证据**，
   链接探针/RTTI 只作旁证。

---

## 3. 结论表（47 项）

| # | target（名字 + 零售地址） | 引擎执行？ | 证据 | 判定 |
|---|---|---|---|---|
| **A. `Distance<double, Wm3::Vector3<double>>`（5）** | | | | |
| 1 | `Distance<double,Vector3<double>>::SetDifferenceStep(double)` `0x00A39DB0` | 否 | `grep -rn "Wm3::Distance\|Distance<" src/` → **0 命中**；`grep -rn "Distance2d\|Distance3d" src/` → 0。唯一实例化点是 `dependencies/WildMagic3p8/Foundation/Distance/Wm3Distance.cpp:357`（`template class Distance<double,Vector3d>;`）；它的成员只能经一个 `Distance3d` 对象到达，而树上唯一的 `Distance3d` 派生类就是下面 C–G 那几个 `Dist*<double>`，全部 `dead` | `dead` |
| 2 | `Distance<double,Vector3<double>>::Get(double,double,const Vector3<double>&,const Vector3<double>&)` `0x00A39860` | 否 | 同上；`Distance3d` 无 `src/` 侧持有者 | `dead` |
| 3 | `Distance<double,Vector3<double>>::GetSquared(double,double,…)` `0x00A39AB0` | 否 | 同上 | `dead` |
| 4 | `Distance<double,Vector3<double>>::GetDerivative(double,…)` `0x00A39D00` | 否 | 同上 | `dead` |
| 5 | `Distance<double,Vector3<double>>::GetDerivativeSquared(double,…)` `0x00A39D60` | 否 | 同上 | `dead` |
| **B. `Distance<double, Wm3::Vector2<double>>`（5）** | | | | |
| 6 | `Distance<double,Vector2<double>>::SetDifferenceStep(double)` `0x00A397C0` | 否 | 同 A：`grep -rn "Distance2d\|Distance3d\|Wm3::Distance" src/` → 0 命中；实例化点 `Wm3Distance.cpp:356` | `dead` |
| 7 | `Distance<double,Vector2<double>>::Get(double,double,…)` `0x00A39270` | 否 | 同上 | `dead` |
| 8 | `Distance<double,Vector2<double>>::GetSquared(double,double,…)` `0x00A394C0` | 否 | 同上 | `dead` |
| 9 | `Distance<double,Vector2<double>>::GetDerivative(double,…)` `0x00A39710` | 否 | 同上 | `dead` |
| 10 | `Distance<double,Vector2<double>>::GetDerivativeSquared(double,…)` `0x00A39770` | 否 | 同上 | `dead` |
| **C. `DistVector3Box3<double>`（4）** | | | | |
| 11 | `DistVector3Box3<double>::GetSquared()` `0x00A460D0` | 否 | `src/` 只有 float：`src/moho/collision/CColPrimitiveBase.cpp:429` `Wm3::DistVector3Box3f distance(sphere->Center, mShape);`（`mShape` 是 `Wm3::Box3f`，`CColPrimitiveBase.h:344` ⇒ 实参 `Vector3f`/`Box3f` ⇒ 只实例化 float）。树内唯一的其它调用者是 `Foundation/Distance/Wm3DistRay3Box3.cpp:62` 与 `Wm3DistSegment3Box3.cpp:65,73`（`DistRay3Box3<Real>` / `DistSegment3Box3<Real>`），而 `grep -rn "DistRay3Box3\|DistSegment3Box3" src/` → **0 命中** ⇒ 这两个 `<double>` 实例化无人调用 | `dead` |
| 12 | `DistVector3Box3<double>::Get()` `0x00A45800` | 否 | 同 11 | `dead` |
| 13 | `DistVector3Box3<double>::GetSquared(double,const Vector3<double>&,const Vector3<double>&)` `0x00A45FB0` | 否 | 同 11 | `dead` |
| 14 | `DistVector3Box3<double>::Get(double,const Vector3<double>&,const Vector3<double>&)` `0x00A45E90` | 否 | 同 11 | `dead` |
| **D. `DistVector3Segment3<double>`（4）** | | | | |
| 15 | `DistVector3Segment3<double>::GetSquared()` `0x00A48910` | 否 | `src/` 只有 float：`src/moho/projectile/Projectile.cpp:1552` `Wm3::DistVector3Segment3f targetDistance(target->mVarDat.mCurTransform.pos_, segment);`、`src/moho/ui/CUIWorldView.cpp:276` `Wm3::DistVector3Segment3f pointDistance(points[index], span);`（实参 `Vector3f`/`Segment3f`）。树内其它调用者：`Foundation/Containment/Wm3ContCapsule3.cpp:102,114`（`ContCapsule3<Real>`），`grep -rn "ContCapsule3" src/` → 0 命中 | `dead` |
| 16 | `DistVector3Segment3<double>::Get()` `0x00A48100` | 否 | 同 15 | `dead` |
| 17 | `DistVector3Segment3<double>::GetSquared(double,…)` `0x00A487E0` | 否 | 同 15 | `dead` |
| 18 | `DistVector3Segment3<double>::Get(double,…)` `0x00A486B0` | 否 | 同 15 | `dead` |
| **E. `DistVector2Box2<double>`（4）** | | | | |
| 19 | `DistVector2Box2<double>::GetSquared()` `0x00A6CD70` | 否 | `grep -rn "DistVector2Box2" src/` → **1 命中，且是注释**（`src/moho/render/VisionShader.h:46` 描述零售用法）⇒ `src/` 根本没有 float 调用点。树内唯一调用者 `Foundation/Intersection/Wm3IntrBox2Circle2.cpp:42`（`IntrBox2Circle2<Real>`），而该类的 double 实例化本身无调用者（见 H） | `dead` |
| 20 | `DistVector2Box2<double>::Get()` `0x00A6C6A0` | 否 | 同 19 | `dead` |
| 21 | `DistVector2Box2<double>::GetSquared(double,…)` `0x00A6CC60` | 否 | 同 19 | `dead` |
| 22 | `DistVector2Box2<double>::Get(double,…)` `0x00A6CB50` | 否 | 同 19 | `dead` |
| **F. `DistLine3Segment3<double>`（4）** | | | | |
| 23 | `DistLine3Segment3<double>::GetSquared()` `0x00A81330` | 否 | `grep -rn "DistLine3Segment3" src/` → **0 命中**（float 也没有）。树内唯一调用者 `Foundation/Distance/Wm3DistLine3Rectangle3.cpp:61`（`DistLine3Rectangle3<Real>`），`grep -rn "DistLine3Rectangle3" src/` → 0 命中 ⇒ 该 `<double>` 实例化无人调用 | `dead` |
| 24 | `DistLine3Segment3<double>::Get()` `0x00A809C0` | 否 | 同 23 | `dead` |
| 25 | `DistLine3Segment3<double>::GetSquared(double,…)` `0x00A811F0` | 否 | 同 23 | `dead` |
| 26 | `DistLine3Segment3<double>::Get(double,…)` `0x00A810B0` | 否 | 同 23 | `dead` |
| **G. `DistSegment3Segment3<double>`（4）** | | | | |
| 27 | `DistSegment3Segment3<double>::GetSquared()` `0x00A7F100` | 否 | `grep -rn "DistSegment3Segment3" src/` → **0 命中**（连 float 都没有）。树内唯一调用者 `Foundation/Distance/Wm3DistSegment3Rectangle3.cpp:62`（`DistSegment3Rectangle3<Real>`），`grep -rn "DistSegment3Rectangle3" src/` → 0 命中 | `dead` |
| 28 | `DistSegment3Segment3<double>::Get()` `0x00A7E250` | 否 | 同 27 | `dead` |
| 29 | `DistSegment3Segment3<double>::GetSquared(double,…)` `0x00A7EFB0` | 否 | 同 27 | `dead` |
| 30 | `DistSegment3Segment3<double>::Get(double,…)` `0x00A7EE60` | 否 | 同 27 | `dead` |
| **H. `IntrBox2Circle2<double>`（1）** | | | | |
| 31 | `IntrBox2Circle2<double>::Test()` `0x00A4EE50` | 否 | **这个类是执行的 —— 但只有 float 实例化**：`src/moho/vision/VisionDB.cpp:88` `Wm3::IntrBox2Circle2f overlap{box, sampled};`、`src/moho/render/VisionRenderer.cpp:312` `Wm3::IntrBox2Circle2f overlap{box, sampled};` ⇒ 实参是 `Wm3::Box2f` / `Wm3::Circle2f` ⇒ 只实例化 `IntrBox2Circle2<float>`。`grep -rn "IntrBox2Circle2<double>\|IntrBox2Circle2d" src/` → 0。树内除自身 cpp 外**无任何使用者**（`grep -rn "IntrBox2Circle2<" Foundation/ \| grep -v Wm3IntrBox2Circle2.cpp` → 无命中） | `dead` |
| **I. `IntrBox3Sphere3<double>`（1）** | | | | |
| 32 | `IntrBox3Sphere3<double>::Test()` `0x00A425C0` | 否 | `src/` 只有 float：`src/moho/collision/CColPrimitiveBase.cpp:607` `Wm3::IntrBox3Sphere3f intersection(*box, mShape);`（`mShape` 是 `Wm3::Sphere3f`，`CColPrimitiveBase.h:441`）。树内除自身 cpp 外**无任何使用者**（`grep -rn "IntrBox3Sphere3<" Foundation/ \| grep -v Wm3IntrBox3Sphere3.cpp` → 无命中） | `dead` |
| **J. `IntrLine3Box3<double>`（2）** | | | | |
| 33 | `IntrLine3Box3<double>::Test()` `0x00A50090` | 否 | `src/` 只有 float：`src/moho/ui/UiRuntimeTypes.cpp:21007` `Wm3::IntrLine3Box3f intersector(wmPickRay, mesh->box);`。树内使用者只有 `Foundation/Intersection/Wm3IntrRay3Box3.cpp:102` 与 `Wm3IntrSegment3Box3.cpp:101`（静态 `DoClipping`），二者所属类 `IntrRay3Box3` / `IntrSegment3Box3` 在 `src/` 里 0 命中 ⇒ double 侧无根 | `dead` |
| 34 | `IntrLine3Box3<double>::Find()` `0x00A50940` | 否 | 同 33 | `dead` |
| **K. `IntrSegment3Box3<double>`（2）** | | | | |
| 35 | `IntrSegment3Box3<double>::Test()` `0x00A46720` | 否 | `src/` 只有 float：`src/moho/collision/CColPrimitiveBase.cpp:384` `Wm3::IntrSegment3Box3f intersection(segment, mShape, false);`。树内除自身 cpp 外**无任何使用者**（`grep -rn "IntrSegment3Box3<" Foundation/ \| grep -v Wm3IntrSegment3Box3.cpp` → 无命中） | `dead` |
| 36 | `IntrSegment3Box3<double>::Find()` `0x00A46330` | 否 | 同 35 | `dead` |
| **L. `IntrSegment3Sphere3<double>`（3）** | | | | |
| 37 | `IntrSegment3Sphere3<double>::Test()` `0x00A475E0` | 否 | `src/` 只有 float：`src/moho/collision/CColPrimitiveBase.cpp:577` `Wm3::IntrSegment3Sphere3f intersection(segment, mShape);`。树内使用者只有 `Foundation/Intersection/Wm3IntrSegment3Sphere3.cpp:146,178`（→ `IntrSegment3Capsule3<Real>`），两边都在 double 孤岛里 | `dead` |
| 38 | `IntrSegment3Sphere3<double>::Find()` `0x00A479B0` | 否 | 同 37 | `dead` |
| 39 | `IntrSegment3Sphere3<double>::Find(double,const Vector3<double>&,const Vector3<double>&)` `0x00A47690` | 否 | 同 37 | `dead` |
| **M. `IntrSegment3Capsule3<double>`（1）** | | | | |
| 40 | `IntrSegment3Capsule3<double>::Find()` `0x00A6C3C0` | 否 | `grep -rn "IntrSegment3Capsule3" src/` → **0 命中**（float 也没有）。树内唯一使用者 `Foundation/Intersection/Wm3IntrSegment3Sphere3.cpp:146,178` —— 而 `IntrSegment3Sphere3<double>` 本身只被自己的显式实例化引用（见 L）⇒ 链断在 L ⇒ `dead` | `dead` |
| **N. `Vector3<double>`（2）** | | | | |
| 41 | `Vector3<double>::GenerateOrthonormalBasis(Vector3<double>&,Vector3<double>&,Vector3<double>&,bool)` `0x00A3D150` | 否 | `grep -rn "Vector3d\|Vector3<double>" src/` → **0 命中**。`Foundation/**` 里 `Vector3<Real>::GenerateOrthonormalBasis` 的调用点全在 double 孤岛内：`Foundation/Containment/Wm3ContBox3.cpp:292,307,369`（`ContMinBox<Real>` —— 其唯一调用者是**它自己的递归** `Wm3ContBox3.cpp:319,402`）、`Wm3ContCapsule3.cpp:52`、`Foundation/Intersection/Wm3IntrLine3Capsule3.cpp:99`、`Wm3IntrLine3Cylinder3.cpp:89`、`Wm3IntrPlane3Sphere3.cpp:67`、`Wm3IntrSphere3Sphere3.cpp:80`、`Foundation/Meshes/Wm3MeshCurvature.cpp:146`、`Foundation/Surfaces/Wm3ImplicitSurface.cpp:63` —— 对应的类在 `src/` 里全部 0 命中（`grep -rn "ContCapsule3\|IntrLine3Capsule3\|IntrLine3Cylinder3\|IntrPlane3Sphere3\|IntrSphere3Sphere3\|MeshCurvature\|ImplicitSurface" src/` → 0） | `dead` |
| 42 | `Vector3<double>::Normalize()` `0x00A3D0A0` | 否 | 同 41：`Vector3<Real>::Normalize` 的 `Foundation/**` 调用点（`Wm3ContBox3.cpp:288,430,436,447`、`Wm3ContBox2.cpp:269,297,301`、`Wm3ContCapsule3.cpp:171`、`Wm3ContEllipse2.cpp:122,127`、`Wm3ContScribeCircle3Sphere3.cpp:57,164-176`、`Wm3ContSeparatePoints2.cpp:50,82`、`Wm3ApprCylinderFit3.cpp:127,184,215`、`Foundation/Curves/*`、`Wm3PointController.cpp:129`）所属的类在 `src/` 里全部 0 命中；`src/` 侧的 float 调用点（`src/moho/entity/Entity.cpp:7664`、`src/moho/math/QuaternionMath.cpp:594` 等）给的实参是 `Wm3::Vector3f` ⇒ 只实例化 float | `dead` |
| **O. `Eigen<double>`（5）** | | | | |
| 43 | `Eigen<double>::IncrSortEigenStuff3()` `0x00A71460` | 否 | `grep -rn "Eigen" src/` → **0 命中**（连头文件都不 include）。树内 `Eigen<Real>` 的使用者：`Foundation/Approximation/Wm3ApprGaussPointsFit2.cpp:92`、`Wm3ApprGaussPointsFit3.cpp:103`、`Wm3ApprLineFit2.cpp:165`、`Wm3ApprLineFit3.cpp:98`、`Wm3ApprPlaneFit3.cpp:189`、`Wm3ApprQuadraticFit2.cpp:22,106`、`Wm3ApprQuadraticFit3.cpp:22,161`、`Foundation/Distance/Wm3DistVector2Quadratic2.cpp:49`、`Wm3DistVector3Quadratic3.cpp:49`、`Foundation/Math/Wm3Ellipse2.inl:140`。其中 `GaussPointsFit2/3<Real>` 的调用者是 `Foundation/Containment/Wm3ContBox2.cpp:109`（`ContOrientedBox<Real>`）、`Wm3ContBox3.cpp:133`（`ContOrientedBox<Real>`）、`Wm3ContEllipse2.cpp:25`、`Wm3ContEllipsoid3.cpp:26`、`Wm3ContLozenge3.cpp:27` —— `grep -rn "ContOrientedBox\|ContEllipse\|ContEllipsoid\|ContLozenge\|GaussPointsFit\|CircleFit\|EllipseFit\|EllipsoidFit\|SphereFit" src/` → **0 命中**；`src/` 唯一那个 containment 调用是 `src/moho/math/Wm3AxisAlignedBox3FafExtras.cpp:26` `Wm3::Box3f aligned = Wm3::ContAlignedBox(8, vertices);`，而 `vertices` 是 `Wm3::Vector3f vertices[8]`（同文件 `:23`）⇒ **只实例化 float**；并且 `ContAlignedBox` 自己算 min/max，**不**调 `GaussPointsFit3`（那是 `ContOrientedBox`，`Wm3ContBox3.cpp:133`）⇒ 链在同一文件里就分开了 | `dead` |
| 44 | `Eigen<double>::IncrSortEigenStuff2()` `0x00A71420` | 否 | 同 43 | `dead` |
| 45 | `Eigen<double>::GetEigenvalue(int) const` `0x00A6CEA0` | 否 | 同 43 | `dead` |
| 46 | `Eigen<double>::GetEigenvector(int,Vector3<double>&) const` `0x00A6EE40` | 否 | 同 43 | `dead` |
| 47 | `Eigen<double>::GetEigenvector(int,Vector2<double>&) const` `0x00A6EE90` | 否 | 同 43 | `dead` |

### 每组计数

| 组 | 类 | 目标数 | `executed` | `dead` | `unsettled` |
|---|---|---:|---:|---:|---:|
| A | `Distance<double,Vector3d>` | 5 | 0 | 5 | 0 |
| B | `Distance<double,Vector2d>` | 5 | 0 | 5 | 0 |
| C | `DistVector3Box3<double>` | 4 | 0 | 4 | 0 |
| D | `DistVector3Segment3<double>` | 4 | 0 | 4 | 0 |
| E | `DistVector2Box2<double>` | 4 | 0 | 4 | 0 |
| F | `DistLine3Segment3<double>` | 4 | 0 | 4 | 0 |
| G | `DistSegment3Segment3<double>` | 4 | 0 | 4 | 0 |
| H | `IntrBox2Circle2<double>` | 1 | 0 | 1 | 0 |
| I | `IntrBox3Sphere3<double>` | 1 | 0 | 1 | 0 |
| J | `IntrLine3Box3<double>` | 2 | 0 | 2 | 0 |
| K | `IntrSegment3Box3<double>` | 2 | 0 | 2 | 0 |
| L | `IntrSegment3Sphere3<double>` | 3 | 0 | 3 | 0 |
| M | `IntrSegment3Capsule3<double>` | 1 | 0 | 1 | 0 |
| N | `Vector3<double>` | 2 | 0 | 2 | 0 |
| O | `Eigen<double>` | 5 | 0 | 5 | 0 |
| | **合计** | **47** | **0** | **47** | **0** |

---

## 4. "没有调用点"是搜出来的，不是猜的

这一节把 §3 里用到的**全部**搜索列出来，方便复核。

### 4.1 `src/` 里有没有 `double` 类型的 Wild Magic 实参

```bash
grep -rn "Vector3d\|Vector2d" src/                       # -> 0 命中
grep -rn "Vector3<double>\|Vector2<double>\|<double>" src/ # -> 只有 gpg/gal 与 gpg/core
                                                            #    的非 Wild Magic 代码（计时、profiler、libpng）
grep -rn "\bdouble\b" src/ | grep -i "wm3"              # -> 0 命中（没有一行同时出现
                                                            #    double 和 Wm3）
```

⇒ **`src/` 里没有一处把 `double` 传给 Wild Magic 模板。**

### 4.2 `src/` 里 `Wm3::` 的全部名字（按出现次数）

```
2542 Wm3::Vector3f     1208 Wm3::Vec3f      364 Wm3::Quaternionf   302 Wm3::Vector2f
 283 Wm3::Vector3      220 Wm3::AxisAlignedBox3f  158 Wm3::Box3f   140 Wm3::Quatf
 113 Wm3::Sphere3f      82 Wm3::Vector2i     46 Wm3::Circle2f      41 Wm3::Quaternion
  36 Wm3::Vec2f         23 Wm3::Plane3f      22 Wm3::Vector2       17 Wm3::MultiplyQuaternionVector
  16 Wm3::Box3          16 Wm3::Box2f        12 Wm3::Sphere3        11 Wm3::Vec2i
  11 Wm3::Segment3f      9 Wm3::IVector2      8 Wm3::AxisAlignedBox3  7 Wm3::MakeSegment3fFromEndpoints
   6 Wm3::Vector3i       6 Wm3::SqrtfBinary   6 Wm3::Plane3          4 Wm3::IntrBox2Circle2f
   2 Wm3::System         2 Wm3::GeomLine3     2 Wm3::DistVector3Segment3f  2 Wm3::ConvexHull3（**注释**）
   1 Wm3::Vector3fIsntNaN  1 Wm3::Line3f     1 Wm3::IntrSegment3Sphere3f 1 Wm3::IntrSegment3Box3f
   1 Wm3::IntrLine3Box3f  1 Wm3::IntrBox3Sphere3f  1 Wm3::IntrBox2Box2f  1 Wm3::InBox
   1 Wm3::DistVector3Box3f  1 Wm3::DistVector2Box2f  1 Wm3::ContAlignedBox
```

* 所有带 `f` 的都是 float 别名；不带后缀的 `Wm3::Vector3` / `Wm3::Box3` / `Wm3::Sphere3` /
  `Wm3::Quaternion` / `Wm3::Plane3` 在 `src/` 里**全部**是 `<float>` 显式写法
  （例：`src/moho/ai/CAiAttackerImpl.cpp:2089` `const Wm3::Vector3f position = SCR_FromLuaCopy<Wm3::Vector3<float>>(argObject);`）
  —— `grep -rn "Wm3::Vector3[^fdi]" src/` 的 249 条命中逐条核对后**没有一条是 `<double>`**。
* `Wm3::ConvexHull3` 只出现在注释里：`src/legacy/containers/RbTree.h:4365-4368` 与 `:4701`
  （讲 `m_kHull.erase(pkTri)` 的所有权）—— **不是调用点**。
* `Wm3::ContAlignedBox` 唯一的调用点：`src/moho/math/Wm3AxisAlignedBox3FafExtras.cpp:26`，
  实参 `vertices` 的静态类型是 `Wm3::Vector3f[8]`（同文件 `:23`）⇒ **只实例化 float**。

### 4.3 `src/` 里对相关类的调用点（float 一侧，逐个列出）

```bash
grep -rn "DistVector3Segment3f" src/   # src/moho/projectile/Projectile.cpp:1552
                                     # src/moho/ui/CUIWorldView.cpp:276
grep -rn "DistVector3Box3f"   src/   # src/moho/collision/CColPrimitiveBase.cpp:429
grep -rn "DistVector2Box2f"   src/   # src/moho/render/VisionShader.h:46（**只有注释**）
grep -rn "DistLine3Segment3"  src/   # -> 0
grep -rn "DistSegment3Segment3" src/ # -> 0
grep -rn "IntrBox2Circle2f"   src/   # src/moho/vision/VisionDB.cpp:88
                                     # src/moho/render/VisionRenderer.cpp:312
grep -rn "IntrBox3Sphere3f"   src/   # src/moho/collision/CColPrimitiveBase.cpp:607
grep -rn "IntrLine3Box3f"     src/   # src/moho/ui/UiRuntimeTypes.cpp:21007
grep -rn "IntrSegment3Box3f"  src/   # src/moho/collision/CColPrimitiveBase.cpp:384
grep -rn "IntrSegment3Sphere3f" src/ # src/moho/collision/CColPrimitiveBase.cpp:577
grep -rn "IntrSegment3Capsule3" src/ # -> 0
grep -rn "Eigen"              src/   # -> 0
grep -rn "Wm3::Distance\|Distance<" src/  # -> 0
grep -rn "Distance2d\|Distance3d" src/    # -> 0
```

### 4.4 `src/` 引用的 Wild Magic 头文件（全部）

```
Wm3AxisAlignedBox3.h  Wm3AxisAlignedBox3FafExtras.h  Wm3Box2.h  Wm3Box3.h
Wm3Circle2.h  Wm3ContBox3.h  Wm3DistVector3Box3.h  Wm3DistVector3Segment3.h
Wm3IntrBox2Box2.h  Wm3IntrBox2Circle2.h  Wm3IntrBox3Sphere3.h  Wm3IntrLine3Box3.h
Wm3IntrSegment3Box3.h  Wm3IntrSegment3Sphere3.h  Wm3IVector2.h  Wm3Line3.h
Wm3Plane3.h  Wm3Quaternion.h  Wm3Segment3.h  Wm3Sphere3.h  Wm3Vector2.h  Wm3Vector3.h
```

⇒ **`Foundation/Approximation/`（`Eigen` 的上游）、`Foundation/Containment/` 的拟合/椭圆部分、
`Foundation/Distance/` 的 rectangle/ray 变体、`Foundation/Intersection/` 的
capsule/cylinder/ray/sphere 变体，一张都没被 `src/` 引用。**

### 4.5 `src/` 里的 `double` 都去哪儿了

`src/` 里确实有 `double`（666 处命中），但**全部**属于三类与 Wild Magic 无关的东西：

| 位置 | 用途 |
|---|---|
| `src/gpg/gal/**` | 计时、profiler、shader 参数（`std::optional<double>`）|
| `src/libpng/**` | libpng 的 gamma 参数 |
| `src/legacy/math/X87Math.*` | 引擎自己的 x87 数学（`fmod`/`pow`/`acos`/`asin`），**不是** Wild Magic |
| `src/moho/math/QuaternionMath.*` | 引擎自己的 double 四元数工具（`QuaternionToRotationMatrix3x3`）|
| `src/moho/math/MathReflection.*` | `MathGlobalRandomUnitScaled` 等返回 `double` 的引擎函数 |

⇒ 没有任何一个把 `double` 交给 `Wm3::` 的模板。

---

## 5. 旁证（只作参考，不用来结案）

**链接进 `main.exe` 的二进制里的 RTTI**（`build/clang-x86-release/src/main.exe`，已 strip）：

```
.?AV?$Distance@MV?$Vector3@M@Wm3@@@Wm3@@      <- Distance<float,Vector3f>
.?AV?$Distance@MV?$Vector2@M@Wm3@@@Wm3@@      <- Distance<float,Vector2f>
.?AV?$DistSegment3Segment3@M@Wm3@@   ?$DistVector2Box2@M@Wm3@@
.?AV?$DistVector3Box3@M@Wm3@@       ?$DistVector3Segment3@M@Wm3@@
.?AV?$IntrBox2Box2@M@Wm3@@          ?$IntrBox2Circle2@M@Wm3@@
.?AV?$IntrBox3Sphere3@M@Wm3@@       ?$IntrLine3Box3@M@Wm3@@
.?AV?$IntrSegment3Box3@M@Wm3@@      ?$IntrSegment3Capsule3@M@Wm3@@
.?AV?$IntrSegment3Sphere3@M@Wm3@@
```

* MSVC x86 模板实参编码：`M` = `float`，`N` = `double`。
* 上面**全部是 float**；带 `@H@Wm3@@`（`int`）的只有 `IVector2<int>` / `IVector3<int>`；
  **`@N@Wm3@@`（`double`）的 Wild Magic 类型描述符数量 = 0**（在 `main.exe` 里全文扫描：
  `re.finditer(rb'[ -~]{6,}', data)` 里凡含 `@N@Wm3@@` 的串，0 条）。
* 这些 `Distance`/`Dist*`/`Intr*` 都是**多态**类，其 double 实例化的类型描述符若被程序
  引用，就不会在 `/OPT:REF` 下被丢掉 —— 它们**不在** `main.exe` 里 ⇒ 程序里没有任何
  double 的 `Distance`/`Dist*`/`Intr*` 对象。
* 库里 `Eigen<double>` **确实**被编译进了静态库
  （`llvm-nm build/clang-x86-release/cmake/deps/wildmagic/Wm3Foundation.lib` 里能看到
  `??0?$Eigen@N@Wm3@@…`）⇒ 这就是老记录里"**linked-but-unreachable**"的字面含义：
  **在二进制里，但永不被调用**。
* ⚠ 反过来的坑：RTTI 的**存在**只说明"库被链进来了"，**不**说明"引擎用了它"
  （例如 `DistSegment3Segment3<float>` 的 RTTI 在 `main.exe` 里，但 `src/` 里
  `grep -rn "DistSegment3Segment3" src/` → 0 命中 —— 那是库自己的显式实例化带进来的）。
  所以本文件的判定仍然只用 §3 的调用点证据。

---

## 6. 收官

### 6.1 47 个里有几个需要修

| 判定 | 数量 | 含义 |
|---|---:|---|
| `executed` | **0** | 没有 `src/` 调用点以 `double` 实参到达 |
| `dead` | **47** | 链断在 `src/` 之前 ⇒ **不需要为它们修重建** |
| `unsettled` | **0** | —— |

⇒ **这 47 个 `double` 失败项全部是"死代码被拿来和零售比"**：
它们对应的 `double` 实例化在引擎里既不会被构造，也不会被调用，
所以它们的失配（失配率 20%–100%）**与引擎行为无关**，
修它们"改不了引擎" ⇒ 按本仓库的范围规则，这 47 项**不排期**。

### 6.2 `executed` 的一项都没有 —— 那"最短链"是什么

没有 `executed` 项，所以没有"从 `src/` 到目标函数的最短链"。
作为对照，下面是**同一批类的 float 实例化**的链（这些才是引擎真正执行的）：

| float 类 | 从 `src/` 出发的最短链 |
|---|---|
| `IntrBox2Circle2f` | `src/moho/vision/VisionDB.cpp:88` → `IntrBox2Circle2f::Test()` |
| `IntrBox3Sphere3f` | `src/moho/collision/CColPrimitiveBase.cpp:607` → `IntrBox3Sphere3f::Test()` |
| `IntrLine3Box3f` | `src/moho/ui/UiRuntimeTypes.cpp:21007` → `IntrLine3Box3f::Find()` |
| `IntrSegment3Box3f` | `src/moho/collision/CColPrimitiveBase.cpp:384` → `IntrSegment3Box3f::Find()` |
| `IntrSegment3Sphere3f` | `src/moho/collision/CColPrimitiveBase.cpp:577` → `IntrSegment3Sphere3f::Find()` → `IntrSegment3Capsule3f::Find()` |
| `DistVector3Segment3f` | `src/moho/projectile/Projectile.cpp:1552` → `DistVector3Segment3f::GetSquared()` |
| `DistVector3Box3f` | `src/moho/collision/CColPrimitiveBase.cpp:429` → `DistVector3Box3f::GetSquared()` |
| `Vector3f::Normalize` | `src/moho/entity/Entity.cpp:7664` → `Wm3::Vector3f::Normalize(&pushedUp)` |

注意 `IntrBox2Circle2f` / `IntrBox3Sphere3f` / `IntrSegment3Box3f` / `IntrLine3Box3f` /
`IntrSegment3Sphere3f` 这几个类**在 float 一侧是执行的**，而它们的 `double` 侧是死的 ——
这正是"`double` 一律默认可疑"这条规则的教科书例子：**同一份源码、两个实例化、只有一个活的**。

### 6.3 与旧普查的关系（重新验证，不是引用）

`.tmp/WildMagic3p8-re/reachability/reachability.json` 里这批条目的 `work.status` 是
`excluded`，理由写的是"撤掉显式实例化后 `main.vcxproj` 仍能链接"（**链接探针**）。
本轮按新树重做，结论**方向一致**，但**依据换了**：

* 依据从"链接探针"换成"**调用点的实参静态类型 + 逐级追链到 `src/`**"；
* 新树（`src/` 从 `src/sdk/` 搬上来、`dependencies/WildMagic3p8` 换成上游树）里，
  `src/` **仍然一处 `double` 实参都没有** ⇒ 结论不变；
* 旧记录说四个 double 实例化被引擎"引用"（`Eigen`、`ConvexHull`、`ConvexHull2`、
  `ConvexHull3`）—— **在新树里这条要撤回**：`grep -rn "Eigen" src/` → 0，
  `grep -rn "ConvexHull3" src/` → 只有注释（`src/legacy/containers/RbTree.h:4365`），
  `main.exe` 的 RTTI 里也没有任何 `double` 的 Wild Magic 类 ⇒ 四个全是
  **linked-but-unreachable**（`Eigen<double>` 在 `Wm3Foundation.lib` 里确实在，
  但引擎从不调用它）。

---

## 7. 给下一轮的建议

1. 这 47 项**不要修重建**（修了也改不了引擎），但**要留着**：
   它们逐位验证过、跑起来不花钱，而且 x87 的方法学（`Wm3Detail::X87Asm` 建模、
   `_PC_24` 语义、`fchs` 的 `-0.0`）就是从它们身上建立的
   —— 见 `tools/wmdiff/AGENTS.md` 的"教训"一节。
2. 若要**降噪**，正确做法是**给这批项标注 `not-executed`**（或按可达性筛掉），
   **不是**去改 double 的源码，也**不是**去改编译选项。
3. 真正需要盯的 double 只有**一处**：如果将来 `src/` 里出现任何
   `Wm3::Vector3d` / `Vector3<double>` / 以 `double` 实参调用 Wild Magic 模板的代码，
   上面 A–O 全部立刻变成 `executed`，届时才需要按
   `tools/wmdiff/AGENTS.md` 的"内联 x87"路线把对应体转写。

---

### 附：本轮的可复现命令

```bash
# 1) 失败清单
./build/clang-x86-release/tools/wmdiff/wmdiff.exe --iters 200000 --seed 0x5EED2007 --max-mismatch 0 \
  | grep '^\[FAIL\]' | grep -i double        # -> 47 行

# 2) src/ 侧的三条搜索（本审计的全部基石）
grep -rn "Vector3d\|Vector2d" src/                       # -> 0
grep -rn "\bdouble\b" src/ | grep -i "wm3"              # -> 0
grep -rn "Wm3::Distance\|Distance<\|Eigen\|ConvexHull3" src/   # -> 0

# 3) 旁证：引擎二进制里没有 double 的 Wild Magic RTTI
llvm-nm --defined-only build/clang-x86-release/cmake/deps/wildmagic/Wm3Foundation.lib | grep 'Eigen@N'
# -> 有（库里在），但 main.exe 里没有任何 double 的 Wm3 类型描述符
```

*本文件由审计轮次写入；harness、Wild Magic 树与目标表均未改动。*
