
#include "ChunkBase.h"
#include "ProceduralMeshComponent.h"
#include "Voxel/Utils/FastNoiseLite.h"

AChunkBase::AChunkBase()
{
	PrimaryActorTick.bCanEverTick = false;
	
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>("Mesh");
	Noise = new FastNoiseLite();

	// Mesh 设置
	Mesh->SetCastShadow(false);
	SetRootComponent(Mesh);
}

void AChunkBase::ModifyVoxel(const FIntVector Position, EBlock Block)
{
	if (Position.X < 0 || Position.Y < 0 || Position.Z < 0 || Position.X >= Size || Position.Y >= Size || Position.Z >= Size) return;

	ModifyVoxelData(Position, Block);

	ClearMesh();

	GenerateMesh();

	ApplyMesh();
}

void AChunkBase::BeginPlay()
{
	Super::BeginPlay();

	Noise->SetFrequency(Frequency);
	Noise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
	Noise->SetFractalType(FastNoiseLite::FractalType_FBm);

	Setup();

	GenerateHeightMap();

	GenerateMesh();

	UE_LOG(LogTemp, Warning, TEXT("Vertex Count : %d"), VertexCount);
	
	ApplyMesh();
}

void AChunkBase::GenerateHeightMap()
{
	switch (GenerationType)
	{
	case EGenerationType::GT_3D:
		Generate3DHeightMap(GetActorLocation() / 100);
		break;
	case EGenerationType::GT_2D:
		Generate2DHeightMap(GetActorLocation() / 100);
		break;
	default:
		throw std::exception("Invalid Generation Type");
	}
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
		false
	);
}

void AChunkBase::ClearMesh()
{
	VertexCount = 0;
	MeshData.Clear();
}
