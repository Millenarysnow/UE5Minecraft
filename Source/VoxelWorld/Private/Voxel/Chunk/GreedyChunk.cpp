#include "GreedyChunk.h"

#include "Voxel/Utils/Enums.h"
#include "ProceduralMeshComponent.h"
#include "Voxel/Utils/FastNoiseLite.h"

void AGreedyChunk::Setup()
{
	// 初始化方块数组
	Blocks.SetNum(Size * Size * Size);
}

void AGreedyChunk::Generate2DHeightMap(const FVector Position)
{
	for (int x = 0; x < Size; x++)
	{
		for (int y = 0; y < Size; y++)
		{
			const float Xpos = x + Position.X;
			const float Ypos = y + Position.Y;

			// GetNoise返回一个[-1, 1]的数
			// (Noise->GetNoise(Xpos, Ypos) + 1) * Size / 2 用于将这个数缩放至[0, Size]
			// RoundToInt 将浮点数四舍五入为整数
			// Clamp 将值限制在[0, Size]之间，实际上仅仅只是为了更安全
			const int Height = FMath::Clamp(FMath::RoundToInt((Noise->GetNoise(Xpos, Ypos) + 1) * Size / 2), 0, Size);
    
			// 下面为填充方块
			for (int z = 0; z < Size; z++)
			{
				if (z < Height - 3) Blocks[GetBlockIndex(x, y, z)] = EBlock::Stone;
				else if (z < Height - 1) Blocks[GetBlockIndex(x, y, z)] = EBlock::Dirt;
				else if (z == Height - 1) Blocks[GetBlockIndex(x, y, z)] = EBlock::Grass;
				else Blocks[GetBlockIndex(x, y, z)] = EBlock::Air;
			}
		}
	}
}

void AGreedyChunk::Generate3DHeightMap(const FVector Position)
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

		TArray<FMask> Mask; // 对于当前切片的掩码数组
		Mask.SetNum(Axis1Limit * Axis2Limit); // 初始化大小（这是一个二维数组被压成了一维）

		// 遍历所有切片
		for (ChunkItr[Axis] = -1; ChunkItr[Axis] < MainAxisLimit; )
		{
			int N = 0; // 掩码中的索引

			// 遍历垂直面，生成该面的所有掩码
			for (ChunkItr[Axis2] = 0; ChunkItr[Axis2] < Axis2Limit; ChunkItr[Axis2]++)
			{
				for (ChunkItr[Axis1] = 0; ChunkItr[Axis1] < Axis1Limit; ChunkItr[Axis1]++)
				{
					const auto CurrentBlock = GetBlock(ChunkItr); // 当前方块
					const auto CompareBlock = GetBlock(ChunkItr + AxisMask); // 在当前主轴方向上与当前方块相邻的方块

					// 判断两个方块是否是不透明方块
					const bool CurrentBlockOpaque = CurrentBlock != EBlock::Air;
					const bool CompareBlockOpaque = CompareBlock != EBlock::Air;

					/*
					 * 如果两个方块的透明属性相同，意味着当前这个方块的这个面不会被看到
					 * 反之，就需要在这里创建一个掩码，法线方向指向不透明方块方向
					*/
					if (CurrentBlockOpaque == CompareBlockOpaque) // 相同则不创建面
					{
						Mask[N++] = FMask { EBlock::Null, 0 };
					}
					else if (CurrentBlockOpaque) // 当前方块不透明
					{
						Mask[N++] = FMask { CurrentBlock, 1}; // 法线方向 1 代表朝着当前方块创建了一个面
					}
					else // 相邻方块不透明
					{
						Mask[N++] = FMask { CompareBlock, -1}; // 法线方向 -1 代表朝着相邻方块创建了一个面
					}
				}
			}

			ChunkItr[Axis]++;
			N = 0;

			// 从掩码中生成网格体
			for (int j = 0; j < Axis2Limit; j++)
			{
				for (int i = 0; i < Axis1Limit; )
				{
					if (Mask[N].Normal != 0)
					{
						const auto CurrentMask = Mask[N];
						ChunkItr[Axis1] = i;
						ChunkItr[Axis2] = j;

						// 横向扩展，获取当前贪心合并的面的宽度
						int width;
						for (width = 1; i + width < Axis1Limit && CompareMask(Mask[N + width], CurrentMask); width++)
						{ }

						// 纵向扩展高度
						int height;
						bool done = false;
						for (height = 1; j + height < Axis2Limit; height++) // 纵向扩展的层数
						{
							for (int k = 0; k < width; k++) // 比较当前待扩展的层的方块是否都相同，相同才能扩展一层
							{
								if (CompareMask(Mask[N + k + height * Axis1Limit], CurrentMask))
									continue;

								done = true;
								break;
							}

							if (done) break;
						}

						// 记录计算结果，方便后续处理
						DeltaAxis1[Axis1] = width;
						DeltaAxis2[Axis2] = height;

						// 创建合并后的一个面
						CreateQuad(
							CurrentMask,
							AxisMask,
							ChunkItr,
							ChunkItr + DeltaAxis1,
							ChunkItr+ DeltaAxis2,
							ChunkItr + DeltaAxis1 + DeltaAxis2,
							width,
							height
						);

						// 清理变量
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
						i ++;
						N ++;
					}
				}
			}
		}
	}
}

