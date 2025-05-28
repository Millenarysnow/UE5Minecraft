#include "ChunkWorld.h"
#include "ChunkBase.h"

AChunkWorld::AChunkWorld()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AChunkWorld::BeginPlay()
{
	Super::BeginPlay();

	const int Size = Chunk.GetDefaultObject()->Size;
	int ChunkCount = 0;

	if (Draw3D)
	{
		for (int x = -DrawDistance; x <= DrawDistance; x++)
		{
			for (int y = -DrawDistance; y <= DrawDistance; ++y)
			{
				for (int z = -DrawDistance; z <= DrawDistance; ++z)
				{
					GetWorld()->SpawnActor<AChunkBase>(
						Chunk,
						FVector(x * Size * 100, y * Size * 100, z * Size * 100),
						FRotator::ZeroRotator
					);

					ChunkCount++;
				}
			}
		}
	}
	else
	{
		for (int x = -DrawDistance; x <= DrawDistance; x++)
		{
			for (int y = -DrawDistance; y <= DrawDistance; ++y)
			{
				GetWorld()->SpawnActor<AChunkBase>(
					Chunk,
					FVector(x * Size * 100, y * Size * 100, 0),
					FRotator::ZeroRotator
				);

				ChunkCount++;
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("%d Chunks Created"), ChunkCount);
}
