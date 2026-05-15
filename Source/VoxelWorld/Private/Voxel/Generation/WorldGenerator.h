#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Voxel/Generation/BiomeSource.h"
#include "Voxel/Generation/FeaturePlacer.h"
#include "Voxel/Generation/NoiseRouter.h"
#include "Voxel/Generation/SurfaceSystem.h"
#include "Voxel/Utils/Enums.h"
#include "WorldGenerator.generated.h"

/**
 * 世界生成入口。
 *
 * Phase 2：FillChunk 用 NoiseRouter 评估 final_density 决定每个方块是 stone / water / air。
 *   - 海平面 y=63
 *   - 基岩 y=-64
 *   - density > 0 → stone（y==-64 时为 bedrock）
 *   - density ≤ 0 且 y ≤ 63 → water
 *   - 其余 → air
 *
 * 表层（grass/dirt/sand/snow）、生物群系、洞穴、矿、树留给后续 Phase。
 */
UCLASS()
class UWorldGenerator : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "WorldGen")
	int64 WorldSeed = 0;

	// 由 MyGameMode 传入。
	bool bEnableCaves = true;

	// 由 MyGameMode 传入。整体抬升 offset，让 plains 浮出水面（默认 0 = Mojang 真值）。
	double ContinentBias = 0.0;

	// 调试：开启后表层方块按 biome 着色。由 MyGameMode 在 BeginPlay 设置。
	bool bDebugBiomeColors = false;

	/// 填充一个 chunk 的体素数据。ChunkSize^3 个方块。
	void FillChunk(const FIntVector& ChunkOriginWorldVoxel, int ChunkSize, TArray<EBlock>& OutBlocks);

	/// 单点查询：(WorldVoxel) 处的方块是不是 opaque（用于 chunk 边界面剔除）。
	/// 仅判断 stone-or-not + bedrock，不跑完整 surface / cave-water 流程，专为 GreedyChunk OOB 邻居查询用。
	bool IsBlockSolidAt(const FIntVector& WorldVoxel);

	UFUNCTION(BlueprintCallable)
	static UWorldGenerator* Get(const UObject* WorldContextObject);

	void LogPhase1SmokeTest() const;
	void LogPhase2SmokeTest();

private:
	void EnsureRouter();

	TUniquePtr<MCWorldGen::FNoiseRouter> Router;
	TUniquePtr<MCWorldGen::FSurfaceSystem> SurfaceSystem;
	TUniquePtr<MCWorldGen::FBiomeSource> BiomeSource;
	TUniquePtr<MCWorldGen::FFeaturePlacer> FeaturePlacer;
	int64 RouterSeed = 0;
	bool bSmokeTested = false;
	bool bPhase2Logged = false;
};
