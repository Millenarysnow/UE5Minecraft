// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enums.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VoxelFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class UVoxelFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/// 从世界位置获取所属的方块的位置（除以100）
	/// @param Position 世界位置
	/// @return 方块的位置
	UFUNCTION(BlueprintPure, Category = "Voxel")
	static FIntVector WorldToBlockPosition(const FVector& Position);

	/// 从世界位置获取所属的方块在区块中的位置
	/// @param Position 世界位置
	/// @param Size 区块大小
	/// @return 方块在区块中的位置
	UFUNCTION(BlueprintPure, Category = "Voxel")
	static FIntVector WorldToLocalBlockPosition(const FVector& Position, const int Size);

	/// 从世界位置获取当前点属于的区块的坐标（区块的相对坐标）
	/// @param Position 世界位置
	/// @param Size 区块大小
	/// @return 区块的世界坐标
	UFUNCTION(BlueprintPure, Category = "Voxel")
	static FIntVector WorldToChunkPosition(const FVector& Position, const int Size);

	UFUNCTION(BlueprintPure, Category = "Voxel")
	static FVector LocalBlockToWorldPosition(const FIntVector& LocalBlockPosition, const FVector& ChunkPosition);
};
