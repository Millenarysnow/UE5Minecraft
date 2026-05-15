# Architecture

详细代码架构。看完这篇你能找到任何模块在哪、为什么存在、跟谁说话。

## 整体分层

```
┌─────────────────────────────────────────────────────────────────┐
│ Game/MyGameModeBase                                             │
│   读 BP_MyGameMode 配置 → 推到 Subsystems → 启动流式加载 + 云    │
└─────────────────────────────────────────────────────────────────┘
        │                                    │
        ▼                                    ▼
┌─────────────────────────────┐    ┌──────────────────────────────┐
│ UWorldGenerator             │    │ UChunkWorldSubsystem         │
│ (UGameInstanceSubsystem)    │    │ (UGameInstanceSubsystem)     │
│   - WorldSeed/ContinentBias │    │   - ChunkType/Material/...   │
│   - 拥有 NoiseRouter等组件  │    │   - 拥有 chunk 表 + 流式 timer│
│   - FillChunk(x,y,z,blocks) │    │   - 按 player 位置 spawn/遣散│
│   - IsBlockSolidAt()        │    │     chunk actors             │
└────────────┬────────────────┘    └──────────────┬───────────────┘
             │                                    │
             │ FillChunk                          │ Spawn/Destroy
             ▼                                    ▼
┌────────────────────────────────┐    ┌─────────────────────────────┐
│ Generation/ 子模块              │    │ AGreedyChunk : AChunkBase   │
│ ────────────────────────────── │    │   - 32×32×32 EBlock 数组   │
│  Hash.h        - 种子派生      │    │   - GreedyMesh → 2 mesh    │
│  OctavedNoise  - NormalNoise   │    │     sections (textured/    │
│  Noises        - 噪声参数表    │    │     colored)                │
│  CubicSpline   - 嵌套 Hermite │    │   - 通过 GetBlock 跨 chunk │
│  DensityFunc.  - 密度函数 ADT │    │     查询 IsBlockSolidAt    │
│  TerrainSplines- spline 锚点   │    └─────────────────────────────┘
│  NoiseRouter   - 装配 final_  │
│                  density       │
│  BiomeSource   - 6 biome 决策 │
│  SurfaceSystem - 顶层方块替换 │    ┌─────────────────────────────┐
│  FeaturePlacer - 树 + 矿     │    │ ACloudLayer                 │
│  WorldGenerator- Pass 1/2/3   │    │   - y=192 处的云层          │
│                  入口          │    │   - Tick 累积风偏 + 重建    │
└────────────────────────────────┘    └─────────────────────────────┘
```

## 模块职责

### `Game/MyGameModeBase`
- 暴露所有 BP_MyGameMode 可调参数（chunk 类型、绘制距离、垂直范围、洞穴开关、ContinentBias、Debug、Cloud 配置等）
- `BeginPlay()`:
  1. 处理 `WorldSeed = 0` → 用系统时间换成真种子（日志输出"auto seed: ..."）
  2. 把所有参数推到 `UWorldGenerator` 和 `UChunkWorldSubsystem`
  3. 调 `ChunkWorldSubsystem->StartStreaming()`
  4. 如果 `bSpawnClouds`，spawn 一个 `ACloudLayer`
- 不在 Tick 做事

### `Voxel/ChunkWorldSubsystem` (`UGameInstanceSubsystem`)
- 拥有 `TMap<FIntVector, AChunkBase*>` 已加载 chunk 表
- 周期 timer 驱动 `TickStreaming()`：
  - 找 player pawn
  - 算期望集 = `(playerChunk ± DrawDistance)² × 垂直范围`
  - 卸载离开期望集的 chunk（`Destroy()`）
  - 期望但未加载的 chunk 按距离排序，每 tick 最多 spawn `MaxSpawnsPerTick` 个
- `ModifyTargetVoxel` / `GetTargetVoxelType`：给 BP 用的方块编辑接口（玩家挖 / 放方块）

### `Voxel/Generation/` 各模块

#### `Hash.h`
- `SplitMix64`, `HashCombine64`, `HashKey(seed, str)`, `NoiseWrap`
- 用于种子派生（每个 noise 实例从 worldSeed + key 派生）

