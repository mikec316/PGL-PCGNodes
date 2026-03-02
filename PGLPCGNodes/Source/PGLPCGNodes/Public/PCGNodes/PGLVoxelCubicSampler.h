// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelMinimal.h"
#include "VoxelPCGHelpers.h"
#include "VoxelStackLayer.h"
#include "VoxelMetadataRef.h"
#include "PGLVoxelCubicSampler.generated.h"

class FVoxelLayers;
class UPCGPointData;
class FVoxelSurfaceTypeTable;

// Generates PCG points at the center of each visible cubic voxel face.
// Designed for voxel worlds using blockiness or cubic meshing — points are placed
// on axis-aligned solid/air boundaries with flat normals matching the face direction.
UCLASS(DisplayName = "PGL Voxel Cubic Sampler")
class PGLPCGNODES_API UPGLVoxelCubicSamplerSettings : public UVoxelPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EPCGSettingsType GetType() const override
	{
		return EPCGSettingsType::Sampler;
	}
	virtual EPCGChangeType GetChangeTypeForProperty(const FName& InPropertyName) const override
	{
		return Super::GetChangeTypeForProperty(InPropertyName) | EPCGChangeType::Cosmetic;
	}
#endif

	virtual bool UseSeed() const override
	{
		return true;
	}

	virtual FString GetAdditionalTitleInformation() const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

	//~ Begin UVoxelPCGSettings Interface
	virtual TSharedPtr<FVoxelPCGOutput> CreateOutput(FPCGContext& Context) const override;
	virtual FString GetNodeDebugInfo() const override;
	//~ End UVoxelPCGSettings Interface

public:
	// If no Bounding Shape input is provided, the actor bounds are used.
	// Enable this to generate over the entire surface when no shape is connected.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
	bool bUnbounded = false;

	// Layer to sample
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
	FVoxelStackLayer Layer;

	// Cubic grid cell size in world units. Should match the Voxel World's cubic
	// mesher VoxelSize for points to align with the rendered blocky geometry.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable, ClampMin = 1))
	int32 VoxelSize = 100;

	// Number of cells per processing chunk. Larger values use more memory but may
	// reduce overhead from chunk boundary processing.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", AdvancedDisplay, meta = (PCG_Overridable, ClampMin = 4, ClampMax = 128))
	int32 ChunkSize = 32;

	// Skip faces at the boundary of unsampled (NaN) regions.
	// Disable to include faces even where the distance field is undefined.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", AdvancedDisplay, meta = (PCG_Overridable))
	bool bSkipNaNFaces = true;

	// If false, smart surface types won't be resolved
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", AdvancedDisplay, meta = (PCG_Overridable))
	bool bResolveSmartSurfaceTypes = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
	TArray<TObjectPtr<UVoxelMetadata>> MetadatasToQuery;

	// The LOD to sample
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", AdvancedDisplay, meta = (PCG_Overridable, ClampMin = 0))
	int32 LOD = 0;
};

class FPGLVoxelCubicSamplerOutput : public FVoxelPCGOutput
{
public:
	const TSharedRef<FVoxelLayers> Layers;
	const TSharedRef<FVoxelSurfaceTypeTable> SurfaceTypeTable;
	const FVoxelBox Bounds;

	const FVoxelWeakStackLayer WeakLayer;
	const int32 LOD;
	const int32 VoxelSize;
	const int32 ChunkSize;
	const bool bSkipNaNFaces;
	const bool bResolveSmartSurfaceTypes;
	const TVoxelObjectPtr<UPCGPointData> WeakPointData;
	const TVoxelArray<FVoxelMetadataRef> MetadatasToQuery;

	FPGLVoxelCubicSamplerOutput(
		const TSharedRef<FVoxelLayers>& Layers,
		const TSharedRef<FVoxelSurfaceTypeTable>& SurfaceTypeTable,
		const FVoxelBox& Bounds,
		const FVoxelWeakStackLayer& WeakLayer,
		int32 LOD,
		int32 VoxelSize,
		int32 ChunkSize,
		bool bSkipNaNFaces,
		bool bResolveSmartSurfaceTypes,
		const TVoxelObjectPtr<UPCGPointData>& WeakPointData,
		const TVoxelArray<FVoxelMetadataRef>& MetadatasToQuery)
		: Layers(Layers)
		, SurfaceTypeTable(SurfaceTypeTable)
		, Bounds(Bounds)
		, WeakLayer(WeakLayer)
		, LOD(LOD)
		, VoxelSize(VoxelSize)
		, ChunkSize(ChunkSize)
		, bSkipNaNFaces(bSkipNaNFaces)
		, bResolveSmartSurfaceTypes(bResolveSmartSurfaceTypes)
		, WeakPointData(WeakPointData)
		, MetadatasToQuery(MetadatasToQuery)
	{
	}

public:
	//~ Begin FVoxelPCGOutput Interface
	virtual FVoxelFuture Run() const override;
	//~ End FVoxelPCGOutput Interface
};
