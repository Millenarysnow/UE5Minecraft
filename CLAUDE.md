# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Unreal Engine **5.4.4** project (`VoxelWorld.uproject`) — a Minecraft-style voxel world. C++ module name: `VoxelWorld`. Default map: `Content/_Game/Maps/TestMap.umap`.

The terrain generation is a **simplified port of Minecraft 1.21's `NoiseRouterData` pipeline**: octaved Perlin climate noise → cubic-Hermite splines → density function tree → greedy-meshed voxel chunks. See "Terrain pipeline" below for the layered breakdown.

Module dependencies (`Source/VoxelWorld/VoxelWorld.Build.cs`):
- Public: `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`
- Private: `ProceduralMeshComponent`

Reference Mojang sources (read-only, on disk at `D:\Minecraft\MCP-Reborn-1.21\src\main\java`): `NoiseRouterData.java`, `TerrainProvider.java`, `OverworldBiomeBuilder.java`, `SurfaceSystem.java`, `Noises.java`, `NoiseData.java`, `NormalNoise.java`, `PerlinNoise.java`, `CubicSpline.java`. Our port files cite the corresponding Mojang class in their header docs.

## Build & Run

Standard UBT-driven UE project. From the project root:

```powershell
# Build the editor target:
& "C:\Program Files\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat" VoxelWorldEditor Win64 Development -Project="E:\Repo\UE5Minecraft\VoxelWorld.uproject" -WaitMutex -FromMsBuild

# Launch the editor:
& "C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe" "E:\Repo\UE5Minecraft\VoxelWorld.uproject"
```

There is no test framework wired up. Verification is by PIE inspection + structured `[Phase1]` / `[Phase2]` smoke logs in `Output Log` (printed once at first chunk generation).

## Architecture

### Source layout (`Source/VoxelWorld/Private/`)

```
Game/
  MyGameModeBase.{h,cpp}        ← entry point; reads BP_MyGameMode props (seed/draw distance/etc.),
                                  pushes them into the subsystems, kicks off generation
  MyPlayerController.{h,cpp}    ← thin player controller stub
Voxel/
  ChunkWorldSubsystem.{h,cpp}   ← UGameInstanceSubsystem: owns the chunk actor map,
                                  drives spawn/despawn (Phase 9 dynamic streaming)
  Chunk/
    ChunkBase.{h,cpp}           ← Abstract actor (PURE_VIRTUAL GenerateVoxelData / GenerateMesh).
                                  Owns the ProceduralMeshComponent + two FChunkMeshData buffers
                                  (textured vs vertex-color) + two material slots
    GreedyChunk.{h,cpp}         ← The only concrete chunk impl. Greedy meshing with two output
                                  sections: textured (Grass/Dirt/Stone/Wood/Leaf via texture array)
                                  and colored (everything else via VertexColor.RGB)
  Generation/                    ← terrain pipeline (port of Mojang 1.21 NoiseRouterData)
    Hash.h                       ← SplitMix64 + key-derived seeds + NoiseWrap
    OctavedNoise.{h,cpp}         ← FOctavedPerlin + FNormalNoise (== Mojang NormalNoise)
    Noises.{h,cpp}               ← named noise parameter table (== Mojang NoiseData.bootstrap)
    CubicSpline.{h,cpp}          ← nested Hermite spline (== Mojang CubicSpline.Multipoint)
    DensityFunction.{h,cpp}      ← node ADT: Constant/Add/Mul/Min/Max/Clamp/Abs/Cube/HalfNeg/
                                  QuarterNeg/YClampedGradient/Noise2D/Noise3D/ShiftedNoise2D/Spline
    TerrainSplines.{h,cpp}       ← BuildOffset/BuildFactor/BuildJaggedness with Mojang spline
                                  anchors (port of TerrainProvider.java)
    NoiseRouter.{h,cpp}          ← assembles climate/depth/factor/jaggedness DFs,
                                  computes final_density (sloped_cheese + slide + cheese cave +
                                  cave entrance). Per-column FColumnState cache for hot loop
    SurfaceSystem.{h,cpp}        ← finds top stone in a column, replaces 1+SurfaceDepth blocks
                                  with biome surface (grass/dirt, sand, snow, etc.)
    BiomeSource.{h,cpp}          ← decision-tree biome lookup (6 biomes; loose thresholds tuned
                                  for our Perlin σ ≈ 0.3, not Mojang's strict spec values)
    FeaturePlacer.{h,cpp}        ← per-chunk deterministic RNG; trees (oak/spruce) + ores
                                  (coal/iron/diamond)
    WorldGenerator.{h,cpp}       ← UGameInstanceSubsystem entry. FillChunk does:
                                    Pass 1 (density → primary block, top-down state machine
                                            to disambiguate sea Water vs cave Air)
                                    Pass 2 (SurfaceSystem on columns that contain world surface)
                                    Pass 3 (FeaturePlacer trees + ores)
  Utils/
    Enums.h                      ← EBlock, EDirection, EBiome. EBlock has DebugBiomePlains/
                                  DebugBiomeMountains markers for the BP debug toggle
    ChunkMeshData.h              ← Vertices/Triangles/Normals/Colors/UVO buffers
    VoxelFunctionLibrary.{h,cpp} ← world↔chunk↔local conversions (handles negative coords)
    FastNoiseLite.h              ← vendored single-header Perlin (used by FOctavedPerlin)
```

