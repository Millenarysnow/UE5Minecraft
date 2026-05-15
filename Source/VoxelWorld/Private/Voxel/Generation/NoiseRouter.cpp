#include "Voxel/Generation/NoiseRouter.h"

#include "Voxel/Generation/CubicSpline.h"
#include "Voxel/Generation/Hash.h"
#include "Voxel/Generation/Noises.h"
#include "Voxel/Generation/OctavedNoise.h"
#include "Voxel/Generation/TerrainSplines.h"

namespace MCWorldGen
{
	namespace
	{
		// Mojang NoiseRouterData.GLOBAL_OFFSET，添加到 offset spline 输出上的全局偏置。
		constexpr double GLOBAL_OFFSET = -0.50375;

		// Mojang slide：让世界顶/底密度平滑过渡到全空气/全石头。参数照 Mojang slideOverworld(false, ...)。
		double ApplySlide(double Density, double Wy)
		{
			// Top slide: y=240..256 lerp 到 -0.078125（让 y>=256 都是空气）
			constexpr double TopFromY = 240.0; // MinY + Height - 80
			constexpr double TopToY   = 256.0; // MinY + Height - 64
			constexpr double ValTop   = -0.078125;
			if (Wy >= TopFromY)
			{
				const double T = (Wy >= TopToY) ? 0.0 : 1.0 - (Wy - TopFromY) / (TopToY - TopFromY);
				Density = ValTop + T * (Density - ValTop);
			}

			// Bottom slide: y=-64..-40 lerp 到 0.1171875（让 y<=-64 都是石头）
			constexpr double BotFromY = -64.0; // MinY + 0
			constexpr double BotToY   = -40.0; // MinY + 24
			constexpr double ValBot   = 0.1171875;
			if (Wy <= BotToY)
			{
				const double T = (Wy <= BotFromY) ? 0.0 : (Wy - BotFromY) / (BotToY - BotFromY);
				Density = ValBot + T * (Density - ValBot);
			}

			return Density;
		}
	}

	FNoiseRouter::FNoiseRouter(uint64 WorldSeed, bool bEnableCaves, double ContinentBias)
		: bCavesEnabled(bEnableCaves)
	{
		auto MakeNoise = [WorldSeed](const FNoiseDef& Def) -> TSharedRef<const FNormalNoise>
		{
			return MakeShared<FNormalNoise>(HashKey(WorldSeed, Def.Key), Def.Params);
		};

		ContinentNoise = MakeNoise(Noises::Continentalness);
		ErosionNoise   = MakeNoise(Noises::Erosion);
		RidgeNoise     = MakeNoise(Noises::Ridge);
		TempNoise      = MakeNoise(Noises::Temperature);
		VegNoise       = MakeNoise(Noises::Vegetation);
		JaggedNoise    = MakeNoise(Noises::Jagged);

		if (bCavesEnabled)
		{
			CaveCheeseNoise   = MakeNoise(Noises::CaveCheese);
			CaveEntranceNoise = MakeNoise(Noises::CaveEntrance);
		}

		// base_3d_noise 替代品：Mojang 用 BlendedNoise(0.25, 0.125, 80, 160, 8.0)，等效采样在
		// 大约 (x*0.003125, y*0.000781, z*0.003125)。我们用一个 5 octave 的 NormalNoise 近似，
		// 频率倍率最终通过 Noise3D(scale) 设置。这里直接构造并保存为成员。
		Base3DNoise = MakeShared<FNormalNoise>(
			HashKey(WorldSeed, TEXT("base_3d_noise")),
			FOctavedNoiseParameters(-7, {1.0, 1.0, 1.0, 1.0, 1.0})
		);

		// Climate samplers（v1 不做 domain warp，直接 2D 噪声）
		ContinentalnessDF = DF::Noise2D(ContinentNoise.ToSharedRef(), 0.25);
		ErosionDF         = DF::Noise2D(ErosionNoise.ToSharedRef(),   0.25);
		RidgesDF          = DF::Noise2D(RidgeNoise.ToSharedRef(),     0.25);
		TemperatureDF     = DF::Noise2D(TempNoise.ToSharedRef(),      0.25);
		HumidityDF        = DF::Noise2D(VegNoise.ToSharedRef(),       0.25);

		// peaks-and-valleys 折叠：
		//   pv(r) = -3 * (abs(abs(r) + (-2/3)) + (-1/3))
		RidgesFoldedDF = DF::Mul(
			DF::Add(
				DF::Abs(DF::Add(DF::Abs(RidgesDF), DF::Constant(-2.0 / 3.0))),
				DF::Constant(-1.0 / 3.0)
			),
			DF::Constant(-3.0)
		);

		// 三个核心地形 spline
		FCubicSpline OffsetSpline     = TerrainSplines::BuildOffset(ContinentalnessDF, ErosionDF, RidgesFoldedDF);
		FCubicSpline FactorSpline     = TerrainSplines::BuildFactor(ContinentalnessDF, ErosionDF, RidgesDF, RidgesFoldedDF);
		FCubicSpline JaggednessSpline = TerrainSplines::BuildJaggedness(ContinentalnessDF, ErosionDF, RidgesDF, RidgesFoldedDF);

		OffsetDF     = DF::Add(DF::Constant(GLOBAL_OFFSET + ContinentBias), DF::Spline(MoveTemp(OffsetSpline)));
		FactorDF     = DF::Spline(MoveTemp(FactorSpline));
		JaggednessDF = DF::Spline(MoveTemp(JaggednessSpline));
	}

