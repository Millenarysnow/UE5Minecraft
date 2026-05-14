#include "Voxel/Generation/DensityFunction.h"

#include "Voxel/Generation/CubicSpline.h"
#include "Voxel/Generation/OctavedNoise.h"

namespace MCWorldGen::DF
{
	namespace
	{
		// ──────────────────────────────────────────────────────────
		// 节点实现
		// ──────────────────────────────────────────────────────────

		class FConstant : public IDensityFunction
		{
		public:
			explicit FConstant(double V) : Value(V) {}
			virtual double Compute(double, double, double) const override { return Value; }
		private:
			double Value;
		};

		class FBinaryOp : public IDensityFunction
		{
		public:
			FBinaryOp(FDensityRef InA, FDensityRef InB) : A(MoveTemp(InA)), B(MoveTemp(InB)) {}
		protected:
			FDensityRef A;
			FDensityRef B;
		};

		class FAdd : public FBinaryOp
		{
		public:
			using FBinaryOp::FBinaryOp;
			virtual double Compute(double X, double Y, double Z) const override
			{
				return A->Compute(X, Y, Z) + B->Compute(X, Y, Z);
			}
		};

		class FMul : public FBinaryOp
		{
		public:
			using FBinaryOp::FBinaryOp;
			virtual double Compute(double X, double Y, double Z) const override
			{
				return A->Compute(X, Y, Z) * B->Compute(X, Y, Z);
			}
		};

		class FMin : public FBinaryOp
		{
		public:
			using FBinaryOp::FBinaryOp;
			virtual double Compute(double X, double Y, double Z) const override
			{
				return FMath::Min(A->Compute(X, Y, Z), B->Compute(X, Y, Z));
			}
		};

		class FMax : public FBinaryOp
		{
		public:
			using FBinaryOp::FBinaryOp;
			virtual double Compute(double X, double Y, double Z) const override
			{
				return FMath::Max(A->Compute(X, Y, Z), B->Compute(X, Y, Z));
			}
		};

		class FClamp : public IDensityFunction
		{
		public:
			FClamp(FDensityRef In, double InLo, double InHi)
				: Inner(MoveTemp(In)), Lo(InLo), Hi(InHi) {}
			virtual double Compute(double X, double Y, double Z) const override
			{
				return FMath::Clamp(Inner->Compute(X, Y, Z), Lo, Hi);
			}
		private:
			FDensityRef Inner;
			double Lo;
			double Hi;
		};

		class FUnaryOp : public IDensityFunction
		{
		public:
			explicit FUnaryOp(FDensityRef In) : Inner(MoveTemp(In)) {}
		protected:
			FDensityRef Inner;
		};

		class FAbs : public FUnaryOp
		{
		public:
			using FUnaryOp::FUnaryOp;
			virtual double Compute(double X, double Y, double Z) const override
			{
				return FMath::Abs(Inner->Compute(X, Y, Z));
			}
		};

		class FCube : public FUnaryOp
		{
		public:
			using FUnaryOp::FUnaryOp;
			virtual double Compute(double X, double Y, double Z) const override
			{
				const double V = Inner->Compute(X, Y, Z);
				return V * V * V;
			}
		};

		class FSquare : public FUnaryOp
		{
		public:
			using FUnaryOp::FUnaryOp;
			virtual double Compute(double X, double Y, double Z) const override
			{
				const double V = Inner->Compute(X, Y, Z);
				return V * V;
			}
		};

		class FHalfNegative : public FUnaryOp
		{
		public:
			using FUnaryOp::FUnaryOp;
			virtual double Compute(double X, double Y, double Z) const override
			{
				const double V = Inner->Compute(X, Y, Z);
				return V < 0.0 ? V * 0.5 : V;
			}
		};

		class FQuarterNegative : public FUnaryOp
		{
		public:
			using FUnaryOp::FUnaryOp;
			virtual double Compute(double X, double Y, double Z) const override
			{
				const double V = Inner->Compute(X, Y, Z);
				return V < 0.0 ? V * 0.25 : V;
			}
		};

		class FYClampedGradient : public IDensityFunction
		{
		public:
			FYClampedGradient(double InYMin, double InYMax, double InVMin, double InVMax)
				: YMin(InYMin), YMax(InYMax), VMin(InVMin), VMax(InVMax)
			{}
			virtual double Compute(double, double Y, double) const override
			{
				if (Y <= YMin) return VMin;
				if (Y >= YMax) return VMax;
				const double T = (Y - YMin) / (YMax - YMin);
				return VMin + T * (VMax - VMin);
			}
		private:
			double YMin, YMax, VMin, VMax;
		};

		class FNoise3D : public IDensityFunction
		{
		public:
			FNoise3D(TSharedRef<const FNormalNoise> InNoise, double InXz, double InY)
				: Noise(MoveTemp(InNoise)), XzScale(InXz), YScale(InY) {}
			virtual double Compute(double X, double Y, double Z) const override
			{
				return Noise->Sample(X * XzScale, Y * YScale, Z * XzScale);
			}
		private:
			TSharedRef<const FNormalNoise> Noise;
			double XzScale;
			double YScale;
		};

