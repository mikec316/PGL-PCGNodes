// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Voxel/WaterVoxelModifierComponent.h"
#include "IslandVoxelModifierComponent.generated.h"

class UVoxelVolumeClosedSplineGraph;
class UVoxelHeightClosedSplineGraph;
class UVoxelVolumeLayer;
class UVoxelHeightLayer;

/** Which voxel graph family drives the island stamp. */
UENUM(BlueprintType)
enum class EIslandVoxelGraphKind : uint8
{
	/** Use a height-field closed-spline graph (cheaper, heightmap-style terrains). */
	Height UMETA(DisplayName = "Height"),

	/** Use a volume closed-spline graph (full 3D SDF — supports overhangs, caves). */
	Volume UMETA(DisplayName = "Volume"),
};

/**
 * Bridges an AWaterBodyIsland to a Voxel Plugin closed-spline stamp.
 *
 * Expects to be attached to an AWaterBodyIsland actor with a closed-loop spline.
 * Pick Height or Volume graph kind to match the rest of your voxel terrain pipeline;
 * the corresponding graph slot is used and the other is ignored.
 *
 * The graph is responsible for defining the island SDF. Unlike lakes (which carve
 * down), islands typically displace terrain up — usually expressed as an Additive
 * blend mode on the stamp or via the graph's own sign convention.
 */
UCLASS(ClassGroup = Voxel, meta = (BlueprintSpawnableComponent, DisplayName = "Island Voxel Modifier"))
class PGLPCGNODES_API UIslandVoxelModifierComponent : public UWaterVoxelModifierComponent
{
	GENERATED_BODY()

public:
	/** Which graph family to stamp with — Height is lighter, Volume supports full 3D shapes. */
	UPROPERTY(EditAnywhere, Category = "Water Voxel Bridge")
	EIslandVoxelGraphKind GraphKind = EIslandVoxelGraphKind::Height;

	/** Height graph asset — used when GraphKind = Height. */
	UPROPERTY(EditAnywhere, Category = "Water Voxel Bridge", meta = (EditCondition = "GraphKind == EIslandVoxelGraphKind::Height", EditConditionHides))
	TObjectPtr<UVoxelHeightClosedSplineGraph> HeightGraph;

	/** Height layer the stamp is applied to — used when GraphKind = Height. */
	UPROPERTY(EditAnywhere, Category = "Water Voxel Bridge", meta = (EditCondition = "GraphKind == EIslandVoxelGraphKind::Height", EditConditionHides))
	TObjectPtr<UVoxelHeightLayer> HeightLayer;

	/** Volume graph asset — used when GraphKind = Volume. */
	UPROPERTY(EditAnywhere, Category = "Water Voxel Bridge", meta = (EditCondition = "GraphKind == EIslandVoxelGraphKind::Volume", EditConditionHides))
	TObjectPtr<UVoxelVolumeClosedSplineGraph> VolumeGraph;

	/** Volume layer the stamp is applied to — used when GraphKind = Volume. */
	UPROPERTY(EditAnywhere, Category = "Water Voxel Bridge", meta = (EditCondition = "GraphKind == EIslandVoxelGraphKind::Volume", EditConditionHides))
	TObjectPtr<UVoxelVolumeLayer> VolumeLayer;

protected:
	//~ Begin UWaterVoxelModifierComponent Interface
	virtual void BuildStampInternal() override;
	virtual bool WantsClosedLoop() const override { return true; }
	virtual USplineComponent* GetSourceSpline() const override;
	//~ End UWaterVoxelModifierComponent Interface
};
