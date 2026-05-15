#include "Voxel/CloudLayer.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "ProceduralMeshComponent.h"
#include "Voxel/Utils/FastNoiseLite.h"

ACloudLayer::ACloudLayer()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CloudMesh"));
	Mesh->SetCastShadow(false);
	Mesh->bUseAsyncCooking = true;
	SetRootComponent(Mesh);
}

void ACloudLayer::BeginPlay()
{
	Super::BeginPlay();

	Noise = new FastNoiseLite(20240515);
	Noise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
	Noise->SetFrequency(1.0f);

	// 立刻在玩家位置 spawn 一次（如果玩家还没生成则用 actor 当前位置）
	int CenterX = 0;
	int CenterY = 0;
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				const FVector P = Pawn->GetActorLocation();
				CenterX = FMath::RoundToInt(P.X / 100.f);
				CenterY = FMath::RoundToInt(P.Y / 100.f);
			}
		}
	}
	Rebuild(CenterX, CenterY);
}

void ACloudLayer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Noise)
	{
		delete Noise;
		Noise = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void ACloudLayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	WindOffsetBlocks += WindSpeedBlocksPerSec * DeltaTime;
	TimeSinceRebuild += DeltaTime;

	UWorld* World = GetWorld();
	if (!World) return;
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return;
	APawn* Pawn = PC->GetPawn();
	if (!Pawn) return;

	const FVector P = Pawn->GetActorLocation();
	const int PlayerBlockX = FMath::RoundToInt(P.X / 100.f);
	const int PlayerBlockY = FMath::RoundToInt(P.Y / 100.f);

	const bool bPlayerMoved =
		FMath::Abs(PlayerBlockX - LastPlayerBlockX) >= PlayerMoveRebuildThresholdBlocks ||
		FMath::Abs(PlayerBlockY - LastPlayerBlockY) >= PlayerMoveRebuildThresholdBlocks;

	if (TimeSinceRebuild >= RebuildInterval || bPlayerMoved)
	{
		Rebuild(PlayerBlockX, PlayerBlockY);
		TimeSinceRebuild = 0.0f;
		LastPlayerBlockX = PlayerBlockX;
		LastPlayerBlockY = PlayerBlockY;
	}
}

