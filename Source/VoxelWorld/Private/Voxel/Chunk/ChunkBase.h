#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Voxel/Utils/ChunkMeshData.h"
#include "Voxel/Utils/Enums.h"

#include "ChunkBase.generated.h"

class UProceduralMeshComponent;

UCLASS(Abstract)
class AChunkBase : public AActor
{
	GENERATED_BODY()

public:
	AChunkBase();

	UPROPERTY(EditDefaultsOnly, Category = "Chunk")
	int Size = 32;

	// 贴图材质（采样 voxel texture array）。供 Grass/Dirt/Stone/Wood/Leaf 等已有贴图的方块使用。
	UPROPERTY()
	TObjectPtr<UMaterialInterface> Material;

	// 纯色材质（VertexColor.RGB → BaseColor）。供尚未制作贴图的方块使用。
	UPROPERTY()
	TObjectPtr<UMaterialInterface> MaterialColor;

	// 区块原点（区块 (0,0,0) 方块对应的世界方块坐标）。世界坐标 = ChunkOriginVoxel * 100。
	FIntVector ChunkOriginVoxel = FIntVector::ZeroValue;

	UFUNCTION(BlueprintCallable, Category = "Chunk")
	void ModifyVoxel(const FIntVector Position, EBlock Block);

	virtual EBlock GetVoxel(const FIntVector Position) const { return EBlock::Null; }

	virtual void ModifyVoxelData(const FIntVector Position, EBlock Block) PURE_VIRTUAL(AChunkBase::ModifyVoxelData);

protected:
	virtual void BeginPlay() override;

	// 子类在这里把体素数据填进自己的容器（例如 GreedyChunk::Blocks）。
	virtual void GenerateVoxelData() PURE_VIRTUAL(AChunkBase::GenerateVoxelData);

	// 子类在这里读取体素数据并写入 MeshData。
	virtual void GenerateMesh() PURE_VIRTUAL(AChunkBase::GenerateMesh);

	TObjectPtr<UProceduralMeshComponent> Mesh;
	FChunkMeshData MeshData;       // section 0：贴图方块
	FChunkMeshData MeshDataColor;  // section 1：纯色方块
	int VertexCount = 0;
	int VertexCountColor = 0;

private:
	void ApplyMesh() const;
	void ClearMesh();
};