### Runtime flow (top-down)

1. `AMyGameModeBase::BeginPlay` reads `BP_MyGameMode` properties (`WorldSeed` (0 = auto),
   `DrawDistance`, `Size`, `MinWorldY/MaxWorldY`, `bEnableCaves`, `bDebugBiomeColors`,
   `Material`, `MaterialColor`), pushes them to `UWorldGenerator` and `UChunkWorldSubsystem`,
   calls `ChunkWorldSubsystem->StartStreaming()`.
2. `UChunkWorldSubsystem::StartStreaming()` registers a periodic timer that drives
   `TickStreaming()`. On each tick: locate the player pawn, compute the desired chunk set
   `[playerChunk ± DrawDistance]² × verticalRange`, spawn missing chunks (rate-limited via
   `MaxSpawnsPerTick`, prioritized by distance to player), despawn anything outside the set.
3. `AChunkBase::BeginPlay` calls the virtual pipeline `GenerateVoxelData → GenerateMesh →
   ApplyMesh`. `GreedyChunk::GenerateVoxelData` delegates to `UWorldGenerator::FillChunk`.
4. `UWorldGenerator::FillChunk` builds the column-cache once per `(lx, ly)`, runs the three
   passes above, and emits an `EBlock[ChunkSize³]` array. `GreedyChunk::GenerateMesh` then
   greedy-meshes that into two `FChunkMeshData` buffers (textured + colored), and
   `AChunkBase::ApplyMesh` creates two `ProceduralMeshComponent` sections with separate
   materials.
5. Block edits go `AChunkBase::ModifyVoxel` → subclass `ModifyVoxelData` → `ClearMesh →
   GenerateMesh → ApplyMesh` (full chunk remesh, no incremental update).

### Density function pipeline (Mojang correspondence)

