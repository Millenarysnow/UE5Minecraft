#include "Voxel/Generation/OctavedNoise.h"

#include "Voxel/Generation/Hash.h"
#include "Voxel/Utils/FastNoiseLite.h"

namespace MCWorldGen
{
	namespace
	{
		// Mojang NormalNoise 中用于让两个 PerlinNoise 解相关的输入比例。
		constexpr double NORMAL_NOISE_INPUT_FACTOR = 1.0181268882175227;

		// Mojang expectedDeviation：用来归一化两套 PerlinNoise 的合成输出。
		double ExpectedDeviation(int Span)
		{
			return 0.1 * (1.0 + 1.0 / static_cast<double>(Span + 1));
		}

		// 在非零 amplitudes 中找首/末下标，返回 span（lastIdx - firstIdx）；全零则返回 0。
		int NonZeroAmplitudeSpan(const TArray<double>& Amplitudes)
		{
			int First = INT32_MAX;
			int Last = INT32_MIN;
			for (int i = 0; i < Amplitudes.Num(); ++i)
			{
				if (Amplitudes[i] != 0.0)
				{
					First = FMath::Min(First, i);
					Last = FMath::Max(Last, i);
				}
			}
			return (First > Last) ? 0 : (Last - First);
		}
	}

	// ────────────────────────────────────────────────────────────
	// FOctavedPerlin
	// ────────────────────────────────────────────────────────────

	FOctavedPerlin::FOctavedPerlin(uint64 RootSeed, const FOctavedNoiseParameters& Params)
		: FirstOctave(Params.FirstOctave)
		, Amplitudes(Params.Amplitudes)
	{
		const int N = Amplitudes.Num();
		Octaves.SetNum(N);

		for (int i = 0; i < N; ++i)
		{
			if (Amplitudes[i] == 0.0)
			{
				Octaves[i] = nullptr;
				continue;
			}

			// Mojang 用 "octave_<firstOctave + i>" 作为 fork key。我们沿用同样的语义：
			// 不同 octave 必须有独立种子。
			const int OctaveIndex = FirstOctave + i;
			const uint64 OctaveSeed = HashKey(RootSeed, OctaveIndex);

			TUniquePtr<FastNoiseLite> Noise = MakeUnique<FastNoiseLite>(static_cast<int>(OctaveSeed & 0x7fffffffu));
			Noise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
			Noise->SetFrequency(1.0f); // 频率由我们外层乘进 inputFactor，本体保持 1
			Octaves[i] = MoveTemp(Noise);
		}

		// Mojang PerlinNoise:
		//   j = -firstOctave
		//   lowestFreqInputFactor = 2^(-j) = 2^firstOctave
		//   lowestFreqValueFactor = 2^(N-1) / (2^N - 1)
		const int J = -FirstOctave;
		LowestFreqInputFactor = FMath::Pow(2.0, static_cast<double>(-J));
		if (N > 0)
		{
			LowestFreqValueFactor = FMath::Pow(2.0, static_cast<double>(N - 1)) / (FMath::Pow(2.0, static_cast<double>(N)) - 1.0);
		}
		else
		{
			LowestFreqValueFactor = 1.0;
		}
	}

	FOctavedPerlin::~FOctavedPerlin() = default;

	double FOctavedPerlin::Sample(double X, double Y, double Z) const
	{
		double Sum = 0.0;
		double Input = LowestFreqInputFactor;
		double Value = LowestFreqValueFactor;

		const int N = Octaves.Num();
		for (int i = 0; i < N; ++i)
		{
			if (Octaves[i].IsValid())
			{
				const double Sx = NoiseWrap(X * Input);
				const double Sy = NoiseWrap(Y * Input);
				const double Sz = NoiseWrap(Z * Input);
				const double S = static_cast<double>(Octaves[i]->GetNoise(Sx, Sy, Sz));
				Sum += Amplitudes[i] * S * Value;
			}
			Input *= 2.0;
			Value *= 0.5;
		}

		return Sum;
	}

	// ────────────────────────────────────────────────────────────
	// FNormalNoise
	// ────────────────────────────────────────────────────────────

	FNormalNoise::FNormalNoise(uint64 RootSeed, const FOctavedNoiseParameters& Params)
		: First(HashKey(RootSeed, TEXT("first")), Params)
		, Second(HashKey(RootSeed, TEXT("second")), Params)
	{
		const int Span = NonZeroAmplitudeSpan(Params.Amplitudes);
		ValueFactor = (1.0 / 6.0) / ExpectedDeviation(Span);
	}

	double FNormalNoise::Sample(double X, double Y, double Z) const
	{
		const double A = First.Sample(X, Y, Z);
		const double B = Second.Sample(X * NORMAL_NOISE_INPUT_FACTOR,
		                                Y * NORMAL_NOISE_INPUT_FACTOR,
		                                Z * NORMAL_NOISE_INPUT_FACTOR);
		return (A + B) * ValueFactor;
	}
}
