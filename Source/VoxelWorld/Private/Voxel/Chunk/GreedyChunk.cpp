#include "GreedyChunk.h"

#include "Voxel/Generation/WorldGenerator.h"
#include "Voxel/Utils/Enums.h"

namespace
{
	// 已制作贴图的方块直接走 texture array 路径；未制作贴图的方块走纯色 vertex-color 路径。
	bool IsColoredBlock(EBlock B)
	{
		switch (B)
		{
		case EBlock::Grass:
		case EBlock::Dirt:
		case EBlock::Stone:
		case EBlock::Wood:
		case EBlock::Leaf:
			return false;
		default:
			return true;
		}
	}

	// 纯色方块的颜色查找表。每个方块挑一个易区分的颜色。
	FColor GetBlockColor(EBlock B)
	{
		switch (B)
		{
		case EBlock::Sand:        return FColor(220, 200, 130, 255); // 沙黄
		case EBlock::Sandstone:   return FColor(200, 175, 110, 255); // 沙岩
		case EBlock::Snow:        return FColor(245, 245, 255, 255); // 雪白带蓝
		case EBlock::Water:       return FColor( 60, 110, 200, 255); // 水蓝
		case EBlock::Bedrock:     return FColor( 40,  40,  45, 255); // 近黑
		case EBlock::CoalOre:     return FColor( 60,  60,  60, 255); // 暗灰
		case EBlock::IronOre:     return FColor(180, 140, 100, 255); // 铁锈
		case EBlock::DiamondOre:  return FColor(130, 220, 220, 255); // 钻石青
		case EBlock::SpruceLog:   return FColor( 90,  60,  30, 255); // 深棕
		case EBlock::SpruceLeaf:  return FColor( 40, 100,  50, 255); // 深绿
		default:                  return FColor(255,   0, 255, 255); // 洋红 = 未配置
		}
	}
}

void AGreedyChunk::GenerateVoxelData()
{
	UWorldGenerator* Gen = UWorldGenerator::Get(GetWorld());
	if (!Gen)
	{
		Blocks.SetNumZeroed(Size * Size * Size);
		return;
	}

	Gen->FillChunk(ChunkOriginVoxel, Size, Blocks);
}

void AGreedyChunk::GenerateMesh()
{
	// 贪婪网格（Greedy Meshing）生成主函数

	// 遍历三个轴
	for (int Axis = 0; Axis < 3; Axis++)
	{
		// 获取到垂直的另外两个轴
		const int Axis1 = (Axis + 1) % 3;
		const int Axis2 = (Axis + 2) % 3;

		// 获取当前对应轴的限制范围
		const int MainAxisLimit = Size;
		int Axis1Limit = Size;
		int Axis2Limit = Size;

		auto DeltaAxis1 = FIntVector::ZeroValue;
		auto DeltaAxis2 = FIntVector::ZeroValue;

		auto ChunkItr = FIntVector::ZeroValue; // 当前正在遍历的方块
		auto AxisMask = FIntVector::ZeroValue; // 表示当前正在处理的主轴是谁（对应为1，其余为0）

		AxisMask[Axis] = 1;

		TArray<FMask> Mask;
		Mask.SetNum(Axis1Limit * Axis2Limit);

		for (ChunkItr[Axis] = -1; ChunkItr[Axis] < MainAxisLimit; )
		{
			int N = 0;

			for (ChunkItr[Axis2] = 0; ChunkItr[Axis2] < Axis2Limit; ChunkItr[Axis2]++)
			{
				for (ChunkItr[Axis1] = 0; ChunkItr[Axis1] < Axis1Limit; ChunkItr[Axis1]++)
				{
					const auto CurrentBlock = GetBlock(ChunkItr);
					const auto CompareBlock = GetBlock(ChunkItr + AxisMask);

					const bool CurrentBlockOpaque = CurrentBlock != EBlock::Air;
					const bool CompareBlockOpaque = CompareBlock != EBlock::Air;

					if (CurrentBlockOpaque == CompareBlockOpaque)
					{
						Mask[N++] = FMask { EBlock::Null, 0 };
					}
					else if (CurrentBlockOpaque)
					{
						Mask[N++] = FMask { CurrentBlock, 1};
					}
					else
					{
						Mask[N++] = FMask { CompareBlock, -1};
					}
				}
			}

			ChunkItr[Axis]++;
			N = 0;

			for (int j = 0; j < Axis2Limit; j++)
			{
				for (int i = 0; i < Axis1Limit; )
				{
					if (Mask[N].Normal != 0)
					{
						const auto CurrentMask = Mask[N];
						ChunkItr[Axis1] = i;
						ChunkItr[Axis2] = j;

						int width;
						for (width = 1; i + width < Axis1Limit && CompareMask(Mask[N + width], CurrentMask); width++)
						{ }

						int height;
						bool done = false;
						for (height = 1; j + height < Axis2Limit; height++)
						{
							for (int k = 0; k < width; k++)
							{
								if (CompareMask(Mask[N + k + height * Axis1Limit], CurrentMask))
									continue;

								done = true;
								break;
							}

							if (done) break;
						}

						DeltaAxis1[Axis1] = width;
						DeltaAxis2[Axis2] = height;

						const bool bColored = IsColoredBlock(CurrentMask.Block);
						FChunkMeshData& Buf = bColored ? MeshDataColor : MeshData;
						int& Count = bColored ? VertexCountColor : VertexCount;

						CreateQuad(
							CurrentMask,
							AxisMask,
							ChunkItr,
							ChunkItr + DeltaAxis1,
							ChunkItr + DeltaAxis2,
							ChunkItr + DeltaAxis1 + DeltaAxis2,
							width,
							height,
							Buf,
							Count
						);

						DeltaAxis1 = FIntVector::ZeroValue;
						DeltaAxis2 = FIntVector::ZeroValue;
						for (int l = 0; l < height; l++)
						{
							for (int k = 0; k < width; k++)
							{
								Mask[N + k + l * Axis1Limit] = FMask { EBlock::Null, 0 };
							}
						}

						i += width;
						N += width;
					}
					else
					{
						i++;
						N++;
					}
				}
			}
		}
	}
}