For each `(x, z)` column the router exposes:
- **Climate samplers** (`Continentalness/ErosionVal/Ridges/RidgesFolded/Temperature/Humidity`)
  via `DF::Noise2D` over the corresponding `FNormalNoise` instance. **No domain warp** in v1
  (Mojang's `shiftA/shiftB` pair skipped — biome borders may look slightly axis-aligned).
- **Spline outputs** (`Offset/Factor/Jaggedness`) via `TerrainSplines::Build*`. Anchors
  copied verbatim from `TerrainProvider.java` (no amplified mode).
- **`final_density(x, y, z)`** computed by `DensityInColumn(FColumnState, y)`:
  ```
  ygrad      = y_clamped_gradient(MinY..MaxY, 1.5..-1.5)(y)
  depth      = ygrad + offset
  jagTerm    = jaggedness · half_negative(JaggedNoise(x*1500, z*1500))
  inner      = (depth + jagTerm) · factor
  slopedNoB  = 4 · qn(inner)
  base3D     = Base3DNoise(x*1.0, y*0.5, z*1.0)        # replaces Mojang BlendedNoise
  slopedCh   = slopedNoB + base3D
  cheeseTerm = clamp(0.27 + CaveCheese(x, y*0.6666, z), -1, 1)
             + clamp(1.5 - 0.64·slopedCh, 0, 0.5)
  yEntGrad   = y_clamped_gradient(-10..30, 0.3..0.0)(y)
  entrance   = CaveEntrance(x*0.75, y*0.5, z*0.75) + 0.37 + yEntGrad
  density    = min(slopedCh, cheeseTerm, 5·entrance)
  return ApplySlide(density, y)   # smooth top/bottom transitions
  ```
- **Optimization** (`EstimateColumnBounds`): from offset+factor alone, derive `StoneYMax`
  (below = guaranteed stone) and `AirYMin` (above = guaranteed air/water). Lets `FillChunk`
  short-circuit "all-air" chunks. The "all-stone" fast path is only safe when caves are
  disabled (since cheese can carve through deep stone).

### Sea Water vs cave Air state machine

Inside `FillChunk`'s slow path, columns are processed top-down with `bSeenStone` state:
- Above any stone hit: `density ≤ 0` → `Water` if `y ≤ SeaLevel` else `Air` (sky).
- After the first stone: `density ≤ 0` → `Air` (cave), regardless of y.

Initial state (at `lz = ChunkSize-1`) is set by an offset-based surface estimate combined
with a single density sample at `AboveChunkUEz` — see the comment block at top of
`FillChunk`'s pre-pass section.

### Chunk Y range

- `MinWorldY = -64` and `MaxWorldY = 320` (Mojang spec) by default; configurable in
  `BP_MyGameMode`. With `Size = 32`, that's 12 vertical chunks per `(cx, cy)` column
  (cz from -2 to 9).
- `SeaLevel = 63` (Mojang spec); compile-time constant in `FNoiseRouter`.

### Cross-chunk coordinate conventions

- One block = **100 Unreal units**.
- **UE.X = MC.X**, **UE.Y = MC.Z**, **UE.Z = MC.Y**. The density functions are written in
  MC coordinates (Y is up); `FillChunk` does the swap when feeding `(McX, McY, McZ)` into
  the router.
- `Chunks` is keyed on `FIntVector` chunk coords. Conversions in
  `VoxelFunctionLibrary::WorldToChunkPosition / WorldToLocalBlockPosition` round away from
  zero for negative coordinates — copy that pattern when adding new conversions.

### Subsystem access

`UWorldGenerator::Get(WorldContext)` and `UChunkWorldSubsystem::Get(WorldContext)` static
helpers fetch via `World->GetGameInstance()->GetSubsystem<...>()`. Use these from anywhere
with a world context.

### Greedy meshing & rendering

`GreedyChunk::GenerateMesh` walks 3 axes × 2 directions, builds a 2D mask of
`FMask{Block, Normal}` per slice, expands rectangular runs of equal masks into single quads.
`CompareMask` is the equality predicate.

Routing per block:
- **Textured blocks** (`Grass/Dirt/Stone/Wood/Leaf`) — vertex `Color.A = GetTextureIndex(Block, Normal)`,
  RGB unused. Material slot 0 = `M_TextureArray_Inst` samples the array by index.
- **Colored blocks** (everything else, including `Sand/Snow/Water/Bedrock/SpruceLog/SpruceLeaf/
  Coal/Iron/DiamondOre/DebugBiomePlains/DebugBiomeMountains`) — vertex `Color.RGB =
  GetBlockColor(Block)`, A=255. Material slot 1 = `M_VoxelColor` (Unlit, VertexColor.RGB
  → EmissiveColor).

This means **adding a textured block** requires updating both `GetTextureIndex` (return the
texture-array index) and **NOT** adding it to `IsColoredBlock`. **Adding a colored block**
just needs a case in `GetBlockColor`; `IsColoredBlock` defaults to true for unlisted blocks.

## Conventions

- Comments are in Chinese; match that style when adding inline notes near existing Chinese comments.
- `EBlock` order is **append-only**. New blocks go at the end. The first 7 entries
  (`Null/Air/Stone/Dirt/Grass/Wood/Leaf`) are referenced as load-bearing texture indices
  in `GreedyChunk::GetTextureIndex`. Don't reorder.
- `EDirection` order matches the legacy NaiveChunk cube vertex tables; even though
  NaiveChunk is gone, several places (`GreedyChunk` axis logic) implicitly assume this
  order. Don't reorder.
- Density / spline values are `double` end-to-end; only `FCubicSpline` interpolates in
  `float` (matches Mojang's spline math, which uses `float`).
- `WorldSeed = 0` in `BP_MyGameMode` is interpreted as "use system time as auto-seed".
  The actually-used seed is logged at startup (`[WorldGen] WorldSeed=0 → auto seed: …`).
  Copy that integer into `WorldSeed` to reproduce a world.
- `bDebugBiomeColors` in `BP_MyGameMode`: when on, surface system replaces top + few
  layers with a biome-specific marker block (e.g. `DebugBiomePlains` solid green,
  `Snow` for SnowyPlains). Trees are skipped in this mode. Used to verify biome
  distribution from the air.

## Known v1 simplifications (vs Mojang)

Listed by importance to "looking like MC". Each is a documented gap, addressable later:

- **No spaghetti / noodle / pillar caves**. Only cheese caves + cave-entrance density. Tunnels
  and ravines are absent. Cave entrances are present but visually subtle.
- **No aquifers**. Caves below sea level are forced to air via the state machine, not
  determined by Mojang's per-region aquifer noise. No underground water pockets.
- **No `BlendedNoise`**. The `base_3d_noise` slot uses a 5-octave `FNormalNoise` substitute,
  hand-tuned scales `(xz=1.0, y=0.5)`. Effects roughly Mojang's terrain wobble but not bit-exact.
- **No domain warp on climate noise**. Biome borders may look slightly axis-aligned at very
  zoomed-in scales.
- **Decision-tree biome lookup**, not Mojang's 6D nearest-box weighted vote. Thresholds
  loosened from spec values (e.g. `T < -0.3` instead of `< -0.45` for snowy) so biomes are
  visible in our small viewport.
- **No 4×8×4 trilinear interpolation** (Mojang `interpolated`). Density is evaluated
  per-block. Generation cost ~22ms/chunk in cave mode; no optimization needed yet.
- **Surface rules are per-biome dispatch, not Mojang's full rule tree**. No badlands
  terracotta bands, no swamp surface, no iceberg.
- **Trees are clamped to single chunk**. Cross-chunk leaf overhang is clipped (skip tree
  if it doesn't fit vertically). 2 tree species (oak / spruce); no birch / dark oak / cherry.
- **Ores use Mojang's placed-feature counts directly** (not 4× scaled for our 32×32 chunks),
  so they're a bit sparser than vanilla. Cluster generation is a 3×3×3 random-walk, not
  Mojang's `OreVeinifier`.

## File pointers (quick reference for common edits)

- Add a new biome: extend `EBiome` in `Enums.h`, add a branch in `BiomeSource::SampleBiome`,
  add cases in `SurfaceSystem::ApplyColumn` (both biome and debug paths), maybe in
  `FeaturePlacer::PlaceTrees` if it gets vegetation.
- Tweak terrain noise feel: edit `Base3DNoise` scale in `NoiseRouter::DensityInColumn`,
  or adjust spline anchors in `TerrainSplines.cpp` (mirrored to Mojang's
  `TerrainProvider.java`).
- Adjust cave density / size: `cheeseTerm` clamps in `NoiseRouter::DensityInColumn`,
  or `5×entrance` multiplier.
- Add a new block type: extend `EBlock` (append!), add color or texture index to
  `GreedyChunk::GetBlockColor` / `GetTextureIndex`, then use it in `SurfaceSystem` /
  `FeaturePlacer` as needed.
- Tune chunk loading: `UChunkWorldSubsystem::TickStreaming` for the streaming policy,
  `MaxSpawnsPerTick` / `StreamingTickInterval` for rate.
