# Math/Vector.hpp · Swizzle 宏展开说明

> 本文件记录 `Math/Vector.hpp` 第 38–148 行 swizzle 宏经预处理器**真实展开**后的结果，
> 用于直观展示每个 `Vector<T,N>` 特化到底生成了哪些方法、每个方法对应哪组分量下标。
>
> 展开由 `g++ -E` 得到，仅做格式整理（每个方法单独一行），**未改动任何语义**。
> 真正的实现仍是类里手写的 `Swizzle<Indices...>()`（见 `Vector.hpp:176`），宏只是给每种"分量名组合"包一层语法糖。

---

## 一、宏系统的四层结构

C++ 宏没有循环，只有替换。要批量生成"笛卡儿积"形式的方法名表，必须用**多层嵌套宏模拟嵌套 for 循环**。本文件按"由内到外"分四层：

### 第 1 层：生成单个方法的三个宏（Vector.hpp:38–43）

```cpp
DEFINE_SWIZZLE_2(A, AI, B, BI)                  // ->  constexpr Vector<T,2> A##B()        { return Swizzle<AI,BI>(); }
DEFINE_SWIZZLE_3(A, AI, B, BI, C, CI)           // ->  constexpr Vector<T,3> A##B##C()     { return Swizzle<AI,BI,CI>(); }
DEFINE_SWIZZLE_4(A, AI, B, BI, C, CI, D, DI)    // ->  constexpr Vector<T,4> A##B##C##D()  { return Swizzle<AI,BI,CI,DI>(); }
```

- `A/B/C/D`：分量**名字**（x、y、z、w 或 r、g、b、a）。
- `AI/BI/CI/DI`：对应**下标**（0、1、2、3）。
- `A##B` 用 `##` 拼接出方法名，方法体统一转交给真正的实现 `Swizzle<...>()`。

### 第 2 层：固定高位、让最低位遍历 —— ROW / PAIR / TRIPLE（Vector.hpp:45–135）

命名规律 `SWIZZLE_<输出长度>_<结构>_<可用分量数>`：

- `ROW`：固定**首位**，让第二位在分量集合上扫一遍。
- `PAIR`：固定前两位，让第三位扫一遍（只在 3/4 维方法里出现）。
- `TRIPLE`：固定前三位，让第四位扫一遍（只在 4 维方法里出现）。

每一层多固定一个位置，最内层调用 `DEFINE_SWIZZLE_n` 落地成真实方法。

### 第 3 层：顶层 `DEFINE_SWIZZLES_n_COMPONENTS`（Vector.hpp:63、95、136）

把"首字母"也在所有可用分量上扫一遍 → 覆盖某维向量在给定分量集合下、所有长度 2–4 的组合。

### 第 4 层：在特化里调用（Vector.hpp:189 / 236 / 283）

```cpp
// Vector<T,2>：可用分量 {x,y}
DEFINE_SWIZZLES_2_COMPONENTS(x, 0, y, 1)     // x 系
DEFINE_SWIZZLES_2_COMPONENTS(r, 0, g, 1)     // rgba 系

// Vector<T,3>：可用分量 {x,y,z}
DEFINE_SWIZZLES_3_COMPONENTS(x, 0, y, 1, z, 2)
DEFINE_SWIZZLES_3_COMPONENTS(r, 0, g, 1, b, 2)

// Vector<T,4>：可用分量 {x,y,z,w}
DEFINE_SWIZZLES_4_COMPONENTS(x, 0, y, 1, z, 2, w, 3)
DEFINE_SWIZZLES_4_COMPONENTS(r, 0, g, 1, b, 2, a, 3)
```

> ⚠ 关键易错点：`Vector<T,2>` 只能调用 `_2_COMPONENTS`、`<T,3>` 只能调用 `_3_COMPONENTS`、`<T,4>` 只能调用 `_4_COMPONENTS`。
> 之前出现的 `Vector<T,2>` 误用 `_3_COMPONENTS`、`<T,3>` 误用 `_2_COMPONENTS`（参数个数与宏不匹配）会导致 C4003/C2059 连锁报错，并让整个特化失效，进而牵连所有引用该向量的运算符出现"假报错"。

---

## 二、一次完整追踪：`xx()` / `xy()` 是怎么来的

以 `Vector<T,2>` 调用 `DEFINE_SWIZZLES_2_COMPONENTS(x, 0, y, 1)` 为例，沿宏调用链逐层展开：

```
DEFINE_SWIZZLES_2_COMPONENTS(x, 0, y, 1)
  └─ SWIZZLE_2_ROW_2(x, 0, x, 0, y, 1)              // 首字母 = x
       ├─ DEFINE_SWIZZLE_2(x, 0, x, 0)   ->  xx()  { return Swizzle<0,0>(); }
       └─ DEFINE_SWIZZLE_2(x, 0, y, 1)   ->  xy()  { return Swizzle<0,1>(); }
  └─ SWIZZLE_2_ROW_2(y, 1, x, 0, y, 1)              // 首字母 = y
       ├─ DEFINE_SWIZZLE_2(y, 1, x, 0)   ->  yx()  { return Swizzle<1,0>(); }
       └─ DEFINE_SWIZZLE_2(y, 1, y, 1)   ->  yy()  { return Swizzle<1,1>(); }
```

3 维同理，只是多一层 PAIR：固定前两位、让第三位扫遍分量。`xxx / xxy / xyx / xyy / yxx / yxy / yyx / yyy` 共 8 个。
4 维再加一层 TRIPLE，共 16 个。命名规律：**长度 k 的方法数 = 分量数^k**。

---

## 三、Vector&lt;T,2&gt; 展开结果（56 个方法）

可用分量 `{x=0, y=1}`，两套命名系（x 系 + rgba 系）。
2 维：4 个 / 系，3 维：8 个 / 系，4 维：16 个 / 系 → 每系 28 个，合计 56。

