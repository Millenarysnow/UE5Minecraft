# Status / Progress / Roadmap

项目阶段进度 + 已知限制 + 下一步建议。

## 总览

✅ Phase 0-9 + 几轮关键 bug fix 全部 ship。基础地形管线 + 动态加载 + 云完成。

🚧 用户已知缺陷有数项（见下"已知限制"），但都不阻塞玩。

📍 下一阶段（用户已批准）：**Phase 10 — 完整 MC 洞穴系统**。

## Phase-by-phase 历史

每个 phase 一个 commit-worthy 的功能块。前面 phase 是早期搭基础，后面是逐步加 polish。

| # | 名字 | 主要产出 |
|---|---|---|
| 0 | Cleanup + 脚手架 | 删 MarchingChunk/NaiveChunk/ChunkWorld/GenerateTreeSubsystem/bezier；ChunkBase 虚函数收敛；ChunkWorldSubsystem 改列布局；EBlock 扩展；WorldGenerator 桩 |
| 0+ | 纯色路径 | 加 MaterialColor 槽 + 第二份 mesh section；GreedyChunk 路由贴图块/纯色块；新 EBlock 配色 |
| 1 | 噪声原语 + DF ADT | OctavedNoise / Noises 表 / CubicSpline / DensityFunction 工厂 / Phase1 烟测 |
| 2 | Spline + final_density | TerrainSplines（Mojang TerrainProvider 完整移植）/ NoiseRouter 装配 / column cache / slide / Phase2 烟测 / base_3d_noise 替代品 |
| 3 | Surface system | SurfaceSystem 类 / FillChunk 双 pass / 表层 stone 替换 grass+dirt 或 sand |
| 4 | Biome | BiomeSource 决策树 / SurfaceSystem 按 biome 派发 / 调试着色块 |
| 5 | Cheese caves | NoiseRouter 加 cheese 公式 / fast path 调整 |
| 5b | Cave entrances | 加 CaveEntrance 噪声 / 5×entrance 项 / yClampedGradient 抑制 |
| 6 | Trees | FeaturePlacer.PlaceTrees / oak / spruce / per-chunk 决定性 RNG |
| 7 | Ores | FeaturePlacer.PlaceOres / coal / iron / diamond / 三角分布 |
| 8 | 收尾 | CLAUDE.md 重写 / 残留 grep 清理 |
| 9 | 动态加载 | ChunkWorldSubsystem.StartStreaming / Tick + 距离优先 spawn / 远 chunk despawn |

### 关键 bug fix（不算独立 phase 但很重要）

- **Cave water bug**：cave entrance 穿透 sea level 后整列被错判成 sea Water。修法：post-process 检测"列内无 stone + chunk 顶低于估算 surface" → Water 转 Air。`WorldGenerator.cpp::FillChunk` slow path 末尾。
- **Chunk 边界面双重渲染 + z-fight**：之前 OOB → Stone 让 mask 用错误 block type 生成对面的 face。修法：mask 逻辑只为 in-chunk opaque 生面；OOB 查询保留用作"两 opaque 不出面"的剔除信号。`GreedyChunk.cpp::GenerateMesh` mask 构建段。
- **ACES tonemapping 让深色变中灰**：`Bedrock` 用 RGB(40,40,45) 显示出来约等于灰色。修成 `(6, 6, 8)` 后才像石头底色。
- **WorldSeed=0 总是相同地形**：约定 0 = 系统时间自动种子。非 0 = 显式可复现。`MyGameModeBase.cpp::BeginPlay`。
- **ContinentBias**：plains 默认 surface ~y=50，全在水下。加了非 Mojang 的 BP 参数 `ContinentBias`（默认 0.1）抬升 OffsetDF 让 plains 浮出水面。

### Phase 10 暂定（用户批准）

完整 MC 洞穴系统：
- `Noises::Spaghetti2D / Spaghetti2DElevation / Spaghetti2DModulator / Spaghetti2DThickness / Spaghetti3D1/2/Rarity/Thickness / SpaghettiRoughness / SpaghettiRoughnessModulator / Pillar / PillarRareness / PillarThickness / CaveLayer`
- `DensityFunction` 加 `WeirdScaledSampler / Mapped` 节点
- `NoiseRouter` 装配完整 `underground` 函数（cheese + spaghetti + pillar + cave_layer）
- `NoiseRouter` 装配完整 `entrances` 函数（v1 简化版的进阶）
- 用 `rangeChoice` 在 sloped_cheese 大于阈值时切到 `underground`，小于时用 `min(slopedCheese, 5·entrances)`
- 看是否上 `noodle` 细蛀洞

