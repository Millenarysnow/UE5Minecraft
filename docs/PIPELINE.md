# Pipeline

地形生成所有公式 / 数值 / 决策。next agent 改任何东西前看这里查实际参数。

## 1. 噪声原语

### `FOctavedPerlin`

Mojang `PerlinNoise` 等价。

```
sample(x, y, z) = Σ_{i=0..N-1} amp[i] · perlin_i(x·F, y·F, z·F) · V_i
```

其中：
- `F = 2^(firstOctave + i)` （第 i octave 的频率倍率）
- `V_i = lowestFreqValueFactor / 2^i`
- `lowestFreqValueFactor = 2^(N-1) / (2^N - 1)`
- `amp[i] == 0` 的 octave 跳过不算

输出范围实测约 [-1, 1]（典型 |output| ≤ 0.7）。

### `FNormalNoise`

Mojang `NormalNoise` 等价。

```
sample(x, y, z) = (P1.sample(x, y, z) + P2.sample(x·k, y·k, z·k)) · valueFactor
```

其中：
- `k = 1.0181268882175227` （Mojang 硬编码常量，让两个 PerlinNoise 解相关）
- `valueFactor = (1/6) / expectedDeviation(span)`
- `expectedDeviation(n) = 0.1 · (1 + 1/(n+1))`
- `span = 最高非零 amp idx - 最低非零 amp idx`

### 噪声参数表（`Noises::*`）

完全照搬 Mojang `NoiseData.bootstrap()`：

| 名字 | firstOctave | amplitudes |
|---|---|---|
| Temperature       | -10 | [1.5, 0, 1, 0, 0, 0] |
| Vegetation        |  -8 | [1, 1, 0, 0, 0, 0] |
| Continentalness   |  -9 | [1, 1, 2, 2, 2, 1, 1, 1, 1] |
| Erosion           |  -9 | [1, 1, 0, 1, 1] |
| Ridge             |  -7 | [1, 2, 1, 0, 0, 0] |
| Shift             |  -3 | [1, 1, 1, 0] |
| Jagged            | -16 | [16 个 1] |
| CaveCheese        |  -8 | [0.5, 1, 2, 1, 2, 1, 0, 2, 0] |
| CaveEntrance      |  -7 | [0.4, 0.5, 1.0] |
| Surface           |  -6 | [1, 1, 1] |
| SurfaceSecondary  |  -6 | [1, 1, 0, 1] |

### 种子派生

每个 `FNormalNoise` 实例的种子 = `HashKey(WorldSeed, Def.Key)`，其中 `Def.Key` 是字符串如 `"minecraft:continentalness"`。

每个 octave 的子种子 = `HashKey(NoiseSeed, firstOctave + i)`。

`SplitMix64` + `HashCombine64` 在 `Hash.h`。

## 2. Climate samplers

```cpp
ContinentalnessDF = DF::Noise2D(ContinentNoise, 0.25)  // 缩放 0.25 = Mojang 默认
ErosionDF         = DF::Noise2D(ErosionNoise, 0.25)
RidgesDF          = DF::Noise2D(RidgeNoise, 0.25)
TemperatureDF     = DF::Noise2D(TempNoise, 0.25)
HumidityDF        = DF::Noise2D(VegNoise, 0.25)
```

**v1 简化**：没做 Mojang 的 domain warp（`shiftA/shiftB` 用 SHIFT 噪声做 X/Z 偏移）。biome 边界因此略呈轴对齐。

### Peaks-and-valleys 折叠

```
ridgesFolded = -3 · (|abs(ridges) - 2/3| - 1/3)
```

为代码：

```cpp
RidgesFoldedDF = DF::Mul(
    DF::Add(
        DF::Abs(DF::Add(DF::Abs(RidgesDF), DF::Constant(-2.0/3.0))),
        DF::Constant(-1.0/3.0)
    ),
    DF::Constant(-3.0)
);
```

`pv(0) = -1`、`pv(±0.5) = +0.5`、`pv(±2/3) = +1`。所以 `ridges ≈ 0`（典型 Perlin 中央值）→ `pv ≈ -1`（valley）。

## 3. 地形 splines

完整复刻 Mojang `TerrainProvider.java`。`TerrainSplines.cpp` 三个公开入口：

