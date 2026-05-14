#include "Voxel/Generation/TerrainSplines.h"

namespace MCWorldGen::TerrainSplines
{
	namespace
	{
		// ────────────────────────────────────────────────────────
		// 纯标量辅助（来自 TerrainProvider.java 私有静态方法）
		// ────────────────────────────────────────────────────────

		float CalculateSlope(float Y1, float Y2, float X1, float X2)
		{
			return (Y2 - Y1) / (X2 - X1);
		}

		// TerrainProvider.mountainContinentalness
		float MountainContinentalness(float P0, float P1, float P2)
		{
			const float F = 1.17f;
			const float F1 = 0.46082947f;
			const float F2 = 1.0f - (1.0f - P1) * 0.5f;
			const float F3 = 0.5f * (1.0f - P1);
			const float F4 = (P0 + F) * F1;
			const float F5 = F4 * F2 - F3;
			return P0 < P2 ? FMath::Max(F5, -0.2222f) : FMath::Max(F5, 0.0f);
		}

		// TerrainProvider.calculateMountainRidgeZeroContinentalnessPoint
		float CalculateMountainRidgeZeroContinentalnessPoint(float P)
		{
			const float F1 = 0.46082947f;
			const float F2 = 1.0f - (1.0f - P) * 0.5f;
			const float F3 = 0.5f * (1.0f - P);
			return F3 / (F1 * F2) - 1.17f;
		}

		// NoiseRouterData.peaksAndValleys (float overload)
		float PeaksAndValleys(float W)
		{
			return -(FMath::Abs(FMath::Abs(W) - 0.6666667f) - 0.33333334f) * 3.0f;
		}

		// ────────────────────────────────────────────────────────
		// 子样条 builder（按 ridges_folded / ridges 分锚点）
		// ────────────────────────────────────────────────────────

		FCubicSpline BuildMountainRidgeSplineWithPoints(FDensityRef RidgesFolded, float P, bool Special)
		{
			FSplineBuilder Builder(RidgesFolded);

			const float F2 = MountainContinentalness(-1.0f, P, -0.7f);
			const float F4 = MountainContinentalness( 1.0f, P, -0.7f);
			const float F5 = CalculateMountainRidgeZeroContinentalnessPoint(P);

			if (-0.65f < F5 && F5 < 1.0f)
			{
				const float F14 = MountainContinentalness(-0.65f, P, -0.7f);
				const float F9  = MountainContinentalness(-0.75f, P, -0.7f);
				const float F10 = CalculateSlope(F2, F9, -1.0f, -0.75f);
				Builder.Add(-1.0f, F2, F10);
				Builder.Add(-0.75f, F9);
				Builder.Add(-0.65f, F14);
				const float F11 = MountainContinentalness(F5, P, -0.7f);
				const float F12 = CalculateSlope(F11, F4, F5, 1.0f);
				Builder.Add(F5 - 0.01f, F11);
				Builder.Add(F5, F11, F12);
				Builder.Add(1.0f, F4, F12);
			}
			else
			{
				const float F7 = CalculateSlope(F2, F4, -1.0f, 1.0f);
				if (Special)
				{
					Builder.Add(-1.0f, FMath::Max(0.2f, F2));
					Builder.Add(0.0f, FMath::Lerp(F2, F4, 0.5f), F7);
				}
				else
				{
					Builder.Add(-1.0f, F2, F7);
				}
				Builder.Add(1.0f, F4, F7);
			}

			return Builder.Build();
		}

		// TerrainProvider.ridgeSpline
		FCubicSpline RidgeSpline(FDensityRef RidgesFolded, float P2, float P3, float P4, float P5, float P6, float P7)
		{
			const float F  = FMath::Max(0.5f * (P3 - P2), P7);
			const float F1 = 5.0f * (P4 - P3);
			return FSplineBuilder(RidgesFolded)
				.Add(-1.0f, P2, F)
				.Add(-0.4f, P3, FMath::Min(F, F1))
				.Add( 0.0f, P4, F1)
				.Add( 0.4f, P5, 2.0f * (P5 - P4))
				.Add( 1.0f, P6, 0.7f * (P6 - P5))
				.Build();
		}

