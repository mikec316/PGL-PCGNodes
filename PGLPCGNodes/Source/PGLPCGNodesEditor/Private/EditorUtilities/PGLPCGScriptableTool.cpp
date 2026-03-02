// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "EditorUtilities/PGLPCGScriptableTool.h"
#include "InteractiveToolManager.h"
#include "ToolBuilderUtil.h"
#include "BaseBehaviors/ClickDragBehavior.h"
#include "Drawing/PreviewGeometryActor.h"
#include "Drawing/LineSetComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "SceneManagement.h"
#include "Sculpt/Volume/VoxelSculptVolume.h"
#include "Sculpt/Volume/VoxelVolumeSculptBlueprintLibrary.h"
#include "Sculpt/Volume/VoxelVolumeSculptStamp.h"
#include "VoxelLayerStack.h"
#include "Voxel/PGLVoxelBlueprintLibrary.h"

#define LOCTEXT_NAMESPACE "PGLPCGScriptableTool"

//------------------------------------------------------------------------------
// UPGLPCGScriptableToolProperties
//------------------------------------------------------------------------------

void UPGLPCGScriptableToolProperties::PostInitProperties()
{
	Super::PostInitProperties();

	// Load presets from saved paths after config is loaded
	LoadPresetsFromConfig();
}

void UPGLPCGScriptableToolProperties::SaveConfig(uint64 Flags, const TCHAR* Filename)
{
	// Save current palette to paths before saving config
	SavePresetsToConfig();

	Super::SaveConfig(Flags, Filename);
}

void UPGLPCGScriptableToolProperties::LoadPresetsFromConfig()
{
	// Clear current palette
	PaintPresetPalette.Empty();

	// Load presets from saved paths
	for (const FSoftObjectPath& PresetPath : SavedPresetPaths)
	{
		if (PresetPath.IsValid())
		{
			UPGLSurfacePaintPreset* Preset = Cast<UPGLSurfacePaintPreset>(PresetPath.TryLoad());
			if (Preset)
			{
				PaintPresetPalette.Add(Preset);
			}
		}
	}

	// Clamp active index to valid range
	if (ActivePaletteIndex >= PaintPresetPalette.Num())
	{
		ActivePaletteIndex = FMath::Max(0, PaintPresetPalette.Num() - 1);
	}
}

void UPGLPCGScriptableToolProperties::SavePresetsToConfig()
{
	// Clear saved paths
	SavedPresetPaths.Empty();

	// Convert current palette to paths
	for (UPGLSurfacePaintPreset* Preset : PaintPresetPalette)
	{
		if (Preset)
		{
			SavedPresetPaths.Add(FSoftObjectPath(Preset));
		}
	}
}

//------------------------------------------------------------------------------
// UPGLPCGScriptableToolBuilder
//------------------------------------------------------------------------------

bool UPGLPCGScriptableToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	// Tool can always be activated in the editor
	return true;
}

UInteractiveTool* UPGLPCGScriptableToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UPGLPCGScriptableTool* NewTool = NewObject<UPGLPCGScriptableTool>(SceneState.ToolManager);
	return NewTool;
}

//------------------------------------------------------------------------------
// UPGLPCGScriptableTool
//------------------------------------------------------------------------------

void UPGLPCGScriptableTool::Setup()
{
	// Enable hover events for preview cube display BEFORE calling Super::Setup()
	// This is critical - the parent class reads this flag during Setup()
	bWantMouseHover = true;

	Super::Setup();

	// Create and register property set with config loading
	ToolProperties = NewObject<UPGLPCGScriptableToolProperties>(this);
	ToolProperties->LoadConfig();  // Explicitly load config
	AddToolPropertySource(ToolProperties);

	// Create preview geometry for visualizing the sculpt cube
	PreviewGeometry = NewObject<UPreviewGeometry>(this);
	PreviewGeometry->CreateInWorld(GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld(), FTransform::Identity);

	// Initialize drag state
	bIsDragging = false;
	bHasValidHoverPosition = false;
	DragStartPosition = FVector::ZeroVector;
	DragCurrentPosition = FVector::ZeroVector;
	HoverPosition = FVector::ZeroVector;

	// Create the preview cube geometry immediately
	const FString LineSetID = TEXT("SculptPreviewCube");
	PreviewGeometry->AddLineSet(LineSetID);
}

