// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VoxelStackLayer.h"
#include "PGLSurfacePaintPreset.generated.h"

class UVoxelSurfaceTypeInterface;

/**
 * Primary Data Asset that defines a complete paint preset
 * Includes surface type, sculpt size multiplier, and layer information
 */
UCLASS(BlueprintType)
class PGLPCGNODESEDITOR_API UPGLSurfacePaintPreset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The surface type to paint when this preset is active */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface")
	TObjectPtr<UVoxelSurfaceTypeInterface> SurfaceType;

	/** Sculpt size override (replaces default 50,50,50). X=forward/back, Y=left/right, Z=up/down. X and Y swap based on player rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sculpt", meta = (DisplayName = "Sculpt Size Relative"))
	FVector SculptSizeRelative = FVector(50.0f, 50.0f, 50.0f);

	/** Volume layer to write to when sculpting (optional) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layer")
	FVoxelStackVolumeLayer VolumeLayer;

	/** Display name for this preset (shown in palette if set, otherwise uses asset name) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText DisplayName;

	/** Optional color tint for the palette button to help identify this preset visually */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FLinearColor ButtonTint = FLinearColor::White;

	// UPrimaryDataAsset interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId("PGLSurfacePaintPreset", GetFName());
	}

	/** Returns the display name, falling back to asset name if not set */
	FText GetDisplayName() const
	{
		if (!DisplayName.IsEmpty())
		{
			return DisplayName;
		}
		return FText::FromName(GetFName());
	}
};
