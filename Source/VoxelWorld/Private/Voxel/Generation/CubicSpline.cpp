#include "Voxel/Generation/CubicSpline.h"

namespace MCWorldGen
{
	namespace
	{
		FORCEINLINE float LinearExtend(float F, const TArray<float>& Locations, float Value, const TArray<float>& Derivatives, int Idx)
		{
			const float D = Derivatives[Idx];
			return D == 0.0f ? Value : Value + D * (F - Locations[Idx]);
		}

		// 返回最大的 i 使 locations[i] <= f；若 f 小于全部，返回 -1；若 f 大于等于最后一个，返回 N-1。
		int FindIntervalStart(const TArray<float>& Locations, float F)
		{
			int Lo = 0;
			int Hi = Locations.Num();
			while (Lo < Hi)
			{
				const int Mid = (Lo + Hi) / 2;
				if (F < Locations[Mid]) Hi = Mid;
				else                    Lo = Mid + 1;
			}
			return Lo - 1;
		}
	}

	// ──────────────────────────────────────────────────────────
	// FCubicSpline
	// ──────────────────────────────────────────────────────────

	FCubicSpline FCubicSpline::Constant(float Value)
	{
		FCubicSpline S;
		S.Kind = EKind::Constant;
		S.ConstantValue = Value;
		return S;
	}

	float FCubicSpline::Evaluate(double X, double Y, double Z) const
	{
		if (Kind == EKind::Constant)
		{
			return ConstantValue;
		}

		check(Multi.IsValid());
		const FMultipoint& M = *Multi;
		const float F = static_cast<float>(M.Coordinate->Compute(X, Y, Z));

		const int N = M.Locations.Num();
		const int I = FindIntervalStart(M.Locations, F);
		const int Last = N - 1;

		if (I < 0)
		{
			// f 落在第一个锚点之前，按锚点 0 的导数做线性外推
			const float V0 = M.Values[0]->Evaluate(X, Y, Z);
			return LinearExtend(F, M.Locations, V0, M.Derivatives, 0);
		}
		if (I == Last)
		{
			// f 落在最后一个锚点之后或正好等于
			const float Vn = M.Values[Last]->Evaluate(X, Y, Z);
			return LinearExtend(F, M.Locations, Vn, M.Derivatives, Last);
		}

		const float X0 = M.Locations[I];
		const float X1 = M.Locations[I + 1];
		const float DX = X1 - X0;
		const float T = (F - X0) / DX;

		const float Y0 = M.Values[I]->Evaluate(X, Y, Z);
		const float Y1 = M.Values[I + 1]->Evaluate(X, Y, Z);

		const float D0 = M.Derivatives[I];
		const float D1 = M.Derivatives[I + 1];

		const float K0 = D0 * DX - (Y1 - Y0);
		const float K1 = -D1 * DX + (Y1 - Y0);

		return FMath::Lerp(Y0, Y1, T) + T * (1.0f - T) * FMath::Lerp(K0, K1, T);
	}

	// ──────────────────────────────────────────────────────────
	// FSplineBuilder
	// ──────────────────────────────────────────────────────────

	FSplineBuilder::FSplineBuilder(FDensityRef InCoordinate)
		: Coordinate(MoveTemp(InCoordinate))
	{}

	FSplineBuilder& FSplineBuilder::Add(float Location, float Value, float Derivative)
	{
		return Add(Location, FCubicSpline::Constant(Value), Derivative);
	}

	FSplineBuilder& FSplineBuilder::Add(float Location, FCubicSpline Spline, float Derivative)
	{
		// Mojang 要求按 location 升序添加。
		if (Locations.Num() > 0 && Location <= Locations.Last())
		{
			UE_LOG(LogTemp, Error, TEXT("FSplineBuilder: locations must be added in strictly ascending order (got %f after %f)"),
				Location, Locations.Last());
		}
		Locations.Add(Location);
		Values.Add(MakeShared<const FCubicSpline>(MoveTemp(Spline)));
		Derivatives.Add(Derivative);
		return *this;
	}

	FCubicSpline FSplineBuilder::Build()
	{
		check(Locations.Num() > 0);

		auto Multi = MakeShared<FCubicSpline::FMultipoint>(
			MoveTemp(Coordinate),
			MoveTemp(Locations),
			MoveTemp(Values),
			MoveTemp(Derivatives)
		);

		FCubicSpline S;
		S.Kind = FCubicSpline::EKind::Multipoint;
		S.Multi = Multi;
		return S;
	}
}
