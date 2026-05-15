#include "Voxel/ChunkWorldSubsystem.h"

#include "Chunk/ChunkBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Utils/VoxelFunctionLibrary.h"

void UChunkWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	RandomEngine.seed(static_cast<unsigned>(std::time(0)));
}

void UChunkWorldSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StreamingTimerHandle);
	}
	Super::Deinitialize();
}

void UChunkWorldSubsystem::StartStreaming()
{
	if (!ChunkType)
	{
		UE_LOG(LogTemp, Error, TEXT("ChunkWorldSubsystem: ChunkType is null, abort StartStreaming"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	// 立刻 tick 一次，让 player 周围 spawn 一些 chunk（避免开局掉到 void）
	TickStreaming();

	// 注册周期 timer
	World->GetTimerManager().SetTimer(
		StreamingTimerHandle,
		this,
		&UChunkWorldSubsystem::TickStreaming,
		StreamingTickInterval,
		true /* looping */
	);

	UE_LOG(LogTemp, Display, TEXT("ChunkWorldSubsystem: streaming started, interval=%.2fs, drawDistance=%d, maxPerTick=%d"),
		StreamingTickInterval, DrawDistance, MaxSpawnsPerTick);
}

void UChunkWorldSubsystem::StopStreaming(bool bDestroyAllChunks)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StreamingTimerHandle);
	}

	if (bDestroyAllChunks)
	{
		for (auto& KV : Chunks)
		{
			if (KV.Value) KV.Value->Destroy();
		}
		Chunks.Empty();
		ChunkCount = 0;
	}
}

FIntVector UChunkWorldSubsystem::PlayerWorldToChunkXY(const FVector& WorldLocation) const
{
	// UE.X / UE.Y → 水平 chunk 网格。每 chunk 边长 = Size 块 = Size*100 UE 单位。
	const int Cx = FMath::FloorToInt(WorldLocation.X / static_cast<float>(Size * 100));
	const int Cy = FMath::FloorToInt(WorldLocation.Y / static_cast<float>(Size * 100));
	return FIntVector(Cx, Cy, 0);
}

void UChunkWorldSubsystem::TickStreaming()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 获取 player 位置；如果还没生成 pawn，跳过这一次 tick
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return;
	APawn* Pawn = PC->GetPawn();
	if (!Pawn) return;

	const FVector PlayerLoc = Pawn->GetActorLocation();
	const FIntVector PlayerChunk = PlayerWorldToChunkXY(PlayerLoc);

	// 期望加载的 chunk 集 = (player ± DrawDistance)² × 垂直范围
	const int MinCz = FMath::FloorToInt(static_cast<float>(MinWorldY) / Size);
	const int MaxCz = FMath::FloorToInt(static_cast<float>(MaxWorldY - 1) / Size);

	TSet<FIntVector> Desired;
	Desired.Reserve((2 * DrawDistance + 1) * (2 * DrawDistance + 1) * (MaxCz - MinCz + 1));
	for (int dx = -DrawDistance; dx <= DrawDistance; ++dx)
	{
		for (int dy = -DrawDistance; dy <= DrawDistance; ++dy)
		{
			for (int cz = MinCz; cz <= MaxCz; ++cz)
			{
				Desired.Add(FIntVector(PlayerChunk.X + dx, PlayerChunk.Y + dy, cz));
			}
		}
	}

	// 卸载离开期望集的 chunk
	TArray<FIntVector> ToDespawn;
	for (auto& KV : Chunks)
	{
		if (!Desired.Contains(KV.Key)) ToDespawn.Add(KV.Key);
	}
	for (const FIntVector& Grid : ToDespawn)
	{
		DespawnChunk(Grid);
	}

	// 收集"未加载且期望"的 chunk，按距离 player 排序，每 tick 最多 spawn MaxSpawnsPerTick 个
	struct FPending
	{
		FIntVector Grid;
		float DistSq;
	};
	TArray<FPending> Pending;
	Pending.Reserve(Desired.Num());
	for (const FIntVector& Grid : Desired)
	{
		if (Chunks.Contains(Grid)) continue;
		// chunk 中心的世界坐标（用 cx, cy, cz 中点近似）
		const FVector ChunkCenter(
			(Grid.X + 0.5f) * Size * 100.f,
			(Grid.Y + 0.5f) * Size * 100.f,
			(Grid.Z + 0.5f) * Size * 100.f
		);
		Pending.Add({ Grid, static_cast<float>(FVector::DistSquared(ChunkCenter, PlayerLoc)) });
	}
	Pending.Sort([](const FPending& A, const FPending& B) { return A.DistSq < B.DistSq; });

	const int N = FMath::Min(MaxSpawnsPerTick, Pending.Num());
	for (int i = 0; i < N; ++i)
	{
		SpawnChunkAtGrid(Pending[i].Grid);
	}
}

void UChunkWorldSubsystem::SpawnChunkAtGrid(const FIntVector& ChunkGrid)
{
	UWorld* World = GetWorld();
	if (!World) return;

	const FIntVector OriginVoxel(ChunkGrid.X * Size, ChunkGrid.Y * Size, ChunkGrid.Z * Size);
	const FVector ChunkLocation = FVector(OriginVoxel) * 100.f;

	const FTransform Transform(FRotator::ZeroRotator, ChunkLocation, FVector::OneVector);

	AChunkBase* Chunk = World->SpawnActorDeferred<AChunkBase>(
		ChunkType,
		Transform,
		nullptr
	);
	if (!Chunk) return;

	Chunks.Emplace(ChunkGrid, Chunk);

	Chunk->Material = Material;
	Chunk->MaterialColor = MaterialColor;
	Chunk->Size = Size;
	Chunk->ChunkOriginVoxel = OriginVoxel;

	UGameplayStatics::FinishSpawningActor(Chunk, Transform);

	ChunkCount++;
}

void UChunkWorldSubsystem::DespawnChunk(const FIntVector& ChunkGrid)
{
	TObjectPtr<AChunkBase>* Found = Chunks.Find(ChunkGrid);
	if (!Found) return;
	if (*Found)
	{
		(*Found)->Destroy();
	}
	Chunks.Remove(ChunkGrid);
}

void UChunkWorldSubsystem::ModifyTargetVoxel(const FIntVector ChunkPosition, const FVector WorldPosition, EBlock Block)
{
	const FIntVector ChunkLocation = UVoxelFunctionLibrary::WorldToChunkPosition(WorldPosition, Size);

	if (!Chunks.Contains(ChunkLocation)) return;
	AChunkBase* TargetChunk = Chunks[ChunkLocation];

	TargetChunk->ModifyVoxel(ChunkPosition, Block);
}

EBlock UChunkWorldSubsystem::GetTargetVoxelType(const FVector WorldPosition)
{
	const FIntVector ChunkLocation = UVoxelFunctionLibrary::WorldToChunkPosition(WorldPosition, Size);

	if (!Chunks.Contains(ChunkLocation)) return EBlock::Null;
	AChunkBase* TargetChunk = Chunks[ChunkLocation];

	return TargetChunk->GetVoxel(UVoxelFunctionLibrary::WorldToLocalBlockPosition(WorldPosition, Size));
}

UChunkWorldSubsystem* UChunkWorldSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject) return nullptr;
	UGameInstance* GameInstance = WorldContextObject->GetWorld()->GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UChunkWorldSubsystem>() : nullptr;
}
