#include "Voxel/Generation/FeaturePlacer.h"

#include "Math/RandomStream.h"
#include "Voxel/Generation/Hash.h"

namespace MCWorldGen
{
	namespace
	{
		// 在 OutBlocks 的 (lx, ly) 列里，从顶向下找第一个不是 Air / 不是 Water 的方块的 lz；找不到返回 -1。
		int FindTopSolidLz(int lx, int ly, int ChunkSize, const TArray<EBlock>& OutBlocks)
		{
			for (int lz = ChunkSize - 1; lz >= 0; --lz)
			{
				const int Idx = lx + ChunkSize * (ly + ChunkSize * lz);
				const EBlock B = OutBlocks[Idx];
				if (B != EBlock::Air && B != EBlock::Water)
				{
					return lz;
				}
			}
			return -1;
		}

		EBlock GetBlockAt(int lx, int ly, int lz, int ChunkSize, const TArray<EBlock>& OutBlocks)
		{
			return OutBlocks[lx + ChunkSize * (ly + ChunkSize * lz)];
		}

		void SetBlockAt(int lx, int ly, int lz, int ChunkSize, TArray<EBlock>& OutBlocks, EBlock Block)
		{
			OutBlocks[lx + ChunkSize * (ly + ChunkSize * lz)] = Block;
		}

		// 仅当目标格子是 Air 时才放叶（不要覆盖 trunk 自己）。
		void TrySetLeaf(int lx, int ly, int lz, int ChunkSize, TArray<EBlock>& OutBlocks, EBlock LeafType)
		{
			if (lx < 0 || lx >= ChunkSize || ly < 0 || ly >= ChunkSize || lz < 0 || lz >= ChunkSize) return;
			const int Idx = lx + ChunkSize * (ly + ChunkSize * lz);
			if (OutBlocks[Idx] == EBlock::Air)
			{
				OutBlocks[Idx] = LeafType;
			}
		}
	}

	FFeaturePlacer::FFeaturePlacer(uint64 InWorldSeed)
		: WorldSeed(InWorldSeed)
	{
	}

	void FFeaturePlacer::PlaceFeatures(const FIntVector& ChunkOriginVoxel, int ChunkSize, EBiome ChunkBiome, TArray<EBlock>& OutBlocks) const
	{
		// 不在沙漠 / 海洋种树
		if (ChunkBiome == EBiome::Desert || ChunkBiome == EBiome::Ocean) return;

		// 每 chunk 决定性 RNG（同一 worldSeed + chunk XY → 同一棵树布局）
		uint64 Seed = HashCombine64(WorldSeed, static_cast<uint64>(static_cast<int64>(ChunkOriginVoxel.X)));
		Seed = HashCombine64(Seed, static_cast<uint64>(static_cast<int64>(ChunkOriginVoxel.Y)));
		FRandomStream Rng(static_cast<int32>(Seed & 0x7fffffff));

		// 树尝试次数（v1 简单分布；后续可按 placed_feature 的 weighted_list 配置）
		int Attempts = 0;
		switch (ChunkBiome)
		{
		case EBiome::Forest:       Attempts = 10; break;
		case EBiome::Plains:       Attempts = (Rng.FRand() < 0.05f) ? 1 : 0; break;
		case EBiome::SnowyPlains:  Attempts = (Rng.FRand() < 0.10f) ? 1 : 0; break;
		case EBiome::Mountains:    Attempts = (Rng.FRand() < 0.08f) ? 1 : 0; break;
		default:                   Attempts = 0; break;
		}

		// 树中心 ≥ 2 块离边界，5x5 叶子 blob 才能完全在本 chunk 内
		const int Margin = 2;
		const int MaxXY = ChunkSize - Margin - 1;
		if (MaxXY <= Margin) return;

		const bool bUseOak    = (ChunkBiome == EBiome::Forest || ChunkBiome == EBiome::Plains);
		const bool bUseSpruce = (ChunkBiome == EBiome::SnowyPlains || ChunkBiome == EBiome::Mountains);

		for (int t = 0; t < Attempts; ++t)
		{
			const int rx = Rng.RandRange(Margin, MaxXY);
			const int ry = Rng.RandRange(Margin, MaxXY);

			const int topLz = FindTopSolidLz(rx, ry, ChunkSize, OutBlocks);
			if (topLz < 0) continue;

			const EBlock TopBlock = GetBlockAt(rx, ry, topLz, ChunkSize, OutBlocks);

			if (bUseOak)
			{
				// Oak 只在 grass / dirt 上长
				if (TopBlock != EBlock::Grass && TopBlock != EBlock::Dirt) continue;
				PlaceOak(rx, ry, topLz, ChunkSize, OutBlocks, Rng);
			}
			else if (bUseSpruce)
			{
				// Spruce 在 grass / dirt / snow 上长
				if (TopBlock != EBlock::Grass && TopBlock != EBlock::Dirt && TopBlock != EBlock::Snow) continue;
				PlaceSpruce(rx, ry, topLz, ChunkSize, OutBlocks, Rng);
			}
		}
	}