#### `OctavedNoise.{h,cpp}`
- `FOctavedNoiseParameters` — 等于 Mojang `NormalNoise.NoiseParameters`：`firstOctave + amplitudes[]`
- `FOctavedPerlin` — Mojang `PerlinNoise` 等价。N 个 octave 的 Perlin 加权和
- `FNormalNoise` — Mojang `NormalNoise` 等价。两个 `FOctavedPerlin` 的归一化合成
- 底层 Perlin 用 `FastNoiseLite`（`Voxel/Utils/FastNoiseLite.h`），不与 Mojang `ImprovedNoise` bit-exact

#### `Noises.{h,cpp}`
- 静态噪声参数表，对应 Mojang `NoiseData.bootstrap()`
- 含的 key：Temperature / Vegetation / Continentalness / Erosion / Ridge / Shift / Jagged / CaveCheese / CaveEntrance / Surface / SurfaceSecondary
- 每个 def 的 `Key` 字段是字符串（用作种子派生）+ `Params` 是 firstOctave/amplitudes

#### `CubicSpline.{h,cpp}`
- `FCubicSpline` — 可嵌套的 Hermite 三次样条
- `FSplineBuilder` — 仿 Mojang `CubicSpline.Builder`
- 关键：spline 节点的 value 可以是 float **或另一个 FCubicSpline**（递归）
- 求值算法 `Evaluate(x, y, z)` 完全照 Mojang `Multipoint.apply`

#### `DensityFunction.{h,cpp}`
- `IDensityFunction` 抽象基类，纯虚 `Compute(x, y, z) → double`
- `FDensityRef = TSharedPtr<const IDensityFunction>`
- namespace `DF::` 工厂函数：`Constant / Add / Mul / Min / Max / Clamp / Abs / Cube / Square / HalfNegative / QuarterNegative / YClampedGradient / Noise2D / Noise3D / ShiftedNoise2D / ShiftNoise / Spline`
- 节点都是 immutable，可任意组合树

#### `TerrainSplines.{h,cpp}`
- `BuildOffset(continents, erosion, ridgesFolded) → FCubicSpline`
- `BuildFactor(continents, erosion, ridges, ridgesFolded) → FCubicSpline`
- `BuildJaggedness(continents, erosion, ridges, ridgesFolded) → FCubicSpline`
- 内部辅助：`mountainContinentalness / calculateMountainRidgeZeroContinentalnessPoint / ridgeSpline / buildErosionOffsetSpline / getErosionFactor / buildRidgeJaggednessSpline / buildErosionJaggednessSpline / buildWeirdnessJaggednessSpline / peaksAndValleys (float overload)`
- 锚点常量直接搬自 Mojang `TerrainProvider.java`，不简化

#### `NoiseRouter.{h,cpp}`
- `FNoiseRouter` —— Mojang `NoiseRouterData.overworld()` 的简化等价
- 构造时一次性建好所有 NormalNoise + DensityFunction 树
- 核心 API：
  - 6 个 climate samplers：`Continentalness/ErosionVal/Ridges/RidgesFolded/Temperature/Humidity`（per-XZ，y 无关）
  - 3 个 spline 输出：`Offset / Factor / Jaggedness`（per-XZ）
  - `BuildColumn(Wx, Wz) → FColumnState`：缓存所有 y-无关量
  - `DensityInColumn(Col, Wy) → double`：在缓存上对单 y 求 final_density（含 cheese cave + entrance + slide）
  - `EstimateColumnBounds(Col) → {StoneYMax, AirYMin}`：解析估算"必固体/必空气" y 边界，给 fast path 用
- 接收构造参数：`bEnableCaves`、`ContinentBias`

#### `BiomeSource.{h,cpp}`
- `EBiome SampleBiome(cont, ero, pv, T, H, depth)` 决策树
- 阈值见 `PIPELINE.md`，不是 Mojang 严格 spec（放宽了让稀有 biome 在小视野里看得见）

#### `SurfaceSystem.{h,cpp}`
- `ApplyColumn(Wx, Wz, ChunkOriginY, Biome, BlockAbove, bDebug, ColumnView)`
- 找列内最顶 stone（其上是非 stone）作为世界表面
- 按 biome 派发 top + under 方块（grass+dirt / sand / snow / debug 标记块）
- Debug 模式下用 `DebugBiomePlains/DebugBiomeMountains` 等纯色标记