// 后四个参数表示矩形的四个顶点位置
void AGreedyChunk::CreateQuad(FMask Mask, FIntVector AxisMask, FIntVector V1, FIntVector V2, FIntVector V3,
	FIntVector V4, const int Width, const int Height)
{
	const auto Normal = FVector(AxisMask * Mask.Normal);
	
	// 暂时使用颜色通道的Alpha传递纹理索引
	const auto Color = FColor(0, 0,0, GetTextureIndex(Mask.Block, Normal));

	MeshData.Vertices.Add(FVector(V1) * 100);
	MeshData.Vertices.Add(FVector(V2) * 100);
	MeshData.Vertices.Add(FVector(V3) * 100);
	MeshData.Vertices.Add(FVector(V4) * 100);

	MeshData.Triangles.Add(VertexCount);
	MeshData.Triangles.Add(VertexCount + 2 + Mask.Normal);
	MeshData.Triangles.Add(VertexCount + 2 - Mask.Normal);
	MeshData.Triangles.Add(VertexCount + 3);
	MeshData.Triangles.Add(VertexCount + 1 - Mask.Normal);
	MeshData.Triangles.Add(VertexCount + 1 + Mask.Normal);

	// 不考虑顶视图被旋转的情况下，修复UV方向问题
	if (Normal.X == 1 || Normal.X == -1)
	{
		MeshData.UVO.Append({
			FVector2D(Width, Height),
			FVector2D(0, Height),
			FVector2D(Width, 0),
			FVector2D(0, 0)
		});
	}
	else
	{
		MeshData.UVO.Append({
			FVector2D(Height, Width),
			FVector2D(Height, 0),
			FVector2D(0, Width),
			FVector2D(0, 0)
		});

	}

	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);

	MeshData.Colors.Append({Color, Color, Color, Color});

	VertexCount += 4;
}

int AGreedyChunk::GetBlockIndex(int X, int Y, int Z) const
{
	return Z * Size * Size + Y * Size + X;
}

EBlock AGreedyChunk::GetBlock(FIntVector Index) const
{
	// 越界时返回 Air
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
	default:
		return 255;
	}
}

void AGreedyChunk::ModifyTargetVoxel(const int& TargetIndex, const EBlock& Block)
{
	Blocks[TargetIndex] = Block;
}

void AGreedyChunk::ModifyVoxelData(const FIntVector Position, EBlock Block)
{
	const int Index = GetBlockIndex(Position.X, Position.Y, Position.Z);

	Blocks[Index] = Block;
}
