// Fill out your copyright notice in the Description page of Project Settings.


#include "MarchingChunk.h"

#include "Voxel/Utils/FastNoiseLite.h"

void AMarchingChunk::Setup()
{
	/*
	 * 初始化时多初始化一个方块的原因是
	 * 如果只生成与区块同等大小的网格体，
	 * 那么由于取测试点时，是在区块内的每个小方块中心取点，
	 * 就会导致每个区块内部生成网格体，而距离区块边缘半个小方块的范围内不生成
	 * 所以向三个方向各多初始化一个方块，
	 * 这样就能保证区块之间可以无缝拼在一起
	*/
	Voxels.SetNum((Size + 1) * (Size + 1) * (Size + 1));
}

void AMarchingChunk::Generate2DHeightMap(const FVector Position)
{
	for (int x = 0; x <= Size; x++)
	{
		for (int y = 0; y <= Size; y++)
		{
			const float Xpos = x + Position.X;
			const float Ypos = y + Position.Y;

			const int Height = FMath::Clamp(FMath::RoundToInt((Noise->GetNoise(Xpos, Ypos) + 1) * Size / 2), 0, Size);
			
			for (int z = 0; z < Height; z++)
			{
				Voxels[GetVoxelIndex(x, y, z)] = 1.0f;
			}
			for (int z = Height; z < Size; z++)
			{
				Voxels[GetVoxelIndex(x, y, z)] = -1.0f;
			}
		}
	}
}

void AMarchingChunk::Generate3DHeightMap(const FVector Position)
{
	for (int x = 0; x <= Size; x++)
	{
		for (int y = 0; y <= Size; y++)
		{
			for (int z = 0; z <= Size; z++)
			{
				Voxels[GetVoxelIndex(x, y, z)] = Noise->GetNoise(Position.X + x, Position.Y + y, Position.Z + z);
			}
		}
	}
}

void AMarchingChunk::GenerateMesh()
{
	// 根据 SurfaceLevel 判断是否要翻转三角形朝向
	if (SurfaceLevel > 0.0f)
	{
		TriangleOrder = {0, 1, 2};
	}
	else
	{
		TriangleOrder = {2, 1, 0};
	}

	// 遍历所有小方块
	float Cube[8];
	for (int x = 0; x < Size; x++)
	{
		for (int y = 0; y < Size; y++)
		{
			for (int z = 0; z < Size; z++)
			{
				// 填充方块顶点坐标数据
				for (int i = 0; i < 8; i++)
				{
					Cube[i] = Voxels[GetVoxelIndex(x + VertexOffset[i][0], y + VertexOffset[i][1], z + VertexOffset[i][2])];
				}

				// 生成地形
				March(x, y, z, Cube);
			}
		}
	}
}

void AMarchingChunk::March(int X, int Y, int Z, const float Cube[8])
{
	int VertexMask = 0;
	FVector EdgeVertex[12];

	// 遍历八个顶点
	for (int i = 0; i < 8; i++)
	{
		if (Cube[i] <= SurfaceLevel)
			VertexMask |= 1 << i;
	}

	const int EdgeMask = CubeEdgeFlags[VertexMask];

	// 如果没有边，则这个方块没有需要生成的对象
	if (EdgeMask == 0) return;

	// 遍历所有的边
	for (int i = 0; i < 12; i++)
	{
		if ((EdgeMask & 1 << i) != 0)
		{
			const float Offset =
				Interpolation ? GetInterpolationOffset(Cube[EdgeConnection[i][0]], Cube[EdgeConnection[i][1]]) : 0.5f;

			EdgeVertex[i].X = X + (VertexOffset[EdgeConnection[i][0]][0] + Offset * EdgeDirection[i][0]);
			EdgeVertex[i].Y = Y + (VertexOffset[EdgeConnection[i][0]][1] + Offset * EdgeDirection[i][1]);
			EdgeVertex[i].Z = Z + (VertexOffset[EdgeConnection[i][0]][2] + Offset * EdgeDirection[i][2]);
		}
	}

	// 数学上可证明此处最多只会创建五个三角形
	for (int i = 0; i < 5; i++)
	{
		// 这句是由于观察 TriangleConnectionTable ，可以发现 -1 都是连续且在末尾
		// 同时当遇到 -1 时，即代表这里没有三角形
		if (TriangleConnectionTable[VertexMask][3 * i] < 0) break;

		// 获取三角形的三个顶点
		auto V1 = EdgeVertex[TriangleConnectionTable[VertexMask][3 * i]] * 100;
		auto V2 = EdgeVertex[TriangleConnectionTable[VertexMask][3 * i + 1]] * 100;
		auto V3 = EdgeVertex[TriangleConnectionTable[VertexMask][3 * i + 2]] * 100;

		// 计算该三角形的法线
		/*
		 * 计算方法：
		 * 已知三个点在一个平面上，那么这三个点就可以确定这个平面
		 * 然后任意两两相减，得到两个属于这个平面的向量，叉乘即得法向向量
		*/
		auto Normal = FVector::CrossProduct(V2 - V1, V3 - V1);
		Normal.Normalize();

		// 向网格体中添加数据，由于UV很复杂，暂时没有处理
		
		MeshData.Vertices.Add(V1);
		MeshData.Vertices.Add(V2);
		MeshData.Vertices.Add(V3);

		MeshData.Triangles.Add(VertexCount + TriangleOrder[0]);
		MeshData.Triangles.Add(VertexCount + TriangleOrder[1]);
		MeshData.Triangles.Add(VertexCount + TriangleOrder[2]);

		MeshData.Normals.Add(Normal);
		MeshData.Normals.Add(Normal);
		MeshData.Normals.Add(Normal);

		auto Color = FColor::MakeRandomColor();
		
		MeshData.Colors.Append({Color, Color, Color});

		VertexCount += 3;
	}
}

int AMarchingChunk::GetVoxelIndex(int X, int Y, int Z) const
{
	return Z * (Size + 1) * (Size + 1) + Y * (Size + 1) + X;
}

float AMarchingChunk::GetInterpolationOffset(float V1, float V2) const
{
	const float Delta = V2 - V1;
	return Delta == 0.0f ? SurfaceLevel : (SurfaceLevel - V1) / Delta;
}
