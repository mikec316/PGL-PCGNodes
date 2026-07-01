// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PGLStagingVoxelStamp.h"

#include "Graphs/VoxelHeightGraphStamp.h"
#include "Graphs/VoxelVolumeGraphStamp.h"
#include "VoxelPCGHelpers.h"
#include "VoxelPCGTracker.h"
#include "VoxelStampManager.h"
#include "VoxelInvalidationCallstack.h"
#include "VoxelInstancedStampComponent.h"
#include "PCGManagedVoxelInstancedStampComponent.h"

#include "PCGComponent.h"
#include "PCGEdge.h"
#include "PCGParamData.h"
#include "Data/PCGBasePointData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "Core/PCGExAssetCollection.h"
#include "PCGExCollectionsCommon.h"
#include "Helpers/PCGExCollectionsHelpers.h"

#include "Graphs/VoxelHeightGraph.h"
#include "Graphs/VoxelVolumeGraph.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PGLStagingVoxelStamp)

#define LOCTEXT_NAMESPACE "PGLStagingVoxelStamp"

// ---------------------------------------------------------------------------
// UPGLStagingVoxelStampSettings
// ---------------------------------------------------------------------------

#if WITH_EDITOR
FText UPGLStagingVoxelStampSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Reads VoxelGraph collection entries from the staging pipeline and spawns\n"
		"voxel stamps (height or volume graph stamps) at each point location.\n\n"
		"Connect the 'Out' pin (with CollectionType:VoxelGraph tag) to 'In',\n"
		"and the 'Map' pin from PGL Staging Distribute to 'Map'.\n\n"
		"Each entry's VoxelGraph, ParameterOverrides, and StampProperties are\n"
		"applied to the spawned stamps. Entries without stamp property overrides\n"
		"use the node's DefaultStampProperties.");
}
#endif

TArray<FPCGPinProperties> UPGLStagingVoxelStampSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace_GetRef(PGLStagingVoxelStampConstants::InputPointsLabel, EPCGDataType::Point).SetRequiredPin();
	PinProperties.Emplace_GetRef(PGLStagingVoxelStampConstants::InputMapLabel, EPCGDataType::Param).SetRequiredPin();
	return PinProperties;
}

TArray<FPCGPinProperties> UPGLStagingVoxelStampSettings::OutputPinProperties() const
{
	return DefaultPointOutputPinProperties();
}

FPCGElementPtr UPGLStagingVoxelStampSettings::CreateElement() const
{
	return MakeShared<FPGLStagingVoxelStampElement>();
}

// ---------------------------------------------------------------------------
// FPGLStagingVoxelStampElement
// ---------------------------------------------------------------------------

bool FPGLStagingVoxelStampElement::CanExecuteOnlyOnMainThread(FPCGContext* Context) const
{
	return
		!Context ||
		Context->CurrentPhase == EPCGExecutionPhase::Execute ||
		Context->CurrentPhase == EPCGExecutionPhase::PrepareData;
}

FPCGContext* FPGLStagingVoxelStampElement::CreateContext()
{
	return new FPGLStagingVoxelStampContext();
}

bool FPGLStagingVoxelStampElement::PrepareDataInternal(FPCGContext* InContext) const
{
	FPGLStagingVoxelStampContext* Context = static_cast<FPGLStagingVoxelStampContext*>(InContext);
	const UPGLStagingVoxelStampSettings* Settings = Context->GetInputSettings<UPGLStagingVoxelStampSettings>();
	check(Settings);

	UPCGComponent* Component = GetPCGComponent(*Context);
	if (!ensure(Component))
	{
		return true;
	}

#if WITH_EDITOR
	const bool bGenerateOutput = true;
#else
	const bool bGenerateOutput = Context->Node && Context->Node->IsOutputPinConnected(PCGPinConstants::DefaultOutputLabel);
#endif

	// Reuse check: skip re-execution when inputs haven't changed
	if (!Context->bReuseCheckDone)
	{
		if (!Context->DependenciesCrc.IsValid())
		{
#if VOXEL_ENGINE_VERSION < 506
			GetDependenciesCrc(Context->InputData, Settings, Component, Context->DependenciesCrc);
#else
			GetDependenciesCrc(FPCGGetDependenciesCrcParams(&Context->InputData, Settings, Context->ExecutionSource.Get()), Context->DependenciesCrc);
#endif
		}

		if (Context->DependenciesCrc.IsValid())
		{
			TArray<UPCGManagedVoxelInstancedStampComponent*> ManagedComponents;
			Component->ForEachManagedResource([&ManagedComponents, &Context, Settings](UPCGManagedResource* InResource)
			{
				if (UPCGManagedVoxelInstancedStampComponent* Resource = Cast<UPCGManagedVoxelInstancedStampComponent>(InResource))
				{
					if (Resource->GetSettingsUID() == Settings->GetStableUID() &&
						Resource->GetCrc().IsValid() &&
						Resource->GetCrc() == Context->DependenciesCrc)
					{
						ManagedComponents.Add(Resource);
					}
				}
			});

			for (UPCGManagedVoxelInstancedStampComponent* ManagedComp : ManagedComponents)
			{
				ManagedComp->MarkAsReused();
			}

			if (!ManagedComponents.IsEmpty())
			{
				Context->bSkippedDueToReuse = true;
			}
		}

		Context->bReuseCheckDone = true;
	}

	if (!bGenerateOutput && Context->bSkippedDueToReuse)
	{
		return true;
	}

	// Pass through input points to output
	if (bGenerateOutput)
	{
		const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PGLStagingVoxelStampConstants::InputPointsLabel);
		TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

		for (const FPCGTaggedData& Input : Inputs)
		{
			const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);
			if (!SpatialData) { continue; }

#if VOXEL_ENGINE_VERSION >= 506
			const UPCGBasePointData* PointData = SpatialData->ToBasePointData(Context);
#else
			const UPCGPointData* PointData = SpatialData->ToPointData(Context);
#endif
			if (!PointData) { continue; }

			FPCGTaggedData& Output = Outputs.Add_GetRef(Input);
			Output.Data = PointData;
		}
	}

	return true;
}