- `BuildOffset(continents, erosion, ridgesFolded)`
- `BuildFactor(continents, erosion, ridges, ridgesFolded)`
- `BuildJaggedness(continents, erosion, ridges, ridgesFolded)`

锚点常量直接从 Mojang 抄过来。**不做 amplified 模式**。

### 关键数值（`overworldOffset` 外层）

```
.Add(-1.10f,  0.044f)
.Add(-1.02f, -0.2222f)
.Add(-0.51f, -0.2222f)  ← 深海
.Add(-0.44f, -0.12f)
.Add(-0.18f, -0.12f)    ← coast
.Add(-0.16f, ero1)      ← coast/inland 过渡
.Add(-0.15f, ero1)
.Add(-0.10f, ero2)      ← near-inland plains
.Add( 0.25f, ero3)      ← mid-inland
.Add( 1.00f, ero4)      ← far-inland mountain
```

`ero1..4` 是按 erosion + ridgesFolded 嵌套的子样条。

### Spline 求值

`FCubicSpline::Evaluate(x, y, z)`：
- coordinate 是个 `IDensityFunction`，每次 evaluate 都 `coord->Compute(x,y,z) → float f`
- 找区间 `i`，使 `locations[i] ≤ f < locations[i+1]`
- 区间内：Hermite 插值
  ```
  t = (f - x_i) / (x_{i+1} - x_i)
  k0 = d_i · dx - (y_{i+1} - y_i)
  k1 = -d_{i+1} · dx + (y_{i+1} - y_i)
  result = lerp(t, y0, y1) + t·(1-t)·lerp(t, k0, k1)
  ```
- 区间外：按边缘 derivative 线性外推
- 子节点 `value` 可以是另一个 spline（递归 evaluate）

## 4. final_density 公式

`FNoiseRouter::DensityInColumn(Col, Wy)`：

```
ygrad     = y_clamped_gradient(MinY..MaxY, 1.5..-1.5)(Wy)
           = 1.5 - (Wy - MinY) · 3 / 384  （在范围内线性）
depth     = ygrad + Col.Offset
jagPerlin = JaggedNoise.Sample(Wx·1500, 0, Wz·1500)  ← 高频
jagTerm   = Col.Jaggedness · halfNegative(jagPerlin)
inner     = (depth + jagTerm) · Col.Factor
slopedNoB = 4 · qn(inner)   ← qn = quarterNegative
base3D    = Base3DNoise.Sample(Wx·1.0, Wy·0.5, Wz·1.0)
slopedCh  = slopedNoB + base3D

[caves enabled 时还要]:
caveNoise = CaveCheeseNoise.Sample(Wx, Wy·0.6666, Wz)
chsPart1  = clamp(0.27 + caveNoise, -1, 1)
chsPart2  = clamp(1.5 - 0.64 · slopedCh, 0, 0.5)   ← 表层保护
cheeseTerm= chsPart1 + chsPart2
entrance  = CaveEntranceNoise.Sample(Wx·0.75, Wy·0.5, Wz·0.75)
yEntGrad  = y_clamped_gradient(-10..30, 0.3..0.0)(Wy)
entTerm   = entrance + 0.37 + yEntGrad
density   = min(slopedCh, cheeseTerm, 5·entTerm)

[Slide 顶/底]:
return ApplySlide(density, Wy)
```

### Slide

```cpp
// Top: y=240..256 lerp 到 -0.078125（让 y≥256 都是空气）
if (Wy >= 240) {
    T = (Wy >= 256) ? 0 : 1 - (Wy - 240) / 16
    density = -0.078125 + T · (density - (-0.078125))
}
// Bottom: y=-64..-40 lerp 到 0.1171875（让 y≤-64 都是石头）
if (Wy <= -40) {
    T = (Wy <= -64) ? 0 : (Wy - (-64)) / 24
    density = 0.1171875 + T · (density - 0.1171875)
}
```

### 替代品：`base_3d_noise`

Mojang 用 `BlendedNoise(0.25, 0.125, 80, 160, 8)`，我们用：

```cpp
FNormalNoise(seed, FOctavedNoiseParameters(-7, {1, 1, 1, 1, 1}))
sample at (Wx · 1.0, Wy · 0.5, Wz · 1.0)
```

