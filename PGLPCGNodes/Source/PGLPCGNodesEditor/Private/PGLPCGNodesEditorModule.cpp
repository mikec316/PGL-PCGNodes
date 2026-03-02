// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PGLPCGNodesEditorModule.h"
#include "EditorUtilities/PGLSurfaceTypePaletteCustomization.h"
#include "EditorUtilities/PGLPCGScriptableTool.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "FPGLPCGNodesEditorModule"

void FPGLPCGEditorNodesModule::StartupModule()
{
	// Register custom detail panel customization for the tool properties
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyModule.RegisterCustomClassLayout(
		UPGLPCGScriptableToolProperties::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FPGLSurfaceTypePaletteCustomization::MakeInstance)
	);
}

void FPGLPCGEditorNodesModule::ShutdownModule()
{
	// Unregister customization on shutdown
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		PropertyModule.UnregisterCustomClassLayout(
			UPGLPCGScriptableToolProperties::StaticClass()->GetFName()
		);
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPGLPCGEditorNodesModule, PGLPCGNodesEditor)
