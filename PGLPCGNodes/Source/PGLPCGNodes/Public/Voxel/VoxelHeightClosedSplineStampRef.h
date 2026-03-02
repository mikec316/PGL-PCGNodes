// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelHeightStampRef.h"
#include "Voxel/VoxelHeightClosedSplineStamp.h"
#include "VoxelHeightClosedSplineStampRef.generated.h"

////////////////////////////////////////////////////
///////// The code below is auto-generated /////////
////////////////////////////////////////////////////

USTRUCT(BlueprintType, DisplayName = "Voxel Height Closed Spline Stamp", Category = "Voxel|Stamp|Height Closed Spline", meta = (HasNativeMake = "/Script/PGLPCGNodes.VoxelHeightClosedSplineStamp_K2.Make", HasNativeBreak = "/Script/PGLPCGNodes.VoxelHeightClosedSplineStamp_K2.Break"))
struct PGLPCGNODES_API FVoxelHeightClosedSplineStampRef final : public FVoxelHeightStampRef
{
	GENERATED_BODY()
	GENERATED_VOXEL_STAMP_REF_BODY(FVoxelHeightClosedSplineStampRef, FVoxelHeightClosedSplineStamp)
};

template<>
struct TStructOpsTypeTraits<FVoxelHeightClosedSplineStampRef> : public TStructOpsTypeTraits<FVoxelStampRef>
{
};

template<>
struct TVoxelStampRefImpl<FVoxelHeightClosedSplineStamp>
{
	using Type = FVoxelHeightClosedSplineStampRef;
};

USTRUCT()
struct PGLPCGNODES_API FVoxelHeightClosedSplineInstancedStampRef final : public FVoxelHeightInstancedStampRef
{
	GENERATED_BODY()
	GENERATED_VOXEL_STAMP_REF_BODY(FVoxelHeightClosedSplineInstancedStampRef, FVoxelHeightClosedSplineStamp)
};

template<>
struct TStructOpsTypeTraits<FVoxelHeightClosedSplineInstancedStampRef> : public TStructOpsTypeTraits<FVoxelInstancedStampRef>
{
};

template<>
struct TVoxelInstancedStampRefImpl<FVoxelHeightClosedSplineStamp>
{
	using Type = FVoxelHeightClosedSplineInstancedStampRef;
};