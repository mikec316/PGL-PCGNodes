// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PGLStagingDistribute.h"

#include "PCGContext.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "Data/PCGBasePointData.h"
#include "Elements/PCGActorSelector.h"
#if WITH_EDITOR
#include "Helpers/PCGDynamicTrackingHelpers.h"
#endif
#include "Async/ParallelFor.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "PCGNodes/PGLFloatRangeProperty.h"

#include "Core/PCGExAssetCollection.h"
#include "Collections/PCGExMeshCollection.h"
#include "PCGExCollectionsCommon.h"
#include "PCGExPropertyTypes.h"
#include "Helpers/PCGExCollectionsHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PGLStagingDistribute)

#define LOCTEXT_NAMESPACE "PGLStagingDistribute"

// ---------------------------------------------------------------------------
// Helper: copy a subset of points from input to a new output point data
// ---------------------------------------------------------------------------
namespace PGLStagingDistributeInternal
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

	enum class EEqualityType : uint8 { Float, Int, Bool, SoftObjectPath };

	// Cached criterion data for a single entry/criterion pair
	struct FEntryCriterionData
	{
		float Min = -MAX_FLT;
		float Max = MAX_FLT;
		bool bHasMin = false;
		bool bHasMax = false;
		bool bIsEquality = false; // When true, Min (or typed target) holds the target value for equality comparison
		EEqualityType EqualityType = EEqualityType::Float;
		int64 IntTarget = 0;
		bool BoolTarget = false;
		FSoftObjectPath PathTarget;
	};

	// All precomputed data for one collection
	struct FCollectionData
	{
		UPCGExAssetCollection* Collection = nullptr;
		PCGExAssetCollection::FCache* Cache = nullptr;
		int32 NumEntries = 0;

		// Flat array: [EntryIdx * NumCriteria + CritIdx]
		TArray<FEntryCriterionData> EntryProps;
	};

	// Extract an equality target from any FPCGExProperty subclass using virtual dispatch.
	// Works for any concrete type — no class casts on the caller side.
	// Must run on the main thread (uses NewObject).
	static bool ExtractEqualityTarget(const FPCGExProperty* Base, FEntryCriterionData& OutData)
	{
		UPCGMetadata* TempMeta = NewObject<UPCGMetadata>();
		static const FName TempAttrName(TEXT("_EqTarget"));

		FPCGMetadataAttributeBase* AttrBase = Base->CreateMetadataAttribute(TempMeta, TempAttrName);
		if (!AttrBase) { return false; }

		const int64 Key = TempMeta->AddEntry();
		Base->WriteMetadataValue(AttrBase, Key);

		switch (Base->GetOutputType())
		{
		case EPCGMetadataTypes::Float:
			OutData.Min = static_cast<FPCGMetadataAttribute<float>*>(AttrBase)->GetValue(Key);
			OutData.EqualityType = EEqualityType::Float;
			break;
		case EPCGMetadataTypes::Integer32:
			OutData.IntTarget = static_cast<int64>(static_cast<FPCGMetadataAttribute<int32>*>(AttrBase)->GetValue(Key));
			OutData.EqualityType = EEqualityType::Int;
			break;
		case EPCGMetadataTypes::Integer64:
			OutData.IntTarget = static_cast<FPCGMetadataAttribute<int64>*>(AttrBase)->GetValue(Key);
			OutData.EqualityType = EEqualityType::Int;
			break;
		case EPCGMetadataTypes::Boolean:
			OutData.BoolTarget = static_cast<FPCGMetadataAttribute<bool>*>(AttrBase)->GetValue(Key);
			OutData.EqualityType = EEqualityType::Bool;
			break;
		case EPCGMetadataTypes::SoftObjectPath:
			OutData.PathTarget = static_cast<FPCGMetadataAttribute<FSoftObjectPath>*>(AttrBase)->GetValue(Key);
			OutData.EqualityType = EEqualityType::SoftObjectPath;
			break;
		default:
			return false; // Unsupported output type — treat property as missing
		}

		OutData.bHasMin = true;
		OutData.bIsEquality = true;
		return true;
		// TempMeta is not retained; eligible for GC after this call
	}

	// Build the criterion cache for a collection
	TSharedPtr<FCollectionData> BuildCollectionData(
		UPCGExAssetCollection* Collection,
		const TArray<FPGLStagingCriterion>& Criteria)
	{
		TSharedPtr<FCollectionData> Data = MakeShared<FCollectionData>();
		Data->Collection = Collection;
		Data->Cache = Collection->LoadCache();

		if (!Data->Cache || Data->Cache->IsEmpty() || !Data->Cache->Main)
		{
			Data->NumEntries = 0;
			return Data;
		}

		Data->NumEntries = Data->Cache->Main->Entries.Num();
		const int32 NumCriteria = Criteria.Num();

		// EntryProps is indexed by RAW entry index (from Indices array) so that
		// both Main and named category lookups use the same key space.
		int32 RawEntryCount = Data->NumEntries;
		for (int32 i = 0; i < Data->NumEntries; ++i)
		{
			RawEntryCount = FMath::Max(RawEntryCount, Data->Cache->Main->Indices[i] + 1);
		}
		Data->EntryProps.SetNum(RawEntryCount * NumCriteria);

		for (int32 EntryIdx = 0; EntryIdx < Data->NumEntries; ++EntryIdx)
		{
			const FPCGExAssetCollectionEntry* Entry = Data->Cache->Main->Entries[EntryIdx];
			if (!Entry) { continue; }

			const int32 RawIdx = Data->Cache->Main->Indices[EntryIdx];

			for (int32 CritIdx = 0; CritIdx < NumCriteria; ++CritIdx)
			{
				const FPGLStagingCriterion& Criterion = Criteria[CritIdx];
				FEntryCriterionData& PropData = Data->EntryProps[RawIdx * NumCriteria + CritIdx];

				if (Criterion.PropertyMode == EPGLCriterionPropertyMode::FloatRange)
				{
					// Single Float Range property
					if (!Criterion.RangePropertyName.IsNone())
					{
						if (const FPGLProperty_FloatRange* Prop = Entry->GetResolvedProperty<FPGLProperty_FloatRange>(
							Collection, Criterion.RangePropertyName))
						{
							PropData.Min = Prop->Min;
							PropData.Max = Prop->Max;
							PropData.bHasMin = true;
							PropData.bHasMax = true;
						}
					}
				}
				else if (Criterion.PropertyMode == EPGLCriterionPropertyMode::FloatEquals)
				{
					// Untyped lookup — entry override first, then collection default.
					// Value extraction uses virtual dispatch so any property subclass works.
					if (!Criterion.EqualsPropertyName.IsNone())
					{
						const FInstancedStruct* PropStruct = Entry->PropertyOverrides.GetOverride(Criterion.EqualsPropertyName);
						if (!PropStruct)
						{
							PropStruct = Collection->CollectionProperties.GetPropertyByName(Criterion.EqualsPropertyName);
						}

						if (const FPCGExProperty* Base = PropStruct ? PropStruct->GetPtr<FPCGExProperty>() : nullptr)
						{
							ExtractEqualityTarget(Base, PropData);
						}
					}
				}
				else
				{
					// Separate Min / Max float properties
					if (!Criterion.MinPropertyName.IsNone())
					{
						if (const FPCGExProperty_Float* Prop = Entry->GetResolvedProperty<FPCGExProperty_Float>(
							Collection, Criterion.MinPropertyName))
						{
							PropData.Min = Prop->Value;
							PropData.bHasMin = true;
						}
					}

					if (!Criterion.MaxPropertyName.IsNone())
					{
						if (const FPCGExProperty_Float* Prop = Entry->GetResolvedProperty<FPCGExProperty_Float>(
							Collection, Criterion.MaxPropertyName))
						{
							PropData.Max = Prop->Value;
							PropData.bHasMax = true;
						}
					}
				}
			}
		}

		return Data;
	}

	// ---------------------------------------------------------------------------
	// Meta-collection resolution: expand meta collections into leaf collections
	// ---------------------------------------------------------------------------

	struct FResolvedLeaf
	{
		UPCGExAssetCollection* Collection;
		float WeightMultiplier;
	};

	void ResolveToLeafCollections(
		UPCGExAssetCollection* InCollection,
		TArray<FResolvedLeaf>& OutLeaves,
		float InWeightMultiplier,
		TSet<UPCGExAssetCollection*>& Visited)
	{
		static const FName MetaCollTypeId(TEXT("MetaCollection"));

		if (Visited.Contains(InCollection)) { return; } // Cycle detection
		Visited.Add(InCollection);

		if (InCollection->GetTypeId() != MetaCollTypeId)
		{
			// Non-meta collection: add directly as a leaf (for its direct asset entries)
			OutLeaves.Add({InCollection, InWeightMultiplier});

			// Also recurse into any sub-collection entries within this collection so that
			// nested sub-collections (e.g. a sub-mesh-collection inside a mesh collection)
			// are resolved and treated as additional leaves. Without this, sub-collection
			// entries are skipped in ExecuteInternal (bIsSubCollection check) and never used.
			PCGExAssetCollection::FCache* Cache = InCollection->LoadCache();
			if (Cache && !Cache->IsEmpty() && Cache->Main)
			{
				for (int32 i = 0; i < Cache->Main->Entries.Num(); ++i)
				{
					const FPCGExAssetCollectionEntry* Entry = Cache->Main->Entries[i];
					if (!Entry || !Entry->bIsSubCollection) { continue; }

					UPCGExAssetCollection* SubColl = const_cast<UPCGExAssetCollection*>(Entry->GetSubCollectionPtr());
					if (!SubColl) { continue; }

					const float ChildMult = InWeightMultiplier * static_cast<float>(FMath::Max(Entry->Weight, 1));
					ResolveToLeafCollections(SubColl, OutLeaves, ChildMult, Visited);
				}
			}
			return;
		}

		// Meta collection: expand entries into leaf collections
		PCGExAssetCollection::FCache* MetaCache = InCollection->LoadCache();
		if (!MetaCache || MetaCache->IsEmpty() || !MetaCache->Main) { return; }

		for (int32 i = 0; i < MetaCache->Main->Entries.Num(); ++i)
		{
			const FPCGExAssetCollectionEntry* Entry = MetaCache->Main->Entries[i];
			if (!Entry) { continue; }

			UPCGExAssetCollection* SubColl = nullptr;

			if (Entry->bIsSubCollection)
			{
				// Subcollection entry: get the nested (meta) collection
				SubColl = const_cast<UPCGExAssetCollection*>(Entry->GetSubCollectionPtr());
			}
			else if (Entry->Staging.Path.IsValid())
			{
				// Regular entry: load the referenced collection from its staged path
				SubColl = Cast<UPCGExAssetCollection>(Entry->Staging.Path.TryLoad());
			}

			if (!SubColl) { continue; }

			const float ChildMult = InWeightMultiplier * static_cast<float>(FMath::Max(Entry->Weight, 1));
			ResolveToLeafCollections(SubColl, OutLeaves, ChildMult, Visited);
		}
	}

	struct FWeightedCollectionData
	{
		TSharedPtr<FCollectionData> Data;
		float WeightMultiplier = 1.0f;
	};

	TArray<FWeightedCollectionData> BuildMultiCollectionData(
		UPCGExAssetCollection* InCollection,
		const TArray<FPGLStagingCriterion>& Criteria)
	{
		TArray<FWeightedCollectionData> Result;

		TArray<FResolvedLeaf> Leaves;
		TSet<UPCGExAssetCollection*> Visited;
		ResolveToLeafCollections(InCollection, Leaves, 1.0f, Visited);

		for (const FResolvedLeaf& Leaf : Leaves)
		{
			TSharedPtr<FCollectionData> SubData = BuildCollectionData(Leaf.Collection, Criteria);
			if (SubData && SubData->NumEntries > 0)
			{
				Result.Add({SubData, Leaf.WeightMultiplier});
			}
		}

		return Result;
	}

	// Candidate entry tracked during per-point evaluation across multiple collections
	struct FCandidate
	{
		const FPCGExAssetCollectionEntry* Entry = nullptr;
		UPCGExAssetCollection* Collection = nullptr;
		const FCollectionData* ColData = nullptr;
		int32 MainIdx = -1; // Raw entry index into ColData->EntryProps
		float Weight = 0.0f;
	};

	// Extract the point value for a criterion
	float GetPointValue(
		const FPGLStagingCriterion& Criterion,
		const FVector& PointLocation,
		const FVector& PointNormal,
		float PointDensity,
		int32 PointIdx,
		const IPCGAttributeAccessor* CustomAccessor,
		const IPCGAttributeAccessorKeys* CustomKeys)
	{
		switch (Criterion.PointSource)
		{
		case EPGLPointValueSource::PositionZ:
			return static_cast<float>(PointLocation.Z);

		case EPGLPointValueSource::SlopeAngle:
		{
			const float DotUp = static_cast<float>(FVector::DotProduct(PointNormal, FVector::UpVector));
			return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotUp, -1.0f, 1.0f)));
		}

		case EPGLPointValueSource::NormalDotUp:
			return static_cast<float>(FVector::DotProduct(PointNormal, FVector::UpVector));

		case EPGLPointValueSource::Density:
			return PointDensity;

		case EPGLPointValueSource::Attribute:
		{
			if (CustomAccessor && CustomKeys)
			{
				float Value = 0.0f;
				CustomAccessor->Get<float>(Value, PointIdx, *CustomKeys);
				return Value;
			}
			return 0.0f;
		}

		case EPGLPointValueSource::MinDistanceToSameEntry:
			return 0.0f; // Evaluated as a post-selection spatial filter; not used in the per-point pass.

		default:
			return 0.0f;
		}
	}
}

