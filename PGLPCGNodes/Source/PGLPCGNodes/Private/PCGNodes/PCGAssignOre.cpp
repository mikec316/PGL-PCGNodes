// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PCGAssignOre.h"

#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGBasePointData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGAssignOre)

#define LOCTEXT_NAMESPACE "PCGAssignOreElement"

// -----------------------------------------------------------------------------
// Helper: copy a subset of points from input to a new output point data
// -----------------------------------------------------------------------------
namespace PCGAssignOreInternal
{
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
		OutPointData->AllocateProperties(InPointData->GetAllocatedProperties());
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
}

// -----------------------------------------------------------------------------
// UPCGAssignOreSettings
// -----------------------------------------------------------------------------

#if WITH_EDITOR
FName UPCGAssignOreSettings::GetDefaultNodeName() const
{
	return FName(TEXT("AssignOreFromDataTable"));
}

FText UPCGAssignOreSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Assign Ore From Data Table");
}

FText UPCGAssignOreSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Assigns ore types to points based on a data table.\n"
		"Surface points use SurfaceSpawnRate. Buried points use depth-weighted BuriedSpawnRate.\n"
		"Each point receives OreType and OreVoxelGraph attributes.\n"
		"Points that fail all spawn rolls go to the Discarded output.");
}
#endif

TArray<FPCGPinProperties> UPCGAssignOreSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point).SetRequiredPin();
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGAssignOreSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGAssignOreConstants::OutputLabel, EPCGDataType::Point);
	PinProperties.Emplace(PCGAssignOreConstants::DiscardedLabel, EPCGDataType::Point);
	return PinProperties;
}

FPCGElementPtr UPCGAssignOreSettings::CreateElement() const
{
	return MakeShared<FPCGAssignOreElement>();
}

// -----------------------------------------------------------------------------
// FPCGAssignOreElement
// -----------------------------------------------------------------------------

