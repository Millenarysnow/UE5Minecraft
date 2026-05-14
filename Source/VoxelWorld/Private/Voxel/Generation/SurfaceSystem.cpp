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

	void FSurfaceSystem::ApplyColumn(double Wx, double Wz, int ChunkOriginY, EBlock BlockAbove, TArrayView<EBlock> ColumnView) const
	{
		(void)ChunkOriginY; // 当前未使用，但 Phase 4 起可能需要传 Y 给 biome / surface 规则。

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

		// 顶 stone 正上方是 water → 海床/沙滩，用 sand；否则陆地，用 grass+dirt。
		// 这种判断比"y < SeaLevel"更准确，能正确处理 stone 顶恰好在海平面附近的情况。
		const EBlock AboveTopStone = (TopStoneLz + 1 < Num) ? ColumnView[TopStoneLz + 1] : BlockAbove;
		const bool bUnderwater = (AboveTopStone == EBlock::Water);

		// 表层厚度（dirt/sand 总层数）：noise * 2.75 + 3，clamp 到 [2, 6]
		const double SurfNoiseRaw = SurfaceNoise->Sample2D(Wx, Wz);
		const int SurfaceDepth = FMath::Clamp(static_cast<int>(SurfNoiseRaw * 2.75 + 3.0), 2, 6);

		const EBlock TopBlock   = bUnderwater ? EBlock::Sand : EBlock::Grass;
		const EBlock UnderBlock = bUnderwater ? EBlock::Sand : EBlock::Dirt;

		// 顶层一格替换。
		ColumnView[TopStoneLz] = TopBlock;

		// 下面 SurfaceDepth 层依次替换为 dirt/sand，遇到非 Stone 即停（避免穿透洞穴顶）。
		for (int i = 1; i <= SurfaceDepth; ++i)
		{
			const int Lz = TopStoneLz - i;
			if (Lz < 0) break;
			if (ColumnView[Lz] != EBlock::Stone) break;
			ColumnView[Lz] = UnderBlock;
		}
	}
}
