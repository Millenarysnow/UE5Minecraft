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

	/// 获取方块类型
	/// @param Position 方块的坐标（区块局部坐标）
	/// @return 方块类型
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
	
	double Tree = 0.1; // 生成树的概率
	
	TArray<EBlock> Blocks;
	TArray<FVector> TreePoints; // 生成树的位置

	void CreateQuad(FMask Mask, FIntVector AxisMask, FIntVector V1, FIntVector V2, FIntVector V3, FIntVector V4, const int Width, const int Height);

	int GetBlockIndex(int X, int Y, int Z) const;

	EBlock GetBlock(FIntVector Index) const;

	bool CompareMask(FMask M1, FMask M2) const;

	int GetTextureIndex(EBlock Block, FVector Normal);

	/// 生成树
	/// @param X 树根的X坐标（区块局部坐标）
	/// @param Y 树根的Y坐标（区块局部坐标）
	/// @param Z 树根的Z坐标（区块局部坐标）
	void GenerateTree(int X, int Y, int Z);

	/// DFS填充一层树叶
	/// @param X DFS中当前的X坐标（世界坐标）
	/// @param Y DFS中当前的Y坐标（世界坐标）
	/// @param Z DFS中当前的Z坐标（世界坐标）
	/// @param R 当前层树叶的半径
	/// @param CenterX 当前层中点的X坐标（世界坐标）
	/// @param CenterY 当前层中点的Y坐标（世界坐标）
	void DfsStuffLeaves(int X, int Y, int Z, float R, int CenterX, int CenterY);

	/// 计算两个点之间的距离 2D
	float Calculate2DDistance(float X1, float Y1, float X2, float Y2);
};
