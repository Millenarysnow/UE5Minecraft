// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/World/GenerateTreeSubsystem.h"

#include <random>

#include "Voxel/ChunkWorldSubsystem.h"
#include "Voxel/Chunk/ChunkBase.h"
#include "Voxel/Utils/bezier.h"
#include "Voxel/Utils/VoxelFunctionLibrary.h"

void UGenerateTreeSubsystem::GenerateTree(int X, int Y, int Z, FVector ChunkPosition, AChunkBase* Chunk, int Size)
{
	this->ChunkSize = Size;
	
	// 随机树的高度
	std::uniform_int_distribution<int> u(5,7);
	const int TreeHeight = u(UChunkWorldSubsystem::Get(GetWorld())->RandomEngine);

	// 生成树干
	for (int i = 0; i < TreeHeight; i++)
	{
		Chunk->ModifyVoxelData(FIntVector(X, Y, Z + i), EBlock::Wood);
	}

	// 填充树叶的贝塞尔曲线 高度->贝塞尔x, 半径->贝塞尔y
	float ControlPoint[4];
	ControlPoint[0] = 2; // 第一层树叶的下面一层
	ControlPoint[1] = (TreeHeight - 2 + 1) / 3.0f + 2;
	ControlPoint[2] = (TreeHeight - 2 + 1) / 3.0f * 2 + 2; 
	ControlPoint[3] = TreeHeight + 1; // 最顶层树叶的上面一层

	const bezier::Bezier<3> Bezier({
		{ControlPoint[0], 0.0f},
		{ControlPoint[1], 4.5f},
		{ControlPoint[2], 2.5f},
		{ControlPoint[3], 0.0f}
	});

	// 按层生成树叶
	for (int i = ControlPoint[0] + 1; i < ControlPoint[3]; i++)
	{
		// 二分 t ,找到对应 x 位置的 t
		float l = 0, r = 1;
		while (r - l > exp)
		{
			float mid = (l + r) / 2.0f;
			if (Bezier.valueAt(mid, 0) < i) l = mid;
			else r = mid;
		}
		
		// 按二分得到的 t 获取贝塞尔对应的y
		const float R = Bezier.valueAt(l, 1) * 100.0f;

		// 转换为世界坐标
		const FVector Center = UVoxelFunctionLibrary::LocalBlockToWorldPosition(FIntVector(X, Y, Z + i), ChunkPosition) + FVector(1, 1,1);

		// 填充当前层树叶
		DfsStuffLeaves(Center.X, Center.Y, Center.Z, R, Center.X, Center.Y);
	}
}

void UGenerateTreeSubsystem::DfsStuffLeaves(int X, int Y, int Z, float R, int CenterX, int CenterY)
{
	// 遍历水平四个方向
	for (int i = 0; i < 4; i++)
	{
		const float x = X + dx[i] * 100.0f;
		const float y = Y + dy[i] * 100.0f;

		// 如果大于半径就跳过
		if (UVoxelFunctionLibrary::Calculate2DDistance(x, y, CenterX, CenterY) > R) continue;

		// 只有位置为空才填充
		if (UChunkWorldSubsystem::Get(GetWorld())->GetTargetVoxelType(FVector(x, y, Z)) == EBlock::Air)
		{
			UChunkWorldSubsystem::Get(GetWorld())->ModifyTargetVoxel(
				UVoxelFunctionLibrary::WorldToLocalBlockPosition(FVector(x, y, Z), ChunkSize),
				FVector(x, y, Z),
				EBlock::Leaf
			);
			
			DfsStuffLeaves(x, y, Z, R, CenterX, CenterY);
		}
	}
}

UGenerateTreeSubsystem* UGenerateTreeSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject) return nullptr;
	UGameInstance* GameInstance = WorldContextObject->GetWorld()->GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UGenerateTreeSubsystem>() : nullptr;
}