	void FFeaturePlacer::PlaceOak(int lx, int ly, int topLz, int ChunkSize, TArray<EBlock>& OutBlocks, FRandomStream& Rng) const
	{
		// 树干高度 4..6
		const int TrunkHeight = Rng.RandRange(4, 6);

		// 检查垂直空间是否够（trunk + leaves blob 共 trunk_h + 2 块）
		if (topLz + TrunkHeight + 2 >= ChunkSize) return;

		// 树干
		for (int i = 0; i < TrunkHeight; ++i)
		{
			SetBlockAt(lx, ly, topLz + 1 + i, ChunkSize, OutBlocks, EBlock::Wood);
		}

		// 5x5x3 叶子 blob，最底层位于 trunk 顶之下 1 块（覆盖最顶 trunk 周围）
		// 第 0、1 层：5x5（radius=2），跳 4 个角
		// 第 2 层：3x3（radius=1）
		const int LeafBaseLz = topLz + TrunkHeight - 1;
		for (int dz = 0; dz < 3; ++dz)
		{
			const int leafLz = LeafBaseLz + dz;
			const int radius = (dz < 2) ? 2 : 1;

			for (int dy = -radius; dy <= radius; ++dy)
			{
				for (int dx = -radius; dx <= radius; ++dx)
				{
					// 跳过 5x5 的 4 个角
					if (dz < 2 && FMath::Abs(dx) == 2 && FMath::Abs(dy) == 2) continue;
					TrySetLeaf(lx + dx, ly + dy, leafLz, ChunkSize, OutBlocks, EBlock::Leaf);
				}
			}
		}
	}

	void FFeaturePlacer::PlaceSpruce(int lx, int ly, int topLz, int ChunkSize, TArray<EBlock>& OutBlocks, FRandomStream& Rng) const
	{
		// 树干 5..8
		const int TrunkHeight = Rng.RandRange(5, 8);

		if (topLz + TrunkHeight + 1 >= ChunkSize) return;

		for (int i = 0; i < TrunkHeight; ++i)
		{
			SetBlockAt(lx, ly, topLz + 1 + i, ChunkSize, OutBlocks, EBlock::SpruceLog);
		}

		// 锥形叶。从树干中段到顶上 1 块，5 层，半径 (2, 2, 1, 1, 0)
		const int FoliageStartLz = topLz + (TrunkHeight - 4);
		const int RadiusByLayer[5] = { 2, 2, 1, 1, 0 };

		for (int layer = 0; layer < 5; ++layer)
		{
			const int leafLz = FoliageStartLz + layer;
			if (leafLz < 0 || leafLz >= ChunkSize) continue;

			const int radius = RadiusByLayer[layer];
			if (radius == 0)
			{
				// 顶层：单块叶
				TrySetLeaf(lx, ly, leafLz, ChunkSize, OutBlocks, EBlock::SpruceLeaf);
				continue;
			}

			for (int dy = -radius; dy <= radius; ++dy)
			{
				for (int dx = -radius; dx <= radius; ++dx)
				{
					if (FMath::Abs(dx) == radius && FMath::Abs(dy) == radius) continue; // 跳角
					TrySetLeaf(lx + dx, ly + dy, leafLz, ChunkSize, OutBlocks, EBlock::SpruceLeaf);
				}
			}
		}
	}
}
