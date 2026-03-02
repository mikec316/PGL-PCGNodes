// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "EditorUtilities/PGLSurfaceTypePaletteCustomization.h"
#include "EditorUtilities/PGLPCGScriptableTool.h"
#include "EditorUtilities/PGLSurfacePaintPreset.h"
#include "Surface/VoxelSurfaceTypeInterface.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "IDetailChildrenBuilder.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "AssetThumbnail.h"

#define LOCTEXT_NAMESPACE "PGLPCGNodesEditor"

TSharedRef<IDetailCustomization> FPGLSurfaceTypePaletteCustomization::MakeInstance()
{
	return MakeShared<FPGLSurfaceTypePaletteCustomization>();
}

void FPGLSurfaceTypePaletteCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	UE_LOG(LogTemp, Warning, TEXT("PGLSurfaceTypePaletteCustomization::CustomizeDetails called!"));

	// Get the category where our properties live
	IDetailCategoryBuilder& PaintCategory = DetailBuilder.EditCategory("Paint Settings");

	// Get handles to the properties we want to customize
	PaletteArrayHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(UPGLPCGScriptableToolProperties, PaintPresetPalette)
	);

	ActiveIndexHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(UPGLPCGScriptableToolProperties, ActivePaletteIndex)
	);

	// Get the object being customized
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	if (ObjectsBeingCustomized.Num() > 0)
	{
		PropertiesBeingCustomized = Cast<UPGLPCGScriptableToolProperties>(ObjectsBeingCustomized[0].Get());
		UE_LOG(LogTemp, Warning, TEXT("  Found properties object: %s"), PropertiesBeingCustomized.IsValid() ? TEXT("Valid") : TEXT("Invalid"));
	}

	// Hide the default ActivePaletteIndex property (we'll render it visually)
	DetailBuilder.HideProperty(ActiveIndexHandle);

	// Keep the array property visible but add our custom palette widget after it
	// This allows users to add/remove/configure surface types using standard UI

	// Add a property change delegate to refresh the widget when the array changes
	if (PaletteArrayHandle.IsValid())
	{
		PaletteArrayHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([this, &DetailBuilder]()
		{
			// Force refresh the details panel when array changes
			DetailBuilder.ForceRefreshDetails();
		}));

		// Also listen to child property changes (when array elements are assigned)
		PaletteArrayHandle->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateLambda([this, &DetailBuilder]()
		{
			// Force refresh when individual array elements change
			DetailBuilder.ForceRefreshDetails();
		}));
	}

	PaintCategory.AddCustomRow(LOCTEXT("PaletteSelection", "Palette Selection"))
		.WholeRowContent()
		[
			CreatePaletteWidget()
		];

	UE_LOG(LogTemp, Warning, TEXT("  Custom palette widget added!"));
}

