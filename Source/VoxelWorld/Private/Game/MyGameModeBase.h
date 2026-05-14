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

protected:
	virtual void BeginPlay() override;
};