```cpp
    constexpr Vector<T, 2> xx() const noexcept { return Swizzle<0, 0>(); }
    constexpr Vector<T, 2> xy() const noexcept { return Swizzle<0, 1>(); }
    constexpr Vector<T, 2> yx() const noexcept { return Swizzle<1, 0>(); }
    constexpr Vector<T, 2> yy() const noexcept { return Swizzle<1, 1>(); }
    constexpr Vector<T, 3> xxx() const noexcept { return Swizzle<0, 0, 0>(); }
    constexpr Vector<T, 3> xxy() const noexcept { return Swizzle<0, 0, 1>(); }
    constexpr Vector<T, 3> xyx() const noexcept { return Swizzle<0, 1, 0>(); }
    constexpr Vector<T, 3> xyy() const noexcept { return Swizzle<0, 1, 1>(); }
    constexpr Vector<T, 3> yxx() const noexcept { return Swizzle<1, 0, 0>(); }
    constexpr Vector<T, 3> yxy() const noexcept { return Swizzle<1, 0, 1>(); }
    constexpr Vector<T, 3> yyx() const noexcept { return Swizzle<1, 1, 0>(); }
    constexpr Vector<T, 3> yyy() const noexcept { return Swizzle<1, 1, 1>(); }
    constexpr Vector<T, 4> xxxx() const noexcept { return Swizzle<0, 0, 0, 0>(); }
    constexpr Vector<T, 4> xxxy() const noexcept { return Swizzle<0, 0, 0, 1>(); }
    constexpr Vector<T, 4> xxyx() const noexcept { return Swizzle<0, 0, 1, 0>(); }
    constexpr Vector<T, 4> xxyy() const noexcept { return Swizzle<0, 0, 1, 1>(); }
    constexpr Vector<T, 4> xyxx() const noexcept { return Swizzle<0, 1, 0, 0>(); }
    constexpr Vector<T, 4> xyxy() const noexcept { return Swizzle<0, 1, 0, 1>(); }
    constexpr Vector<T, 4> xyyx() const noexcept { return Swizzle<0, 1, 1, 0>(); }
    constexpr Vector<T, 4> xyyy() const noexcept { return Swizzle<0, 1, 1, 1>(); }
    constexpr Vector<T, 4> yxxx() const noexcept { return Swizzle<1, 0, 0, 0>(); }
    constexpr Vector<T, 4> yxxy() const noexcept { return Swizzle<1, 0, 0, 1>(); }
    constexpr Vector<T, 4> yxyx() const noexcept { return Swizzle<1, 0, 1, 0>(); }
    constexpr Vector<T, 4> yxyy() const noexcept { return Swizzle<1, 0, 1, 1>(); }
    constexpr Vector<T, 4> yyxx() const noexcept { return Swizzle<1, 1, 0, 0>(); }
    constexpr Vector<T, 4> yyxy() const noexcept { return Swizzle<1, 1, 0, 1>(); }
    constexpr Vector<T, 4> yyyx() const noexcept { return Swizzle<1, 1, 1, 0>(); }
    constexpr Vector<T, 4> yyyy() const noexcept { return Swizzle<1, 1, 1, 1>(); }
    constexpr Vector<T, 2> rr() const noexcept { return Swizzle<0, 0>(); }
    constexpr Vector<T, 2> rg() const noexcept { return Swizzle<0, 1>(); }
    constexpr Vector<T, 2> gr() const noexcept { return Swizzle<1, 0>(); }
    constexpr Vector<T, 2> gg() const noexcept { return Swizzle<1, 1>(); }
    constexpr Vector<T, 3> rrr() const noexcept { return Swizzle<0, 0, 0>(); }
    constexpr Vector<T, 3> rrg() const noexcept { return Swizzle<0, 0, 1>(); }
    constexpr Vector<T, 3> rgr() const noexcept { return Swizzle<0, 1, 0>(); }
    constexpr Vector<T, 3> rgg() const noexcept { return Swizzle<0, 1, 1>(); }
    constexpr Vector<T, 3> grr() const noexcept { return Swizzle<1, 0, 0>(); }
    constexpr Vector<T, 3> grg() const noexcept { return Swizzle<1, 0, 1>(); }
    constexpr Vector<T, 3> ggr() const noexcept { return Swizzle<1, 1, 0>(); }
    constexpr Vector<T, 3> ggg() const noexcept { return Swizzle<1, 1, 1>(); }
    constexpr Vector<T, 4> rrrr() const noexcept { return Swizzle<0, 0, 0, 0>(); }
    constexpr Vector<T, 4> rrrg() const noexcept { return Swizzle<0, 0, 0, 1>(); }
    constexpr Vector<T, 4> rrgr() const noexcept { return Swizzle<0, 0, 1, 0>(); }
    constexpr Vector<T, 4> rrgg() const noexcept { return Swizzle<0, 0, 1, 1>(); }
    constexpr Vector<T, 4> rgrr() const noexcept { return Swizzle<0, 1, 0, 0>(); }
    constexpr Vector<T, 4> rgrg() const noexcept { return Swizzle<0, 1, 0, 1>(); }
    constexpr Vector<T, 4> rggr() const noexcept { return Swizzle<0, 1, 1, 0>(); }
    constexpr Vector<T, 4> rggg() const noexcept { return Swizzle<0, 1, 1, 1>(); }
    constexpr Vector<T, 4> grrr() const noexcept { return Swizzle<1, 0, 0, 0>(); }
    constexpr Vector<T, 4> grrg() const noexcept { return Swizzle<1, 0, 0, 1>(); }
    constexpr Vector<T, 4> grgr() const noexcept { return Swizzle<1, 0, 1, 0>(); }
    constexpr Vector<T, 4> grgg() const noexcept { return Swizzle<1, 0, 1, 1>(); }
    constexpr Vector<T, 4> ggrr() const noexcept { return Swizzle<1, 1, 0, 0>(); }
    constexpr Vector<T, 4> ggrg() const noexcept { return Swizzle<1, 1, 0, 1>(); }
    constexpr Vector<T, 4> gggr() const noexcept { return Swizzle<1, 1, 1, 0>(); }
    constexpr Vector<T, 4> gggg() const noexcept { return Swizzle<1, 1, 1, 1>(); }


---

## 四、Vector&lt;T,3&gt; 展开结果（234 个方法）

可用分量 `{x=0, y=1, z=2}`，两套命名系（x 系 + rgba 系）。
2 维：9 个 / 系，3 维：27 个 / 系，4 维：81 个 / 系 → 每系 117 个，合计 234。

```cpp
    constexpr Vector<T, 2> xx() const noexcept { return Swizzle<0, 0>(); }
    constexpr Vector<T, 2> xy() const noexcept { return Swizzle<0, 1>(); }
    constexpr Vector<T, 2> xz() const noexcept { return Swizzle<0, 2>(); }
    constexpr Vector<T, 2> yx() const noexcept { return Swizzle<1, 0>(); }
    constexpr Vector<T, 2> yy() const noexcept { return Swizzle<1, 1>(); }
    constexpr Vector<T, 2> yz() const noexcept { return Swizzle<1, 2>(); }
    constexpr Vector<T, 2> zx() const noexcept { return Swizzle<2, 0>(); }
    constexpr Vector<T, 2> zy() const noexcept { return Swizzle<2, 1>(); }
    constexpr Vector<T, 2> zz() const noexcept { return Swizzle<2, 2>(); }
    constexpr Vector<T, 3> xxx() const noexcept { return Swizzle<0, 0, 0>(); }
    constexpr Vector<T, 3> xxy() const noexcept { return Swizzle<0, 0, 1>(); }
    constexpr Vector<T, 3> xxz() const noexcept { return Swizzle<0, 0, 2>(); }
    constexpr Vector<T, 3> xyx() const noexcept { return Swizzle<0, 1, 0>(); }
    constexpr Vector<T, 3> xyy() const noexcept { return Swizzle<0, 1, 1>(); }
    constexpr Vector<T, 3> xyz() const noexcept { return Swizzle<0, 1, 2>(); }
    constexpr Vector<T, 3> xzx() const noexcept { return Swizzle<0, 2, 0>(); }
    constexpr Vector<T, 3> xzy() const noexcept { return Swizzle<0, 2, 1>(); }
    constexpr Vector<T, 3> xzz() const noexcept { return Swizzle<0, 2, 2>(); }
    constexpr Vector<T, 3> yxx() const noexcept { return Swizzle<1, 0, 0>(); }
    constexpr Vector<T, 3> yxy() const noexcept { return Swizzle<1, 0, 1>(); }
    constexpr Vector<T, 3> yxz() const noexcept { return Swizzle<1, 0, 2>(); }
    constexpr Vector<T, 3> yyx() const noexcept { return Swizzle<1, 1, 0>(); }
    constexpr Vector<T, 3> yyy() const noexcept { return Swizzle<1, 1, 1>(); }
    constexpr Vector<T, 3> yyz() const noexcept { return Swizzle<1, 1, 2>(); }
    constexpr Vector<T, 3> yzx() const noexcept { return Swizzle<1, 2, 0>(); }
    constexpr Vector<T, 3> yzy() const noexcept { return Swizzle<1, 2, 1>(); }
    constexpr Vector<T, 3> yzz() const noexcept { return Swizzle<1, 2, 2>(); }
    constexpr Vector<T, 3> zxx() const noexcept { return Swizzle<2, 0, 0>(); }
    constexpr Vector<T, 3> zxy() const noexcept { return Swizzle<2, 0, 1>(); }
    constexpr Vector<T, 3> zxz() const noexcept { return Swizzle<2, 0, 2>(); }
    constexpr Vector<T, 3> zyx() const noexcept { return Swizzle<2, 1, 0>(); }
    constexpr Vector<T, 3> zyy() const noexcept { return Swizzle<2, 1, 1>(); }
    constexpr Vector<T, 3> zyz() const noexcept { return Swizzle<2, 1, 2>(); }
    constexpr Vector<T, 3> zzx() const noexcept { return Swizzle<2, 2, 0>(); }
    constexpr Vector<T, 3> zzy() const noexcept { return Swizzle<2, 2, 1>(); }
    constexpr Vector<T, 3> zzz() const noexcept { return Swizzle<2, 2, 2>(); }
    constexpr Vector<T, 4> xxxx() const noexcept { return Swizzle<0, 0, 0, 0>(); }
    constexpr Vector<T, 4> xxxy() const noexcept { return Swizzle<0, 0, 0, 1>(); }
    constexpr Vector<T, 4> xxxz() const noexcept { return Swizzle<0, 0, 0, 2>(); }
    constexpr Vector<T, 4> xxyx() const noexcept { return Swizzle<0, 0, 1, 0>(); }
    constexpr Vector<T, 4> xxyy() const noexcept { return Swizzle<0, 0, 1, 1>(); }
    constexpr Vector<T, 4> xxyz() const noexcept { return Swizzle<0, 0, 1, 2>(); }
    constexpr Vector<T, 4> xxzx() const noexcept { return Swizzle<0, 0, 2, 0>(); }
    constexpr Vector<T, 4> xxzy() const noexcept { return Swizzle<0, 0, 2, 1>(); }
    constexpr Vector<T, 4> xxzz() const noexcept { return Swizzle<0, 0, 2, 2>(); }
    constexpr Vector<T, 4> xyxx() const noexcept { return Swizzle<0, 1, 0, 0>(); }
    constexpr Vector<T, 4> xyxy() const noexcept { return Swizzle<0, 1, 0, 1>(); }
    constexpr Vector<T, 4> xyxz() const noexcept { return Swizzle<0, 1, 0, 2>(); }
    constexpr Vector<T, 4> xyyx() const noexcept { return Swizzle<0, 1, 1, 0>(); }
    constexpr Vector<T, 4> xyyy() const noexcept { return Swizzle<0, 1, 1, 1>(); }
    constexpr Vector<T, 4> xyyz() const noexcept { return Swizzle<0, 1, 1, 2>(); }
    constexpr Vector<T, 4> xyzx() const noexcept { return Swizzle<0, 1, 2, 0>(); }
    constexpr Vector<T, 4> xyzy() const noexcept { return Swizzle<0, 1, 2, 1>(); }
    constexpr Vector<T, 4> xyzz() const noexcept { return Swizzle<0, 1, 2, 2>(); }
    constexpr Vector<T, 4> xzxx() const noexcept { return Swizzle<0, 2, 0, 0>(); }
    constexpr Vector<T, 4> xzxy() const noexcept { return Swizzle<0, 2, 0, 1>(); }
    constexpr Vector<T, 4> xzxz() const noexcept { return Swizzle<0, 2, 0, 2>(); }
    constexpr Vector<T, 4> xzyx() const noexcept { return Swizzle<0, 2, 1, 0>(); }
    constexpr Vector<T, 4> xzyy() const noexcept { return Swizzle<0, 2, 1, 1>(); }
    constexpr Vector<T, 4> xzyz() const noexcept { return Swizzle<0, 2, 1, 2>(); }
    constexpr Vector<T, 4> xzzx() const noexcept { return Swizzle<0, 2, 2, 0>(); }
    constexpr Vector<T, 4> xzzy() const noexcept { return Swizzle<0, 2, 2, 1>(); }
    constexpr Vector<T, 4> xzzz() const noexcept { return Swizzle<0, 2, 2, 2>(); }
    constexpr Vector<T, 4> yxxx() const noexcept { return Swizzle<1, 0, 0, 0>(); }
    constexpr Vector<T, 4> yxxy() const noexcept { return Swizzle<1, 0, 0, 1>(); }
    constexpr Vector<T, 4> yxxz() const noexcept { return Swizzle<1, 0, 0, 2>(); }
    constexpr Vector<T, 4> yxyx() const noexcept { return Swizzle<1, 0, 1, 0>(); }
    constexpr Vector<T, 4> yxyy() const noexcept { return Swizzle<1, 0, 1, 1>(); }
    constexpr Vector<T, 4> yxyz() const noexcept { return Swizzle<1, 0, 1, 2>(); }
    constexpr Vector<T, 4> yxzx() const noexcept { return Swizzle<1, 0, 2, 0>(); }
    constexpr Vector<T, 4> yxzy() const noexcept { return Swizzle<1, 0, 2, 1>(); }
    constexpr Vector<T, 4> yxzz() const noexcept { return Swizzle<1, 0, 2, 2>(); }
    constexpr Vector<T, 4> yyxx() const noexcept { return Swizzle<1, 1, 0, 0>(); }
    constexpr Vector<T, 4> yyxy() const noexcept { return Swizzle<1, 1, 0, 1>(); }
    constexpr Vector<T, 4> yyxz() const noexcept { return Swizzle<1, 1, 0, 2>(); }
    constexpr Vector<T, 4> yyyx() const noexcept { return Swizzle<1, 1, 1, 0>(); }
    constexpr Vector<T, 4> yyyy() const noexcept { return Swizzle<1, 1, 1, 1>(); }
    constexpr Vector<T, 4> yyyz() const noexcept { return Swizzle<1, 1, 1, 2>(); }
    constexpr Vector<T, 4> yyzx() const noexcept { return Swizzle<1, 1, 2, 0>(); }
    constexpr Vector<T, 4> yyzy() const noexcept { return Swizzle<1, 1, 2, 1>(); }
    constexpr Vector<T, 4> yyzz() const noexcept { return Swizzle<1, 1, 2, 2>(); }
    constexpr Vector<T, 4> yzxx() const noexcept { return Swizzle<1, 2, 0, 0>(); }
    constexpr Vector<T, 4> yzxy() const noexcept { return Swizzle<1, 2, 0, 1>(); }
    constexpr Vector<T, 4> yzxz() const noexcept { return Swizzle<1, 2, 0, 2>(); }
    constexpr Vector<T, 4> yzyx() const noexcept { return Swizzle<1, 2, 1, 0>(); }
    constexpr Vector<T, 4> yzyy() const noexcept { return Swizzle<1, 2, 1, 1>(); }
    constexpr Vector<T, 4> yzyz() const noexcept { return Swizzle<1, 2, 1, 2>(); }
    constexpr Vector<T, 4> yzzx() const noexcept { return Swizzle<1, 2, 2, 0>(); }
    constexpr Vector<T, 4> yzzy() const noexcept { return Swizzle<1, 2, 2, 1>(); }
    constexpr Vector<T, 4> yzzz() const noexcept { return Swizzle<1, 2, 2, 2>(); }
    constexpr Vector<T, 4> zxxx() const noexcept { return Swizzle<2, 0, 0, 0>(); }
    constexpr Vector<T, 4> zxxy() const noexcept { return Swizzle<2, 0, 0, 1>(); }
    constexpr Vector<T, 4> zxxz() const noexcept { return Swizzle<2, 0, 0, 2>(); }
    constexpr Vector<T, 4> zxyx() const noexcept { return Swizzle<2, 0, 1, 0>(); }
    constexpr Vector<T, 4> zxyy() const noexcept { return Swizzle<2, 0, 1, 1>(); }
    constexpr Vector<T, 4> zxyz() const noexcept { return Swizzle<2, 0, 1, 2>(); }
    constexpr Vector<T, 4> zxzx() const noexcept { return Swizzle<2, 0, 2, 0>(); }
    constexpr Vector<T, 4> zxzy() const noexcept { return Swizzle<2, 0, 2, 1>(); }
    constexpr Vector<T, 4> zxzz() const noexcept { return Swizzle<2, 0, 2, 2>(); }
    constexpr Vector<T, 4> zyxx() const noexcept { return Swizzle<2, 1, 0, 0>(); }
    constexpr Vector<T, 4> zyxy() const noexcept { return Swizzle<2, 1, 0, 1>(); }
    constexpr Vector<T, 4> zyxz() const noexcept { return Swizzle<2, 1, 0, 2>(); }
    constexpr Vector<T, 4> zyyx() const noexcept { return Swizzle<2, 1, 1, 0>(); }
    constexpr Vector<T, 4> zyyy() const noexcept { return Swizzle<2, 1, 1, 1>(); }
    constexpr Vector<T, 4> zyyz() const noexcept { return Swizzle<2, 1, 1, 2>(); }
    constexpr Vector<T, 4> zyzx() const noexcept { return Swizzle<2, 1, 2, 0>(); }
    constexpr Vector<T, 4> zyzy() const noexcept { return Swizzle<2, 1, 2, 1>(); }
    constexpr Vector<T, 4> zyzz() const noexcept { return Swizzle<2, 1, 2, 2>(); }
    constexpr Vector<T, 4> zzxx() const noexcept { return Swizzle<2, 2, 0, 0>(); }
    constexpr Vector<T, 4> zzxy() const noexcept { return Swizzle<2, 2, 0, 1>(); }
    constexpr Vector<T, 4> zzxz() const noexcept { return Swizzle<2, 2, 0, 2>(); }
    constexpr Vector<T, 4> zzyx() const noexcept { return Swizzle<2, 2, 1, 0>(); }
    constexpr Vector<T, 4> zzyy() const noexcept { return Swizzle<2, 2, 1, 1>(); }
    constexpr Vector<T, 4> zzyz() const noexcept { return Swizzle<2, 2, 1, 2>(); }
    constexpr Vector<T, 4> zzzx() const noexcept { return Swizzle<2, 2, 2, 0>(); }
    constexpr Vector<T, 4> zzzy() const noexcept { return Swizzle<2, 2, 2, 1>(); }
    constexpr Vector<T, 4> zzzz() const noexcept { return Swizzle<2, 2, 2, 2>(); }
    constexpr Vector<T, 2> rr() const noexcept { return Swizzle<0, 0>(); }
    constexpr Vector<T, 2> rg() const noexcept { return Swizzle<0, 1>(); }
    constexpr Vector<T, 2> rb() const noexcept { return Swizzle<0, 2>(); }
    constexpr Vector<T, 2> gr() const noexcept { return Swizzle<1, 0>(); }
    constexpr Vector<T, 2> gg() const noexcept { return Swizzle<1, 1>(); }
    constexpr Vector<T, 2> gb() const noexcept { return Swizzle<1, 2>(); }
    constexpr Vector<T, 2> br() const noexcept { return Swizzle<2, 0>(); }
    constexpr Vector<T, 2> bg() const noexcept { return Swizzle<2, 1>(); }
    constexpr Vector<T, 2> bb() const noexcept { return Swizzle<2, 2>(); }
    constexpr Vector<T, 3> rrr() const noexcept { return Swizzle<0, 0, 0>(); }
    constexpr Vector<T, 3> rrg() const noexcept { return Swizzle<0, 0, 1>(); }
    constexpr Vector<T, 3> rrb() const noexcept { return Swizzle<0, 0, 2>(); }
    constexpr Vector<T, 3> rgr() const noexcept { return Swizzle<0, 1, 0>(); }
    constexpr Vector<T, 3> rgg() const noexcept { return Swizzle<0, 1, 1>(); }
    constexpr Vector<T, 3> rgb() const noexcept { return Swizzle<0, 1, 2>(); }
    constexpr Vector<T, 3> rbr() const noexcept { return Swizzle<0, 2, 0>(); }
    constexpr Vector<T, 3> rbg() const noexcept { return Swizzle<0, 2, 1>(); }
    constexpr Vector<T, 3> rbb() const noexcept { return Swizzle<0, 2, 2>(); }
    constexpr Vector<T, 3> grr() const noexcept { return Swizzle<1, 0, 0>(); }
    constexpr Vector<T, 3> grg() const noexcept { return Swizzle<1, 0, 1>(); }
    constexpr Vector<T, 3> grb() const noexcept { return Swizzle<1, 0, 2>(); }
    constexpr Vector<T, 3> ggr() const noexcept { return Swizzle<1, 1, 0>(); }
    constexpr Vector<T, 3> ggg() const noexcept { return Swizzle<1, 1, 1>(); }
    constexpr Vector<T, 3> ggb() const noexcept { return Swizzle<1, 1, 2>(); }
    constexpr Vector<T, 3> gbr() const noexcept { return Swizzle<1, 2, 0>(); }
    constexpr Vector<T, 3> gbg() const noexcept { return Swizzle<1, 2, 1>(); }
    constexpr Vector<T, 3> gbb() const noexcept { return Swizzle<1, 2, 2>(); }
    constexpr Vector<T, 3> brr() const noexcept { return Swizzle<2, 0, 0>(); }
    constexpr Vector<T, 3> brg() const noexcept { return Swizzle<2, 0, 1>(); }
    constexpr Vector<T, 3> brb() const noexcept { return Swizzle<2, 0, 2>(); }
    constexpr Vector<T, 3> bgr() const noexcept { return Swizzle<2, 1, 0>(); }
    constexpr Vector<T, 3> bgg() const noexcept { return Swizzle<2, 1, 1>(); }
    constexpr Vector<T, 3> bgb() const noexcept { return Swizzle<2, 1, 2>(); }
    constexpr Vector<T, 3> bbr() const noexcept { return Swizzle<2, 2, 0>(); }
    constexpr Vector<T, 3> bbg() const noexcept { return Swizzle<2, 2, 1>(); }
    constexpr Vector<T, 3> bbb() const noexcept { return Swizzle<2, 2, 2>(); }
    constexpr Vector<T, 4> rrrr() const noexcept { return Swizzle<0, 0, 0, 0>(); }
    constexpr Vector<T, 4> rrrg() const noexcept { return Swizzle<0, 0, 0, 1>(); }
    constexpr Vector<T, 4> rrrb() const noexcept { return Swizzle<0, 0, 0, 2>(); }
    constexpr Vector<T, 4> rrgr() const noexcept { return Swizzle<0, 0, 1, 0>(); }
    constexpr Vector<T, 4> rrgg() const noexcept { return Swizzle<0, 0, 1, 1>(); }
    constexpr Vector<T, 4> rrgb() const noexcept { return Swizzle<0, 0, 1, 2>(); }
    constexpr Vector<T, 4> rrbr() const noexcept { return Swizzle<0, 0, 2, 0>(); }
    constexpr Vector<T, 4> rrbg() const noexcept { return Swizzle<0, 0, 2, 1>(); }
    constexpr Vector<T, 4> rrbb() const noexcept { return Swizzle<0, 0, 2, 2>(); }
    constexpr Vector<T, 4> rgrr() const noexcept { return Swizzle<0, 1, 0, 0>(); }
    constexpr Vector<T, 4> rgrg() const noexcept { return Swizzle<0, 1, 0, 1>(); }
    constexpr Vector<T, 4> rgrb() const noexcept { return Swizzle<0, 1, 0, 2>(); }
    constexpr Vector<T, 4> rggr() const noexcept { return Swizzle<0, 1, 1, 0>(); }
    constexpr Vector<T, 4> rggg() const noexcept { return Swizzle<0, 1, 1, 1>(); }
    constexpr Vector<T, 4> rggb() const noexcept { return Swizzle<0, 1, 1, 2>(); }
    constexpr Vector<T, 4> rgbr() const noexcept { return Swizzle<0, 1, 2, 0>(); }
    constexpr Vector<T, 4> rgbg() const noexcept { return Swizzle<0, 1, 2, 1>(); }
    constexpr Vector<T, 4> rgbb() const noexcept { return Swizzle<0, 1, 2, 2>(); }
    constexpr Vector<T, 4> rbrr() const noexcept { return Swizzle<0, 2, 0, 0>(); }
    constexpr Vector<T, 4> rbrg() const noexcept { return Swizzle<0, 2, 0, 1>(); }
    constexpr Vector<T, 4> rbrb() const noexcept { return Swizzle<0, 2, 0, 2>(); }
    constexpr Vector<T, 4> rbgr() const noexcept { return Swizzle<0, 2, 1, 0>(); }
    constexpr Vector<T, 4> rbgg() const noexcept { return Swizzle<0, 2, 1, 1>(); }
    constexpr Vector<T, 4> rbgb() const noexcept { return Swizzle<0, 2, 1, 2>(); }
    constexpr Vector<T, 4> rbbr() const noexcept { return Swizzle<0, 2, 2, 0>(); }
    constexpr Vector<T, 4> rbbg() const noexcept { return Swizzle<0, 2, 2, 1>(); }
    constexpr Vector<T, 4> rbbb() const noexcept { return Swizzle<0, 2, 2, 2>(); }
    constexpr Vector<T, 4> grrr() const noexcept { return Swizzle<1, 0, 0, 0>(); }
    constexpr Vector<T, 4> grrg() const noexcept { return Swizzle<1, 0, 0, 1>(); }
    constexpr Vector<T, 4> grrb() const noexcept { return Swizzle<1, 0, 0, 2>(); }
    constexpr Vector<T, 4> grgr() const noexcept { return Swizzle<1, 0, 1, 0>(); }
    constexpr Vector<T, 4> grgg() const noexcept { return Swizzle<1, 0, 1, 1>(); }
    constexpr Vector<T, 4> grgb() const noexcept { return Swizzle<1, 0, 1, 2>(); }
    constexpr Vector<T, 4> grbr() const noexcept { return Swizzle<1, 0, 2, 0>(); }
    constexpr Vector<T, 4> grbg() const noexcept { return Swizzle<1, 0, 2, 1>(); }
    constexpr Vector<T, 4> grbb() const noexcept { return Swizzle<1, 0, 2, 2>(); }
    constexpr Vector<T, 4> ggrr() const noexcept { return Swizzle<1, 1, 0, 0>(); }
    constexpr Vector<T, 4> ggrg() const noexcept { return Swizzle<1, 1, 0, 1>(); }
    constexpr Vector<T, 4> ggrb() const noexcept { return Swizzle<1, 1, 0, 2>(); }
    constexpr Vector<T, 4> gggr() const noexcept { return Swizzle<1, 1, 1, 0>(); }
    constexpr Vector<T, 4> gggg() const noexcept { return Swizzle<1, 1, 1, 1>(); }
    constexpr Vector<T, 4> gggb() const noexcept { return Swizzle<1, 1, 1, 2>(); }
    constexpr Vector<T, 4> ggbr() const noexcept { return Swizzle<1, 1, 2, 0>(); }
    constexpr Vector<T, 4> ggbg() const noexcept { return Swizzle<1, 1, 2, 1>(); }
    constexpr Vector<T, 4> ggbb() const noexcept { return Swizzle<1, 1, 2, 2>(); }
    constexpr Vector<T, 4> gbrr() const noexcept { return Swizzle<1, 2, 0, 0>(); }
    constexpr Vector<T, 4> gbrg() const noexcept { return Swizzle<1, 2, 0, 1>(); }
    constexpr Vector<T, 4> gbrb() const noexcept { return Swizzle<1, 2, 0, 2>(); }
    constexpr Vector<T, 4> gbgr() const noexcept { return Swizzle<1, 2, 1, 0>(); }
    constexpr Vector<T, 4> gbgg() const noexcept { return Swizzle<1, 2, 1, 1>(); }
    constexpr Vector<T, 4> gbgb() const noexcept { return Swizzle<1, 2, 1, 2>(); }
    constexpr Vector<T, 4> gbbr() const noexcept { return Swizzle<1, 2, 2, 0>(); }
    constexpr Vector<T, 4> gbbg() const noexcept { return Swizzle<1, 2, 2, 1>(); }
    constexpr Vector<T, 4> gbbb() const noexcept { return Swizzle<1, 2, 2, 2>(); }
    constexpr Vector<T, 4> brrr() const noexcept { return Swizzle<2, 0, 0, 0>(); }
    constexpr Vector<T, 4> brrg() const noexcept { return Swizzle<2, 0, 0, 1>(); }
    constexpr Vector<T, 4> brrb() const noexcept { return Swizzle<2, 0, 0, 2>(); }
    constexpr Vector<T, 4> brgr() const noexcept { return Swizzle<2, 0, 1, 0>(); }
    constexpr Vector<T, 4> brgg() const noexcept { return Swizzle<2, 0, 1, 1>(); }
    constexpr Vector<T, 4> brgb() const noexcept { return Swizzle<2, 0, 1, 2>(); }
    constexpr Vector<T, 4> brbr() const noexcept { return Swizzle<2, 0, 2, 0>(); }
    constexpr Vector<T, 4> brbg() const noexcept { return Swizzle<2, 0, 2, 1>(); }
    constexpr Vector<T, 4> brbb() const noexcept { return Swizzle<2, 0, 2, 2>(); }
    constexpr Vector<T, 4> bgrr() const noexcept { return Swizzle<2, 1, 0, 0>(); }
    constexpr Vector<T, 4> bgrg() const noexcept { return Swizzle<2, 1, 0, 1>(); }
    constexpr Vector<T, 4> bgrb() const noexcept { return Swizzle<2, 1, 0, 2>(); }
    constexpr Vector<T, 4> bggr() const noexcept { return Swizzle<2, 1, 1, 0>(); }
    constexpr Vector<T, 4> bggg() const noexcept { return Swizzle<2, 1, 1, 1>(); }
    constexpr Vector<T, 4> bggb() const noexcept { return Swizzle<2, 1, 1, 2>(); }
    constexpr Vector<T, 4> bgbr() const noexcept { return Swizzle<2, 1, 2, 0>(); }
    constexpr Vector<T, 4> bgbg() const noexcept { return Swizzle<2, 1, 2, 1>(); }
    constexpr Vector<T, 4> bgbb() const noexcept { return Swizzle<2, 1, 2, 2>(); }
    constexpr Vector<T, 4> bbrr() const noexcept { return Swizzle<2, 2, 0, 0>(); }
    constexpr Vector<T, 4> bbrg() const noexcept { return Swizzle<2, 2, 0, 1>(); }
    constexpr Vector<T, 4> bbrb() const noexcept { return Swizzle<2, 2, 0, 2>(); }
    constexpr Vector<T, 4> bbgr() const noexcept { return Swizzle<2, 2, 1, 0>(); }
    constexpr Vector<T, 4> bbgg() const noexcept { return Swizzle<2, 2, 1, 1>(); }
    constexpr Vector<T, 4> bbgb() const noexcept { return Swizzle<2, 2, 1, 2>(); }
    constexpr Vector<T, 4> bbbr() const noexcept { return Swizzle<2, 2, 2, 0>(); }
    constexpr Vector<T, 4> bbbg() const noexcept { return Swizzle<2, 2, 2, 1>(); }
    constexpr Vector<T, 4> bbbb() const noexcept { return Swizzle<2, 2, 2, 2>(); }


