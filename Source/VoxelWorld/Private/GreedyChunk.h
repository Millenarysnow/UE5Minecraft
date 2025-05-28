#pragma once

#include "CoreMinimal.h"
#include "ChunkBase.h"
#include "ChunkMeshData.h"
#include "GameFramework/Actor.h"
#include "GreedyChunk.generated.h"

class FastNoiseLite;
class UProceduralMeshComponent;
enum class EBlock;
enum class EDirection;

UCLASS()
class AGreedyChunk : public AChunkBase
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

	/*
	UPROPERTY(EditAnywhere, Category = "Chunk")
	FIntVector Size = FIntVector(1, 1, 1) * 32;
	*/
	
protected:
	virtual void BeginPlay() override;

	virtual void GenerateHeightMap() override;

	virtual void GenerateMesh() override;

private:
	TArray<EBlock> Blocks;

	void CreateQuad(FMask Mask, FIntVector AxisMask, FIntVector V1, FIntVector V2, FIntVector V3, FIntVector V4);

	int GetBlockIndex(int X, int Y, int Z) const;

	EBlock GetBlock(FIntVector Index) const;

	bool CompareMask(FMask M1, FMask M2) const;
};
