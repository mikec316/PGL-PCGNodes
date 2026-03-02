// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PGLFoliagePaletteSettings.h"

#include "PCGContext.h"
#include "PCGPin.h"

#include "DataTypes/PVMeshData.h"
#include "Facades/PVFoliageFacade.h"
#include "GeometryCollection/ManagedArrayCollection.h"

#define LOCTEXT_NAMESPACE "PGLFoliagePaletteSettings"

// =============================================================================
//  Editor metadata
// =============================================================================

#if WITH_EDITOR

FText UPGLFoliagePaletteSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Foliage Palette");
}

FText UPGLFoliagePaletteSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Allows the user to visualize and change the foliage meshes.\n\n"
		"When Override Foliage is OFF the list mirrors the upstream collection.\n"
		"When Override Foliage is ON entries can be freely added, removed, or replaced.\n\n"
		"This is the PGL replacement for the PVE Foliage Palette node,\n"
		"with the array size restriction removed."
	);
}

void UPGLFoliagePaletteSettings::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property &&
		PropertyChangedEvent.Property->GetFName() ==
			GET_MEMBER_NAME_CHECKED(UPGLFoliagePaletteSettings, bOverrideFoliage))
	{
		DirtyCache();
	}
}

#endif // WITH_EDITOR

// =============================================================================
//  Pin type identifiers — uses PVMeshData (post Mesh Builder)
// =============================================================================

FPCGDataTypeIdentifier UPGLFoliagePaletteSettings::GetInputPinTypeIdentifier() const
{
	return FPCGDataTypeIdentifier{ FPVDataTypeInfoMesh::AsId() };
}

FPCGDataTypeIdentifier UPGLFoliagePaletteSettings::GetOutputPinTypeIdentifier() const
{
	return FPCGDataTypeIdentifier{ FPVDataTypeInfoMesh::AsId() };
}

// =============================================================================
//  Element
// =============================================================================

FPCGElementPtr UPGLFoliagePaletteSettings::CreateElement() const
{
	return MakeShared<FPGLFoliagePaletteElement>();
}

// =============================================================================
//  Execution
// =============================================================================

bool FPGLFoliagePaletteElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPGLFoliagePaletteElement::Execute);

	check(Context);

	const UPGLFoliagePaletteSettings* Settings =
		Context->GetInputSettings<UPGLFoliagePaletteSettings>();
	check(Settings);

	UPGLFoliagePaletteSettings* const MutableSettings =
		const_cast<UPGLFoliagePaletteSettings*>(Settings);
	check(MutableSettings);

	const TArray<FPCGTaggedData>& Inputs =
		Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	if (Inputs.IsEmpty())
	{
		return true;
	}

	const UPVMeshData* InputData = Cast<UPVMeshData>(Inputs[0].Data);
	if (!InputData)
	{
		PCGE_LOG(Error, GraphAndLog,
			LOCTEXT("InvalidInput",
				"PGL Foliage Palette: input is not valid PV Mesh Data."));
		return true;
	}

	// Copy the collection so we can modify it.
	FManagedArrayCollection Collection = InputData->GetCollection();

	UPVMeshData* OutData = FPCGContext::NewObject_AnyThread<UPVMeshData>(Context);

	PV::Facades::FFoliageFacade Facade(Collection);

	if (!Settings->bOverrideFoliage)
	{
		// -----------------------------------------------------------------
		//  Mirror mode: read foliage names from the collection into the UI.
		// -----------------------------------------------------------------
		MutableSettings->FoliageMeshes.Empty();
		for (int32 i = 0; i < Facade.NumFoliageNames(); ++i)
		{
			MutableSettings->FoliageMeshes.Emplace(
				FSoftObjectPath(Facade.GetFoliageName(i)));
		}
#if WITH_EDITOR
		MutableSettings->PostEditChange();
#endif
	}
	else
	{
		// -----------------------------------------------------------------
		//  Override mode: write the user's mesh list into the collection.
		// -----------------------------------------------------------------
		TArray<FString> FoliageNames;
		FoliageNames.Reserve(Settings->FoliageMeshes.Num());

		for (const TSoftObjectPtr<UObject>& MeshRef : Settings->FoliageMeshes)
		{
			FoliageNames.Emplace(MeshRef.ToString());
		}

		Facade.SetFoliageNames(FoliageNames);
	}

	OutData->Initialize(MoveTemp(Collection));
	Context->OutputData.TaggedData.Emplace(OutData);

	return true;
}

#undef LOCTEXT_NAMESPACE
