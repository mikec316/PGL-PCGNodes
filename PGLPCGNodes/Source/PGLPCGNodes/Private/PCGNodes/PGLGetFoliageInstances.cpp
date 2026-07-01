// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PGLGetFoliageInstances.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGData.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

#include "InstancedFoliageActor.h"
#include "FoliageType.h"
#include "InstancedFoliage.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PGLGetFoliageInstances)

#define LOCTEXT_NAMESPACE "PGLGetFoliageInstances"

// ---------------------------------------------------------------------------
// UPGLGetFoliageInstancesSettings
// ---------------------------------------------------------------------------

#if WITH_EDITOR
FText UPGLGetFoliageInstancesSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "PGL Get Foliage Instances");
}

FText UPGLGetFoliageInstancesSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Reads all instanced static mesh components from the level's InstancedFoliageActor.\n"
		"Outputs one PCG point per foliage instance with the instance transform,\n"
		"a MeshReference attribute (soft object path to the static mesh),\n"
		"and a FoliageType attribute (soft object path to the UFoliageType).");
}
#endif

TArray<FPCGPinProperties> UPGLGetFoliageInstancesSettings::InputPinProperties() const
{
	// No inputs — this is a source node.
	return TArray<FPCGPinProperties>();
}

TArray<FPCGPinProperties> UPGLGetFoliageInstancesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
	return PinProperties;
}

FPCGElementPtr UPGLGetFoliageInstancesSettings::CreateElement() const
{
	return MakeShared<FPGLGetFoliageInstancesElement>();
}

// ---------------------------------------------------------------------------
// FPGLGetFoliageInstancesElement
// ---------------------------------------------------------------------------

bool FPGLGetFoliageInstancesElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPGLGetFoliageInstancesElement::Execute);
	check(Context);

	const UPGLGetFoliageInstancesSettings* Settings = Context->GetInputSettings<UPGLGetFoliageInstancesSettings>();
	check(Settings);

	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (!SourceComponent)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoSourceComponent", "Invalid source PCG component."));
		return true;
	}

	UWorld* World = SourceComponent->GetWorld();
	if (!World)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoWorld", "Could not get world from source component."));
		return true;
	}

	// Find the foliage actor in the level
	AInstancedFoliageActor* FoliageActor = nullptr;
	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		FoliageActor = *It;
		break; // Use the first (typically only) one
	}

	if (!FoliageActor)
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoFoliageActor", "No InstancedFoliageActor found in the level."));
		return true;
	}

	// Optional mesh filter
	UStaticMesh* FilterMesh = Settings->FilterMesh.Get();

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	// Gather all ISM components from the foliage actor
	TArray<UInstancedStaticMeshComponent*> ISMComponents;
	FoliageActor->GetComponents<UInstancedStaticMeshComponent>(ISMComponents);

	if (ISMComponents.Num() == 0)
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoISMs", "InstancedFoliageActor has no instanced static mesh components."));
		return true;
	}

	// If not splitting, we use a single output data
	UPCGPointData* MergedPointData = nullptr;
	FPCGMetadataAttribute<FSoftObjectPath>* MergedMeshAttr = nullptr;
	FPCGMetadataAttribute<FSoftObjectPath>* MergedFoliageTypeAttr = nullptr;

	if (!Settings->bSplitByFoliageType)
	{
		MergedPointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		MergedPointData->InitializeFromData(nullptr);

		if (!Settings->MeshAttributeName.IsNone())
		{
			MergedMeshAttr = MergedPointData->Metadata->CreateAttribute<FSoftObjectPath>(
				Settings->MeshAttributeName, FSoftObjectPath(), false, false);
		}
		if (!Settings->FoliageTypeAttributeName.IsNone())
		{
			MergedFoliageTypeAttr = MergedPointData->Metadata->CreateAttribute<FSoftObjectPath>(
				Settings->FoliageTypeAttributeName, FSoftObjectPath(), false, false);
		}
	}

	// Iterate the foliage actor's foliage info map
	// Each entry maps a UFoliageType* to its FFoliageInfo (which contains the ISM component + instances)
	for (const auto& FoliagePair : FoliageActor->GetFoliageInfos())
	{
		const UFoliageType* FoliageType = FoliagePair.Key;
		const FFoliageInfo& FoliageInfo = *FoliagePair.Value;

		if (!FoliageType)
		{
			continue;
		}

		UStaticMesh* Mesh = Cast<UStaticMesh>(FoliageType->GetSource());
		if (!Mesh)
		{
			continue;
		}

		// Apply mesh filter if set
		if (FilterMesh && Mesh != FilterMesh)
		{
			continue;
		}

		// Get the ISM component for this foliage type
		UHierarchicalInstancedStaticMeshComponent* ISMComp = FoliageInfo.GetComponent();
		if (!ISMComp)
		{
			continue;
		}

		const int32 InstanceCount = ISMComp->GetInstanceCount();
		if (InstanceCount == 0)
		{
			continue;
		}

		// Target point data and attributes
		UPCGPointData* PointData = MergedPointData;
		FPCGMetadataAttribute<FSoftObjectPath>* MeshAttr = MergedMeshAttr;
		FPCGMetadataAttribute<FSoftObjectPath>* FoliageTypeAttr = MergedFoliageTypeAttr;

		if (Settings->bSplitByFoliageType)
		{
			PointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
			PointData->InitializeFromData(nullptr);

			if (!Settings->MeshAttributeName.IsNone())
			{
				MeshAttr = PointData->Metadata->CreateAttribute<FSoftObjectPath>(
					Settings->MeshAttributeName, FSoftObjectPath(), false, false);
			}
			if (!Settings->FoliageTypeAttributeName.IsNone())
			{
				FoliageTypeAttr = PointData->Metadata->CreateAttribute<FSoftObjectPath>(
					Settings->FoliageTypeAttributeName, FSoftObjectPath(), false, false);
			}
		}

		TArray<FPCGPoint>& Points = PointData->GetMutablePoints();
		const FSoftObjectPath MeshPath(Mesh);
		const FSoftObjectPath FoliageTypePath(FoliageType);

		// Extract each instance transform
		for (int32 i = 0; i < InstanceCount; ++i)
		{
			FTransform InstanceTransform;
			if (!ISMComp->GetInstanceTransform(i, InstanceTransform, /*bWorldSpace=*/true))
			{
				continue;
			}

			FPCGPoint& Point = Points.Add_GetRef(FPCGPoint());
			Point.Transform = InstanceTransform;
			Point.SetLocalBounds(FBox(FVector(-50.0), FVector(50.0))); // Reasonable default

			PCGMetadataEntryKey EntryKey = PointData->Metadata->AddEntry();
			Point.MetadataEntry = EntryKey;

			if (MeshAttr)
			{
				MeshAttr->SetValue(EntryKey, MeshPath);
			}
			if (FoliageTypeAttr)
			{
				FoliageTypeAttr->SetValue(EntryKey, FoliageTypePath);
			}
		}

		if (Settings->bSplitByFoliageType && Points.Num() > 0)
		{
			FPCGTaggedData& Output = Outputs.Add_GetRef(FPCGTaggedData());
			Output.Data = PointData;
			Output.Pin = PCGPinConstants::DefaultOutputLabel;
			Output.Tags.Add(Mesh->GetName());
		}
	}

	// Emit merged data if not splitting
	if (!Settings->bSplitByFoliageType && MergedPointData && MergedPointData->GetPoints().Num() > 0)
	{
		FPCGTaggedData& Output = Outputs.Add_GetRef(FPCGTaggedData());
		Output.Data = MergedPointData;
		Output.Pin = PCGPinConstants::DefaultOutputLabel;
	}

	const int32 TotalPoints = Settings->bSplitByFoliageType
		? Outputs.Num()
		: (MergedPointData ? MergedPointData->GetPoints().Num() : 0);

	UE_LOG(LogPCG, Log, TEXT("PGLGetFoliageInstances: Output %d points from %d foliage types"),
		Settings->bSplitByFoliageType ? -1 : TotalPoints, Outputs.Num());

	return true;
}

#undef LOCTEXT_NAMESPACE
