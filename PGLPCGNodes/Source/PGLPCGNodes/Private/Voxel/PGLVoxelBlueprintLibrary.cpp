// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "Voxel/PGLVoxelBlueprintLibrary.h"
#include "Voxel/VoxelPaintCubeVolumeModifier.h"
#include "Sculpt/Volume/VoxelSculptVolume.h"
#include "DrawDebugHelpers.h"

//------------------------------------------------------------------------------
// Helper: snap a position to a uniform grid
//------------------------------------------------------------------------------
static FVector SnapPositionToGrid(const FVector& Position, float GridSize)
{
	return FVector(
		FMath::RoundToFloat(Position.X / GridSize) * GridSize,
		FMath::RoundToFloat(Position.Y / GridSize) * GridSize,
		FMath::RoundToFloat(Position.Z / GridSize) * GridSize
	);
}

//------------------------------------------------------------------------------
// Helper: offset a hit location along the surface normal (add mode only)
//------------------------------------------------------------------------------
static FVector OffsetHitLocation(const FHitResult& Hit, float GridSize, bool bRemoveMode, bool bPaintOnly)
{
	if (!bRemoveMode && !bPaintOnly)
	{
		// Place the cube on top of the surface rather than embedded in it
		const float Offset = GridSize * 0.4f;
		return Hit.Location + (Hit.Normal * Offset);
	}
	// When painting or removing, use the exact hit location
	return Hit.Location;
}

//------------------------------------------------------------------------------
FPGLSculptParams UPGLVoxelBlueprintLibrary::ComputeSculptParamsFromHitResult(
	const FHitResult& StartHitResult,
	const FHitResult& CurrentHitResult,
	float GridSize,
	FVector PresetSculptSize,
	bool bRemoveMode,
	bool bPaintOnly)
{
	FPGLSculptParams Result;

	// Validate inputs
	if (GridSize <= 0.0f)
	{
		GridSize = 50.0f;
	}

	// Need at least the start hit to be valid
	if (!StartHitResult.bBlockingHit)
	{
		return Result;
	}

	// Compute and snap start position
	const FVector StartRaw = OffsetHitLocation(StartHitResult, GridSize, bRemoveMode, bPaintOnly);
	const FVector StartSnapped = SnapPositionToGrid(StartRaw, GridSize);

	// Sculpt mode
	Result.SculptMode = bRemoveMode ? EVoxelSculptMode::Remove : EVoxelSculptMode::Add;

	// If current hit is not valid, treat as single click at start
	if (!CurrentHitResult.bBlockingHit)
	{
		Result.Centre = StartSnapped;
		Result.Size = PresetSculptSize;
		Result.bValid = true;
		return Result;
	}

	// Compute and snap current position
	const FVector CurrentRaw = OffsetHitLocation(CurrentHitResult, GridSize, bRemoveMode, bPaintOnly);
	const FVector CurrentSnapped = SnapPositionToGrid(CurrentRaw, GridSize);

	// Check if the player has actually dragged (positions differ)
	const bool bActuallyDragged = !StartSnapped.Equals(CurrentSnapped, 0.1f);

	if (!bActuallyDragged)
	{
		// Single click – use preset size centred on the snapped position
		Result.Centre = StartSnapped;
		Result.Size = PresetSculptSize;
		Result.bValid = true;
		return Result;
	}

	// ---------- Drag extension logic (mirrors GetSculptBounds) ----------

	const FVector DragDelta(
		FMath::Abs(CurrentSnapped.X - StartSnapped.X),
		FMath::Abs(CurrentSnapped.Y - StartSnapped.Y),
		FMath::Abs(CurrentSnapped.Z - StartSnapped.Z)
	);

	const float DragThreshold = GridSize * 0.5f;
	const bool bDraggedX = DragDelta.X > DragThreshold;
	const bool bDraggedY = DragDelta.Y > DragThreshold;
	const bool bDraggedZ = DragDelta.Z > DragThreshold;

	// For each axis: if dragged, extend; otherwise keep preset size
	Result.Size.X = bDraggedX ? (DragDelta.X + GridSize) : PresetSculptSize.X;
	Result.Size.Y = bDraggedY ? (DragDelta.Y + GridSize) : PresetSculptSize.Y;
	Result.Size.Z = bDraggedZ ? (DragDelta.Z + GridSize) : PresetSculptSize.Z;

	// Centre is the midpoint between start and current
	Result.Centre = (StartSnapped + CurrentSnapped) * 0.5f;
	Result.bValid = true;

	return Result;
}

//------------------------------------------------------------------------------
void UPGLVoxelBlueprintLibrary::DrawSculptPreviewBox(
	const UObject* WorldContext,
	const FPGLSculptParams& SculptParams,
	FLinearColor Color,
	float Thickness,
	float LifeTime)
{
	if (!SculptParams.bValid)
	{
		return;
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return;
	}

	const FVector HalfExtent = SculptParams.Size * 0.5f;

	DrawDebugBox(
		World,
		SculptParams.Centre,
		HalfExtent,
		Color.ToFColor(true),
		false,          // bPersistentLines
		LifeTime,       // LifeTime (0 = one frame)
		0,              // DepthPriority
		Thickness
	);
}

//------------------------------------------------------------------------------
void UPGLVoxelBlueprintLibrary::PaintCube(
	AVoxelSculptVolume* SculptActor,
	const FVector Center,
	const FVector Size,
	const FRotator Rotation,
	const float Strength,
	const EVoxelSculptMode Mode,
	UVoxelSurfaceTypeInterface* SurfaceTypeToPaint,
	const FVoxelMetadataOverrides MetadatasToPaint)
{
	Voxel::ExecuteSynchronously([&]
	{
		return PaintCubeAsync(
			SculptActor,
			Center,
			Size,
			Rotation,
			Strength,
			Mode,
			SurfaceTypeToPaint,
			MetadatasToPaint);
	});
}

//------------------------------------------------------------------------------
FVoxelFuture UPGLVoxelBlueprintLibrary::PaintCubeAsync(
	AVoxelSculptVolume* SculptActor,
	const FVector Center,
	const FVector Size,
	const FRotator Rotation,
	const float Strength,
	const EVoxelSculptMode Mode,
	UVoxelSurfaceTypeInterface* SurfaceTypeToPaint,
	const FVoxelMetadataOverrides MetadatasToPaint)
{
	VOXEL_FUNCTION_COUNTER();

	if (!SculptActor)
	{
		VOXEL_MESSAGE(Error, "Invalid SculptActor");
		return {};
	}

	const TSharedRef<FVoxelPaintCubeVolumeModifier> Modifier = MakeShared<FVoxelPaintCubeVolumeModifier>();
	Modifier->Center = Center;
	Modifier->Size = Size;
	Modifier->Rotation = Rotation;
	Modifier->SurfaceTypeToPaint = SurfaceTypeToPaint;
	Modifier->MetadatasToPaint = MetadatasToPaint;
	Modifier->Strength = Strength;
	Modifier->Mode = Mode;

	return SculptActor->ApplyModifier(Modifier);
}