最低 octave 周期 ~128 块（xz）/ 256 块（y），5 octaves。视觉上类似 Mojang 的"地形抖动"。

## 5. ContinentBias

我们加的非 Mojang 参数：

```cpp
OffsetDF = DF::Add(
    DF::Constant(GLOBAL_OFFSET + ContinentBias),  // GLOBAL_OFFSET = -0.50375
    DF::Spline(OffsetSpline)
)
```

- `0.0` = Mojang spec（plains 表层在 sea level 附近，大部分 underwater）
- `0.1`（推荐默认）= plains surface 抬到 ~y=77
- 范围 `[-0.3, +0.3]`

## 6. 边界估算

`FNoiseRouter::EstimateColumnBounds(Col)`：

```cpp
const double Base3DMax = 2.5;  // 经验上界
const double F = max(Col.Factor, 0.01);

// depth > DepthStone → density 一定 > 0（必石）
DepthStone = Base3DMax * 0.25 / F   // 因为 4·qn(inner > 0) = 4·inner，需要 > Base3DMax

// depth < DepthAir → density 一定 < 0（必空气，但还要看 y 决定 sea Water / 天空 Air）
DepthAir = -Base3DMax / F           // qn(inner < 0) = inner，需要 < -Base3DMax

// 反解 y
StoneYMax = floor(128 · (1.5 + Col.Offset - DepthStone) - 64)
AirYMin   = ceil(128 · (1.5 + Col.Offset - DepthAir) - 64)
```

`FillChunk` fast path：
- `ChunkYMax ≤ StoneYMax` AND caves 关 → 全 stone（caves 开时不安全，因为洞会挖穿）
- `ChunkYMin ≥ AirYMin` → 全 air/water
- 否则逐 y 评估

## 7. Sea Water vs cave Air 状态机

Pass 1 自上而下扫每列：

```
state = bSeenStoneInit  // 列开头初始化（见下）

for lz from ChunkSize-1 down to 0:
    density = DensityInColumn(Col, worldY)
    if worldY == MinY:
        Block = Bedrock; state = true
    elif density > 0:
        Block = Stone; state = true
    elif state:
        Block = Air         # 已经在石头之下 → cave
    else:
        Block = (worldY ≤ SeaLevel) ? Water : Air   # 还在表面之上 → sea/sky

# Post-process: cave entrance 穿过整列 → 改 Water 为 Air
if !bAnyStone && ChunkYMax < EstimatedSurface:
    convert all Water in Column to Air
```

`bSeenStoneInit` 启发式（在 `WorldGenerator.cpp` `FillChunk` 里）：

```cpp
EstimatedSurface = MinY + 128 · (1.5 + Col.Offset)
AboveChunkUEz = ChunkOriginVoxel.Z + ChunkSize  // chunk 顶上一格

if (caves 关 && AboveChunkUEz <= Bounds.StoneYMax):
    init = true (under continuous stone)
elif (AboveChunkUEz >= Bounds.AirYMin):
    init = false (in sky/sea)
else:
    AboveDensity = density(AboveChunkUEz)
    if AboveDensity > 0:
        init = true (stone above)
    elif AboveChunkUEz < EstimatedSurface - 30:
        init = true (caves below surface; conservatively assume stone above somewhere)
    else:
        init = false (above estimated surface)
```

## 8. Surface system

`FSurfaceSystem::ApplyColumn`：

1. 找列内最高的 stone block（其上是 air/water/BlockAbove）
2. 算 surface depth：`clamp(noise · 2.75 + 3, 2, 6)` 块
3. 按 biome / debug / underwater 派发：

| 情况 | TopBlock | UnderBlock |
|---|---|---|
| Debug + Plains | DebugBiomePlains | DebugBiomePlains |
| Debug + Forest | SpruceLeaf | SpruceLeaf |
| Debug + Desert | Sand | Sand |
| Debug + SnowyPlains | Snow | Snow |
| Debug + Mountains | DebugBiomeMountains | DebugBiomeMountains |
| Debug + Ocean | Sand | Sand |
| 非 debug + Underwater | Sand | Sand |
| 非 debug + Desert | Sand | Sand |
| 非 debug + SnowyPlains | Snow | Dirt |
| 非 debug + 其它 | Grass | Dirt |

