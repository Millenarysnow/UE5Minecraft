// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/ChunkWorldSubsystem.h"

#include "Chunk/ChunkBase.h"
#include "Kismet/GameplayStatics.h"
#include "Utils/VoxelFunctionLibrary.h"

void UChunkWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	RandomEngine.seed(std::time(0));
}

void UChunkWorldSubsystem::GenerateWorld()
{
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

void UChunkWorldSubsystem::ModifyTargetVoxel(const FIntVector ChunkPosition, const FVector WorldPosition, EBlock Block)
{
	const FIntVector ChunkLocation = UVoxelFunctionLibrary::WorldToChunkPosition((FVector)WorldPosition, Size);

	//UE_LOG(LogTemp, Warning, TEXT("Modifying Voxel %s"), *ChunkLocation.ToString());
	
	if (!Chunks.Contains(ChunkLocation)) return;
	AChunkBase* TargetChunk = Chunks[ChunkLocation];

	TargetChunk->ModifyVoxel(ChunkPosition, Block);
}

EBlock UChunkWorldSubsystem::GetTargetVoxelType(const FVector WorldPosition)
{
	//UE_LOG(LogTemp, Warning, TEXT("Getting Voxel WorldPoint : %s"), *WorldPosition.ToString())
	
	const FIntVector ChunkLocation = UVoxelFunctionLibrary::WorldToChunkPosition((FVector)WorldPosition, Size);

	//UE_LOG(LogTemp, Warning, TEXT("Getting Voxel ChunkWorld : %s"), *ChunkLocation.ToString())
	
	if (!Chunks.Contains(ChunkLocation)) return EBlock::Null;
	AChunkBase* TargetChunk = Chunks[ChunkLocation];

	//UE_LOG(LogTemp, Warning, TEXT("Getting Voxel Comp"));

	return TargetChunk->GetVoxel(UVoxelFunctionLibrary::WorldToLocalBlockPosition(WorldPosition, Size));
}

UChunkWorldSubsystem* UChunkWorldSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject) return nullptr;
	UGameInstance* GameInstance = WorldContextObject->GetWorld()->GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UChunkWorldSubsystem>() : nullptr;
}

void UChunkWorldSubsystem::Generate3DWorld()
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
					nullptr
				);

				chunk->GenerationType = EGenerationType::GT_3D;
				chunk->Frequency = Frequency;
				chunk->Material = Material;
				chunk->Size = Size;
				chunk->ChunkPosition = ChunkLocation;

				UGameplayStatics::FinishSpawningActor(chunk, transform);

				Chunks.Emplace(FVector(x, y, z), chunk);
				
				ChunkCount++;
			}
		}
	}
}

void UChunkWorldSubsystem::Generate2DWorld()
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
				nullptr
			);

			Chunks.Emplace(FVector(x, y, 0), chunk);

			chunk->GenerationType = EGenerationType::GT_2D;
			chunk->Frequency = Frequency;
			chunk->Material = Material;
			chunk->Size = Size;
			chunk->ChunkPosition = ChunkLocation;

			UGameplayStatics::FinishSpawningActor(chunk, transform);

			ChunkCount++;
		}
	}
}
