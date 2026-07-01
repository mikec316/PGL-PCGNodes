// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PGLAddComponent.h"

#include "PCGComponent.h"
#include "PCGManagedResource.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "Core/PCGExAssetCollection.h"
#include "PCGExCollectionsCommon.h"
#include "Helpers/PCGExCollectionsHelpers.h"

#include "PGLComponentCollection.h"

#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PGLAddComponent)

#define LOCTEXT_NAMESPACE "PGLAddComponent"

// ---------------------------------------------------------------------------
// UPGLAddComponentSettings
// ---------------------------------------------------------------------------

#if WITH_EDITOR
FText UPGLAddComponentSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Reads Component collection entries from the staging pipeline and creates\n"
		"component instances on the target actor at each point location.\n\n"
		"Connect the 'Out' pin (with CollectionType:Component tag) to 'In',\n"
		"and the 'Map' pin from PGL Staging Distribute to 'Map'.\n\n"
		"Each entry's component class is loaded and instantiated. Scene components\n"
		"are attached to the target actor and positioned at the point's transform.");
}
#endif

TArray<FPCGPinProperties> UPGLAddComponentSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace_GetRef(PGLAddComponentConstants::InputPointsLabel, EPCGDataType::Point).SetRequiredPin();
	PinProperties.Emplace_GetRef(PGLAddComponentConstants::InputMapLabel, EPCGDataType::Param).SetRequiredPin();
	return PinProperties;
}

TArray<FPCGPinProperties> UPGLAddComponentSettings::OutputPinProperties() const
{
	return DefaultPointOutputPinProperties();
}

FPCGElementPtr UPGLAddComponentSettings::CreateElement() const
{
	return MakeShared<FPGLAddComponentElement>();
}

// ---------------------------------------------------------------------------
// FPGLAddComponentElement
// ---------------------------------------------------------------------------

bool FPGLAddComponentElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPGLAddComponentElement::Execute);

	FPCGContext* Context = InContext;
	const UPGLAddComponentSettings* Settings = Context->GetInputSettings<UPGLAddComponentSettings>();
	check(Settings);

	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (!SourceComponent)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoSourceComponent", "Invalid source PCG component."));
		return true;
	}

	AActor* TargetActor = Settings->TargetActor.Get();
	if (!TargetActor)
	{
		TargetActor = Context->GetTargetActor(nullptr);
	}

	if (!TargetActor)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoTargetActor", "Invalid target actor. Ensure TargetActor is set or PCG component has a valid owner."));
		return true;
	}

	// --- Unpack collection map from Map pin ---
	PCGExCollections::FPickUnpacker Unpacker;
	Unpacker.UnpackPin(Context, PGLAddComponentConstants::InputMapLabel);

	if (!Unpacker.HasValidMapping())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoCollectionMap", "Could not rebuild a valid collection mapping from the provided map data."));
		return true;
	}

	const EObjectFlags ObjectFlags = SourceComponent->IsInPreviewMode() ? RF_Transient : RF_NoFlags;
	USceneComponent* RootComponent = TargetActor->GetRootComponent();

	// --- Pass through input points to output ---
#if WITH_EDITOR
	const bool bGenerateOutput = true;
#else
	const bool bGenerateOutput = Context->Node && Context->Node->IsOutputPinConnected(PCGPinConstants::DefaultOutputLabel);
