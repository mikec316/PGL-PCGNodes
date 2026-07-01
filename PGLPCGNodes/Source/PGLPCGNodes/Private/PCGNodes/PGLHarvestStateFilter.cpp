// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PGLHarvestStateFilter.h"

#include "PCGContext.h"
#include "PCGGraphExecutionStateInterface.h"
#include "PCGPin.h"
#include "Data/PCGBasePointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "Harvest/PGLHarvestSettings.h"
#include "Harvest/PGLHarvestableSubsystem.h"

#include "Algo/Sort.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PGLHarvestStateFilter)

#define LOCTEXT_NAMESPACE "PGLHarvestStateFilterElement"

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
namespace PGLHarvestStateFilterInternal
{
	/** Copy a subset of points into a fresh output point data (mirrors PCGAssignOre's helper). */
	UPCGBasePointData* CopyPointSubset(
		const UPCGBasePointData* InPointData,
		TArrayView<int32> InIndexes,
		FPCGContext* InContext)
	{
		UPCGBasePointData* OutPointData = FPCGContext::NewPointData_AnyThread(InContext);

		FPCGInitializeFromDataParams InitParams(InPointData);
		InitParams.bInheritSpatialData = false;
		OutPointData->InitializeFromDataWithParams(InitParams);

		OutPointData->SetNumPoints(InIndexes.Num());
		// MetadataEntry must be allocated for the per-point attribute writes below.
		OutPointData->AllocateProperties(InPointData->GetAllocatedProperties() | EPCGPointNativeProperties::MetadataEntry);
		OutPointData->CopyUnallocatedPropertiesFrom(InPointData);

		Algo::Sort(InIndexes);

		const FConstPCGPointValueRanges InRanges(InPointData);
		FPCGPointValueRanges OutRanges(OutPointData, /*bAllocate=*/false);

		int32 WriteIndex = 0;
		for (const int32 ReadIndex : InIndexes)
		{
			OutRanges.SetFromValueRanges(WriteIndex, InRanges, ReadIndex);
			++WriteIndex;
		}

		return OutPointData;
	}

	/** Create (if needed) and bulk-write a per-point attribute on Data. */
	template <typename T>
	void WriteAttribute(UPCGBasePointData* Data, FName AttributeName, const TArray<T>& Values)
	{
		if (Values.IsEmpty())
		{
			return;
		}

		UPCGMetadata* Metadata = Data->MutableMetadata();
		if (!Metadata || !Metadata->FindOrCreateAttribute<T>(AttributeName, T{}, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true))
		{
			return;
		}

		FPCGAttributePropertyOutputSelector Selector;
		Selector.SetAttributeName(AttributeName);

		TUniquePtr<IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateAccessor(Data, Selector);
		TUniquePtr<IPCGAttributeAccessorKeys> Keys = PCGAttributeAccessorHelpers::CreateKeys(Data, Selector);
		if (Accessor.IsValid() && Keys.IsValid())
		{
			Accessor->SetRange<T>(MakeConstArrayView(Values), 0, *Keys);
		}
	}
}

// -----------------------------------------------------------------------------
// UPGLHarvestStateFilterSettings
// -----------------------------------------------------------------------------

#if WITH_EDITOR
FText UPGLHarvestStateFilterSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Re-applies the harvest registry (chopped/stripped/destroyed foliage) to points during generation.\n"
		"Destroyed -> Discarded, Stumped -> Stumped pin (wire to the stump spawner), everything else -> Out\n"
		"with PGLStripped (float 0/1) for the berry-mask custom data packer.\n"
		"Place after points have final world positions, before the spawners.");
}
#endif

TArray<FPCGPinProperties> UPGLHarvestStateFilterSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point).SetRequiredPin();
	return PinProperties;
}

TArray<FPCGPinProperties> UPGLHarvestStateFilterSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PGLHarvestStateFilterConstants::OutputLabel, EPCGDataType::Point);
	PinProperties.Emplace(PGLHarvestStateFilterConstants::StumpedLabel, EPCGDataType::Point);
	PinProperties.Emplace(PGLHarvestStateFilterConstants::DiscardedLabel, EPCGDataType::Point);
	return PinProperties;
}

