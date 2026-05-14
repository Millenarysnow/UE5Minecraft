#include "Voxel/ChunkWorldSubsystem.h"

#include "Chunk/ChunkBase.h"
#include "Kismet/GameplayStatics.h"
#include "Utils/VoxelFunctionLibrary.h"

void UChunkWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	RandomEngine.seed(static_cast<unsigned>(std::time(0)));
}

void UChunkWorldSubsystem::GenerateWorld()
{
	if (!ChunkType)
	{
		UE_LOG(LogTemp, Error, TEXT("ChunkWorldSubsystem: ChunkType is null, abort GenerateWorld"));
		return;
	}

	// 区块在 z 轴上的覆盖范围（向 0 取整除）
	const int MinChunkZ = FMath::FloorToInt(static_cast<float>(MinWorldY) / Size);
	const int MaxChunkZ = FMath::FloorToInt(static_cast<float>(MaxWorldY - 1) / Size);

	for (int cx = -DrawDistance; cx <= DrawDistance; cx++)
	{
		for (int cy = -DrawDistance; cy <= DrawDistance; cy++)
		{
			for (int cz = MinChunkZ; cz <= MaxChunkZ; cz++)
			{
				const FIntVector ChunkGrid(cx, cy, cz);
				const FIntVector OriginVoxel(cx * Size, cy * Size, cz * Size);
				const FVector ChunkLocation = FVector(OriginVoxel) * 100.f;

				const FTransform Transform(FRotator::ZeroRotator, ChunkLocation, FVector::OneVector);

				AChunkBase* Chunk = GetWorld()->SpawnActorDeferred<AChunkBase>(
					ChunkType,
					Transform,
					nullptr
				);

				Chunks.Emplace(ChunkGrid, Chunk);

				Chunk->Material = Material;
				Chunk->MaterialColor = MaterialColor;
				Chunk->Size = Size;
				Chunk->ChunkOriginVoxel = OriginVoxel;

				UGameplayStatics::FinishSpawningActor(Chunk, Transform);

				ChunkCount++;
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("%d Chunks Created"), ChunkCount);
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
