#pragma once

#include "CoreMinimal.h"
#include "ChunkMeshData.h"
#include "GameFramework/Actor.h"
#include "GreedyChunk.generated.h"

class FastNoiseLite;
class UProceduralMeshComponent;
enum class EBlock;

UCLASS()
class AGreedyChunk : public AActor
{
	GENERATED_BODY()

	// 用于合并的掩码
	struct FMask
	{
		EBlock Block; // 方块类型
		int Normal; // 法线方向
	};
	
public:	
	AGreedyChunk();

	UPROPERTY(EditAnywhere, Category = "Chunk")
	FIntVector Size = FIntVector(1, 1, 1) * 32;
	
protected:
	virtual void BeginPlay() override;

private:
	TObjectPtr<UProceduralMeshComponent> Mesh;
	TObjectPtr<FastNoiseLite> Noise;

	FChunkMeshData MeshData;
	TArray<EBlock> Blocks;

	int VertexCount = 0;

	void GenerateBlocks();

	void ApplyMesh();

	void GenerateMesh();

	void CreateQuad(FMask Mask, FIntVector AxisMask, FIntVector V1, FIntVector V2, FIntVector V3, FIntVector V4);

	int GetBlockIndex(int X, int Y, int Z) const;

	EBlock GetBlock(FIntVector Index) const;

	bool CompareMask(FMask M1, FMask M2) const;
};
