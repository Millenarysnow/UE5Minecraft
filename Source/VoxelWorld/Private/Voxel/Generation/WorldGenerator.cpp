#include "Voxel/Generation/WorldGenerator.h"

#include "Voxel/Generation/CubicSpline.h"
#include "Voxel/Generation/DensityFunction.h"
#include "Voxel/Generation/Hash.h"
#include "Voxel/Generation/NoiseRouter.h"
#include "Voxel/Generation/Noises.h"
#include "Voxel/Generation/OctavedNoise.h"

namespace
{
	using MCWorldGen::FNoiseRouter;
}

void UWorldGenerator::EnsureRouter()
{
	if (!Router.IsValid() || RouterSeed != WorldSeed)
	{
		Router = MakeUnique<FNoiseRouter>(static_cast<uint64>(WorldSeed));
		SurfaceSystem = MakeUnique<MCWorldGen::FSurfaceSystem>(static_cast<uint64>(WorldSeed));
		RouterSeed = WorldSeed;
	}
}

void UWorldGenerator::FillChunk(const FIntVector& ChunkOriginWorldVoxel, int ChunkSize, TArray<EBlock>& OutBlocks)
{
	if (!bSmokeTested)
	{
		LogPhase1SmokeTest();
		bSmokeTested = true;
	}

	EnsureRouter();

	if (!bPhase2Logged)
	{
		LogPhase2SmokeTest();
		bPhase2Logged = true;
	}

	OutBlocks.SetNumUninitialized(ChunkSize * ChunkSize * ChunkSize);

	// 单列暂存缓冲（lz 维），处理完后再写回三维 OutBlocks。
	TArray<EBlock> Column;
	Column.SetNum(ChunkSize);

	// UE 坐标系：X、Y 水平，Z 垂直。MC 坐标系：X、Z 水平，Y 垂直。
	// 我们把 UE.Y 当作 MC.Z，UE.Z 当作 MC.Y，输入给密度函数。
	for (int ly = 0; ly < ChunkSize; ++ly)
	{
		const double McZ = static_cast<double>(ChunkOriginWorldVoxel.Y + ly); // UE.Y → MC.Z
		for (int lx = 0; lx < ChunkSize; ++lx)
		{
			const double McX = static_cast<double>(ChunkOriginWorldVoxel.X + lx); // UE.X → MC.X

			const FNoiseRouter::FColumnState Col = Router->BuildColumn(McX, McZ);

			// Pass 1：density → 主体方块（stone / water / air / bedrock）。
			for (int lz = 0; lz < ChunkSize; ++lz)
			{
				const int worldUEz = ChunkOriginWorldVoxel.Z + lz; // UE.Z = MC.Y
				const double McY = static_cast<double>(worldUEz);

				const double Density = Router->DensityInColumn(Col, McY);

				EBlock Block;
				if (worldUEz == FNoiseRouter::MinY)
				{
					Block = EBlock::Bedrock;
				}
				else if (Density > 0.0)
				{
					Block = EBlock::Stone;
				}
				else
				{
					Block = (worldUEz <= FNoiseRouter::SeaLevel) ? EBlock::Water : EBlock::Air;
				}

				Column[lz] = Block;
			}

			// Pass 2：表层规则（grass / dirt / sand 替换最顶层 stone）。
			// 多采样一次"本 chunk 顶面再上一格"的密度，让表层判断能够覆盖 lz=ChunkSize-1 是 stone 的边界 case。
			const int AboveChunkUEz = ChunkOriginWorldVoxel.Z + ChunkSize;
			const double AboveDensity = Router->DensityInColumn(Col, static_cast<double>(AboveChunkUEz));
			EBlock BlockAbove;
			if (AboveDensity > 0.0)                                    BlockAbove = EBlock::Stone;
			else if (AboveChunkUEz <= FNoiseRouter::SeaLevel)          BlockAbove = EBlock::Water;
			else                                                        BlockAbove = EBlock::Air;
			SurfaceSystem->ApplyColumn(McX, McZ, ChunkOriginWorldVoxel.Z, BlockAbove, Column);

			// 写回三维 OutBlocks。
			for (int lz = 0; lz < ChunkSize; ++lz)
			{
				const int Idx = lx + ChunkSize * (ly + ChunkSize * lz);
				OutBlocks[Idx] = Column[lz];
			}
		}
	}
}

UWorldGenerator* UWorldGenerator::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject) return nullptr;
	UGameInstance* GameInstance = WorldContextObject->GetWorld()->GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UWorldGenerator>() : nullptr;
}