TSharedRef<SWidget> FPGLSurfaceTypePaletteCustomization::CreatePaletteWidget()
{
	const int32 PaletteSize = GetPaletteSize();

	UE_LOG(LogTemp, Warning, TEXT("CreatePaletteWidget called - PaletteSize: %d"), PaletteSize);

	if (PaletteSize == 0)
	{
		// Show message when palette is empty
		UE_LOG(LogTemp, Warning, TEXT("  Showing empty palette message"));
		return SNew(STextBlock)
			.Text(LOCTEXT("EmptyPalette", "Add surface types to the palette above"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground());
	}

	UE_LOG(LogTemp, Warning, TEXT("  Creating button grid with %d buttons"), PaletteSize);

	// Create a uniform grid panel for the palette buttons
	TSharedPtr<SUniformGridPanel> GridPanel = SNew(SUniformGridPanel)
		.SlotPadding(FMargin(4.0f));

	// Layout palette in a 4-column grid
	const int32 NumColumns = 4;

	for (int32 i = 0; i < PaletteSize; ++i)
	{
		const int32 Row = i / NumColumns;
		const int32 Col = i % NumColumns;

		UPGLSurfacePaintPreset* Preset = GetPresetAt(i);
		UVoxelSurfaceTypeInterface* SurfaceType = Preset ? Preset->SurfaceType : nullptr;
		FString ButtonLabel = Preset ? Preset->GetDisplayName().ToString() : TEXT("Empty");

		// Create thumbnail if we have a valid asset
		TSharedPtr<FAssetThumbnail> AssetThumbnail;
		if (SurfaceType)
		{
			AssetThumbnail = MakeShared<FAssetThumbnail>(SurfaceType, 64, 64, UThumbnailManager::Get().GetSharedThumbnailPool());
		}

		// Create size badge text (show only if not default 50,50,50)
		FString SizeText;
		bool bShowSizeBadge = false;
		if (Preset)
		{
			const FVector& Size = Preset->SculptSizeRelative;
			const FVector DefaultSize(50.0f, 50.0f, 50.0f);

			if (!Size.Equals(DefaultSize, 0.1f))
			{
				// Show compact size representation (e.g., "25x50x50")
				SizeText = FString::Printf(TEXT("%.0fx%.0fx%.0f"), Size.X, Size.Y, Size.Z);
				bShowSizeBadge = true;
			}
		}

		TSharedRef<SWidget> ButtonContent = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(4.0f)
			[
				// Thumbnail with size badge overlay
				SNew(SOverlay)
					+ SOverlay::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					[
						SNew(SBox)
							.WidthOverride(64)
							.HeightOverride(64)
							[
								AssetThumbnail.IsValid()
									? AssetThumbnail->MakeThumbnailWidget()
									: StaticCastSharedRef<SWidget>(
										SNew(SBorder)
											.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
											.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f))
											[
												SNew(STextBlock)
													.Text(FText::FromString(TEXT("?")))
													.Justification(ETextJustify::Center)
													.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
											]
									)
							]
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Bottom)
					.Padding(2.0f)
					[
						// Size badge (only show if not default 50,50,50)
						SNew(SBorder)
							.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
							.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f))
							.Padding(FMargin(4.0f, 2.0f))
							.Visibility(bShowSizeBadge ? EVisibility::Visible : EVisibility::Collapsed)
							[
								SNew(STextBlock)
									.Text(FText::FromString(SizeText))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 7))
									.ColorAndOpacity(FLinearColor::White)
							]
					]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(2.0f)
			[
				// Label
				SNew(STextBlock)
					.Text(FText::FromString(ButtonLabel))
					.Justification(ETextJustify::Center)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
					.AutoWrapText(true)
			];

		// Apply preset button tint if available
		FLinearColor ButtonTint = Preset ? Preset->ButtonTint : FLinearColor::White;

		GridPanel->AddSlot(Col, Row)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SButton)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					.ButtonColorAndOpacity(this, &FPGLSurfaceTypePaletteCustomization::GetButtonColor, i)
					.OnClicked(this, &FPGLSurfaceTypePaletteCustomization::OnPaletteButtonClicked, i)
					.IsEnabled(Preset != nullptr && Preset->SurfaceType != nullptr)  // Disable empty slots
					.ContentScale(FVector2D(1.0f, 1.0f))
					[
						SNew(SBorder)
							.BorderImage(FAppStyle::GetBrush("NoBorder"))
							.ColorAndOpacity(ButtonTint)
							[
								ButtonContent
							]
					]
			];
	}

	return GridPanel.ToSharedRef();
}

FReply FPGLSurfaceTypePaletteCustomization::OnPaletteButtonClicked(int32 ButtonIndex)
{
	if (ActiveIndexHandle.IsValid())
	{
		// Set the ActivePaletteIndex property to the clicked button index
		ActiveIndexHandle->SetValue(ButtonIndex);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

int32 FPGLSurfaceTypePaletteCustomization::GetActiveIndex() const
{
	if (ActiveIndexHandle.IsValid())
	{
		int32 Value = 0;
		ActiveIndexHandle->GetValue(Value);
		return Value;
	}
	return 0;
}

int32 FPGLSurfaceTypePaletteCustomization::GetPaletteSize() const
{
	if (PaletteArrayHandle.IsValid())
	{
		uint32 NumElements = 0;
		PaletteArrayHandle->GetNumChildren(NumElements);
		return static_cast<int32>(NumElements);
	}
	return 0;
}

UPGLSurfacePaintPreset* FPGLSurfaceTypePaletteCustomization::GetPresetAt(int32 Index) const
{
	if (PropertiesBeingCustomized.IsValid() &&
		PropertiesBeingCustomized->PaintPresetPalette.IsValidIndex(Index))
	{
		return PropertiesBeingCustomized->PaintPresetPalette[Index];
	}
	return nullptr;
}

UVoxelSurfaceTypeInterface* FPGLSurfaceTypePaletteCustomization::GetSurfaceTypeAt(int32 Index) const
{
	UPGLSurfacePaintPreset* Preset = GetPresetAt(Index);
	return Preset ? Preset->SurfaceType : nullptr;
}

FSlateColor FPGLSurfaceTypePaletteCustomization::GetButtonColor(int32 ButtonIndex) const
{
	const int32 ActiveIndex = GetActiveIndex();

	// Highlight the selected button
	if (ButtonIndex == ActiveIndex)
	{
		return FSlateColor(FLinearColor(0.0f, 0.5f, 1.0f));  // Blue for selected
	}

	// Check if this slot has a surface type
	UVoxelSurfaceTypeInterface* SurfaceType = GetSurfaceTypeAt(ButtonIndex);
	if (SurfaceType)
	{
		return FSlateColor(FLinearColor(0.2f, 0.2f, 0.2f));  // Dark gray for valid
	}

	return FSlateColor(FLinearColor(0.1f, 0.1f, 0.1f));  // Very dark for empty
}

#undef LOCTEXT_NAMESPACE
