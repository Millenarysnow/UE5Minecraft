#pragma once

#include "CoreMinimal.h"
#include "ChunkBase.h"
#include "Voxel/Utils/Enums.h"
#include "GreedyChunk.generated.h"

UCLASS()
class AGreedyChunk : public AChunkBase
{
	GENERATED_BODY()

	struct FMask
	{
		EBlock Block;
		int Normal;
	};

public:
	UFUNCTION()
	void ModifyTargetVoxel(const int& TargetIndex, const EBlock& Block);

	virtual EBlock GetVoxel(const FIntVector Position) const override;

	virtual void ModifyVoxelData(const FIntVector Position, EBlock Block) override;

protected:
	virtual void GenerateVoxelData() override;
	virtual void GenerateMesh() override;

private:
	TArray<EBlock> Blocks;

	void CreateQuad(FMask Mask, FIntVector AxisMask, FIntVector V1, FIntVector V2, FIntVector V3, FIntVector V4, const int Width, const int Height,
		FChunkMeshData& Buffer, int& Count);

	int GetBlockIndex(int X, int Y, int Z) const;

	EBlock GetBlock(FIntVector Index) const;

	bool CompareMask(FMask M1, FMask M2) const;

	int GetTextureIndex(EBlock Block, FVector Normal);
};
