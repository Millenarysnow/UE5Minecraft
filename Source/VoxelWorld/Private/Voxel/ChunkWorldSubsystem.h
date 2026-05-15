#pragma once

#include <random>

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Utils/Enums.h"
#include "ChunkWorldSubsystem.generated.h"

class AChunkBase;

/**
 * 区块世界子系统：管理已 spawn 的 chunk 表 + 按 player 位置动态加载 / 卸载。
 *
 * 工作流：
 *   1. MyGameMode.BeginPlay → ChunkType / DrawDistance / Size / MinWorldY / MaxWorldY / Material 等设入
 *   2. MyGameMode.BeginPlay → StartStreaming() 注册周期 timer
 *   3. 周期回调 TickStreaming：
 *      - 找到 player pawn，算所在 chunk 网格坐标
 *      - 期望集 = (player ± DrawDistance)² × 垂直范围
 *      - 期望但未加载的 chunk → 按距离排序，每 tick 最多 spawn MaxSpawnsPerTick 个
 *      - 已加载但不在期望集的 chunk → 销毁
 *
 * 同步生成（每 chunk ~30ms）。MaxSpawnsPerTick=2 让单 tick 不超 70ms。
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
	int DrawDistance = 8;

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

	// 流式参数
	UPROPERTY()
	int MaxSpawnsPerTick = 2;

	UPROPERTY()
	float StreamingTickInterval = 0.1f;

	/// 启动流式加载：注册 timer，开始按 player 位置动态 spawn / despawn。
	/// 替代旧的 GenerateWorld()。MyGameMode 在 BeginPlay 调一次。
	UFUNCTION(BlueprintCallable, Category = "Voxel")
	void StartStreaming();

	/// 停止流式加载（清掉 timer）。可选：清空所有已加载 chunk。
	UFUNCTION(BlueprintCallable, Category = "Voxel")
	void StopStreaming(bool bDestroyAllChunks);

	UFUNCTION(BlueprintCallable, Category = "Voxel")
	void ModifyTargetVoxel(const FIntVector ChunkPosition, const FVector WorldPosition, EBlock Block);

	UFUNCTION(BlueprintCallable, Category = "Voxel")
	EBlock GetTargetVoxelType(const FVector WorldPosition);

	UFUNCTION(BlueprintCallable)
	static UChunkWorldSubsystem* Get(const UObject* WorldContextObject);

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	int ChunkCount = 0;
	TMap<FIntVector, TObjectPtr<AChunkBase>> Chunks; // 按区块格坐标 (cx, cy, cz) 索引

	FTimerHandle StreamingTimerHandle;

	void TickStreaming();
	void SpawnChunkAtGrid(const FIntVector& ChunkGrid);
	void DespawnChunk(const FIntVector& ChunkGrid);

	// 把 UE 世界坐标换成水平 chunk 网格坐标（cx, cy）。返回的 z 永远是 0。
	FIntVector PlayerWorldToChunkXY(const FVector& WorldLocation) const;
};
