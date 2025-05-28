#include "GreedyChunk.h"

#include "Enums.h"
#include "ProceduralMeshComponent.h"
#include "FastNoiseLite.h"

AGreedyChunk::AGreedyChunk()
{
	// 初始化 Blocks
	Blocks.SetNum(Size * Size * Size);
}

void AGreedyChunk::BeginPlay()
{
	Super::BeginPlay();
}

void AGreedyChunk::GenerateHeightMap()
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
							ChunkItr + DeltaAxis1 + DeltaAxis2
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
	FIntVector V4)
{
	const auto Normal = FVector(AxisMask * Mask.Normal);

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

	MeshData.UVO.Add(FVector2D(0, 0));
	MeshData.UVO.Add(FVector2D(0, 1));
	MeshData.UVO.Add(FVector2D(1, 0));
	MeshData.UVO.Add(FVector2D(1, 1));

	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);
	MeshData.Normals.Add(Normal);

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
