// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "Voxel/VoxelNode_SelectSurfaceTypeBlend.h"

#include "VoxelBufferAccessor.h"
#include "VoxelDependency.h"
#include "VoxelInvalidationCallstack.h"

#include "Surface/VoxelSurfaceType.h"
#include "Surface/VoxelSurfaceTypeBlendBuilder.h"
#include "Surface/VoxelSurfaceTypeInterface.h"

#include "Core/PCGExAssetCollection.h"
#include "PGLVoxelSurfaceTypeCollection.h"
#include "PCGNodes/PGLFloatRangeProperty.h"
#include "PCGExPropertyTypes.h"

// ---------------------------------------------------------------------------
// Singleton: invalidate dependent graphs when a surface type collection edits.
// Mirrors the manager in VoxelNode_SelectMeshFromCollection.cpp; kept separate
// so each node tracks only the collections it actually consumes.
// ---------------------------------------------------------------------------

class FVoxelSurfaceTypeCollectionDependencyManager : public FVoxelSingleton
{
public:
	virtual void Initialize() override
	{
#if WITH_EDITOR
		FCoreUObjectDelegates::OnObjectPropertyChanged.AddLambda(
			[this](UObject* Object, const FPropertyChangedEvent& PropertyChangedEvent)
			{
				if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
				{
					return;
				}

				const UPCGExAssetCollection* Collection = Cast<UPCGExAssetCollection>(Object);
				if (!Collection)
				{
					return;
				}

				UpdateCollection_GameThread(*Collection);
			});
#endif
	}

	TSharedRef<FVoxelDependency> GetDependency(const UPCGExAssetCollection& Collection)
	{
		VOXEL_SCOPE_LOCK(CriticalSection);

		TSharedPtr<FVoxelDependency>& Dep = CollectionToDependency.FindOrAdd(&Collection);
		if (!Dep)
		{
			Dep = FVoxelDependency::Create("SurfaceTypeCollection " + Collection.GetName());
		}
		return Dep.ToSharedRef();
	}

private:
	void UpdateCollection_GameThread(const UPCGExAssetCollection& Collection)
	{
		check(IsInGameThread());

		TSharedPtr<FVoxelDependency> Dep;
		{
			VOXEL_SCOPE_LOCK(CriticalSection);
			Dep = CollectionToDependency.FindRef(&Collection);
		}

		if (!Dep)
		{
			return;
		}

		FVoxelInvalidationScope Scope(Collection);
		Dep->Invalidate();
	}

	FVoxelCriticalSection CriticalSection;
	TVoxelMap<TWeakObjectPtr<const UPCGExAssetCollection>, TSharedPtr<FVoxelDependency>> CollectionToDependency;
};