---

## 五、Vector&lt;T,4&gt; 展开结果（672 个方法）

可用分量 `{x=0, y=1, z=2, w=3}`，两套命名系（x 系 + rgba 系）。
2 维：16 个 / 系，3 维：64 个 / 系，4 维：256 个 / 系 → 每系 336 个，合计 672。

```cpp
    constexpr Vector<T, 2> xx() const noexcept { return Swizzle<0, 0>(); }
    constexpr Vector<T, 2> xy() const noexcept { return Swizzle<0, 1>(); }
    constexpr Vector<T, 2> xz() const noexcept { return Swizzle<0, 2>(); }
    constexpr Vector<T, 2> xw() const noexcept { return Swizzle<0, 3>(); }
    constexpr Vector<T, 2> yx() const noexcept { return Swizzle<1, 0>(); }
    constexpr Vector<T, 2> yy() const noexcept { return Swizzle<1, 1>(); }
    constexpr Vector<T, 2> yz() const noexcept { return Swizzle<1, 2>(); }
    constexpr Vector<T, 2> yw() const noexcept { return Swizzle<1, 3>(); }
    constexpr Vector<T, 2> zx() const noexcept { return Swizzle<2, 0>(); }
    constexpr Vector<T, 2> zy() const noexcept { return Swizzle<2, 1>(); }
    constexpr Vector<T, 2> zz() const noexcept { return Swizzle<2, 2>(); }
    constexpr Vector<T, 2> zw() const noexcept { return Swizzle<2, 3>(); }
    constexpr Vector<T, 2> wx() const noexcept { return Swizzle<3, 0>(); }
    constexpr Vector<T, 2> wy() const noexcept { return Swizzle<3, 1>(); }
    constexpr Vector<T, 2> wz() const noexcept { return Swizzle<3, 2>(); }
    constexpr Vector<T, 2> ww() const noexcept { return Swizzle<3, 3>(); }
    constexpr Vector<T, 3> xxx() const noexcept { return Swizzle<0, 0, 0>(); }
    constexpr Vector<T, 3> xxy() const noexcept { return Swizzle<0, 0, 1>(); }
    constexpr Vector<T, 3> xxz() const noexcept { return Swizzle<0, 0, 2>(); }
    constexpr Vector<T, 3> xxw() const noexcept { return Swizzle<0, 0, 3>(); }
    constexpr Vector<T, 3> xyx() const noexcept { return Swizzle<0, 1, 0>(); }
    constexpr Vector<T, 3> xyy() const noexcept { return Swizzle<0, 1, 1>(); }
    constexpr Vector<T, 3> xyz() const noexcept { return Swizzle<0, 1, 2>(); }
    constexpr Vector<T, 3> xyw() const noexcept { return Swizzle<0, 1, 3>(); }
    constexpr Vector<T, 3> xzx() const noexcept { return Swizzle<0, 2, 0>(); }
    constexpr Vector<T, 3> xzy() const noexcept { return Swizzle<0, 2, 1>(); }
    constexpr Vector<T, 3> xzz() const noexcept { return Swizzle<0, 2, 2>(); }
    constexpr Vector<T, 3> xzw() const noexcept { return Swizzle<0, 2, 3>(); }
    constexpr Vector<T, 3> xwx() const noexcept { return Swizzle<0, 3, 0>(); }
    constexpr Vector<T, 3> xwy() const noexcept { return Swizzle<0, 3, 1>(); }
    constexpr Vector<T, 3> xwz() const noexcept { return Swizzle<0, 3, 2>(); }
    constexpr Vector<T, 3> xww() const noexcept { return Swizzle<0, 3, 3>(); }
    constexpr Vector<T, 3> yxx() const noexcept { return Swizzle<1, 0, 0>(); }
    constexpr Vector<T, 3> yxy() const noexcept { return Swizzle<1, 0, 1>(); }
    constexpr Vector<T, 3> yxz() const noexcept { return Swizzle<1, 0, 2>(); }
    constexpr Vector<T, 3> yxw() const noexcept { return Swizzle<1, 0, 3>(); }
    constexpr Vector<T, 3> yyx() const noexcept { return Swizzle<1, 1, 0>(); }
    constexpr Vector<T, 3> yyy() const noexcept { return Swizzle<1, 1, 1>(); }
    constexpr Vector<T, 3> yyz() const noexcept { return Swizzle<1, 1, 2>(); }
    constexpr Vector<T, 3> yyw() const noexcept { return Swizzle<1, 1, 3>(); }
    constexpr Vector<T, 3> yzx() const noexcept { return Swizzle<1, 2, 0>(); }
    constexpr Vector<T, 3> yzy() const noexcept { return Swizzle<1, 2, 1>(); }
    constexpr Vector<T, 3> yzz() const noexcept { return Swizzle<1, 2, 2>(); }
    constexpr Vector<T, 3> yzw() const noexcept { return Swizzle<1, 2, 3>(); }
    constexpr Vector<T, 3> ywx() const noexcept { return Swizzle<1, 3, 0>(); }
    constexpr Vector<T, 3> ywy() const noexcept { return Swizzle<1, 3, 1>(); }
    constexpr Vector<T, 3> ywz() const noexcept { return Swizzle<1, 3, 2>(); }
    constexpr Vector<T, 3> yww() const noexcept { return Swizzle<1, 3, 3>(); }
    constexpr Vector<T, 3> zxx() const noexcept { return Swizzle<2, 0, 0>(); }
    constexpr Vector<T, 3> zxy() const noexcept { return Swizzle<2, 0, 1>(); }
    constexpr Vector<T, 3> zxz() const noexcept { return Swizzle<2, 0, 2>(); }
    constexpr Vector<T, 3> zxw() const noexcept { return Swizzle<2, 0, 3>(); }
    constexpr Vector<T, 3> zyx() const noexcept { return Swizzle<2, 1, 0>(); }
    constexpr Vector<T, 3> zyy() const noexcept { return Swizzle<2, 1, 1>(); }
    constexpr Vector<T, 3> zyz() const noexcept { return Swizzle<2, 1, 2>(); }
    constexpr Vector<T, 3> zyw() const noexcept { return Swizzle<2, 1, 3>(); }
    constexpr Vector<T, 3> zzx() const noexcept { return Swizzle<2, 2, 0>(); }
    constexpr Vector<T, 3> zzy() const noexcept { return Swizzle<2, 2, 1>(); }
    constexpr Vector<T, 3> zzz() const noexcept { return Swizzle<2, 2, 2>(); }
    constexpr Vector<T, 3> zzw() const noexcept { return Swizzle<2, 2, 3>(); }
    constexpr Vector<T, 3> zwx() const noexcept { return Swizzle<2, 3, 0>(); }
    constexpr Vector<T, 3> zwy() const noexcept { return Swizzle<2, 3, 1>(); }
    constexpr Vector<T, 3> zwz() const noexcept { return Swizzle<2, 3, 2>(); }
    constexpr Vector<T, 3> zww() const noexcept { return Swizzle<2, 3, 3>(); }
    constexpr Vector<T, 3> wxx() const noexcept { return Swizzle<3, 0, 0>(); }
    constexpr Vector<T, 3> wxy() const noexcept { return Swizzle<3, 0, 1>(); }
    constexpr Vector<T, 3> wxz() const noexcept { return Swizzle<3, 0, 2>(); }
    constexpr Vector<T, 3> wxw() const noexcept { return Swizzle<3, 0, 3>(); }
    constexpr Vector<T, 3> wyx() const noexcept { return Swizzle<3, 1, 0>(); }
    constexpr Vector<T, 3> wyy() const noexcept { return Swizzle<3, 1, 1>(); }
    constexpr Vector<T, 3> wyz() const noexcept { return Swizzle<3, 1, 2>(); }
    constexpr Vector<T, 3> wyw() const noexcept { return Swizzle<3, 1, 3>(); }
    constexpr Vector<T, 3> wzx() const noexcept { return Swizzle<3, 2, 0>(); }
    constexpr Vector<T, 3> wzy() const noexcept { return Swizzle<3, 2, 1>(); }
    constexpr Vector<T, 3> wzz() const noexcept { return Swizzle<3, 2, 2>(); }
    constexpr Vector<T, 3> wzw() const noexcept { return Swizzle<3, 2, 3>(); }
    constexpr Vector<T, 3> wwx() const noexcept { return Swizzle<3, 3, 0>(); }
    constexpr Vector<T, 3> wwy() const noexcept { return Swizzle<3, 3, 1>(); }
    constexpr Vector<T, 3> wwz() const noexcept { return Swizzle<3, 3, 2>(); }
    constexpr Vector<T, 3> www() const noexcept { return Swizzle<3, 3, 3>(); }
    constexpr Vector<T, 4> xxxx() const noexcept { return Swizzle<0, 0, 0, 0>(); }
    constexpr Vector<T, 4> xxxy() const noexcept { return Swizzle<0, 0, 0, 1>(); }
    constexpr Vector<T, 4> xxxz() const noexcept { return Swizzle<0, 0, 0, 2>(); }
    constexpr Vector<T, 4> xxxw() const noexcept { return Swizzle<0, 0, 0, 3>(); }
    constexpr Vector<T, 4> xxyx() const noexcept { return Swizzle<0, 0, 1, 0>(); }
    constexpr Vector<T, 4> xxyy() const noexcept { return Swizzle<0, 0, 1, 1>(); }
    constexpr Vector<T, 4> xxyz() const noexcept { return Swizzle<0, 0, 1, 2>(); }
    constexpr Vector<T, 4> xxyw() const noexcept { return Swizzle<0, 0, 1, 3>(); }
    constexpr Vector<T, 4> xxzx() const noexcept { return Swizzle<0, 0, 2, 0>(); }
    constexpr Vector<T, 4> xxzy() const noexcept { return Swizzle<0, 0, 2, 1>(); }
    constexpr Vector<T, 4> xxzz() const noexcept { return Swizzle<0, 0, 2, 2>(); }
    constexpr Vector<T, 4> xxzw() const noexcept { return Swizzle<0, 0, 2, 3>(); }
    constexpr Vector<T, 4> xxwx() const noexcept { return Swizzle<0, 0, 3, 0>(); }
    constexpr Vector<T, 4> xxwy() const noexcept { return Swizzle<0, 0, 3, 1>(); }
    constexpr Vector<T, 4> xxwz() const noexcept { return Swizzle<0, 0, 3, 2>(); }
    constexpr Vector<T, 4> xxww() const noexcept { return Swizzle<0, 0, 3, 3>(); }
    constexpr Vector<T, 4> xyxx() const noexcept { return Swizzle<0, 1, 0, 0>(); }
    constexpr Vector<T, 4> xyxy() const noexcept { return Swizzle<0, 1, 0, 1>(); }
    constexpr Vector<T, 4> xyxz() const noexcept { return Swizzle<0, 1, 0, 2>(); }
    constexpr Vector<T, 4> xyxw() const noexcept { return Swizzle<0, 1, 0, 3>(); }
    constexpr Vector<T, 4> xyyx() const noexcept { return Swizzle<0, 1, 1, 0>(); }
    constexpr Vector<T, 4> xyyy() const noexcept { return Swizzle<0, 1, 1, 1>(); }
    constexpr Vector<T, 4> xyyz() const noexcept { return Swizzle<0, 1, 1, 2>(); }
    constexpr Vector<T, 4> xyyw() const noexcept { return Swizzle<0, 1, 1, 3>(); }
    constexpr Vector<T, 4> xyzx() const noexcept { return Swizzle<0, 1, 2, 0>(); }
    constexpr Vector<T, 4> xyzy() const noexcept { return Swizzle<0, 1, 2, 1>(); }
    constexpr Vector<T, 4> xyzz() const noexcept { return Swizzle<0, 1, 2, 2>(); }
    constexpr Vector<T, 4> xyzw() const noexcept { return Swizzle<0, 1, 2, 3>(); }
    constexpr Vector<T, 4> xywx() const noexcept { return Swizzle<0, 1, 3, 0>(); }
    constexpr Vector<T, 4> xywy() const noexcept { return Swizzle<0, 1, 3, 1>(); }
    constexpr Vector<T, 4> xywz() const noexcept { return Swizzle<0, 1, 3, 2>(); }
    constexpr Vector<T, 4> xyww() const noexcept { return Swizzle<0, 1, 3, 3>(); }
    constexpr Vector<T, 4> xzxx() const noexcept { return Swizzle<0, 2, 0, 0>(); }
    constexpr Vector<T, 4> xzxy() const noexcept { return Swizzle<0, 2, 0, 1>(); }
    constexpr Vector<T, 4> xzxz() const noexcept { return Swizzle<0, 2, 0, 2>(); }
    constexpr Vector<T, 4> xzxw() const noexcept { return Swizzle<0, 2, 0, 3>(); }
    constexpr Vector<T, 4> xzyx() const noexcept { return Swizzle<0, 2, 1, 0>(); }
    constexpr Vector<T, 4> xzyy() const noexcept { return Swizzle<0, 2, 1, 1>(); }
    constexpr Vector<T, 4> xzyz() const noexcept { return Swizzle<0, 2, 1, 2>(); }
    constexpr Vector<T, 4> xzyw() const noexcept { return Swizzle<0, 2, 1, 3>(); }
    constexpr Vector<T, 4> xzzx() const noexcept { return Swizzle<0, 2, 2, 0>(); }
    constexpr Vector<T, 4> xzzy() const noexcept { return Swizzle<0, 2, 2, 1>(); }
    constexpr Vector<T, 4> xzzz() const noexcept { return Swizzle<0, 2, 2, 2>(); }
    constexpr Vector<T, 4> xzzw() const noexcept { return Swizzle<0, 2, 2, 3>(); }
    constexpr Vector<T, 4> xzwx() const noexcept { return Swizzle<0, 2, 3, 0>(); }
    constexpr Vector<T, 4> xzwy() const noexcept { return Swizzle<0, 2, 3, 1>(); }
    constexpr Vector<T, 4> xzwz() const noexcept { return Swizzle<0, 2, 3, 2>(); }
    constexpr Vector<T, 4> xzww() const noexcept { return Swizzle<0, 2, 3, 3>(); }
    constexpr Vector<T, 4> xwxx() const noexcept { return Swizzle<0, 3, 0, 0>(); }
    constexpr Vector<T, 4> xwxy() const noexcept { return Swizzle<0, 3, 0, 1>(); }
    constexpr Vector<T, 4> xwxz() const noexcept { return Swizzle<0, 3, 0, 2>(); }
    constexpr Vector<T, 4> xwxw() const noexcept { return Swizzle<0, 3, 0, 3>(); }
    constexpr Vector<T, 4> xwyx() const noexcept { return Swizzle<0, 3, 1, 0>(); }
    constexpr Vector<T, 4> xwyy() const noexcept { return Swizzle<0, 3, 1, 1>(); }
    constexpr Vector<T, 4> xwyz() const noexcept { return Swizzle<0, 3, 1, 2>(); }
    constexpr Vector<T, 4> xwyw() const noexcept { return Swizzle<0, 3, 1, 3>(); }
    constexpr Vector<T, 4> xwzx() const noexcept { return Swizzle<0, 3, 2, 0>(); }
    constexpr Vector<T, 4> xwzy() const noexcept { return Swizzle<0, 3, 2, 1>(); }
    constexpr Vector<T, 4> xwzz() const noexcept { return Swizzle<0, 3, 2, 2>(); }
    constexpr Vector<T, 4> xwzw() const noexcept { return Swizzle<0, 3, 2, 3>(); }
    constexpr Vector<T, 4> xwwx() const noexcept { return Swizzle<0, 3, 3, 0>(); }
    constexpr Vector<T, 4> xwwy() const noexcept { return Swizzle<0, 3, 3, 1>(); }
    constexpr Vector<T, 4> xwwz() const noexcept { return Swizzle<0, 3, 3, 2>(); }
    constexpr Vector<T, 4> xwww() const noexcept { return Swizzle<0, 3, 3, 3>(); }
    constexpr Vector<T, 4> yxxx() const noexcept { return Swizzle<1, 0, 0, 0>(); }
    constexpr Vector<T, 4> yxxy() const noexcept { return Swizzle<1, 0, 0, 1>(); }
    constexpr Vector<T, 4> yxxz() const noexcept { return Swizzle<1, 0, 0, 2>(); }
    constexpr Vector<T, 4> yxxw() const noexcept { return Swizzle<1, 0, 0, 3>(); }
    constexpr Vector<T, 4> yxyx() const noexcept { return Swizzle<1, 0, 1, 0>(); }
    constexpr Vector<T, 4> yxyy() const noexcept { return Swizzle<1, 0, 1, 1>(); }
    constexpr Vector<T, 4> yxyz() const noexcept { return Swizzle<1, 0, 1, 2>(); }
    constexpr Vector<T, 4> yxyw() const noexcept { return Swizzle<1, 0, 1, 3>(); }
    constexpr Vector<T, 4> yxzx() const noexcept { return Swizzle<1, 0, 2, 0>(); }
    constexpr Vector<T, 4> yxzy() const noexcept { return Swizzle<1, 0, 2, 1>(); }
    constexpr Vector<T, 4> yxzz() const noexcept { return Swizzle<1, 0, 2, 2>(); }
    constexpr Vector<T, 4> yxzw() const noexcept { return Swizzle<1, 0, 2, 3>(); }
    constexpr Vector<T, 4> yxwx() const noexcept { return Swizzle<1, 0, 3, 0>(); }
    constexpr Vector<T, 4> yxwy() const noexcept { return Swizzle<1, 0, 3, 1>(); }
    constexpr Vector<T, 4> yxwz() const noexcept { return Swizzle<1, 0, 3, 2>(); }
    constexpr Vector<T, 4> yxww() const noexcept { return Swizzle<1, 0, 3, 3>(); }
    constexpr Vector<T, 4> yyxx() const noexcept { return Swizzle<1, 1, 0, 0>(); }
    constexpr Vector<T, 4> yyxy() const noexcept { return Swizzle<1, 1, 0, 1>(); }
    constexpr Vector<T, 4> yyxz() const noexcept { return Swizzle<1, 1, 0, 2>(); }
    constexpr Vector<T, 4> yyxw() const noexcept { return Swizzle<1, 1, 0, 3>(); }
    constexpr Vector<T, 4> yyyx() const noexcept { return Swizzle<1, 1, 1, 0>(); }
    constexpr Vector<T, 4> yyyy() const noexcept { return Swizzle<1, 1, 1, 1>(); }
    constexpr Vector<T, 4> yyyz() const noexcept { return Swizzle<1, 1, 1, 2>(); }
    constexpr Vector<T, 4> yyyw() const noexcept { return Swizzle<1, 1, 1, 3>(); }
    constexpr Vector<T, 4> yyzx() const noexcept { return Swizzle<1, 1, 2, 0>(); }
    constexpr Vector<T, 4> yyzy() const noexcept { return Swizzle<1, 1, 2, 1>(); }
    constexpr Vector<T, 4> yyzz() const noexcept { return Swizzle<1, 1, 2, 2>(); }
    constexpr Vector<T, 4> yyzw() const noexcept { return Swizzle<1, 1, 2, 3>(); }
    constexpr Vector<T, 4> yywx() const noexcept { return Swizzle<1, 1, 3, 0>(); }
    constexpr Vector<T, 4> yywy() const noexcept { return Swizzle<1, 1, 3, 1>(); }
    constexpr Vector<T, 4> yywz() const noexcept { return Swizzle<1, 1, 3, 2>(); }
    constexpr Vector<T, 4> yyww() const noexcept { return Swizzle<1, 1, 3, 3>(); }
    constexpr Vector<T, 4> yzxx() const noexcept { return Swizzle<1, 2, 0, 0>(); }
    constexpr Vector<T, 4> yzxy() const noexcept { return Swizzle<1, 2, 0, 1>(); }
    constexpr Vector<T, 4> yzxz() const noexcept { return Swizzle<1, 2, 0, 2>(); }
    constexpr Vector<T, 4> yzxw() const noexcept { return Swizzle<1, 2, 0, 3>(); }
    constexpr Vector<T, 4> yzyx() const noexcept { return Swizzle<1, 2, 1, 0>(); }
    constexpr Vector<T, 4> yzyy() const noexcept { return Swizzle<1, 2, 1, 1>(); }
    constexpr Vector<T, 4> yzyz() const noexcept { return Swizzle<1, 2, 1, 2>(); }
    constexpr Vector<T, 4> yzyw() const noexcept { return Swizzle<1, 2, 1, 3>(); }
    constexpr Vector<T, 4> yzzx() const noexcept { return Swizzle<1, 2, 2, 0>(); }
    constexpr Vector<T, 4> yzzy() const noexcept { return Swizzle<1, 2, 2, 1>(); }
    constexpr Vector<T, 4> yzzz() const noexcept { return Swizzle<1, 2, 2, 2>(); }
    constexpr Vector<T, 4> yzzw() const noexcept { return Swizzle<1, 2, 2, 3>(); }
    constexpr Vector<T, 4> yzwx() const noexcept { return Swizzle<1, 2, 3, 0>(); }
    constexpr Vector<T, 4> yzwy() const noexcept { return Swizzle<1, 2, 3, 1>(); }
    constexpr Vector<T, 4> yzwz() const noexcept { return Swizzle<1, 2, 3, 2>(); }
    constexpr Vector<T, 4> yzww() const noexcept { return Swizzle<1, 2, 3, 3>(); }
    constexpr Vector<T, 4> ywxx() const noexcept { return Swizzle<1, 3, 0, 0>(); }
    constexpr Vector<T, 4> ywxy() const noexcept { return Swizzle<1, 3, 0, 1>(); }
    constexpr Vector<T, 4> ywxz() const noexcept { return Swizzle<1, 3, 0, 2>(); }
    constexpr Vector<T, 4> ywxw() const noexcept { return Swizzle<1, 3, 0, 3>(); }
    constexpr Vector<T, 4> ywyx() const noexcept { return Swizzle<1, 3, 1, 0>(); }
    constexpr Vector<T, 4> ywyy() const noexcept { return Swizzle<1, 3, 1, 1>(); }
    constexpr Vector<T, 4> ywyz() const noexcept { return Swizzle<1, 3, 1, 2>(); }
    constexpr Vector<T, 4> ywyw() const noexcept { return Swizzle<1, 3, 1, 3>(); }
    constexpr Vector<T, 4> ywzx() const noexcept { return Swizzle<1, 3, 2, 0>(); }
    constexpr Vector<T, 4> ywzy() const noexcept { return Swizzle<1, 3, 2, 1>(); }
    constexpr Vector<T, 4> ywzz() const noexcept { return Swizzle<1, 3, 2, 2>(); }
    constexpr Vector<T, 4> ywzw() const noexcept { return Swizzle<1, 3, 2, 3>(); }
    constexpr Vector<T, 4> ywwx() const noexcept { return Swizzle<1, 3, 3, 0>(); }
    constexpr Vector<T, 4> ywwy() const noexcept { return Swizzle<1, 3, 3, 1>(); }
    constexpr Vector<T, 4> ywwz() const noexcept { return Swizzle<1, 3, 3, 2>(); }
    constexpr Vector<T, 4> ywww() const noexcept { return Swizzle<1, 3, 3, 3>(); }
    constexpr Vector<T, 4> zxxx() const noexcept { return Swizzle<2, 0, 0, 0>(); }
    constexpr Vector<T, 4> zxxy() const noexcept { return Swizzle<2, 0, 0, 1>(); }
    constexpr Vector<T, 4> zxxz() const noexcept { return Swizzle<2, 0, 0, 2>(); }
    constexpr Vector<T, 4> zxxw() const noexcept { return Swizzle<2, 0, 0, 3>(); }
    constexpr Vector<T, 4> zxyx() const noexcept { return Swizzle<2, 0, 1, 0>(); }
    constexpr Vector<T, 4> zxyy() const noexcept { return Swizzle<2, 0, 1, 1>(); }
    constexpr Vector<T, 4> zxyz() const noexcept { return Swizzle<2, 0, 1, 2>(); }
    constexpr Vector<T, 4> zxyw() const noexcept { return Swizzle<2, 0, 1, 3>(); }
    constexpr Vector<T, 4> zxzx() const noexcept { return Swizzle<2, 0, 2, 0>(); }
    constexpr Vector<T, 4> zxzy() const noexcept { return Swizzle<2, 0, 2, 1>(); }
    constexpr Vector<T, 4> zxzz() const noexcept { return Swizzle<2, 0, 2, 2>(); }
    constexpr Vector<T, 4> zxzw() const noexcept { return Swizzle<2, 0, 2, 3>(); }
    constexpr Vector<T, 4> zxwx() const noexcept { return Swizzle<2, 0, 3, 0>(); }
    constexpr Vector<T, 4> zxwy() const noexcept { return Swizzle<2, 0, 3, 1>(); }
    constexpr Vector<T, 4> zxwz() const noexcept { return Swizzle<2, 0, 3, 2>(); }
    constexpr Vector<T, 4> zxww() const noexcept { return Swizzle<2, 0, 3, 3>(); }
    constexpr Vector<T, 4> zyxx() const noexcept { return Swizzle<2, 1, 0, 0>(); }
    constexpr Vector<T, 4> zyxy() const noexcept { return Swizzle<2, 1, 0, 1>(); }
    constexpr Vector<T, 4> zyxz() const noexcept { return Swizzle<2, 1, 0, 2>(); }
    constexpr Vector<T, 4> zyxw() const noexcept { return Swizzle<2, 1, 0, 3>(); }
    constexpr Vector<T, 4> zyyx() const noexcept { return Swizzle<2, 1, 1, 0>(); }
    constexpr Vector<T, 4> zyyy() const noexcept { return Swizzle<2, 1, 1, 1>(); }
    constexpr Vector<T, 4> zyyz() const noexcept { return Swizzle<2, 1, 1, 2>(); }
    constexpr Vector<T, 4> zyyw() const noexcept { return Swizzle<2, 1, 1, 3>(); }
    constexpr Vector<T, 4> zyzx() const noexcept { return Swizzle<2, 1, 2, 0>(); }
    constexpr Vector<T, 4> zyzy() const noexcept { return Swizzle<2, 1, 2, 1>(); }
    constexpr Vector<T, 4> zyzz() const noexcept { return Swizzle<2, 1, 2, 2>(); }
    constexpr Vector<T, 4> zyzw() const noexcept { return Swizzle<2, 1, 2, 3>(); }
    constexpr Vector<T, 4> zywx() const noexcept { return Swizzle<2, 1, 3, 0>(); }
    constexpr Vector<T, 4> zywy() const noexcept { return Swizzle<2, 1, 3, 1>(); }
    constexpr Vector<T, 4> zywz() const noexcept { return Swizzle<2, 1, 3, 2>(); }
    constexpr Vector<T, 4> zyww() const noexcept { return Swizzle<2, 1, 3, 3>(); }
    constexpr Vector<T, 4> zzxx() const noexcept { return Swizzle<2, 2, 0, 0>(); }
    constexpr Vector<T, 4> zzxy() const noexcept { return Swizzle<2, 2, 0, 1>(); }
    constexpr Vector<T, 4> zzxz() const noexcept { return Swizzle<2, 2, 0, 2>(); }
    constexpr Vector<T, 4> zzxw() const noexcept { return Swizzle<2, 2, 0, 3>(); }
    constexpr Vector<T, 4> zzyx() const noexcept { return Swizzle<2, 2, 1, 0>(); }
    constexpr Vector<T, 4> zzyy() const noexcept { return Swizzle<2, 2, 1, 1>(); }
    constexpr Vector<T, 4> zzyz() const noexcept { return Swizzle<2, 2, 1, 2>(); }
    constexpr Vector<T, 4> zzyw() const noexcept { return Swizzle<2, 2, 1, 3>(); }
    constexpr Vector<T, 4> zzzx() const noexcept { return Swizzle<2, 2, 2, 0>(); }
    constexpr Vector<T, 4> zzzy() const noexcept { return Swizzle<2, 2, 2, 1>(); }
    constexpr Vector<T, 4> zzzz() const noexcept { return Swizzle<2, 2, 2, 2>(); }
    constexpr Vector<T, 4> zzzw() const noexcept { return Swizzle<2, 2, 2, 3>(); }
    constexpr Vector<T, 4> zzwx() const noexcept { return Swizzle<2, 2, 3, 0>(); }
    constexpr Vector<T, 4> zzwy() const noexcept { return Swizzle<2, 2, 3, 1>(); }
    constexpr Vector<T, 4> zzwz() const noexcept { return Swizzle<2, 2, 3, 2>(); }
    constexpr Vector<T, 4> zzww() const noexcept { return Swizzle<2, 2, 3, 3>(); }
    constexpr Vector<T, 4> zwxx() const noexcept { return Swizzle<2, 3, 0, 0>(); }
    constexpr Vector<T, 4> zwxy() const noexcept { return Swizzle<2, 3, 0, 1>(); }
    constexpr Vector<T, 4> zwxz() const noexcept { return Swizzle<2, 3, 0, 2>(); }
    constexpr Vector<T, 4> zwxw() const noexcept { return Swizzle<2, 3, 0, 3>(); }
    constexpr Vector<T, 4> zwyx() const noexcept { return Swizzle<2, 3, 1, 0>(); }
    constexpr Vector<T, 4> zwyy() const noexcept { return Swizzle<2, 3, 1, 1>(); }
    constexpr Vector<T, 4> zwyz() const noexcept { return Swizzle<2, 3, 1, 2>(); }
    constexpr Vector<T, 4> zwyw() const noexcept { return Swizzle<2, 3, 1, 3>(); }
    constexpr Vector<T, 4> zwzx() const noexcept { return Swizzle<2, 3, 2, 0>(); }
    constexpr Vector<T, 4> zwzy() const noexcept { return Swizzle<2, 3, 2, 1>(); }
    constexpr Vector<T, 4> zwzz() const noexcept { return Swizzle<2, 3, 2, 2>(); }
    constexpr Vector<T, 4> zwzw() const noexcept { return Swizzle<2, 3, 2, 3>(); }
    constexpr Vector<T, 4> zwwx() const noexcept { return Swizzle<2, 3, 3, 0>(); }
    constexpr Vector<T, 4> zwwy() const noexcept { return Swizzle<2, 3, 3, 1>(); }
    constexpr Vector<T, 4> zwwz() const noexcept { return Swizzle<2, 3, 3, 2>(); }
    constexpr Vector<T, 4> zwww() const noexcept { return Swizzle<2, 3, 3, 3>(); }
    constexpr Vector<T, 4> wxxx() const noexcept { return Swizzle<3, 0, 0, 0>(); }
    constexpr Vector<T, 4> wxxy() const noexcept { return Swizzle<3, 0, 0, 1>(); }
    constexpr Vector<T, 4> wxxz() const noexcept { return Swizzle<3, 0, 0, 2>(); }
    constexpr Vector<T, 4> wxxw() const noexcept { return Swizzle<3, 0, 0, 3>(); }
    constexpr Vector<T, 4> wxyx() const noexcept { return Swizzle<3, 0, 1, 0>(); }
    constexpr Vector<T, 4> wxyy() const noexcept { return Swizzle<3, 0, 1, 1>(); }
    constexpr Vector<T, 4> wxyz() const noexcept { return Swizzle<3, 0, 1, 2>(); }
    constexpr Vector<T, 4> wxyw() const noexcept { return Swizzle<3, 0, 1, 3>(); }
    constexpr Vector<T, 4> wxzx() const noexcept { return Swizzle<3, 0, 2, 0>(); }
    constexpr Vector<T, 4> wxzy() const noexcept { return Swizzle<3, 0, 2, 1>(); }
    constexpr Vector<T, 4> wxzz() const noexcept { return Swizzle<3, 0, 2, 2>(); }
    constexpr Vector<T, 4> wxzw() const noexcept { return Swizzle<3, 0, 2, 3>(); }
    constexpr Vector<T, 4> wxwx() const noexcept { return Swizzle<3, 0, 3, 0>(); }
    constexpr Vector<T, 4> wxwy() const noexcept { return Swizzle<3, 0, 3, 1>(); }
    constexpr Vector<T, 4> wxwz() const noexcept { return Swizzle<3, 0, 3, 2>(); }
    constexpr Vector<T, 4> wxww() const noexcept { return Swizzle<3, 0, 3, 3>(); }
    constexpr Vector<T, 4> wyxx() const noexcept { return Swizzle<3, 1, 0, 0>(); }
    constexpr Vector<T, 4> wyxy() const noexcept { return Swizzle<3, 1, 0, 1>(); }
    constexpr Vector<T, 4> wyxz() const noexcept { return Swizzle<3, 1, 0, 2>(); }
    constexpr Vector<T, 4> wyxw() const noexcept { return Swizzle<3, 1, 0, 3>(); }
    constexpr Vector<T, 4> wyyx() const noexcept { return Swizzle<3, 1, 1, 0>(); }
    constexpr Vector<T, 4> wyyy() const noexcept { return Swizzle<3, 1, 1, 1>(); }
    constexpr Vector<T, 4> wyyz() const noexcept { return Swizzle<3, 1, 1, 2>(); }
    constexpr Vector<T, 4> wyyw() const noexcept { return Swizzle<3, 1, 1, 3>(); }
    constexpr Vector<T, 4> wyzx() const noexcept { return Swizzle<3, 1, 2, 0>(); }
    constexpr Vector<T, 4> wyzy() const noexcept { return Swizzle<3, 1, 2, 1>(); }
    constexpr Vector<T, 4> wyzz() const noexcept { return Swizzle<3, 1, 2, 2>(); }
    constexpr Vector<T, 4> wyzw() const noexcept { return Swizzle<3, 1, 2, 3>(); }
    constexpr Vector<T, 4> wywx() const noexcept { return Swizzle<3, 1, 3, 0>(); }
    constexpr Vector<T, 4> wywy() const noexcept { return Swizzle<3, 1, 3, 1>(); }
    constexpr Vector<T, 4> wywz() const noexcept { return Swizzle<3, 1, 3, 2>(); }
    constexpr Vector<T, 4> wyww() const noexcept { return Swizzle<3, 1, 3, 3>(); }
    constexpr Vector<T, 4> wzxx() const noexcept { return Swizzle<3, 2, 0, 0>(); }
    constexpr Vector<T, 4> wzxy() const noexcept { return Swizzle<3, 2, 0, 1>(); }
    constexpr Vector<T, 4> wzxz() const noexcept { return Swizzle<3, 2, 0, 2>(); }
    constexpr Vector<T, 4> wzxw() const noexcept { return Swizzle<3, 2, 0, 3>(); }
    constexpr Vector<T, 4> wzyx() const noexcept { return Swizzle<3, 2, 1, 0>(); }
    constexpr Vector<T, 4> wzyy() const noexcept { return Swizzle<3, 2, 1, 1>(); }
    constexpr Vector<T, 4> wzyz() const noexcept { return Swizzle<3, 2, 1, 2>(); }
    constexpr Vector<T, 4> wzyw() const noexcept { return Swizzle<3, 2, 1, 3>(); }
    constexpr Vector<T, 4> wzzx() const noexcept { return Swizzle<3, 2, 2, 0>(); }
    constexpr Vector<T, 4> wzzy() const noexcept { return Swizzle<3, 2, 2, 1>(); }
    constexpr Vector<T, 4> wzzz() const noexcept { return Swizzle<3, 2, 2, 2>(); }
    constexpr Vector<T, 4> wzzw() const noexcept { return Swizzle<3, 2, 2, 3>(); }
    constexpr Vector<T, 4> wzwx() const noexcept { return Swizzle<3, 2, 3, 0>(); }
    constexpr Vector<T, 4> wzwy() const noexcept { return Swizzle<3, 2, 3, 1>(); }
    constexpr Vector<T, 4> wzwz() const noexcept { return Swizzle<3, 2, 3, 2>(); }
    constexpr Vector<T, 4> wzww() const noexcept { return Swizzle<3, 2, 3, 3>(); }
    constexpr Vector<T, 4> wwxx() const noexcept { return Swizzle<3, 3, 0, 0>(); }
    constexpr Vector<T, 4> wwxy() const noexcept { return Swizzle<3, 3, 0, 1>(); }
    constexpr Vector<T, 4> wwxz() const noexcept { return Swizzle<3, 3, 0, 2>(); }
    constexpr Vector<T, 4> wwxw() const noexcept { return Swizzle<3, 3, 0, 3>(); }
    constexpr Vector<T, 4> wwyx() const noexcept { return Swizzle<3, 3, 1, 0>(); }
    constexpr Vector<T, 4> wwyy() const noexcept { return Swizzle<3, 3, 1, 1>(); }
    constexpr Vector<T, 4> wwyz() const noexcept { return Swizzle<3, 3, 1, 2>(); }
    constexpr Vector<T, 4> wwyw() const noexcept { return Swizzle<3, 3, 1, 3>(); }
    constexpr Vector<T, 4> wwzx() const noexcept { return Swizzle<3, 3, 2, 0>(); }
    constexpr Vector<T, 4> wwzy() const noexcept { return Swizzle<3, 3, 2, 1>(); }
    constexpr Vector<T, 4> wwzz() const noexcept { return Swizzle<3, 3, 2, 2>(); }
    constexpr Vector<T, 4> wwzw() const noexcept { return Swizzle<3, 3, 2, 3>(); }
    constexpr Vector<T, 4> wwwx() const noexcept { return Swizzle<3, 3, 3, 0>(); }
    constexpr Vector<T, 4> wwwy() const noexcept { return Swizzle<3, 3, 3, 1>(); }
    constexpr Vector<T, 4> wwwz() const noexcept { return Swizzle<3, 3, 3, 2>(); }
    constexpr Vector<T, 4> wwww() const noexcept { return Swizzle<3, 3, 3, 3>(); }
    constexpr Vector<T, 2> rr() const noexcept { return Swizzle<0, 0>(); }
    constexpr Vector<T, 2> rg() const noexcept { return Swizzle<0, 1>(); }
    constexpr Vector<T, 2> rb() const noexcept { return Swizzle<0, 2>(); }
    constexpr Vector<T, 2> ra() const noexcept { return Swizzle<0, 3>(); }
    constexpr Vector<T, 2> gr() const noexcept { return Swizzle<1, 0>(); }
    constexpr Vector<T, 2> gg() const noexcept { return Swizzle<1, 1>(); }
    constexpr Vector<T, 2> gb() const noexcept { return Swizzle<1, 2>(); }
    constexpr Vector<T, 2> ga() const noexcept { return Swizzle<1, 3>(); }
    constexpr Vector<T, 2> br() const noexcept { return Swizzle<2, 0>(); }
    constexpr Vector<T, 2> bg() const noexcept { return Swizzle<2, 1>(); }
    constexpr Vector<T, 2> bb() const noexcept { return Swizzle<2, 2>(); }
    constexpr Vector<T, 2> ba() const noexcept { return Swizzle<2, 3>(); }
    constexpr Vector<T, 2> ar() const noexcept { return Swizzle<3, 0>(); }
    constexpr Vector<T, 2> ag() const noexcept { return Swizzle<3, 1>(); }
    constexpr Vector<T, 2> ab() const noexcept { return Swizzle<3, 2>(); }
    constexpr Vector<T, 2> aa() const noexcept { return Swizzle<3, 3>(); }
    constexpr Vector<T, 3> rrr() const noexcept { return Swizzle<0, 0, 0>(); }
    constexpr Vector<T, 3> rrg() const noexcept { return Swizzle<0, 0, 1>(); }
    constexpr Vector<T, 3> rrb() const noexcept { return Swizzle<0, 0, 2>(); }
    constexpr Vector<T, 3> rra() const noexcept { return Swizzle<0, 0, 3>(); }
    constexpr Vector<T, 3> rgr() const noexcept { return Swizzle<0, 1, 0>(); }
    constexpr Vector<T, 3> rgg() const noexcept { return Swizzle<0, 1, 1>(); }
    constexpr Vector<T, 3> rgb() const noexcept { return Swizzle<0, 1, 2>(); }
    constexpr Vector<T, 3> rga() const noexcept { return Swizzle<0, 1, 3>(); }
    constexpr Vector<T, 3> rbr() const noexcept { return Swizzle<0, 2, 0>(); }
    constexpr Vector<T, 3> rbg() const noexcept { return Swizzle<0, 2, 1>(); }
    constexpr Vector<T, 3> rbb() const noexcept { return Swizzle<0, 2, 2>(); }
    constexpr Vector<T, 3> rba() const noexcept { return Swizzle<0, 2, 3>(); }
    constexpr Vector<T, 3> rar() const noexcept { return Swizzle<0, 3, 0>(); }
    constexpr Vector<T, 3> rag() const noexcept { return Swizzle<0, 3, 1>(); }
    constexpr Vector<T, 3> rab() const noexcept { return Swizzle<0, 3, 2>(); }
    constexpr Vector<T, 3> raa() const noexcept { return Swizzle<0, 3, 3>(); }
    constexpr Vector<T, 3> grr() const noexcept { return Swizzle<1, 0, 0>(); }
    constexpr Vector<T, 3> grg() const noexcept { return Swizzle<1, 0, 1>(); }
    constexpr Vector<T, 3> grb() const noexcept { return Swizzle<1, 0, 2>(); }
    constexpr Vector<T, 3> gra() const noexcept { return Swizzle<1, 0, 3>(); }
    constexpr Vector<T, 3> ggr() const noexcept { return Swizzle<1, 1, 0>(); }
    constexpr Vector<T, 3> ggg() const noexcept { return Swizzle<1, 1, 1>(); }
    constexpr Vector<T, 3> ggb() const noexcept { return Swizzle<1, 1, 2>(); }
    constexpr Vector<T, 3> gga() const noexcept { return Swizzle<1, 1, 3>(); }
    constexpr Vector<T, 3> gbr() const noexcept { return Swizzle<1, 2, 0>(); }
    constexpr Vector<T, 3> gbg() const noexcept { return Swizzle<1, 2, 1>(); }
    constexpr Vector<T, 3> gbb() const noexcept { return Swizzle<1, 2, 2>(); }
    constexpr Vector<T, 3> gba() const noexcept { return Swizzle<1, 2, 3>(); }
    constexpr Vector<T, 3> gar() const noexcept { return Swizzle<1, 3, 0>(); }
    constexpr Vector<T, 3> gag() const noexcept { return Swizzle<1, 3, 1>(); }
    constexpr Vector<T, 3> gab() const noexcept { return Swizzle<1, 3, 2>(); }
    constexpr Vector<T, 3> gaa() const noexcept { return Swizzle<1, 3, 3>(); }
    constexpr Vector<T, 3> brr() const noexcept { return Swizzle<2, 0, 0>(); }
    constexpr Vector<T, 3> brg() const noexcept { return Swizzle<2, 0, 1>(); }
    constexpr Vector<T, 3> brb() const noexcept { return Swizzle<2, 0, 2>(); }
    constexpr Vector<T, 3> bra() const noexcept { return Swizzle<2, 0, 3>(); }
    constexpr Vector<T, 3> bgr() const noexcept { return Swizzle<2, 1, 0>(); }
    constexpr Vector<T, 3> bgg() const noexcept { return Swizzle<2, 1, 1>(); }
    constexpr Vector<T, 3> bgb() const noexcept { return Swizzle<2, 1, 2>(); }
    constexpr Vector<T, 3> bga() const noexcept { return Swizzle<2, 1, 3>(); }
    constexpr Vector<T, 3> bbr() const noexcept { return Swizzle<2, 2, 0>(); }
    constexpr Vector<T, 3> bbg() const noexcept { return Swizzle<2, 2, 1>(); }
    constexpr Vector<T, 3> bbb() const noexcept { return Swizzle<2, 2, 2>(); }
    constexpr Vector<T, 3> bba() const noexcept { return Swizzle<2, 2, 3>(); }
    constexpr Vector<T, 3> bar() const noexcept { return Swizzle<2, 3, 0>(); }
    constexpr Vector<T, 3> bag() const noexcept { return Swizzle<2, 3, 1>(); }
    constexpr Vector<T, 3> bab() const noexcept { return Swizzle<2, 3, 2>(); }
    constexpr Vector<T, 3> baa() const noexcept { return Swizzle<2, 3, 3>(); }
    constexpr Vector<T, 3> arr() const noexcept { return Swizzle<3, 0, 0>(); }
    constexpr Vector<T, 3> arg() const noexcept { return Swizzle<3, 0, 1>(); }
    constexpr Vector<T, 3> arb() const noexcept { return Swizzle<3, 0, 2>(); }
    constexpr Vector<T, 3> ara() const noexcept { return Swizzle<3, 0, 3>(); }
    constexpr Vector<T, 3> agr() const noexcept { return Swizzle<3, 1, 0>(); }
    constexpr Vector<T, 3> agg() const noexcept { return Swizzle<3, 1, 1>(); }
    constexpr Vector<T, 3> agb() const noexcept { return Swizzle<3, 1, 2>(); }
    constexpr Vector<T, 3> aga() const noexcept { return Swizzle<3, 1, 3>(); }
    constexpr Vector<T, 3> abr() const noexcept { return Swizzle<3, 2, 0>(); }
    constexpr Vector<T, 3> abg() const noexcept { return Swizzle<3, 2, 1>(); }
    constexpr Vector<T, 3> abb() const noexcept { return Swizzle<3, 2, 2>(); }
    constexpr Vector<T, 3> aba() const noexcept { return Swizzle<3, 2, 3>(); }
    constexpr Vector<T, 3> aar() const noexcept { return Swizzle<3, 3, 0>(); }
    constexpr Vector<T, 3> aag() const noexcept { return Swizzle<3, 3, 1>(); }
    constexpr Vector<T, 3> aab() const noexcept { return Swizzle<3, 3, 2>(); }
    constexpr Vector<T, 3> aaa() const noexcept { return Swizzle<3, 3, 3>(); }
    constexpr Vector<T, 4> rrrr() const noexcept { return Swizzle<0, 0, 0, 0>(); }
    constexpr Vector<T, 4> rrrg() const noexcept { return Swizzle<0, 0, 0, 1>(); }
    constexpr Vector<T, 4> rrrb() const noexcept { return Swizzle<0, 0, 0, 2>(); }
    constexpr Vector<T, 4> rrra() const noexcept { return Swizzle<0, 0, 0, 3>(); }
    constexpr Vector<T, 4> rrgr() const noexcept { return Swizzle<0, 0, 1, 0>(); }
    constexpr Vector<T, 4> rrgg() const noexcept { return Swizzle<0, 0, 1, 1>(); }
    constexpr Vector<T, 4> rrgb() const noexcept { return Swizzle<0, 0, 1, 2>(); }
    constexpr Vector<T, 4> rrga() const noexcept { return Swizzle<0, 0, 1, 3>(); }
    constexpr Vector<T, 4> rrbr() const noexcept { return Swizzle<0, 0, 2, 0>(); }
    constexpr Vector<T, 4> rrbg() const noexcept { return Swizzle<0, 0, 2, 1>(); }
    constexpr Vector<T, 4> rrbb() const noexcept { return Swizzle<0, 0, 2, 2>(); }
    constexpr Vector<T, 4> rrba() const noexcept { return Swizzle<0, 0, 2, 3>(); }
    constexpr Vector<T, 4> rrar() const noexcept { return Swizzle<0, 0, 3, 0>(); }
    constexpr Vector<T, 4> rrag() const noexcept { return Swizzle<0, 0, 3, 1>(); }
    constexpr Vector<T, 4> rrab() const noexcept { return Swizzle<0, 0, 3, 2>(); }
    constexpr Vector<T, 4> rraa() const noexcept { return Swizzle<0, 0, 3, 3>(); }
    constexpr Vector<T, 4> rgrr() const noexcept { return Swizzle<0, 1, 0, 0>(); }
    constexpr Vector<T, 4> rgrg() const noexcept { return Swizzle<0, 1, 0, 1>(); }
    constexpr Vector<T, 4> rgrb() const noexcept { return Swizzle<0, 1, 0, 2>(); }
    constexpr Vector<T, 4> rgra() const noexcept { return Swizzle<0, 1, 0, 3>(); }
    constexpr Vector<T, 4> rggr() const noexcept { return Swizzle<0, 1, 1, 0>(); }
    constexpr Vector<T, 4> rggg() const noexcept { return Swizzle<0, 1, 1, 1>(); }
    constexpr Vector<T, 4> rggb() const noexcept { return Swizzle<0, 1, 1, 2>(); }
    constexpr Vector<T, 4> rgga() const noexcept { return Swizzle<0, 1, 1, 3>(); }
    constexpr Vector<T, 4> rgbr() const noexcept { return Swizzle<0, 1, 2, 0>(); }
    constexpr Vector<T, 4> rgbg() const noexcept { return Swizzle<0, 1, 2, 1>(); }
    constexpr Vector<T, 4> rgbb() const noexcept { return Swizzle<0, 1, 2, 2>(); }
    constexpr Vector<T, 4> rgba() const noexcept { return Swizzle<0, 1, 2, 3>(); }
    constexpr Vector<T, 4> rgar() const noexcept { return Swizzle<0, 1, 3, 0>(); }
    constexpr Vector<T, 4> rgag() const noexcept { return Swizzle<0, 1, 3, 1>(); }
    constexpr Vector<T, 4> rgab() const noexcept { return Swizzle<0, 1, 3, 2>(); }
    constexpr Vector<T, 4> rgaa() const noexcept { return Swizzle<0, 1, 3, 3>(); }
    constexpr Vector<T, 4> rbrr() const noexcept { return Swizzle<0, 2, 0, 0>(); }
    constexpr Vector<T, 4> rbrg() const noexcept { return Swizzle<0, 2, 0, 1>(); }
    constexpr Vector<T, 4> rbrb() const noexcept { return Swizzle<0, 2, 0, 2>(); }
    constexpr Vector<T, 4> rbra() const noexcept { return Swizzle<0, 2, 0, 3>(); }
    constexpr Vector<T, 4> rbgr() const noexcept { return Swizzle<0, 2, 1, 0>(); }
    constexpr Vector<T, 4> rbgg() const noexcept { return Swizzle<0, 2, 1, 1>(); }
    constexpr Vector<T, 4> rbgb() const noexcept { return Swizzle<0, 2, 1, 2>(); }
    constexpr Vector<T, 4> rbga() const noexcept { return Swizzle<0, 2, 1, 3>(); }
    constexpr Vector<T, 4> rbbr() const noexcept { return Swizzle<0, 2, 2, 0>(); }
    constexpr Vector<T, 4> rbbg() const noexcept { return Swizzle<0, 2, 2, 1>(); }
    constexpr Vector<T, 4> rbbb() const noexcept { return Swizzle<0, 2, 2, 2>(); }
    constexpr Vector<T, 4> rbba() const noexcept { return Swizzle<0, 2, 2, 3>(); }
    constexpr Vector<T, 4> rbar() const noexcept { return Swizzle<0, 2, 3, 0>(); }
    constexpr Vector<T, 4> rbag() const noexcept { return Swizzle<0, 2, 3, 1>(); }
    constexpr Vector<T, 4> rbab() const noexcept { return Swizzle<0, 2, 3, 2>(); }
    constexpr Vector<T, 4> rbaa() const noexcept { return Swizzle<0, 2, 3, 3>(); }
    constexpr Vector<T, 4> rarr() const noexcept { return Swizzle<0, 3, 0, 0>(); }
    constexpr Vector<T, 4> rarg() const noexcept { return Swizzle<0, 3, 0, 1>(); }
    constexpr Vector<T, 4> rarb() const noexcept { return Swizzle<0, 3, 0, 2>(); }
    constexpr Vector<T, 4> rara() const noexcept { return Swizzle<0, 3, 0, 3>(); }
    constexpr Vector<T, 4> ragr() const noexcept { return Swizzle<0, 3, 1, 0>(); }
    constexpr Vector<T, 4> ragg() const noexcept { return Swizzle<0, 3, 1, 1>(); }
    constexpr Vector<T, 4> ragb() const noexcept { return Swizzle<0, 3, 1, 2>(); }
    constexpr Vector<T, 4> raga() const noexcept { return Swizzle<0, 3, 1, 3>(); }
    constexpr Vector<T, 4> rabr() const noexcept { return Swizzle<0, 3, 2, 0>(); }
    constexpr Vector<T, 4> rabg() const noexcept { return Swizzle<0, 3, 2, 1>(); }
    constexpr Vector<T, 4> rabb() const noexcept { return Swizzle<0, 3, 2, 2>(); }
    constexpr Vector<T, 4> raba() const noexcept { return Swizzle<0, 3, 2, 3>(); }
    constexpr Vector<T, 4> raar() const noexcept { return Swizzle<0, 3, 3, 0>(); }
    constexpr Vector<T, 4> raag() const noexcept { return Swizzle<0, 3, 3, 1>(); }
    constexpr Vector<T, 4> raab() const noexcept { return Swizzle<0, 3, 3, 2>(); }
    constexpr Vector<T, 4> raaa() const noexcept { return Swizzle<0, 3, 3, 3>(); }
    constexpr Vector<T, 4> grrr() const noexcept { return Swizzle<1, 0, 0, 0>(); }
    constexpr Vector<T, 4> grrg() const noexcept { return Swizzle<1, 0, 0, 1>(); }
    constexpr Vector<T, 4> grrb() const noexcept { return Swizzle<1, 0, 0, 2>(); }
    constexpr Vector<T, 4> grra() const noexcept { return Swizzle<1, 0, 0, 3>(); }
    constexpr Vector<T, 4> grgr() const noexcept { return Swizzle<1, 0, 1, 0>(); }
    constexpr Vector<T, 4> grgg() const noexcept { return Swizzle<1, 0, 1, 1>(); }
    constexpr Vector<T, 4> grgb() const noexcept { return Swizzle<1, 0, 1, 2>(); }
    constexpr Vector<T, 4> grga() const noexcept { return Swizzle<1, 0, 1, 3>(); }
    constexpr Vector<T, 4> grbr() const noexcept { return Swizzle<1, 0, 2, 0>(); }
    constexpr Vector<T, 4> grbg() const noexcept { return Swizzle<1, 0, 2, 1>(); }
    constexpr Vector<T, 4> grbb() const noexcept { return Swizzle<1, 0, 2, 2>(); }
    constexpr Vector<T, 4> grba() const noexcept { return Swizzle<1, 0, 2, 3>(); }
    constexpr Vector<T, 4> grar() const noexcept { return Swizzle<1, 0, 3, 0>(); }
    constexpr Vector<T, 4> grag() const noexcept { return Swizzle<1, 0, 3, 1>(); }
    constexpr Vector<T, 4> grab() const noexcept { return Swizzle<1, 0, 3, 2>(); }
    constexpr Vector<T, 4> graa() const noexcept { return Swizzle<1, 0, 3, 3>(); }
    constexpr Vector<T, 4> ggrr() const noexcept { return Swizzle<1, 1, 0, 0>(); }
    constexpr Vector<T, 4> ggrg() const noexcept { return Swizzle<1, 1, 0, 1>(); }
    constexpr Vector<T, 4> ggrb() const noexcept { return Swizzle<1, 1, 0, 2>(); }
    constexpr Vector<T, 4> ggra() const noexcept { return Swizzle<1, 1, 0, 3>(); }
    constexpr Vector<T, 4> gggr() const noexcept { return Swizzle<1, 1, 1, 0>(); }
    constexpr Vector<T, 4> gggg() const noexcept { return Swizzle<1, 1, 1, 1>(); }
    constexpr Vector<T, 4> gggb() const noexcept { return Swizzle<1, 1, 1, 2>(); }
    constexpr Vector<T, 4> ggga() const noexcept { return Swizzle<1, 1, 1, 3>(); }
    constexpr Vector<T, 4> ggbr() const noexcept { return Swizzle<1, 1, 2, 0>(); }
    constexpr Vector<T, 4> ggbg() const noexcept { return Swizzle<1, 1, 2, 1>(); }
    constexpr Vector<T, 4> ggbb() const noexcept { return Swizzle<1, 1, 2, 2>(); }
    constexpr Vector<T, 4> ggba() const noexcept { return Swizzle<1, 1, 2, 3>(); }
    constexpr Vector<T, 4> ggar() const noexcept { return Swizzle<1, 1, 3, 0>(); }
    constexpr Vector<T, 4> ggag() const noexcept { return Swizzle<1, 1, 3, 1>(); }
    constexpr Vector<T, 4> ggab() const noexcept { return Swizzle<1, 1, 3, 2>(); }
    constexpr Vector<T, 4> ggaa() const noexcept { return Swizzle<1, 1, 3, 3>(); }
    constexpr Vector<T, 4> gbrr() const noexcept { return Swizzle<1, 2, 0, 0>(); }
    constexpr Vector<T, 4> gbrg() const noexcept { return Swizzle<1, 2, 0, 1>(); }
    constexpr Vector<T, 4> gbrb() const noexcept { return Swizzle<1, 2, 0, 2>(); }
    constexpr Vector<T, 4> gbra() const noexcept { return Swizzle<1, 2, 0, 3>(); }
    constexpr Vector<T, 4> gbgr() const noexcept { return Swizzle<1, 2, 1, 0>(); }
    constexpr Vector<T, 4> gbgg() const noexcept { return Swizzle<1, 2, 1, 1>(); }
    constexpr Vector<T, 4> gbgb() const noexcept { return Swizzle<1, 2, 1, 2>(); }
    constexpr Vector<T, 4> gbga() const noexcept { return Swizzle<1, 2, 1, 3>(); }
    constexpr Vector<T, 4> gbbr() const noexcept { return Swizzle<1, 2, 2, 0>(); }
    constexpr Vector<T, 4> gbbg() const noexcept { return Swizzle<1, 2, 2, 1>(); }
    constexpr Vector<T, 4> gbbb() const noexcept { return Swizzle<1, 2, 2, 2>(); }
    constexpr Vector<T, 4> gbba() const noexcept { return Swizzle<1, 2, 2, 3>(); }
    constexpr Vector<T, 4> gbar() const noexcept { return Swizzle<1, 2, 3, 0>(); }
    constexpr Vector<T, 4> gbag() const noexcept { return Swizzle<1, 2, 3, 1>(); }
    constexpr Vector<T, 4> gbab() const noexcept { return Swizzle<1, 2, 3, 2>(); }
    constexpr Vector<T, 4> gbaa() const noexcept { return Swizzle<1, 2, 3, 3>(); }
    constexpr Vector<T, 4> garr() const noexcept { return Swizzle<1, 3, 0, 0>(); }
    constexpr Vector<T, 4> garg() const noexcept { return Swizzle<1, 3, 0, 1>(); }
    constexpr Vector<T, 4> garb() const noexcept { return Swizzle<1, 3, 0, 2>(); }
    constexpr Vector<T, 4> gara() const noexcept { return Swizzle<1, 3, 0, 3>(); }
    constexpr Vector<T, 4> gagr() const noexcept { return Swizzle<1, 3, 1, 0>(); }
    constexpr Vector<T, 4> gagg() const noexcept { return Swizzle<1, 3, 1, 1>(); }
    constexpr Vector<T, 4> gagb() const noexcept { return Swizzle<1, 3, 1, 2>(); }
    constexpr Vector<T, 4> gaga() const noexcept { return Swizzle<1, 3, 1, 3>(); }
    constexpr Vector<T, 4> gabr() const noexcept { return Swizzle<1, 3, 2, 0>(); }
    constexpr Vector<T, 4> gabg() const noexcept { return Swizzle<1, 3, 2, 1>(); }
    constexpr Vector<T, 4> gabb() const noexcept { return Swizzle<1, 3, 2, 2>(); }
    constexpr Vector<T, 4> gaba() const noexcept { return Swizzle<1, 3, 2, 3>(); }
    constexpr Vector<T, 4> gaar() const noexcept { return Swizzle<1, 3, 3, 0>(); }
    constexpr Vector<T, 4> gaag() const noexcept { return Swizzle<1, 3, 3, 1>(); }
    constexpr Vector<T, 4> gaab() const noexcept { return Swizzle<1, 3, 3, 2>(); }
    constexpr Vector<T, 4> gaaa() const noexcept { return Swizzle<1, 3, 3, 3>(); }
    constexpr Vector<T, 4> brrr() const noexcept { return Swizzle<2, 0, 0, 0>(); }
    constexpr Vector<T, 4> brrg() const noexcept { return Swizzle<2, 0, 0, 1>(); }
    constexpr Vector<T, 4> brrb() const noexcept { return Swizzle<2, 0, 0, 2>(); }
    constexpr Vector<T, 4> brra() const noexcept { return Swizzle<2, 0, 0, 3>(); }
    constexpr Vector<T, 4> brgr() const noexcept { return Swizzle<2, 0, 1, 0>(); }
    constexpr Vector<T, 4> brgg() const noexcept { return Swizzle<2, 0, 1, 1>(); }
    constexpr Vector<T, 4> brgb() const noexcept { return Swizzle<2, 0, 1, 2>(); }
    constexpr Vector<T, 4> brga() const noexcept { return Swizzle<2, 0, 1, 3>(); }
    constexpr Vector<T, 4> brbr() const noexcept { return Swizzle<2, 0, 2, 0>(); }
    constexpr Vector<T, 4> brbg() const noexcept { return Swizzle<2, 0, 2, 1>(); }
    constexpr Vector<T, 4> brbb() const noexcept { return Swizzle<2, 0, 2, 2>(); }
    constexpr Vector<T, 4> brba() const noexcept { return Swizzle<2, 0, 2, 3>(); }
    constexpr Vector<T, 4> brar() const noexcept { return Swizzle<2, 0, 3, 0>(); }
    constexpr Vector<T, 4> brag() const noexcept { return Swizzle<2, 0, 3, 1>(); }
    constexpr Vector<T, 4> brab() const noexcept { return Swizzle<2, 0, 3, 2>(); }
    constexpr Vector<T, 4> braa() const noexcept { return Swizzle<2, 0, 3, 3>(); }
    constexpr Vector<T, 4> bgrr() const noexcept { return Swizzle<2, 1, 0, 0>(); }
    constexpr Vector<T, 4> bgrg() const noexcept { return Swizzle<2, 1, 0, 1>(); }
    constexpr Vector<T, 4> bgrb() const noexcept { return Swizzle<2, 1, 0, 2>(); }
    constexpr Vector<T, 4> bgra() const noexcept { return Swizzle<2, 1, 0, 3>(); }
    constexpr Vector<T, 4> bggr() const noexcept { return Swizzle<2, 1, 1, 0>(); }
    constexpr Vector<T, 4> bggg() const noexcept { return Swizzle<2, 1, 1, 1>(); }
    constexpr Vector<T, 4> bggb() const noexcept { return Swizzle<2, 1, 1, 2>(); }
    constexpr Vector<T, 4> bgga() const noexcept { return Swizzle<2, 1, 1, 3>(); }
    constexpr Vector<T, 4> bgbr() const noexcept { return Swizzle<2, 1, 2, 0>(); }
    constexpr Vector<T, 4> bgbg() const noexcept { return Swizzle<2, 1, 2, 1>(); }
    constexpr Vector<T, 4> bgbb() const noexcept { return Swizzle<2, 1, 2, 2>(); }
    constexpr Vector<T, 4> bgba() const noexcept { return Swizzle<2, 1, 2, 3>(); }
    constexpr Vector<T, 4> bgar() const noexcept { return Swizzle<2, 1, 3, 0>(); }
    constexpr Vector<T, 4> bgag() const noexcept { return Swizzle<2, 1, 3, 1>(); }
    constexpr Vector<T, 4> bgab() const noexcept { return Swizzle<2, 1, 3, 2>(); }
    constexpr Vector<T, 4> bgaa() const noexcept { return Swizzle<2, 1, 3, 3>(); }
    constexpr Vector<T, 4> bbrr() const noexcept { return Swizzle<2, 2, 0, 0>(); }
    constexpr Vector<T, 4> bbrg() const noexcept { return Swizzle<2, 2, 0, 1>(); }
    constexpr Vector<T, 4> bbrb() const noexcept { return Swizzle<2, 2, 0, 2>(); }
    constexpr Vector<T, 4> bbra() const noexcept { return Swizzle<2, 2, 0, 3>(); }
    constexpr Vector<T, 4> bbgr() const noexcept { return Swizzle<2, 2, 1, 0>(); }
    constexpr Vector<T, 4> bbgg() const noexcept { return Swizzle<2, 2, 1, 1>(); }
    constexpr Vector<T, 4> bbgb() const noexcept { return Swizzle<2, 2, 1, 2>(); }
    constexpr Vector<T, 4> bbga() const noexcept { return Swizzle<2, 2, 1, 3>(); }
    constexpr Vector<T, 4> bbbr() const noexcept { return Swizzle<2, 2, 2, 0>(); }
    constexpr Vector<T, 4> bbbg() const noexcept { return Swizzle<2, 2, 2, 1>(); }
    constexpr Vector<T, 4> bbbb() const noexcept { return Swizzle<2, 2, 2, 2>(); }
    constexpr Vector<T, 4> bbba() const noexcept { return Swizzle<2, 2, 2, 3>(); }
    constexpr Vector<T, 4> bbar() const noexcept { return Swizzle<2, 2, 3, 0>(); }
    constexpr Vector<T, 4> bbag() const noexcept { return Swizzle<2, 2, 3, 1>(); }
    constexpr Vector<T, 4> bbab() const noexcept { return Swizzle<2, 2, 3, 2>(); }
    constexpr Vector<T, 4> bbaa() const noexcept { return Swizzle<2, 2, 3, 3>(); }
    constexpr Vector<T, 4> barr() const noexcept { return Swizzle<2, 3, 0, 0>(); }
    constexpr Vector<T, 4> barg() const noexcept { return Swizzle<2, 3, 0, 1>(); }
    constexpr Vector<T, 4> barb() const noexcept { return Swizzle<2, 3, 0, 2>(); }
    constexpr Vector<T, 4> bara() const noexcept { return Swizzle<2, 3, 0, 3>(); }
    constexpr Vector<T, 4> bagr() const noexcept { return Swizzle<2, 3, 1, 0>(); }
    constexpr Vector<T, 4> bagg() const noexcept { return Swizzle<2, 3, 1, 1>(); }
    constexpr Vector<T, 4> bagb() const noexcept { return Swizzle<2, 3, 1, 2>(); }
    constexpr Vector<T, 4> baga() const noexcept { return Swizzle<2, 3, 1, 3>(); }
    constexpr Vector<T, 4> babr() const noexcept { return Swizzle<2, 3, 2, 0>(); }
    constexpr Vector<T, 4> babg() const noexcept { return Swizzle<2, 3, 2, 1>(); }
    constexpr Vector<T, 4> babb() const noexcept { return Swizzle<2, 3, 2, 2>(); }
    constexpr Vector<T, 4> baba() const noexcept { return Swizzle<2, 3, 2, 3>(); }
    constexpr Vector<T, 4> baar() const noexcept { return Swizzle<2, 3, 3, 0>(); }
    constexpr Vector<T, 4> baag() const noexcept { return Swizzle<2, 3, 3, 1>(); }
    constexpr Vector<T, 4> baab() const noexcept { return Swizzle<2, 3, 3, 2>(); }
    constexpr Vector<T, 4> baaa() const noexcept { return Swizzle<2, 3, 3, 3>(); }
    constexpr Vector<T, 4> arrr() const noexcept { return Swizzle<3, 0, 0, 0>(); }
    constexpr Vector<T, 4> arrg() const noexcept { return Swizzle<3, 0, 0, 1>(); }
    constexpr Vector<T, 4> arrb() const noexcept { return Swizzle<3, 0, 0, 2>(); }
    constexpr Vector<T, 4> arra() const noexcept { return Swizzle<3, 0, 0, 3>(); }
    constexpr Vector<T, 4> argr() const noexcept { return Swizzle<3, 0, 1, 0>(); }
    constexpr Vector<T, 4> argg() const noexcept { return Swizzle<3, 0, 1, 1>(); }
    constexpr Vector<T, 4> argb() const noexcept { return Swizzle<3, 0, 1, 2>(); }
    constexpr Vector<T, 4> arga() const noexcept { return Swizzle<3, 0, 1, 3>(); }
    constexpr Vector<T, 4> arbr() const noexcept { return Swizzle<3, 0, 2, 0>(); }
    constexpr Vector<T, 4> arbg() const noexcept { return Swizzle<3, 0, 2, 1>(); }
    constexpr Vector<T, 4> arbb() const noexcept { return Swizzle<3, 0, 2, 2>(); }
    constexpr Vector<T, 4> arba() const noexcept { return Swizzle<3, 0, 2, 3>(); }
    constexpr Vector<T, 4> arar() const noexcept { return Swizzle<3, 0, 3, 0>(); }
    constexpr Vector<T, 4> arag() const noexcept { return Swizzle<3, 0, 3, 1>(); }
    constexpr Vector<T, 4> arab() const noexcept { return Swizzle<3, 0, 3, 2>(); }
    constexpr Vector<T, 4> araa() const noexcept { return Swizzle<3, 0, 3, 3>(); }
    constexpr Vector<T, 4> agrr() const noexcept { return Swizzle<3, 1, 0, 0>(); }
    constexpr Vector<T, 4> agrg() const noexcept { return Swizzle<3, 1, 0, 1>(); }
    constexpr Vector<T, 4> agrb() const noexcept { return Swizzle<3, 1, 0, 2>(); }
    constexpr Vector<T, 4> agra() const noexcept { return Swizzle<3, 1, 0, 3>(); }
    constexpr Vector<T, 4> aggr() const noexcept { return Swizzle<3, 1, 1, 0>(); }
    constexpr Vector<T, 4> aggg() const noexcept { return Swizzle<3, 1, 1, 1>(); }
    constexpr Vector<T, 4> aggb() const noexcept { return Swizzle<3, 1, 1, 2>(); }
    constexpr Vector<T, 4> agga() const noexcept { return Swizzle<3, 1, 1, 3>(); }
    constexpr Vector<T, 4> agbr() const noexcept { return Swizzle<3, 1, 2, 0>(); }
    constexpr Vector<T, 4> agbg() const noexcept { return Swizzle<3, 1, 2, 1>(); }
    constexpr Vector<T, 4> agbb() const noexcept { return Swizzle<3, 1, 2, 2>(); }
    constexpr Vector<T, 4> agba() const noexcept { return Swizzle<3, 1, 2, 3>(); }
    constexpr Vector<T, 4> agar() const noexcept { return Swizzle<3, 1, 3, 0>(); }
    constexpr Vector<T, 4> agag() const noexcept { return Swizzle<3, 1, 3, 1>(); }
    constexpr Vector<T, 4> agab() const noexcept { return Swizzle<3, 1, 3, 2>(); }
    constexpr Vector<T, 4> agaa() const noexcept { return Swizzle<3, 1, 3, 3>(); }
    constexpr Vector<T, 4> abrr() const noexcept { return Swizzle<3, 2, 0, 0>(); }
    constexpr Vector<T, 4> abrg() const noexcept { return Swizzle<3, 2, 0, 1>(); }
    constexpr Vector<T, 4> abrb() const noexcept { return Swizzle<3, 2, 0, 2>(); }
    constexpr Vector<T, 4> abra() const noexcept { return Swizzle<3, 2, 0, 3>(); }
    constexpr Vector<T, 4> abgr() const noexcept { return Swizzle<3, 2, 1, 0>(); }
    constexpr Vector<T, 4> abgg() const noexcept { return Swizzle<3, 2, 1, 1>(); }
    constexpr Vector<T, 4> abgb() const noexcept { return Swizzle<3, 2, 1, 2>(); }
    constexpr Vector<T, 4> abga() const noexcept { return Swizzle<3, 2, 1, 3>(); }
    constexpr Vector<T, 4> abbr() const noexcept { return Swizzle<3, 2, 2, 0>(); }
    constexpr Vector<T, 4> abbg() const noexcept { return Swizzle<3, 2, 2, 1>(); }
    constexpr Vector<T, 4> abbb() const noexcept { return Swizzle<3, 2, 2, 2>(); }
    constexpr Vector<T, 4> abba() const noexcept { return Swizzle<3, 2, 2, 3>(); }
    constexpr Vector<T, 4> abar() const noexcept { return Swizzle<3, 2, 3, 0>(); }
    constexpr Vector<T, 4> abag() const noexcept { return Swizzle<3, 2, 3, 1>(); }
    constexpr Vector<T, 4> abab() const noexcept { return Swizzle<3, 2, 3, 2>(); }
    constexpr Vector<T, 4> abaa() const noexcept { return Swizzle<3, 2, 3, 3>(); }
    constexpr Vector<T, 4> aarr() const noexcept { return Swizzle<3, 3, 0, 0>(); }
    constexpr Vector<T, 4> aarg() const noexcept { return Swizzle<3, 3, 0, 1>(); }
    constexpr Vector<T, 4> aarb() const noexcept { return Swizzle<3, 3, 0, 2>(); }
    constexpr Vector<T, 4> aara() const noexcept { return Swizzle<3, 3, 0, 3>(); }
    constexpr Vector<T, 4> aagr() const noexcept { return Swizzle<3, 3, 1, 0>(); }
    constexpr Vector<T, 4> aagg() const noexcept { return Swizzle<3, 3, 1, 1>(); }
    constexpr Vector<T, 4> aagb() const noexcept { return Swizzle<3, 3, 1, 2>(); }
    constexpr Vector<T, 4> aaga() const noexcept { return Swizzle<3, 3, 1, 3>(); }
    constexpr Vector<T, 4> aabr() const noexcept { return Swizzle<3, 3, 2, 0>(); }
    constexpr Vector<T, 4> aabg() const noexcept { return Swizzle<3, 3, 2, 1>(); }
    constexpr Vector<T, 4> aabb() const noexcept { return Swizzle<3, 3, 2, 2>(); }
    constexpr Vector<T, 4> aaba() const noexcept { return Swizzle<3, 3, 2, 3>(); }
    constexpr Vector<T, 4> aaar() const noexcept { return Swizzle<3, 3, 3, 0>(); }
    constexpr Vector<T, 4> aaag() const noexcept { return Swizzle<3, 3, 3, 1>(); }
    constexpr Vector<T, 4> aaab() const noexcept { return Swizzle<3, 3, 3, 2>(); }
    constexpr Vector<T, 4> aaaa() const noexcept { return Swizzle<3, 3, 3, 3>(); }
```

