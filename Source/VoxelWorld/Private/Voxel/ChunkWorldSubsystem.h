#pragma once

#include <random>

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Utils/Enums.h"
#include "ChunkWorldSubsystem.generated.h"

class AChunkBase;

/**
 * 区块世界子系统：管理已 spawn 的 chunk 表，负责按列布局生成。
 * y ∈ [-64, 320] 共 12 个 size=32 的立方区块，每个 (cx, cy) 列堆 12 个。
 */
UCLASS()
class UChunkWorldSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	std::default_random_engine RandomEngine;

	UPROPERTY()
	TSubclassOf<AChunkBase> ChunkType;

	UPROPERTY()
	int DrawDistance = 5;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> Material;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> MaterialColor;

	UPROPERTY()
	int Size = 32;

	// y 范围（方块坐标）。Mojang 默认 [-64, 320]，高 384。
	UPROPERTY()
	int MinWorldY = -64;

	UPROPERTY()
	int MaxWorldY = 320;

	UFUNCTION(BlueprintCallable, Category = "Voxel")
	void GenerateWorld();

	UFUNCTION(BlueprintCallable, Category = "Voxel")
	void ModifyTargetVoxel(const FIntVector ChunkPosition, const FVector WorldPosition, EBlock Block);

	UFUNCTION(BlueprintCallable, Category = "Voxel")
	EBlock GetTargetVoxelType(const FVector WorldPosition);

	UFUNCTION(BlueprintCallable)
	static UChunkWorldSubsystem* Get(const UObject* WorldContextObject);

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

private:
	int ChunkCount = 0;
	TMap<FIntVector, TObjectPtr<AChunkBase>> Chunks; // 按区块格坐标 (cx, cy, cz) 索引
};
