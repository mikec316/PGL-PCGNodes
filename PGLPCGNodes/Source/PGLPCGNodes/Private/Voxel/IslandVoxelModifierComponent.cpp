// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "Voxel/IslandVoxelModifierComponent.h"

#include "Spline/VoxelVolumeClosedSplineStamp.h"
#include "Spline/VoxelVolumeClosedSplineStampRef.h"
#include "Spline/VoxelHeightClosedSplineStamp.h"
#include "Spline/VoxelHeightClosedSplineStampRef.h"
#include "VoxelStampComponent.h"
#include "WaterBodyIslandActor.h"
#include "WaterSplineComponent.h"

void UIslandVoxelModifierComponent::BuildStampInternal()
{
	if (!VoxelStamp)
	{
		return;
	}

	switch (GraphKind)
	{
	case EIslandVoxelGraphKind::Height:
	{
		if (!HeightGraph)
		{
			return;
		}

		FVoxelHeightClosedSplineStampRef StampRef = FVoxelHeightClosedSplineStampRef::New();
		StampRef->Graph = HeightGraph;
		StampRef->Layer = HeightLayer;
		StampRef->Smoothness = Smoothness;
		StampRef->Priority = Priority;
		StampRef->Transform = FTransform::Identity;

		ApplyParameterOverrides(*StampRef);

		VoxelStamp->SetStamp(StampRef);
		break;
	}

	case EIslandVoxelGraphKind::Volume:
	{
		if (!VolumeGraph)
		{
			return;
		}

		FVoxelVolumeClosedSplineStampRef StampRef = FVoxelVolumeClosedSplineStampRef::New();
		StampRef->Graph = VolumeGraph;
		StampRef->Layer = VolumeLayer;
		StampRef->Smoothness = Smoothness;
		StampRef->Priority = Priority;
		StampRef->Transform = FTransform::Identity;

		ApplyParameterOverrides(*StampRef);

		VoxelStamp->SetStamp(StampRef);
		break;
	}
	}
}

USplineComponent* UIslandVoxelModifierComponent::GetSourceSpline() const
{
	const AWaterBodyIsland* const Island = Cast<AWaterBodyIsland>(GetOwner());
	return Island ? Island->GetWaterSpline() : nullptr;
}