		// TerrainProvider.buildErosionOffsetSpline
		FCubicSpline BuildErosionOffsetSpline(
			FDensityRef Erosion, FDensityRef RidgesFolded,
			float P3, float P4, float P5, float P6, float P7, float P8,
			bool P9, bool P10)
		{
			FCubicSpline S0 = BuildMountainRidgeSplineWithPoints(RidgesFolded, FMath::Lerp(0.6f, 1.5f, P6), P10);
			FCubicSpline S1 = BuildMountainRidgeSplineWithPoints(RidgesFolded, FMath::Lerp(0.6f, 1.0f, P6), P10);
			FCubicSpline S2 = BuildMountainRidgeSplineWithPoints(RidgesFolded, P6, P10);

			FCubicSpline S3 = RidgeSpline(RidgesFolded,
				P3 - 0.15f, 0.5f * P6, FMath::Lerp(0.5f, 0.5f, 0.5f) * P6, 0.5f * P6, 0.6f * P6, 0.5f);
			FCubicSpline S4 = RidgeSpline(RidgesFolded,
				P3, P7 * P6, P4 * P6, 0.5f * P6, 0.6f * P6, 0.5f);

			// Mojang 在 erosion=0.2、(0.4) 等位置都引用同一形状的 ridge spline；
			// 我们用值类型 + 内部 TSharedPtr 的 FCubicSpline，可以直接拷贝复用。
			FCubicSpline S5 = RidgeSpline(RidgesFolded, P3, P7, P7, P4, P5, 0.5f);

			FCubicSpline S7 = FSplineBuilder(RidgesFolded)
				.Add(-1.0f, P3)
				.Add(-0.4f, S5)                        // 拷贝
				.Add( 0.0f, P5 + 0.07f)
				.Build();

			FCubicSpline S8 = RidgeSpline(RidgesFolded, -0.02f, P8, P8, P4, P5, 0.0f);

			FSplineBuilder Builder(Erosion);
			Builder
				.Add(-0.85f, MoveTemp(S0))
				.Add(-0.7f,  MoveTemp(S1))
				.Add(-0.4f,  MoveTemp(S2))
				.Add(-0.35f, MoveTemp(S3))
				.Add(-0.1f,  MoveTemp(S4))
				.Add( 0.2f,  S5);                      // 拷贝（下面 if 还要用）

			if (P9)
			{
				Builder.Add(0.4f,  S5);                // 拷贝
				Builder.Add(0.45f, S7);                // 拷贝
				Builder.Add(0.55f, MoveTemp(S7));
				Builder.Add(0.58f, MoveTemp(S5));
			}

			Builder.Add(0.7f, MoveTemp(S8));
			return Builder.Build();
		}

		// ────────────────────────────────────────────────────────
		// Factor 子样条 builder
		// ────────────────────────────────────────────────────────

		FCubicSpline GetErosionFactor(
			FDensityRef Erosion, FDensityRef Ridges, FDensityRef RidgesFolded,
			float BaseFactor, bool P5)
		{
			auto MakeRidgesFlatToBase = [&]() {
				return FSplineBuilder(Ridges)
					.Add(-0.2f, 6.3f)
					.Add( 0.2f, BaseFactor)
					.Build();
			};

			FSplineBuilder Builder(Erosion);
			Builder
				.Add(-0.6f,  MakeRidgesFlatToBase())
				.Add(-0.5f,  FSplineBuilder(Ridges).Add(-0.05f, 6.3f).Add(0.05f, 2.67f).Build())
				.Add(-0.35f, MakeRidgesFlatToBase())
				.Add(-0.25f, MakeRidgesFlatToBase())
				.Add(-0.1f,  FSplineBuilder(Ridges).Add(-0.05f, 2.67f).Add(0.05f, 6.3f).Build())
				.Add( 0.03f, MakeRidgesFlatToBase());

			if (P5)
			{
				auto MakeChain1 = [&]() {
					FCubicSpline Inner = FSplineBuilder(Ridges).Add(0.0f, BaseFactor).Add(0.1f, 0.625f).Build();
					return FSplineBuilder(RidgesFolded)
						.Add(-0.9f, BaseFactor)
						.Add(-0.69f, MoveTemp(Inner))
						.Build();
				};

				Builder
					.Add(0.35f, BaseFactor)
					.Add(0.45f, MakeChain1())
					.Add(0.55f, MakeChain1())
					.Add(0.62f, BaseFactor);
			}
			else
			{
				auto MakeChain2 = [&]() {
					return FSplineBuilder(RidgesFolded)
						.Add(-0.7f, MakeRidgesFlatToBase())
						.Add(-0.15f, 1.37f)
						.Build();
				};
				auto MakeChain3 = [&]() {
					return FSplineBuilder(RidgesFolded)
						.Add(0.45f, MakeRidgesFlatToBase())
						.Add( 0.7f, 1.56f)
						.Build();
				};

				Builder
					.Add(0.05f, MakeChain3())
					.Add(0.4f,  MakeChain3())
					.Add(0.45f, MakeChain2())
					.Add(0.55f, MakeChain2())
					.Add(0.58f, BaseFactor);
			}

			return Builder.Build();
		}

		// ────────────────────────────────────────────────────────
		// Jaggedness 子样条 builder
		// ────────────────────────────────────────────────────────

		FCubicSpline BuildWeirdnessJaggednessSpline(FDensityRef Ridges, float P)
		{
			const float F  = 0.63f * P;
			const float F1 = 0.3f  * P;
			return FSplineBuilder(Ridges)
				.Add(-0.01f, F)
				.Add( 0.01f, F1)
				.Build();
		}

