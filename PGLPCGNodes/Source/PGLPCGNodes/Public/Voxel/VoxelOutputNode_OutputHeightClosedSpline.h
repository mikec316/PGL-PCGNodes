// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "Buffer/VoxelFloatBuffers.h"
#include "VoxelHeightBlendMode.h"
#include "Graphs/VoxelHeightLayerPinType.h"
#include "Nodes/VoxelOutputNode.h"
#include "VoxelOutputNode_OutputHeightClosedSpline.generated.h"

// Output node for the Height Closed Spline graph type.
// Declares the same Height/range/layer/blend pins as FVoxelOutputNode_OutputHeightBase
// but inherits directly from FVoxelOutputNode to avoid cross-module linker issues
// with FVoxelOutputNode_MetadataBase::FDefinition.
USTRUCT(meta = (Placeable))
struct PGLPCGNODES_API FVoxelOutputNode_OutputHeightClosedSpline : public FVoxelOutputNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	VOXEL_INPUT_PIN(FVoxelFloatBuffer, Height, nullptr);

	// Height will be clamped between Min and Max
	VOXEL_INPUT_PIN(FVoxelFloatRange, HeightRange, FVoxelFloatRange(-1000.f, 1000.f));
	// If true and the blend mode is set to Override, HeightRange will be set relative to the previous stamps heights
	VOXEL_INPUT_PIN(bool, RelativeHeightRange, false, AdvancedDisplay);

	// The weight with which to overlay this graph surface and metadatas, over top of the previous state.
	// 0 means entirely the previous state, 1 means entirely this graph's state.
	// Used only with Override blend mode.
	VOXEL_INPUT_PIN(FVoxelFloatBuffer, Alpha, 1.f, AdvancedDisplay);

	VOXEL_INPUT_PIN(bool, EnableLayerOverride, false, AdvancedDisplay);
	VOXEL_INPUT_PIN(FVoxelHeightLayerObject, LayerOverride, nullptr, AdvancedDisplay);
	VOXEL_INPUT_PIN(bool, EnableBlendModeOverride, false, AdvancedDisplay);
	VOXEL_INPUT_PIN(EVoxelHeightBlendMode, BlendModeOverride, EVoxelHeightBlendMode::Override, AdvancedDisplay);

	// How much to enlarge the spline bounding box (in cm) beyond
	// the tight-fitting box around all spline points.
	// Increase this for smoother blending over longer distances.
	VOXEL_INPUT_PIN(float, Padding, 1000.f);
};
