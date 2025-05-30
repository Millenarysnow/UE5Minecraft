#pragma once

#include "CoreMinimal.h"
#include "ChunkBase.h"
#include "Voxel/Utils/Enums.h"
#include "GreedyChunk.generated.h"

class FastNoiseLite;
class UProceduralMeshComponent;

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

protected:
	virtual void GenerateMesh() override;
	virtual void Setup() override;
	virtual void Generate2DHeightMap(const FVector Position) override;
	virtual void Generate3DHeightMap(const FVector Position) override;

	virtual void ModifyVoxelData(const FIntVector Position, EBlock Block) override;

private:
	TArray<EBlock> Blocks;

	void CreateQuad(FMask Mask, FIntVector AxisMask, FIntVector V1, FIntVector V2, FIntVector V3, FIntVector V4, const int Width, const int Height);

	int GetBlockIndex(int X, int Y, int Z) const;

	EBlock GetBlock(FIntVector Index) const;

	bool CompareMask(FMask M1, FMask M2) const;

	int GetTextureIndex(EBlock Block, FVector Normal);
};
