// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/MyGameModeBase.h"

#include "Voxel/ChunkWorldSubsystem.h"
#include "Voxel/Generation/WorldGenerator.h"

void AMyGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	UWorldGenerator* WorldGen = UWorldGenerator::Get(GetWorld());
	if (WorldGen)
	{
		WorldGen->WorldSeed = WorldSeed;
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