// ---------------------------------------------------------------------------
// Helper: resolve stamp properties from entry or defaults
// ---------------------------------------------------------------------------
namespace PGLStagingVoxelStampInternal
{
	const FPGLVoxelStampProperties& GetEffectiveStampProperties(
		const FPGLVoxelGraphCollectionEntry* Entry,
		const FPGLVoxelStampProperties& Defaults)
	{
		if (Entry->bOverrideStampProperties && Entry->StampProperties.StampType != EPGLVoxelStampType::None)
		{
			return Entry->StampProperties;
		}
		return Defaults;
	}

	void ApplyCommonStampProperties(FVoxelStamp& Stamp, const FPGLVoxelStampProperties& Props)
	{
		Stamp.Behavior = Props.Behavior;
		Stamp.Priority = Props.Priority;
		Stamp.Smoothness = Props.Smoothness;
	}

	FVoxelStampRef CreateHeightGraphStamp(
		const FPGLVoxelGraphCollectionEntry* Entry,
		const FPGLVoxelStampProperties& Props,
		const FTransform& PointTransform)
	{
		FVoxelHeightGraphStamp HeightStamp;

		// Set graph
		UVoxelGraph* LoadedGraph = Entry->VoxelGraph.LoadSynchronous();
		HeightStamp.Graph = Cast<UVoxelHeightGraph>(LoadedGraph);

		// Copy parameter overrides from entry
		HeightStamp.ParameterOverrides = Entry->ParameterOverrides;

		// Common stamp properties
		ApplyCommonStampProperties(HeightStamp, Props);

		// Height-specific properties
		if (Props.HeightLayer.ToSoftObjectPath().IsValid())
		{
			HeightStamp.Layer = Props.HeightLayer.LoadSynchronous();
		}
		HeightStamp.BlendMode = Props.HeightBlendMode;
		HeightStamp.HeightPaddingMultiplier = Props.HeightPaddingMultiplier;

		// Transform
		HeightStamp.Transform = PointTransform;

		return FVoxelStampRef::New(HeightStamp);
	}

	FVoxelStampRef CreateVolumeGraphStamp(
		const FPGLVoxelGraphCollectionEntry* Entry,
		const FPGLVoxelStampProperties& Props,
		const FTransform& PointTransform)
	{
		FVoxelVolumeGraphStamp VolumeStamp;

		// Set graph
		UVoxelGraph* LoadedGraph = Entry->VoxelGraph.LoadSynchronous();
		VolumeStamp.Graph = Cast<UVoxelVolumeGraph>(LoadedGraph);

		// Copy parameter overrides from entry
		VolumeStamp.ParameterOverrides = Entry->ParameterOverrides;

		// Common stamp properties
		ApplyCommonStampProperties(VolumeStamp, Props);

		// Volume-specific properties
		if (Props.VolumeLayer.ToSoftObjectPath().IsValid())
		{
			VolumeStamp.Layer = Props.VolumeLayer.LoadSynchronous();
		}
		VolumeStamp.BlendMode = Props.VolumeBlendMode;
		VolumeStamp.BoundsExtensionMultiplier = Props.BoundsExtensionMultiplier;
		VolumeStamp.MaximumBoundsExtension = Props.MaximumBoundsExtension;

		// Transform
		VolumeStamp.Transform = PointTransform;

		return FVoxelStampRef::New(VolumeStamp);
	}
}

