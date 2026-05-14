#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/CubicSpline.h"
#include "Voxel/Generation/DensityFunction.h"

namespace MCWorldGen
{
	/**
	 * Mojang TerrainProvider.java 的 C++ 等价物。
	 * 接收已经 build 好的 climate density functions，输出三个核心地形样条。
	 *
	 * 这三个样条都是嵌套结构：
	 *   外层按 continentalness 分锚点，
	 *   每个 anchor 的值是按 erosion 分锚点的子样条，
	 *   再嵌套 ridges / ridges_folded 子样条。
	 *
	 * 锚点位置 / 数值 / 斜率全部直接从 TerrainProvider.java 抄过来，
	 * v1 不做 amplified 模式（NO_TRANSFORM 等价于恒等）。
	 */
	namespace TerrainSplines
	{
		FCubicSpline BuildOffset(FDensityRef Continents, FDensityRef Erosion, FDensityRef RidgesFolded);
		FCubicSpline BuildFactor(FDensityRef Continents, FDensityRef Erosion, FDensityRef Ridges, FDensityRef RidgesFolded);
		FCubicSpline BuildJaggedness(FDensityRef Continents, FDensityRef Erosion, FDensityRef Ridges, FDensityRef RidgesFolded);
	}
}
