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
	TSubclassOf<AChunkBase> ChunkType;

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

	UFUNCTION(BlueprintCallable, Category = "Voxel")
	void ModifyTargetVoxel(const FIntVector ChunkPosition, const FVector WorldPosition, EBlock Block);

	TMap<FVector, TObjectPtr<AChunkBase>> Chunks;
	
protected:
	virtual void BeginPlay() override;

private:
	int ChunkCount;

	void Generate3DWorld();
	void Generate2DWorld();
};
