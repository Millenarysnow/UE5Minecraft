#include "Voxel/Generation/SurfaceSystem.h"

#include "Voxel/Generation/Hash.h"
#include "Voxel/Generation/Noises.h"
#include "Voxel/Generation/OctavedNoise.h"

namespace MCWorldGen
{
	FSurfaceSystem::FSurfaceSystem(uint64 WorldSeed)
	{
		SurfaceNoise = MakeShared<FNormalNoise>(
			HashKey(WorldSeed, Noises::Surface.Key),
			Noises::Surface.Params
		);
	}

	void FSurfaceSystem::ApplyColumn(double Wx, double Wz, int ChunkOriginY, EBiome Biome, EBlock BlockAbove, bool bDebugBiomeColors, TArrayView<EBlock> ColumnView) const
	{
		(void)ChunkOriginY; // 当前未使用，但 Phase 5 起 surface rules 可能要看 Y。

		const int Num = ColumnView.Num();
		if (Num <= 0) return;

		// 找"真正的世界表层"：从顶往下找第一个 stone，且其正上方是 air 或 water。
		// 这样可以排除完全在地下、整列都是 stone 的 chunk —— 它们没有表层。
		int TopStoneLz = -1;
		for (int Lz = Num - 1; Lz >= 0; --Lz)
		{
			const EBlock Above = (Lz + 1 < Num) ? ColumnView[Lz + 1] : BlockAbove;
			if (ColumnView[Lz] == EBlock::Stone && Above != EBlock::Stone)
			{
				TopStoneLz = Lz;
				break;
			}
		}
		if (TopStoneLz < 0) return;

		// 顶 stone 正上方是 water → 海床/沙滩，用 sand；否则按 biome 派发。
		const EBlock AboveTopStone = (TopStoneLz + 1 < Num) ? ColumnView[TopStoneLz + 1] : BlockAbove;
		const bool bUnderwater = (AboveTopStone == EBlock::Water);

		// 表层厚度（dirt/sand 总层数）：noise * 2.75 + 3，clamp 到 [2, 6]
		const double SurfNoiseRaw = SurfaceNoise->Sample2D(Wx, Wz);
		const int SurfaceDepth = FMath::Clamp(static_cast<int>(SurfNoiseRaw * 2.75 + 3.0), 2, 6);

		EBlock TopBlock;
		EBlock UnderBlock;

		if (bDebugBiomeColors)
		{
			// 调试可视化：top + under 全部用同一个 biome 标记块覆盖，肉眼一眼可辨。
			//   Plains      → DebugBiomePlains    (纯色鲜绿)
			//   Forest      → SpruceLeaf          (纯色深绿)
			//   Desert      → Sand                (纯色沙黄)
			//   SnowyPlains → Snow                (纯色雪白)
			//   Mountains   → DebugBiomeMountains (纯色中灰)
			//   Ocean       → Sand                (沙黄；ocean 多在水下，跟海床一致)
			// 注意：bUnderwater 在此模式下不再覆盖 biome 标记 —— 我们要看 biome 而不是水下沙。
			EBlock Marker;
			switch (Biome)
			{
			case EBiome::Plains:       Marker = EBlock::DebugBiomePlains;    break;
			case EBiome::Forest:       Marker = EBlock::SpruceLeaf;          break;
			case EBiome::Desert:       Marker = EBlock::Sand;                break;
			case EBiome::SnowyPlains:  Marker = EBlock::Snow;                break;
			case EBiome::Mountains:    Marker = EBlock::DebugBiomeMountains; break;
			case EBiome::Ocean:        Marker = EBlock::Sand;                break;
			default:                   Marker = EBlock::DebugBiomePlains;    break;
			}
			TopBlock   = Marker;
			UnderBlock = Marker;
		}
		else if (bUnderwater)
		{
			// 海床 / 浅滩 一律 sand（biome 无关）
			TopBlock   = EBlock::Sand;
			UnderBlock = EBlock::Sand;
		}
		else
		{
			switch (Biome)
			{
			case EBiome::Desert:
				// v1：沙漠 top + under 都是 sand。Sandstone 深层留给 Phase 8 调。
				TopBlock   = EBlock::Sand;
				UnderBlock = EBlock::Sand;
				break;

			case EBiome::SnowyPlains:
				TopBlock   = EBlock::Snow;
				UnderBlock = EBlock::Dirt;
				break;

			case EBiome::Plains:
			case EBiome::Forest:
			case EBiome::Mountains:
			case EBiome::Ocean: // Ocean 在水面上的极少数高地
			default:
				TopBlock   = EBlock::Grass;
				UnderBlock = EBlock::Dirt;
				break;
			}
		}

		// 顶层一格替换。
		ColumnView[TopStoneLz] = TopBlock;

		// 下面 SurfaceDepth 层依次替换为 dirt/sand/marker，遇到非 Stone 即停（避免穿透洞穴顶）。
		for (int i = 1; i <= SurfaceDepth; ++i)
		{
			const int Lz = TopStoneLz - i;
			if (Lz < 0) break;
			if (ColumnView[Lz] != EBlock::Stone) break;
			ColumnView[Lz] = UnderBlock;
		}
	}
}
