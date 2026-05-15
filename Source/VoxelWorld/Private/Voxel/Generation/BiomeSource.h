#pragma once

#include "CoreMinimal.h"
#include "Voxel/Utils/Enums.h"

namespace MCWorldGen
{
	/**
	 * 生物群系选择器。v1 用决策树（不是 Mojang 真实的 6D nearest-box 查找）
	 * 基于以下 climate 阈值（来自 OverworldBiomeBuilder.java 的同名 Climate.Parameter）：
	 *   - Ocean continentalness < -0.19
	 *   - Mountains erosion 槽 0：[-1.0, -0.78)（最强冲蚀 = 高山）
	 *   - Frozen temperature 槽 0：[-1.0, -0.45)
	 *   - Hot temperature 槽 4：[+0.55, +1.0]
	 *   - Forest humidity 槽 3：[+0.1, +0.3]（中高湿度）
	 *
	 * 决策树的优先级（高 → 低）：Ocean > Mountains > Snowy > Desert > Forest > Plains。
	 * Phase 5+ 会对每 biome 覆盖更细的 surface 规则；现在只决定方块颜色。
	 */
	class FBiomeSource
	{
	public:
		FBiomeSource() = default;

		/// 根据 climate 6 元组返回 biome。
		/// 注意 Depth 在 v1 暂未使用（Cave biomes 需要 Depth > 0.2，我们不接洞穴 biome）。
		EBiome SampleBiome(double Continentalness, double Erosion, double RidgesFolded,
		                   double Temperature, double Humidity, double Depth) const;
	};
}