// ---------------------------------------------------------------------------
// UPGLStagingDistributeSettings
// ---------------------------------------------------------------------------

#if WITH_EDITOR
FName UPGLStagingDistributeSettings::GetDefaultNodeName() const
{
	return FName(TEXT("PGLStagingDistribute"));
}

FText UPGLStagingDistributeSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "PGL Staging Distribute");
}

FText UPGLStagingDistributeSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Distributes asset collection entries to points by testing named property\n"
		"overrides (float ranges) against point attributes like height and slope.\n"
		"Only entries whose property ranges contain the point's value are eligible.\n"
		"Among eligible entries, selection uses weighted random.\n\n"
		"Supports per-point collections via a path attribute.\n"
		"Applies per-entry random variations (offset, rotation, scale) to output transforms.\n"
		"Outputs a Collection Map compatible with the PCGEx pipeline.");
}

void UPGLStagingDistributeSettings::GetStaticTrackedKeys(
	FPCGSelectionKeyToSettingsMap& OutKeysToSettings,
	TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const
{
	Super::GetStaticTrackedKeys(OutKeysToSettings, OutVisitedGraphs);

	if (CollectionSource == EPGLCollectionSource::Asset && !Collection.IsNull())
	{
		const FPCGSelectionKey Key = FPCGSelectionKey::CreateFromPath(Collection.ToSoftObjectPath());
		OutKeysToSettings.FindOrAdd(Key).Emplace(this, /*bIsCulled=*/false);
	}
}
#endif

TArray<FPCGPinProperties> UPGLStagingDistributeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point).SetRequiredPin();
	return PinProperties;
}

