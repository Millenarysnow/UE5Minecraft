#include "Chunk.h"

#include "Enums.h"
#include "ProceduralMeshComponent.h"
#include "FastNoiseLite.h"

AChunk::AChunk()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>("Mesh");
	Noise = new FastNoiseLite();
	Noise->SetFrequency(0.03f);
	Noise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
	Noise->SetFractalType(FastNoiseLite::FractalType_FBm);

	Blocks.SetNum(Size * Size * Size);

	Mesh->SetCastShadow(false);
}

void AChunk::BeginPlay()
{
	Super::BeginPlay();

	GenerateBlocks();

	GenerateMesh();

	ApplyMesh();
}

void AChunk::GenerateBlocks()
{
	const auto Location = GetActorLocation();

	// 使用噪声生成高度图
	// x与y只是简单遍历
	for (int x = 0; x < Size; x++)
	{
		for (int y = 0; y < Size; y++)
		{
			// 乘100因为单个方块大小为100
			// 除以100是为了获取到一个整数
			const float Xpos = (x * 100 + Location.X) / 100;
			const float Ypos = (y * 100 + Location.Y) / 100;

			// GetNoise返回一个[-1, 1]的数
			// (Noise->GetNoise(Xpos, Ypos) + 1) * Size / 2 用于将这个数缩放至[0, Size]
			// RoundToInt 将浮点数四舍五入为整数
			// Clamp 将值限制在[0, Size]之间，实际上仅仅只是为了更安全
			const int Height = FMath::Clamp(FMath::RoundToInt((Noise->GetNoise(Xpos, Ypos) + 1) * Size / 2), 0, Size);

			// 下面为填充方块
			
			for (int z = 0; z < Size; z++)
			{
				Blocks[GetBlockIndex(x, y, z)] = EBlock::Stone;
			}

			for (int z = Height; z < Size; z++)
			{
				Blocks[GetBlockIndex(x, y, z)] = EBlock::Air;
			}
		}
	}
}

void AChunk::GenerateMesh()
{
	// 遍历所有方块
	for (int x = 0; x < Size; x++)
	{
		for (int y = 0; y < Size; y++)
		{
			for (int z = 0; z < Size; z++)
			{
				// 如果不为空气方块（透明方块）
				if (Blocks[GetBlockIndex(x, y, z)] != EBlock::Air)
				{
					const auto Position = FVector(x, y, z);

					// 遍历当前方块的六个方向，检查相邻位置是否为透明方块
					// 如果是透明方块，则创建一个面
					for (auto Direction : {EDirection::Forward, EDirection::Right, EDirection::Back, EDirection::Left, EDirection::Up, EDirection::Down})
					{
						if (Check(GetPositionInDirection(Direction, Position)))
						{
							CreateFace(Direction, Position * 100);
						}
					}
				}
			}
		}
	}
}

void AChunk::ApplyMesh() const
{
	Mesh->CreateMeshSection(
		0,
		VertexData,
		TriangleData,
		TArray<FVector>(),
		UVData,
		TArray<FColor>(),
		TArray<FProcMeshTangent>(),
		false
	);
}

bool AChunk::Check(FVector Position) const
{
	if (Position.X < 0 || Position.X >= Size || Position.Y < 0 || Position.Y >= Size || Position.Z < 0 || Position.Z >= Size)
	{
		return true;
	}

	return Blocks[GetBlockIndex(Position.X, Position.Y, Position.Z)] == EBlock::Air;
}

void AChunk::CreateFace(EDirection Direction, FVector Position)
{
	VertexData.Append(GetFaceVertices(Direction, Position));
	UVData.Append({FVector2D(1, 1), FVector2D(1, 0), FVector2D(0, 0), FVector2D(0, 1)});
	TriangleData.Append({VertexCount + 3, VertexCount + 2, VertexCount, VertexCount + 2, VertexCount + 1, VertexCount});
	VertexCount += 4;
}

TArray<FVector> AChunk::GetFaceVertices(EDirection Direction, FVector Position) const
{
	TArray<FVector> Vertices; // 顶点数组

	for (int i = 0; i < 4; i++)
	{
		Vertices.Add(BlockVertexData[BlockTriangleData[i + static_cast<int>(Direction) * 4]] * Scale + Position);
	}

	return Vertices;
}

FVector AChunk::GetPositionInDirection(EDirection Direction, FVector Position) const
{
	switch (Direction)
	{
	case EDirection::Forward:
		return Position + FVector::ForwardVector;
	case EDirection::Right:
		return Position + FVector::RightVector;
	case EDirection::Back:
		return Position + FVector::BackwardVector;
	case EDirection::Left:
		return Position + FVector::LeftVector;
	case EDirection::Up:
		return Position + FVector::UpVector;
	case EDirection::Down:
		return Position + FVector::DownVector;

	default:
		throw std::exception();
	}
}

int AChunk::GetBlockIndex(int X, int Y, int Z) const
{
	return Z * Size * Size + Y * Size + X;
}


