#include "NaiveChunk.h"

#include "Voxel/Utils/Enums.h"
#include "ProceduralMeshComponent.h"
#include "Voxel/Utils/FastNoiseLite.h"

void ANaiveChunk::Setup()
{
	Blocks.SetNum(Size * Size * Size);
}

void ANaiveChunk::Generate2DHeightMap(const FVector Position)
{
	for (int x = 0; x < Size; x++)
	{
		for (int y = 0; y < Size; y++)
		{
			const float Xpos = x + Position.X;
			const float Ypos = y + Position.Y;

			const int Height = FMath::Clamp(FMath::RoundToInt((Noise->GetNoise(Xpos, Ypos) + 1) * Size / 2), 0, Size);
			
			for (int z = 0; z < Height; z++)
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

void ANaiveChunk::Generate3DHeightMap(const FVector Position)
{
	for (int x = 0; x < Size; x++)
	{
		for (int y = 0; y < Size; y++)
		{
			for (int z = 0; z < Size; z++)
			{
				const auto NoiseValue = Noise->GetNoise(Position.X + x, Position.Y + y, Position.Z + z);

				if (NoiseValue >= 0)
				{
					Blocks[GetBlockIndex(x, y, z)] = EBlock::Air;
				}
				else
				{
					Blocks[GetBlockIndex(x, y, z)] = EBlock::Stone;
				}
			}
		}
	}
}

void ANaiveChunk::GenerateMesh()
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

void ANaiveChunk::ModifyVoxelData(const FIntVector Position, EBlock Block)
{
	const int Index = GetBlockIndex(Position.X, Position.Y, Position.Z);

	Blocks[Index] = Block;
}

bool ANaiveChunk::Check(FVector Position) const
{
	if (Position.X < 0 || Position.X >= Size || Position.Y < 0 || Position.Y >= Size || Position.Z < 0 || Position.Z >= Size)
	{
		return true;
	}

	if (Position.Z >= Size) return true; // ?

	return Blocks[GetBlockIndex(Position.X, Position.Y, Position.Z)] == EBlock::Air;
}

void ANaiveChunk::CreateFace(EDirection Direction, FVector Position)
{
	const auto Color = FColor::MakeRandomColor();
	const auto Normal = GetNormal(Direction);
	
	MeshData.Vertices.Append(GetFaceVertices(Direction, Position));
	MeshData.UVO.Append({FVector2D(1, 1), FVector2D(1, 0), FVector2D(0, 0), FVector2D(0, 1)});
	MeshData.Triangles.Append({VertexCount + 3, VertexCount + 2, VertexCount, VertexCount + 2, VertexCount + 1, VertexCount});
	MeshData.Normals.Append({Normal, Normal, Normal, Normal});
	MeshData.Colors.Append({Color, Color, Color, Color});
	
	VertexCount += 4;
}

TArray<FVector> ANaiveChunk::GetFaceVertices(EDirection Direction, FVector Position) const
{
	TArray<FVector> Vertices; // 顶点数组

	for (int i = 0; i < 4; i++)
	{
		Vertices.Add(BlockVertexData[BlockTriangleData[i + static_cast<int>(Direction) * 4]] + Position);
	}

	return Vertices;
}

FVector ANaiveChunk::GetPositionInDirection(EDirection Direction, FVector Position) const
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

int ANaiveChunk::GetBlockIndex(int X, int Y, int Z) const
{
	return Z * Size * Size + Y * Size + X;
}

FVector ANaiveChunk::GetNormal(EDirection Direction)
{
	switch (Direction)
	{
		case EDirection::Forward:
			return FVector::ForwardVector;
		case EDirection::Right:
			return FVector::RightVector;
		case EDirection::Back:
			return FVector::BackwardVector;
		case EDirection::Left:
			return FVector::LeftVector;
		case EDirection::Up:
			return FVector::UpVector;
		case EDirection::Down:
			return FVector::DownVector;
		default:
			throw std::exception();
	}
}



