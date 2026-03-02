// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Sculpt/Volume/VoxelSculptVolume.h"
#include "Sculpt/VoxelSculptMode.h"
#include "VoxelMetadataOverrides.h"
#include "PGLVoxelBlueprintLibrary.generated.h"

class AVoxelSculptVolume;
class UVoxelSurfaceTypeInterface;

/**
 * Output struct for ComputeSculptParamsFromHitResult
 */
USTRUCT(BlueprintType)
struct PGLPCGNODES_API FPGLSculptParams
{
	GENERATED_BODY()

	/** Center position for the sculpt operation (grid-snapped) */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpt")
	FVector Centre = FVector::ZeroVector;

	/** Size of the sculpt cube */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpt")
	FVector Size = FVector(50.0f, 50.0f, 50.0f);

	/** Sculpt mode (Add or Remove) */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpt")
	EVoxelSculptMode SculptMode = EVoxelSculptMode::Add;

	/** Whether the computation was valid (hit result was blocking) */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpt")
	bool bValid = false;
};

/**
 * Blueprint library for extended voxel operations (runtime-safe)
 */
UCLASS()
class PGLPCGNODES_API UPGLVoxelBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Compute sculpt parameters from a hit result, replicating the grid-snapping
	 * and surface-offset logic from the editor scriptable tool.
	 *
	 * For single-click placement: pass the same hit result for both Start and Current.
	 * For click-drag extension:   pass the initial click hit as Start and the current
	 *                             drag hit as Current. Axes that are dragged beyond the
	 *                             threshold will extend; un-dragged axes keep the preset size.
	 *
	 * @param StartHitResult     Hit result from the initial click (line trace)
	 * @param CurrentHitResult   Hit result from the current mouse position (same as Start for single click)
	 * @param GridSize           Grid size used for snapping (must match your voxel grid, e.g. 50)
	 * @param PresetSculptSize   Default cube size per axis before any drag extension (e.g. 50,50,50)
	 * @param bRemoveMode        True = Remove sculpt mode (like holding Shift in the editor tool)
	 * @param bPaintOnly         True = paint-only mode (hit location is not offset along normal)
	 * @return                   Computed sculpt parameters (Centre, Size, SculptMode, bValid)
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Sculpt", meta = (DisplayName = "Compute Sculpt Params From Hit Result"))
	static FPGLSculptParams ComputeSculptParamsFromHitResult(
		const FHitResult& StartHitResult,
		const FHitResult& CurrentHitResult,
		float GridSize = 50.0f,
		FVector PresetSculptSize = FVector(50.0f, 50.0f, 50.0f),
		bool bRemoveMode = false,
		bool bPaintOnly = false);

	/**
	 * Draw a debug wireframe box for the given sculpt params.
	 * Call this every frame (e.g. from Tick) to keep the preview visible.
	 *
	 * @param WorldContext        Any world-context object (self, actor, etc.)
	 * @param SculptParams        The params returned by ComputeSculptParamsFromHitResult
	 * @param Color               Wireframe color (green = add, red = remove is a good convention)
	 * @param Thickness           Line thickness
	 * @param LifeTime            How long the lines persist (0 = one frame, use 0 when calling every tick)
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Sculpt", meta = (WorldContext = "WorldContext", DisplayName = "Draw Sculpt Preview Box"))
	static void DrawSculptPreviewBox(
		const UObject* WorldContext,
		const FPGLSculptParams& SculptParams,
		FLinearColor Color = FLinearColor::Green,
		float Thickness = 2.0f,
		float LifeTime = 0.0f);

	/**
	 * Paint a surface type on a cube-shaped volume (synchronous blueprint wrapper)
	 * @param SculptActor        The sculpt actor to paint
	 * @param Center             Center, in world space
	 * @param Size               Size of the cube, in world space
	 * @param Rotation           Rotation, in world space
	 * @param Strength           Strength, usually between 0 and 1
	 * @param Mode               Add or remove surface type
	 * @param SurfaceTypeToPaint Surface type to paint
	 * @param MetadatasToPaint   Metadatas to paint
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Sculpt")
	static void PaintCube(
		UPARAM(Required) AVoxelSculptVolume* SculptActor,
		const FVector Center,
		const FVector Size = FVector(1000.f),
		const FRotator Rotation = FRotator::ZeroRotator,
		const float Strength = 1.0f,
		const EVoxelSculptMode Mode = EVoxelSculptMode::Add,
		UVoxelSurfaceTypeInterface* SurfaceTypeToPaint = nullptr,
		const FVoxelMetadataOverrides MetadatasToPaint = FVoxelMetadataOverrides());

	/** C++ only: Paint a cube and return the voxel future for chaining */
	static FVoxelFuture PaintCubeAsync(
		AVoxelSculptVolume* SculptActor,
		const FVector Center,
		const FVector Size = FVector(1000.f),
		const FRotator Rotation = FRotator::ZeroRotator,
		const float Strength = 1.0f,
		const EVoxelSculptMode Mode = EVoxelSculptMode::Add,
		UVoxelSurfaceTypeInterface* SurfaceTypeToPaint = nullptr,
		const FVoxelMetadataOverrides MetadatasToPaint = FVoxelMetadataOverrides());
};
