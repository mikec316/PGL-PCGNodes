// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "Voxel/VoxelHeightClosedSplineGraph.h"
#include "Voxel/VoxelOutputNode_OutputHeightClosedSpline.h"

#if WITH_EDITOR
UVoxelGraph::FFactoryInfo UVoxelHeightClosedSplineGraph::GetFactoryInfo()
{
	FFactoryInfo Result;
	Result.Category = "Height";
	Result.Description =
		"Used by Stamp actors to create a height shape from a closed spline. "
		"This uses XY positions and outputs height, so it cannot produce overhangs or caves.";

	Result.Template = LoadObject<UVoxelHeightClosedSplineGraph>(nullptr,
		TEXT("/PGLPCGNodes/HeightClosedSplineGraphTemplate.HeightClosedSplineGraphTemplate"));

	return Result;
}
#endif

UScriptStruct* UVoxelHeightClosedSplineGraph::GetOutputNodeStruct() const
{
	return FVoxelOutputNode_OutputHeightClosedSpline::StaticStruct();
}
