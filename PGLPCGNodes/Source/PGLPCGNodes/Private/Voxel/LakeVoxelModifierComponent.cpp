// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "Voxel/LakeVoxelModifierComponent.h"

#include "Spline/VoxelVolumeClosedSplineStamp.h"
#include "Spline/VoxelVolumeClosedSplineStampRef.h"
#include "Spline/VoxelHeightClosedSplineStamp.h"
#include "Spline/VoxelHeightClosedSplineStampRef.h"
#include "VoxelParameterOverridesOwner.h"
#include "VoxelStampComponent.h"
#include "WaterBodyActor.h"
#include "WaterBodyComponent.h"
#include "WaterSplineComponent.h"

void ULakeVoxelModifierComponent::BuildStampInternal()
{
	if (!VoxelStamp)
	{
		return;
	}

	switch (GraphKind)
	{
	case ELakeVoxelGraphKind::Height:
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

	case ELakeVoxelGraphKind::Volume:
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

USplineComponent* ULakeVoxelModifierComponent::GetSourceSpline() const
{
	const AWaterBody* const WaterBody = Cast<AWaterBody>(GetOwner());
	return WaterBody ? WaterBody->GetWaterSpline() : nullptr;
}

UWaterBodyComponent* ULakeVoxelModifierComponent::GetWaterBodyComponent() const
{
	const AWaterBody* const WaterBody = Cast<AWaterBody>(GetOwner());
	return WaterBody ? WaterBody->GetWaterBodyComponent() : nullptr;
}

void ULakeVoxelModifierComponent::ApplyParameterOverrides(IVoxelParameterOverridesOwner& Target) const
{
	if (!bMapChannelDepth || WaterDepthParameterName.IsNone())
	{
		return;
	}

	const UWaterBodyComponent* const WaterBodyComp = GetWaterBodyComponent();
	if (!WaterBodyComp)
	{
		return;
	}

	const float ChannelDepth = WaterBodyComp->GetChannelDepth();

	FString Error;
	if (!Target.SetParameter(WaterDepthParameterName, ChannelDepth, &Error))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[%s] Failed to set voxel graph parameter '%s' to channel depth %f: %s"),
			*GetName(), *WaterDepthParameterName.ToString(), ChannelDepth, *Error);
	}
}
