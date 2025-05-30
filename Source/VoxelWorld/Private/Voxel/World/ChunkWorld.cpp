#include "ChunkWorld.h"
#include "Voxel/Chunk/ChunkBase.h"
#include "Voxel/Utils/VoxelFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"

AChunkWorld::AChunkWorld()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AChunkWorld::BeginPlay()
{
	Super::BeginPlay();

	switch (GenerationType)
	{
	case EGenerationType::GT_3D:
		Generate3DWorld();
		break;
	case EGenerationType::GT_2D:
		Generate2DWorld();
		break;
	default:
			throw std::exception("Invalid Generation Type");
	}

	UE_LOG(LogTemp, Warning, TEXT("%d Chunks Created"), ChunkCount);
}

void AChunkWorld::Generate3DWorld()
{
	for (int x = -DrawDistance; x <= DrawDistance; x++)
	{
		for (int y = -DrawDistance; y <= DrawDistance; ++y)
		{
			for (int z = -DrawDistance; z <= DrawDistance; ++z)
			{
				const auto ChunkLocation = FVector(x * Size * 100, y * Size * 100, z * Size * 100);
				
				auto transform = FTransform(
					FRotator::ZeroRotator,
					ChunkLocation,
					FVector::OneVector
				);
				
				const auto chunk = GetWorld()->SpawnActorDeferred<AChunkBase>(
					ChunkType,
					transform,
					this
				);

				chunk->GenerationType = EGenerationType::GT_3D;
				chunk->Frequency = Frequency;
				chunk->Material = Material;
				chunk->Size = Size;

				UGameplayStatics::FinishSpawningActor(chunk, transform);

				Chunks.Emplace(FVector(x, y, z), chunk);
				
				ChunkCount++;
			}
		}
	}
}

void AChunkWorld::Generate2DWorld()
{
	for (int x = -DrawDistance; x <= DrawDistance; x++)
	{
		for (int y = -DrawDistance; y <= DrawDistance; ++y)
		{
			const auto ChunkLocation = FVector(x * Size * 100, y * Size * 100, 0);
			
			auto transform = FTransform(
				FRotator::ZeroRotator,
				ChunkLocation,
				FVector::OneVector
			);
				
			const auto chunk = GetWorld()->SpawnActorDeferred<AChunkBase>(
				ChunkType,
				transform,
				this
			);

			chunk->GenerationType = EGenerationType::GT_2D;
			chunk->Frequency = Frequency;
			chunk->Material = Material;
			chunk->Size = Size;

			UGameplayStatics::FinishSpawningActor(chunk, transform);

			Chunks.Emplace(FVector(x, y, 0), chunk);

			ChunkCount++;
		}
	}
}

void AChunkWorld::ModifyTargetVoxel(const FIntVector ChunkPosition, const FVector WorldPosition, EBlock Block)
{
	const FVector ChunkLocation = (FVector)UVoxelFunctionLibrary::WorldToChunkPosition((FVector)WorldPosition, Size);

	UE_LOG(LogTemp, Warning, TEXT("Modifying Voxel %s"), *ChunkLocation.ToString());
	
	if (!Chunks.Contains(ChunkLocation)) return;
	AChunkBase* TargetChunk = Chunks[ChunkLocation];

	TargetChunk->ModifyVoxel(ChunkPosition, Block);
}
