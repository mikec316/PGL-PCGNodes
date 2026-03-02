// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Spline/VoxelSplineStamp.h"
#include "VoxelVolumeStamp.h"
#include "VoxelParameterOverridesOwner.h"
#include "Components/SplineComponent.h"
#include "VoxelClosedSplineStamp.generated.h"

class UVoxelClosedSplineGraph;
struct FVoxelSplineMetadataRuntime;
struct FVoxelOutputNode_OutputClosedSpline;

USTRUCT(meta = (ShortName = "Closed Spline", Icon = "ClassIcon.SplineComponent", SortOrder = 3))
struct PGLPCGNODES_API FVoxelClosedSplineStamp final
	: public FVoxelVolumeStamp
#if CPP
	, public IVoxelParameterOverridesOwner
#endif
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Config")
	TObjectPtr<UVoxelClosedSplineGraph> Graph;

	UPROPERTY()
	FVoxelParameterOverrides ParameterOverrides;

public:
	TSharedPtr<FVoxelGraphEnvironment> CreateEnvironment(
		const FVoxelStampRuntime& Runtime,
		FVoxelDependencyCollector& DependencyCollector) const;

public:
	//~ Begin IVoxelParameterOverridesOwner Interface
	virtual TSharedPtr<IVoxelParameterOverridesOwner> GetShared() const override;
	virtual bool ShouldForceEnableOverride(const FGuid& Guid) const override;
	virtual UVoxelGraph* GetGraph() const override;
	virtual FVoxelParameterOverrides& GetParameterOverrides() override;
	virtual FProperty* GetParameterOverridesProperty() const override;
	//~ End IVoxelParameterOverridesOwner Interface

public:
	//~ Begin FVoxelVolumeStamp Interface
	virtual UObject* GetAsset() const override;
	virtual void FixupProperties() override;
	virtual void FixupComponents(const IVoxelStampComponentInterface& Interface) override;
	virtual TVoxelArray<TSubclassOf<USceneComponent>> GetRequiredComponents() const override;
	//~ End FVoxelVolumeStamp Interface
};

USTRUCT()
struct PGLPCGNODES_API FVoxelClosedSplineStampRuntime : public FVoxelVolumeStampRuntime
{
	GENERATED_BODY()
	GENERATED_VOXEL_RUNTIME_STAMP_BODY(FVoxelClosedSplineStamp)

public:
	TVoxelNodeEvaluator<FVoxelOutputNode_OutputClosedSpline> Evaluator;
	float Padding = 0.f;
	TSharedPtr<const FVoxelSplineMetadataRuntime> MetadataRuntime;

	bool bClosedLoop = false;
	int32 ReparamStepsPerSegment = 0;
	float SplineLength = 0.f;
	FSplineCurves SplineCurves;

	TVoxelArray<FVoxelSplineSegment> Segments;

	// Single bounding box for the entire spline (all segments + padding)
	FVoxelBox SplineBounds;

public:
	//~ Begin FVoxelVolumeStampRuntime Interface
	virtual bool Initialize(FVoxelDependencyCollector& DependencyCollector) override;
	virtual bool Initialize_Parallel(FVoxelDependencyCollector& DependencyCollector) override;
	virtual FVoxelBox GetLocalBounds() const override;
	virtual bool ShouldUseQueryPrevious() const override;
	virtual TVoxelInlineArray<FVoxelBox, 1> GetChildren() const override;

	virtual void Apply(
		const FVoxelVolumeBulkQuery& Query,
		const FVoxelVolumeTransform& StampToQuery) const override;

	virtual void Apply(
		const FVoxelVolumeSparseQuery& Query,
		const FVoxelVolumeTransform& StampToQuery) const override;
	//~ End FVoxelVolumeStampRuntime Interface
};