void UPGLPCGScriptableTool::Shutdown(EToolShutdownType ShutdownType)
{
	// Save config before shutdown
	if (ToolProperties)
	{
		ToolProperties->SaveConfig();
	}

	// Clean up preview geometry
	if (PreviewGeometry)
	{
		PreviewGeometry->Disconnect();
		PreviewGeometry = nullptr;
	}

	Super::Shutdown(ShutdownType);
}

void UPGLPCGScriptableTool::OnTick(float DeltaTime)
{
	Super::OnTick(DeltaTime);

	// Always update preview visualization every tick
	UpdatePreviewCube();
}

void UPGLPCGScriptableTool::OnDragBegin_Implementation(FInputDeviceRay StartPosition, const FScriptableToolModifierStates& Modifiers)
{
	// Store modifiers first so GetWorldHitLocation can check them
	CurrentModifiers = Modifiers;

	// Raycast to find world hit location
	FVector HitLocation;
	if (GetWorldHitLocation(StartPosition, HitLocation))
	{
		// Snap to grid
		DragStartPosition = SnapToGrid(HitLocation);
		DragCurrentPosition = DragStartPosition;
		bIsDragging = true;

		// Initialize preview cube at the snapped location
		UpdatePreviewCube();
	}
}

void UPGLPCGScriptableTool::OnDragUpdatePosition_Implementation(FInputDeviceRay NewPosition, const FScriptableToolModifierStates& Modifiers)
{
	if (bIsDragging)
	{
		// Update current drag position and modifiers
		FVector HitLocation;
		if (GetWorldHitLocation(NewPosition, HitLocation))
		{
			DragCurrentPosition = SnapToGrid(HitLocation);
			CurrentModifiers = Modifiers;
			// Preview cube will be updated in OnTick
		}
	}
}

void UPGLPCGScriptableTool::OnDragEnd_Implementation(FInputDeviceRay EndPosition, const FScriptableToolModifierStates& Modifiers)
{
	if (bIsDragging)
	{
		// Get the target voxel sculpt volume (either selected or auto-found)
		AVoxelSculptVolume* SculptVolume = GetTargetSculptVolume();
		if (!SculptVolume)
		{
			UE_LOG(LogTemp, Warning, TEXT("PGLPCGScriptableTool: No Voxel Sculpt Volume available"));
			bIsDragging = false;
			return;
		}

		// Calculate the sculpt bounds BEFORE clearing bIsDragging
		// (GetSculptBounds needs bIsDragging to be true to use drag positions)
		// GetSculptBounds handles all rotation logic internally
		FVector Center, Size;
		GetSculptBounds(Center, Size);

		// Now we can end the drag operation
		bIsDragging = false;

		// Check if Shift is held to determine sculpt mode
		const bool bIsRemoving = Modifiers.bShiftDown;
		const EVoxelSculptMode SculptMode = bIsRemoving ? EVoxelSculptMode::Remove : EVoxelSculptMode::Add;

		// Sculpt the cube (unless in paint-only mode)
		if (!ToolProperties->bPaintOnly)
		{
			UVoxelVolumeSculptBlueprintLibrary::SculptCube(
				SculptVolume,
				Center,
				Size,
				FRotator::ZeroRotator,  // No rotation for axis-aligned cubes
				0.0f,  // Roundness - 0 for sharp corners
				SculptMode,
				ToolProperties->SculptSmoothness
			);
		}

		// Get active surface type from palette
		UVoxelSurfaceTypeInterface* ActiveSurfaceType = ToolProperties->GetActiveSurfaceType();

		// Paint the cube if a surface type is selected AND we're not removing
		if (ActiveSurfaceType && !bIsRemoving)
		{
			// First, remove any existing paint in this area (use full strength to clear)
			UPGLVoxelBlueprintLibrary::PaintCube(
				SculptVolume,
				Center,
				Size,
				FRotator::ZeroRotator,
				1.0f,  // Full strength for removal
				EVoxelSculptMode::Remove,  // Remove existing paint
				ActiveSurfaceType,
				FVoxelMetadataOverrides()
			);

			// Then apply the new paint
			UPGLVoxelBlueprintLibrary::PaintCube(
				SculptVolume,
				Center,
				Size,
				FRotator::ZeroRotator,  // No rotation for axis-aligned cubes
				ToolProperties->PaintStrength,
				EVoxelSculptMode::Add,  // Add surface type when painting
				ActiveSurfaceType,
				FVoxelMetadataOverrides()  // No metadata overrides
			);
		}
		else if (ToolProperties->bPaintOnly && !ActiveSurfaceType)
		{
			UE_LOG(LogTemp, Warning, TEXT("PGLPCGScriptableTool: Paint Only mode requires an active surface type in palette"));
		}

		// Don't clear preview - keep it visible
	}
}

