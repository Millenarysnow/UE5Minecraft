#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CloudLayer.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class FastNoiseLite;

/**
 * MC 风格云层。
 *
 * 实现方式：
 *   - 单个 actor，`UProceduralMeshComponent` 渲染所有云 cell
 *   - 云在水平方向以 cell 为单位（cell 12 块），垂直方向 4 块厚
 *   - 2D Perlin 噪声 > Threshold 处生成 cell
 *   - 每 cell 加 6 面 cube（top/bottom 一定生成，4 sides 仅当邻居非云时生成）
 *   - Actor 位置跟随玩家水平位置，云高度 y=192 固定
 *   - Tick 累积 WindOffsetBlocks 模拟风速；每秒重建 mesh 让云看起来匀速移动
 *
 * 所需材质：half-transparent 白色（蓝图里建一个 M_Cloud：Translucent，Base Color 白，
 * Opacity ~0.7。或 Unlit + Translucent 也行）。
 */
UCLASS()
class ACloudLayer : public AActor
{
	GENERATED_BODY()

public:
	ACloudLayer();

	// 云层 y 高度（块坐标，UE.Z 方向）。默认 MC 的 192。
	UPROPERTY(EditAnywhere, Category = "Cloud")
	int CloudHeight = 192;

	// 云层覆盖半径（块）。云 mesh 在 [-Radius, +Radius]² 范围生成。
	UPROPERTY(EditAnywhere, Category = "Cloud")
	int RadiusBlocks = 384;

	// 单个云 cell 边长（块）。MC 默认 12。
	UPROPERTY(EditAnywhere, Category = "Cloud")
	int CellSizeBlocks = 12;

	// 云层厚度（块）。MC 默认 4。
	UPROPERTY(EditAnywhere, Category = "Cloud")
	int ThicknessBlocks = 4;

	// 风速（块/秒）。MC 默认 0.6。
	UPROPERTY(EditAnywhere, Category = "Cloud")
	float WindSpeedBlocksPerSec = 0.6f;

	// 噪声阈值。> Threshold 处为云，否则空。范围 ~[-0.5, 0.5]。
	UPROPERTY(EditAnywhere, Category = "Cloud")
	float Threshold = 0.0f;

	// 重建 mesh 间隔（秒）。低值更平滑但更卡。
	UPROPERTY(EditAnywhere, Category = "Cloud", meta = (ClampMin = "0.2"))
	float RebuildInterval = 1.0f;

	// 玩家移动多少块就重建一次（避免云"消失在视野边缘"）。
	UPROPERTY(EditAnywhere, Category = "Cloud", meta = (ClampMin = "8"))
	int PlayerMoveRebuildThresholdBlocks = 64;

	UPROPERTY(EditAnywhere, Category = "Cloud")
	TObjectPtr<UMaterialInterface> CloudMaterial;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Mesh;

	FastNoiseLite* Noise = nullptr;

	// 累积风偏移（块）。注入到 noise 采样位置，等价于云沿 +X 移动。
	float WindOffsetBlocks = 0.0f;

	// 上次重建时玩家所在的块坐标（用于判断玩家走远）
	int LastPlayerBlockX = 0;
	int LastPlayerBlockY = 0;

	// 上次重建以来累积时间
	float TimeSinceRebuild = 0.0f;

	void Rebuild(int CenterBlockX, int CenterBlockY);
};
