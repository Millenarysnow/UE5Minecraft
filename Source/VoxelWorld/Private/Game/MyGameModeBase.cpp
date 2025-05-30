// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/MyGameModeBase.h"

#include "Voxel/ChunkWorldSubsystem.h"

void AMyGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	UChunkWorldSubsystem* ChunkWorldSubsystem = UChunkWorldSubsystem::Get(GetWorld());

	ChunkWorldSubsystem->ChunkType = this->ChunkType;
	ChunkWorldSubsystem->DrawDistance = this->DrawDistance;
	ChunkWorldSubsystem->Frequency = this->Frequency;
	ChunkWorldSubsystem->GenerationType = this->GenerationType;
	ChunkWorldSubsystem->Material = this->Material;
	ChunkWorldSubsystem->Size = this->Size;

	ChunkWorldSubsystem->GenerateWorld();
}
