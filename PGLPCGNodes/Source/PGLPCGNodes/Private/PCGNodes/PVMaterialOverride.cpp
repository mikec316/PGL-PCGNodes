// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PVMaterialOverride.h"

#include "Materials/MaterialInterface.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "PCGComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"                                        // TActorIterator

#include "GeometryCollection/GeometryCollectionActor.h"         // AGeometryCollectionActor
#include "GeometryCollection/GeometryCollectionComponent.h"     // UGeometryCollectionComponent

#define LOCTEXT_NAMESPACE "PVMaterialOverrideSettings"

// -----------------------------------------------------------------------------
//  Editor metadata
// -----------------------------------------------------------------------------

#if WITH_EDITOR

FText UPVMaterialOverrideSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Material Override");
}

FText UPVMaterialOverrideSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Finds AGeometryCollectionActor instances in the world and calls\n"
		"SetMaterial(MaterialSlotIndex, CapMaterial) on their\n"
		"UGeometryCollectionComponent.\n\n"
		"Place this node DOWNSTREAM of PVE's output node so the GC actors\n"
		"already exist when this node executes.  Set ActorTag to the tag\n"
		"assigned to your PVE trees to avoid affecting other GC actors.\n\n"
		"All input data is passed straight through to the output pin."
	);
}

#endif // WITH_EDITOR

// -----------------------------------------------------------------------------
//  Pins
// -----------------------------------------------------------------------------

TArray<FPCGPinProperties> UPVMaterialOverrideSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Props;
	// Optional pass-through — not required for data, only to establish ordering.
	// Wire this pin to something downstream of PVE's spawner so PCG guarantees
	// PVE has already created the GC actors before this node runs.
	Props.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Any);
	return Props;
}

TArray<FPCGPinProperties> UPVMaterialOverrideSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Props;
	// Pass-through — upstream data flows on unchanged.
	Props.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Any);
	return Props;
}

FPCGElementPtr UPVMaterialOverrideSettings::CreateElement() const
{
	return MakeShared<FPVMaterialOverrideElement>();
}

// -----------------------------------------------------------------------------
//  Execution
// -----------------------------------------------------------------------------

bool FPVMaterialOverrideElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPVMaterialOverrideElement::Execute);

	check(Context);

	// ------------------------------------------------------------------
	//  Game-thread guard
	//  CanExecuteOnlyOnMainThread() requests this, but guard defensively.
	// ------------------------------------------------------------------
	if (!IsInGameThread())
	{
		PCGE_LOG(Error, GraphAndLog,
			LOCTEXT("NotGameThread",
				"PVMaterialOverride must run on the game thread. "
				"Verify CanExecuteOnlyOnMainThread returns true."));
		return true;
	}

	const UPVMaterialOverrideSettings* Settings =
		Context->GetInputSettings<UPVMaterialOverrideSettings>();
	check(Settings);

	// ------------------------------------------------------------------
	//  Validate material
	// ------------------------------------------------------------------
	UMaterialInterface* Material = Settings->CapMaterial.LoadSynchronous();
	if (!Material)
	{
		PCGE_LOG(Warning, GraphAndLog,
			LOCTEXT("NullMaterial",
				"CapMaterial is null — assign a material asset to this node."));
		Context->OutputData = Context->InputData;
		return true;
	}

	// ------------------------------------------------------------------
	//  Validate world
	// ------------------------------------------------------------------
	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (!SourceComponent)
	{
		PCGE_LOG(Error, GraphAndLog,
			LOCTEXT("NoSourceComponent", "SourceComponent is invalid."));
		Context->OutputData = Context->InputData;
		return true;
	}

	UWorld* World = SourceComponent->GetWorld();
	if (!World)
	{
		PCGE_LOG(Error, GraphAndLog,
			LOCTEXT("NoWorld", "Could not obtain a valid UWorld."));
		Context->OutputData = Context->InputData;
		return true;
	}

	// ------------------------------------------------------------------
	//  Tag filter setup
	// ------------------------------------------------------------------
	const FName FilterTag    = Settings->ActorTag;
	const bool  bFilterByTag = !FilterTag.IsNone();
	const int32 SlotIndex    = Settings->MaterialSlotIndex;

	if (!bFilterByTag)
	{
		PCGE_LOG(Warning, GraphAndLog,
			LOCTEXT("NoTagFilter",
				"ActorTag is empty — ALL AGeometryCollectionActors in the "
				"world will have their material slot overwritten."));
	}

	// ------------------------------------------------------------------
	//  Iterate actors and apply material
	// ------------------------------------------------------------------
	int32 AffectedCount = 0;

	for (TActorIterator<AGeometryCollectionActor> It(World); It; ++It)
	{
		AGeometryCollectionActor* GCActor = *It;

		if (!IsValid(GCActor))
		{
			continue;
		}

		if (bFilterByTag && !GCActor->Tags.Contains(FilterTag))
		{
			continue;
		}

		UGeometryCollectionComponent* GCComp =
			GCActor->GetGeometryCollectionComponent();

		if (!GCComp)
		{
			PCGE_LOG(Warning, GraphAndLog,
				LOCTEXT("NoGCComp",
					"A matching GC actor has no GeometryCollectionComponent — "
					"skipping."));
			continue;
		}

		GCComp->SetMaterial(SlotIndex, Material);
		++AffectedCount;
	}

	// ------------------------------------------------------------------
	//  Diagnostic log
	// ------------------------------------------------------------------
	if (AffectedCount == 0)
	{
		if (bFilterByTag)
		{
			PCGE_LOG(Warning, GraphAndLog,
				FText::Format(
					LOCTEXT("NoMatchingActors",
						"No AGeometryCollectionActors found with tag '{0}'. "
						"Make sure PVE has already spawned the actors before "
						"this node runs, and that the tag matches."),
					FText::FromName(FilterTag)));
		}
		else
		{
			PCGE_LOG(Warning, GraphAndLog,
				LOCTEXT("NoActorsAtAll",
					"No AGeometryCollectionActors found in the world."));
		}
	}
	else
	{
		PCGE_LOG(Verbose, LogOnly,
			FText::Format(
				LOCTEXT("AppliedMaterial",
					"Applied material to slot {0} on {1} "
					"GeometryCollectionComponent(s)."),
				FText::AsNumber(SlotIndex),
				FText::AsNumber(AffectedCount)));
	}

	// ------------------------------------------------------------------
	//  Pass input data through unchanged
	// ------------------------------------------------------------------
	Context->OutputData = Context->InputData;
	return true;
}

#undef LOCTEXT_NAMESPACE