void ACloudLayer::Rebuild(int CenterBlockX, int CenterBlockY)
{
	if (!Noise) return;
	if (CellSizeBlocks <= 0 || ThicknessBlocks <= 0 || RadiusBlocks <= 0) return;

	// Actor 位置：以玩家中心为原点，y=CloudHeight。
	const FVector NewLocation(CenterBlockX * 100.f, CenterBlockY * 100.f, CloudHeight * 100.f);
	SetActorLocation(NewLocation);

	// 半径以 cell 为单位
	const int Cells = FMath::Max(1, RadiusBlocks / CellSizeBlocks);
	const int Side = 2 * Cells + 1;

	// 先扫一遍噪声，记录哪些 cell 是云（让 side face 知道邻居）
	TArray<bool> Grid;
	Grid.SetNumZeroed(Side * Side);

	// 噪声采样频率：~ 1/(4 * cell)，让 cell 之间有连贯性
	const double NoiseFreq = 1.0 / (CellSizeBlocks * 4.0);

	for (int dy = -Cells; dy <= Cells; ++dy)
	{
		for (int dx = -Cells; dx <= Cells; ++dx)
		{
			// cell 在世界中的中心块坐标
			const int CellCenterBlockX = CenterBlockX + dx * CellSizeBlocks;
			const int CellCenterBlockY = CenterBlockY + dy * CellSizeBlocks;
			// 风偏移：减去 WindOffset 让云沿 +X 向前飘
			const double Sx = (CellCenterBlockX - WindOffsetBlocks) * NoiseFreq;
			const double Sy = CellCenterBlockY * NoiseFreq;
			const float V = Noise->GetNoise(Sx, Sy);
			if (V > Threshold)
			{
				Grid[(dy + Cells) * Side + (dx + Cells)] = true;
			}
		}
	}

	auto IsCloudCell = [&](int dx, int dy) -> bool
	{
		if (dx < -Cells || dx > Cells || dy < -Cells || dy > Cells) return false;
		return Grid[(dy + Cells) * Side + (dx + Cells)];
	};

	// 准备 mesh buffers
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;

	const float CellUE = CellSizeBlocks * 100.f;
	const float ThicknessUE = ThicknessBlocks * 100.f;

	auto AddQuad = [&](const FVector& V0, const FVector& V1, const FVector& V2, const FVector& V3, const FVector& N)
	{
		const int32 Base = Vertices.Num();
		Vertices.Add(V0); Vertices.Add(V1); Vertices.Add(V2); Vertices.Add(V3);
		Normals.Add(N); Normals.Add(N); Normals.Add(N); Normals.Add(N);
		UVs.Add(FVector2D(0, 0)); UVs.Add(FVector2D(1, 0)); UVs.Add(FVector2D(1, 1)); UVs.Add(FVector2D(0, 1));
		Colors.Add(FColor::White); Colors.Add(FColor::White); Colors.Add(FColor::White); Colors.Add(FColor::White);
		Triangles.Add(Base + 0); Triangles.Add(Base + 1); Triangles.Add(Base + 2);
		Triangles.Add(Base + 0); Triangles.Add(Base + 2); Triangles.Add(Base + 3);
	};

	for (int dy = -Cells; dy <= Cells; ++dy)
	{
		for (int dx = -Cells; dx <= Cells; ++dx)
		{
			if (!IsCloudCell(dx, dy)) continue;

			// cell 在 actor-local 坐标里的最小角
			const float X0 = (dx * CellSizeBlocks) * 100.f - CellUE * 0.5f;
			const float Y0 = (dy * CellSizeBlocks) * 100.f - CellUE * 0.5f;
			const float X1 = X0 + CellUE;
			const float Y1 = Y0 + CellUE;
			const float Z0 = 0.0f;            // actor 的 z = CloudHeight*100，所以本地 z=0 对应世界 y=192
			const float Z1 = ThicknessUE;

			// Top（朝 +Z）
			AddQuad(
				FVector(X0, Y0, Z1),
				FVector(X1, Y0, Z1),
				FVector(X1, Y1, Z1),
				FVector(X0, Y1, Z1),
				FVector::UpVector
			);

			// Bottom（朝 -Z；为正确朝向，绕反向写）
			AddQuad(
				FVector(X0, Y1, Z0),
				FVector(X1, Y1, Z0),
				FVector(X1, Y0, Z0),
				FVector(X0, Y0, Z0),
				FVector::DownVector
			);

			// Side faces：仅当邻居非云时生成
			// -X side
			if (!IsCloudCell(dx - 1, dy))
			{
				AddQuad(
					FVector(X0, Y0, Z0),
					FVector(X0, Y1, Z0),
					FVector(X0, Y1, Z1),
					FVector(X0, Y0, Z1),
					FVector(-1, 0, 0)
				);
			}
			// +X side
			if (!IsCloudCell(dx + 1, dy))
			{
				AddQuad(
					FVector(X1, Y1, Z0),
					FVector(X1, Y0, Z0),
					FVector(X1, Y0, Z1),
					FVector(X1, Y1, Z1),
					FVector(1, 0, 0)
				);
			}
			// -Y side
			if (!IsCloudCell(dx, dy - 1))
			{
				AddQuad(
					FVector(X1, Y0, Z0),
					FVector(X0, Y0, Z0),
					FVector(X0, Y0, Z1),
					FVector(X1, Y0, Z1),
					FVector(0, -1, 0)
				);
			}
			// +Y side
			if (!IsCloudCell(dx, dy + 1))
			{
				AddQuad(
					FVector(X0, Y1, Z0),
					FVector(X1, Y1, Z0),
					FVector(X1, Y1, Z1),
					FVector(X0, Y1, Z1),
					FVector(0, 1, 0)
				);
			}
		}
	}

	Mesh->ClearAllMeshSections();
	Mesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, TArray<FProcMeshTangent>(), false);
	if (CloudMaterial)
	{
		Mesh->SetMaterial(0, CloudMaterial);
	}
}