#### `FeaturePlacer.{h,cpp}`
- `PlaceFeatures` 入口 → 调用 `PlaceTrees` + `PlaceOres`
- 树：oak (4-6 块 + 5×5×3 叶 blob) / spruce (5-8 + 锥形叶)
- 矿：Coal (uniform y∈[0,96]) / Iron (trapezoid y∈[80,320]) / Diamond (trapezoid y∈[-64,16])
- 决定性 RNG：`HashKey(WorldSeed, "trees" 或 "ores", chunkX, chunkY)`

#### `WorldGenerator.{h,cpp}` (`UGameInstanceSubsystem`)
- 顶层入口，拥有 NoiseRouter / SurfaceSystem / BiomeSource / FeaturePlacer
- `EnsureRouter()` lazy 构造（seed 改变时重建）
- `FillChunk(ChunkOriginVoxel, ChunkSize, OutBlocks)` 三 pass：
  - **Pass 1**：density → 主体方块（stone/water/air/bedrock），状态机区分 sea Water vs cave Air；fast path 处理"全 stone"和"全 air"列；post-process 修复 cave entrance 穿透 sea level 时的水洞 bug
  - **Pass 2**：surface system 按 biome 替换顶层 stone（仅当本 chunk 不是"完全在 stone 之下"时）
  - **Pass 3**：feature placer 撒树 + 矿（debug biome 模式下跳过）
- `IsBlockSolidAt(WorldVoxel)` —— 单点 opacity 查询，给 GreedyChunk 跨 chunk 边界查邻居用

### `Voxel/Chunk/`

#### `ChunkBase.{h,cpp}` (`AChunkBase`, `UCLASS(Abstract)`)
- 拥有 `UProceduralMeshComponent`
- 两个 `FChunkMeshData` buffer + 两个 `Material` slot（textured / colored）
- 抽象虚函数：`GenerateVoxelData()`、`GenerateMesh()`、`ModifyVoxelData()`、`GetVoxel()`
- `BeginPlay`: GenerateVoxelData → GenerateMesh → ApplyMesh
- `ApplyMesh`: 创建 2 个 mesh sections（section 0 用 `Material`，section 1 用 `MaterialColor`）

#### `GreedyChunk.{h,cpp}` (`AGreedyChunk : AChunkBase`)
- 唯一具体实现
- `GenerateVoxelData()` 调 `UWorldGenerator::FillChunk` 拿 `EBlock[Size³]`
- `GenerateMesh()`：Greedy meshing 三轴扫描 + 矩形扩展
- `IsColoredBlock(B)` 谓词：`Grass/Dirt/Stone/Wood/Leaf` 走贴图，其余走纯色
- `GetTextureIndex(Block, Normal)`：贴图方块的 texture array 索引（vertex Color.A）
- `GetBlockColor(Block)`：纯色方块的 RGB（vertex Color.RGB）
- **关键修复（chunk 边界面剔除）**：
  - `GetBlock(Index)` OOB 时调 `WorldGenerator->IsBlockSolidAt`：固体 → 返回 Stone（让 mask 视为不透明 → 剔除 face），非固体 → Air
  - mask 逻辑只为 in-chunk 的 opaque 方块生面（`bCurrentInChunk` / `bCompareInChunk` 检查），让邻居 chunk 自己出对面 → 不双重渲染

### `Voxel/CloudLayer.{h,cpp}` (`ACloudLayer`)
- 单 actor，`UProceduralMeshComponent`
- 2D Perlin 噪声决定哪些 cell 是云
- 每个云 cell 生成 12 块边的 4 块厚 cube（top/bottom 必生成，sides 仅与非云邻居共面时生成）
- Tick 累积 `WindOffsetBlocks`，每秒 / 玩家移动够远时重建 mesh
- 重建时 actor 位置跟随玩家（保证视野内总有云）

## 运行时流程