bool FPCGAssignOreElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGAssignOreElement::Execute);

	check(Context);

	const UPCGAssignOreSettings* Settings = Context->GetInputSettings<UPCGAssignOreSettings>();
	check(Settings);

	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	// Validate data table
	const UDataTable* DataTable = Settings->DataTable;
	if (!DataTable)
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoDataTable", "No DataTable assigned. All points will be discarded."));
	}

	// Read all rows from the data table
	struct FOreCandidate
	{
		FName RowName;
		const FOreDataTableRow* Row;
	};

	TArray<FOreCandidate> OreCandidates;
	if (DataTable)
	{
		// Validate that the data table uses the correct row struct
		if (DataTable->GetRowStruct() && !DataTable->GetRowStruct()->IsChildOf(FOreDataTableRow::StaticStruct()))
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(
				LOCTEXT("WrongRowStruct", "DataTable '{0}' uses row struct '{1}', expected FOreDataTableRow. Please recreate the DataTable with the correct struct."),
				FText::FromString(DataTable->GetName()),
				FText::FromString(DataTable->GetRowStruct()->GetName())));
		}
		else
		{
			const TArray<FName> RowNames = DataTable->GetRowNames();
			OreCandidates.Reserve(RowNames.Num());
			for (const FName& RowName : RowNames)
			{
				const FOreDataTableRow* Row = DataTable->FindRow<FOreDataTableRow>(RowName, TEXT("PCGAssignOre"));
				if (Row)
				{
					OreCandidates.Add({RowName, Row});
				}
			}

			if (OreCandidates.IsEmpty() && RowNames.Num() > 0)
			{
				PCGE_LOG(Warning, GraphAndLog, FText::Format(
					LOCTEXT("NoValidRows", "DataTable '{0}' has {1} rows but none could be read as FOreDataTableRow. Check that the row struct matches."),
					FText::FromString(DataTable->GetName()),
					FText::AsNumber(RowNames.Num())));
			}
		}
	}

	UE_LOG(LogPCG, Verbose, TEXT("AssignOre: %d ore candidates loaded from DataTable"), OreCandidates.Num());

	const int32 ContextSeed = Context->GetSeed();

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

		// If no candidates, all points go to discarded
		if (OreCandidates.IsEmpty())
		{
			FPCGTaggedData& DiscardedOutput = Outputs.Add_GetRef(CurrentInput);
			DiscardedOutput.Pin = PCGAssignOreConstants::DiscardedLabel;
			continue;
		}

		// Access point data via value ranges
		const TConstPCGValueRange<FTransform> TransformRange = InputPointData->GetConstTransformValueRange();
		const TConstPCGValueRange<int32> SeedRange = InputPointData->GetConstSeedValueRange();

		// Parallel arrays for results
		TArray<int32> AssignedIndices;
		TArray<int32> DiscardedIndices;
		AssignedIndices.Reserve(NumPoints);
		DiscardedIndices.Reserve(NumPoints / 4);

		// Per-assigned-point ore data (indexed in order of AssignedIndices)
		TArray<FName> AssignedOreTypes;
		TArray<FString> AssignedOrePaths;
		AssignedOreTypes.Reserve(NumPoints);
		AssignedOrePaths.Reserve(NumPoints);

		// Temp arrays reused per point
		TArray<float> CandidateWeights;
		CandidateWeights.SetNumUninitialized(OreCandidates.Num());

		const float SurfaceThreshold = Settings->SurfaceDepthThreshold;

		int32 DebugSurfaceCount = 0;
		int32 DebugBuriedCount = 0;

		for (int32 i = 0; i < NumPoints; ++i)
		{
			const FVector PointLocation = TransformRange[i].GetLocation();
			const double PointZ = PointLocation.Z;
			const bool bIsSurface = PointZ >= SurfaceThreshold;

			if (bIsSurface)
			{
				++DebugSurfaceCount;
			}
			else
			{
				++DebugBuriedCount;
			}

			// Build weighted candidate list
			float TotalWeight = 0.0f;

			for (int32 c = 0; c < OreCandidates.Num(); ++c)
			{
				const FOreDataTableRow* Row = OreCandidates[c].Row;
				float Weight = 0.0f;

				if (bIsSurface)
				{
					Weight = Row->SurfaceSpawnRate;
				}
				else
				{
					// Buried logic
					const double MinDepth = Row->SpawningRange.X; // deepest (most negative)
					const double MaxDepth = Row->SpawningRange.Y; // shallowest

					// Skip if point is outside this ore's depth range
					if (PointZ < MinDepth || PointZ > MaxDepth)
					{
						CandidateWeights[c] = 0.0f;
						continue;
					}

					// Avoid division by zero for zero-width ranges
					const double RangeSize = MaxDepth - MinDepth;
					if (FMath::IsNearlyZero(RangeSize))
					{
						CandidateWeights[c] = 0.0f;
						continue;
					}

					// NormalizedDepth: 0.0 = at MaxDepth (shallow), 1.0 = at MinDepth (deep)
					const double NormalizedDepth = (MaxDepth - PointZ) / RangeSize;

					// Distance from the peak occurrence depth
					const double DistFromPeak = FMath::Abs(NormalizedDepth - static_cast<double>(Row->MaxOccurrenceDepth));

					// Apply falloff curve
					double DepthMultiplier = FMath::Pow(
						FMath::Max(1.0 - DistFromPeak, 0.0),
						FMath::Abs(static_cast<double>(Row->DepthFalloff))
					);

					// Negative DepthFalloff inverts the curve
					if (Row->DepthFalloff < 0.0f)
					{
						DepthMultiplier = 1.0 - DepthMultiplier;
					}

					Weight = Row->BuriedSpawnRate * static_cast<float>(DepthMultiplier);
				}

				CandidateWeights[c] = FMath::Max(Weight, 0.0f);
				TotalWeight += CandidateWeights[c];
			}

			// If no valid candidates, discard this point
			if (TotalWeight <= 0.0f)
			{
				DiscardedIndices.Add(i);
				continue;
			}

			// Per-point deterministic seed
			const int32 PointSeed = PCGHelpers::ComputeSeed(ContextSeed, SeedRange[i]);
			FRandomStream PointStream(PointSeed);

			// Weighted random selection
			float Roll = PointStream.FRand() * TotalWeight;
			int32 ChosenIndex = 0;
			for (int32 c = 0; c < OreCandidates.Num(); ++c)
			{
				Roll -= CandidateWeights[c];
				if (Roll <= 0.0f)
				{
					ChosenIndex = c;
					break;
				}
			}

			AssignedIndices.Add(i);
			AssignedOreTypes.Add(OreCandidates[ChosenIndex].RowName);
			AssignedOrePaths.Add(OreCandidates[ChosenIndex].Row->VoxelGraph.ToString());
		}

		UE_LOG(LogPCG, Log, TEXT("AssignOre: %d points total — %d surface, %d buried — %d assigned, %d discarded (Threshold=%.1f)"),
			NumPoints, DebugSurfaceCount, DebugBuriedCount, AssignedIndices.Num(), DiscardedIndices.Num(), SurfaceThreshold);

		// --- Build "Out" output (assigned points with ore attributes) ---
		{
			UPCGBasePointData* OutPointData = PCGAssignOreInternal::CopyPointSubset(
				InputPointData, AssignedIndices, Context);

			UPCGMetadata* OutMetadata = OutPointData->MutableMetadata();
			check(OutMetadata);

			if (AssignedIndices.Num() > 0)
			{
				// Create OreType attribute (FName)
				FPCGMetadataAttribute<FName>* OreTypeAttr = OutMetadata->FindOrCreateAttribute<FName>(
					PCGAssignOreConstants::OreTypeAttributeName, FName(), /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);

				// Create OreVoxelGraph attribute (FString)
				FPCGMetadataAttribute<FString>* OrePathAttr = OutMetadata->FindOrCreateAttribute<FString>(
					PCGAssignOreConstants::OreVoxelGraphAttributeName, FString(), /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);

				if (OreTypeAttr && OrePathAttr)
				{
					// Write per-point values via accessors
					FPCGAttributePropertyOutputSelector OreTypeSelector;
					OreTypeSelector.SetAttributeName(PCGAssignOreConstants::OreTypeAttributeName);

					FPCGAttributePropertyOutputSelector OrePathSelector;
					OrePathSelector.SetAttributeName(PCGAssignOreConstants::OreVoxelGraphAttributeName);

					TUniquePtr<IPCGAttributeAccessor> OreTypeAccessor =
						PCGAttributeAccessorHelpers::CreateAccessor(OreTypeAttr, OutMetadata);
					TUniquePtr<IPCGAttributeAccessorKeys> OreTypeKeys =
						PCGAttributeAccessorHelpers::CreateKeys(OutPointData, OreTypeSelector);

					TUniquePtr<IPCGAttributeAccessor> OrePathAccessor =
						PCGAttributeAccessorHelpers::CreateAccessor(OrePathAttr, OutMetadata);
					TUniquePtr<IPCGAttributeAccessorKeys> OrePathKeys =
						PCGAttributeAccessorHelpers::CreateKeys(OutPointData, OrePathSelector);

					if (OreTypeAccessor && OreTypeKeys && OrePathAccessor && OrePathKeys)
					{
						OreTypeAccessor->SetRange<FName>(MakeConstArrayView(AssignedOreTypes), 0, *OreTypeKeys);
						OrePathAccessor->SetRange<FString>(MakeConstArrayView(AssignedOrePaths), 0, *OrePathKeys);
					}
				}
			}

			FPCGTaggedData& AssignedOutput = Outputs.Add_GetRef(CurrentInput);
			AssignedOutput.Data = OutPointData;
			AssignedOutput.Pin = PCGAssignOreConstants::OutputLabel;
		}

		// --- Build "Discarded" output ---
		{
			UPCGBasePointData* DiscardedPointData = PCGAssignOreInternal::CopyPointSubset(
				InputPointData, DiscardedIndices, Context);

			FPCGTaggedData& DiscardedOutput = Outputs.Add_GetRef(CurrentInput);
			DiscardedOutput.Data = DiscardedPointData;
			DiscardedOutput.Pin = PCGAssignOreConstants::DiscardedLabel;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
