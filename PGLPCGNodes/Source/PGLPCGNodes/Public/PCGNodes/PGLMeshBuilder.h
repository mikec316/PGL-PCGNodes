// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGNodes/PGLBaseSettings.h"
#include "PVFloatRamp.h"
#include "Engine/Texture2D.h"
#include "PGLMeshBuilder.generated.h"

/**
 * Mesh-builder parameters that mirror PVE's FPVMeshBuilderParams by name,
 * enabling reflection-based property copy at runtime.
 *
 * Fields that reference non-exported PVE types (FPVMaterialSettings) are omitted;
 * the PVE element uses material data from the input collection by default.
 * Transient fields populated by the element itself (PlantProfileOptions,
 * bIsPlantProfileDropdownEnabled, DisplacementValues) are also omitted.
 */
USTRUCT(BlueprintType)
struct FPGLMeshBuilderParams
{
	GENERATED_BODY()

	// ---- Mesh Settings ----

	UPROPERTY(EditAnywhere, Category="Mesh Settings", DisplayName="Point Removal",
		meta=(ClampMin=0.0f, ClampMax=0.1f, UIMin=0.0f, UIMax=0.1f,
			Tooltip="Removes small points to simplify the mesh.\nHigher values remove more points, producing lighter meshes."))
	float PointRemoval = 0.0f;

	UPROPERTY(EditAnywhere, Category="Mesh Settings", DisplayName="Segment Reduction",
		meta=(ClampMin=0.0f, ClampMax=1.0f, UIMin=0.0f, UIMax=1.0f,
			Tooltip="Controls how aggressively segments are reduced.\nHigher values reduce edge count for lighter meshes; lower values preserve detail."))
	float SegmentReduction = 0.0f;

	UPROPERTY(EditAnywhere, Category="Mesh Settings", DisplayName="Min Divisions",
		meta=(ClampMin=3, ClampMax=1024, UIMin=3, UIMax=36,
			Tooltip="Minimum radial divisions.\nThe lowest polygon count around cross-sections."))
	int32 MinMeshDivisions = 6;

	UPROPERTY(EditAnywhere, Category="Mesh Settings", DisplayName="Max Divisions",
		meta=(ClampMin=3, ClampMax=1024, UIMin=3, UIMax=36,
			Tooltip="Maximum radial divisions.\nUpper limit for cross-section detail on trunks/branches."))
	int32 MaxMeshDivisions = 12;

	// ---- Advanced ----

	UPROPERTY(EditAnywhere, Category="Mesh Settings|Advanced", DisplayName="Reduction Accuracy",
		meta=(ClampMin=1, ClampMax=15, UIMin=1, UIMax=15,
			Tooltip="Accuracy level for mesh reduction.\nMore iterations = more accurate but slower."))
	int32 Accuracy = 5;

	UPROPERTY(EditAnywhere, Category="Mesh Settings|Advanced", DisplayName="Longest Segment Allowed (meters)",
		meta=(ClampMin=0.0f, ClampMax=50.0f, UIMin=0.0f, UIMax=10.0f,
			Tooltip="Maximum allowed segment length after simplification."))
	float LongestSegmentLength = 10.0f;

	UPROPERTY(EditAnywhere, Category="Mesh Settings|Advanced", DisplayName="Shortest Segment Allowed (meters)",
		meta=(ClampMin=0.0f, ClampMax=50.0f, UIMin=0.0f, UIMax=1.0f,
			Tooltip="Minimum allowed segment length.\nShorter segments are merged or removed."))
	float ShortestSegmentLength = 0.0f;

	UPROPERTY(EditAnywhere, Category="Mesh Settings|Advanced", DisplayName="Segment Retention Impact",
		meta=(ClampMin=0.0f, ClampMax=5.0f, UIMin=0.0f, UIMax=1.0f,
			Tooltip="Controls how much the retention settings below re-introduce reduced segments."))
	float SegmentRetentionImpact = 1.0f;

	UPROPERTY(EditAnywhere, Category="Mesh Settings|Advanced", DisplayName="Hull Retention",
		meta=(ClampMin=0.0f, ClampMax=1.0f, UIMin=0.0f, UIMax=1.0f,
			Tooltip="Strength of preserving the outer hull silhouette during reduction."))
	float HullRetention = 0.0f;

	UPROPERTY(EditAnywhere, Category="Mesh Settings|Advanced", DisplayName="Main Trunk Retention",
		meta=(ClampMin=0.0f, ClampMax=1.0f, UIMin=0.0f, UIMax=1.0f,
			Tooltip="Preserves detail in the main trunk during simplification."))
	float MainTrunkRetention = 0.0f;

	UPROPERTY(EditAnywhere, Category="Mesh Settings|Advanced", DisplayName="Ground Retention",
		meta=(ClampMin=0.0f, ClampMax=1.0f, UIMin=0.0f, UIMax=1.0f,
			Tooltip="Preserves detail near ground regions during simplification."))
	float GroundRetention = 0.0f;

	UPROPERTY(EditAnywhere, Category="Mesh Settings|Advanced", DisplayName="Scale Retention",
		meta=(ClampMin=0.0f, ClampMax=1.0f, UIMin=0.0f, UIMax=1.0f,
			Tooltip="Preserves detail relative to branch size.\nLarger structures keep more detail."))
	float ScaleRetention = 0.0f;

	UPROPERTY(EditAnywhere, Category="Mesh Settings|Advanced", DisplayName="Hull Retention Gradient",
		meta=(XAxisMin=0.0f, XAxisMax=1.0f, YAxisMin=0.0f, YAxisMax=1.0f,
			Tooltip="Gradient for preserving outer hull detail."))
	FPVFloatRamp HullRetentionGradient;

