# AGENTS.md —— fafdiff 配对 harness

本文件是 `tools/fafdiff` 的 agent 操作指引。仓库级的约定（构建入口、部署、机器环境、术语）
在仓库根目录的 `AGENTS.md`；`dependencies/WildMagic3p8/wmdiff/AGENTS.md` 是另一个 harness
的指引。

**当前状态：表里有三十一条比对项。** 被测的 TU 是 `src/moho/math/Vector3f.cpp`
（八条：零售 `0x0044F7E0` 的 `moho::NormalizeVectorInto`，以及
`moho::VEC_LargestAxis`、`moho::VecSetLength`、`moho::VecSetLengthTo`、
`moho::VEC_HeadingFromXZ`、`moho::EulerRollToQuat`、`moho::IsValidVector3f`、
`moho::IsUnitLengthVector3f`）与 `src/moho/math/QuaternionMath.cpp`（十五条：
八条四元数比对项 —— `moho::MatrixToQuat`、`moho::RotateQuatByAngle`、
`moho::QuatLERP`、`moho::SLERP`、`moho::NormalizeQuatInPlace`、
`moho::QuatsNearEqual`、`moho::QuatFromVecRot`、`moho::QuatToAxisAndAngle` ——
外加第二批的七条：`moho::MatrixRowsToQuatCanonical`、
`moho::MatrixColumnsToQuatCanonical`、`moho::NormalizeAngleSignedRadians`、
`moho::QuatFromAxisAngleVector`、`moho::BuildRotationMatrixFromEulerHPR`、
`moho::BuildTiltShortestArcDelta`、`moho::BlendOrientationDeltaByMaxAngle`），
外加 `src/moho/math/Vector4f.cpp`（一条：`moho::Vector4f::operator=`）、
`src/moho/math/Vector2f.cpp`（一条：`moho::IsValidVector2f`）与
`src/legacy/math/X87Math.cpp`（五条 x87 助手：`msvc8::atan2f`、`msvc8::cosf`、
`msvc8::sinf`、`msvc8::frexp`、`msvc8::asin` —— 它原来只是依赖 TU，现在本身也是
被测 TU），再外加从 `src/moho/sim/Sim.cpp` 按行号复制出来的
`moho::QuatCrossAdd`（`extract.json`；见下文“复制的闭环”）；它们带的依赖 TU 是
`src/gpg/core/containers/String.cpp`（`gpg::STR_Printf`）、
`src/legacy/containers/String.cpp`（`msvc8::string`）与 Wild Magic 的
`Foundation/System/Wm3System.cpp`（`Wm3::Quaternion` 的拷贝赋值调用
`Wm3::System::Memcpy`）。逐条证据在 `tools/fafdiff/README.md`，覆盖账本在
`tools/fafdiff/COVERAGE.md`。

**零售的格式化路径在本 harness 里跑不起来。** `moho::ToString`
（零售 `0x004ECED0`）在账本里记为 `不适用`：调用进入零售的
`gpg::STR_Va`（`0x00938E00`）之后不再返回，探针（`FAFDIFF_INT3_AT`）显示
`0x00938E0E` 的 `mov dword ptr fs:[0], esp` 装上的零售自己的 SEH 帧吞掉了
故障，展开再也没完成 ⇒ ⇒ 详见 `README.md` 的那一节。

## 术语：中文一律叫“比对项”

一条比对项的中文叫**比对项**，不叫“行”。英文文档保留 `row`（那里确实指表格行），
中文正文、注释、提交信息一律不用“行”指代它——`行号`、`执行`、`一行源码` 这类词照旧。

## 它是什么

一个**独立的 32 位控制台工具**：把零售二进制 `ForgedAlliance_6c6c92f.exe`
映射进自己的地址空间，与**重建版主引擎**（`src/...`）**逐位**比对同一批输入。

关键区别：**被测代码是从 `src/` 直接编进 harness 的**，用的是引擎自己的编译开关
（harness 是引擎 CMake 工程的一个 target，从 `faf_engine_options` 继承全部设置）；镜像代码
一律不镜像。

### 从 wmdiff 搬共享内核时的两条硬注意事项

1. **环境变量是"写"和"读"两处**。`pe_image.cpp` 负责 `SetEnvironmentVariableA`，`main.cpp`
   负责读；只改一处，子进程会**无限重开自己**（本轮真的踩到：一次 fork bomb，清掉了约
   150 个残留进程）。改名前先 `grep` 全目录，两处一起改 ✓
2. **`NDEBUG` 属于 Release 分支，不属于公共开关**。引擎的 Debug 配置只定义 `_DEBUG`、
   不定义 `NDEBUG`；把 `NDEBUG` 放进公共开关会让 debug 构建的宏集合与引擎的 debug 不一致 ✓