4. 顶 stone → TopBlock；下面 SurfaceDepth 层（碰到非 stone 即停）→ UnderBlock

**只在 `bSeenStoneInit == false` 的列上跑**（避免给地下的洞穴顶贴草）

## 9. Biome decision tree

`FBiomeSource::SampleBiome(cont, ero, pv, T, H, depth)` 顺序判断：

```cpp
if (cont < -0.19)  return Ocean;         // Mojang 真值
if (ero < -0.4)    return Mountains;     // 放宽（Mojang 是 < -0.78）
if (T < -0.3)      return SnowyPlains;   // 放宽（Mojang 是 < -0.45）
if (T > 0.3 && H < -0.05) return Desert; // 放宽
if (H > 0.1)       return Forest;
return Plains;
```

阈值放宽是因为我们的 Perlin σ ~0.3，Mojang spec 阈值在小视野里几乎触发不了。

## 10. Trees

`FFeaturePlacer::PlaceTrees`，每 chunk 决定性 RNG（seed = `HashKey(WorldSeed, "trees", chunkX, chunkY)`）。

Attempts 数：
- Forest: 10
- Plains: 5% 概率 1 棵
- SnowyPlains: 10% 概率 1 棵
- Mountains: 8% 概率 1 棵
- 其它: 0

每棵树：
- 中心位置 `[2, ChunkSize-3]`²（保证 5×5 叶 blob 在本 chunk 内）
- 找最顶 stone 的 lz
- 检查方块类型（oak 要 grass/dirt，spruce 要 grass/dirt/snow）
- 检查垂直空间够（trunk + leaves blob 全在 chunk 内，否则跳过）

形状：
- **Oak**：trunk `Wood × 4..6`，叶 `Leaf` 5×5×3 blob（4 角抠掉），中心位于 trunk 顶之下 1 块
- **Spruce**：trunk `SpruceLog × 5..8`，叶 `SpruceLeaf` 锥形 5 层（半径 2,2,1,1,0），从 trunk 中段开始

## 11. Ores

`FFeaturePlacer::PlaceOres`，每 chunk 决定性 RNG（seed = `HashKey(WorldSeed, "ores", chunkX, chunkY)`）。

| 矿 | Y 范围 | 分布 | 尝试次数 | 簇大小 |
|---|---|---|---|---|
| CoalOre    | [0, 96]    | uniform  | 20 | 4-8 |
| IronOre    | [80, 320]  | trapezoid | 4  | 4-9 |
| DiamondOre | [-64, 16]  | trapezoid | 7  | 4-8 |

实现：
- 跳过本 chunk 不在 Y 范围
- 每次尝试：随机 `(lx, ly)`，按分布采样 `worldY`，必须在本 chunk
- 三角分布用"两个均匀变量取均值"
- 中心必须是 Stone
- 在中心 ±1 的 3×3×3 立方里随机投 N 个，**仅替换 Stone**

## 12. Cloud

`ACloudLayer::Rebuild`：

- 单 actor，actor 位置跟随 player 水平位置 + `z = CloudHeight × 100`
- 在 actor 周围 `[-Radius/Cell, +Radius/Cell]²` cell 网格里采 Perlin
- `Perlin` 频率 `1/(CellSize · 4)`
- `noise > Threshold` → 该 cell 是云
- 风偏：采样 X 减去 `WindOffsetBlocks`（让云沿 +X 飘）

每个云 cell 生成 6 面立方体（top/bottom 永远生成，4 个 side 仅当邻居非云时生成）。

Tick：
- `WindOffsetBlocks += WindSpeedBlocksPerSec × DeltaTime`
- 时间累积超 `RebuildInterval` 或玩家移动够远 → 调 Rebuild()

默认参数：
- CloudHeight: 192
- RadiusBlocks: 384
- CellSizeBlocks: 12（Mojang 真值）
- ThicknessBlocks: 4（Mojang 真值）
- WindSpeedBlocksPerSec: 0.6（Mojang 真值）
- Threshold: 0.0
- RebuildInterval: 1.0