	FNoiseRouter::FColumnState FNoiseRouter::BuildColumn(double Wx, double Wz) const
	{
		FColumnState S;
		S.Wx = Wx;
		S.Wz = Wz;
		S.Offset     = OffsetDF    ->Compute(Wx, 0.0, Wz);
		S.Factor     = FactorDF    ->Compute(Wx, 0.0, Wz);
		S.Jaggedness = JaggednessDF->Compute(Wx, 0.0, Wz);

		// jagged 噪声：xz_scale = 1500（高频），y_scale = 0（不依赖 y）
		const double JagPerlin = JaggedNoise->Sample(Wx * 1500.0, 0.0, Wz * 1500.0);
		S.JagPerlinHalfNeg = (JagPerlin < 0.0) ? JagPerlin * 0.5 : JagPerlin;

		return S;
	}

	double FNoiseRouter::DensityInColumn(const FColumnState& Col, double Wy) const
	{
		// y_clamped_gradient(-64..320, 1.5..-1.5)
		double Ygrad;
		if      (Wy <= MinY)        Ygrad =  1.5;
		else if (Wy >= MaxY)        Ygrad = -1.5;
		else                         Ygrad =  1.5 + (Wy - MinY) * (-3.0 / Height);

		const double Depth   = Ygrad + Col.Offset;
		const double JagTerm = Col.Jaggedness * Col.JagPerlinHalfNeg;
		const double Inner   = (Depth + JagTerm) * Col.Factor;

		// Mojang noiseGradientDensity = 4 * quarter_negative(inner)
		const double SlopedNoBase = 4.0 * (Inner < 0.0 ? Inner * 0.25 : Inner);

		// base_3d_noise（替代 Mojang BlendedNoise）。Base3DNoise 内部 firstOctave=-7，
		// 故最低 octave 在采样值上的内置周期 = 2^7 = 128。再用 (1.0, 0.5) 缩放 xz / y，
		// 得到 (xz=128, y=256) 块的 3D 噪声特征——山脉级尺度，并稍稍拉高 y 以更易出现 overhang。
		const double B3 = Base3DNoise->Sample(Col.Wx * 1.0, Wy * 0.5, Col.Wz * 1.0);

		const double SlopedCheese = SlopedNoBase + B3;

		double Density = SlopedCheese;

		// Cheese 洞穴：Mojang underground.cheese 项的 v1 简化（不含 spaghetti / pillar / cave_layer）。
		//   cheese_term = clamp(0.27 + cave_noise, -1, 1) + clamp(1.5 - 0.64·sloped_cheese, 0, 0.5)
		// 第二项在表层附近常驻 0.5 防止洞被挖到地表，深处 → 0 才允许 cave_noise 主导。
		// final = min(sloped_cheese, cheese_term, 5·entrance_term)：任一项 < 0 即挖空。
		if (bCavesEnabled && CaveCheeseNoise.IsValid())
		{
			// Cheese caves（地下大型空腔）：Mojang noise(CAVE_CHEESE, 0.6666) → xz=1, y=0.6666
			const double CaveNoise = CaveCheeseNoise->Sample(Col.Wx * 1.0, Wy * 0.6666666666666666, Col.Wz * 1.0);
			const double CheesePart1 = FMath::Clamp(0.27 + CaveNoise, -1.0, 1.0);
			const double CheesePart2 = FMath::Clamp(1.5 - 0.64 * SlopedCheese, 0.0, 0.5);
			const double CheeseTerm  = CheesePart1 + CheesePart2;

			// Cave entrances（表层入口洞）：Mojang NoiseRouterData.entrances() 的 densityfunction7。
			//   entrance = caveEntranceNoise(xz=0.75, y=0.5) + 0.37 + yClampedGradient(-10..30, 0.3..0.0)
			// y-gradient：地下深处 (y≤-10) 加 +0.3 抑制入口；y≥30 不加偏置，让入口从 y=30 起切到地表。
			// final 时再 ×5 让入口在表层 sloped_cheese≈0 时也能强力挖穿。
			const double EntranceNoise = CaveEntranceNoise.IsValid()
				? CaveEntranceNoise->Sample(Col.Wx * 0.75, Wy * 0.5, Col.Wz * 0.75)
				: 1.0; // 不会触发 carve
			double YEntranceGrad;
			if (Wy <= -10.0)      YEntranceGrad = 0.3;
			else if (Wy >= 30.0)  YEntranceGrad = 0.0;
			else                  YEntranceGrad = 0.3 * (30.0 - Wy) / 40.0;
			const double EntranceTerm = EntranceNoise + 0.37 + YEntranceGrad;

			// 合并三个 carve 项（Mojang underground = min(cheese, entrances, spaghetti)；我们没 spaghetti）
			const double CombinedCaves = FMath::Min(CheeseTerm, 5.0 * EntranceTerm);
			Density = FMath::Min(Density, CombinedCaves);
		}

		return ApplySlide(Density, Wy);
	}

