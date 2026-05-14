#pragma once

#include "CoreMinimal.h"
#include "Voxel/Utils/Enums.h"

namespace MCWorldGen
{
	class FNormalNoise;

	/**
	 * 表层规则：在 chunk 一列的体素数据上，把最顶层的 stone 换成 biome 表层方块。
	 *
	 * v1（Phase 3）只实现 Plains：
	 *   - top stone 在海平面（含）以上 → 顶层 grass + 下面 N 块 dirt
	 *   - top stone 在海平面以下     → 顶层 sand + 下面 N 块 sand（海床）
	 *
	 * surface_depth 取自 Mojang surface 噪声：clamp(noise * 2.75 + 3, 2, 6)。
	 *
	 * Phase 4 会按 biome 分发到不同规则（snow / sandstone 等）。
	 */
	class FSurfaceSystem
	{
	public:
		explicit FSurfaceSystem(uint64 WorldSeed);

		/// 在一个 chunk 的 (x,z) 列上应用表层规则。
		/// @param Wx, Wz       MC 块坐标
		/// @param ChunkOriginY lz=0 对应的 MC 块 Y
		/// @param BlockAbove   本 chunk 顶部 (lz=ChunkSize) 上方一格的方块；用于判断顶端 stone
		///                     是否真的暴露在 air/water 下。如果未知传 EBlock::Stone（保守跳过）。
		/// @param ColumnView   该列的 EBlock 数组（长度 = ChunkSize），原地修改
		void ApplyColumn(double Wx, double Wz, int ChunkOriginY, EBlock BlockAbove, TArrayView<EBlock> ColumnView) const;

	private:
		TSharedPtr<const FNormalNoise> SurfaceNoise;

		static constexpr int SeaLevel = 63;
	};
}