## 范围规则：零售二进制里**有对应体**的函数才配对

**这是范围规则，先读它再读别处。** fafdiff 比对的是"重建版的这个函数，与零售二进制里那个
函数，逐位相同"⇒ ⇒ **判据是零售有没有对应体**，与调用者无关：有没有人调用、当前是否被链接，
都不影响比对的价值（今天没接上的调用，接上那天它就得是对的）✓

⇒ ⇒ 重建版自造的函数（零售里没有对应地址）**不需要、也没有办法**配对 ✓

⇒ ⇒ **不做预先排查**：试着配对的时候，对着零售体一看就知道有没有对应——反汇编对不上、地址
指向的是别的函数、或零售那边根本没有这个入口 ⇒ ⇒ 这时把它记进 `COVERAGE.md` 的 `不适用`
并写明原因（"零售无对应体：……"），然后接着做下一条 ✓

⇒ ⇒ 不要为了"确认是否可达"去追调用链：那不是 fafdiff 的问题，追了也不改变任何比对结果 ✓

## harness 编译被测代码的方式，必须与引擎编译它的方式一致

否则 harness 测的是另一个程序，而不是引擎构建的那个。

**绝对不得为了让比对通过而修改任何编译选项的值** —— 哪怕只做一次实验、哪怕只改一行。差异
的收敛位置只有两个：重建源码，或比对中零售一侧的接线；**绝不在"怎么编译"上**。

**这些开关只存在一处：`src/CMakeLists.txt` 的 INTERFACE target `faf_engine_options`。**
defines、include 路径、`_faf_main_options`（`/EHs /permissive- /W3`、DXSDK `-idirafter`、
clang 警告抑制、按架构的 `/FI`）以及 `faf_codegen`/`faf::wx`/`Wm3Dx9Application` 的链接要求
都挂在那里，`faf_engine` 与 `fafdiff` 都链它 ⇒ ⇒ **这里不许再抄第二份**：抄一份就是让它漂
（曾经就是抄的，漏了 DXSDK 与 `/FI`，于是 `src/moho/sim/Sim.cpp` 在 harness 里编不过）✓

⇒ ⇒ 核对方式：`faf_engine` 加这个 target 前后，`main.exe` 的 `.text` 必须逐位相同
（`e20608c567f60bda8be68c22e7aeab0d`，2026-10-29 实测改前改后一致）⇒ ⇒ 一致才说明这只是
"把设置搬了个家"，没有动引擎的产物 ✓

⇒ ⇒ 引擎侧开关一旦变化（`/D` 列表、`/FI`、include 路径），只需改 `faf_engine_options`；
harness 自动跟上 ✓

## 为什么 `Sim.cpp` 不直接编进 harness

`moho::QuatCrossAdd` 的实现曾长期在 `src/moho/sim/Sim.cpp`。试过把它整个 TU 编进来，结论是
**不可行**，原因不是"未定义符号多"（未引用的 COMDAT 确实会被 `/OPT:REF` 丢弃），而是：

1. `Sim.cpp` 有一个 **非 COMDAT 的 `.CRT$XCU` 静态初始化器**（`__GLOBAL__sub_I_Sim.cpp`）——
   它永远被保留，引用文件内所有静态对象的构造函数，于是把 sim 核心一起拖进来；
2. 实测：只加 `Sim.cpp` → 缺 20 个符号（`gpg::Logf`、`gpg::RRef`、`msvc8::string`、`MD5`、
   `BVIntSet`…）；补齐定义它们的 8 个 TU 后，残差换成**另一批** 20 个更深的 sim 符号
   （`CWldSession`、`IdPool`、`UserCommandIssueHelper`…）⇒ ⇒ 不收敛；
3. 引擎 `.obj` 的符号闭包（上界）是 **591/941 个 TU**。

⇒ ⇒ 所以 harness **不链接这个 TU，只编译那个函数**：`extract.json` 用手写的行号区间声明要复制
什么，`scripts/extract_sources.py` 把它切到 `<build>/tools/fafdiff/generated/`，CMake 用引擎
自己的开关编译这份副本。复制出来的 TU 与引擎的 `Sim.cpp.obj` 里那个函数逐指令相同
（172 条）✓

### 复制的闭环：两个哈希，缺一不可

`extract.json` 每个 entry 带两个哈希，脚本只做三件事：校验源哈希、按行号切片、校验生成哈希
（**没有符号搜索，没有 C++ 解析**，行号是人写的）：