void UWorldGenerator::LogPhase1SmokeTest() const
{
	using namespace MCWorldGen;

	const uint64 Seed = static_cast<uint64>(WorldSeed);

	auto Cont = MakeShared<const FNormalNoise>(HashKey(Seed, Noises::Continentalness.Key), Noises::Continentalness.Params);
	UE_LOG(LogTemp, Display, TEXT("[Phase1] continentalness@(0,0)        = %.4f"), Cont->Sample2D(0.0, 0.0));
	UE_LOG(LogTemp, Display, TEXT("[Phase1] continentalness@(1000,500)   = %.4f"), Cont->Sample2D(1000.0, 500.0));
	UE_LOG(LogTemp, Display, TEXT("[Phase1] continentalness@(-2048,2048) = %.4f"), Cont->Sample2D(-2048.0, 2048.0));

	auto YGrad = DF::YClampedGradient(-64.0, 320.0, 1.5, -1.5);
	UE_LOG(LogTemp, Display, TEXT("[Phase1] yGrad(y=-64)  = %.4f (expect 1.5)"),  YGrad->Compute(0, -64, 0));
	UE_LOG(LogTemp, Display, TEXT("[Phase1] yGrad(y=128)  = %.4f (expect 0.0)"),  YGrad->Compute(0, 128, 0));
	UE_LOG(LogTemp, Display, TEXT("[Phase1] yGrad(y=320)  = %.4f (expect -1.5)"), YGrad->Compute(0, 320, 0));

	auto ContDF = DF::Noise2D(Cont, 0.25);
	FCubicSpline S = FSplineBuilder(ContDF)
		.Add(-1.0f, -0.20f)
		.Add( 0.0f,  0.10f)
		.Add( 1.0f,  0.50f)
		.Build();
	UE_LOG(LogTemp, Display, TEXT("[Phase1] spline @(0,0)        = %.4f"),  S.Evaluate(0.0, 0.0, 0.0));
	UE_LOG(LogTemp, Display, TEXT("[Phase1] spline @(1000,500)   = %.4f"),  S.Evaluate(1000.0, 0.0, 500.0));

	auto Hn = DF::HalfNegative(DF::Constant(-0.4));
	auto Qn = DF::QuarterNegative(DF::Constant(-0.4));
	auto Pn = DF::HalfNegative(DF::Constant(0.4));
	UE_LOG(LogTemp, Display, TEXT("[Phase1] halfNeg(-0.4)  = %.4f (expect -0.20)"), Hn->Compute(0, 0, 0));
	UE_LOG(LogTemp, Display, TEXT("[Phase1] quartNeg(-0.4) = %.4f (expect -0.10)"), Qn->Compute(0, 0, 0));
	UE_LOG(LogTemp, Display, TEXT("[Phase1] halfNeg(+0.4)  = %.4f (expect +0.40)"), Pn->Compute(0, 0, 0));
}

void UWorldGenerator::LogPhase2SmokeTest()
{
	using namespace MCWorldGen;
	if (!Router.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Phase2] Router not ready"));
		return;
	}

	auto LogPos = [this](const TCHAR* Label, double Wx, double Wz)
	{
		const double Cont = Router->Continentalness(Wx, Wz);
		const double Ero  = Router->ErosionVal(Wx, Wz);
		const double Rid  = Router->Ridges(Wx, Wz);
		const double Pv   = Router->RidgesFolded(Wx, Wz);
		const double Off  = Router->Offset(Wx, Wz);
		const double Fac  = Router->Factor(Wx, Wz);
		const double Jag  = Router->Jaggedness(Wx, Wz);
		UE_LOG(LogTemp, Display,
			TEXT("[Phase2] %s @(%.0f,%.0f) cont=%.3f ero=%.3f ridge=%.3f pv=%.3f | off=%.3f fac=%.3f jag=%.3f"),
			Label, Wx, Wz, Cont, Ero, Rid, Pv, Off, Fac, Jag);

		// 也打几条垂直 density profile，方便看 surface y
		const FNoiseRouter::FColumnState Col = Router->BuildColumn(Wx, Wz);
		for (const int Y : { -32, 0, 32, 63, 80, 100, 128, 180, 240 })
		{
			const double D = Router->DensityInColumn(Col, static_cast<double>(Y));
			UE_LOG(LogTemp, Display, TEXT("[Phase2]   y=%4d  density=%+0.3f  %s"),
				Y, D, D > 0.0 ? TEXT("STONE") : (Y <= FNoiseRouter::SeaLevel ? TEXT("WATER") : TEXT("AIR")));
		}
	};

	LogPos(TEXT("origin"),  0.0,    0.0);
	LogPos(TEXT("near"),    100.0,  100.0);
	LogPos(TEXT("far"),     1000.0, 1000.0);
	LogPos(TEXT("negfar"), -1500.0, 500.0);
}
