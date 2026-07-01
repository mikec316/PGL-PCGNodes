// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "VoxelToHLSL/PGLVoxelToHLSLConverter.h"
#include "VoxelToHLSL/PGLVoxelToHLSLTypes.h"

#include "VoxelGraph.h"

#include "ToolMenus.h"
#include "ContentBrowserMenuContexts.h"
#include "AssetRegistry/AssetData.h"
#include "Misc/MessageDialog.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "PGLVoxelToHLSL"

namespace PGLVoxelToHLSLAction
{

static void ExecuteConversion(const TArray<FAssetData>& SelectedAssets)
{
	for (const FAssetData& AssetData : SelectedAssets)
	{
		UVoxelGraph* VoxelGraph = Cast<UVoxelGraph>(AssetData.GetAsset());
		if (!VoxelGraph)
		{
			continue;
		}

		// Default options
		FPGLVoxelToHLSLOptions Options;

		// Run conversion
		const FPGLHLSLTranslationResult Result = UPGLVoxelToHLSLConverter::ConvertVoxelGraphToHLSL(VoxelGraph, Options);

		if (Result.Status == EPGLHLSLTranslationStatus::Failure)
		{
			FString ErrorMsg = TEXT("Failed to convert voxel graph:\n");
			for (const FString& Warning : Result.Warnings)
			{
				ErrorMsg += TEXT("  - ") + Warning + TEXT("\n");
			}
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::FromString(ErrorMsg));
			continue;
		}

		// Generate output path: same directory as source, with _HLSL suffix
		const FString SourcePath = AssetData.PackagePath.ToString();
		const FString AssetName = AssetData.AssetName.ToString() + TEXT("_HLSL");

		// Save as PCG Compute Source
		UPCGComputeSource* CreatedAsset = UPGLVoxelToHLSLConverter::SaveAsComputeSource(
			SourcePath, AssetName, Result.GeneratedHLSL);

		if (CreatedAsset)
		{
			FString StatusMsg = FString::Printf(
				TEXT("Converted \"%s\" to PCG Compute Source \"%s\"\n\nNodes: %d/%d translated"),
				*AssetData.AssetName.ToString(),
				*AssetName,
				Result.TranslatedNodes,
				Result.TotalNodes);

			if (Result.RequiredOutputAttributes.Num() > 0)
			{
				StatusMsg += TEXT("\n\nREQUIRED SETUP — Add these to your Custom HLSL node:\n");
				StatusMsg += TEXT("Output Pins > Out > GPU Properties > Attributes to Create:");
				for (const FString& Attr : Result.RequiredOutputAttributes)
				{
					StatusMsg += TEXT("\n  - ") + Attr;
				}
			}

			if (Result.Warnings.Num() > 0)
			{
				StatusMsg += TEXT("\n\nWarnings:");
				for (const FString& Warning : Result.Warnings)
				{
					StatusMsg += TEXT("\n  - ") + Warning;
				}
			}

			FMessageDialog::Open(EAppMsgType::Ok,
				FText::FromString(StatusMsg));
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				LOCTEXT("SaveFailed", "Failed to save PCG Compute Source asset."));
		}
	}
}

static void RegisterMenuExtension()
{
	UE_LOG(LogTemp, Log, TEXT("PGLVoxelToHLSL: Registering content browser menu extension"));

	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!ToolMenus)
	{
		UE_LOG(LogTemp, Warning, TEXT("PGLVoxelToHLSL: UToolMenus not available"));
		return;
	}

	// Register on the content browser asset context menu
	UToolMenu* Menu = ToolMenus->ExtendMenu("ContentBrowser.AssetContextMenu");
	if (!Menu)
	{
		UE_LOG(LogTemp, Warning, TEXT("PGLVoxelToHLSL: Failed to extend ContentBrowser.AssetContextMenu"));
		return;
	}

	FToolMenuSection& Section = Menu->FindOrAddSection("PGLVoxelToHLSL");
	Section.AddDynamicEntry("ConvertToHLSL",
		FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			// Only show for VoxelGraph assets
			const UContentBrowserAssetContextMenuContext* Context =
				InSection.FindContext<UContentBrowserAssetContextMenuContext>();

			if (!Context || Context->SelectedAssets.Num() == 0)
			{
				return;
			}

			// Use the type-safe helper which checks class hierarchy without loading
			const TArray<FAssetData> VoxelAssets = Context->GetSelectedAssetsOfType(UVoxelGraph::StaticClass());
			if (VoxelAssets.Num() == 0)
			{
				return;
			}

			InSection.AddMenuEntry(
				"PGL_ConvertVoxelToHLSL",
				LOCTEXT("ConvertToHLSL", "Convert to PCG Compute Source"),
				LOCTEXT("ConvertToHLSL_Tooltip", "Translate this Voxel Graph into HLSL and save as a PCG Compute Source asset"),
				FSlateIcon(),
				FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& InContext)
				{
					const UContentBrowserAssetContextMenuContext* CBContext =
						InContext.FindContext<UContentBrowserAssetContextMenuContext>();
					if (CBContext)
					{
						ExecuteConversion(CBContext->SelectedAssets);
					}
				})
			);
		})
	);
}

// Module-level registration function called from module startup
static FDelegateHandle RegistrationHandle;

void Register()
{
	// Defer registration until ToolMenus is ready
	if (UToolMenus::IsToolMenuUIEnabled())
	{
		RegisterMenuExtension();
	}
	else
	{
		RegistrationHandle = UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateStatic(&RegisterMenuExtension));
	}
}

void Unregister()
{
	if (RegistrationHandle.IsValid())
	{
		UToolMenus::UnRegisterStartupCallback(RegistrationHandle);
		RegistrationHandle.Reset();
	}
}

} // namespace PGLVoxelToHLSLAction

#undef LOCTEXT_NAMESPACE
