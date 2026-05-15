// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/MyGameModeBase.h"

#include "Voxel/ChunkWorldSubsystem.h"
#include "Voxel/Generation/WorldGenerator.h"

void AMyGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	// WorldSeed 约定：0 → 用当前系统时间生成随机种子；非 0 → 显式种子（用于可重现）。
	// 实际使用的种子会打印到日志，方便复现一个看着不错的世界。
	int64 EffectiveSeed = WorldSeed;
	if (EffectiveSeed == 0)
	{
		EffectiveSeed = static_cast<int64>(FDateTime::Now().GetTicks());
		UE_LOG(LogTemp, Warning, TEXT("[WorldGen] WorldSeed=0 → auto seed: %lld"), EffectiveSeed);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorldGen] Using explicit WorldSeed: %lld"), WorldSeed);
	}

	UWorldGenerator* WorldGen = UWorldGenerator::Get(GetWorld());
	if (WorldGen)
	{
		WorldGen->WorldSeed = EffectiveSeed;
		WorldGen->bEnableCaves = bEnableCaves;
		WorldGen->bDebugBiomeColors = bDebugBiomeColors;
	}

	UChunkWorldSubsystem* ChunkWorldSubsystem = UChunkWorldSubsystem::Get(GetWorld());
	if (!ChunkWorldSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("ChunkWorldSubsystem unavailable"));
		return;
	}

	ChunkWorldSubsystem->ChunkType = ChunkType;
	ChunkWorldSubsystem->DrawDistance = DrawDistance;
	ChunkWorldSubsystem->Material = Material;
	ChunkWorldSubsystem->MaterialColor = MaterialColor;
	ChunkWorldSubsystem->Size = Size;
	ChunkWorldSubsystem->MinWorldY = MinWorldY;
	ChunkWorldSubsystem->MaxWorldY = MaxWorldY;

	ChunkWorldSubsystem->GenerateWorld();
}