工作量估计：3-4 倍 Phase 5 的代码量，主要是新 DensityFunction 节点 + 装配。

## 已知限制（按重要性）

### 视觉 / 玩法明显的

1. **没 spaghetti / noodle / pillar 洞穴**：现在地下只有圆形 cheese 大洞 + 偶尔的 cave_entrance 表层切口。MC 那种长条蜿蜒洞还没。Phase 10 解决。
2. **Cave entrance 表层入口效果弱**：cave_entrance 噪声 + 5× 系数下确实在公式中，但视觉上很少看到明显的"洞口在山腰"。可能是 entrance noise 阈值或缩放选错。Phase 10 时一起调。
3. **没 aquifer**：洞穴里默认是 air；穿过 sea level 的 cave entrance 由 post-process 修成 air。MC 的"地下水池"（aquifer）没有。Phase 11 可做。
4. **ContinentBias 偏离 Mojang spec**：默认 0.1（非 0）让 plains 浮出水面。0.0 接近 Mojang，但小视野下大部分浅水。
5. **生物群系决策树而非 6D nearest-box**：阈值放宽过让稀有 biome 在小视野能看到。生成的 biome 边界可能跟原版 MC 不完全一致。
6. **Trees 限制在单 chunk**：跨 chunk 的叶子被裁掉；chunk 顶上空间不够整棵树就跳过。结果有些山顶/边界看不到树。

### 不影响玩法但偏离真值

7. **没 4×8×4 trilinear 插值**：每方块独立评估密度。Mojang 用 4×8×4 grid + 三线性插值快 ~128×。我们 ~30ms/chunk 还能接受。
8. **没 vein system**：Mojang 的"矿脉"（ore_veininess + vein_a/b）跟 placed_feature 是两套独立系统，我们只做了后者。Phase 12 可做。
9. **没 structure**：村庄、矿洞、要塞、远古城市、海底神殿 ... 全没。
10. **`base_3d_noise` 用 5 octave NormalNoise 替代 Mojang BlendedNoise**：视觉上接近，不 bit-exact。
11. **Climate 没 domain warp**：Mojang 用 SHIFT 噪声做 X/Z 偏移让 biome 边界不轴对齐。我们直接采样。

### 渲染上的小坑

12. **Stone-ore 边界正确剔除（非 bug）**：MC 也这样做。x-ray "看穿石头看矿" 是 MC mod 行为，我们没做。如果要做：在 `GreedyChunk::IsColoredBlock` / mask 逻辑加 `bShowOres` 开关让 ore 不参与 opaque 合并。
13. **Cross-chunk 表层 dirt 缺失**：如果世界 surface 恰好落在 chunk 顶 lz=ChunkSize-1，下面几层 dirt 写不进当前 chunk（应该写到当前 chunk 但 SurfaceSystem 当前列扫只动一列，且 dirt 偶尔越界到下面 chunk）。Phase 12+ 可加 surface 跨 chunk 处理。
14. **"全 stone" fast path 在 caves 开启时关闭**：因 cheese 可能挖空 deep stone。代价：~3× 慢生成。Phase 11+ 可做"轻量 cave 检查"恢复 fast path。

## 路线图

按优先级排：

### 短期（推进核心 MC 还原）

#### Phase 10：完整 MC 洞穴系统 ⬅️ next
- 加 spaghetti_2d / spaghetti_3d / pillar / cave_layer / 各种 modulator 噪声
- 加 `DensityFunctions::WeirdScaledSampler / Mapped` 节点
- 装配完整 `underground()` 函数
- 改进 `entrances()` 装配（加 spaghetti_roughness）
- 用 `rangeChoice`(slopedCheese, 1.5625) 区分 deep underground vs near-surface

#### Phase 11：Aquifer
- 加 aquifer_barrier / fluid_level_floodedness / fluid_level_spread / lava 噪声（Mojang 已经在 NoiseRouterData 里全装好了）
- 实现 `Aquifer::computeSubstance`（Mojang `Aquifer.java`）：根据噪声决定每个区域的水位、是水还是岩浆
- 替换我们 post-process 的简单"列内无 stone → cave Air"启发式
- 移除 ContinentBias 这种 hack

