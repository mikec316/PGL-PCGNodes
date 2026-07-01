// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "Voxel/VoxelNode_SelectMeshFromCollection.h"
#include "VoxelPointId.h"
#include "Buffer/VoxelFloatBuffers.h"
#include "Buffer/VoxelDoubleBuffers.h"
#include "Buffer/VoxelGraphStaticMeshBuffer.h"

#include "VoxelDependency.h"
#include "VoxelInvalidationCallstack.h"

#include "Core/PCGExAssetCollection.h"
#include "Collections/PCGExMeshCollection.h"
#include "Fitting/PCGExFittingVariations.h"
#include "PCGNodes/PGLFloatRangeProperty.h"
#include "PCGExPropertyTypes.h"

// ---------------------------------------------------------------------------
// Singleton: invalidate scatter graph when a UPCGExMeshCollection is edited
// ---------------------------------------------------------------------------

class FVoxelMeshCollectionDependencyManager : public FVoxelSingleton
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
			Dep = FVoxelDependency::Create("MeshCollection " + Collection.GetName());
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

// Heap-allocated so the FVoxelSingletonManager owns its lifetime.
// A global static would be freed when PGLPCGNodes unloads, but the
// singleton manager (in VoxelCore) may destroy it later — causing a
// use-after-free crash at shutdown.
FVoxelMeshCollectionDependencyManager& GVoxelMeshCollectionDependencyManager =
	*new FVoxelMeshCollectionDependencyManager();

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace VoxelSelectMeshInternal
{
	/** Pre-cached bounds for one entry / one criterion. */
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
	 * Pre-cache all criterion data for every valid entry in the collection.
	 *
	 * Returns a flat array indexed by [RawEntryIndex * NumCriteria + CritIdx].
	 * The caller must also get MaxRawIndex to size this correctly.
	 */
	static void BuildEntryCriterionCache(
		const UPCGExMeshCollection* Collection,
		const PCGExAssetCollection::FCache* Cache,
		const TArray<FVoxelMeshCriterion>& Criteria,
		TArray<FEntryCriterionData>& OutCache,
		int32& OutMaxRawIndex)
	{
		const int32 NumCriteria = Criteria.Num();
		const int32 NumCacheEntries = Cache->Main->Entries.Num();

		// Find max raw index to size the flat array
		OutMaxRawIndex = 0;
		for (int32 i = 0; i < NumCacheEntries; i++)
		{
			const int32 RawIdx = Cache->Main->Indices[i];
			OutMaxRawIndex = FMath::Max(OutMaxRawIndex, RawIdx);
		}
		OutMaxRawIndex += 1; // Convert to count

		OutCache.SetNumZeroed(OutMaxRawIndex * NumCriteria);

		for (int32 EntryIdx = 0; EntryIdx < NumCacheEntries; EntryIdx++)
		{
			const FPCGExAssetCollectionEntry* Entry = Cache->Main->Entries[EntryIdx];
			const int32 RawIdx = Cache->Main->Indices[EntryIdx];

			for (int32 CritIdx = 0; CritIdx < NumCriteria; CritIdx++)
			{
				const FVoxelMeshCriterion& Criterion = Criteria[CritIdx];
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

	/** Read a float value from voxel point buffers for a given criterion. */
	static float GetVoxelPointValue(
		const FVoxelMeshCriterion& Criterion,
		int32 Index,
		const FVoxelDoubleVectorBuffer* PositionBuffer,
		const FVoxelFloatBuffer* SteepnessBuffer,
		const FVoxelFloatBuffer* DensityBuffer,
		const TArray<const FVoxelFloatBuffer*>& CustomBuffers,
		int32 CritIdx)
	{
		switch (Criterion.PointSource)
		{
		case EVoxelPointValueSource::PositionZ:
			return PositionBuffer
				? static_cast<float>((*PositionBuffer)[Index].Z)
				: 0.0f;

		case EVoxelPointValueSource::Steepness:
			return SteepnessBuffer
				? (*SteepnessBuffer)[Index]
				: 0.0f;

		case EVoxelPointValueSource::Density:
			return DensityBuffer
				? (*DensityBuffer)[Index]
				: 0.0f;

		case EVoxelPointValueSource::CustomFloatAttribute:
			if (CustomBuffers.IsValidIndex(CritIdx) && CustomBuffers[CritIdx])
			{
				return (*CustomBuffers[CritIdx])[Index];
			}
			return 0.0f;
		}

		return 0.0f;
	}

	/** A candidate entry that passed all criteria. */
	struct FCandidate
	{
		int32 RawEntryIndex = -1;
		float Weight = 0.0f;
	};
}

// ---------------------------------------------------------------------------
// Compute
// ---------------------------------------------------------------------------

void FVoxelNode_SelectMeshFromCollection::Compute(const FVoxelGraphQuery Query) const
{
	const TValue<FVoxelPointSet> Points = InPin.Get(Query);
	const TValue<FVoxelPCGExMeshCollectionRef> CollectionRef = CollectionPin.Get(Query);
	const TValue<FVoxelSeed> Seed = SeedPin.Get(Query);

	VOXEL_GRAPH_WAIT(Points, CollectionRef, Seed)
	{
		if (Points->Num() == 0)
		{
			return;
		}

		// -----------------------------------------------------------
		// Resolve the collection asset
		// -----------------------------------------------------------

		UPCGExMeshCollection* Collection = CollectionRef.Object.Resolve();
		if (!Collection)
		{
			VOXEL_MESSAGE(Error, "{0}: No mesh collection assigned", this);
			return;
		}

		// -----------------------------------------------------------
		// Register dependency so scatter regenerates when collection
		// is edited in the editor
		// -----------------------------------------------------------

		{
			const TSharedRef<FVoxelDependency> CollectionDep =
				GVoxelMeshCollectionDependencyManager.GetDependency(*Collection);
			Query->Context.DependencyCollector.AddDependency(*CollectionDep);
		}

		// -----------------------------------------------------------
		// Build / fetch the collection's internal cache (thread-safe)
		// -----------------------------------------------------------

		PCGExAssetCollection::FCache* Cache = Collection->LoadCache();
		if (!Cache || Cache->IsEmpty() || !Cache->Main || Cache->Main->Entries.IsEmpty())
		{
			VOXEL_MESSAGE(Error, "{0}: Mesh collection is empty or has no valid entries", this);
			return;
		}

		const TArray<FPCGExMeshCollectionEntry>& Entries = Collection->Entries;

		// -----------------------------------------------------------
		// Validate input points carry an Id attribute
		// -----------------------------------------------------------

		const FVoxelPointIdBuffer* IdBuffer =
			Points->Find<FVoxelPointIdBuffer>(FVoxelPointAttributes::Id);
		if (!IdBuffer)
		{
			VOXEL_MESSAGE(Error, "{0}: Missing attribute Id", this);
			return;
		}

		// -----------------------------------------------------------
		// Per-point random generator (deterministic by Id + Seed)
		// -----------------------------------------------------------

		const FVoxelPointRandom MeshRandom(Seed, STATIC_HASH("SelectMeshFromCollection"));

		FVoxelNodeStatScope StatScope(*this, Points->Num());

		// -----------------------------------------------------------
		// Criteria setup (only when criteria are defined)
		// -----------------------------------------------------------

		const int32 NumCriteria = Criteria.Num();

		// Voxel attribute buffers for criteria evaluation
		const FVoxelDoubleVectorBuffer* PositionBuffer = nullptr;
		const FVoxelFloatBuffer* SteepnessBuffer = nullptr;
		const FVoxelFloatBuffer* DensityBuffer = nullptr;
		TArray<const FVoxelFloatBuffer*> CustomBuffers;

		// Pre-cached entry criterion data
		TArray<VoxelSelectMeshInternal::FEntryCriterionData> CriterionCache;
		int32 MaxRawIndex = 0;

		if (NumCriteria > 0)
		{
			// Look up attribute buffers that criteria reference
			for (int32 CritIdx = 0; CritIdx < NumCriteria; CritIdx++)
			{
				const FVoxelMeshCriterion& Crit = Criteria[CritIdx];

				switch (Crit.PointSource)
				{
				case EVoxelPointValueSource::PositionZ:
					if (!PositionBuffer)
					{
						PositionBuffer = Points->Find<FVoxelDoubleVectorBuffer>(
							FVoxelPointAttributes::Position);
						if (!PositionBuffer)
						{
							VOXEL_MESSAGE(Warning, "{0}: Criterion {1} needs Position attribute but it is missing", this, CritIdx);
						}
					}
					break;

				case EVoxelPointValueSource::Steepness:
					if (!SteepnessBuffer)
					{
						SteepnessBuffer = Points->Find<FVoxelFloatBuffer>(
							FVoxelPointAttributes::Steepness);
						if (!SteepnessBuffer)
						{
							VOXEL_MESSAGE(Warning, "{0}: Criterion {1} needs Steepness attribute but it is missing", this, CritIdx);
						}
					}
					break;

				case EVoxelPointValueSource::Density:
					if (!DensityBuffer)
					{
						DensityBuffer = Points->Find<FVoxelFloatBuffer>(
							FVoxelPointAttributes::Density);
						if (!DensityBuffer)
						{
							VOXEL_MESSAGE(Warning, "{0}: Criterion {1} needs Density attribute but it is missing", this, CritIdx);
						}
					}
					break;

				case EVoxelPointValueSource::CustomFloatAttribute:
					break;
				}
			}

			// Resolve custom float attribute buffers per-criterion
			CustomBuffers.SetNum(NumCriteria);
			for (int32 CritIdx = 0; CritIdx < NumCriteria; CritIdx++)
			{
				CustomBuffers[CritIdx] = nullptr;
				if (Criteria[CritIdx].PointSource == EVoxelPointValueSource::CustomFloatAttribute
					&& !Criteria[CritIdx].CustomAttributeName.IsNone())
				{
					CustomBuffers[CritIdx] = Points->Find<FVoxelFloatBuffer>(
						Criteria[CritIdx].CustomAttributeName);
					if (!CustomBuffers[CritIdx])
					{
						VOXEL_MESSAGE(Warning, "{0}: Criterion {1} needs custom attribute '{2}' but it is missing",
							this, CritIdx, Criteria[CritIdx].CustomAttributeName);
					}
				}
			}

			// Pre-cache criterion property data for all entries
			VoxelSelectMeshInternal::BuildEntryCriterionCache(
				Collection, Cache, Criteria, CriterionCache, MaxRawIndex);
		}

		// -----------------------------------------------------------
		// Variations setup
		// -----------------------------------------------------------

		// Read existing transform buffers if variations are enabled
		const FVoxelDoubleVectorBuffer* InPositionBuffer = nullptr;
		const FVoxelQuaternionBuffer* InRotationBuffer = nullptr;
		const FVoxelVectorBuffer* InScaleBuffer = nullptr;

		if (bApplyVariations)
		{
			InPositionBuffer = Points->Find<FVoxelDoubleVectorBuffer>(FVoxelPointAttributes::Position);
			InRotationBuffer = Points->Find<FVoxelQuaternionBuffer>(FVoxelPointAttributes::Rotation);
			InScaleBuffer = Points->Find<FVoxelVectorBuffer>(FVoxelPointAttributes::Scale);

			// Also ensure PositionBuffer is available for criteria if needed
			if (!PositionBuffer && InPositionBuffer)
			{
				PositionBuffer = InPositionBuffer;
			}
		}

		// -----------------------------------------------------------
		// Allocate output buffers
		// -----------------------------------------------------------

		const int32 NumPoints = Points->Num();

		FVoxelGraphStaticMeshBuffer MeshBuffer;
		MeshBuffer.Allocate(NumPoints);

		// Variation output buffers (only allocated when needed)
		FVoxelDoubleVectorBuffer OutPositionBuffer;
		FVoxelQuaternionBuffer OutRotationBuffer;
		FVoxelVectorBuffer OutScaleBuffer;

		if (bApplyVariations)
		{
			OutPositionBuffer.Allocate(NumPoints);
			OutRotationBuffer.Allocate(NumPoints);
			OutScaleBuffer.Allocate(NumPoints);

			// Initialize from existing values
			for (int32 i = 0; i < NumPoints; i++)
			{
				OutPositionBuffer.Set(i, InPositionBuffer ? (*InPositionBuffer)[i] : FVector::ZeroVector);
				OutRotationBuffer.Set(i, InRotationBuffer ? (*InRotationBuffer)[i] : FQuat4f::Identity);
				OutScaleBuffer.Set(i, InScaleBuffer ? (*InScaleBuffer)[i] : FVector3f::OneVector);
			}
		}

		// Local cache: avoid redundant LoadSynchronous calls
		TMap<FSoftObjectPath, UStaticMesh*> LoadedMeshes;

		// Helper: resolve a raw entry index to a FVoxelGraphStaticMesh
		auto ResolveMesh = [&](int32 RawEntryIndex) -> FVoxelGraphStaticMesh
		{
			if (!Entries.IsValidIndex(RawEntryIndex))
			{
				return FVoxelGraphStaticMesh{};
			}

			const FPCGExMeshCollectionEntry& MeshEntry = Entries[RawEntryIndex];
			const FSoftObjectPath& AssetPath = MeshEntry.StaticMesh.ToSoftObjectPath();

			UStaticMesh* LoadedMesh = nullptr;
			if (UStaticMesh** Found = LoadedMeshes.Find(AssetPath))
			{
				LoadedMesh = *Found;
			}
			else
			{
				LoadedMesh = MeshEntry.StaticMesh.LoadSynchronous();
				LoadedMeshes.Add(AssetPath, LoadedMesh);
			}

			FVoxelGraphStaticMesh GraphMesh;
			GraphMesh.StaticMesh = LoadedMesh;
			return GraphMesh;
		};

		// -----------------------------------------------------------
		// Pick a mesh for every point
		// -----------------------------------------------------------

		// Reusable candidate array (allocated once, reset per point)
		TArray<VoxelSelectMeshInternal::FCandidate> Candidates;

		for (int32 Index = 0; Index < NumPoints; Index++)
		{
			const FVoxelPointId PointId = (*IdBuffer)[Index];
			const int32 PointSeed = static_cast<int32>(
				MeshRandom.BaseHash ^ PointId.PointId);

			// Will be set by either path
			int32 SelectedRawIdx = -1;

			if (NumCriteria == 0)
			{
				// -----------------------------------------------
				// Fast path: no criteria — pick directly from cache
				// -----------------------------------------------

				switch (Distribution)
				{
				case EVoxelCollectionDistribution::WeightedRandom:
					SelectedRawIdx = Cache->Main->GetPickRandomWeighted(PointSeed);
					break;
				case EVoxelCollectionDistribution::Random:
					SelectedRawIdx = Cache->Main->GetPickRandom(PointSeed);
					break;
				}
			}
			else
			{
				// -----------------------------------------------
				// Filtered path: test criteria, collect candidates
				// -----------------------------------------------

				Candidates.Reset();
				float TotalWeight = 0.0f;

				const int32 NumCacheEntries = Cache->Main->Entries.Num();

				for (int32 EntryIdx = 0; EntryIdx < NumCacheEntries; EntryIdx++)
				{
					const int32 RawIdx = Cache->Main->Indices[EntryIdx];
					bool bPassesAll = true;

					for (int32 CritIdx = 0; CritIdx < NumCriteria; CritIdx++)
					{
						const FVoxelMeshCriterion& Criterion = Criteria[CritIdx];
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

						const VoxelSelectMeshInternal::FEntryCriterionData& PropData =
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

						const float PointValue =
							VoxelSelectMeshInternal::GetVoxelPointValue(
								Criterion, Index,
								PositionBuffer, SteepnessBuffer, DensityBuffer,
								CustomBuffers, CritIdx);

						if (PropData.bIsComparison)
						{
							bool bPasses = false;
							switch (PropData.Operator)
							{
							case EVoxelCriterionFloatOperator::Greater:
								bPasses = PointValue > PropData.Min;
								break;
							case EVoxelCriterionFloatOperator::GreaterOrEqual:
								bPasses = PointValue >= PropData.Min;
								break;
							case EVoxelCriterionFloatOperator::Less:
								bPasses = PointValue < PropData.Min;
								break;
							case EVoxelCriterionFloatOperator::LessOrEqual:
								bPasses = PointValue <= PropData.Min;
								break;
							case EVoxelCriterionFloatOperator::Equal:
								bPasses = FMath::Abs(PointValue - PropData.Min) <= PropData.ComparisonTolerance;
								break;
							case EVoxelCriterionFloatOperator::NotEqual:
								bPasses = FMath::Abs(PointValue - PropData.Min) > PropData.ComparisonTolerance;
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

				// Weighted random from candidates
				if (!Candidates.IsEmpty() && TotalWeight > 0.0f)
				{
					FRandomStream PointStream(PointSeed);
					float Roll = PointStream.FRand() * TotalWeight;

					SelectedRawIdx = Candidates[0].RawEntryIndex;
					for (const VoxelSelectMeshInternal::FCandidate& C : Candidates)
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
			// Set mesh for this point
			// -----------------------------------------------------------

			if (!Entries.IsValidIndex(SelectedRawIdx))
			{
				MeshBuffer.Set(Index, FVoxelGraphStaticMesh{});
				continue;
			}

			MeshBuffer.Set(Index, ResolveMesh(SelectedRawIdx));

			// -----------------------------------------------------------
			// Apply entry variations to transform (if enabled)
			// -----------------------------------------------------------

			if (bApplyVariations)
			{
				// Get the base entry to resolve variations
				const FPCGExAssetCollectionEntry* BaseEntry =
					static_cast<const FPCGExAssetCollectionEntry*>(&Entries[SelectedRawIdx]);

				const FPCGExFittingVariations& Variations =
					BaseEntry->GetVariations(Collection);

				// Build a FTransform from the point's current attributes
				// Voxel buffers use float precision (FVector3f / FQuat4f);
				// FTransform uses double — explicit casts required.
				const FVector Position = InPositionBuffer
					? FVector((*InPositionBuffer)[Index])
					: FVector::ZeroVector;
				const FQuat Rotation = InRotationBuffer
					? FQuat((*InRotationBuffer)[Index])
					: FQuat::Identity;
				const FVector Scale = InScaleBuffer
					? FVector((*InScaleBuffer)[Index])
					: FVector::OneVector;

				FTransform PointTransform(Rotation, Position, Scale);

				// Use a deterministic seed derived from point ID + a variation salt
				const int32 VarSeed = static_cast<int32>(
					MeshRandom.BaseHash ^ PointId.PointId ^ STATIC_HASH("Variations"));
				FRandomStream VarStream(VarSeed);

				// Apply all three variation types
				Variations.ApplyOffset(VarStream, PointTransform);
				Variations.ApplyRotation(VarStream, PointTransform);
				Variations.ApplyScale(VarStream, PointTransform);

				// Decompose back to float-precision buffers
				OutPositionBuffer.Set(Index, PointTransform.GetLocation());
				OutRotationBuffer.Set(Index, FQuat4f(PointTransform.GetRotation()));
				OutScaleBuffer.Set(Index, FVector3f(PointTransform.GetScale3D()));
			}
		}

		// -----------------------------------------------------------
		// Build output point set
		// -----------------------------------------------------------

		const TSharedRef<FVoxelPointSet> Result = MakeShared<FVoxelPointSet>();
		Result->SetNum(NumPoints);

		// Copy all existing attributes from input
		for (const auto& It : Points->GetAttributes())
		{
			Result->Add(It.Key, It.Value.ToSharedRef());
		}

		// Write mesh buffer (overwrites any previous Mesh attribute)
		Result->Add(FVoxelPointAttributes::Mesh, MoveTemp(MeshBuffer));

		// Write modified transform buffers (overwrites existing)
		if (bApplyVariations)
		{
			Result->Add(FVoxelPointAttributes::Position, MoveTemp(OutPositionBuffer));
			Result->Add(FVoxelPointAttributes::Rotation, MoveTemp(OutRotationBuffer));
			Result->Add(FVoxelPointAttributes::Scale, MoveTemp(OutScaleBuffer));
		}

		OutPin.Set(Query, Result);
	};
}
