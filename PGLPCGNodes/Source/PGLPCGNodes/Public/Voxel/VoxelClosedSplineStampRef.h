// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelVolumeStampRef.h"
#include "Voxel/VoxelClosedSplineStamp.h"
#include "VoxelClosedSplineStampRef.generated.h"

////////////////////////////////////////////////////
///////// The code below is auto-generated /////////
////////////////////////////////////////////////////

USTRUCT(BlueprintType, DisplayName = "Voxel Closed Spline Stamp", Category = "Voxel|Stamp|Closed Spline", meta = (HasNativeMake = "/Script/PGLPCGNodes.VoxelClosedSplineStamp_K2.Make", HasNativeBreak = "/Script/PGLPCGNodes.VoxelClosedSplineStamp_K2.Break"))
struct PGLPCGNODES_API FVoxelClosedSplineStampRef final : public FVoxelVolumeStampRef
{
	GENERATED_BODY()
	GENERATED_VOXEL_STAMP_REF_BODY(FVoxelClosedSplineStampRef, FVoxelClosedSplineStamp)
};

template<>
struct TStructOpsTypeTraits<FVoxelClosedSplineStampRef> : public TStructOpsTypeTraits<FVoxelStampRef>
{
};

template<>
struct TVoxelStampRefImpl<FVoxelClosedSplineStamp>
{
	using Type = FVoxelClosedSplineStampRef;
};

USTRUCT()
struct PGLPCGNODES_API FVoxelClosedSplineInstancedStampRef final : public FVoxelVolumeInstancedStampRef
{
	GENERATED_BODY()
	GENERATED_VOXEL_STAMP_REF_BODY(FVoxelClosedSplineInstancedStampRef, FVoxelClosedSplineStamp)
};

template<>
struct TStructOpsTypeTraits<FVoxelClosedSplineInstancedStampRef> : public TStructOpsTypeTraits<FVoxelInstancedStampRef>
{
};

template<>
struct TVoxelInstancedStampRefImpl<FVoxelClosedSplineStamp>
{
	using Type = FVoxelClosedSplineInstancedStampRef;
};