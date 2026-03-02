// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Spline/VoxelSplineStamp.h"
#include "VoxelHeightStamp.h"
#include "VoxelParameterOverridesOwner.h"
#include "Components/SplineComponent.h"
#include "VoxelHeightClosedSplineStamp.generated.h"

class UVoxelHeightClosedSplineGraph;
struct FVoxelSplineMetadataRuntime;
struct FVoxelOutputNode_OutputHeightClosedSpline;

USTRUCT(meta = (ShortName = "Closed Spline", Icon = "ClassIcon.SplineComponent", SortOrder = 3))
struct PGLPCGNODES_API FVoxelHeightClosedSplineStamp final
	: public FVoxelHeightStamp
#if CPP
	, public IVoxelParameterOverridesOwner
#endif
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Config")
	TObjectPtr<UVoxelHeightClosedSplineGraph> Graph;

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
	//~ Begin FVoxelHeightStamp Interface
	virtual UObject* GetAsset() const override;
	virtual void FixupProperties() override;
	virtual void FixupComponents(const IVoxelStampComponentInterface& Interface) override;
	virtual TVoxelArray<TSubclassOf<USceneComponent>> GetRequiredComponents() const override;
	//~ End FVoxelHeightStamp Interface
};

USTRUCT()
struct PGLPCGNODES_API FVoxelHeightClosedSplineStampRuntime : public FVoxelHeightStampRuntime
{
	GENERATED_BODY()
	GENERATED_VOXEL_RUNTIME_STAMP_BODY(FVoxelHeightClosedSplineStamp)

public:
	TVoxelNodeEvaluator<FVoxelOutputNode_OutputHeightClosedSpline> Evaluator;
	float Padding = 0.f;
	FVoxelFloatRange HeightRange;
	bool bRelativeHeightRange = false;
	TSharedPtr<const FVoxelSplineMetadataRuntime> MetadataRuntime;

	bool bClosedLoop = false;
	int32 ReparamStepsPerSegment = 0;
	float SplineLength = 0.f;
	FSplineCurves SplineCurves;

	TVoxelArray<FVoxelSplineSegment> Segments;

	// Single bounding box for the entire spline (all segments + padding + height range)
	FVoxelBox SplineBounds;

public:
	//~ Begin FVoxelHeightStampRuntime Interface
	virtual bool Initialize(FVoxelDependencyCollector& DependencyCollector) override;
	virtual bool Initialize_Parallel(FVoxelDependencyCollector& DependencyCollector) override;
	virtual FVoxelBox GetLocalBounds() const override;
	virtual bool ShouldUseQueryPrevious() const override;
	virtual bool HasRelativeHeightRange() const override;
	virtual TVoxelInlineArray<FVoxelBox, 1> GetChildren() const override;

	virtual void Apply(
		const FVoxelHeightBulkQuery& Query,
		const FVoxelHeightTransform& StampToQuery) const override;

	virtual void Apply(
		const FVoxelHeightSparseQuery& Query,
		const FVoxelHeightTransform& StampToQuery) const override;
	//~ End FVoxelHeightStampRuntime Interface
};
