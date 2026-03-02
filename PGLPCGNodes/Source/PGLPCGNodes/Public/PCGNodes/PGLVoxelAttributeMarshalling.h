// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelMinimal.h"
#include "VoxelBuffer.h"
#include "VoxelPointSet.h"
#include "VoxelPCGUtilities.h"
#include "Buffer/VoxelFloatBuffers.h"
#include "Buffer/VoxelDoubleBuffers.h"
#include "Buffer/VoxelNameBuffer.h"
#include "Buffer/VoxelSoftObjectPathBuffer.h"
#include "VoxelPinType.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

/**
 * Bulk PCG <-> Voxel buffer conversion helpers using FPCGPoint arrays.
 * Separated for reuse across multiple PCG-Voxel bridge nodes.
 *
 * Performance features:
 * - ParallelFor batching for voxel buffer build (faster than sequential)
 * - In-place FPCGPoint modification with pass-through detection (worker thread)
 * - Shared entry keys: metadata writers receive pre-extracted entry key array
 * - Zero intermediate arrays: writers capture buffer refs directly
 */
namespace PGLVoxelMarshalling
{
	/** Batch size for ParallelFor operations on point data. */
	constexpr int32 ParallelBatchSize = 4096;

	/**
	 * Type alias for metadata writers that use pre-extracted entry keys.
	 * Entry keys are extracted ONCE and shared across all writers to avoid
	 * redundant per-attribute extraction.
	 */
	using FMetadataWriter = TFunction<void(
		UPCGMetadata& Metadata,
		const TConstVoxelArrayView<PCGMetadataEntryKey>& EntryKeys)>;

	/**
	 * Build the 9 standard PCG point attributes (Position, Rotation, Scale, Density,
	 * BoundsMin, BoundsMax, Color, Steepness, Id) as voxel buffers from FPCGPoint array.
	 * Decomposes FTransform into separate Position, Rotation, Scale buffers.
	 * Uses ParallelFor for buffer fill — faster than the original's sequential loop.
	 *
	 * Thread-safe (reads only from const FPCGPoint array).
	 */
	PGLPCGNODES_API void BuildStandardAttributes(
		TConstVoxelArrayView<FPCGPoint> Points,
		int32 NumPoints,
		FVoxelPointSet& OutPointSet);

	/**
	 * Write voxel output back into an FPCGPoint array IN-PLACE on a worker thread.
	 * Recomposes Position + Rotation + Scale into FTransform.
	 *
	 * Pass-through detection: Compares output buffer pointers to input buffer pointers.
	 * - Same pointer: property was not modified — skip write (point keeps input value).
	 * - Different pointer: overwrite in ParallelFor.
	 *
	 * If OutputNumPoints != Points.Num(), the array is resized and ALL properties
	 * are written (in-place assumption no longer holds).
	 *
	 * Thread-safe (only modifies the owned FPCGPoint array, no UObject involvement).
	 */
	PGLPCGNODES_API void WriteStandardAttributesToPoints(
		const FVoxelPointSet& InputPointSet,
		const FVoxelPointSet& OutputPointSet,
		TVoxelArray<FPCGPoint>& Points,
		int32 OutputNumPoints);

	/**
	 * Convert a single PCG metadata attribute to a voxel buffer using
	 * FVoxelPCGUtilities::SwitchOnAttribute for type dispatch.
	 * Uses FPCGPoint::MetadataEntry for entry key lookup.
	 * Returns nullptr for unsupported types.
	 */
	PGLPCGNODES_API TSharedPtr<FVoxelBuffer> PCGAttributeToVoxelBuffer(
		const FPCGMetadataAttributeBase& Attribute,
		EPCGMetadataTypes AttributeType,
		TConstVoxelArrayView<FPCGPoint> Points,
		int32 NumPoints,
		const FName& AttributeName,
		const TMap<FName, FVoxelPinType>& AttributeToObjectType);

	/**
	 * Convert a voxel buffer back into a PCG metadata writer lambda.
	 * The writer captures the buffer shared pointer directly (no intermediate TVoxelArray copy).
	 * Receives pre-extracted entry keys from the shared array in the GameTask.
	 * Returns an empty FMetadataWriter for unsupported types.
	 */
	PGLPCGNODES_API FMetadataWriter VoxelBufferToPCGWriter(
		const FName& AttributeName,
		const TSharedPtr<const FVoxelBuffer>& Buffer,
		int32 NumPoints,
		EPCGMetadataTypes OriginalType);
}
