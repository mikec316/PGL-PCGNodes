// Copyright by Procgen Labs Ltd. All Rights Reserved.

// Outputs the corners (Voronoi vertices) of the cellular noise cell
// that each input point belongs to as new child points.
//
// Uses the same hash/grid as TrueDistanceCellularNoise2D so that
// matching Seed, FeatureScale, and Jitter produces corners that
// lie exactly on the cell boundaries of that noise.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelNode.h"
#include "VoxelPointSet.h"
#include "Buffer/VoxelBaseBuffers.h"
#include "VoxelNode_CellularCorners2D.generated.h"

// For each input point, determines which 2D Voronoi cell it belongs to
// and outputs all corners (vertices) of that cell as new child points.
//
// Output point attributes:
//   Position  - world-space Voronoi vertex (Z inherited from parent)
//   Id        - deterministic child ID
//   CornerIndex - integer index of the corner in CCW angular order
//   Parent.*  - all parent attributes replicated
USTRUCT(Category = "Point", DisplayName = "Cellular Corners 2D")
struct PGLPCGNODES_API FVoxelNode_CellularCorners2D : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	// Input point set. Each point's XY position determines which Voronoi cell it is in.
	VOXEL_INPUT_PIN(FVoxelPointSet, In, nullptr);

	// Cell size. Must match the FeatureScale of the corresponding
	// TrueDistanceCellularNoise2D node.
	VOXEL_INPUT_PIN(float, FeatureScale, 100000.f);

	// Jitter amount (0-1). Must match the Jitter of the corresponding
	// TrueDistanceCellularNoise2D node.
	VOXEL_INPUT_PIN(float, Jitter, 0.9f);

	// Noise seed. Must match the Seed of the corresponding
	// TrueDistanceCellularNoise2D node.
	VOXEL_INPUT_PIN(FVoxelSeed, Seed, nullptr, AdvancedDisplay);

	// Output point set containing all Voronoi cell corners.
	VOXEL_OUTPUT_PIN(FVoxelPointSet, Out);

	//~ Begin FVoxelNode Interface
	virtual void Compute(FVoxelGraphQuery Query) const override;
	//~ End FVoxelNode Interface
};
