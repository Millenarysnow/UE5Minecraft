#pragma once

#include "CoreMinimal.h"
#include "Voxel/Utils/Enums.h"
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
	TObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditInstanceOnly, Category = "ChunkWorld")
	int Size = 32;

	UPROPERTY(EditInstanceOnly, Category = "ChunkWorld")
	EGenerationType GenerationType;

	UPROPERTY(EditInstanceOnly, Category = "ChunkWorld")
	float Frequency = 0.03f;
	
	/*
	// 注意必须与Chunk中相同
	UPROPERTY(EditAnywhere, Category = "ChunkWorld")
	int ChunkSize = 32; 
	*/
	
protected:
	virtual void BeginPlay() override;

private:
	int ChunkCount;

	void Generate3DWorld();
	void Generate2DWorld();
};