	UPROPERTY(EditAnywhere, Category="Mesh Settings|Advanced", DisplayName="Main Trunk Retention Gradient",
		meta=(XAxisMin=0.0f, XAxisMax=1.0f, YAxisMin=0.0f, YAxisMax=1.0f,
			Tooltip="Gradient controlling trunk detail retention."))
	FPVFloatRamp MainTrunkRetentionGradient;

	UPROPERTY(EditAnywhere, Category="Mesh Settings|Advanced", DisplayName="Ground Retention Gradient",
		meta=(XAxisMin=0.0f, XAxisMax=1.0f, YAxisMin=0.0f, YAxisMax=1.0f,
			Tooltip="Gradient for preserving ground-region detail."))
	FPVFloatRamp GroundRetentionGradient;

	UPROPERTY(EditAnywhere, Category="Mesh Settings|Advanced", DisplayName="Scale Retention Gradient",
		meta=(XAxisMin=0.0f, XAxisMax=1.0f, YAxisMin=0.0f, YAxisMax=1.0f,
			Tooltip="Gradient controlling size-based retention."))
	FPVFloatRamp ScaleRetentionGradient;

	// ---- Plant Profile ----

	UPROPERTY(EditAnywhere, Category="Plant Profile Settings", DisplayName="Profile",
		meta=(Tooltip="Select a profile (cross-sectional shape) from predefined options.\n'None' applies a simple circular cross-section."))
	FString SelectedPlantProfile = "None";

	UPROPERTY(EditAnywhere, Category="Plant Profile Settings", DisplayName="Profile Falloff",
		meta=(XAxisMin=0.0f, XAxisMax=1.0f, YAxisMin=0.0f, YAxisMax=1.0f,
			Tooltip="Gradient controlling how the profile shape is applied across the length of the trunk and branches."))
	FPVFloatRamp PlantProfileFallOff;

	UPROPERTY(EditAnywhere, Category="Plant Profile Settings", DisplayName="Profile Scale",
		meta=(ClampMin=0.0f, ClampMax=10.0f, UIMin=0.0f, UIMax=10.0f,
			Tooltip="Scale of the profile applied."))
	float PlantProfileScale = 1.0f;

	UPROPERTY(EditAnywhere, Category="Plant Profile Settings",
		meta=(Tooltip="Should the profile be applied to branches or just the trunk."))
	bool bApplyProfileToBranches = false;

	// ---- Displacement ----

	UPROPERTY(EditAnywhere, Category="Displacement Settings",
		meta=(Tooltip="Texture used to drive mesh displacement.\nOnly power-of-2 textures with source texture formats are supported."))
	TObjectPtr<UTexture2D> DisplacementTexture = nullptr;

	UPROPERTY(EditAnywhere, Category="Displacement Settings", DisplayName="Displacement Scale",
		meta=(ClampMin=0.0f, ClampMax=10.0f, UIMin=0.0f, UIMax=1.0f,
			Tooltip="Intensity of displacement effect."))
	float DisplacementStrength = 0.5f;

	UPROPERTY(EditAnywhere, Category="Displacement Settings", DisplayName="Displacement Bias",
		meta=(ClampMin=0.0f, ClampMax=10.0f, UIMin=0.0f, UIMax=10.0f,
			Tooltip="Shifts the midpoint of displacement."))
	float DisplacementBias = 0.0f;

	UPROPERTY(EditAnywhere, Category="Displacement Settings", DisplayName="Displacement UV Scale",
		meta=(Tooltip="Controls tiling of displacement texture."))
	FVector2f DisplacementUVScale = FVector2f(1.0f, 1.0f);

	UPROPERTY(EditAnywhere, Category="Displacement Settings", DisplayName="Apply Displacement up to generation",
		meta=(ClampMin=1, ClampMax=10, UIMin=1, UIMax=10,
			Tooltip="Controls which generations the displacement is applied on."))
	int32 DisplacementGenerationUpperLimit = 1;
};


/**
 * PGL wrapper around PVE's Mesh Builder node.
 *
 * Delegates execution to PVE's FPVMeshBuilderElement at runtime via UClass
 * reflection while propagating FrameMeshPath / FrameMeshTransform through the
 * output collection.
 *
 * Input : UPVGrowthData  (from Growth Generator, Gravity, etc.)
 * Output: UPVMeshData    (geometry collection for downstream Foliage Distributor)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PGLPCGNODES_API UPGLMeshBuilderSettings : public UPGLBaseSettings
{
	GENERATED_BODY()

public:

#if WITH_EDITOR
	virtual FName        GetDefaultNodeName()  const override { return FName(TEXT("PGLMeshBuilder")); }
	virtual FText        GetDefaultNodeTitle() const override;
	virtual FText        GetNodeTooltipText()  const override;
	virtual FLinearColor GetNodeTitleColor()   const override { return FLinearColor(FColor::Cyan); }

	virtual TArray<EPVRenderType> GetDefaultRenderType()    const override { return { EPVRenderType::Mesh }; }
	virtual TArray<EPVRenderType> GetSupportedRenderTypes() const override { return { EPVRenderType::PointData, EPVRenderType::Mesh }; }
#endif

protected:
	virtual FPCGDataTypeIdentifier GetInputPinTypeIdentifier()  const override;
	virtual FPCGDataTypeIdentifier GetOutputPinTypeIdentifier() const override;
	virtual FPCGElementPtr         CreateElement()              const override;

public:

	UPROPERTY(EditAnywhere, Category = "Mesh", meta = (ShowOnlyInnerProperties))
	FPGLMeshBuilderParams MesherSettings;
};


class PGLPCGNODES_API FPGLMeshBuilderElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return false; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
