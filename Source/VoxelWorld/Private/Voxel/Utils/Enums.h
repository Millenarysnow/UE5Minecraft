#pragma once

UENUM(BlueprintType)
enum class EDirection : uint8
{
	Forward, Right, Back, Left, Up, Down
};

// 方块类型。新增方块追加在末尾以避免破坏既有材质 / 顶点表索引。
UENUM(BlueprintType)
enum class EBlock : uint8
{
	Null,
	Air,
	Stone,
	Dirt,
	Grass,
	Wood,
	Leaf,
	Sand,
	Sandstone,
	Snow,
	Water,
	Bedrock,
	CoalOre,
	IronOre,
	DiamondOre,
	SpruceLog,
	SpruceLeaf
};

// MC 1.21 v1 选用的 6 个生物群系（参照 OverworldBiomeBuilder 中纬度表）。
UENUM(BlueprintType)
enum class EBiome : uint8
{
	Ocean,
	Plains,
	Forest,
	Desert,
	SnowyPlains,
	Mountains
};
