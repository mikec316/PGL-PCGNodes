// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PGLRegisterEncounterPoints.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGGraphExecutionStateInterface.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Grid/PCGPartitionActor.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

#include "Enemies/PGLEncounterSubsystem.h"
#include "Enemies/PGLEncounterTypes.h"

#include "Engine/World.h"
#include "UObject/ObjectKey.h"
#include "UObject/SoftObjectPath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PGLRegisterEncounterPoints)

#define LOCTEXT_NAMESPACE "PGLRegisterEncounterPointsElement"

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
namespace PGLRegisterEncounterPointsInternal
{
	const FName EliteTierName = TEXT("Elite");

	/**
	 * Typed attribute views over one input data set, resolved ONCE before the point loop.
	 * GetConstTypedAttribute returns null on type mismatch (no static_cast roulette), so each
	 * storage type a graph might author gets its own slot — content stores BiomeID as int or
	 * name depending on which source graph emitted it, and we tolerate all of them.
	 */
	struct FResolvedAttributes
	{
		const FPCGMetadataAttribute<FName>* BiomeAsName = nullptr;
		const FPCGMetadataAttribute<FString>* BiomeAsString = nullptr;
		const FPCGMetadataAttribute<int32>* BiomeAsInt32 = nullptr;
		const FPCGMetadataAttribute<int64>* BiomeAsInt64 = nullptr;

		const FPCGMetadataAttribute<FName>* TierAsName = nullptr;
		const FPCGMetadataAttribute<FString>* TierAsString = nullptr;
		const FPCGMetadataAttribute<int32>* TierAsInt32 = nullptr;
		const FPCGMetadataAttribute<int64>* TierAsInt64 = nullptr;

		const FPCGMetadataAttribute<FSoftObjectPath>* ArchetypeAsPath = nullptr;
		const FPCGMetadataAttribute<FString>* ArchetypeAsString = nullptr;

		const FPCGMetadataAttribute<int32>* PackIdAsInt32 = nullptr;
		const FPCGMetadataAttribute<int64>* PackIdAsInt64 = nullptr;

		const FPCGMetadataAttribute<float>* WeightAsFloat = nullptr;
		const FPCGMetadataAttribute<double>* WeightAsDouble = nullptr;

		void Resolve(const UPCGMetadata* Metadata, const UPGLRegisterEncounterPointsSettings* Settings)
		{
			if (!Metadata)
			{
				return;
			}

			BiomeAsName = Metadata->GetConstTypedAttribute<FName>(Settings->BiomeAttribute);
			BiomeAsString = Metadata->GetConstTypedAttribute<FString>(Settings->BiomeAttribute);
			BiomeAsInt32 = Metadata->GetConstTypedAttribute<int32>(Settings->BiomeAttribute);
			BiomeAsInt64 = Metadata->GetConstTypedAttribute<int64>(Settings->BiomeAttribute);

			TierAsName = Metadata->GetConstTypedAttribute<FName>(Settings->TierAttribute);
			TierAsString = Metadata->GetConstTypedAttribute<FString>(Settings->TierAttribute);
			TierAsInt32 = Metadata->GetConstTypedAttribute<int32>(Settings->TierAttribute);
			TierAsInt64 = Metadata->GetConstTypedAttribute<int64>(Settings->TierAttribute);

			ArchetypeAsPath = Metadata->GetConstTypedAttribute<FSoftObjectPath>(Settings->ArchetypeAttribute);
			ArchetypeAsString = Metadata->GetConstTypedAttribute<FString>(Settings->ArchetypeAttribute);

			PackIdAsInt32 = Metadata->GetConstTypedAttribute<int32>(Settings->PackIdAttribute);
			PackIdAsInt64 = Metadata->GetConstTypedAttribute<int64>(Settings->PackIdAttribute);

			WeightAsFloat = Metadata->GetConstTypedAttribute<float>(Settings->WeightAttribute);
			WeightAsDouble = Metadata->GetConstTypedAttribute<double>(Settings->WeightAttribute);
		}

		FName ReadBiome(PCGMetadataEntryKey Entry) const
		{
			if (BiomeAsName)
			{
				return BiomeAsName->GetValueFromItemKey(Entry);
			}
			if (BiomeAsString)
			{
				const FString Value = BiomeAsString->GetValueFromItemKey(Entry);
				return Value.IsEmpty() ? NAME_None : FName(*Value);
			}
			// Integer biome ids become name-ified numbers ("3") — the table author keys rows the same way.
			if (BiomeAsInt32)
			{
				return FName(*LexToString(BiomeAsInt32->GetValueFromItemKey(Entry)));
			}
			if (BiomeAsInt64)
			{
				return FName(*LexToString(BiomeAsInt64->GetValueFromItemKey(Entry)));
			}
			// NAME_None routes to the encounter table's default row.
			return NAME_None;
		}

