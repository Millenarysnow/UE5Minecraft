#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/DensityFunction.h"

namespace MCWorldGen
{
	class FNormalNoise;

	/**
	 * NoiseRouter：Mojang NoiseRouterData.overworld() 的 C++ 等价物（v1 简化版）。
	 *   - 装配 climate samplers（continentalness / erosion / ridges / ridgesFolded / temperature / humidity）
	 *   - 装配 offset / factor / jaggedness 三个核心样条
	 *   - 提供 final_density 评估（含顶/底 slide 平滑）
	 *
	 * 简化点：
	 *   - 不做 domain warp（climate noise 直接采样，无 shiftA/shiftB）
	 *   - 不做 cache2d / flatCache（FillChunk 自己用 FColumnState 做 per-column 缓存）
	 *   - base_3d_noise 用一个简单 OctavedPerlin 替代 Mojang 的 BlendedNoise
	 *   - 不含洞穴（cheese caves 留 Phase 5）
	 *   - 不含 amplified 模式
	 */
	class FNoiseRouter
	{
	public:
		explicit FNoiseRouter(uint64 WorldSeed, bool bEnableCaves = true, double ContinentBias = 0.0);

		// Mojang 默认值：海平面 63，世界 y ∈ [-64, 320]，高度 384。
		static constexpr int SeaLevel  = 63;
		static constexpr int MinY      = -64;
		static constexpr int MaxY      = 320;
		static constexpr int Height    = MaxY - MinY; // 384

		bool AreCavesEnabled() const { return bCavesEnabled; }

		// 单点 climate samplers（MC 块坐标系；不依赖 Y）。
		double Continentalness(double Wx, double Wz) const { return ContinentalnessDF->Compute(Wx, 0.0, Wz); }
		double ErosionVal     (double Wx, double Wz) const { return ErosionDF->Compute(Wx, 0.0, Wz);        }
		double Ridges         (double Wx, double Wz) const { return RidgesDF->Compute(Wx, 0.0, Wz);         }
		double RidgesFolded   (double Wx, double Wz) const { return RidgesFoldedDF->Compute(Wx, 0.0, Wz);   }
		double Temperature    (double Wx, double Wz) const { return TemperatureDF->Compute(Wx, 0.0, Wz);    }
		double Humidity       (double Wx, double Wz) const { return HumidityDF->Compute(Wx, 0.0, Wz);       }

		// 单点 offset / factor / jaggedness 样条结果。
		double Offset    (double Wx, double Wz) const { return OffsetDF    ->Compute(Wx, 0.0, Wz); }
		double Factor    (double Wx, double Wz) const { return FactorDF    ->Compute(Wx, 0.0, Wz); }
		double Jaggedness(double Wx, double Wz) const { return JaggednessDF->Compute(Wx, 0.0, Wz); }

		// 单点 final_density（含 slide）。慢，仅用于调试；批量请用 FColumnState。
		double FinalDensity(double Wx, double Wy, double Wz) const;

		// 列缓存：所有 (x, z) 上 y-无关的量。
		struct FColumnState
		{
			double Wx = 0;
			double Wz = 0;
			double Offset = 0;
			double Factor = 0;
			double Jaggedness = 0;
			double JagPerlinHalfNeg = 0;
		};
		FColumnState BuildColumn(double Wx, double Wz) const;

		// 在已计算的列缓存上对单个 y 求 final_density。Hot loop 走这个。
		double DensityInColumn(const FColumnState& Col, double Wy) const;

		// 解析估计本列的密度过渡 y 区间，用于跳过完全在 stone 或完全在 air 一侧的 chunk。
		//   y <= StoneYMax  → 一定 stone（base3D 影响翻不了号）
		//   y >= AirYMin    → 一定 air
		//   StoneYMax < y < AirYMin → 必须走逐 y 密度评估
		// 只看 offset / factor 两个 spline 输出，不算 base3D（很便宜）。
		struct FColumnBounds
		{
			int StoneYMax;
			int AirYMin;
		};
		FColumnBounds EstimateColumnBounds(const FColumnState& Col) const;

	private:
		// 噪声实例
		TSharedPtr<const FNormalNoise> ContinentNoise;
		TSharedPtr<const FNormalNoise> ErosionNoise;
		TSharedPtr<const FNormalNoise> RidgeNoise;
		TSharedPtr<const FNormalNoise> TempNoise;
		TSharedPtr<const FNormalNoise> VegNoise;
		TSharedPtr<const FNormalNoise> JaggedNoise;
		TSharedPtr<const FNormalNoise> Base3DNoise;
		TSharedPtr<const FNormalNoise> CaveCheeseNoise;   // 仅在 bCavesEnabled 时有效
		TSharedPtr<const FNormalNoise> CaveEntranceNoise; // 仅在 bCavesEnabled 时有效

		// 密度函数树
		FDensityRef ContinentalnessDF;
		FDensityRef ErosionDF;
		FDensityRef RidgesDF;
		FDensityRef RidgesFoldedDF;
		FDensityRef TemperatureDF;
		FDensityRef HumidityDF;

		FDensityRef OffsetDF;
		FDensityRef FactorDF;
		FDensityRef JaggednessDF;

		bool bCavesEnabled = true;
	};
}
