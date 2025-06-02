#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Voxel/Utils/ChunkMeshData.h"
#include "Voxel/Utils/Enums.h"

#include "ChunkBase.generated.h"

class FastNoiseLite;
class UProceduralMeshComponent;

UCLASS(Abstract)
class AChunkBase : public AActor
{
	GENERATED_BODY()

public:
	AChunkBase();

	UPROPERTY(EditDefaultsOnly, Category = "Chunk")
	int Size = 64;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> Material;

	FVector ChunkPosition = FVector::ZeroVector;

	float Frequency = 0.03f;

	EGenerationType GenerationType;

	UFUNCTION(BlueprintCallable, Category = "Chunk")
	void ModifyVoxel(const FIntVector Position, EBlock Block);
	
	virtual EBlock GetVoxel(const FIntVector Position) const {return EBlock::Null;}

	virtual void ModifyVoxelData(const FIntVector Position, EBlock Block) PURE_VIRTUAL(AChunkBase::ModifyVoxelData);

protected:
	virtual void BeginPlay() override;
	virtual void Setup() PURE_VIRTUAL(AChunkBase::Setup);
	virtual void Generate2DHeightMap(const FVector Position) PURE_VIRTUAL(AChunkBase::Generate2DHeightMap);
	virtual void Generate3DHeightMap(const FVector Position) PURE_VIRTUAL(AChunkBase::Generate3DHeightMap);
	virtual void GenerateMesh() PURE_VIRTUAL(AChunkBase::GenerateMesh);

	TObjectPtr<UProceduralMeshComponent> Mesh;
	FastNoiseLite* Noise;
	FChunkMeshData MeshData;
	int VertexCount = 0;

private:
	void ApplyMesh() const;
	void ClearMesh();
	virtual void GenerateHeightMap();
};