// ---------------------------------------------------------------------------
// ExecuteInternal
// ---------------------------------------------------------------------------

bool FPGLStagingVoxelStampElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPGLStagingVoxelStampElement::Execute);

	FPGLStagingVoxelStampContext* Context = static_cast<FPGLStagingVoxelStampContext*>(InContext);
	const UPGLStagingVoxelStampSettings* Settings = Context->GetInputSettings<UPGLStagingVoxelStampSettings>();
	check(Settings);

	if (Context->bSkippedDueToReuse)
	{
		return true;
	}

	UPCGComponent* Component = GetPCGComponent(*Context);
	if (!ensure(Component))
	{
		return true;
	}

	AActor* TargetActor = Settings->TargetActor.Get() ? Settings->TargetActor.Get() : Context->GetTargetActor(nullptr);
	if (!TargetActor)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoTargetActor", "Invalid target actor. Ensure TargetActor is set or PCG component has a valid owner."));
		return true;
	}

	// --- Unpack collection map from Map pin ---
	PCGExCollections::FPickUnpacker Unpacker;
	Unpacker.UnpackPin(Context, PGLStagingVoxelStampConstants::InputMapLabel);

	if (!Unpacker.HasValidMapping())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoCollectionMap", "Could not rebuild a valid collection mapping from the provided map data."));
		return true;
	}

	// --- Build managed component ---
	UPCGManagedVoxelInstancedStampComponent* Resource = GetOrCreateManagedComponent(
		TargetActor,
		Component,
		Settings->GetStableUID());

	check(Resource);
	Resource->SetCrc(Context->DependenciesCrc);

	UVoxelInstancedStampComponent* InstancedStampComponent = Resource->GetComponent();
	check(InstancedStampComponent);

	if (const USceneComponent* SceneComponent = TargetActor->GetRootComponent())
	{
		InstancedStampComponent->Mobility = SceneComponent->Mobility;
	}

	// --- Build invalidation callstack ---
	const TSharedRef<FVoxelInvalidationCallstack> Callstack = FVoxelInvalidationFrame_PCG::Create(
		Component,
		*Settings,
		FName(TEXT("PGL Staging Voxel Stamp")));

	FVoxelInvalidationScope Scope(Callstack);

	// --- Process each input point data ---
	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PGLStagingVoxelStampConstants::InputPointsLabel);

	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);
		if (!SpatialData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInput", "Invalid input data."));
			continue;
		}

#if VOXEL_ENGINE_VERSION >= 506
		const UPCGBasePointData* PointData = SpatialData->ToBasePointData(Context);
#else
		const UPCGPointData* PointData = SpatialData->ToPointData(Context);
#endif
		if (!PointData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoPointData", "Unable to get point data from input."));
			continue;
		}

#if VOXEL_ENGINE_VERSION >= 506
		const int32 NumPoints = PointData->GetNumPoints();
		const TConstPCGValueRange<FTransform> TransformRange = PointData->GetConstTransformValueRange();
#else
		const TArray<FPCGPoint>& Points = PointData->GetPoints();
		const int32 NumPoints = Points.Num();
#endif

		if (NumPoints == 0) { continue; }

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

		// Build stamps
		TVoxelArray<FVoxelStampRef> NewStamps;
		int32 StampsCreated = 0;

		for (int32 PointIdx = 0; PointIdx < NumPoints; ++PointIdx)
		{
			int64 EntryHash = 0;
			if (!HashAccessor->Get<int64>(EntryHash, PointIdx, *HashKeys) || EntryHash == -1)
			{
				continue;
			}

			// Resolve entry from hash
			int16 SecondaryIndex = -1;
			FPCGExEntryAccessResult Result = Unpacker.ResolveEntry(static_cast<uint64>(EntryHash), SecondaryIndex);

			if (!Result.IsValid())
			{
				continue;
			}

			// Must be a VoxelGraph entry
			if (!Result.IsType(PCGExAssetCollection::TypeIds::VoxelGraph))
			{
				continue; // Silently skip non-VoxelGraph entries
			}

			const FPGLVoxelGraphCollectionEntry* VGEntry = Result.As<FPGLVoxelGraphCollectionEntry>();
			if (!VGEntry || VGEntry->VoxelGraph.IsNull())
			{
				continue;
			}

			// Get effective stamp properties (entry override or node defaults)
			using namespace PGLStagingVoxelStampInternal;
			const FPGLVoxelStampProperties& Props = GetEffectiveStampProperties(VGEntry, Settings->DefaultStampProperties);

			if (Props.StampType == EPGLVoxelStampType::None)
			{
				PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoStampType",
					"Stamp type is None for an entry and no default stamp type is set. Skipping."));
				continue;
			}

#if VOXEL_ENGINE_VERSION >= 506
			const FTransform& PointTransform = TransformRange[PointIdx];
