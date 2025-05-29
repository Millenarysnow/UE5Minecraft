#pragma once

#include "CoreMinimal.h"
#include "ChunkBase.h"
#include "Voxel/Utils/Enums.h"
#include "NaiveChunk.generated.h"


class FastNoiseLite;
class UProceduralMeshComponent;


UCLASS()
class ANaiveChunk : public AChunkBase
{
	GENERATED_BODY()

protected:
	virtual void Setup() override;
	virtual void Generate2DHeightMap(const FVector Position) override;
	virtual void Generate3DHeightMap(const FVector Position) override;
	virtual void GenerateMesh() override;

	virtual void ModifyVoxelData(const FIntVector Position, EBlock Block) override;

private:
	TArray<EBlock> Blocks;
	
	// 立方体的顶点表
	const FVector BlockVertexData[8] = {
		FVector(100,100,100),
		FVector(100,0,100),
		FVector(100,0,0),
		FVector(100,100,0),
		FVector(0,0,100),
		FVector(0,100,100),
		FVector(0,100,0),
		FVector(0,0,0)
	};

	// 立方体的面表
	// 存储的是每个面的顶点数据（索引）,与BlockVertexData必须对应使用
	// 比如Forward面，0123代表着前面的顶点数据在BlockVertexData中的下标为0123
	const int BlockTriangleData[24] = {
		0,1,2,3, // Forward
		5,0,3,6, // Right
		4,5,6,7, // Back
		1,4,7,2, // Left
		5,4,1,0, // Up
		3,2,7,6  // Down
	};
	
	// 检查给定位置是否为空气方块（即透明方块）
	// 如果位置为透明方块，那么就需要绘制
	// 在位置超过当前 Chunk 范围的情况下也会返回 True
	bool Check(FVector Position) const;

	// 创建立方体的面（初始化网格体）
	void CreateFace(EDirection Direction, FVector Position);

	// 查询方块顶点数据以获取四个顶点
	TArray<FVector> GetFaceVertices(EDirection Direction, FVector Position) const;

	FVector GetPositionInDirection(EDirection Direction, FVector Position) const;

	int GetBlockIndex(int X, int Y, int Z) const;
	
	FVector GetNormal(EDirection Direction);
};