```
APawn::BeginPlay (UE 内部)
    │
AMyGameModeBase::BeginPlay
    ├─ WorldSeed=0 → auto seed
    ├─ WorldGen->WorldSeed/bEnableCaves/ContinentBias/bDebugBiomeColors
    ├─ ChunkWorldSubsystem->ChunkType/Material/.../MaxSpawnsPerTick/.../StartStreaming()
    │     │
    │     UChunkWorldSubsystem::StartStreaming
    │       - 立即 TickStreaming() 一次（spawn player 周边几个 chunk）
    │       - SetTimer(StreamingTickInterval) → 周期 TickStreaming
    │           │
    │           UChunkWorldSubsystem::TickStreaming
    │             - 算期望 chunk 集
    │             - 卸载离开的
    │             - 按距排序 + 每 tick 最多 N 个 spawn
    │             - SpawnChunkAtGrid() → SpawnActorDeferred + FinishSpawningActor
    │                 │
    │                 AChunkBase::BeginPlay
    │                   - GenerateVoxelData (子类实现)
    │                       │
    │                       AGreedyChunk::GenerateVoxelData
    │                         → UWorldGenerator::FillChunk
    │                             - Pass 1 (density → block)
    │                             - Pass 2 (surface system)
    │                             - Pass 3 (features)
    │                   - GenerateMesh (子类实现)
    │                       │
    │                       AGreedyChunk::GenerateMesh
    │                         → 三轴 greedy meshing
    │                         → CreateQuad → 写入 MeshData / MeshDataColor
    │                   - ApplyMesh
    │                       → CreateMeshSection × 2
    │
    └─ if bSpawnClouds: SpawnActor<ACloudLayer> + 推参数
         │
         ACloudLayer::BeginPlay → 立即 Rebuild() spawn 云
         ACloudLayer::Tick → 累积 WindOffset；周期 Rebuild
```

## 关键数据结构 / 约定

### 坐标系
- **UE 世界**：X、Y 水平，**Z 垂直**
- **MC 世界**：X、Z 水平，**Y 垂直**
- 桥梁：UE.X = MC.X，UE.Y = MC.Z，**UE.Z = MC.Y**
- `FillChunk` 在调密度函数时做坐标转换（`McX/McY/McZ`）
- 1 块 = 100 UE 单位
- 海平面 `SeaLevel = 63`，世界范围 `MinY=-64..MaxY=320`（可改）

### 三维 EBlock 数组索引
`OutBlocks[lx + ChunkSize * (ly + ChunkSize * lz)]`，lx 最内层（cache friendly 取决于扫描方向）

### EBlock 顺序约束
- `Null/Air/Stone/Dirt/Grass/Wood/Leaf` 顺序**载荷重要**（GreedyChunk 的 texture array 索引）
- 后续追加的方块（Sand/Sandstone/Snow/Water/Bedrock/CoalOre/.../DebugBiomePlains/DebugBiomeMountains）自动走纯色路径，没顺序约束
- 新增贴图方块要同时改 `GreedyChunk::GetTextureIndex` 并加到 `IsColoredBlock` 的 false 分支

### 渲染分支
- vertex `Color.A` 用作 texture array 索引（贴图路径）
- vertex `Color.RGB` 用作 emissive color（纯色路径）
- 两个 mesh section 分别使用 `Material` 和 `MaterialColor`
- `MaterialColor` 必须是 Unlit + VertexColor.RGB → EmissiveColor（详见 `EDITOR_SETUP.md`）

### 状态机：sea Water vs cave Air
（Mojang 用 aquifer 解决；我们没 aquifer，用启发式）
- Pass 1 自上而下扫每列。`bSeenStone` 状态：
  - 还没碰到 stone（state=false）：density ≤ 0 的 cell → Water if y ≤ 63 else Air
  - 已经碰到 stone（state=true）：density ≤ 0 的 cell → Air（cave，无视 y）
- 状态初值 `bSeenStoneInit` 用 `BlockAbove` + `EstimatedSurface` 启发式确定
- Post-process：列内整段没 stone（cave entrance 一路挖穿）+ chunk 顶低于估算 surface → 把 Water 全改 Air

## 跨模块依赖

```
WorldGenerator.cpp 包含：
    NoiseRouter, BiomeSource, SurfaceSystem, FeaturePlacer, Hash, Noises, OctavedNoise

NoiseRouter.cpp 包含：
    Hash, Noises, OctavedNoise, CubicSpline, DensityFunction, TerrainSplines

TerrainSplines.cpp 包含：
    CubicSpline, DensityFunction

DensityFunction.cpp 包含：
    OctavedNoise, CubicSpline

CubicSpline.cpp 包含：
    DensityFunction (forward decl in header)

GreedyChunk.cpp 包含：
    ChunkBase, WorldGenerator (用 IsBlockSolidAt 做边界剔除)

ChunkWorldSubsystem.cpp 包含：
    ChunkBase

CloudLayer.cpp 包含：
    FastNoiseLite (直接用 Perlin)

MyGameModeBase.cpp 包含：
    ChunkWorldSubsystem, WorldGenerator, CloudLayer
```
