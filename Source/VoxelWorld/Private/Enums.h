#pragma once

// 顺序不能变动，因为和立方体的顶点数据一一对应
enum class EDirection
{
	Forward, Right, Back, Left, Up, Down
};

enum class EBlock
{
	Null, Air, Stone, Dirt, Grass
};
