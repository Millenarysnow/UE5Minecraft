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
		Router = MakeUnique<FNoiseRouter>(static_cast<uint64>(WorldSeed), bEnableCaves);
		SurfaceSystem = MakeUnique<MCWorldGen::FSurfaceSystem>(static_cast<uint64>(WorldSeed));
		BiomeSource = MakeUnique<MCWorldGen::FBiomeSource>();
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
			const FNoiseRouter::FColumnBounds Bounds = Router->EstimateColumnBounds(Col);

			const int ChunkYMin = ChunkOriginWorldVoxel.Z;
			const int ChunkYMax = ChunkOriginWorldVoxel.Z + ChunkSize - 1;

			// 计算本列的 biome（一次，整列共用 —— v1 不支持垂直 biome 分层）。
			// Depth 在 Mojang 是 y-相关量，但仅用于 cave biomes，我们 v1 不接，传 0。
			const EBiome Biome = BiomeSource->SampleBiome(
				Router->Continentalness(McX, McZ),
				Router->ErosionVal(McX, McZ),
				Router->RidgesFolded(McX, McZ),
				Router->Temperature(McX, McZ),
				Router->Humidity(McX, McZ),
				0.0
			);

			// 先确定本 chunk 顶部 (lz=ChunkSize) 上方一格的方块 + 状态机初始位（"是否已经在 stone 之下"），
			// 用于 Pass 1 的自上而下扫描判断 "air-density 方块该判为 cave Air 还是 sea Water"。
			const int AboveChunkUEz = ChunkOriginWorldVoxel.Z + ChunkSize;
			const double EstimatedSurface = static_cast<double>(FNoiseRouter::MinY) + 128.0 * (1.5 + Col.Offset);
			const bool bCanFastStone = !Router->AreCavesEnabled();

			EBlock BlockAbove;
			bool bSeenStoneInit; // Pass 1 状态机初始值
			if (bCanFastStone && AboveChunkUEz <= Bounds.StoneYMax)
			{
				BlockAbove = EBlock::Stone;
				bSeenStoneInit = true;
			}
			else if (AboveChunkUEz >= Bounds.AirYMin)
			{
				BlockAbove = (AboveChunkUEz <= FNoiseRouter::SeaLevel) ? EBlock::Water : EBlock::Air;
				bSeenStoneInit = false;
			}
			else
			{
				const double AboveDensity = Router->DensityInColumn(Col, static_cast<double>(AboveChunkUEz));
				if (AboveDensity > 0.0)
				{
					BlockAbove = EBlock::Stone;
					bSeenStoneInit = true;
				}
				else
				{
					// density ≤ 0 但不一定是天空：可能是地下洞穴。用 offset 估算的 surface 区分。
					const bool bAboveIsUnderground = (static_cast<double>(AboveChunkUEz) < EstimatedSurface - 30.0);
					if (bAboveIsUnderground)
					{
						BlockAbove = EBlock::Air; // 洞穴内
						bSeenStoneInit = true;
					}
					else
					{
						BlockAbove = (AboveChunkUEz <= FNoiseRouter::SeaLevel) ? EBlock::Water : EBlock::Air;
						bSeenStoneInit = false;
					}
				}
			}

			// Pass 1：density → 主体方块（stone / water / air / bedrock）。
			// 快速路径：本 chunk 完全在 stone 或 air 一侧 → 跳过逐 y 评估。
			// 注意：当 caves 启用时，"全 stone" 不再安全（cheese 洞会挖空地下），所以只对 air 走 fast path。
			if (bCanFastStone && ChunkYMax <= Bounds.StoneYMax)
			{
				// 全 stone（无 caves 模式下安全）。
				for (int lz = 0; lz < ChunkSize; ++lz)
				{
					const int worldUEz = ChunkYMin + lz;
					Column[lz] = (worldUEz == FNoiseRouter::MinY) ? EBlock::Bedrock : EBlock::Stone;
				}
			}
			else if (ChunkYMin >= Bounds.AirYMin)
			{
				// 全 air / water（依 y 跟 sea level 比）。caves 不影响 air 区。
				for (int lz = 0; lz < ChunkSize; ++lz)
				{
					const int worldUEz = ChunkYMin + lz;
					Column[lz] = (worldUEz <= FNoiseRouter::SeaLevel) ? EBlock::Water : EBlock::Air;
				}
			}
			else
			{
				// 跨越过渡区域：自上而下逐 y 评估密度。带状态机区分 sea Water / cave Air。
				bool bSeenStone = bSeenStoneInit;
				for (int lz = ChunkSize - 1; lz >= 0; --lz)
				{
					const int worldUEz = ChunkYMin + lz; // UE.Z = MC.Y
					const double McY = static_cast<double>(worldUEz);

					const double Density = Router->DensityInColumn(Col, McY);

					EBlock Block;
					if (worldUEz == FNoiseRouter::MinY)
					{
						Block = EBlock::Bedrock;
						bSeenStone = true;
					}
					else if (Density > 0.0)
					{
						Block = EBlock::Stone;
						bSeenStone = true;
					}
					else if (bSeenStone)
					{
						// 已经在 stone 之下：低密度方块 = 洞穴空腔，永远 Air（不论 y）。
						Block = EBlock::Air;
					}
					else
					{
						// 还没碰到 stone：处在世界表面之上，按 sea level 决定 sea Water / 天空 Air。
						Block = (worldUEz <= FNoiseRouter::SeaLevel) ? EBlock::Water : EBlock::Air;
					}

					Column[lz] = Block;
				}
			}

			// Pass 2：表层规则（grass / dirt / sand 替换最顶层 stone）。BlockAbove 已经在上面算过了。
			// 仅当本 chunk 不是"完全在 stone 之下"时才应用 —— 否则会把洞穴顶误判成世界表面糊草。
			if (!bSeenStoneInit)
			{
				SurfaceSystem->ApplyColumn(McX, McZ, ChunkOriginWorldVoxel.Z, Biome, BlockAbove, bDebugBiomeColors, Column);
			}

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
		const double Tem  = Router->Temperature(Wx, Wz);
		const double Hum  = Router->Humidity(Wx, Wz);
		const EBiome Biome = BiomeSource->SampleBiome(Cont, Ero, Pv, Tem, Hum, 0.0);

		const TCHAR* BiomeName = TEXT("?");
		switch (Biome)
		{
		case EBiome::Ocean:        BiomeName = TEXT("Ocean");       break;
		case EBiome::Plains:       BiomeName = TEXT("Plains");      break;
		case EBiome::Forest:       BiomeName = TEXT("Forest");      break;
		case EBiome::Desert:       BiomeName = TEXT("Desert");      break;
		case EBiome::SnowyPlains:  BiomeName = TEXT("SnowyPlains"); break;
		case EBiome::Mountains:    BiomeName = TEXT("Mountains");   break;
		}

		UE_LOG(LogTemp, Display,
			TEXT("[Phase2] %s @(%.0f,%.0f) cont=%.3f ero=%.3f ridge=%.3f pv=%.3f T=%.3f H=%.3f | off=%.3f fac=%.3f jag=%.3f | biome=%s"),
			Label, Wx, Wz, Cont, Ero, Rid, Pv, Tem, Hum, Off, Fac, Jag, BiomeName);

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
