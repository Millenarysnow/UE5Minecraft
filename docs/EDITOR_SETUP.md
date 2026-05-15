# Editor Setup

UE 编辑器里必须手动做的事情。代码里能做的都做了；剩下的是材质 + BP 配置。

## 必需的材质

### `M_TextureArray_Inst`（已存在）

位于 `Content/_Game/Material/`。这是教程留下来的 texture array 材质，用于贴图方块（Grass / Dirt / Stone / Wood / Leaf）。texture array 索引由 vertex `Color.A` 提供。**不要动这个**，它已经配好了。

如果 ChunkBase 的 Material slot 突然丢失，重新指给它即可。

### `M_VoxelColor`（已存在 / 用户 Phase 0+ 创建）

位于 `Content/_Game/Material/`。给"还没有贴图的方块"用的纯色材质。

如果丢了，重建步骤：
1. 内容浏览器右键 → Material → 命名 `M_VoxelColor`
2. 双击打开，**Details 面板**：
   - Shading Model = `Unlit`
3. 节点编辑器：
   - 加 `Vertex Color` 节点
   - 把 `VertexColor` 的 `RGB` 输出连到主节点的 **Emissive Color**
4. Save

### `M_Cloud`（用户需要新建）

云层用的半透明材质。

1. 内容浏览器右键 → Material → 命名 `M_Cloud`
2. 双击打开，**Details 面板**：
   - **Blend Mode** = `Translucent`
   - **Shading Model** = `Unlit`
3. 节点编辑器：
   - 加 `Constant3Vector` 节点，颜色 = 白 `(1, 1, 1)`
   - 连到主节点的 **Emissive Color**
   - 加 `Constant` 节点，值 `0.7`
   - 连到主节点的 **Opacity**
4. Save

如果你想做"云投影 / 真实云"，用 Default Lit 也可以，但 Unlit 性能更好且 MC 风。

## `BP_MyGameMode` 配置清单

`Content/_Game/Blueprints/Player/BP_MyGameMode`。打开 → Class Defaults / Details 面板。

### `World` 类别

| 字段 | 默认 | 必须 | 说明 |
|---|---|---|---|
| **Chunk Type** | None | ✅ | 指给 `BP_ChunkBase`（或任意 `AGreedyChunk` 派生 BP） |
| **Draw Distance** | 8 | | 周围 chunk 半径。8 = 17×17 列 ≈ 544 块视野 |
| **Material** | None | ✅ | 指给 `M_TextureArray_Inst` |
| **Material Color** | None | ✅ | 指给 `M_VoxelColor` |
| **Size** | 32 | | 立方区块边长（块）。改了之后整个垂直 chunk 数量会变 |
| **Min World Y** | -64 | | 世界最低 y。Mojang spec |
| **Max World Y** | 320 | | 世界最高 y。Mojang spec |
| **Max Spawns Per Tick** | 2 | | 每流式 tick 最多 spawn 几个 chunk。开局快慢 / 单 tick 卡顿权衡 |
| **Streaming Tick Interval** | 0.1 | | 流式 tick 间隔（秒） |
| **World Seed** | 0 | | 0 = 自动用系统时间。非 0 = 显式（复现指定世界） |
| **Enable Caves** | true | | 关掉地下没空腔，生成更快（biome 验证用） |
| **Continent Bias** | 0.1 | | 大陆偏置。0 = Mojang spec（plains 多在水下）。0.1 推荐 |

### `Debug` 类别

| 字段 | 默认 | 说明 |
|---|---|---|
| **Debug Biome Colors** | false | 开启后 surface 用 biome 标记块全染色，便于从空中看 biome 分布。**调试模式自动跳过种树** |

### `Cloud` 类别

| 字段 | 默认 | 必须 | 说明 |
|---|---|---|---|
| **Spawn Clouds** | true | | 是否自动 spawn 云层 |
| **Cloud Material** | None | (推荐) | 指给 `M_Cloud`（如果没指 / `Spawn Clouds=false`，云不显示）|
| **Cloud Height** | 192 | | 云层 Y 高度（块）。Mojang spec |
| **Cloud Wind Speed Blocks Per Sec** | 0.6 | | 风速。Mojang spec |

### Compile + Save 之后才生效

每次改 BP 字段，记得点编辑器左上角的 **Compile** + **Save**。

## 默认 chunk BP

仓库里 `Content/_Game/Blueprints/BP_ChunkBase.uasset` 是个 chunk BP。它应该 derive from `AGreedyChunk`（C++ 类）。如果哪天它不见了，重建：

1. 内容浏览器右键 → Blueprint Class
2. Search → 选 `GreedyChunk`
3. 命名 `BP_ChunkBase`
4. 这个 BP 不需要任何额外配置（属性都从父类继承，运行时由 ChunkWorldSubsystem 设置）

## TestMap

`Content/_Game/Maps/TestMap.umap` 是默认地图。GameMode 已经在 `Config/DefaultEngine.ini` 里设过了：

```ini
[/Script/EngineSettings.GameMapsSettings]
GameDefaultMap=/Game/_Game/Maps/TestMap.TestMap
EditorStartupMap=/Game/_Game/Maps/TestMap.TestMap
```

地图里应该有：
- `BP_MyGameMode` 作为 Game Mode Override（或全局默认）
- `BP_Sky`（一个简单天空盒，给场景一个天空）
- 默认光照（DirectionalLight、SkyAtmosphere、SkyLight、ExponentialHeightFog 等）

如果地图被搞坏，新建一个空地图，把 GameMode Override 设为 `BP_MyGameMode` 就行 —— 玩家 spawn 后 chunk 会自动加载，连地都不用铺。

## 验证清单

第一次启动 PIE 应该看到：
- ✅ 玩家周围 chunk 在加载（绿草地 / 海洋 / 山脉混合）
- ✅ 云层在天空（白色，缓慢飘移）
- ✅ 没有 checker 占位材质（如果看到 = `MaterialColor` 没指对 `M_VoxelColor`）
- ✅ 水是蓝色（如果看到 z-fight / stone 纹理 = chunk 边界面剔除 bug 复发了）
- ✅ 山顶有偶尔的 oak（Plains/Forest）/ spruce（SnowyPlains/Mountains）
- ✅ 矿可以挖出来：表层下煤 + 深处铁/钻石

如果某项缺，先检查 BP_MyGameMode 的对应字段。

## 性能调节速查

| 现象 | 调什么 |
|---|---|
| 开局太慢 | DrawDistance ↓（4-6） / MaxSpawnsPerTick ↑（4） |
| 卡顿 | MaxSpawnsPerTick ↓（1） / StreamingTickInterval ↑（0.2） |
| 云不够多 / 看不到 | CloudMaterial 的 Opacity ↑ / Cloud Height ↓ / `M_Cloud` Threshold ↓ |
| 没看到 desert / snowy | 多刷几次 seed / 改 ContinentBias / 改 BiomeSource 阈值 |
| Plains 全在水下 | ContinentBias ↑（0.15-0.2）|
| 生成太多 ocean | ContinentBias ↑ |