#### Phase 12：完整 surface rules
- Mojang `SurfaceRules` 完整 rule tree（badlands 黏土带、swamp、iceberg）
- 我们的 `SurfaceSystem` 现在只有 5 个简单 case

### 中期（玩起来更"满"）

- **Vein system**：ore_veininess + vein_a/b/gap 噪声 → 大型矿脉
- **更多 biome**：Mojang 64+，我们 6 个。先扩到 12-15 个常见 biome
- **更多树种**：birch / dark oak / cherry / jungle
- **Cross-chunk feature placement**：跨 chunk 的树叶能正确写到邻居（队列 / "feature deferred apply"）
- **Sapling growth check**：检查放置位置周围地形 / 光照（MC 真实行为）
- **简单 structure**：村庄（最小 11 块预制 + 路径）

### 长期（性能 / 质量）

- **4×8×4 trilinear 插值**：节省 ~100× 噪声开销，实现 Mojang 的 `interpolated()`
- **Async chunk gen**：density evaluation 移到 worker thread；mesh 生成在 game thread
- **Chunk 持久化**：玩家修改的方块持久化到磁盘，重新加载不丢
- **LOD**：远处 chunk 用低分辨率 mesh / 不 mesh

## Tunable 参数速查

代码常量（编辑代码改）：

| 参数 | 位置 | 当前值 | 含义 |
|---|---|---|---|
| GLOBAL_OFFSET | NoiseRouter.cpp 内匿名 namespace | -0.50375 | Mojang spec 全局 offset |
| Base3DMax | NoiseRouter::EstimateColumnBounds | 2.5 | base3D 经验上界 |
| Top slide range | ApplySlide | y=240..256 | Mojang spec |
| Bottom slide range | ApplySlide | y=-64..-40 | Mojang spec |
| Cheese cave 表层保护 | DensityInColumn | clamp(1.5 - 0.64·sloped, 0, 0.5) | Mojang spec |
| Cave entrance 5× 系数 | DensityInColumn | 5.0 | Mojang spec |
| Cave entrance y-gradient | DensityInColumn | y=-10..30, 0.3..0.0 | Mojang spec |
| Surface depth | SurfaceSystem | clamp(noise·2.75+3, 2, 6) | Mojang spec |
| Tree counts | FeaturePlacer::PlaceTrees | switch(biome) | 我们调的 |
| Ore counts | FeaturePlacer::PlaceOres | OreDefs[] | 取自 Mojang placed_feature |
| Biome thresholds | BiomeSource | 决策树 | **放宽过的，非 Mojang spec** |

BP 暴露（编辑器改）：见 [EDITOR_SETUP.md](EDITOR_SETUP.md)

## 给下一个 agent 的注意事项

1. **改公式前先看 Mojang 真值**。我们的所有公式 / 锚点都注明了 Mojang 出处。如果有疑问，对照 `D:\Minecraft\MCP-Reborn-1.21\src\main\java`。
2. **不要碰 EBlock 前 7 个 enum 值的顺序**（NaiveChunk 删了，但 `GreedyChunk::GetTextureIndex` 仍然按这个 mapping）。新方块追加在末尾。
3. **改 SurfaceSystem 时也要更新 debug 着色 case**。SurfaceSystem.cpp 里有两套派发逻辑（debug 和正常），加新 biome 要同步两边。
4. **新 BP 参数走"MyGameMode UPROPERTY → BeginPlay 推到 Subsystem"** 的模式。不要让 Subsystem 自己读 BP。
5. **改完做小步验证**。前几次 phase 9 / chunk 边界剔除的修法都需要 PIE 看一眼才知道对不对。bug 都是视觉肉眼可见的，跑一下能看出来。
6. **用 [Phase1] / [Phase2] 烟测日志**。`UWorldGenerator::LogPhase1SmokeTest` / `LogPhase2SmokeTest` 在第一个 chunk 加载时输出。改公式后看这些数值能判断很多事。
7. **WorldSeed=0 测试时改成固定数字**。开发期间想要可重现就把 seed 写死，bug 才能稳定复现。
