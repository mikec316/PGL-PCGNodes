// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Voxel/WaterVoxelModifierComponent.h"
#include "LakeVoxelModifierComponent.generated.h"

class UVoxelVolumeClosedSplineGraph;
class UVoxelHeightClosedSplineGraph;
class UVoxelVolumeLayer;
class UVoxelHeightLayer;
class UWaterBodyComponent;

/** Which voxel graph family drives the lake stamp. */
UENUM(BlueprintType)
enum class ELakeVoxelGraphKind : uint8
{
	/** Use a height-field closed-spline graph (cheaper, heightmap-style terrains). */
	Height UMETA(DisplayName = "Height"),

	/** Use a volume closed-spline graph (full 3D SDF — supports overhangs, caves). */
	Volume UMETA(DisplayName = "Volume"),
};

/**
 * Bridges an AWaterBodyLake to a Voxel Plugin closed-spline stamp.
 *
 * Expects to be attached to an AWaterBodyLake actor with a closed-loop spline.
 * Pick Height or Volume graph kind to match the rest of your voxel terrain pipeline;
 * the corresponding graph slot is used and the other is ignored.
 *
 * The assigned graph is responsible for defining the basin SDF — typically it reads
 * GetSignedDistanceFromClosedSpline2D() and maps it through a depth/falloff curve,
 * analogous to the FalloffSettings + CurveSettings on the water body. The graph's
 * WaterDepth parameter (if defined) is driven by the water body's Channel Depth.
 */
UCLASS(ClassGroup = Voxel, meta = (BlueprintSpawnableComponent, DisplayName = "Lake Voxel Modifier"))
class PGLPCGNODES_API ULakeVoxelModifierComponent : public UWaterVoxelModifierComponent
{
	GENERATED_BODY()

public:
	/** Which graph family to stamp with — Height is lighter, Volume supports full 3D shapes. */
	UPROPERTY(EditAnywhere, Category = "Water Voxel Bridge")
	ELakeVoxelGraphKind GraphKind = ELakeVoxelGraphKind::Height;

	/** Height graph asset — used when GraphKind = Height. */
	UPROPERTY(EditAnywhere, Category = "Water Voxel Bridge", meta = (EditCondition = "GraphKind == ELakeVoxelGraphKind::Height", EditConditionHides))
	TObjectPtr<UVoxelHeightClosedSplineGraph> HeightGraph;

	/** Height layer the stamp is applied to — used when GraphKind = Height. */
	UPROPERTY(EditAnywhere, Category = "Water Voxel Bridge", meta = (EditCondition = "GraphKind == ELakeVoxelGraphKind::Height", EditConditionHides))
	TObjectPtr<UVoxelHeightLayer> HeightLayer;

	/** Volume graph asset — used when GraphKind = Volume. */
	UPROPERTY(EditAnywhere, Category = "Water Voxel Bridge", meta = (EditCondition = "GraphKind == ELakeVoxelGraphKind::Volume", EditConditionHides))
	TObjectPtr<UVoxelVolumeClosedSplineGraph> VolumeGraph;

	/** Volume layer the stamp is applied to — used when GraphKind = Volume. */
	UPROPERTY(EditAnywhere, Category = "Water Voxel Bridge", meta = (EditCondition = "GraphKind == ELakeVoxelGraphKind::Volume", EditConditionHides))
	TObjectPtr<UVoxelVolumeLayer> VolumeLayer;

	/** If true, the water body's channel depth is pushed into the voxel graph parameter named by WaterDepthParameterName. */
	UPROPERTY(EditAnywhere, Category = "Water Voxel Bridge|Parameters")
	bool bMapChannelDepth = true;

	/** Name of the float parameter on the voxel graph that receives the water body's channel depth. */
	UPROPERTY(EditAnywhere, Category = "Water Voxel Bridge|Parameters", meta = (EditCondition = "bMapChannelDepth"))
	FName WaterDepthParameterName = TEXT("WaterDepth");

protected:
	//~ Begin UWaterVoxelModifierComponent Interface
	virtual void BuildStampInternal() override;
	virtual bool WantsClosedLoop() const override { return true; }
	virtual USplineComponent* GetSourceSpline() const override;
	virtual void ApplyParameterOverrides(IVoxelParameterOverridesOwner& Target) const override;
	//~ End UWaterVoxelModifierComponent Interface

private:
	UWaterBodyComponent* GetWaterBodyComponent() const;
};
