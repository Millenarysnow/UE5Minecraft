// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GenerateTreeSubsystem.generated.h"

class AChunkBase;
/**
 * 
 */
UCLASS()
class UGenerateTreeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/// 生成树
	/// @param X 树根的X坐标（区块局部坐标）
	/// @param Y 树根的Y坐标（区块局部坐标）
	/// @param Z 树根的Z坐标（区块局部坐标）
	void GenerateTree(int X, int Y, int Z, FVector ChunkPosition, AChunkBase* Chunk, int Size);

	/// DFS填充一层树叶
	/// @param X DFS中当前的X坐标（世界坐标）
	/// @param Y DFS中当前的Y坐标（世界坐标）
	/// @param Z DFS中当前的Z坐标（世界坐标）
	/// @param R 当前层树叶的半径
	/// @param CenterX 当前层中点的X坐标（世界坐标）
	/// @param CenterY 当前层中点的Y坐标（世界坐标）
	void DfsStuffLeaves(int X, int Y, int Z, float R, int CenterX, int CenterY);

	UFUNCTION(BlueprintCallable)
	static UGenerateTreeSubsystem* Get(const UObject* WorldContextObject);

private:
	int ChunkSize = 0;
	
	const float exp = 1e-5;
	
	int dx[4] = { 0, 1, 0, -1};
	int dy[4] = { -1, 0, 1, 0};
};
