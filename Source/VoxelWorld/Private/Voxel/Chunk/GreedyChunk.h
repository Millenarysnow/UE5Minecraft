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

public:
	UFUNCTION()
	void ModifyTargetVoxel(const int& TargetIndex, const EBlock& Block);
	
	virtual EBlock GetVoxel(const FIntVector Position) const override;

protected:
	virtual void GenerateMesh() override;
	virtual void Setup() override;
	virtual void Generate2DHeightMap(const FVector Position) override;
	virtual void Generate3DHeightMap(const FVector Position) override;

	virtual void ModifyVoxelData(const FIntVector Position, EBlock Block) override;

private:
	const float exp = 1e-5;
	
	int dx[4] = { 0, 1, 0, -1};
	int dy[4] = { -1, 0, 1, 0};
	
	double Tree = 0.1;
	
	TArray<EBlock> Blocks;
	TArray<FVector> TreePoints;

	void CreateQuad(FMask Mask, FIntVector AxisMask, FIntVector V1, FIntVector V2, FIntVector V3, FIntVector V4, const int Width, const int Height);

	int GetBlockIndex(int X, int Y, int Z) const;

	EBlock GetBlock(FIntVector Index) const;

	bool CompareMask(FMask M1, FMask M2) const;

	int GetTextureIndex(EBlock Block, FVector Normal);

	// 生成树
	// 参数为树根位置
	void GenerateTree(int X, int Y, int Z);

	void DfsStuffLeaves(int X, int Y, int Z, float R, int CenterX, int CenterY);

	float Calculate2DDistance(float X1, float Y1, float X2, float Y2);
};