TArray<FPCGPinProperties> UPGLStagingDistributeSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PGLStagingDistributeConstants::OutputLabel, EPCGDataType::Point);
	PinProperties.Emplace(PGLStagingDistributeConstants::MapLabel, EPCGDataType::Param);
	return PinProperties;
}

FPCGElementPtr UPGLStagingDistributeSettings::CreateElement() const
{
	return MakeShared<FPGLStagingDistributeElement>();
}

// ---------------------------------------------------------------------------
// Prepared data: loaded on main thread in PrepareDataInternal, consumed off-
// thread in ExecuteInternal so that asset loads never stall the game thread.
// ---------------------------------------------------------------------------

struct FPGLStagingDistributePreparedData
{
	// Asset mode: single collection expanded into weighted leaf data
	TArray<PGLStagingDistributeInternal::FWeightedCollectionData> SingleMultiData;

	// PointAttribute mode: keyed by path string, pre-loaded in PrepareData
	TMap<FString, TArray<PGLStagingDistributeInternal::FWeightedCollectionData>> CollectionCache;
};

// ---------------------------------------------------------------------------
// FPGLStagingDistributeElement
// ---------------------------------------------------------------------------

bool FPGLStagingDistributeElement::CanExecuteOnlyOnMainThread(FPCGContext* Context) const
{
	// PrepareData loads assets and needs the main thread; Execute is pure
	// computation and can safely run on any scheduler thread.
	return !Context ||
		Context->CurrentPhase == EPCGExecutionPhase::PrepareData;
}

FPCGContext* FPGLStagingDistributeElement::CreateContext()
{
	return new FPGLStagingDistributeContext();
}

// ---------------------------------------------------------------------------
// PrepareDataInternal — runs on main thread, loads all collections up front
// ---------------------------------------------------------------------------

bool FPGLStagingDistributeElement::PrepareDataInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPGLStagingDistributeElement::PrepareData);

	FPGLStagingDistributeContext* Context = static_cast<FPGLStagingDistributeContext*>(InContext);
	const UPGLStagingDistributeSettings* Settings = Context->GetInputSettings<UPGLStagingDistributeSettings>();
	check(Settings);

	using namespace PGLStagingDistributeInternal;

	TSharedPtr<FPGLStagingDistributePreparedData> Prepared = MakeShared<FPGLStagingDistributePreparedData>();

	// --- Asset mode: load the single collection ---
	if (Settings->CollectionSource == EPGLCollectionSource::Asset)
	{
		UPCGExAssetCollection* Collection = Settings->Collection.LoadSynchronous();
		if (!Collection)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoCollection", "No asset collection assigned."));
			Context->PreparedData = Prepared;
			return true;
		}
#if WITH_EDITOR
		FPCGDynamicTrackingHelper::AddSingleDynamicTrackingKey(
			Context, FPCGSelectionKey::CreateFromPath(Settings->Collection.ToSoftObjectPath()), /*bIsCulled=*/false);
#endif
		Prepared->SingleMultiData = BuildMultiCollectionData(Collection, Settings->Criteria);

#if WITH_EDITOR
		for (const FWeightedCollectionData& WCD : Prepared->SingleMultiData)
		{
			if (WCD.Data && WCD.Data->Collection && WCD.Data->Collection != Collection)
			{
				FPCGDynamicTrackingHelper::AddSingleDynamicTrackingKey(
					Context, FPCGSelectionKey::CreateFromPath(FSoftObjectPath(WCD.Data->Collection)), /*bIsCulled=*/false);
			}
		}
#endif
		if (Prepared->SingleMultiData.IsEmpty())
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("EmptyCollection", "Asset collection is empty or has no valid entries."));
		}
	}
	else
	{
		// --- PointAttribute mode: pre-scan all inputs for unique collection paths and load them ---
		const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

		for (const FPCGTaggedData& InputTagged : Inputs)
		{
			const UPCGBasePointData* InputPointData = Cast<UPCGBasePointData>(InputTagged.Data);
			if (!InputPointData || InputPointData->GetNumPoints() == 0) { continue; }

			FPCGAttributePropertyInputSelector Selector;
			Selector.SetAttributeName(Settings->CollectionAttributeName);

			TUniquePtr<const IPCGAttributeAccessor> PathAccessor =
				PCGAttributeAccessorHelpers::CreateConstAccessor(InputPointData, Selector);
			TUniquePtr<const IPCGAttributeAccessorKeys> PathKeys =
				PCGAttributeAccessorHelpers::CreateConstKeys(InputPointData, Selector);

			if (!PathAccessor || !PathKeys) { continue; }

			const int32 NumPoints = InputPointData->GetNumPoints();
			for (int32 PointIdx = 0; PointIdx < NumPoints; ++PointIdx)
			{
				FString PathString;
				if (!PathAccessor->Get<FString>(PathString, PointIdx, *PathKeys) || PathString.IsEmpty()) { continue; }
				if (Prepared->CollectionCache.Contains(PathString)) { continue; }

				FSoftObjectPath SoftPath(PathString);
				UPCGExAssetCollection* LoadedColl = Cast<UPCGExAssetCollection>(SoftPath.TryLoad());
				if (!LoadedColl)
				{
					Prepared->CollectionCache.Add(PathString, TArray<FWeightedCollectionData>());
					continue;
				}
#if WITH_EDITOR
				FPCGDynamicTrackingHelper::AddSingleDynamicTrackingKey(
					Context, FPCGSelectionKey::CreateFromPath(SoftPath), /*bIsCulled=*/false);
#endif
				TArray<FWeightedCollectionData>& NewData = Prepared->CollectionCache.Add(
					PathString, BuildMultiCollectionData(LoadedColl, Settings->Criteria));
#if WITH_EDITOR
				for (const FWeightedCollectionData& WCD : NewData)
				{
					if (WCD.Data && WCD.Data->Collection && WCD.Data->Collection != LoadedColl)
					{
						FPCGDynamicTrackingHelper::AddSingleDynamicTrackingKey(
							Context, FPCGSelectionKey::CreateFromPath(FSoftObjectPath(WCD.Data->Collection)), /*bIsCulled=*/false);
					}
				}
#endif
			}
		}
	}

	Context->PreparedData = Prepared;
	return true;
}