---

## 六、数量统计

| 向量类型 | 可用分量数 | 2 维 (k=2) | 3 维 (k=3) | 4 维 (k=4) | 单命名系合计 | 双命名系合计 |
|---------|-----------|-----------|-----------|-----------|------------|------------|
| Vector<T,2> | 2 | 2² = 4   | 2³ = 8    | 2⁴ = 16   | 28         | 56         |
| Vector<T,3> | 3 | 3² = 9   | 3³ = 27   | 3⁴ = 81   | 117        | 234        |
| Vector<T,4> | 4 | 4² = 16  | 4³ = 64   | 4⁴ = 256  | 336        | 672        |

公式：**分量数为 c、输出长度为 k 时，方法数 = cᵏ**；单系合计 = c² + c³ + c⁴。

---

## 七、配套的 SetSwizzle（写入式，Vector.hpp:182）

```cpp
template <std::size_t... Indices>
requires(... && detail::IndicesAreUnique<Indices...>())
constexpr void SetSwizzle(const Vector<T, sizeof...(Indices)>& value) noexcept { ... }
```

宏只生成"读"式 swizzle（返回值），不生成 set 式。set 式留给手写模板，并用 `IndicesAreUnique`
（Vector.hpp:19，consteval 编译期检查下标不重复）约束，避免 `v.xx() = ...` 这种语义无意义、
且会因 `source++` 顺序互相覆盖而出错的写法。