| 字段 | 作用 |
|---|---|
| `sourceSha256` | 整个源文件。不符 ⇒ 行号是写给旧版本的，脚本报错并且**不**生成该 entry |
| `outputSha256` | 生成文件的字节。不符 ⇒ 切出来的内容变了，必须有人读过才能用 |

⇒ ⇒ 首次生成时 `outputSha256` 为空：脚本照样写出文件、算出 hash、**报错**。agent 打开生成
文件确认切出来的正是那个函数，再把 hash 填回配置 ⇒ ⇒ 闭环 ✓

⇒ ⇒ 源文件一改，`sourceSha256` 先挡下来；agent 更新行号与源哈希后，内容若变了，`outputSha256`
再挡一次 ⇒ ⇒ "hash 更新了、行号忘了改"无法静默通过 ✓

⇒ ⇒ 错误是**攒起来统一报**的，最多 10 条；到 10 条就不再处理后面的 entry，避免反复打断 ✓

⇒ ⇒ 脚本跑在**构建期**（`add_custom_command`，依赖 `extract.json` 与源文件），不是 configure
阶段——configure 只跑一次，源文件改了不会触发它 ✓

### 加一条提取项

`tools/fafdiff/extract.json` 的 `files` 数组，每个 entry：

| 字段 | 必填 | 含义 |
|---|---|---|
| `source` | ✓ | 源文件，仓库相对路径（正斜杠） |
| `output` | ✓ | 生成文件名，落在 `<build>/tools/fafdiff/generated/<output>` |
| `ranges` | ✓ | `[[首行, 末行], ...]`，1-based、闭区间、互不重叠；**人写** |
| `includes` | ✓ | 生成文件的前导 `#include`；要够它单独编译（声明所在的头、用到的类型） |
| `sourceSha256` | ✓ | 源文件整个的 sha256 |
| `outputSha256` | ✓ | 生成文件的 sha256；**第一次留空**，由报错给出 |
| `note` | | 说明这是哪个符号、零售地址、对应的比对项 |

步骤：

1. 在 `extract.json` 里写好 `source` / `output` / `ranges` / `includes`，两个哈希都留空 `""`；
2. `cmake --build --preset clang-x86-release --target fafdiff` ⇒ 失败。第一轮报 `sourceSha256`
   为空并给出实际值；填上再构建，第二轮才生成文件并报 `outputSha256` 为空、给出生成哈希；
3. **打开生成文件**，确认 `ranges` 切的正是那个完整函数（不多不少），把 `outputSha256` 填回配置；
4. 再构建 ⇒ 通过；然后把新 TU 的符号加进 `src/targets.cpp` 的比对项，`source` 字段写**源文件**的
   `文件:行`（不是生成文件的）。

⇒ ⇒ 生成文件在 build 目录里，**永远不要手改**：改了只会让 `outputSha256` 不符，下次构建失败 ✓

⇒ ⇒ 行号可以带上函数的 Doxygen 注释，也可以只切函数体；只要 `ranges` 覆盖完整定义、且不含
相邻函数即可 ✓

## 构建与运行

```bash
# 从仓库根目录
cmake --preset clang-x86-release
cmake --build --preset clang-x86-release --target fafdiff
# -> build/clang-x86-release/tools/fafdiff/fafdiff.exe

./build/clang-x86-release/tools/fafdiff/fafdiff.exe --list
./build/clang-x86-release/tools/fafdiff/fafdiff.exe --iters 1000000 --seed 0x5EED2007
```

harness 映射的是 32 位零售镜像，**只有 x86 配置有它**；它是 `EXCLUDE_FROM_ALL`，引擎构建
不依赖它。

## 比对项的 ABI 是**带类型**的

`Target`（`src/fuzz.h`）只有这些字段：

| 字段 | 含义 |
|---|---|
| `name` | 短名，`--filter` 用它 |
| `va` | 零售入口地址（填好之前是 0） |
| `source` | 我们这边的定义位置 `file:line` |
| `inBytes` / `outBytes` | 带类型的输入/输出结构体大小 |
| `genT` | `void(Rng&, void*, DrawKind)`：按行自己的类型填输入 |
| `retailT` | `void(const void*, void*)`：调零售那一侧 |
| `oursT` | `void(const void*, void*)`：调重建那一侧 |

⇒ ⇒ **没有"一摞没有类型的字"通道**：每个比对项带自己的 `In`/`Out` 结构体，harness 只看见
字节缓冲区 ⇒ ⇒ ABI 从一开始就是带类型的，**没有转换历史要保留** ✓

⇒ ⇒ `genT` 的 `DrawKind`：`WithExtremes`（默认）是全混合抽样，`Ordinary`（`--skip-extreme`
选的就是它）只抽有限、量级正常、非非规格化的车道 ⇒ ⇒ 因此**带类型的比对项不会因为输入
"太极端"而被跳过** ✓

