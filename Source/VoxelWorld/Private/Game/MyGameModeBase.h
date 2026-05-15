// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MyGameModeBase.generated.h"

class AChunkBase;
class ACloudLayer;
class UMaterialInterface;

UCLASS()
class AMyGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "World")
	TSubclassOf<AChunkBase> ChunkType;

	UPROPERTY(EditAnywhere, Category = "World")
	int DrawDistance = 8;

	UPROPERTY(EditAnywhere, Category = "World")
	TObjectPtr<UMaterialInterface> Material;

	// 纯色材质：未制作贴图的方块从这里渲染。建议是一个把 VertexColor.RGB 接到 BaseColor 的简单材质。
	UPROPERTY(EditAnywhere, Category = "World")
	TObjectPtr<UMaterialInterface> MaterialColor;

	UPROPERTY(EditAnywhere, Category = "World")
	int Size = 32;

	UPROPERTY(EditAnywhere, Category = "World")
	int MinWorldY = -64;

	UPROPERTY(EditAnywhere, Category = "World")
	int MaxWorldY = 320;

	// 流式加载：每个 tick 最多 spawn 几个 chunk（控制单 tick 卡顿；越大开局越快但单 tick 越久）
	UPROPERTY(EditAnywhere, Category = "World", meta = (ClampMin = "1", ClampMax = "16"))
	int MaxSpawnsPerTick = 2;

	// 流式加载 tick 间隔（秒）
	UPROPERTY(EditAnywhere, Category = "World", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float StreamingTickInterval = 0.1f;

	UPROPERTY(EditAnywhere, Category = "World")
	int64 WorldSeed = 0;

	// 是否生成 cheese 洞穴。关闭后地下不再有空腔（生成更快，便于 biome 验证）。
	UPROPERTY(EditAnywhere, Category = "World")
	bool bEnableCaves = true;

	// 整体抬升 offset，让大部分 plains 浮出海平面。0 = Mojang 真值（plains surface 多在 sea level 附近，大部分 underwater）。
	// 推荐 0.1 让 plains surface 抬到 ~y=77，明显成"陆地"。
	UPROPERTY(EditAnywhere, Category = "World", meta = (ClampMin = "-0.3", ClampMax = "0.3"))
	float ContinentBias = 0.1f;

	// 调试：开启后表层方块按 biome 着色（top + 几层全替换为该 biome 的"标记块"），
	// 便于从空中肉眼验证 biome 分布。正常游玩时关掉。
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDebugBiomeColors = false;

	// 是否自动 spawn 云层。
	UPROPERTY(EditAnywhere, Category = "Cloud")
	bool bSpawnClouds = true;

	// 云层材质（半透明白色，蓝图自建 M_Cloud：Translucent + Base Color = (1,1,1) + Opacity = 0.7）。
	UPROPERTY(EditAnywhere, Category = "Cloud")
	TObjectPtr<UMaterialInterface> CloudMaterial;

	// 云层 Y 高度（块）。
	UPROPERTY(EditAnywhere, Category = "Cloud")
	int CloudHeight = 192;

	// 风速（块/秒）。
	UPROPERTY(EditAnywhere, Category = "Cloud")
	float CloudWindSpeedBlocksPerSec = 0.6f;

protected:
	virtual void BeginPlay() override;
};
