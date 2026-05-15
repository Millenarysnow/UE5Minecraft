# 项目文档

UE5 voxel 项目，目标是把 Minecraft 1.21 (Java Edition) 的地形生成管线移植到 Unreal。

## 文档导航

| 文件 | 内容 |
|---|---|
| [ARCHITECTURE.md](ARCHITECTURE.md) | 代码结构、模块职责、运行时流程、数据流 |
| [PIPELINE.md](PIPELINE.md) | 地形生成算法 / 公式 / spline 锚点 / cave 公式 / biome 阈值 / 树和矿放置 |
| [EDITOR_SETUP.md](EDITOR_SETUP.md) | 必须在 UE 编辑器里手动做的事（材质 / BP_MyGameMode 配置） |
| [STATUS.md](STATUS.md) | 阶段进度 / 已知限制 / 路线图 / 下一步建议 |

## 入门顺序（给新 agent）

1. 先看仓库根的 `CLAUDE.md` —— 项目最高层概览
2. 再看本目录 `ARCHITECTURE.md` —— 详细代码结构
3. 接手新任务前先查 `STATUS.md` —— 别重做已经搞定 / 已知坏掉的东西
4. 改任何东西前看一眼 `PIPELINE.md` —— 公式很多坑，照搬 Mojang 但有 v1 简化

## 项目位置 / 关键路径

- 仓库根：`E:\Repo\UE5Minecraft`
- 引擎：UE 5.4.4，路径 `C:\Program Files\Epic Games\UE_5.4`
- 主要源码：`Source/VoxelWorld/Private/`
  - `Game/` — GameMode / PlayerController
  - `Voxel/Generation/` — 地形生成管线（这是工作重心）
  - `Voxel/Chunk/` — chunk actor + greedy meshing
  - `Voxel/CloudLayer.{h,cpp}` — 3D 云层
  - `Voxel/ChunkWorldSubsystem.{h,cpp}` — chunk 加载 / 卸载流式系统
  - `Voxel/Utils/` — 坐标工具、枚举、FastNoiseLite

- Mojang 真值参考（只读，已有，next agent 应该能直接读）：
  `D:\Minecraft\MCP-Reborn-1.21\src\main\java`
  关键文件：`NoiseRouterData.java`、`TerrainProvider.java`、`OverworldBiomeBuilder.java`、`SurfaceSystem.java`、`Noises.java`、`NoiseData.java`、`NormalNoise.java`、`PerlinNoise.java`、`CubicSpline.java`

## 当前状态摘要

✅ Phase 0-9 完成。基本管线 + 动态加载 + 云全部 ship。

🚧 当前已知主要限制（详见 STATUS.md）：
- 无 spaghetti / noodle / pillar 洞穴（v1 仅 cheese + cave_entrance，且 entrance 视觉效果不强）
- 无 aquifer
- 无 vein system
- 无 structure
- biome 决策树而非 6D nearest-box
- 未做 4×8×4 trilinear 插值

📍 下一阶段（用户已 OK）：**Phase 10 — 完整 MC 洞穴系统**
（spaghetti_2d、noodle、pillar、cave_layer、underground 装配齐全）

## 用户偏好（基于历次对话观察）

- 中文交流，代码注释也用中文（与既有风格一致）
- 拒绝 emoji（除非显式请求）
- 喜欢"先了解再动手"：解释清思路再实施
- 喜欢 phase-by-phase 推进，每阶段完成后停下让他验证
- 不喜欢一次性大批量改动；可逆 / 可验证的小步迭代
- 不喜欢冗余 / 半成品代码
- 性能上能接受 ~30ms/chunk 量级，不强求 vanilla MC 的极致优化