## 种子按比对项在表里的序号派生 ⇒ ⇒ 表里不许有重复项

**种子由比对项在表里的序号派生**（`gSeed ^ (0x9E3779B97F4A7C15 * (i+1))`）⇒ ⇒ 所以**表里插入一条重复的
比对项，会把它后面每一条的输入全部顶掉** ⇒ ⇒ 表看起来"比对项变多、结果都对"，实则每个比对项
的输入都换了 ⇒ ⇒

⇒ ⇒ 所以：**表里不许有重复的比对项**；每轮之后拿 `--list` 与记录里的 `[PASS]` 清单**逐条
对账**（名字、地址、条数）⇒ 这一步能同时抓出重复项、漏记与错址 ✓

## 信比对结果之前，先看构建退出码与 exe 的时间戳

**构建失败时 harness 会跑旧二进制。** 旧二进制会留在 `build/` 里，而比对项照测它 ⇒ ⇒
"改了源码、结果一个数都没变"会被读成"这个改动不影响结果" ⇒ ⇒

⇒ ⇒ **信比对项之前先看构建退出码 + exe 的时间戳** ⇒ ⇒ 一次变体搜索就是这么白跑的（每个变体
都"无变化"，因为根本没重编）✓

⇒ ⇒ 这条与"量具侧的 ABI 错误会伪装成重建的数值差异"同族 ⇒ ⇒ 下"这是重建的缺陷"这个结论
之前，先证明量具是新的、是对齐的 ✓

## 上游那一代 harness 是只读的

更早一代的叶子函数 harness（`tools/` 下的另一个 harness，工具清单见 `tools/README.md`）是
**上游**，**只读**：不许改它，不许把它当模板抄它的目标。

⇒ ⇒ 但**也不许把它的比对项原样搬过来**：那样做等于把上游的工作重做一遍而没有新增维度 ⇒ ⇒
fafdiff 要加的是**新维度** —— **对象状态**（不只返回值与出参，还有函数留下的对象字节）、
**调用序列**（同一批输入走过一串零售调用，而不是每个比对项一次调用）、
**零售对应体的账本**、**按 TU 的系统覆盖** ⇒ ⇒ 一个目标若只是上游已有目标的翻版，**不加维度就不许入表** ✓

⇒ ⇒ 另外：上游 harness 的覆盖**不再是**跳过某个函数的理由 —— 引擎已经换成 clang-cl 构建
（`cmake/FafCodegen.cmake`），而上游那份仍用 MSVC `cl` 编它自己的副本 ⇒ ⇒ 它的覆盖不再为
引擎实际发布的代码作证 ✓

## 判定标准

**与零售逐位一致**，不是"与数学答案一致"。零售的行为也许本身就是缺陷，但现阶段也要复现它，
并在相关函数的注释里记档（零售行为是什么、证据是什么）。

失配要分三档看：**PASS**（逐位一致）/ **已知差异**（机制已查明，记录后跳过）/
**机制不明**（只有这一档是真正的工作量所在）。不要把第二档当待办清单。

## 判别力：每个新比对项都要做双向变异检查

**"比对项通过"只说明生成器走到的地方一致。** 写新比对项时要问：这个函数的哪些区间是随机
抽样几乎到不了的？把它们**构造**出来。

⇒ ⇒ 每个新比对项都要做**双向变异检查**（退源码 → 比对项必须失败 → 恢复 → 必须通过），
并把**两个方向的计数**都记下来 ⇒ ⇒ 只报"通过"的比对项，等于没测 ✓

⇒ ⇒ `bool` 返回值的比对项是**弱见证**：24 位与 binary64 的差别只在值落到阈值一个 ulp 以内
时才翻转布尔 ⇒ ⇒ 它**通过**百万次不能证明底下的算术对，**失败**一次就是真失配 ⇒ ⇒ 要证
算术，用**返回值**的比对项。

## 硬件探针与模拟器

`tools/x87sim` 的**解码器**是读零售体的权威，**hwprobe** 是条件码极性的权威，
**harness 实测**是最终裁决。模拟器跑某个零售体得出的结论，先确认它能正确执行那个体。

零售跑在 `_PC_24`（`fninit; fldcw 0x007F`）下 ⇒ ⇒ 零售的 `double` 路径**不是 binary64** ⇒ ⇒
harness 默认控制字就是 `0x007F`，**不要改**（改了要重新全量测量）。

## 不要为了让比对通过而改编译选项

**这条是硬规则。** 见上面"harness 编译被测代码的方式"一节。