// Heap-allocated so FVoxelSingletonManager owns lifetime — see comment in
// VoxelNode_SelectMeshFromCollection.cpp for the shutdown rationale.
FVoxelSurfaceTypeCollectionDependencyManager& GVoxelSurfaceTypeCollectionDependencyManager =
	*new FVoxelSurfaceTypeCollectionDependencyManager();

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace VoxelSelectSurfaceTypeInternal
{
	struct FEntryCriterionData
	{
		float Min = -MAX_FLT;
		float Max = MAX_FLT;
		bool bHasMin = false;
		bool bHasMax = false;
		bool bIsEquality = false;
		bool bIsComparison = false;
		EVoxelCriterionFloatOperator Operator = EVoxelCriterionFloatOperator::Greater;
		float ComparisonTolerance = 0.001f;
	};

	/**
	 * Pre-cache property bounds for every valid entry × every criterion.
	 * Indexed as [RawEntryIndex * NumCriteria + CritIdx].
	 */
	static void BuildEntryCriterionCache(
		const UPGLVoxelSurfaceTypeCollection* Collection,
		const PCGExAssetCollection::FCache* Cache,
		const TArray<FVoxelSurfaceTypeCriterion>& Criteria,
		TArray<FEntryCriterionData>& OutCache,
		int32& OutMaxRawIndex)
	{
		const int32 NumCriteria = Criteria.Num();
		const int32 NumCacheEntries = Cache->Main->Entries.Num();

		OutMaxRawIndex = 0;
		for (int32 i = 0; i < NumCacheEntries; i++)
		{
			OutMaxRawIndex = FMath::Max(OutMaxRawIndex, Cache->Main->Indices[i]);
		}
		OutMaxRawIndex += 1;

		OutCache.SetNumZeroed(OutMaxRawIndex * NumCriteria);

		for (int32 EntryIdx = 0; EntryIdx < NumCacheEntries; EntryIdx++)
		{
			const FPCGExAssetCollectionEntry* Entry = Cache->Main->Entries[EntryIdx];
			const int32 RawIdx = Cache->Main->Indices[EntryIdx];

			for (int32 CritIdx = 0; CritIdx < NumCriteria; CritIdx++)
			{
				const FVoxelSurfaceTypeCriterion& Criterion = Criteria[CritIdx];
				FEntryCriterionData& PropData = OutCache[RawIdx * NumCriteria + CritIdx];

				switch (Criterion.PropertyMode)
				{
				case EVoxelCriterionPropertyMode::FloatRange:
				{
					if (const FPGLProperty_FloatRange* Prop =
						Entry->GetResolvedProperty<FPGLProperty_FloatRange>(
							Collection, Criterion.RangePropertyName))
					{
						PropData.Min = Prop->Min;
						PropData.Max = Prop->Max;
						PropData.bHasMin = true;
						PropData.bHasMax = true;
					}
					break;
				}

				case EVoxelCriterionPropertyMode::FloatEquals:
				{
					if (const FPCGExProperty_Float* Prop =
						Entry->GetResolvedProperty<FPCGExProperty_Float>(
							Collection, Criterion.EqualsPropertyName))
					{
						PropData.Min = Prop->Value;
						PropData.bHasMin = true;
						PropData.bIsEquality = true;
					}
					break;
				}

				case EVoxelCriterionPropertyMode::SeparateMinMax:
				{
					if (const FPCGExProperty_Float* MinProp =
						Entry->GetResolvedProperty<FPCGExProperty_Float>(
							Collection, Criterion.MinPropertyName))
					{
						PropData.Min = MinProp->Value;
						PropData.bHasMin = true;
					}
					if (const FPCGExProperty_Float* MaxProp =
						Entry->GetResolvedProperty<FPCGExProperty_Float>(
							Collection, Criterion.MaxPropertyName))
					{
						PropData.Max = MaxProp->Value;
						PropData.bHasMax = true;
					}
					break;
				}

				case EVoxelCriterionPropertyMode::FloatComparison:
				{
					if (const FPCGExProperty_Float* Prop =
						Entry->GetResolvedProperty<FPCGExProperty_Float>(
							Collection, Criterion.ComparisonPropertyName))
					{
						PropData.Min = Prop->Value;
						PropData.bHasMin = true;
						PropData.bIsComparison = true;
						PropData.Operator = Criterion.ComparisonOperator;
						PropData.ComparisonTolerance = Criterion.ComparisonTolerance;
					}
					break;
				}
				}
			}
		}
	}

	struct FCandidate
	{
		int32 RawEntryIndex = -1;
		float Weight = 0.0f;
	};
}

// ---------------------------------------------------------------------------
// Compute
// ---------------------------------------------------------------------------

