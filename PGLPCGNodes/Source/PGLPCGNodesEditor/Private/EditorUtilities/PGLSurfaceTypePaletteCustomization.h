// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "IDetailCustomization.h"

/**
 * Custom detail panel customization for UPGLPCGScriptableToolProperties
 * Renders the surface type palette as a visual button grid instead of dropdown
 */
class FPGLSurfaceTypePaletteCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	/** Handle to the SurfaceTypePalette array property */
	TSharedPtr<IPropertyHandle> PaletteArrayHandle;

	/** Handle to the ActivePaletteIndex property */
	TSharedPtr<IPropertyHandle> ActiveIndexHandle;

	/** Cached reference to the property being edited */
	TWeakObjectPtr<class UPGLPCGScriptableToolProperties> PropertiesBeingCustomized;

	/** Called when a palette button is clicked */
	FReply OnPaletteButtonClicked(int32 ButtonIndex);

	/** Generates the palette button grid widget */
	TSharedRef<SWidget> CreatePaletteWidget();

	/** Regenerates the palette widget dynamically */
	TSharedRef<SWidget> GeneratePaletteContent() const;

	/** Gets the current active index from the property handle */
	int32 GetActiveIndex() const;

	/** Gets the number of items in the palette array */
	int32 GetPaletteSize() const;

	/** Gets the surface type at the specified index (or nullptr) */
	class UVoxelSurfaceTypeInterface* GetSurfaceTypeAt(int32 Index) const;

	/** Gets the paint preset at the specified index (or nullptr) */
	class UPGLSurfacePaintPreset* GetPresetAt(int32 Index) const;

	/** Returns the button color based on selection state */
	FSlateColor GetButtonColor(int32 ButtonIndex) const;
};