// ---------------------------------------------------------------------------
// ExecuteInternal — runs off main thread, uses ParallelFor for point evaluation
// ---------------------------------------------------------------------------

bool FPGLStagingDistributeElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPGLStagingDistributeElement::Execute);

	FPGLStagingDistributeContext* Context = static_cast<FPGLStagingDistributeContext*>(InContext);
	check(Context);

	const UPGLStagingDistributeSettings* Settings = Context->GetInputSettings<UPGLStagingDistributeSettings>();
	check(Settings);

	const TSharedPtr<FPGLStagingDistributePreparedData>& Prepared = Context->PreparedData;
	if (!Prepared) { return true; }

	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	const int32 NumCriteria = Settings->Criteria.Num();

	// PickPacker tracks all collections seen and serialises the map (thread-safe)
	TSharedPtr<PCGExCollections::FPickPacker> PickPacker = MakeShared<PCGExCollections::FPickPacker>();

	using namespace PGLStagingDistributeInternal;

	// Register every leaf collection with the packer (PackToDataset only emits registered hosts).
	for (const FWeightedCollectionData& WCD : Prepared->SingleMultiData)
	{
		if (WCD.Data && WCD.Data->Collection)
		{
			PickPacker->RegisterCollection(WCD.Data->Collection);
		}
	}
	for (const TPair<FString, TArray<FWeightedCollectionData>>& CachePair : Prepared->CollectionCache)
	{
		for (const FWeightedCollectionData& WCD : CachePair.Value)
		{
			if (WCD.Data && WCD.Data->Collection)
			{
				PickPacker->RegisterCollection(WCD.Data->Collection);
			}
		}
	}

	const int32 ContextSeed = Context->GetSeed();

	// Initialize variation details once (they're per-settings, not per-input)
	FPCGExFittingVariationsDetails VariationDetails = Settings->Variations;
	VariationDetails.Init(ContextSeed);
	const bool bApplyVariations = VariationDetails.bEnabledBefore || VariationDetails.bEnabledAfter;

	// Per-point result from parallel evaluation
	struct FPointResult
	{
		const FPCGExAssetCollectionEntry* Entry = nullptr;
		UPCGExAssetCollection* Collection = nullptr;
		const FCollectionData* ColData = nullptr;
		int32 MainIdx = -1; // Raw entry index into ColData->EntryProps (for post-process distance filter)
		int16 RawIndex = -1;
		int16 SecondaryIndex = -1;
		bool bMatched = false;
	};

	for (int32 InputIndex = 0; InputIndex < Inputs.Num(); ++InputIndex)
	{
		const FPCGTaggedData& CurrentInput = Inputs[InputIndex];

		const UPCGBasePointData* InputPointData = Cast<UPCGBasePointData>(CurrentInput.Data);
		if (!InputPointData)
		{
			PCGE_LOG(Verbose, GraphAndLog, FText::Format(
				LOCTEXT("NotPointData", "Input {0} is not point data, skipping."), InputIndex));
			continue;
		}

		const int32 NumPoints = InputPointData->GetNumPoints();
		if (NumPoints == 0) { continue; }

		// --- Point data accessors ---

		const TConstPCGValueRange<FTransform> TransformRange = InputPointData->GetConstTransformValueRange();
		const TConstPCGValueRange<int32> SeedRange = InputPointData->GetConstSeedValueRange();
		const TConstPCGValueRange<float> DensityRange = InputPointData->GetConstDensityValueRange();

		// --- Per-point collection resolution (Attribute mode) ---

		TUniquePtr<const IPCGAttributeAccessor> CollPathAccessor;
		TUniquePtr<const IPCGAttributeAccessorKeys> CollPathKeys;

		if (Settings->CollectionSource == EPGLCollectionSource::PointAttribute)
		{
			FPCGAttributePropertyInputSelector Selector;
			Selector.SetAttributeName(Settings->CollectionAttributeName);

			CollPathAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InputPointData, Selector);
			CollPathKeys = PCGAttributeAccessorHelpers::CreateConstKeys(InputPointData, Selector);

			if (!CollPathAccessor || !CollPathKeys)
			{
				PCGE_LOG(Warning, GraphAndLog, FText::Format(
					LOCTEXT("MissingCollAttr", "Could not find collection path attribute '{0}' on input points. Skipping this data set."),
					FText::FromName(Settings->CollectionAttributeName)));
				continue;
			}
		}

		// --- Category attribute accessor ---

		TUniquePtr<const IPCGAttributeAccessor> CategoryAccessor;
		TUniquePtr<const IPCGAttributeAccessorKeys> CategoryKeys;

		if (Settings->bUseCategory)
		{
			FPCGAttributePropertyInputSelector CatSelector;
			CatSelector.SetAttributeName(Settings->CategoryAttributeName);

			CategoryAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InputPointData, CatSelector);
			CategoryKeys = PCGAttributeAccessorHelpers::CreateConstKeys(InputPointData, CatSelector);

			if (!CategoryAccessor || !CategoryKeys)
			{
				PCGE_LOG(Warning, GraphAndLog, FText::Format(
					LOCTEXT("MissingCatAttr", "Could not find category attribute '{0}' on input points. All entries will be eligible."),
					FText::FromName(Settings->CategoryAttributeName)));
			}
		}

		// --- Custom criterion attribute accessors + per-criterion bulk prefetch.
		// Reading point attributes inside the parallel loop costs a virtual
		// GetKeys per call; for N points * C candidates * K Attribute criteria
		// that quickly dominates. Doing one batched GetRange per criterion up
		// front turns the inner loop into pure array indexing.

		enum class EPrefetchedType : uint8 { None, Float, Int, Bool, Path };

		struct FPrefetchedAttr
		{
			EPrefetchedType Type = EPrefetchedType::None;
			TArray<float> Floats;
			TArray<int64> Ints;
			TArray<bool> Bools;
			TArray<FSoftObjectPath> Paths;
		};

		TArray<FPrefetchedAttr> PrefetchedAttrs;
		PrefetchedAttrs.SetNum(NumCriteria);

		constexpr EPCGAttributeAccessorFlags PrefetchFlags =
			EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible;

		for (int32 CritIdx = 0; CritIdx < NumCriteria; ++CritIdx)
		{
			const FPGLStagingCriterion& Criterion = Settings->Criteria[CritIdx];
			if (Criterion.PointSource != EPGLPointValueSource::Attribute) { continue; }
			if (Criterion.CustomAttributeName.IsNone()) { continue; }

			FPCGAttributePropertyInputSelector Selector;
			Selector.SetAttributeName(Criterion.CustomAttributeName);

			TUniquePtr<const IPCGAttributeAccessor> Accessor =
				PCGAttributeAccessorHelpers::CreateConstAccessor(InputPointData, Selector);
			TUniquePtr<const IPCGAttributeAccessorKeys> Keys =
				PCGAttributeAccessorHelpers::CreateConstKeys(InputPointData, Selector);

			if (!Accessor || !Keys)
			{
				PCGE_LOG(Warning, GraphAndLog, FText::Format(
					LOCTEXT("MissingCritAttr", "Could not find attribute '{0}' on input points."),
					FText::FromName(Criterion.CustomAttributeName)));
				continue;
			}

			FPrefetchedAttr& Pf = PrefetchedAttrs[CritIdx];
			const EPCGMetadataTypes UnderlyingType = static_cast<EPCGMetadataTypes>(Accessor->GetUnderlyingType());

			// FloatEquals on a non-float underlying type uses typed equality
			// (Int/Bool/Path); everything else compares as float.
			const bool bIsFloatEquals = Criterion.PropertyMode == EPGLCriterionPropertyMode::FloatEquals;

			if (bIsFloatEquals && UnderlyingType == EPCGMetadataTypes::Boolean)
			{
				Pf.Bools.SetNumZeroed(NumPoints);
				if (Accessor->GetRange<bool>(MakeArrayView(Pf.Bools), 0, *Keys, PrefetchFlags))
				{
					Pf.Type = EPrefetchedType::Bool;
				}
			}
			else if (bIsFloatEquals && UnderlyingType == EPCGMetadataTypes::SoftObjectPath)
			{
				Pf.Paths.SetNum(NumPoints);
				if (Accessor->GetRange<FSoftObjectPath>(MakeArrayView(Pf.Paths), 0, *Keys, PrefetchFlags))
				{
					Pf.Type = EPrefetchedType::Path;
				}
			}
			else if (bIsFloatEquals &&
				(UnderlyingType == EPCGMetadataTypes::Integer32 || UnderlyingType == EPCGMetadataTypes::Integer64))
			{
				Pf.Ints.SetNumZeroed(NumPoints);
				if (Accessor->GetRange<int64>(MakeArrayView(Pf.Ints), 0, *Keys, PrefetchFlags))
				{
					Pf.Type = EPrefetchedType::Int;
				}
			}
			else
			{
				Pf.Floats.SetNumZeroed(NumPoints);
				if (Accessor->GetRange<float>(MakeArrayView(Pf.Floats), 0, *Keys, PrefetchFlags))
				{
					Pf.Type = EPrefetchedType::Float;
				}
			}
		}

		// Pre-fetch the per-point collection-path strings (PointAttribute mode).
		TArray<FString> PrefetchedCollPaths;
		if (CollPathAccessor && CollPathKeys)
		{
			PrefetchedCollPaths.SetNum(NumPoints);
			CollPathAccessor->GetRange<FString>(MakeArrayView(PrefetchedCollPaths), 0, *CollPathKeys, PrefetchFlags);
		}

		// Pre-fetch the per-point category strings.
		TArray<FString> PrefetchedCategories;
		const bool bHaveCategory = CategoryAccessor && CategoryKeys;
		if (bHaveCategory)
		{
			PrefetchedCategories.SetNum(NumPoints);
			CategoryAccessor->GetRange<FString>(MakeArrayView(PrefetchedCategories), 0, *CategoryKeys, PrefetchFlags);
		}

		// --- Parallel per-point evaluation ---
		// Each point writes to its own slot — no synchronization needed.

		TArray<FPointResult> PointResults;
		PointResults.SetNum(NumPoints);

		// Shared read-only pointers for the lambda captures
		const TArray<FWeightedCollectionData>* SingleMultiDataPtr = &Prepared->SingleMultiData;
		const TMap<FString, TArray<FWeightedCollectionData>>* CollectionCachePtr = &Prepared->CollectionCache;
		const TArray<FPrefetchedAttr>* PrefetchedAttrsPtr = &PrefetchedAttrs;
		const TArray<FString>* PrefetchedCollPathsPtr = &PrefetchedCollPaths;
		const TArray<FString>* PrefetchedCategoriesPtr = &PrefetchedCategories;

		ParallelFor(NumPoints, [&](int32 PointIdx)
		{
			// --- Resolve collection(s) for this point ---

			const TArray<FWeightedCollectionData>* PointMultiData = nullptr;

			if (Settings->CollectionSource == EPGLCollectionSource::Asset)
			{
				PointMultiData = SingleMultiDataPtr;
			}
			else
			{
				if (!PrefetchedCollPathsPtr->IsValidIndex(PointIdx)) { return; }
				const FString& PathString = (*PrefetchedCollPathsPtr)[PointIdx];
				if (PathString.IsEmpty()) { return; }

				// Look up from pre-loaded cache (read-only during Execute)
				PointMultiData = CollectionCachePtr->Find(PathString);
			}

			if (!PointMultiData || PointMultiData->IsEmpty()) { return; }

			const FTransform& PointTransform = TransformRange[PointIdx];
			const FVector PointLocation = PointTransform.GetLocation();
			const FVector PointNormal = PointTransform.GetUnitAxis(EAxis::Z);
			const float PointDensity = DensityRange[PointIdx];

			// --- Hoist per-criterion point value computation out of the entries loop.
			// The point's value for a given criterion does not depend on the entry,
			// so compute it once per point — not once per (point * candidate-entry).
			TArray<float, TInlineAllocator<8>> PerCriterionFloatValues;
			PerCriterionFloatValues.SetNumUninitialized(NumCriteria);
			for (int32 CritIdx = 0; CritIdx < NumCriteria; ++CritIdx)
			{
				const FPGLStagingCriterion& Criterion = Settings->Criteria[CritIdx];
				if (Criterion.PointSource == EPGLPointValueSource::Attribute)
				{
					const FPrefetchedAttr& Pf = (*PrefetchedAttrsPtr)[CritIdx];
					PerCriterionFloatValues[CritIdx] = (Pf.Type == EPrefetchedType::Float)
						? Pf.Floats[PointIdx]
						: 0.0f;
				}
				else
				{
					PerCriterionFloatValues[CritIdx] = GetPointValue(
						Criterion, PointLocation, PointNormal, PointDensity,
						PointIdx, /*CustomAccessor=*/nullptr, /*CustomKeys=*/nullptr);
				}
			}

			// Resolve category once per point as well (independent of sub-collection).
			FName PointCategoryKey;
			bool bHasPointCategory = false;
			if (Settings->bUseCategory && bHaveCategory && PrefetchedCategoriesPtr->IsValidIndex(PointIdx))
			{
				const FString& CategoryStr = (*PrefetchedCategoriesPtr)[PointIdx];
				if (!CategoryStr.IsEmpty())
				{
					PointCategoryKey = FName(*CategoryStr);
					bHasPointCategory = true;
				}
			}

			// --- Gather candidates across all resolved sub-collections ---

			TArray<FCandidate> Candidates;
			Candidates.Reserve(64);
			float TotalWeight = 0.0f;

			for (const FWeightedCollectionData& SubColl : *PointMultiData)
			{
				if (!SubColl.Data || SubColl.Data->NumEntries == 0) { continue; }

				// Resolve active category for this sub-collection
				const PCGExAssetCollection::FCategory* ActiveCategory = SubColl.Data->Cache->Main.Get();

				if (bHasPointCategory)
				{
					const int32* FoundIdx = SubColl.Data->Cache->CategoryNameToIndex.Find(PointCategoryKey);
					if (!FoundIdx)
					{
						continue;
					}
					const TSharedPtr<PCGExAssetCollection::FCategory>& FoundCat = SubColl.Data->Cache->Categories[*FoundIdx];
					if (!FoundCat || FoundCat->Entries.Num() == 0)
					{
						continue;
					}
					ActiveCategory = FoundCat.Get();
				}

				const int32 NumEntries = ActiveCategory->Entries.Num();

				for (int32 ActiveIdx = 0; ActiveIdx < NumEntries; ++ActiveIdx)
				{
					const FPCGExAssetCollectionEntry* Entry = ActiveCategory->Entries[ActiveIdx];
					const int32 MainIdx = ActiveCategory->Indices[ActiveIdx];
					if (!Entry || Entry->bIsSubCollection)
					{
						continue;
					}

					bool bPassesAll = true;

					for (int32 CritIdx = 0; CritIdx < NumCriteria; ++CritIdx)
					{
						const FPGLStagingCriterion& Criterion = Settings->Criteria[CritIdx];

						// MinDistanceToSameEntry is a post-selection spatial filter evaluated after all
						// entry assignments are done. Skip it here so all entries remain eligible.
						if (Criterion.PointSource == EPGLPointValueSource::MinDistanceToSameEntry)
						{
							continue;
						}

						const int32 EntryPropsIdx = MainIdx * NumCriteria + CritIdx;
						if (!SubColl.Data->EntryProps.IsValidIndex(EntryPropsIdx))
						{
							if (Criterion.bExcludeIfMissing) { bPassesAll = false; break; }
							continue;
						}
						const FEntryCriterionData& PropData = SubColl.Data->EntryProps[EntryPropsIdx];

						if (!PropData.bHasMin && !PropData.bHasMax)
						{
							if (Criterion.bExcludeIfMissing)
							{
								bPassesAll = false;
								break;
							}
							continue;
						}

						// Non-float equality on Attribute source: read from prefetched
						// typed array. Type mismatch (entry expects bool but point
						// attr is int) yields no match — same as the previous accessor
						// path failing silently.
						if (PropData.bIsEquality && Criterion.PointSource == EPGLPointValueSource::Attribute
							&& PropData.EqualityType != EEqualityType::Float)
						{
							const FPrefetchedAttr& Pf = (*PrefetchedAttrsPtr)[CritIdx];
							bool bMatches = false;

							switch (PropData.EqualityType)
							{
							case EEqualityType::Int:
								if (Pf.Type == EPrefetchedType::Int)
								{
									bMatches = Pf.Ints[PointIdx] == PropData.IntTarget;
								}
								break;
							case EEqualityType::Bool:
								if (Pf.Type == EPrefetchedType::Bool)
								{
									bMatches = Pf.Bools[PointIdx] == PropData.BoolTarget;
								}
								break;
							case EEqualityType::SoftObjectPath:
								if (Pf.Type == EPrefetchedType::Path)
								{
									bMatches = Pf.Paths[PointIdx] == PropData.PathTarget;
								}
								break;
							default:
								break;
							}

							if (!bMatches)
							{
								bPassesAll = false;
								break;
							}
							continue;
						}

						const float PointValue = PerCriterionFloatValues[CritIdx];

						if (PropData.bIsEquality)
						{
							if (FMath::Abs(PointValue - PropData.Min) > Criterion.EqualsTolerance)
							{
								bPassesAll = false;
								break;
							}
						}
						else if (PointValue < PropData.Min || PointValue > PropData.Max)
						{
							bPassesAll = false;
							break;
						}
					}

					if (bPassesAll)
					{
						const float Weight = static_cast<float>(FMath::Max(Entry->Weight, 0)) * SubColl.WeightMultiplier;
						Candidates.Add({Entry, SubColl.Data->Collection, SubColl.Data.Get(), MainIdx, Weight});
						TotalWeight += Weight;
					}
				}
			}

			if (TotalWeight <= 0.0f) { return; }

			// Weighted random pick from all candidates
			const int32 PointSeed = PCGHelpers::ComputeSeed(ContextSeed, SeedRange[PointIdx]);
			FRandomStream PointStream(PointSeed);

			float Roll = PointStream.FRand() * TotalWeight;
			const FCandidate* Selected = &Candidates[0];
			for (const FCandidate& C : Candidates)
			{
				Roll -= C.Weight;
				if (Roll <= 0.0f)
				{
					Selected = &C;
					break;
				}
			}

			if (!Selected->Entry) { return; }

			if (Settings->bPruneEmptyPoints && !Selected->Entry->Staging.Path.IsValid())
			{
				return;
			}

			// Pick material variant
			int16 SecondaryIndex = -1;
			if (const PCGExAssetCollection::FMicroCache* MicroCache = Selected->Entry->MicroCache.Get();
				MicroCache && !MicroCache->IsEmpty() && MicroCache->GetTypeId() == PCGExAssetCollection::TypeIds::Mesh)
			{
				const int32 MaterialSeed = PCGHelpers::ComputeSeed(PointSeed, Selected->Entry->Staging.InternalIndex);
				SecondaryIndex = static_cast<int16>(MicroCache->GetPickRandomWeighted(MaterialSeed));
			}

			FPointResult& Result = PointResults[PointIdx];
			Result.Entry = Selected->Entry;
			Result.Collection = Selected->Collection;
			Result.ColData = Selected->ColData;
			Result.MainIdx = Selected->MainIdx;
			Result.RawIndex = static_cast<int16>(Selected->Entry->Staging.InternalIndex);
			Result.SecondaryIndex = SecondaryIndex;
			Result.bMatched = true;
		});

		// --- Compact parallel results and resolve PickPacker hashes (sequential) ---

		TArray<int32> MatchedPointIndices;
		TArray<int64> MatchedEntryHashes;
		TArray<const FPCGExAssetCollectionEntry*> MatchedEntries;
		TArray<UPCGExAssetCollection*> MatchedCollections;
		TArray<const FCollectionData*> MatchedColDatas;
		TArray<int32> MatchedRawEntryIndices;
		MatchedPointIndices.Reserve(NumPoints);
		MatchedEntryHashes.Reserve(NumPoints);
		MatchedEntries.Reserve(NumPoints);
		MatchedCollections.Reserve(NumPoints);
		MatchedColDatas.Reserve(NumPoints);
		MatchedRawEntryIndices.Reserve(NumPoints);

		for (int32 PointIdx = 0; PointIdx < NumPoints; ++PointIdx)
		{
			const FPointResult& Result = PointResults[PointIdx];
			if (!Result.bMatched) { continue; }

			const uint64 Hash = PickPacker->GetPickIdx(Result.Collection, Result.RawIndex, Result.SecondaryIndex);

			MatchedPointIndices.Add(PointIdx);
			MatchedEntryHashes.Add(static_cast<int64>(Hash));
			MatchedEntries.Add(Result.Entry);
			MatchedCollections.Add(Result.Collection);
			MatchedColDatas.Add(Result.ColData);
			MatchedRawEntryIndices.Add(Result.MainIdx);
		}

		UE_LOG(LogPCG, Log, TEXT("PGLStagingDistribute: %d points - %d matched (%d criteria)"),
			NumPoints, MatchedPointIndices.Num(), NumCriteria);

		// --- Post-process: MinDistanceToSameEntry spatial filter ---
		// Applied after entry assignment. For each such criterion, greedily keep points in
		// original order: a point is kept only if its distance to all already-kept same-entry
		// points satisfies the criterion's range. The first point for each entry always passes.
		{
			bool bNeedsSpatialFilter = false;
			for (const FPGLStagingCriterion& C : Settings->Criteria)
			{
				if (C.PointSource == EPGLPointValueSource::MinDistanceToSameEntry)
				{
					bNeedsSpatialFilter = true;
					break;
				}
			}

			if (bNeedsSpatialFilter && MatchedPointIndices.Num() > 0)
			{
				TArray<bool> bKeep;
				bKeep.Init(true, MatchedPointIndices.Num());

				for (int32 CritIdx = 0; CritIdx < NumCriteria; ++CritIdx)
				{
					const FPGLStagingCriterion& Criterion = Settings->Criteria[CritIdx];
					if (Criterion.PointSource != EPGLPointValueSource::MinDistanceToSameEntry) { continue; }
					if (Criterion.PropertyMode == EPGLCriterionPropertyMode::FloatEquals) { continue; }

					// Greedy pass: per-entry-hash, track locations of already-kept points.
					// Map: entry hash → world locations of already-accepted same-entry points.
					TMap<int64, TArray<FVector>> KeptByHash;
					KeptByHash.Reserve(MatchedPointIndices.Num());

					for (int32 i = 0; i < MatchedPointIndices.Num(); ++i)
					{
						if (!bKeep[i]) { continue; }

						const FCollectionData* ColData = MatchedColDatas[i];
						float MinDist = 0.0f;
						float MaxDist = MAX_FLT;
						bool bHasMin = false;
						bool bHasMax = false;

						if (ColData && MatchedRawEntryIndices[i] >= 0)
						{
							const int32 EntryPropsIdx = MatchedRawEntryIndices[i] * NumCriteria + CritIdx;
							if (ColData->EntryProps.IsValidIndex(EntryPropsIdx))
							{
								const FEntryCriterionData& PropData = ColData->EntryProps[EntryPropsIdx];
								bHasMin = PropData.bHasMin;
								bHasMax = PropData.bHasMax;
								MinDist = PropData.Min;
								MaxDist = PropData.Max;
							}
						}

						if (!bHasMin && !bHasMax)
						{
							// No distance range defined on this entry
							if (Criterion.bExcludeIfMissing)
							{
								bKeep[i] = false;
							}
							else
							{
								// No constraint — add to kept set so subsequent same-entry points see it
								KeptByHash.FindOrAdd(MatchedEntryHashes[i]).Add(
									TransformRange[MatchedPointIndices[i]].GetLocation());
							}
							continue;
						}

						const int64 Hash = MatchedEntryHashes[i];
						const FVector Loc = TransformRange[MatchedPointIndices[i]].GetLocation();
						const TArray<FVector>* KeptLocs = KeptByHash.Find(Hash);

						bool bPasses = true;
						if (KeptLocs && KeptLocs->Num() > 0)
						{
							// Compute min distance to already-kept same-entry points
							float ActualMinDist = MAX_FLT;
							for (const FVector& KeptLoc : *KeptLocs)
							{
								const float D = static_cast<float>(FVector::Dist(Loc, KeptLoc));
								if (D < ActualMinDist) { ActualMinDist = D; }
							}
							bPasses = (!bHasMin || ActualMinDist >= MinDist) && (!bHasMax || ActualMinDist <= MaxDist);
						}
						// else: first point for this entry — always passes (no neighbours to check yet)

						if (bPasses)
						{
							KeptByHash.FindOrAdd(Hash).Add(Loc);
						}
						else
						{
							bKeep[i] = false;
						}
					}
				}

				// Compact all matched arrays, removing culled points
				int32 WriteIdx = 0;
				for (int32 i = 0; i < MatchedPointIndices.Num(); ++i)
				{
					if (!bKeep[i]) { continue; }
					MatchedPointIndices[WriteIdx]    = MatchedPointIndices[i];
					MatchedEntryHashes[WriteIdx]     = MatchedEntryHashes[i];
					MatchedEntries[WriteIdx]         = MatchedEntries[i];
					MatchedCollections[WriteIdx]     = MatchedCollections[i];
					MatchedColDatas[WriteIdx]        = MatchedColDatas[i];
					MatchedRawEntryIndices[WriteIdx] = MatchedRawEntryIndices[i];
					++WriteIdx;
				}
				MatchedPointIndices.SetNum(WriteIdx);
				MatchedEntryHashes.SetNum(WriteIdx);
				MatchedEntries.SetNum(WriteIdx);
				MatchedCollections.SetNum(WriteIdx);
				MatchedColDatas.SetNum(WriteIdx);
				MatchedRawEntryIndices.SetNum(WriteIdx);

				UE_LOG(LogPCG, Log, TEXT("PGLStagingDistribute: %d points after MinDistanceToSameEntry filter"),
					MatchedPointIndices.Num());
			}
		}

		// --- Build "Out" output: partition matched points by collection type ---
		{
			// Group matched indices by collection TypeId
			TMap<FName, TArray<int32>> TypeToMatchIndices;
			for (int32 MatchIdx = 0; MatchIdx < MatchedCollections.Num(); ++MatchIdx)
			{
				const FName TypeId = MatchedCollections[MatchIdx]->GetTypeId();
				TypeToMatchIndices.FindOrAdd(TypeId).Add(MatchIdx);
			}

			// Emit one output data set per collection type, tagged with "CollectionType:<TypeName>"
			for (auto& [TypeId, MatchIndices] : TypeToMatchIndices)
			{
				// Build per-partition arrays
				TArray<int32> PartPointIndices;
				TArray<int64> PartEntryHashes;
				PartPointIndices.Reserve(MatchIndices.Num());
				PartEntryHashes.Reserve(MatchIndices.Num());

				for (const int32 MatchIdx : MatchIndices)
				{
					PartPointIndices.Add(MatchedPointIndices[MatchIdx]);
					PartEntryHashes.Add(MatchedEntryHashes[MatchIdx]);
				}

				// Sort all per-match arrays together by source point index so that
				// output point order (sorted by CopyPointSubset) matches our hash
				// and match-index arrays used for variations and metadata writes.
				{
					TArray<int32> Perm;
					Perm.SetNum(PartPointIndices.Num());
					for (int32 i = 0; i < Perm.Num(); ++i) { Perm[i] = i; }
					Perm.Sort([&PartPointIndices](int32 A, int32 B)
					{
						return PartPointIndices[A] < PartPointIndices[B];
					});

					TArray<int32> SortedPts;   SortedPts.SetNum(Perm.Num());
					TArray<int64> SortedHash;  SortedHash.SetNum(Perm.Num());
					TArray<int32> SortedMatch; SortedMatch.SetNum(Perm.Num());
					for (int32 i = 0; i < Perm.Num(); ++i)
					{
						SortedPts[i]   = PartPointIndices[Perm[i]];
						SortedHash[i]  = PartEntryHashes[Perm[i]];
						SortedMatch[i] = MatchIndices[Perm[i]];
					}
					PartPointIndices = MoveTemp(SortedPts);
					PartEntryHashes  = MoveTemp(SortedHash);
					MatchIndices     = MoveTemp(SortedMatch);
				}

				UPCGBasePointData* OutPointData = CopyPointSubset(
					InputPointData, PartPointIndices, Context);

				// Apply per-entry variations to output point transforms
				if (bApplyVariations && PartPointIndices.Num() > 0)
				{
					TPCGValueRange<FTransform> OutTransforms = OutPointData->GetTransformValueRange();

					for (int32 WriteIdx = 0; WriteIdx < MatchIndices.Num(); ++WriteIdx)
					{
						const int32 MatchIdx = MatchIndices[WriteIdx];
						const int32 SourceIdx = MatchedPointIndices[MatchIdx];
						const FPCGExFittingVariations& EntryVariations =
							MatchedEntries[MatchIdx]->GetVariations(MatchedCollections[MatchIdx]);

						const int32 VarSeed = PCGHelpers::ComputeSeed(SeedRange[SourceIdx], VariationDetails.Seed);
						FRandomStream VarStream(VarSeed);

						if (VariationDetails.bEnabledBefore)
						{
							VariationDetails.Apply(VarStream, OutTransforms[WriteIdx], EntryVariations, EPCGExVariationMode::Before);
						}
						if (VariationDetails.bEnabledAfter)
						{
							VariationDetails.Apply(VarStream, OutTransforms[WriteIdx], EntryVariations, EPCGExVariationMode::After);
						}
					}
				}

				// Update point local bounds from the assigned entry's staging bounds
				if (Settings->bUpdateBoundsFromEntries && PartPointIndices.Num() > 0)
				{
					TPCGValueRange<FVector> OutBoundsMin = OutPointData->GetBoundsMinValueRange(false);
					TPCGValueRange<FVector> OutBoundsMax = OutPointData->GetBoundsMaxValueRange(false);

					for (int32 WriteIdx = 0; WriteIdx < MatchIndices.Num(); ++WriteIdx)
					{
						const int32 MatchIdx = MatchIndices[WriteIdx];
						const FPCGExAssetCollectionEntry* Entry = MatchedEntries[MatchIdx];

						if (Entry && !Entry->bIsSubCollection && Entry->Staging.Bounds.IsValid)
						{
							OutBoundsMin[WriteIdx] = FVector(Entry->Staging.Bounds.Min);
							OutBoundsMax[WriteIdx] = FVector(Entry->Staging.Bounds.Max);
						}
					}
				}

				if (PartPointIndices.Num() > 0)
				{
					UPCGMetadata* OutMetadata = OutPointData->MutableMetadata();
					check(OutMetadata);

					// Write PCGEx/CollectionEntry (int64 packed hash)
					FPCGMetadataAttribute<int64>* EntryAttr = OutMetadata->FindOrCreateAttribute<int64>(
						PCGExCollections::Labels::Tag_EntryIdx, static_cast<int64>(-1),
						/*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);

					if (EntryAttr)
					{
						FPCGAttributePropertyOutputSelector Selector;
						Selector.SetAttributeName(PCGExCollections::Labels::Tag_EntryIdx);

						TUniquePtr<IPCGAttributeAccessor> Accessor =
							PCGAttributeAccessorHelpers::CreateAccessor(EntryAttr, OutMetadata);
						TUniquePtr<IPCGAttributeAccessorKeys> Keys =
							PCGAttributeAccessorHelpers::CreateKeys(OutPointData, Selector);

						if (Accessor && Keys)
						{
							Accessor->SetRange<int64>(MakeConstArrayView(PartEntryHashes), 0, *Keys);
						}
					}
				}

				FPCGTaggedData& Output = Outputs.Add_GetRef(CurrentInput);
				Output.Data = OutPointData;
				Output.Pin = PGLStagingDistributeConstants::OutputLabel;
				Output.Tags.Add(FString::Printf(TEXT("CollectionType:%s"), *TypeId.ToString()));
			}
		}
	}

	// --- Build "Map" output: Collection Map attribute set ---
	{
		UPCGParamData* MapData = NewObject<UPCGParamData>();
		PickPacker->PackToDataset(MapData);

		FPCGTaggedData& MapOutput = Outputs.Emplace_GetRef();
		MapOutput.Data = MapData;
		MapOutput.Pin = PGLStagingDistributeConstants::MapLabel;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