void FVoxelNode_SelectSurfaceTypeBlend::Compute(const FVoxelGraphQuery Query) const
{
	const TValue<FVoxelFloatBuffer> Heights = HeightPin.Get(Query);
	const TValue<FVoxelFloatBuffer> Noises = NoisePin.Get(Query);
	const TValue<FVoxelPCGExSurfaceTypeCollectionRef> CollectionRef = CollectionPin.Get(Query);
	const TValue<FVoxelSeed> Seed = SeedPin.Get(Query);

	VOXEL_GRAPH_WAIT(Heights, Noises, CollectionRef, Seed)
	{
		// Match buffer sizes (broadcast scalars to vector length)
		int32 Num = 1;
		if (!FVoxelBufferAccessor::MergeNum(Num, *Heights) ||
			!FVoxelBufferAccessor::MergeNum(Num, *Noises))
		{
			RaiseBufferError();
			return;
		}

		// -----------------------------------------------------------
		// Resolve the collection asset
		// -----------------------------------------------------------

		UPGLVoxelSurfaceTypeCollection* Collection = CollectionRef.Object.Resolve();
		if (!Collection)
		{
			VOXEL_MESSAGE(Error, "{0}: No surface type collection assigned", this);
			return;
		}

		// Register dependency so graph regenerates when collection edits
		{
			const TSharedRef<FVoxelDependency> CollectionDep =
				GVoxelSurfaceTypeCollectionDependencyManager.GetDependency(*Collection);
			Query->Context.DependencyCollector.AddDependency(*CollectionDep);
		}

		// -----------------------------------------------------------
		// Build / fetch the collection's cache (thread-safe)
		// -----------------------------------------------------------

		PCGExAssetCollection::FCache* Cache = Collection->LoadCache();
		if (!Cache || Cache->IsEmpty() || !Cache->Main || Cache->Main->Entries.IsEmpty())
		{
			VOXEL_MESSAGE(Error, "{0}: Surface type collection is empty or has no valid entries", this);
			return;
		}

		const TArray<FPGLVoxelSurfaceTypeCollectionEntry>& Entries = Collection->Entries;

		FVoxelNodeStatScope StatScope(*this, Num);

		// -----------------------------------------------------------
		// Criterion property cache
		// -----------------------------------------------------------

		const int32 NumCriteria = Criteria.Num();
		TArray<VoxelSelectSurfaceTypeInternal::FEntryCriterionData> CriterionCache;
		int32 MaxRawIndex = 0;

		if (NumCriteria > 0)
		{
			VoxelSelectSurfaceTypeInternal::BuildEntryCriterionCache(
				Collection, Cache, Criteria, CriterionCache, MaxRawIndex);
		}

		// -----------------------------------------------------------
		// Local cache: avoid re-loading the same surface type asset and
		// re-converting it to FVoxelSurfaceType for every element.
		// -----------------------------------------------------------

		TMap<int32, FVoxelSurfaceType> RawIndexToSurfaceType;

		auto ResolveSurfaceType = [&](int32 RawEntryIndex) -> FVoxelSurfaceType
		{
			if (FVoxelSurfaceType* Found = RawIndexToSurfaceType.Find(RawEntryIndex))
			{
				return *Found;
			}

			FVoxelSurfaceType Result;

			if (Entries.IsValidIndex(RawEntryIndex))
			{
				const FPGLVoxelSurfaceTypeCollectionEntry& Entry = Entries[RawEntryIndex];
				if (UVoxelSurfaceTypeInterface* Asset = Entry.SurfaceType.LoadSynchronous())
				{
					Result = FVoxelSurfaceType(Asset);
				}
			}

			RawIndexToSurfaceType.Add(RawEntryIndex, Result);
			return Result;
		};

		// -----------------------------------------------------------
		// Allocate output blend buffer
		// -----------------------------------------------------------

		const TSharedRef<FVoxelSurfaceTypeBlendBuffer> Result = MakeShared<FVoxelSurfaceTypeBlendBuffer>();
		Result->Allocate(Num);

		const uint32 BaseSeedHash = STATIC_HASH("SelectSurfaceTypeBlend") ^ Seed.Seed;

		FVoxelSurfaceTypeBlendBuilder Builder;
		TArray<VoxelSelectSurfaceTypeInternal::FCandidate> Candidates;

		// -----------------------------------------------------------
		// Per-element pick
		// -----------------------------------------------------------

		for (int32 Index = 0; Index < Num; Index++)
		{
			const int32 ElementSeed = static_cast<int32>(BaseSeedHash ^ static_cast<uint32>(Index));

			int32 SelectedRawIdx = -1;

			if (NumCriteria == 0)
			{
				// No criteria — pick from the cache directly
				switch (Distribution)
				{
				case EVoxelCollectionDistribution::WeightedRandom:
					SelectedRawIdx = Cache->Main->GetPickRandomWeighted(ElementSeed);
					break;
				case EVoxelCollectionDistribution::Random:
					SelectedRawIdx = Cache->Main->GetPickRandom(ElementSeed);
					break;
				}
			}
			else
			{
				// Read per-element values for this iteration once
				const float HeightValue = (*Heights)[Index];
				const float NoiseValue = (*Noises)[Index];

				Candidates.Reset();
				float TotalWeight = 0.0f;

				const int32 NumCacheEntries = Cache->Main->Entries.Num();

				for (int32 EntryIdx = 0; EntryIdx < NumCacheEntries; EntryIdx++)
				{
					const int32 RawIdx = Cache->Main->Indices[EntryIdx];
					bool bPassesAll = true;

					for (int32 CritIdx = 0; CritIdx < NumCriteria; CritIdx++)
					{
						const FVoxelSurfaceTypeCriterion& Criterion = Criteria[CritIdx];
						const int32 PropIdx = RawIdx * NumCriteria + CritIdx;

						if (!CriterionCache.IsValidIndex(PropIdx))
						{
							if (Criterion.bExcludeIfMissing)
							{
								bPassesAll = false;
								break;
							}
							continue;
						}

						const VoxelSelectSurfaceTypeInternal::FEntryCriterionData& PropData =
							CriterionCache[PropIdx];

						if (!PropData.bHasMin && !PropData.bHasMax)
						{
							if (Criterion.bExcludeIfMissing)
							{
								bPassesAll = false;
								break;
							}
							continue;
						}

						const float Value =
							(Criterion.BufferSource == EVoxelSurfaceTypeBufferSource::Height)
							? HeightValue
							: NoiseValue;

						if (PropData.bIsComparison)
						{
							bool bPasses = false;
							switch (PropData.Operator)
							{
							case EVoxelCriterionFloatOperator::Greater:
								bPasses = Value > PropData.Min;
								break;
							case EVoxelCriterionFloatOperator::GreaterOrEqual:
								bPasses = Value >= PropData.Min;
								break;
							case EVoxelCriterionFloatOperator::Less:
								bPasses = Value < PropData.Min;
								break;
							case EVoxelCriterionFloatOperator::LessOrEqual:
								bPasses = Value <= PropData.Min;
								break;
							case EVoxelCriterionFloatOperator::Equal:
								bPasses = FMath::Abs(Value - PropData.Min) <= PropData.ComparisonTolerance;
								break;
							case EVoxelCriterionFloatOperator::NotEqual:
								bPasses = FMath::Abs(Value - PropData.Min) > PropData.ComparisonTolerance;
								break;
							}
							if (!bPasses)
							{
								bPassesAll = false;
								break;
							}
						}
						else if (PropData.bIsEquality)
						{
							if (FMath::Abs(Value - PropData.Min) > Criterion.EqualsTolerance)
							{
								bPassesAll = false;
								break;
							}
						}
						else if (Value < PropData.Min || Value > PropData.Max)
						{
							bPassesAll = false;
							break;
						}
					}

					if (!bPassesAll)
					{
						continue;
					}

					const FPCGExAssetCollectionEntry* Entry = Cache->Main->Entries[EntryIdx];
					const float EntryWeight =
						(Distribution == EVoxelCollectionDistribution::WeightedRandom)
						? FMath::Max(static_cast<float>(Entry->Weight), 1.0f)
						: 1.0f;

					Candidates.Add({ RawIdx, EntryWeight });
					TotalWeight += EntryWeight;
				}

				if (!Candidates.IsEmpty() && TotalWeight > 0.0f)
				{
					FRandomStream Stream(ElementSeed);
					float Roll = Stream.FRand() * TotalWeight;

					SelectedRawIdx = Candidates[0].RawEntryIndex;
					for (const VoxelSelectSurfaceTypeInternal::FCandidate& C : Candidates)
					{
						Roll -= C.Weight;
						if (Roll <= 0.0f)
						{
							SelectedRawIdx = C.RawEntryIndex;
							break;
						}
					}
				}
			}

			// -----------------------------------------------------------
			// Build the blend (single layer, Weight=1) for this element
			// -----------------------------------------------------------

			Builder.Reset();

			if (SelectedRawIdx >= 0)
			{
				const FVoxelSurfaceType Type = ResolveSurfaceType(SelectedRawIdx);
				if (!Type.IsNull())
				{
					Builder.AddLayer(Type, 1.0f);
				}
			}

			Builder.Build(Result->View()[Index]);
		}

		SurfaceTypeBlendPin.Set(Query, Result);
	};
}
