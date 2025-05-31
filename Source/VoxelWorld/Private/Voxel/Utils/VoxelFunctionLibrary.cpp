// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Utils/VoxelFunctionLibrary.h"

FIntVector UVoxelFunctionLibrary::WorldToBlockPosition(const FVector& Position)
{
	return FIntVector(Position) / 100;
}

FIntVector UVoxelFunctionLibrary::WorldToLocalBlockPosition(const FVector& Position, const int Size)
{
	const auto ChunkPosition = WorldToChunkPosition(Position, Size);

	// 将方块位置从世界坐标系转至区块坐标系
	auto Result = WorldToBlockPosition(Position) - ChunkPosition * Size;
	
	// 负值的归一化
	if (ChunkPosition.X < 0) Result.X--;
	if (ChunkPosition.Y < 0) Result.Y--;
	if (ChunkPosition.Z < 0) Result.Z--;

	return Result;
}

FIntVector UVoxelFunctionLibrary::WorldToChunkPosition(const FVector& Position, const int Size)
{
	FIntVector Result;

	// Factor 是除数，乘100因为每个方块大小为100
	const int Factor = Size * 100;
	const auto IntPosition = FIntVector(Position);

	// 处理负值情况
	if (IntPosition.X < 0) Result.X = (int)(Position.X / Factor) - 1;
	else Result.X = (int)(Position.X / Factor);

	if (IntPosition.Y < 0) Result.Y = (int)(Position.Y / Factor) - 1;
	else Result.Y = (int)(Position.Y / Factor);

	if (IntPosition.Z < 0) Result.Z = (int)(Position.Z / Factor) - 1;
	else Result.Z = (int)(Position.Z / Factor);

	return Result;
}

FVector UVoxelFunctionLibrary::LocalBlockToWorldPosition(const FIntVector& LocalBlockPosition, const FVector& ChunkPosition)
{
	return FVector(LocalBlockPosition) * 100 + ChunkPosition;
}