		EPGLEnemyTier ReadTier(PCGMetadataEntryKey Entry) const
		{
			if (TierAsName)
			{
				// FName equality is case-insensitive by construction.
				return TierAsName->GetValueFromItemKey(Entry) == EliteTierName ? EPGLEnemyTier::Elite : EPGLEnemyTier::Fodder;
			}
			if (TierAsString)
			{
				return TierAsString->GetValueFromItemKey(Entry).Equals(TEXT("Elite"), ESearchCase::IgnoreCase) ? EPGLEnemyTier::Elite : EPGLEnemyTier::Fodder;
			}
			if (TierAsInt32)
			{
				return TierAsInt32->GetValueFromItemKey(Entry) == 1 ? EPGLEnemyTier::Elite : EPGLEnemyTier::Fodder;
			}
			if (TierAsInt64)
			{
				return TierAsInt64->GetValueFromItemKey(Entry) == 1 ? EPGLEnemyTier::Elite : EPGLEnemyTier::Fodder;
			}
			// Missing attribute -> every point is Fodder; plain fodder graphs author no tier at all.
			return EPGLEnemyTier::Fodder;
		}

		TSoftObjectPtr<UPGLEnemyArchetype> ReadArchetype(PCGMetadataEntryKey Entry) const
		{
			// Path only — NEVER LoadSynchronous here. Registration must stay load-free; the
			// subsystem resolves the archetype at activation time.
			if (ArchetypeAsPath)
			{
				return TSoftObjectPtr<UPGLEnemyArchetype>(ArchetypeAsPath->GetValueFromItemKey(Entry));
			}
			if (ArchetypeAsString)
			{
				const FString Value = ArchetypeAsString->GetValueFromItemKey(Entry);
				if (!Value.IsEmpty())
				{
					return TSoftObjectPtr<UPGLEnemyArchetype>(FSoftObjectPath(Value));
				}
			}
			return TSoftObjectPtr<UPGLEnemyArchetype>();
		}

		int32 ReadPackId(PCGMetadataEntryKey Entry) const
		{
			if (PackIdAsInt32)
			{
				return PackIdAsInt32->GetValueFromItemKey(Entry);
			}
			if (PackIdAsInt64)
			{
				return static_cast<int32>(PackIdAsInt64->GetValueFromItemKey(Entry));
			}
			// Missing -> 0: the whole chunk activates/clears as one pack.
			return 0;
		}

		float ReadWeight(PCGMetadataEntryKey Entry) const
		{
			if (WeightAsFloat)
			{
				return WeightAsFloat->GetValueFromItemKey(Entry);
			}
			if (WeightAsDouble)
			{
				return static_cast<float>(WeightAsDouble->GetValueFromItemKey(Entry));
			}
			return 1.0f;
		}
	};
}

// -----------------------------------------------------------------------------
// UPGLRegisterEncounterPointsSettings
// -----------------------------------------------------------------------------

#if WITH_EDITOR
FText UPGLRegisterEncounterPointsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Registers input points as encounter candidates with the encounter subsystem (pass-through).\n"
		"PCG only nominates where enemies COULD live; the subsystem owns the pool, budget and proximity\n"
		"activation. Re-running a cell replaces that cell's previous registration (idempotent).\n"
		"Reads BiomeID / EncounterTier / Archetype / PackId / Weight attributes — all optional.");
}
#endif

TArray<FPCGPinProperties> UPGLRegisterEncounterPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point).SetRequiredPin();
	return PinProperties;
}

TArray<FPCGPinProperties> UPGLRegisterEncounterPointsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
	return PinProperties;
}

FPCGElementPtr UPGLRegisterEncounterPointsSettings::CreateElement() const
{
	return MakeShared<FPGLRegisterEncounterPointsElement>();
}

// -----------------------------------------------------------------------------
// FPGLRegisterEncounterPointsElement
// -----------------------------------------------------------------------------

bool FPGLRegisterEncounterPointsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPGLRegisterEncounterPointsElement::Execute);

	check(Context);

	const UPGLRegisterEncounterPointsSettings* Settings = Context->GetInputSettings<UPGLRegisterEncounterPointsSettings>();
	check(Settings);

	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	// Pass-through FIRST: whatever happens below (no component, preview world, missing subsystem),
	// downstream nodes must still receive the points untouched. Data and tags are forwarded as-is;
	// only the pin is re-labeled so the data routes through our output pin.
	for (const FPCGTaggedData& Input : Inputs)
	{
		FPCGTaggedData& Output = Context->OutputData.TaggedData.Add_GetRef(Input);
		Output.Pin = PCGPinConstants::DefaultOutputLabel;
	}

	// Componentless execution source (runtime-gen can execute without a UPCGComponent): no owner
	// to key a chunk by and no cleaned delegate to bind, so registration is impossible — not an error.
	UPCGComponent* SourceComponent = Cast<UPCGComponent>(Context->ExecutionSource.Get());
	if (!SourceComponent)
	{
		PCGE_LOG(Warning, LogOnly, LOCTEXT("NoSourceComponent", "Componentless execution source — encounter points pass through unregistered."));
		return true;
	}

	// Editor preview generation must never touch the registry: the subsystem only exists for game
	// worlds, and preview registrations would be stale garbage the moment PIE starts.
	UWorld* World = SourceComponent->GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return true;
	}

	UPGLEncounterSubsystem* Subsystem = UPGLEncounterSubsystem::Get(SourceComponent);
	if (!Subsystem)
	{
		return true;
	}

	// Chunk identity. Partitioned (the normal runtime-gen path): the owning partition actor IS the
	// cell, so the whole execution is one bucket. Non-partitioned (macro/world-build graph spanning
	// the world): quantize each point into FallbackGridSize cells — every cell must be its own
	// record so it can activate independently.
	APCGPartitionActor* PartitionActor = Cast<APCGPartitionActor>(SourceComponent->GetOwner());

	// SourceId disambiguates same-cell keys produced by different source graphs. Hash the ORIGINAL
	// component so the id survives local-component recycling; the component handed to the subsystem
	// below stays the LOCAL one — its cleaned delegate is the per-cell teardown signal.
	const uint32 SourceId = GetTypeHash(FObjectKey(SourceComponent->GetOriginalComponent()));

	const EPGLEncounterChunkSource Source = (PartitionActor && PartitionActor->IsRuntimeGenerated())
		? EPGLEncounterChunkSource::Streamed
		: EPGLEncounterChunkSource::WorldBuild;

	// Overrides bypass the property clamp — guard the divisor before quantizing.
	const int32 GridSize = PartitionActor
		? static_cast<int32>(PartitionActor->GetPCGGridSize())
		: FMath::Max(1, Settings->FallbackGridSize);

	TMap<FIntVector, TArray<FPGLEncounterSpawnPoint>> Buckets;
	if (PartitionActor)
	{
		// Always materialize the cell's bucket: a re-run that now yields ZERO points must still
		// register (empty == unregister), or the chunk would keep stale points until cleanup.
		Buckets.Add(PartitionActor->GetGridCoord());
	}

	int32 TotalPoints = 0;
	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGPointData* PointData = Cast<UPCGPointData>(Input.Data);
		if (!PointData)
		{
			continue;
		}

		PGLRegisterEncounterPointsInternal::FResolvedAttributes Attributes;
		Attributes.Resolve(PointData->Metadata, Settings);

		for (const FPCGPoint& Point : PointData->GetPoints())
		{
			FPGLEncounterSpawnPoint SpawnPoint;
			SpawnPoint.Location = Point.Transform.GetLocation();
			SpawnPoint.BiomeId = Attributes.ReadBiome(Point.MetadataEntry);
			SpawnPoint.Tier = Attributes.ReadTier(Point.MetadataEntry);
			SpawnPoint.ExplicitArchetype = Attributes.ReadArchetype(Point.MetadataEntry);
			SpawnPoint.PackId = Attributes.ReadPackId(Point.MetadataEntry);
			SpawnPoint.Weight = Attributes.ReadWeight(Point.MetadataEntry);

			const FIntVector Cell = PartitionActor
				? PartitionActor->GetGridCoord()
				: FIntVector(
					FMath::FloorToInt32(SpawnPoint.Location.X / GridSize),
					FMath::FloorToInt32(SpawnPoint.Location.Y / GridSize),
					0);

			Buckets.FindOrAdd(Cell).Add(MoveTemp(SpawnPoint));
			++TotalPoints;
		}
	}

	const int32 NumChunks = Buckets.Num();
	for (TPair<FIntVector, TArray<FPGLEncounterSpawnPoint>>& Bucket : Buckets)
	{
		FPGLEncounterChunkKey Key;
		Key.Coords = Bucket.Key;
		Key.GridSize = GridSize;
		Key.SourceId = SourceId;

		// Idempotent replace: a shallow refresh re-runs this sink with no intervening cleaned
		// event, and the subsystem swaps that chunk's points wholesale.
		Subsystem->RegisterChunkEncounterPoints(Key, Source, MoveTemp(Bucket.Value), SourceComponent);
	}

	PCGE_LOG(Log, LogOnly, FText::Format(LOCTEXT("RegisterSummary", "Registered {0} encounter points across {1} chunks."), TotalPoints, NumChunks));

	return true;
}

#undef LOCTEXT_NAMESPACE