#endif

	// --- Process each input point data ---
	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PGLAddComponentConstants::InputPointsLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	int32 TotalComponentsCreated = 0;

	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);
		if (!SpatialData)
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidInput", "Invalid input data, skipping."));
			continue;
		}

		const UPCGPointData* PointData = SpatialData->ToPointData(Context);
		if (!PointData)
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoPointData", "Unable to get point data from input, skipping."));
			continue;
		}

		const TArray<FPCGPoint>& Points = PointData->GetPoints();
		const int32 NumPoints = Points.Num();
		if (NumPoints == 0) { continue; }

		// Create mutable output point data with component reference attribute
		UPCGPointData* OutPointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		OutPointData->InitializeFromData(PointData);
		TArray<FPCGPoint>& MutablePoints = OutPointData->GetMutablePoints();
		MutablePoints = Points; // InitializeFromData only sets up metadata, must copy points explicitly

		// Create the component reference attribute on the output metadata
		FPCGMetadataAttribute<FSoftObjectPath>* CompRefAttr = nullptr;
		if (!Settings->ComponentReferenceAttributeName.IsNone())
		{
			CompRefAttr = OutPointData->Metadata->CreateAttribute<FSoftObjectPath>(
				Settings->ComponentReferenceAttributeName,
				FSoftObjectPath(),
				/*bAllowsInterpolation=*/false,
				/*bOverrideParent=*/false);
		}

		if (bGenerateOutput)
		{
			FPCGTaggedData& Output = Outputs.Add_GetRef(Input);
			Output.Data = OutPointData;
		}

		// Read entry hash attribute
		FPCGAttributePropertyInputSelector HashSelector;
		HashSelector.SetAttributeName(PCGExCollections::Labels::Tag_EntryIdx);

		TUniquePtr<const IPCGAttributeAccessor> HashAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(PointData, HashSelector);
		TUniquePtr<const IPCGAttributeAccessorKeys> HashKeys = PCGAttributeAccessorHelpers::CreateConstKeys(PointData, HashSelector);

		if (!HashAccessor || !HashKeys)
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("MissingHashAttr", "Could not find collection entry attribute on input points. Skipping."));
			continue;
		}

		// Process each point
		for (int32 PointIdx = 0; PointIdx < NumPoints; ++PointIdx)
		{
			int64 EntryHash = 0;
			if (!HashAccessor->Get<int64>(EntryHash, PointIdx, *HashKeys) || EntryHash == -1)
			{
				if (Settings->bEnableDebugLogging)
				{
					UE_LOG(LogPCG, Warning, TEXT("PGLAddComponent: Point %d has no valid entry hash, skipping."), PointIdx);
				}
				continue;
			}

			// Resolve entry from hash
			int16 SecondaryIndex = -1;
			FPCGExEntryAccessResult Result = Unpacker.ResolveEntry(static_cast<uint64>(EntryHash), SecondaryIndex);

			if (!Result.IsValid())
			{
				if (Settings->bEnableDebugLogging)
				{
					UE_LOG(LogPCG, Warning, TEXT("PGLAddComponent: Point %d - entry hash %lld could not be resolved. Skipping."), PointIdx, EntryHash);
				}
				continue;
			}

			// Must be a Component entry
			if (!Result.IsType(PCGExAssetCollection::TypeIds::Component))
			{
				if (Settings->bEnableDebugLogging)
				{
					UE_LOG(LogPCG, Log, TEXT("PGLAddComponent: Point %d - entry is not a Component type. Skipping."), PointIdx);
				}
				continue;
			}

			const FPGLComponentCollectionEntry* CompEntry = Result.As<FPGLComponentCollectionEntry>();
			if (!CompEntry || CompEntry->Component.IsNull())
			{
				if (Settings->bEnableDebugLogging)
				{
					UE_LOG(LogPCG, Warning, TEXT("PGLAddComponent: Point %d - component entry is null or has no class. Skipping."), PointIdx);
				}
				continue;
			}

			// Load the component class
			UClass* LoadedClass = CompEntry->Component.LoadSynchronous();
			if (!LoadedClass)
			{
				PCGE_LOG(Warning, GraphAndLog, FText::Format(
					LOCTEXT("FailedToLoadClass", "Failed to load component class '{0}'. Skipping."),
					FText::FromString(CompEntry->Component.ToString())));
				continue;
			}

			if (!LoadedClass->IsChildOf(UActorComponent::StaticClass()))
			{
				PCGE_LOG(Warning, GraphAndLog, FText::Format(
					LOCTEXT("InvalidClass", "Class '{0}' is not a UActorComponent subclass. Skipping."),
					FText::FromString(LoadedClass->GetName())));
				continue;
			}

			// Create the component instance
			const FName UniqueName = MakeUniqueObjectName(TargetActor, LoadedClass, LoadedClass->GetFName());
			UActorComponent* NewComponent = NewObject<UActorComponent>(TargetActor, LoadedClass, UniqueName, ObjectFlags);

			if (!NewComponent)
			{
				PCGE_LOG(Warning, GraphAndLog, FText::Format(
					LOCTEXT("FailedToCreate", "Failed to create component of class '{0}'. Skipping."),
					FText::FromString(LoadedClass->GetName())));
				continue;
			}

			// If it's a scene component, attach and set transform from the point
			if (USceneComponent* SceneComp = Cast<USceneComponent>(NewComponent))
			{
				if (RootComponent)
				{
					SceneComp->AttachToComponent(
						RootComponent,
						FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false));
				}

				SceneComp->SetWorldTransform(MutablePoints[PointIdx].Transform);
			}

			// Register and add to actor
			NewComponent->RegisterComponent();
			TargetActor->AddInstanceComponent(NewComponent);

			// Tag for PCG tracking
			NewComponent->ComponentTags.AddUnique(PCGHelpers::DefaultPCGTag);
			NewComponent->ComponentTags.AddUnique(SourceComponent->GetFName());

			// Write component reference to output attribute
			if (CompRefAttr)
			{
				FPCGPoint& OutPoint = MutablePoints[PointIdx];
				if (OutPoint.MetadataEntry == PCGInvalidEntryKey)
				{
					OutPoint.MetadataEntry = OutPointData->Metadata->AddEntry();
				}
				CompRefAttr->SetValue(OutPoint.MetadataEntry, FSoftObjectPath(NewComponent));
			}

			// Create managed resource for PCG cleanup lifecycle
			UPCGManagedComponent* ManagedComponent = NewObject<UPCGManagedComponent>(SourceComponent);
			ManagedComponent->GeneratedComponent = NewComponent;
			SourceComponent->AddToManagedResources(ManagedComponent);

			++TotalComponentsCreated;

			if (Settings->bEnableDebugLogging)
			{
				UE_LOG(LogPCG, Log, TEXT("PGLAddComponent: Created component '%s' (%s) on actor '%s' at point %d"),
					*NewComponent->GetName(),
					*LoadedClass->GetName(),
					*TargetActor->GetName(),
					PointIdx);
			}
		}
	}

	UE_LOG(LogPCG, Log, TEXT("PGLAddComponent: %d component(s) created on actor '%s'"),
		TotalComponentsCreated, *TargetActor->GetName());

	// Post-process functions on target actor
	for (UFunction* Function : PCGHelpers::FindUserFunctions(
		TargetActor->GetClass(),
		Settings->PostProcessFunctionNames,
		{ UPCGFunctionPrototypes::GetPrototypeWithNoParams() },
		Context))
	{
		TargetActor->ProcessEvent(Function, nullptr);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