void AGreedyChunk::CreateQuad(FMask Mask, FIntVector AxisMask, FIntVector V1, FIntVector V2, FIntVector V3,
	FIntVector V4, const int Width, const int Height, FChunkMeshData& Buffer, int& Count)
{
	const auto Normal = FVector(AxisMask * Mask.Normal);

	const bool bColored = IsColoredBlock(Mask.Block);
	const FColor Color = bColored
		? GetBlockColor(Mask.Block)
		: FColor(0, 0, 0, GetTextureIndex(Mask.Block, Normal));

	Buffer.Vertices.Add(FVector(V1) * 100);
	Buffer.Vertices.Add(FVector(V2) * 100);
	Buffer.Vertices.Add(FVector(V3) * 100);
	Buffer.Vertices.Add(FVector(V4) * 100);

	Buffer.Triangles.Add(Count);
	Buffer.Triangles.Add(Count + 2 + Mask.Normal);
	Buffer.Triangles.Add(Count + 2 - Mask.Normal);
	Buffer.Triangles.Add(Count + 3);
	Buffer.Triangles.Add(Count + 1 - Mask.Normal);
	Buffer.Triangles.Add(Count + 1 + Mask.Normal);

	if (Normal.X == 1 || Normal.X == -1)
	{
		Buffer.UVO.Append({
			FVector2D(Width, Height),
			FVector2D(0, Height),
			FVector2D(Width, 0),
			FVector2D(0, 0)
		});
	}
	else
	{
		Buffer.UVO.Append({
			FVector2D(Height, Width),
			FVector2D(Height, 0),
			FVector2D(0, Width),
			FVector2D(0, 0)
		});
	}

	Buffer.Normals.Add(Normal);
	Buffer.Normals.Add(Normal);
	Buffer.Normals.Add(Normal);
	Buffer.Normals.Add(Normal);

	Buffer.Colors.Append({Color, Color, Color, Color});

	Count += 4;
}

int AGreedyChunk::GetBlockIndex(int X, int Y, int Z) const
{
	return Z * Size * Size + Y * Size + X;
}

EBlock AGreedyChunk::GetBlock(FIntVector Index) const
{
	if (Index.X < 0 || Index.Y < 0 || Index.Z < 0 || Index.X >= Size || Index.Y >= Size || Index.Z >= Size)
		return EBlock::Air;

	return Blocks[GetBlockIndex(Index.X, Index.Y, Index.Z)];
}

bool AGreedyChunk::CompareMask(FMask M1, FMask M2) const
{
	return M1.Block == M2.Block && M1.Normal == M2.Normal;
}

int AGreedyChunk::GetTextureIndex(EBlock Block, FVector Normal)
{
	// 仅处理 IsColoredBlock 返回 false 的方块；其余方块通过纯色路径渲染。
	switch (Block)
	{
	case EBlock::Grass:
		{
			if (Normal == FVector::UpVector) return 0;
			if (Normal == FVector::DownVector) return 2;
			return 1;
		}
	case EBlock::Dirt:
		return 2;
	case EBlock::Stone:
		return 3;
	case EBlock::Wood:
		{
			if (Normal == FVector::UpVector) return 4;
			if (Normal == FVector::DownVector) return 4;
			return 5;
		}
	case EBlock::Leaf:
		return 6;
	default:
		return 255;
	}
}

void AGreedyChunk::ModifyTargetVoxel(const int& TargetIndex, const EBlock& Block)
{
	Blocks[TargetIndex] = Block;
}

EBlock AGreedyChunk::GetVoxel(const FIntVector Position) const
{
	const int Index = GetBlockIndex(Position.X, Position.Y, Position.Z);
	return Blocks[Index];
}

void AGreedyChunk::ModifyVoxelData(const FIntVector Position, EBlock Block)
{
	const int Index = GetBlockIndex(Position.X, Position.Y, Position.Z);
	Blocks[Index] = Block;
}