		class FNoise2D : public IDensityFunction
		{
		public:
			FNoise2D(TSharedRef<const FNormalNoise> InNoise, double InXz)
				: Noise(MoveTemp(InNoise)), XzScale(InXz) {}
			virtual double Compute(double X, double, double Z) const override
			{
				return Noise->Sample(X * XzScale, 0.0, Z * XzScale);
			}
		private:
			TSharedRef<const FNormalNoise> Noise;
			double XzScale;
		};

		class FShiftedNoise2D : public IDensityFunction
		{
		public:
			FShiftedNoise2D(FDensityRef InShiftX, FDensityRef InShiftZ, double InXz, TSharedRef<const FNormalNoise> InNoise)
				: ShiftX(MoveTemp(InShiftX)), ShiftZ(MoveTemp(InShiftZ)), XzScale(InXz), Noise(MoveTemp(InNoise))
			{}
			virtual double Compute(double X, double Y, double Z) const override
			{
				const double Sx = X * XzScale + ShiftX->Compute(X, Y, Z);
				const double Sz = Z * XzScale + ShiftZ->Compute(X, Y, Z);
				return Noise->Sample(Sx, 0.0, Sz);
			}
		private:
			FDensityRef ShiftX;
			FDensityRef ShiftZ;
			double XzScale;
			TSharedRef<const FNormalNoise> Noise;
		};

		// Mojang shiftA/shiftB：噪声在 (x*0.25, 0, z*0.25) 采样后乘 4。shiftA 用 (x,0,z)，shiftB 用 (z,x,0)。
		// 我们做一个统一的：sample(x*0.25, 0, z*0.25) * 4。这是一个普通的 2D shift 噪声，
		// 由 NoiseRouter 的两个 shiftA/shiftB 实例负责给出 X 与 Z 两个方向的偏移。
		class FShiftNoise : public IDensityFunction
		{
		public:
			explicit FShiftNoise(TSharedRef<const FNormalNoise> InNoise) : Noise(MoveTemp(InNoise)) {}
			virtual double Compute(double X, double Y, double Z) const override
			{
				return Noise->Sample(X * 0.25, Y * 0.25, Z * 0.25) * 4.0;
			}
		private:
			TSharedRef<const FNormalNoise> Noise;
		};

		class FSpline : public IDensityFunction
		{
		public:
			explicit FSpline(FCubicSpline InSpline) : Spline(MoveTemp(InSpline)) {}
			virtual double Compute(double X, double Y, double Z) const override
			{
				return static_cast<double>(Spline.Evaluate(X, Y, Z));
			}
		private:
			FCubicSpline Spline;
		};
	}

	// ──────────────────────────────────────────────────────────
	// 工厂函数定义
	// ──────────────────────────────────────────────────────────

	FDensityRef Constant(double Value)
	{
		return MakeShared<FConstant>(Value);
	}

	FDensityRef Add(FDensityRef A, FDensityRef B) { return MakeShared<FAdd>(MoveTemp(A), MoveTemp(B)); }
	FDensityRef Mul(FDensityRef A, FDensityRef B) { return MakeShared<FMul>(MoveTemp(A), MoveTemp(B)); }
	FDensityRef Min(FDensityRef A, FDensityRef B) { return MakeShared<FMin>(MoveTemp(A), MoveTemp(B)); }
	FDensityRef Max(FDensityRef A, FDensityRef B) { return MakeShared<FMax>(MoveTemp(A), MoveTemp(B)); }

	FDensityRef Clamp(FDensityRef Inner, double Lo, double Hi) { return MakeShared<FClamp>(MoveTemp(Inner), Lo, Hi); }
	FDensityRef Abs(FDensityRef Inner)             { return MakeShared<FAbs>(MoveTemp(Inner)); }
	FDensityRef Cube(FDensityRef Inner)            { return MakeShared<FCube>(MoveTemp(Inner)); }
	FDensityRef Square(FDensityRef Inner)          { return MakeShared<FSquare>(MoveTemp(Inner)); }
	FDensityRef HalfNegative(FDensityRef Inner)    { return MakeShared<FHalfNegative>(MoveTemp(Inner)); }
	FDensityRef QuarterNegative(FDensityRef Inner) { return MakeShared<FQuarterNegative>(MoveTemp(Inner)); }

	FDensityRef YClampedGradient(double YMin, double YMax, double VMin, double VMax)
	{
		return MakeShared<FYClampedGradient>(YMin, YMax, VMin, VMax);
	}

	FDensityRef Noise3D(TSharedRef<const FNormalNoise> Noise, double XzScale, double YScale)
	{
		return MakeShared<FNoise3D>(MoveTemp(Noise), XzScale, YScale);
	}

	FDensityRef Noise2D(TSharedRef<const FNormalNoise> Noise, double XzScale)
	{
		return MakeShared<FNoise2D>(MoveTemp(Noise), XzScale);
	}

	FDensityRef ShiftedNoise2D(FDensityRef ShiftX, FDensityRef ShiftZ, double XzScale, TSharedRef<const FNormalNoise> Noise)
	{
		return MakeShared<FShiftedNoise2D>(MoveTemp(ShiftX), MoveTemp(ShiftZ), XzScale, MoveTemp(Noise));
	}

	FDensityRef ShiftNoise(TSharedRef<const FNormalNoise> Noise)
	{
		return MakeShared<FShiftNoise>(MoveTemp(Noise));
	}

	FDensityRef Spline(FCubicSpline S)
	{
		return MakeShared<FSpline>(MoveTemp(S));
	}
}
