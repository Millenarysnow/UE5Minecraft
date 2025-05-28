#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChunkMeshData.h"

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

	UPROPERTY(EditDefaultsOnly, Category = "Chunk")
	float Frequency = 0.03f;

protected:
	virtual void BeginPlay() override;

	virtual void GenerateHeightMap();

	virtual void GenerateMesh();

	TObjectPtr<UProceduralMeshComponent> Mesh;
	FastNoiseLite* Noise;
	FChunkMeshData MeshData;
	int VertexCount = 0;

private:
	void ApplyMesh() const;
};