#else
			const FTransform& PointTransform = Points[PointIdx].Transform;
#endif

			FVoxelStampRef StampRef;

			if (Props.StampType == EPGLVoxelStampType::Height)
			{
				StampRef = CreateHeightGraphStamp(VGEntry, Props, PointTransform);
			}
			else if (Props.StampType == EPGLVoxelStampType::Volume)
			{
				StampRef = CreateVolumeGraphStamp(VGEntry, Props, PointTransform);
			}

			if (StampRef)
			{
				NewStamps.Add(MoveTemp(StampRef));
				++StampsCreated;
			}
		}

		if (StampsCreated > 0)
		{
			InstancedStampComponent->AddStamps_NoCopy(MoveTemp(NewStamps));
		}

		UE_LOG(LogPCG, Log, TEXT("PGLStagingVoxelStamp: %d points -> %d stamps created"), NumPoints, StampsCreated);
	}

	// Post-process functions
	for (UFunction* Function : PCGHelpers::FindUserFunctions(TargetActor->GetClass(), Settings->PostProcessFunctionNames, { UPCGFunctionPrototypes::GetPrototypeWithNoParams() }, Context))
	{
		TargetActor->ProcessEvent(Function, nullptr);
	}

	return true;
}

// ---------------------------------------------------------------------------
// GetOrCreateManagedComponent
// ---------------------------------------------------------------------------

UPCGManagedVoxelInstancedStampComponent* FPGLStagingVoxelStampElement::GetOrCreateManagedComponent(
	AActor* InTargetActor,
	UPCGComponent* InSourceComponent,
	uint64 SettingsUID)
{
	check(InTargetActor && InSourceComponent);

	const auto AddTagsToComponent = [InSourceComponent](UVoxelInstancedStampComponent* ISC)
	{
		ISC->ComponentTags.AddUnique(PCGHelpers::DefaultPCGTag);
		ISC->ComponentTags.AddUnique(InSourceComponent->GetFName());
	};

	// Look for existing matching managed component
	{
		UPCGManagedVoxelInstancedStampComponent* MatchingResource = nullptr;
		InSourceComponent->ForEachManagedResource([&MatchingResource, &InTargetActor, SettingsUID](UPCGManagedResource* InResource)
		{
			if (MatchingResource) { return; }

			if (UPCGManagedVoxelInstancedStampComponent* Resource = Cast<UPCGManagedVoxelInstancedStampComponent>(InResource))
			{
				if (Resource->GetSettingsUID() != SettingsUID || !Resource->CanBeUsed())
				{
					return;
				}

				if (const UVoxelInstancedStampComponent* ISC = Resource->GetComponent())
				{
					if (IsValid(ISC) && ISC->GetOwner() == InTargetActor)
					{
						MatchingResource = Resource;
					}
				}
			}
		});

		if (MatchingResource)
		{
			MatchingResource->MarkAsUsed();
			UVoxelInstancedStampComponent* ISC = Cast<UVoxelInstancedStampComponent>(MatchingResource->GeneratedComponent.Get());
			if (ensure(ISC))
			{
				ISC->Modify(!InSourceComponent->IsInPreviewMode());
				AddTagsToComponent(ISC);
			}
			return MatchingResource;
		}
	}

	// Create a new UVoxelInstancedStampComponent
	InTargetActor->Modify(!InSourceComponent->IsInPreviewMode());

	const EObjectFlags ObjectFlags = InSourceComponent->IsInPreviewMode() ? RF_Transient : RF_NoFlags;
	UVoxelInstancedStampComponent* NewISC = NewObject<UVoxelInstancedStampComponent>(
		InTargetActor,
		UVoxelInstancedStampComponent::StaticClass(),
		MakeUniqueObjectName(InTargetActor, UVoxelInstancedStampComponent::StaticClass(), {}),
		ObjectFlags);

	NewISC->RegisterComponent();
	InTargetActor->AddInstanceComponent(NewISC);

	NewISC->AttachToComponent(
		InTargetActor->GetRootComponent(),
		FAttachmentTransformRules(EAttachmentRule::KeepRelative, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false));

	AddTagsToComponent(NewISC);

	// Create managed resource on source component
	UPCGManagedVoxelInstancedStampComponent* ManagedResource = NewObject<UPCGManagedVoxelInstancedStampComponent>(InSourceComponent);
	ManagedResource->SetComponent(NewISC);

	if (InTargetActor->GetRootComponent())
	{
		ManagedResource->SetRootLocation(InTargetActor->GetRootComponent()->GetComponentLocation());
	}

	ManagedResource->SetSettingsUID(SettingsUID);
	InSourceComponent->AddToManagedResources(ManagedResource);

	return ManagedResource;
}

#undef LOCTEXT_NAMESPACE