FInputRayHit UPGLPCGScriptableTool::OnHoverHitTest_Implementation(FInputDeviceRay HoverPos, const FScriptableToolModifierStates& Modifiers)
{
	FInputRayHit Hit;
	FVector HitLocation;
	if (GetWorldHitLocation(HoverPos, HitLocation))
	{
		Hit.bHit = true;
		Hit.HitDepth = FVector::Distance(HoverPos.WorldRay.Origin, HitLocation);
	}
	return Hit;
}

bool UPGLPCGScriptableTool::OnHoverUpdate_Implementation(FInputDeviceRay HoverPos, const FScriptableToolModifierStates& Modifiers)
{
	if (!bIsDragging)
	{
		FVector HitLocation;
		if (GetWorldHitLocation(HoverPos, HitLocation))
		{
			HoverPosition = SnapToGrid(HitLocation);
			bHasValidHoverPosition = true;
			CurrentModifiers = Modifiers;
		}
		else
		{
			bHasValidHoverPosition = false;
		}
	}

	return true; // Continue hover
}

bool UPGLPCGScriptableTool::GetWorldHitLocation(const FInputDeviceRay& Ray, FVector& OutHitLocation) const
{
	UWorld* World = GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld();
	if (!World)
	{
		return false;
	}

	FVector RayStart = Ray.WorldRay.Origin;
	FVector RayEnd = Ray.WorldRay.Origin + (Ray.WorldRay.Direction * 100000.0f); // 100000 units max distance

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;

	// Perform line trace
	if (World->LineTraceSingleByChannel(HitResult, RayStart, RayEnd, ECC_Visibility, QueryParams))
	{
		// Only offset when adding new cubes (not when painting or removing)
		const bool bIsRemoving = CurrentModifiers.bShiftDown;
		const bool bIsPaintingOnly = ToolProperties->bPaintOnly;

		if (!bIsRemoving && !bIsPaintingOnly)
		{
			// Offset the hit location along the surface normal by slightly less than half grid size
			// This places the cube on top of the surface rather than embedded in it
			// Using 0.4 instead of 0.5 to ensure better alignment
			const float Offset = ToolProperties->GridSize * 0.4f;
			OutHitLocation = HitResult.Location + (HitResult.Normal * Offset);
		}
		else
		{
			// When painting or removing, use the exact hit location (no offset)
			// This allows targeting existing cubes precisely
			OutHitLocation = HitResult.Location;
		}

		return true;
	}

	return false;
}

FVector UPGLPCGScriptableTool::SnapToGrid(const FVector& Position) const
{
	const float Grid = ToolProperties->GridSize;

	return FVector(
		FMath::RoundToFloat(Position.X / Grid) * Grid,
		FMath::RoundToFloat(Position.Y / Grid) * Grid,
		FMath::RoundToFloat(Position.Z / Grid) * Grid
	);
}

