#include "ChunkBase.h"
#include "ProceduralMeshComponent.h"

AChunkBase::AChunkBase()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>("Mesh");
	Mesh->SetCastShadow(false);
	SetRootComponent(Mesh);
}

void AChunkBase::ModifyVoxel(const FIntVector Position, EBlock Block)
{
	if (Position.X < 0 || Position.Y < 0 || Position.Z < 0 ||
		Position.X >= Size || Position.Y >= Size || Position.Z >= Size) return;

	ModifyVoxelData(Position, Block);

	ClearMesh();
	GenerateMesh();
	ApplyMesh();
}

void AChunkBase::BeginPlay()
{
	Super::BeginPlay();

	GenerateVoxelData();
	GenerateMesh();

	UE_LOG(LogTemp, Verbose, TEXT("Chunk @ %s vertex count: %d"), *ChunkOriginVoxel.ToString(), VertexCount);

	ApplyMesh();
}

void AChunkBase::ApplyMesh() const
{
	Mesh->SetMaterial(0, Material);
	Mesh->CreateMeshSection(
		0,
		MeshData.Vertices,
		MeshData.Triangles,
		MeshData.Normals,
		MeshData.UVO,
		MeshData.Colors,
		TArray<FProcMeshTangent>(),
		true
	);

	Mesh->SetMaterial(1, MaterialColor);
	Mesh->CreateMeshSection(
		1,
		MeshDataColor.Vertices,
		MeshDataColor.Triangles,
		MeshDataColor.Normals,
		MeshDataColor.UVO,
		MeshDataColor.Colors,
		TArray<FProcMeshTangent>(),
		true
	);
}

void AChunkBase::ClearMesh()
{
	VertexCount = 0;
	VertexCountColor = 0;
	MeshData.Clear();
	MeshDataColor.Clear();
}