FPCGElementPtr UPGLHarvestStateFilterSettings::CreateElement() const
{
	return MakeShared<FPGLHarvestStateFilterElement>();
}

// -----------------------------------------------------------------------------
// FPGLHarvestStateFilterElement
// -----------------------------------------------------------------------------

bool FPGLHarvestStateFilterElement::CanExecuteOnlyOnMainThread(FPCGContext* Context) const
{
	// Subsystem/snapshot resolution must happen on the game thread; the per-point pass is pure
	// reads against the immutable snapshot and runs wherever the scheduler likes.
	return !Context || Context->CurrentPhase == EPCGExecutionPhase::PrepareData;
}

FPCGContext* FPGLHarvestStateFilterElement::CreateContext()
{
	return new FPGLHarvestStateFilterContext();
}

bool FPGLHarvestStateFilterElement::PrepareDataInternal(FPCGContext* InContext) const
{
	FPGLHarvestStateFilterContext* Context = static_cast<FPGLHarvestStateFilterContext*>(InContext);
	check(Context);

	const UPGLHarvestSettings* HarvestSettings = GetDefault<UPGLHarvestSettings>();
	Context->CellSizeXY = HarvestSettings->KeyCellSizeXY;
	Context->CellSizeZ = HarvestSettings->KeyCellSizeZ;

	// No subsystem outside game worlds (editor preview gen) — everything passes through Pristine.
	IPCGGraphExecutionSource* ExecutionSource = Context->ExecutionSource.Get();
	UWorld* World = ExecutionSource ? ExecutionSource->GetExecutionState().GetWorld() : nullptr;
	if (World && World->IsGameWorld())
	{
		if (UPGLHarvestableSubsystem* Harvest = World->GetSubsystem<UPGLHarvestableSubsystem>())
		{
			Context->Snapshot = Harvest->GetSnapshot();
		}
	}

	return true;
}

bool FPGLHarvestStateFilterElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPGLHarvestStateFilterElement::Execute);

	FPGLHarvestStateFilterContext* Context = static_cast<FPGLHarvestStateFilterContext*>(InContext);
	check(Context);

	const UPGLHarvestStateFilterSettings* Settings = Context->GetInputSettings<UPGLHarvestStateFilterSettings>();
	check(Settings);

	const TMap<uint64, FPGLHarvestSnapshotEntry>* States = Context->Snapshot ? &Context->Snapshot->States : nullptr;

	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (int32 InputIndex = 0; InputIndex < Inputs.Num(); ++InputIndex)
	{
		const FPCGTaggedData& CurrentInput = Inputs[InputIndex];

		const UPCGBasePointData* InputPointData = Cast<UPCGBasePointData>(CurrentInput.Data);
		if (!InputPointData)
		{
			PCGE_LOG(Verbose, GraphAndLog, FText::Format(LOCTEXT("InvalidData", "Input {0} is not point data, skipping."), InputIndex));
			continue;
		}

		const int32 NumPoints = InputPointData->GetNumPoints();
		if (NumPoints == 0)
		{
			continue;
		}

		const TConstPCGValueRange<FTransform> TransformRange = InputPointData->GetConstTransformValueRange();

		TArray<int32> OutIndices;
		TArray<int32> StumpIndices;
		TArray<int32> DiscardedIndices;
		OutIndices.Reserve(NumPoints);

		// Parallel to OutIndices / StumpIndices respectively.
		TArray<float> OutStripped;
		TArray<int64> OutKeys;
		TArray<int64> StumpKeys;
		OutStripped.Reserve(NumPoints);

		for (int32 PointIndex = 0; PointIndex < NumPoints; ++PointIndex)
		{
			const FVector Location = TransformRange[PointIndex].GetLocation();

			uint64 CandidateKeys[3];
			PGLHarvest::MakeCandidateKeys(Location, Context->CellSizeXY, Context->CellSizeZ, CandidateKeys);

			const FPGLHarvestSnapshotEntry* Entry = nullptr;
			uint64 MatchedKey = CandidateKeys[0];
			if (States && !States->IsEmpty())
			{
				for (int32 CandidateIndex = 0; CandidateIndex < 3; ++CandidateIndex)
				{
					const FPGLHarvestSnapshotEntry* Found = States->Find(CandidateKeys[CandidateIndex]);
					if (!Found)
					{
						continue;
					}
					// Neighbor-Z-cell candidates only count within one Z cell of the registry's
					// stored Z — genuine terrain drift matches, vertically stacked plants don't.
					if (CandidateIndex > 0 && FMath::Abs(Location.Z - Found->WorldZ) > Context->CellSizeZ)
					{
						continue;
					}
					Entry = Found;
					MatchedKey = CandidateKeys[CandidateIndex];
					break;
				}
			}

			const EPGLHarvestState State = Entry ? Entry->State : EPGLHarvestState::Pristine;
			switch (State)
			{
			case EPGLHarvestState::Destroyed:
				DiscardedIndices.Add(PointIndex);
				break;
			case EPGLHarvestState::Stumped:
				if (Entry->bOverlayOwned)
				{
					// The runtime overlay manager already shows this stump — spawning would duplicate it.
					DiscardedIndices.Add(PointIndex);
				}
				else
				{
					StumpIndices.Add(PointIndex);
					StumpKeys.Add(static_cast<int64>(MatchedKey));
				}
				break;
			default:
				OutIndices.Add(PointIndex);
				OutStripped.Add(State == EPGLHarvestState::Stripped ? 1.0f : 0.0f);
				OutKeys.Add(static_cast<int64>(MatchedKey));
				break;
			}
		}

		// --- "Out" (pristine + stripped, with the berry-mask attribute) ---
		{
			UPCGBasePointData* OutPointData = PGLHarvestStateFilterInternal::CopyPointSubset(InputPointData, OutIndices, Context);
			PGLHarvestStateFilterInternal::WriteAttribute<float>(OutPointData, PGLHarvest::StrippedAttributeName, OutStripped);
			if (Settings->bWriteKeyAttribute)
			{
				PGLHarvestStateFilterInternal::WriteAttribute<int64>(OutPointData, PGLHarvest::HarvestKeyAttributeName, OutKeys);
			}

			FPCGTaggedData& Output = Outputs.Add_GetRef(CurrentInput);
			Output.Data = OutPointData;
			Output.Pin = PGLHarvestStateFilterConstants::OutputLabel;
		}

		// --- "Stumped" (route to the stump-mesh spawner) ---
		{
			UPCGBasePointData* StumpPointData = PGLHarvestStateFilterInternal::CopyPointSubset(InputPointData, StumpIndices, Context);
			if (Settings->bWriteKeyAttribute)
			{
				PGLHarvestStateFilterInternal::WriteAttribute<int64>(StumpPointData, PGLHarvest::HarvestKeyAttributeName, StumpKeys);
			}

			FPCGTaggedData& Output = Outputs.Add_GetRef(CurrentInput);
			Output.Data = StumpPointData;
			Output.Pin = PGLHarvestStateFilterConstants::StumpedLabel;
		}

		// --- "Discarded" (tombstoned — kept visible for debugging) ---
		{
			UPCGBasePointData* DiscardedPointData = PGLHarvestStateFilterInternal::CopyPointSubset(InputPointData, DiscardedIndices, Context);

			FPCGTaggedData& Output = Outputs.Add_GetRef(CurrentInput);
			Output.Data = DiscardedPointData;
			Output.Pin = PGLHarvestStateFilterConstants::DiscardedLabel;
		}

		if (StumpIndices.Num() + DiscardedIndices.Num() > 0)
		{
			PCGE_LOG(Verbose, LogOnly, FText::Format(LOCTEXT("FilterInfo", "Harvest filter: {0} pristine/stripped, {1} stumped, {2} culled of {3} points."),
				OutIndices.Num(), StumpIndices.Num(), DiscardedIndices.Num(), NumPoints));
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
