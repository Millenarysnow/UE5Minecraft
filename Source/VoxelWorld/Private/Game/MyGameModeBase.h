// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MyGameModeBase.generated.h"

class AChunkBase;

UCLASS()
class AMyGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "World")
	TSubclassOf<AChunkBase> ChunkType;

	UPROPERTY(EditAnywhere, Category = "World")
	int DrawDistance = 5;

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

	UPROPERTY(EditAnywhere, Category = "World")
	int64 WorldSeed = 0;

	// 调试：开启后表层方块按 biome 着色（top + 几层全替换为该 biome 的"标记块"），
	// 便于从空中肉眼验证 biome 分布。正常游玩时关掉。
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDebugBiomeColors = false;

protected:
	virtual void BeginPlay() override;
};
