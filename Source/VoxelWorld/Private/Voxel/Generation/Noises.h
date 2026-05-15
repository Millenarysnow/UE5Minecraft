#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/OctavedNoise.h"

namespace MCWorldGen
{
	/**
	 * 单个噪声定义：包含一个稳定的 key 字符串（用于种子派生）和参数表。
	 * 数据来自 net.minecraft.data.worldgen.NoiseData.bootstrap()。
	 */
	struct FNoiseDef
	{
		const TCHAR* Key;
		FOctavedNoiseParameters Params;
	};

	namespace Noises
	{
		// Climate（生物群系采样器）—— 来自 NoiseData.registerBiomeNoises(0, ...)
		extern const FNoiseDef Temperature;       // firstOctave -10, [1.5, 0, 1, 0, 0, 0]
		extern const FNoiseDef Vegetation;        // firstOctave  -8, [1, 1, 0, 0, 0, 0]
		extern const FNoiseDef Continentalness;   // firstOctave  -9, [1, 1, 2, 2, 2, 1, 1, 1, 1]
		extern const FNoiseDef Erosion;           // firstOctave  -9, [1, 1, 0, 1, 1]
		extern const FNoiseDef Ridge;             // firstOctave  -7, [1, 2, 1, 0, 0, 0]

		// Shift：domain warp 的 2D 噪声，让生物群系边界不至于轴对齐。
		extern const FNoiseDef Shift;             // firstOctave  -3, [1, 1, 1, 0]

		// Jagged：山脊抖动噪声。
		extern const FNoiseDef Jagged;            // firstOctave -16, [16 个 1.0]

		// Cave Cheese：奶酪洞噪声。
		extern const FNoiseDef CaveCheese;        // firstOctave  -8, [0.5, 1, 2, 1, 2, 1, 0, 2, 0]

		// Cave Entrance：表层洞穴入口噪声。
		extern const FNoiseDef CaveEntrance;      // firstOctave  -7, [0.4, 0.5, 1.0]

		// Surface 表层规则用噪声。
		extern const FNoiseDef Surface;           // firstOctave  -6, [1, 1, 1]
		extern const FNoiseDef SurfaceSecondary;  // firstOctave  -6, [1, 1, 0, 1]
	}
}
