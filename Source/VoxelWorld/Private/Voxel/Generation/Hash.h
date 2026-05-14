#pragma once

#include "CoreMinimal.h"

namespace MCWorldGen
{
	// SplitMix64：将一个 64-bit 整数充分搅匀，用于从 worldSeed 派生子种子。
	FORCEINLINE uint64 SplitMix64(uint64 X)
	{
		X = (X ^ (X >> 30)) * 0xbf58476d1ce4e5b9ull;
		X = (X ^ (X >> 27)) * 0x94d049bb133111ebull;
		return X ^ (X >> 31);
	}

	// Boost-style hash combine + SplitMix 收尾。
	FORCEINLINE uint64 HashCombine64(uint64 A, uint64 B)
	{
		return SplitMix64(A ^ (B + 0x9e3779b97f4a7c15ull + (A << 6) + (A >> 2)));
	}

	// 把字符串键混入种子，等价于 Mojang 的 forkPositional("key") 链。
	FORCEINLINE uint64 HashKey(uint64 Seed, const TCHAR* Key)
	{
		uint64 H = Seed;
		while (*Key)
		{
			H = HashCombine64(H, static_cast<uint64>(*Key));
			++Key;
		}
		return H;
	}

	FORCEINLINE uint64 HashKey(uint64 Seed, int Index)
	{
		return HashCombine64(Seed, static_cast<uint64>(static_cast<int64>(Index)));
	}

	// Mojang 用来防止大坐标精度退化的 wrap：把 v 折叠到 [-M/2, M/2] 区间。
	FORCEINLINE double NoiseWrap(double V)
	{
		constexpr double M = 3.3554432e7;
		return V - FMath::FloorToDouble(V / M + 0.5) * M;
	}
}
