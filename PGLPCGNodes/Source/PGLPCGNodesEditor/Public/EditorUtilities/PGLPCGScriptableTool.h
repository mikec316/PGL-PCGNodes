// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseTools/EditorScriptableClickDragTool.h"
#include "InteractiveToolBuilder.h"
#include "EditorUtilities/PGLSurfacePaintPreset.h"
#include "PGLPCGScriptableTool.generated.h"

// Forward declarations
class AVoxelSculptVolume;
class UPreviewGeometry;
class UVoxelSurfaceTypeInterface;

/**
 * Builder for UPGLPCGScriptableTool
 */
UCLASS()
class PGLPCGNODESEDITOR_API UPGLPCGScriptableToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

/**
 * Properties for the PGLPCG Voxel Sculpting Tool
 */
UCLASS(Config = EditorPerProjectUserSettings)
class PGLPCGNODESEDITOR_API UPGLPCGScriptableToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Grid size for snapping the preview cube */
	UPROPERTY(EditAnywhere, Config, Category = "Tool Settings", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float GridSize = 50.0f;

	/** Target Voxel Sculpt Volume actor to sculpt on */
	UPROPERTY(EditAnywhere, Category = "Tool Settings", meta = (AllowedClasses = "/Script/Voxel.VoxelSculptVolume"))
	TSoftObjectPtr<AVoxelSculptVolume> VoxelSculptVolume;

	/** Smoothness of the sculpting operation (0 = sharp edges, higher = smoother) */
	UPROPERTY(EditAnywhere, Config, Category = "Sculpt Settings", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SculptSmoothness = 0.0f;

	/** Array of paint presets available for quick switching */
	UPROPERTY(EditAnywhere, Category = "Paint Settings")
	TArray<TObjectPtr<UPGLSurfacePaintPreset>> PaintPresetPalette;

	/** Saved preset paths for persistence (automatically synced with PaintPresetPalette) */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> SavedPresetPaths;

	/** Index of the currently active paint preset (0-based) */
	UPROPERTY(EditAnywhere, Config, Category = "Paint Settings",
		meta = (ClampMin = "0", UIMin = "0", EditCondition = "PaintPresetPalette.Num() > 0"))
	int32 ActivePaletteIndex = 0;

	/** Strength of the paint operation (0-1) */
	UPROPERTY(EditAnywhere, Config, Category = "Paint Settings", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float PaintStrength = 1.0f;

	/** If true, only paint without sculpting terrain (requires active preset to be set) */
	UPROPERTY(EditAnywhere, Config, Category = "Paint Settings")
	bool bPaintOnly = false;

	/** EXPERIMENTAL: If true, automatically find volume with matching stack/layer instead of using selected volume */
	UPROPERTY(EditAnywhere, Config, Category = "Experimental")
	bool bUniqueVolumePerMaterial = false;

	/** Get the currently active paint preset from the palette */
	UPGLSurfacePaintPreset* GetActivePreset() const
	{
		if (ActivePaletteIndex >= 0 && ActivePaletteIndex < PaintPresetPalette.Num())
		{
			return PaintPresetPalette[ActivePaletteIndex];
		}
		return nullptr;
	}

	/** Get the currently selected surface type from the active preset */
	UVoxelSurfaceTypeInterface* GetActiveSurfaceType() const
	{
		UPGLSurfacePaintPreset* Preset = GetActivePreset();
		return Preset ? Preset->SurfaceType : nullptr;
	}

	/** Get the sculpt size from the active preset (defaults to 50,50,50) */
	FVector GetActiveSculptSize() const
	{
		UPGLSurfacePaintPreset* Preset = GetActivePreset();
		return Preset ? Preset->SculptSizeRelative : FVector(50.0f, 50.0f, 50.0f);
	}

	/** Get the volume layer from the active preset */
	FVoxelStackVolumeLayer GetActiveVolumeLayer() const
	{
		UPGLSurfacePaintPreset* Preset = GetActivePreset();
		return Preset ? Preset->VolumeLayer : FVoxelStackVolumeLayer();
	}

	/** Called after properties are loaded from config */
	virtual void PostInitProperties() override;

	/** Called before properties are saved to config */
	void SaveConfig(uint64 Flags = CPF_Config, const TCHAR* Filename = nullptr);

private:
	/** Loads presets from saved paths */
	void LoadPresetsFromConfig();

	/** Saves preset paths from current palette */
	void SavePresetsToConfig();
};

/**
 * PGLPCG Voxel Sculpting Tool
 *
 * Interactive tool for sculpting voxel terrain by dragging out cube volumes.
 * - Click to place the initial corner
 * - Drag to define the size of the sculpt cube
 * - Release to apply the sculpt operation to the voxel volume
 */
UCLASS()
class PGLPCGNODESEDITOR_API UPGLPCGScriptableTool : public UEditorScriptableClickDragTool
{
	GENERATED_BODY()

public:
	// UInteractiveTool interface
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void OnTick(float DeltaTime) override;

	// UScriptableClickDragTool interface
	virtual void OnDragBegin_Implementation(FInputDeviceRay StartPosition, const FScriptableToolModifierStates& Modifiers) override;
	virtual void OnDragUpdatePosition_Implementation(FInputDeviceRay NewPosition, const FScriptableToolModifierStates& Modifiers) override;
	virtual void OnDragEnd_Implementation(FInputDeviceRay EndPosition, const FScriptableToolModifierStates& Modifiers) override;
	virtual FInputRayHit OnHoverHitTest_Implementation(FInputDeviceRay HoverPos, const FScriptableToolModifierStates& Modifiers) override;
	virtual bool OnHoverUpdate_Implementation(FInputDeviceRay HoverPos, const FScriptableToolModifierStates& Modifiers) override;

protected:
	UPROPERTY()
	TObjectPtr<UPGLPCGScriptableToolProperties> ToolProperties;

	/** Preview geometry for the sculpt cube */
	UPROPERTY()
	TObjectPtr<UPreviewGeometry> PreviewGeometry;

	/** Starting position of the drag operation (grid-snapped) */
	FVector DragStartPosition;

	/** Current position during drag (grid-snapped) */
	FVector DragCurrentPosition;

	/** Whether a drag operation is in progress */
	bool bIsDragging = false;

	/** Current modifier states during drag */
	FScriptableToolModifierStates CurrentModifiers;

	/** Current hover position (grid-snapped) when not dragging */
	FVector HoverPosition;

	/** Whether we have a valid hover position */
	bool bHasValidHoverPosition = false;

	/** Helper function to perform world raycast and get hit location */
	bool GetWorldHitLocation(const FInputDeviceRay& Ray, FVector& OutHitLocation) const;

	/** Snaps a world position to the grid */
	FVector SnapToGrid(const FVector& Position) const;

	/** Updates the preview cube based on current drag state */
	void UpdatePreviewCube();

	/** Calculates the bounds for the sculpt cube */
	void GetSculptBounds(FVector& OutCenter, FVector& OutSize) const;

	/** Finds a voxel sculpt volume that matches the given stack/layer combination */
	AVoxelSculptVolume* FindVolumeForLayer(const FVoxelStackVolumeLayer& VolumeLayer) const;

	/** Gets the target sculpt volume (either selected or auto-found based on settings) */
	AVoxelSculptVolume* GetTargetSculptVolume() const;
};
