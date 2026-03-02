// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Buffer/VoxelFloatBuffers.h"
#include "VoxelVolumeBlendMode.h"
#include "Graphs/VoxelVolumeLayerPinType.h"
#include "Nodes/VoxelOutputNode.h"
#include "VoxelOutputNode_OutputClosedSpline.generated.h"

// Output node for the Closed Spline graph type.
// Declares the same Distance/layer/blend pins as FVoxelOutputNode_OutputVolumeBase
// but inherits directly from FVoxelOutputNode to avoid cross-module linker issues
// with FVoxelOutputNode_MetadataBase::FDefinition.
USTRUCT(meta = (Placeable))
struct PGLPCGNODES_API FVoxelOutputNode_OutputClosedSpline : public FVoxelOutputNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_INPUT_PIN(FVoxelFloatBuffer, Distance, nullptr);

	VOXEL_INPUT_PIN(bool, EnableLayerOverride, false, AdvancedDisplay);
	VOXEL_INPUT_PIN(FVoxelVolumeLayerObject, LayerOverride, nullptr, AdvancedDisplay);
	VOXEL_INPUT_PIN(bool, EnableBlendModeOverride, false, AdvancedDisplay);
	VOXEL_INPUT_PIN(EVoxelVolumeBlendMode, BlendModeOverride, EVoxelVolumeBlendMode::Override, AdvancedDisplay);

	// How much to enlarge the spline bounding box (in cm) beyond
	// the tight-fitting box around all spline points.
	// Increase this for smoother blending over longer distances.
	VOXEL_INPUT_PIN(float, Padding, 1000.f);
};
