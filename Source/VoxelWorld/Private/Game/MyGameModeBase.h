// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Voxel/Utils/Enums.h"
#include "MyGameModeBase.generated.h"

class AChunkBase;

/**
 * 
 */
UCLASS()
class AMyGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TSubclassOf<AChunkBase> ChunkType;

	UPROPERTY(EditAnywhere)
	int DrawDistance = 5;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere)
	int Size = 32;

	UPROPERTY(EditAnywhere)
	EGenerationType GenerationType = EGenerationType::GT_2D;

	UPROPERTY(EditAnywhere)
	float Frequency = 0.03f;

protected:
	virtual void BeginPlay() override;
};
