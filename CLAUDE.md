# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Unreal Engine **5.4.4** project (`VoxelWorld.uproject`) — a Minecraft-style voxel world built on `ProceduralMeshComponent`. C++ module name: `VoxelWorld`. Default map: `Content/_Game/Maps/TestMap.umap`.

Module dependencies (`Source/VoxelWorld/VoxelWorld.Build.cs`):
- Public: `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`
- Private: `ProceduralMeshComponent`

## Build & Run

There is no custom build script; this is a standard UBT-driven UE project. From the project root:

```powershell
# Generate Visual Studio project files (right-click .uproject in Explorer → "Generate Visual Studio project files",
# or via UBT directly):
& "C:\Program Files\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat" VoxelWorldEditor Win64 Development -Project="E:\Repo\UE5Minecraft\VoxelWorld.uproject" -WaitMutex -FromMsBuild

# Launch the editor:
& "C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe" "E:\Repo\UE5Minecraft\VoxelWorld.uproject"
```

Adjust the `UE_5.4` path if the engine is installed elsewhere. There is no test framework wired up in this project.

## Architecture

### Layered structure (`Source/VoxelWorld/Private/`)

```
Game/                     ← GameMode + PlayerController entry points
Voxel/
  ChunkWorldSubsystem.*   ← GameInstanceSubsystem: owns the chunk map, dispatches generation
  Chunk/
    ChunkBase.*           ← Abstract base actor (PURE_VIRTUAL Setup/GenerateMesh/etc.)
    NaiveChunk.*          ← Per-face cube meshing
    GreedyChunk.*         ← Greedy meshing (merges co-planar quads of same block type)
    MarchingChunk.*       ← Marching Cubes (smooth terrain, 256-entry edge/triangle tables inline in header)
  World/
    ChunkWorld.*          ← DEPRECATED actor-based driver (kept for reference, header marked 已弃用)
    GenerateTreeSubsystem.*  ← Cubic-Bezier-shaped trees, DFS leaf fill
  Utils/
    Enums.h               ← EBlock (Null/Air/Stone/Dirt/Grass/Wood/Leaf), EDirection, EGenerationType
    ChunkMeshData.h       ← Vertices/Triangles/Normals/Colors/UVO buffers
    VoxelFunctionLibrary  ← World↔chunk↔local coordinate conversions (handles negative-coord rounding)
    FastNoiseLite.h       ← Vendored Perlin/FBm noise (single-header)
    bezier.h              ← Vendored Bezier curve lib (used for tree foliage shape)
```

### Runtime flow

1. `AMyGameModeBase::BeginPlay` copies its editor-exposed config (chunk type, draw distance, size, frequency, material, generation type) into `UChunkWorldSubsystem` and calls `GenerateWorld()`.
2. `UChunkWorldSubsystem::GenerateWorld()` branches on `EGenerationType` (`GT_2D` or `GT_3D`) and spawns a `(2*DrawDistance+1)^N` grid of `AChunkBase` subclasses via `SpawnActorDeferred`, populating `Chunks: TMap<FIntVector, AChunkBase*>`.
3. Each `AChunkBase::BeginPlay` configures `Noise` (Perlin + FBm), then calls the virtual pipeline `Setup → Generate2D/3DHeightMap → GenerateMesh → ApplyMesh`.
4. `GreedyChunk::Generate2DHeightMap` rolls per-column tree probability (`Tree = 0.1`) and delegates to `UGenerateTreeSubsystem::GenerateTree`, which uses a cubic Bezier (binary-searches `t` for each y-layer) to drive a 4-direction DFS leaf fill via `ChunkWorldSubsystem` cross-chunk modify calls.
5. Block edits go `AChunkBase::ModifyVoxel` → subclass `ModifyVoxelData` → `ClearMesh → GenerateMesh → ApplyMesh` (full chunk remesh, no incremental update).

### Cross-chunk coordinate conventions

- One block = **100 Unreal units**; chunk size in blocks is `Size` (default 32 in subsystem, 64 in `ChunkBase` default).
- `VoxelFunctionLibrary::WorldToChunkPosition` / `WorldToLocalBlockPosition` explicitly handle negative coordinates by rounding away from zero — copy this pattern when adding new conversions, do not use plain integer division.
- `Chunks` is keyed on `FIntVector` chunk coords (declared as `TMap<FIntVector, ...>` in the subsystem header; the `.cpp` Emplace currently passes `FVector` keys — be aware of this when touching that map).

### Subsystem access pattern

Both subsystems expose a static `Get(const UObject* WorldContextObject)` that fetches via `World->GetGameInstance()->GetSubsystem<...>()`. Use this from anywhere with a world context — it is the project's idiomatic way to reach world generation and tree generation.

### Marching Cubes specifics

`MarchingChunk.h` keeps the full 256-entry `CubeEdgeFlags` and `TriangleConnectionTable` inline. `Voxels` stores **floats**, not `EBlock` — the surface is extracted at `SurfaceLevel` (default 0). `Interpolation` toggles smooth vs blocky output.

### Greedy meshing specifics

`GreedyChunk::GenerateMesh` walks 3 axes × 2 directions, builds a 2D mask of `FMask{Block, Normal}` per slice, then expands rectangular runs of equal masks into single quads. `CompareMask` is the equality predicate; `GetTextureIndex` maps `(Block, Normal)` to the texture-array slice index.

## Conventions

- Block enum order in `Enums.h` is **load-bearing** for the `NaiveChunk` cube vertex/triangle tables (`BlockVertexData[8]` / `BlockTriangleData[24]`) and for the texture-array indices used by greedy meshing — do not reorder `EDirection` or `EBlock` casually.
- Comments in this codebase are written in Chinese; match that style when adding inline notes near existing Chinese comments.
- `ChunkWorld` (the actor) is marked deprecated in its header; new world-driving code should go through `UChunkWorldSubsystem` instead.
- `CoreRedirects` in `Config/DefaultEngine.ini` redirect `Chunk → NaiveChunk` and a `ChunkWorld.Chunk → ChunkType` property rename — preserve these if you rename classes.
