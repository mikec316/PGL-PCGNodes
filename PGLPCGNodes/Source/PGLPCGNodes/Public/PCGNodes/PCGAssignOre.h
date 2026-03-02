// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "Engine/DataTable.h"

#include "PCGAssignOre.generated.h"

/**
 * Data table row defining an ore type and its spawn parameters.
 */
USTRUCT(BlueprintType)
struct PGLPCGNODES_API FOreDataTableRow : public FTableRowBase
{
	GENERATED_BODY()

	/** The voxel modifier graph asset for this ore. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ore")
	FSoftObjectPath VoxelGraph;

	/** Spawn weight when the point is on or above the surface. 0 = never spawns on surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "0.0"))
	float SurfaceSpawnRate = 0.0f;

	/** Native grid spacing for surface placement (informational, not used by this node). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "0.0"))
	float SurfaceGridSize = 4800.0f;

	/** Base spawn weight when underground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buried", meta = (ClampMin = "0.0"))
	float BuriedSpawnRate = 1.0f;

	/**
	 * Z range where this ore can exist underground.
	 * X = min depth (deepest, most negative), Y = max depth (shallowest, closest to 0).
	 * Example: (-5000, 0) means the ore spans from Z=-5000 up to Z=0.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buried")
	FVector2D SpawningRange = FVector2D::ZeroVector;

	/**
	 * Normalized 0-1 value within the depth range where spawn rate peaks.
	 * 0.0 = peak at the shallow end, 0.5 = middle, 1.0 = peak at the deepest end.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buried", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxOccurrenceDepth = 0.5f;

	/**
	 * Controls the curve shape around the peak.
	 * Positive = gentle falloff, negative = inverted/sharp.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buried")
	float DepthFalloff = 1.0f;
};

namespace PCGAssignOreConstants
{
	const FName OutputLabel = TEXT("Out");
	const FName DiscardedLabel = TEXT("Discarded");
	const FName OreTypeAttributeName = TEXT("OreType");
	const FName OreVoxelGraphAttributeName = TEXT("OreVoxelGraph");
}

/**
 * Assigns ore types to PCG points based on a data table.
 * Surface points use SurfaceSpawnRate, buried points use depth-weighted BuriedSpawnRate.
 * Each point receives OreType (FName) and OreVoxelGraph (FString) attributes.
 * Points that fail all spawn rate rolls are sent to the Discarded output.
 */
UCLASS(BlueprintType)
class PGLPCGNODES_API UPCGAssignOreSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Filter; }
#endif
	virtual bool UseSeed() const override { return true; }

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** Reference to the ore data table (must use FOreDataTableRow). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
	TObjectPtr<UDataTable> DataTable;

	/** Z value above which a point is considered on the surface. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
	float SurfaceDepthThreshold = 0.0f;
};

class FPCGAssignOreElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual EPCGElementExecutionLoopMode ExecutionLoopMode(const UPCGSettings* Settings) const override { return EPCGElementExecutionLoopMode::SinglePrimaryPin; }
};
