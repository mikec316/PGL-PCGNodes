// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "Voxel/VoxelClosedSplineGraph.h"
#include "Voxel/VoxelOutputNode_OutputClosedSpline.h"

#if WITH_EDITOR
UVoxelGraph::FFactoryInfo UVoxelClosedSplineGraph::GetFactoryInfo()
{
	FFactoryInfo Result;
	Result.Category = "Volume";
	Result.Description = "Used by Stamp actors to create a volume from a closed spline";

	Result.Template = LoadObject<UVoxelClosedSplineGraph>(nullptr,
		TEXT("/PGLPCGNodes/ClosedSplineGraphTemplate.ClosedSplineGraphTemplate"));

	return Result;
}
#endif

UScriptStruct* UVoxelClosedSplineGraph::GetOutputNodeStruct() const
{
	return FVoxelOutputNode_OutputClosedSpline::StaticStruct();
}
