
#include "ChunkBase.h"
#include "ProceduralMeshComponent.h"
#include "FastNoiseLite.h"

AChunkBase::AChunkBase()
{
	PrimaryActorTick.bCanEverTick = false;
	
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>("Mesh");
	Noise = new FastNoiseLite();
	
	Noise->SetFrequency(0.03f);
	Noise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
	Noise->SetFractalType(FastNoiseLite::FractalType_FBm);

	// Mesh 设置
	Mesh->SetCastShadow(false);
	SetRootComponent(Mesh);
}

void AChunkBase::BeginPlay()
{
	Super::BeginPlay();

	GenerateHeightMap();

	GenerateMesh();

	UE_LOG(LogTemp, Warning, TEXT("Vertex Count : %d"), VertexCount);
	
	ApplyMesh();
}

void AChunkBase::GenerateHeightMap()
{
}

void AChunkBase::GenerateMesh()
{
}

void AChunkBase::ApplyMesh() const
{
	Mesh->CreateMeshSection(
		0,
		MeshData.Vertices,
		MeshData.Triangles,
		MeshData.Normals,
		MeshData.UVO,
		TArray<FColor>(),
		TArray<FProcMeshTangent>(),
		false
	);
}
