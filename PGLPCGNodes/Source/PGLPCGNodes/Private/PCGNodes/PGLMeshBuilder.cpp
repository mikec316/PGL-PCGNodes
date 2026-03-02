// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PGLMeshBuilder.h"

#include "PCGContext.h"

#define LOCTEXT_NAMESPACE "PGLMeshBuilder"
#include "PCGNodes/PGLDelegationHelper.h"
#include "DataTypes/PVGrowthData.h"
#include "DataTypes/PVMeshData.h"
#include "UObject/UnrealType.h"

// =============================================================================
//  UPGLMeshBuilderSettings
// =============================================================================

#if WITH_EDITOR
FText UPGLMeshBuilderSettings::GetDefaultNodeTitle() const
{
	return NSLOCTEXT("PGLMeshBuilder", "NodeTitle", "PGL Mesh Builder");
}

FText UPGLMeshBuilderSettings::GetNodeTooltipText() const
{
	return NSLOCTEXT("PGLMeshBuilder", "Tooltip",
		"Generates a geometry collection mesh from PVE growth data.\n"
		"Delegates to PVE's Mesh Builder at runtime and propagates Frame Mesh data.");
}
#endif

FPCGDataTypeIdentifier UPGLMeshBuilderSettings::GetInputPinTypeIdentifier() const
{
	return FPCGDataTypeIdentifier{ FPVDataTypeInfoGrowth::AsId() };
}

FPCGDataTypeIdentifier UPGLMeshBuilderSettings::GetOutputPinTypeIdentifier() const
{
	return FPCGDataTypeIdentifier{ FPVDataTypeInfoMesh::AsId() };
}

FPCGElementPtr UPGLMeshBuilderSettings::CreateElement() const
{
	return MakeShared<FPGLMeshBuilderElement>();
}


// =============================================================================
//  FPGLMeshBuilderElement
// =============================================================================

bool FPGLMeshBuilderElement::ExecuteInternal(FPCGContext* Context) const
{
	const UPGLMeshBuilderSettings* Settings =
		Context->GetInputSettings<UPGLMeshBuilderSettings>();
	if (!Settings)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingSettings", "PGL Mesh Builder: missing settings."));
		return true;
	}

	// --- Capture FrameMesh from input ---
	FString CapturedFrameMeshPath;
	FTransform CapturedFrameMeshTransform;
	PGLDelegation::CaptureFrameMeshFromInput(
		Context->InputData, CapturedFrameMeshPath, CapturedFrameMeshTransform);

	// --- Find PVE Mesh Builder settings class ---
	static UClass* PVEClass = FindObject<UClass>(
		nullptr, TEXT("/Script/ProceduralVegetation.PVMeshBuilderSettings"));
	if (!PVEClass)
	{
		PCGE_LOG(Error, GraphAndLog,
			LOCTEXT("PVEClassNotFound", "PGL Mesh Builder: PVE MeshBuilderSettings class not found. Is ProceduralVegetation plugin enabled?"));
		return true;
	}

	// --- Create PVE settings instance and copy properties ---
	UPCGSettings* PVESettings = NewObject<UPCGSettings>(
		GetTransientPackage(), PVEClass, NAME_None, RF_Transient);

	// Copy our MesherSettings struct → PVE's MesherSettings struct via reflection.
	// Both have the same property name "MesherSettings"; the inner fields share
	// names by design so CopyMatchingStructProperties matches them.
	FStructProperty* DstMesherProp = CastField<FStructProperty>(
		PVEClass->FindPropertyByName(TEXT("MesherSettings")));
	if (DstMesherProp)
	{
		void* DstMesherData = DstMesherProp->ContainerPtrToValuePtr<void>(PVESettings);
		PGLDelegation::CopyMatchingStructProperties(
			&Settings->MesherSettings,
			FPGLMeshBuilderParams::StaticStruct(),
			DstMesherData,
			DstMesherProp->Struct);
	}

	// --- Delegate to PVE element ---
	FPCGDataCollection DelegatedOutput;
	if (!PGLDelegation::DelegateToPVEElement(Context, PVESettings, DelegatedOutput))
	{
		PCGE_LOG(Error, GraphAndLog,
			LOCTEXT("DelegationFailed", "PGL Mesh Builder: delegation to PVE element failed."));
		return true;
	}

	// --- Set output and propagate FrameMesh ---
	Context->OutputData = MoveTemp(DelegatedOutput);
	PGLDelegation::EnsureFrameMeshInOutput(
		Context->OutputData, CapturedFrameMeshPath, CapturedFrameMeshTransform);

	return true;
}

#undef LOCTEXT_NAMESPACE
