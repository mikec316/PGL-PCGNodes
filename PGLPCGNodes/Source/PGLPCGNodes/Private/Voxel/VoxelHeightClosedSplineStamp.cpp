// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "Voxel/VoxelHeightClosedSplineStamp.h"
#include "Voxel/VoxelHeightClosedSplineGraph.h"
#include "Voxel/VoxelOutputNode_OutputHeightClosedSpline.h"
#include "Spline/VoxelSplineMetadata.h"
#include "Spline/VoxelSplineComponent.h"
#include "Spline/VoxelSplineGraphParameters.h"
#include "VoxelStampRef.h"
#include "VoxelGraphTracker.h"
#include "VoxelGraphPositionParameter.h"
#include "VoxelQuery.h"
#include "VoxelStampUtilities.h"
#include "Graphs/VoxelStampGraphParameters.h"

TSharedPtr<FVoxelGraphEnvironment> FVoxelHeightClosedSplineStamp::CreateEnvironment(
	const FVoxelStampRuntime& Runtime,
	FVoxelDependencyCollector& DependencyCollector) const
{
	VOXEL_FUNCTION_COUNTER();

	const TSharedPtr<FVoxelGraphEnvironment> Environment = FVoxelGraphEnvironment::Create(
		Runtime.GetComponent(),
		*this,
		Runtime.GetLocalToWorld(),
		DependencyCollector);

	if (!Environment)
	{
		return nullptr;
	}

	FVoxelGraphParameters::FHeightStamp& Parameter = Environment->AddParameter<FVoxelGraphParameters::FHeightStamp>();
	Parameter.Smoothness = Smoothness;
	Parameter.BlendMode = BlendMode;

	Environment->AddParameter<FVoxelGraphParameters::FStampSeed>(StampSeed.GetSeed());

	return Environment;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedPtr<IVoxelParameterOverridesOwner> FVoxelHeightClosedSplineStamp::GetShared() const
{
	return SharedThis(ConstCast(this));
}

bool FVoxelHeightClosedSplineStamp::ShouldForceEnableOverride(const FGuid& Guid) const
{
	return true;
}

UVoxelGraph* FVoxelHeightClosedSplineStamp::GetGraph() const
{
	return Graph;
}

FVoxelParameterOverrides& FVoxelHeightClosedSplineStamp::GetParameterOverrides()
{
	return ParameterOverrides;
}

FProperty* FVoxelHeightClosedSplineStamp::GetParameterOverridesProperty() const
{
	return &FindFPropertyChecked(FVoxelHeightClosedSplineStamp, ParameterOverrides);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UObject* FVoxelHeightClosedSplineStamp::GetAsset() const
{
	return Graph;
}

void FVoxelHeightClosedSplineStamp::FixupProperties()
{
	Super::FixupProperties();

	FixupParameterOverrides();
}

void FVoxelHeightClosedSplineStamp::FixupComponents(const IVoxelStampComponentInterface& Interface)
{
	VOXEL_FUNCTION_COUNTER();

	if (!Graph)
	{
		return;
	}

	const UVoxelSplineComponent* Component = Interface.FindComponent<UVoxelSplineComponent>();
	if (!Component ||
		!ensure(Component->Metadata))
	{
		return;
	}

	Component->Metadata->Fixup(*Graph);
}

TVoxelArray<TSubclassOf<USceneComponent>> FVoxelHeightClosedSplineStamp::GetRequiredComponents() const
{
	return { UVoxelSplineComponent::StaticClass() };
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool FVoxelHeightClosedSplineStampRuntime::Initialize(FVoxelDependencyCollector& DependencyCollector)
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	if (!Stamp.Graph)
	{
		return false;
	}

	const TSharedPtr<const FVoxelGraphEnvironment> Environment = Stamp.CreateEnvironment(
		*this,
		DependencyCollector);

	if (!Environment)
	{
		return false;
	}

	Evaluator = FVoxelNodeEvaluator::Create<FVoxelOutputNode_OutputHeightClosedSpline>(Environment.ToSharedRef());

	if (!Evaluator)
	{
		return false;
	}

#if WITH_EDITOR
	GVoxelGraphTracker->OnParameterValueChanged(*Stamp.Graph).Add(FOnVoxelGraphChanged::Make(this, [this]
	{
		RequestUpdate();
	}));
#endif

	const UVoxelSplineComponent* Component = FindComponent<UVoxelSplineComponent>();
	if (!Component ||
		Component->GetNumberOfSplinePoints() <= 1 ||
		!ensure(Component->Metadata))
	{
		return false;
	}

	if (!Component->IsClosedLoop())
	{
		VOXEL_MESSAGE(Error, "HeightClosedSpline stamp requires a closed-loop spline");
		return false;
	}

	MetadataRuntime = Component->Metadata->GetRuntime();

	bClosedLoop = true;
	ReparamStepsPerSegment = Component->ReparamStepsPerSegment;
	SplineLength = Component->GetSplineLength();
	SplineCurves = Component->SplineCurves;

	Segments = FVoxelSplineSegment::Create(SplineCurves, *MetadataRuntime);

	return true;
}

bool FVoxelHeightClosedSplineStampRuntime::Initialize_Parallel(FVoxelDependencyCollector& DependencyCollector)
{
	VOXEL_FUNCTION_COUNTER();

	FVoxelGraphContext Context = Evaluator.MakeContext(DependencyCollector);
	FVoxelGraphQueryImpl& GraphQuery = Context.MakeQuery();

	Padding = Evaluator->PaddingPin.GetSynchronous(GraphQuery);
	HeightRange = Evaluator->HeightRangePin.GetSynchronous(GraphQuery);

	bRelativeHeightRange =
		GetBlendMode() == EVoxelHeightBlendMode::Override &&
		Evaluator->RelativeHeightRangePin.GetSynchronous(GraphQuery);

	ensure(
		SplineCurves.Position.LoopKeyOffset == 0.f ||
		SplineCurves.Position.LoopKeyOffset == 1.f);

	for (int32 Index = 0; Index < SplineCurves.Position.Points.Num(); Index++)
	{
		ensure(SplineCurves.Position.Points[Index].InVal == Index);
		ensure(SplineCurves.Rotation.Points[Index].InVal == Index);
		ensure(SplineCurves.Scale.Points[Index].InVal == Index);
	}

	FVoxelHeightClosedSplineStamp& MutableStamp = GetMutableStamp();
	MutableStamp.bDisableEditingLayers = Evaluator->EnableLayerOverridePin.GetSynchronous(GraphQuery);
	MutableStamp.bDisableEditingBlendMode = Evaluator->EnableBlendModeOverridePin.GetSynchronous(GraphQuery);

	if (MutableStamp.bDisableEditingLayers)
	{
		MutableStamp.Layer = Evaluator->LayerOverridePin.GetSynchronous(GraphQuery).Layer.Resolve_Ensured();
		MutableStamp.AdditionalLayers = {};
	}

	if (MutableStamp.bDisableEditingBlendMode)
	{
		MutableStamp.BlendMode = Evaluator->BlendModeOverridePin.GetSynchronous(GraphQuery);
	}

	// Compute a single bounding box from all segment bounds + padding + height range
	if (Segments.Num() > 0)
	{
		SplineBounds = Segments[0].Bounds;
		for (int32 Index = 1; Index < Segments.Num(); Index++)
		{
			SplineBounds = SplineBounds + Segments[Index].Bounds;
		}

		// Extend XY by Padding
		SplineBounds.Min.X -= Padding;
		SplineBounds.Min.Y -= Padding;
		SplineBounds.Max.X += Padding;
		SplineBounds.Max.Y += Padding;

		// Extend Z by HeightRange
		if (bRelativeHeightRange)
		{
			// Relative to previous stamps
			SplineBounds.Min.Z = HeightRange.Min;
			SplineBounds.Max.Z = HeightRange.Max;
		}
		else
		{
			SplineBounds.Min.Z += HeightRange.Min;
			SplineBounds.Max.Z += HeightRange.Max;
		}
	}

	return true;
}

FVoxelBox FVoxelHeightClosedSplineStampRuntime::GetLocalBounds() const
{
	return SplineBounds;
}

bool FVoxelHeightClosedSplineStampRuntime::ShouldUseQueryPrevious() const
{
	return GetBlendMode() == EVoxelHeightBlendMode::Override;
}

bool FVoxelHeightClosedSplineStampRuntime::HasRelativeHeightRange() const
{
	return bRelativeHeightRange;
}

TVoxelInlineArray<FVoxelBox, 1> FVoxelHeightClosedSplineStampRuntime::GetChildren() const
{
	TVoxelInlineArray<FVoxelBox, 1> Result;
	Result.Add(SplineBounds);
	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelHeightClosedSplineStampRuntime::Apply(
	const FVoxelHeightBulkQuery& Query,
	const FVoxelHeightTransform& StampToQuery) const
{
	VOXEL_FUNCTION_COUNTER();

	if (!AffectShape())
	{
		if (ShouldUseQueryPrevious() &&
			ensure(Query.QueryPrevious))
		{
			Query.QueryPrevious->Query(Query);
		}

		return;
	}

	const FVoxelDoubleVector2DBuffer Positions = FVoxelStampUtilities::ComputePositions(
		Query,
		StampToQuery);

	FVoxelGraphContext Context = Evaluator.MakeContext(Query.Query.DependencyCollector);

	FVoxelGraphQueryImpl& GraphQuery = Context.MakeQuery();
	GraphQuery.AddParameter<FVoxelGraphParameters::FLOD>().Value = Query.Query.LOD;
	GraphQuery.AddParameter<FVoxelGraphParameters::FQuery>(Query.Query);
	GraphQuery.AddParameter<FVoxelGraphParameters::FPosition2D>().SetLocalPosition(Positions);
	GraphQuery.AddParameter<FVoxelGraphParameters::FHeightQuery>(Query, StampToQuery);

	{
		FVoxelGraphParameters::FSplineStamp& Parameter = GraphQuery.AddParameter<FVoxelGraphParameters::FSplineStamp>();
		Parameter.bIs2D = true;
		Parameter.bClosedLoop = bClosedLoop;
		Parameter.SplineLength = SplineLength;
		Parameter.ReparamStepsPerSegment = ReparamStepsPerSegment;
		Parameter.SplineCurves = &SplineCurves;
		Parameter.MetadataRuntime = MetadataRuntime.Get();
		Parameter.Segments = Segments;
	}

	FVoxelStampUtilities::ApplyHeights(
		*this,
		Query,
		StampToQuery,
		*Evaluator->HeightPin.GetSynchronous(GraphQuery),
		Positions);
}

void FVoxelHeightClosedSplineStampRuntime::Apply(
	const FVoxelHeightSparseQuery& Query,
	const FVoxelHeightTransform& StampToQuery) const
{
	VOXEL_FUNCTION_COUNTER();

	const FVoxelDoubleVector2DBuffer Positions = FVoxelStampUtilities::ComputePositions(
		Query,
		StampToQuery);

	FVoxelGraphContext Context = Evaluator.MakeContext(Query.Query.DependencyCollector);

	FVoxelGraphQueryImpl& GraphQuery = Context.MakeQuery();
	GraphQuery.AddParameter<FVoxelGraphParameters::FLOD>().Value = Query.Query.LOD;
	GraphQuery.AddParameter<FVoxelGraphParameters::FQuery>(Query.Query);
	GraphQuery.AddParameter<FVoxelGraphParameters::FPosition2D>().SetLocalPosition(Positions);
	GraphQuery.AddParameter<FVoxelGraphParameters::FHeightQuery>(Query, StampToQuery);

	{
		FVoxelGraphParameters::FSplineStamp& Parameter = GraphQuery.AddParameter<FVoxelGraphParameters::FSplineStamp>();
		Parameter.bIs2D = true;
		Parameter.bClosedLoop = bClosedLoop;
		Parameter.SplineLength = SplineLength;
		Parameter.ReparamStepsPerSegment = ReparamStepsPerSegment;
		Parameter.SplineCurves = &SplineCurves;
		Parameter.MetadataRuntime = MetadataRuntime.Get();
		Parameter.Segments = Segments;
	}

	FVoxelStampUtilities::ApplyHeights(
		*this,
		Query,
		StampToQuery,
		*Evaluator->HeightPin.GetSynchronous(GraphQuery),
		Positions,
		true,
		nullptr);
}
