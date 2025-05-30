// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

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
	
	UFUNCTION(BlueprintCallable, Category = "Voxel")
	void ModifyTargetVoxel(const FIntVector ChunkPosition, const FVector WorldPosition, EBlock Block);

	UFUNCTION(BlueprintCallable)
	static UChunkWorldSubsystem* Get(const UObject* WorldContextObject);

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

private:
	int ChunkCount;
	TMap<FVector, TObjectPtr<AChunkBase>> Chunks;

	void Generate3DWorld();
	void Generate2DWorld();
};


