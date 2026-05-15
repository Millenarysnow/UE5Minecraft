#include "Voxel/Generation/BiomeSource.h"

namespace MCWorldGen
{
	EBiome FBiomeSource::SampleBiome(double Continentalness, double Erosion, double /*RidgesFolded*/,
	                                  double Temperature, double Humidity, double /*Depth*/) const
	{
		// 注：Mojang 的 OverworldBiomeBuilder 用 nearest-box 加权投票，门槛可以做得更"极端"
		// （T < -0.45、erosion < -0.78、T > 0.55 等）。我们用决策树 + 视野较小，
		// 以观测到的 Perlin std ≈ 0.3 反推门槛，让稀有 biome 概率提升到 5–15%，
		// 才能在 17×17 chunk 的视野里稳定看到。

		// 1) Ocean：保持 Mojang 上界 -0.19
		if (Continentalness < -0.19) return EBiome::Ocean;

		// 2) Mountains：erosion < -0.4（原 -0.78 太严，几乎看不到）
		if (Erosion < -0.4) return EBiome::Mountains;

		// 3) SnowyPlains：T < -0.3（原 -0.45）
		if (Temperature < -0.3) return EBiome::SnowyPlains;

		// 4) Desert：T > 0.3 且偏干燥（H < -0.05）
		if (Temperature > 0.3 && Humidity < -0.05) return EBiome::Desert;

		// 5) Forest：湿度 > 0.1
		if (Humidity > 0.1) return EBiome::Forest;

		// 6) 默认 Plains
		return EBiome::Plains;
	}
}
