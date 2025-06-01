// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <random>

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Utils/Enums.h"
#include "ChunkWorldSubsystem.generated.h"

class AChunkBase;

/**
 * 
 */
UCLASS()
class UChunkWorldSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	std::default_random_engine RandomEngine; // C++随机引擎
	
	UPROPERTY()
	TSubclassOf<AChunkBase> ChunkType;

	UPROPERTY()
	int DrawDistance = 5;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> Material;

	UPROPERTY()
	int Size = 32;

	UPROPERTY()
	EGenerationType GenerationType = EGenerationType::GT_2D;

	UPROPERTY()
	float Frequency = 0.03f;

	UFUNCTION(BlueprintCallable, Category = "Voxel")
	void GenerateWorld();

	/// 修改指定位置的方块
	/// @param ChunkPosition 待修改位置的区块局部坐标
	/// @param WorldPosition 待修改位置的世界坐标
	/// @param Block 目标方块类型
	UFUNCTION(BlueprintCallable, Category = "Voxel")
	void ModifyTargetVoxel(const FIntVector ChunkPosition, const FVector WorldPosition, EBlock Block);

	/// 获取指定位置的方块类型
	/// @param WorldPosition 待获取位置的世界坐标
	/// @return 目标位置的方块类型
	UFUNCTION(BlueprintCallable, Category = "Voxel")
	EBlock GetTargetVoxelType(const FVector WorldPosition);

	UFUNCTION(BlueprintCallable)
	static UChunkWorldSubsystem* Get(const UObject* WorldContextObject);

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

private:
	int ChunkCount;
	TMap<FIntVector, TObjectPtr<AChunkBase>> Chunks; // 区块列表

	void Generate3DWorld();
	void Generate2DWorld();
};


