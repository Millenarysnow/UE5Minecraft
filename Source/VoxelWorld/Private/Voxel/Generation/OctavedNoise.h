#pragma once

#include "CoreMinimal.h"

class FastNoiseLite;

namespace MCWorldGen
{
	/**
	 * Octaved Perlin 噪声参数。等价于 Mojang 的 NormalNoise.NoiseParameters：
	 *   - FirstOctave: 通常为负，例如 -9 表示最低频对应周期 = 2^9 = 512 方块
	 *   - Amplitudes[i]: 第 i 个 octave 的振幅；amplitude == 0 的 octave 跳过不计算
	 */
	struct FOctavedNoiseParameters
	{
		int FirstOctave;
		TArray<double> Amplitudes;

		FOctavedNoiseParameters() : FirstOctave(0) {}

		FOctavedNoiseParameters(int InFirstOctave, std::initializer_list<double> InAmplitudes)
			: FirstOctave(InFirstOctave), Amplitudes(InAmplitudes)
		{}
	};

	/**
	 * Mojang PerlinNoise 等价：在固定频率倍率下叠加 N 个 octave。
	 * 输出范围近似 [-1, 1]（与 amplitudes 配置相关）。
	 *
	 * 数学：sample(x,y,z) = Σ amplitudes[i] * perlin_i(x*f, y*f, z*f) * v
	 *   其中 f = 2^(firstOctave + i)，v = lowestFreqValueFactor / 2^i
	 *   lowestFreqValueFactor = 2^(N-1) / (2^N - 1)
	 */
	class FOctavedPerlin
	{
	public:
		FOctavedPerlin(uint64 RootSeed, const FOctavedNoiseParameters& Params);
		~FOctavedPerlin();

		FOctavedPerlin(const FOctavedPerlin&) = delete;
		FOctavedPerlin& operator=(const FOctavedPerlin&) = delete;
		FOctavedPerlin(FOctavedPerlin&&) = default;
		FOctavedPerlin& operator=(FOctavedPerlin&&) = default;

		double Sample(double X, double Y, double Z) const;

	private:
		int FirstOctave = 0;
		TArray<double> Amplitudes;
		TArray<TUniquePtr<FastNoiseLite>> Octaves; // index 与 Amplitudes 对齐；amplitude==0 处为 nullptr
		double LowestFreqInputFactor = 1.0;
		double LowestFreqValueFactor = 1.0;
	};

	/**
	 * Mojang NormalNoise 等价：
	 *   sample(x,y,z) = (first(x,y,z) + second(x*k, y*k, z*k)) * valueFactor
	 *   k = 1.0181268882175227 （让两个 PerlinNoise 解相关）
	 *   valueFactor = (1/6) / expectedDeviation(span)
	 *   expectedDeviation(n) = 0.1 * (1 + 1/(n+1))，span = 非零 amplitude 的 index 范围
	 *
	 * 输出范围近似 [-1, 1]，远离 0 的尾部偶尔会超出，调用方按需 clamp。
	 */
	class FNormalNoise
	{
	public:
		FNormalNoise(uint64 RootSeed, const FOctavedNoiseParameters& Params);

		double Sample(double X, double Y, double Z) const;
		double Sample2D(double X, double Z) const { return Sample(X, 0.0, Z); }

	private:
		FOctavedPerlin First;
		FOctavedPerlin Second;
		double ValueFactor = 1.0;
	};
}
