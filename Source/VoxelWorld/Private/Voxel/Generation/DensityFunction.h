#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

namespace MCWorldGen
{
	class FNormalNoise;
	class FCubicSpline;

	/**
	 * 密度函数节点：纯函数 (x, y, z) -> double。
	 * 设计跟 Mojang 的 DensityFunction 树一致：每个节点持有子节点的 SharedRef，
	 * 在 Compute 时递归求值。v1 不实现 cache2d / flatCache / cacheOnce，
	 * 接受性能损失换简单。
	 */
	class IDensityFunction
	{
	public:
		virtual ~IDensityFunction() = default;
		virtual double Compute(double X, double Y, double Z) const = 0;
	};

	using FDensityRef = TSharedPtr<const IDensityFunction>;

	/** 工厂函数族。命名取自 Mojang DensityFunctions 的同名静态方法。 */
	namespace DF
	{
		FDensityRef Constant(double Value);

		FDensityRef Add(FDensityRef A, FDensityRef B);
		FDensityRef Mul(FDensityRef A, FDensityRef B);
		FDensityRef Min(FDensityRef A, FDensityRef B);
		FDensityRef Max(FDensityRef A, FDensityRef B);

		FDensityRef Clamp(FDensityRef Inner, double Lo, double Hi);
		FDensityRef Abs(FDensityRef Inner);
		FDensityRef Cube(FDensityRef Inner);
		FDensityRef Square(FDensityRef Inner);

		// 若 x < 0 返回 x/2，否则返回 x（Mojang halfNegative）。
		FDensityRef HalfNegative(FDensityRef Inner);
		// 若 x < 0 返回 x/4，否则返回 x（Mojang quarterNegative）。
		FDensityRef QuarterNegative(FDensityRef Inner);

		// 在 [YMin, YMax] 区间线性插值至 [VMin, VMax]，超出区间钳制。
		FDensityRef YClampedGradient(double YMin, double YMax, double VMin, double VMax);

		// 噪声采样：在世界坐标 (x*XzScale, y*YScale, z*XzScale) 处采样。
		FDensityRef Noise3D(TSharedRef<const FNormalNoise> Noise, double XzScale, double YScale);
		FDensityRef Noise2D(TSharedRef<const FNormalNoise> Noise, double XzScale);

		// Shifted 2D 噪声：用一对 shift 函数做 domain warp 后采样 2D 噪声。
		// Mojang 的 shiftedNoise2d(shiftX, shiftZ, scale, noise) 等价物：
		//   noise.Sample(x*scale + shiftX(x,y,z), 0, z*scale + shiftZ(x,y,z))
		FDensityRef ShiftedNoise2D(FDensityRef ShiftX, FDensityRef ShiftZ, double XzScale, TSharedRef<const FNormalNoise> Noise);

		// 把 Inner 的输出 4× 后乘到 Inner（Mojang shiftA/shiftB 的等价用法）：
		//   shiftA(noise) = sample at (x*0.25, 0, z*0.25) then * 4
		FDensityRef ShiftNoise(TSharedRef<const FNormalNoise> Noise);

		// 把一个 CubicSpline 包成密度函数节点。等价于 Mojang DensityFunctions.spline。
		FDensityRef Spline(FCubicSpline Spline);
	}
}