void UPGLPCGScriptableTool::UpdatePreviewCube()
{
	if (!PreviewGeometry)
	{
		return;
	}

	const FString LineSetID = TEXT("SculptPreviewCube");
	ULineSetComponent* LineSet = PreviewGeometry->FindLineSet(LineSetID);

	if (!LineSet)
	{
		PreviewGeometry->AddLineSet(LineSetID);
		LineSet = PreviewGeometry->FindLineSet(LineSetID);
		if (!LineSet)
		{
			return;
		}
	}

	// If we don't have a valid position, clear and return
	if (!bIsDragging && !bHasValidHoverPosition)
	{
		LineSet->Clear();
		return;
	}

	// Calculate the bounds of the preview cube
	FVector Center, Size;
	GetSculptBounds(Center, Size);

	if (!LineSet)
	{
		return;
	}

	LineSet->Clear();

	// Calculate the 8 corners of the cube
	FVector HalfSize = Size * 0.5f;
	FVector Corners[8];
	Corners[0] = Center + FVector(-HalfSize.X, -HalfSize.Y, -HalfSize.Z);
	Corners[1] = Center + FVector(HalfSize.X, -HalfSize.Y, -HalfSize.Z);
	Corners[2] = Center + FVector(HalfSize.X, HalfSize.Y, -HalfSize.Z);
	Corners[3] = Center + FVector(-HalfSize.X, HalfSize.Y, -HalfSize.Z);
	Corners[4] = Center + FVector(-HalfSize.X, -HalfSize.Y, HalfSize.Z);
	Corners[5] = Center + FVector(HalfSize.X, -HalfSize.Y, HalfSize.Z);
	Corners[6] = Center + FVector(HalfSize.X, HalfSize.Y, HalfSize.Z);
	Corners[7] = Center + FVector(-HalfSize.X, HalfSize.Y, HalfSize.Z);

	// Draw the 12 edges of the cube
	// Choose color based on mode:
	// - Blue: Paint Only mode
	// - Red: Remove mode (Shift held)
	// - Green: Normal Add mode
	FColor PreviewColor;
	if (ToolProperties->bPaintOnly)
	{
		PreviewColor = FColor::Blue;
	}
	else if (CurrentModifiers.bShiftDown)
	{
		PreviewColor = FColor::Red;
	}
	else
	{
		PreviewColor = FColor::Green;
	}
	float LineThickness = 2.0f;

	// Bottom face
	LineSet->AddLine(Corners[0], Corners[1], PreviewColor, LineThickness);
	LineSet->AddLine(Corners[1], Corners[2], PreviewColor, LineThickness);
	LineSet->AddLine(Corners[2], Corners[3], PreviewColor, LineThickness);
	LineSet->AddLine(Corners[3], Corners[0], PreviewColor, LineThickness);

	// Top face
	LineSet->AddLine(Corners[4], Corners[5], PreviewColor, LineThickness);
	LineSet->AddLine(Corners[5], Corners[6], PreviewColor, LineThickness);
	LineSet->AddLine(Corners[6], Corners[7], PreviewColor, LineThickness);
	LineSet->AddLine(Corners[7], Corners[4], PreviewColor, LineThickness);

	// Vertical edges
	LineSet->AddLine(Corners[0], Corners[4], PreviewColor, LineThickness);
	LineSet->AddLine(Corners[1], Corners[5], PreviewColor, LineThickness);
	LineSet->AddLine(Corners[2], Corners[6], PreviewColor, LineThickness);
	LineSet->AddLine(Corners[3], Corners[7], PreviewColor, LineThickness);
}

void UPGLPCGScriptableTool::GetSculptBounds(FVector& OutCenter, FVector& OutSize) const
{
	// Get preset size from active palette preset
	FVector PresetSize = ToolProperties->GetActiveSculptSize();

	// Get player camera rotation to determine facing direction
	UWorld* World = GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	FRotator CameraRotation = FRotator::ZeroRotator;
	if (PC && PC->PlayerCameraManager)
	{
		CameraRotation = PC->PlayerCameraManager->GetCameraRotation();
	}

	// Determine if we need to swap X and Y based on camera yaw
	// Camera facing North/South (90°/270°): swap X/Y so forward dimension aligns with view
	float Yaw = FMath::Fmod(CameraRotation.Yaw + 360.0f, 360.0f);
	bool bSwapXY = (Yaw >= 45.0f && Yaw < 135.0f) || (Yaw >= 225.0f && Yaw < 315.0f);

	// Apply Ctrl rotation if held (additional 90° rotation)
	if (CurrentModifiers.bCtrlDown)
	{
		bSwapXY = !bSwapXY;  // Toggle the swap
	}

	// Apply camera-based rotation to preset size
	FVector RotatedPresetSize;
	if (bSwapXY)
	{
		RotatedPresetSize.X = PresetSize.Y;
		RotatedPresetSize.Y = PresetSize.X;
	}
	else
	{
		RotatedPresetSize.X = PresetSize.X;
		RotatedPresetSize.Y = PresetSize.Y;
	}
	RotatedPresetSize.Z = PresetSize.Z;

	// Determine size and center based on drag state
	if (bIsDragging)
	{
		// Check if this is actually a drag (positions differ) or just a click
		const bool bActuallyDragged = !DragStartPosition.Equals(DragCurrentPosition, 0.1f);

		if (bActuallyDragged)
		{
			// Calculate the drag delta for each axis
			FVector DragDelta = FVector(
				FMath::Abs(DragCurrentPosition.X - DragStartPosition.X),
				FMath::Abs(DragCurrentPosition.Y - DragStartPosition.Y),
				FMath::Abs(DragCurrentPosition.Z - DragStartPosition.Z)
			);

			// Detect which axes were actually dragged (threshold of half grid size to avoid noise)
			const float DragThreshold = ToolProperties->GridSize * 0.5f;
			const bool bDraggedX = DragDelta.X > DragThreshold;
			const bool bDraggedY = DragDelta.Y > DragThreshold;
			const bool bDraggedZ = DragDelta.Z > DragThreshold;

			// For each axis: if dragged, use dragged size; otherwise use preset size
			if (bDraggedX)
			{
				OutSize.X = DragDelta.X + ToolProperties->GridSize;
			}
			else
			{
				OutSize.X = RotatedPresetSize.X;
			}

			if (bDraggedY)
			{
				OutSize.Y = DragDelta.Y + ToolProperties->GridSize;
			}
			else
			{
				OutSize.Y = RotatedPresetSize.Y;
			}

			if (bDraggedZ)
			{
				OutSize.Z = DragDelta.Z + ToolProperties->GridSize;
			}
			else
			{
				OutSize.Z = RotatedPresetSize.Z;
			}

			// Center is midpoint between start and current
			OutCenter = (DragStartPosition + DragCurrentPosition) * 0.5f;
		}
		else
		{
			// Single click - use preset size
			OutSize = RotatedPresetSize;
			OutCenter = DragStartPosition;
		}
	}
	else
	{
		// Hover preview - use preset size
		OutSize = RotatedPresetSize;
		OutCenter = HoverPosition;
	}
}