	double FNoiseRouter::FinalDensity(double Wx, double Wy, double Wz) const
	{
		const FColumnState Col = BuildColumn(Wx, Wz);
		return DensityInColumn(Col, Wy);
	}

	FNoiseRouter::FColumnBounds FNoiseRouter::EstimateColumnBounds(const FColumnState& Col) const
	{
		// density(y) = 4 * qn((depth(y) + jagTerm) * factor) + base3D
		//   depth(y) = ygrad(y) + offset，ygrad(y) = 1.5 - 3·(y+64)/384 = 1.5 - (y+64)/128
		//   |jagTerm| ≤ |jaggedness|（半负），但 jaggedness 通常很小，可忽略
		//   |base3D| 上界用经验值 2.5（5 octave NormalNoise 的实际典型上限）
		//
		// 必为 stone：4·qn(inner) > +base3D_max
		//   inner > base3D_max / 4 = 0.625（因为 inner > 0 时 qn(inner)=inner）
		//   depth · factor > 0.625
		//   depth > 0.625 / factor
		// 必为 air：4·qn(inner) < -base3D_max
		//   inner negative path：4·qn(inner) = inner，需 inner < -base3D_max
		//   depth · factor < -base3D_max
		//   depth < -base3D_max / factor

		constexpr double Base3DMax = 2.5;

		// 防御性：factor 不应为 0 或负，但加点保护
		const double F = FMath::Max(Col.Factor, 0.01);

		const double DepthStone = Base3DMax * 0.25 / F; // depth 阈值，超过就一定 stone
		const double DepthAir   = -Base3DMax / F;       // depth 阈值，低于就一定 air

		// depth = ygrad + offset，ygrad = 1.5 - (y+64)/128
		//   solve depth > DepthStone → 1.5 - (y+64)/128 + offset > DepthStone
		//                            → y < 128·(1.5 + offset - DepthStone) - 64
		//   solve depth < DepthAir   → y > 128·(1.5 + offset - DepthAir) - 64
		const double KStone = 128.0 * (1.5 + Col.Offset - DepthStone) - 64.0;
		const double KAir   = 128.0 * (1.5 + Col.Offset - DepthAir)   - 64.0;

		FColumnBounds B;
		// floor 取整保证 "y <= StoneYMax 必为 stone"。
		B.StoneYMax = static_cast<int>(FMath::FloorToDouble(KStone));
		// ceil 取整保证 "y >= AirYMin 必为 air"。
		B.AirYMin   = static_cast<int>(FMath::CeilToDouble(KAir));
		return B;
	}
}
