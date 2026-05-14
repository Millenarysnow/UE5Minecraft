#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/DensityFunction.h"

namespace MCWorldGen
{
	/**
	 * 等价于 Mojang CubicSpline。每个节点要么是常量，要么是一个 multipoint：
	 *   coordinate (DensityFunction) → 锚点 (location, value, derivative)，
	 *   value 本身可以是另一个 spline（递归）。
	 *
	 * 求值算法（Mojang Multipoint.apply）：
	 *   t = (f - x_i) / (x_{i+1} - x_i)
	 *   k0 = d_i * (x_{i+1} - x_i) - (y_{i+1} - y_i)
	 *   k1 = -d_{i+1} * (x_{i+1} - x_i) + (y_{i+1} - y_i)
	 *   result = lerp(t, y_i, y_{i+1}) + t*(1-t) * lerp(t, k0, k1)
	 *
	 * 范围外按 derivatives[edge] 做线性外推：
	 *   linearExtend(f, x, y, d, idx) = (d[idx] == 0) ? y : y + d[idx] * (f - x[idx])
	 */
	class FCubicSpline
	{
	public:
		// 常量样条。
		static FCubicSpline Constant(float Value);

		// 评估：递归求值嵌套样条。
		float Evaluate(double X, double Y, double Z) const;

		// 内部 Multipoint 形态。
		struct FMultipoint
		{
			FDensityRef Coordinate;
			TArray<float> Locations;
			TArray<TSharedPtr<const FCubicSpline>> Values; // builder 保证非空
			TArray<float> Derivatives;

			FMultipoint(FDensityRef InCoordinate,
			            TArray<float> InLocations,
			            TArray<TSharedPtr<const FCubicSpline>> InValues,
			            TArray<float> InDerivatives)
				: Coordinate(MoveTemp(InCoordinate))
				, Locations(MoveTemp(InLocations))
				, Values(MoveTemp(InValues))
				, Derivatives(MoveTemp(InDerivatives))
			{}
		};

		FCubicSpline() = default;

	private:
		enum class EKind : uint8 { Constant, Multipoint };
		EKind Kind = EKind::Constant;
		float ConstantValue = 0.0f;
		TSharedPtr<const FMultipoint> Multi;

		friend class FSplineBuilder;
	};

	/**
	 * 样条 builder，仿 Mojang CubicSpline.Builder。
	 *
	 * 使用：
	 *   FCubicSpline S = FSplineBuilder(coord)
	 *       .Add(-1.0f, 0.0f)
	 *       .Add(-0.4f, NestedSpline)
	 *       .Add(0.0f, 0.07f, 0.5f)  // 带斜率
	 *       .Build();
	 */
	class FSplineBuilder
	{
	public:
		explicit FSplineBuilder(FDensityRef InCoordinate);

		FSplineBuilder& Add(float Location, float Value, float Derivative = 0.0f);
		FSplineBuilder& Add(float Location, FCubicSpline Spline, float Derivative = 0.0f);

		FCubicSpline Build();

	private:
		FDensityRef Coordinate;
		TArray<float> Locations;
		TArray<TSharedPtr<const FCubicSpline>> Values;
		TArray<float> Derivatives;
	};
}