AVoxelSculptVolume* UPGLPCGScriptableTool::FindVolumeForLayer(const FVoxelStackVolumeLayer& VolumeLayer) const
{
	UWorld* World = GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld();
	if (!World)
	{
		return nullptr;
	}

	// Check if we have a valid layer to search for
	if (!VolumeLayer.Stack || !VolumeLayer.Layer)
	{
		UE_LOG(LogTemp, Warning, TEXT("FindVolumeForLayer: Invalid VolumeLayer (Stack or Layer is null)"));
		return nullptr;
	}

	// Iterate through all VoxelSculptVolume actors in the level
	for (TActorIterator<AVoxelSculptVolume> It(World); It; ++It)
	{
		AVoxelSculptVolume* Volume = *It;
		if (!Volume)
		{
			continue;
		}

		// Get the sculpt stamp from the volume
		FVoxelVolumeSculptStampRef StampRef = Volume->GetStamp();
		FVoxelVolumeSculptStamp* Stamp = StampRef.operator->();

		if (!Stamp || !Stamp->Layer)
		{
			continue;
		}

		// Check if the layer matches
		if (Stamp->Layer != VolumeLayer.Layer)
		{
			continue;
		}

		// Check stack - either using StackOverride or matching against target stack
		if (Stamp->StackOverride)
		{
			// Volume has a stack override - check if it matches
			if (Stamp->StackOverride == VolumeLayer.Stack)
			{
				return Volume;
			}
		}
		else
		{
			// No stack override - check if the target stack contains this layer
			if (VolumeLayer.Stack->VolumeLayers.Contains(Stamp->Layer))
			{
				return Volume;
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("FindVolumeForLayer: No matching volume found for Stack=%s, Layer=%s"),
		*VolumeLayer.Stack->GetName(),
		*VolumeLayer.Layer->GetName());

	return nullptr;
}

AVoxelSculptVolume* UPGLPCGScriptableTool::GetTargetSculptVolume() const
{
	// If experimental mode is enabled, try to find volume by layer
	if (ToolProperties->bUniqueVolumePerMaterial)
	{
		UPGLSurfacePaintPreset* ActivePreset = ToolProperties->GetActivePreset();
		if (ActivePreset)
		{
			FVoxelStackVolumeLayer VolumeLayer = ActivePreset->VolumeLayer;
			AVoxelSculptVolume* FoundVolume = FindVolumeForLayer(VolumeLayer);

			if (FoundVolume)
			{
				return FoundVolume;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("GetTargetSculptVolume: No volume found for layer, falling back to selected volume"));
			}
		}
	}

	// Default: use the manually selected volume
	return ToolProperties->VoxelSculptVolume.Get();
}

#undef LOCTEXT_NAMESPACE
