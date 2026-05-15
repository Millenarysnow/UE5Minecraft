#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Voxel/Generation/BiomeSource.h"
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

	// 调试：开启后表层方块按 biome 着色。由 MyGameMode 在 BeginPlay 设置。
	bool bDebugBiomeColors = false;

	/// 填充一个 chunk 的体素数据。ChunkSize^3 个方块。
	void FillChunk(const FIntVector& ChunkOriginWorldVoxel, int ChunkSize, TArray<EBlock>& OutBlocks);

	UFUNCTION(BlueprintCallable)
	static UWorldGenerator* Get(const UObject* WorldContextObject);

	void LogPhase1SmokeTest() const;
	void LogPhase2SmokeTest();

private:
	void EnsureRouter();

	TUniquePtr<MCWorldGen::FNoiseRouter> Router;
	TUniquePtr<MCWorldGen::FSurfaceSystem> SurfaceSystem;
	TUniquePtr<MCWorldGen::FBiomeSource> BiomeSource;
	int64 RouterSeed = 0;
	bool bSmokeTested = false;
	bool bPhase2Logged = false;
};
