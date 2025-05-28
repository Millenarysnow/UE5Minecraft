#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChunkWorld.generated.h"

class AChunkBase;

UCLASS()
class AChunkWorld : public AActor
{
	GENERATED_BODY()
	
public:	
	AChunkWorld();

	UPROPERTY(EditAnywhere, Category = "ChunkWorld")
	TSubclassOf<AChunkBase> Chunk;

	UPROPERTY(EditAnywhere, Category = "ChunkWorld")
	int DrawDistance = 5;

	UPROPERTY(EditInstanceOnly, Category = "ChunkWorld")
	bool Draw3D = false;
	
	/*
	// 注意必须与Chunk中相同
	UPROPERTY(EditAnywhere, Category = "ChunkWorld")
	int ChunkSize = 32; 
	*/
	
protected:
	virtual void BeginPlay() override;

};
