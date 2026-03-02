// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PGLEditorSettings.generated.h"

/**
 * Persistent settings for the PGL Graph Editor viewport.
 * Stored in EditorPerProjectUserSettings.ini — per-project, per-user.
 *
 * Mirrors the UPVEditorSettings pattern from the PVE plugin.
 */
UCLASS(MinimalAPI, config=EditorPerProjectUserSettings, meta = (DisplayName = "PGL Editor Settings"))
class UPGLEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	/** Show a mannequin mesh in the viewport as a scale reference. */
	UPROPERTY(EditAnywhere, config, Category = Viewport)
	bool bShowMannequin = false;

	/** Horizontal offset of the mannequin along the X axis. */
	UPROPERTY(EditAnywhere, config, Category = Viewport)
	float MannequinOffset = 0.0f;

	/** Show coloured axis lines with dimension labels in the viewport. */
	UPROPERTY(EditAnywhere, config, Category = Viewport)
	bool bShowScaleVisualization = false;

	/** Automatically focus the viewport when scene data changes. */
	UPROPERTY(EditAnywhere, config, Category = Viewport)
	bool bAutoFocusViewport = false;
};