		FCubicSpline BuildRidgeJaggednessSpline(
			FDensityRef Ridges, FDensityRef RidgesFolded,
			float P3, float P4)
		{
			const float F  = PeaksAndValleys(0.4f);
			const float F1 = PeaksAndValleys(0.56666666f);
			const float F2 = (F + F1) * 0.5f;

			FSplineBuilder Builder(RidgesFolded);
			Builder.Add(F, 0.0f);
			if (P4 > 0.0f)
			{
				Builder.Add(F2, BuildWeirdnessJaggednessSpline(Ridges, P4));
			}
			else
			{
				Builder.Add(F2, 0.0f);
			}
			if (P3 > 0.0f)
			{
				Builder.Add(1.0f, BuildWeirdnessJaggednessSpline(Ridges, P3));
			}
			else
			{
				Builder.Add(1.0f, 0.0f);
			}
			return Builder.Build();
		}

		FCubicSpline BuildErosionJaggednessSpline(
			FDensityRef Erosion, FDensityRef Ridges, FDensityRef RidgesFolded,
			float P4, float P5, float P6, float P7)
		{
			FCubicSpline C0 = BuildRidgeJaggednessSpline(Ridges, RidgesFolded, P4, P6);
			FCubicSpline C1 = BuildRidgeJaggednessSpline(Ridges, RidgesFolded, P5, P7);

			return FSplineBuilder(Erosion)
				.Add(-1.0f,    MoveTemp(C0))
				.Add(-0.78f,   C1)            // 拷贝
				.Add(-0.5775f, MoveTemp(C1))
				.Add(-0.375f,  0.0f)
				.Build();
		}
	}

	// ────────────────────────────────────────────────────────────
	// 公开入口
	// ────────────────────────────────────────────────────────────

	FCubicSpline BuildOffset(FDensityRef Continents, FDensityRef Erosion, FDensityRef RidgesFolded)
	{
		FCubicSpline Ero1 = BuildErosionOffsetSpline(Erosion, RidgesFolded, -0.15f, 0.0f, 0.0f, 0.1f, 0.0f, -0.03f, false, false);
		FCubicSpline Ero2 = BuildErosionOffsetSpline(Erosion, RidgesFolded, -0.1f,  0.03f, 0.1f, 0.1f, 0.01f, -0.03f, false, false);
		FCubicSpline Ero3 = BuildErosionOffsetSpline(Erosion, RidgesFolded, -0.1f,  0.03f, 0.1f, 0.7f, 0.01f, -0.03f, true,  true );
		FCubicSpline Ero4 = BuildErosionOffsetSpline(Erosion, RidgesFolded, -0.05f, 0.03f, 0.1f, 1.0f, 0.01f,  0.01f, true,  true );

		return FSplineBuilder(Continents)
			.Add(-1.10f,  0.044f)
			.Add(-1.02f, -0.2222f)
			.Add(-0.51f, -0.2222f)
			.Add(-0.44f, -0.12f)
			.Add(-0.18f, -0.12f)
			.Add(-0.16f, Ero1)             // 拷贝
			.Add(-0.15f, MoveTemp(Ero1))
			.Add(-0.10f, MoveTemp(Ero2))
			.Add( 0.25f, MoveTemp(Ero3))
			.Add( 1.00f, MoveTemp(Ero4))
			.Build();
	}

	FCubicSpline BuildFactor(FDensityRef Continents, FDensityRef Erosion, FDensityRef Ridges, FDensityRef RidgesFolded)
	{
		return FSplineBuilder(Continents)
			.Add(-0.19f, 3.95f)
			.Add(-0.15f, GetErosionFactor(Erosion, Ridges, RidgesFolded, 6.25f, true))
			.Add(-0.10f, GetErosionFactor(Erosion, Ridges, RidgesFolded, 5.47f, true))
			.Add( 0.03f, GetErosionFactor(Erosion, Ridges, RidgesFolded, 5.08f, true))
			.Add( 0.06f, GetErosionFactor(Erosion, Ridges, RidgesFolded, 4.69f, false))
			.Build();
	}

	FCubicSpline BuildJaggedness(FDensityRef Continents, FDensityRef Erosion, FDensityRef Ridges, FDensityRef RidgesFolded)
	{
		return FSplineBuilder(Continents)
			.Add(-0.11f, 0.0f)
			.Add( 0.03f, BuildErosionJaggednessSpline(Erosion, Ridges, RidgesFolded, 1.0f, 0.5f, 0.0f, 0.0f))
			.Add( 0.65f, BuildErosionJaggednessSpline(Erosion, Ridges, RidgesFolded, 1.0f, 1.0f, 1.0f, 0.0f))
			.Build();
	}
}
