// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"

#include "PGLRegisterEncounterPoints.generated.h"

/**
 * Pass-through sink that registers its input points as encounter CANDIDATES with
 * UPGLEncounterSubsystem during generation. PCG never spawns an enemy here: it only nominates
 * locations (+ biome/tier/pack metadata); the subsystem owns the pawn pool, the spawn budget and
 * proximity activation, so a graph re-run can never directly spawn — or leak — pawns.
 *
 * Chunk identity: under partitioned generation the owning APCGPartitionActor IS the cell (one
 * registration per execution, Streamed when runtime-generated). A non-partitioned macro/world-build
 * graph emits points across many cells, so those are bucketed per point into FallbackGridSize cells
 * instead — each cell must stay its own record for independent activation. Keys carry a SourceId
 * hashed from the ORIGINAL component, so two source graphs covering the same cell never clobber
 * each other's registrations.
 *
 * Re-execution REPLACES the chunk's previous registration (a PCG shallow refresh re-runs the sink
 * with no intervening cleaned event), including replace-with-empty when a regenerated cell no
 * longer yields points. Cell cleanup/recycling teardown is the subsystem's job: it binds the LOCAL
 * component's cleaned delegate at registration.
 *
 * Attributes read (all optional, tolerant of FName/FString/int storage — different graphs author
 * BiomeID differently): BiomeAttribute (-> UPGLBiomeEncounterTable row, missing -> default row),
 * TierAttribute ("Elite"/1 -> Elite, anything else/missing -> Fodder), ArchetypeAttribute (explicit
 * soft path — stored, never loaded here; the subsystem loads at activation), PackIdAttribute
 * (points sharing an id activate/clear together), WeightAttribute (budget prioritization).
 *
 * Non-cacheable + main-thread by design: the element's entire purpose is a game-state side effect
 * that must re-run on every cell (re)generation, and the subsystem/partition-actor access is
 * game-thread only (the executor always runs main-thread-only elements there, including under
 * runtime-gen). Editor preview generation passes points through untouched — only game worlds have
 * a registry to feed.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PGLPCGNODES_API UPGLRegisterEncounterPointsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("PGLRegisterEncounterPoints")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PGLRegisterEncounterPointsSettings", "NodeTitle", "PGL Register Encounter Points"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Generic; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** Biome row key into the encounter table (project convention: ST_BiomeColorMap / PCGEx_Biomes
	 *  emit "BiomeID"). FName/FString/int storage all accepted; missing/unknown -> table default row. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
	FName BiomeAttribute = "BiomeID";

	/** "Elite" (case-insensitive) or integer 1 -> Elite; anything else — including a missing
	 *  attribute — is Fodder, so plain fodder graphs need no tier authoring at all. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
	FName TierAttribute = "EncounterTier";

	/** Optional explicit UPGLEnemyArchetype soft path (overrides the biome-table pick). Stored as a
	 *  path only — registration stays load-free; the subsystem loads at activation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
	FName ArchetypeAttribute = "Archetype";

	/** Points sharing a PackId activate/clear together. Missing -> 0 (the whole chunk is one pack). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
	FName PackIdAttribute = "PackId";

	/** Reserved for budget prioritization. Missing -> 1. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
	FName WeightAttribute = "Weight";

	/** Cell quantization (uu) for NON-partitioned components (macro/world-build graphs): a single
	 *  graph spanning the world must still register one record per cell so each cell activates
	 *  independently. Partitioned components use their partition actor's grid instead. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable, ClampMin = "1"))
	int32 FallbackGridSize = 25600;
};

class PGLPCGNODES_API FPGLRegisterEncounterPointsElement : public IPCGElement
{
public:
	/** Subsystem registration + partition-actor reads are game-thread only; the executor honors
	 *  this for every execution path, including runtime-gen. */
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	/** The whole point is a registry side effect — every cell (re)generation must re-register,
	 *  so a cached pass-through would silently starve the encounter registry. */
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
