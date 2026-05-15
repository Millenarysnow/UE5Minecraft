#pragma once

#include "CoreMinimal.h"
#include "Voxel/Utils/Enums.h"

struct FRandomStream;

namespace MCWorldGen
{
	/**
	 * 特征放置器（v1：仅树）。
	 *
	 * 流程：FillChunk 在主体方块（含 surface system）填完后调用 PlaceFeatures，
	 * 这里对一个 chunk 用确定性 RNG 随机抽几个 (lx, ly) 位置，从顶向下扫到表层方块，
	 * 按 biome 决定 oak / spruce 形状，把树干 + 叶子写进 OutBlocks。
	 *
	 * v1 简化：
	 *   - 树中心距 chunk 边界 ≥ 2，垂直空间不够也跳过 → 不跨 chunk 写入
	 *   - 数量由 chunk-center biome 决定，所有树用同一种 biome 数据
	 *   - 树种：Plains/Forest → oak；SnowyPlains/Mountains → spruce；其它 → 无树
	 */
	class FFeaturePlacer
	{
	public:
		explicit FFeaturePlacer(uint64 InWorldSeed);

		void PlaceFeatures(const FIntVector& ChunkOriginVoxel, int ChunkSize, EBiome ChunkBiome, TArray<EBlock>& OutBlocks) const;

	private:
		uint64 WorldSeed;

		void PlaceTrees(const FIntVector& ChunkOriginVoxel, int ChunkSize, EBiome ChunkBiome, TArray<EBlock>& OutBlocks) const;
		void PlaceOres(const FIntVector& ChunkOriginVoxel, int ChunkSize, TArray<EBlock>& OutBlocks) const;

		void PlaceOak(int lx, int ly, int topLz, int ChunkSize, TArray<EBlock>& OutBlocks, FRandomStream& Rng) const;
		void PlaceSpruce(int lx, int ly, int topLz, int ChunkSize, TArray<EBlock>& OutBlocks, FRandomStream& Rng) const;
	};
}
