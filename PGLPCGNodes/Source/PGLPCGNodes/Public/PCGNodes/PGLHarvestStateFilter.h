// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "Harvest/PGLHarvestTypes.h" // FPGLHarvestSnapshot, EPGLHarvestState, PGLHarvest key utils

#include "PGLHarvestStateFilter.generated.h"

class UPGLHarvestableSubsystem;

namespace PGLHarvestStateFilterConstants
{
	const FName OutputLabel = TEXT("Out");
	const FName StumpedLabel = TEXT("Stumped");
	const FName DiscardedLabel = TEXT("Discarded");
}

/**
 * Re-applies the harvest registry (UPGLHarvestableSubsystem) to foliage points DURING generation —
 * the node that makes a chopped tree stay chopped through runtime-PCG cell regen and voxel-edit
 * regens (P2 interactive foliage).
 *
 * Place it AFTER points have their final world positions (post Voxel Sampler / transforms) and
 * BEFORE the spawners. Per point it hashes the quantized position (PGLHarvest::MakeKey; +-1 Z-cell
 * candidates accepted only within one Z cell of the registry's stored Z, so stacked cave/surface
 * plants never alias) into the registry snapshot and routes:
 *   - Destroyed                  -> Discarded (culled; tombstoned for the run)
 *   - Stumped, overlay-owned     -> Discarded (the runtime overlay manager already shows the stump;
 *                                   spawning would duplicate it)
 *   - Stumped, NOT overlay-owned -> "Stumped" pin (wire to the stump-mesh spawner — same mesh as
 *                                   the row's OverlayMesh)
 *   - otherwise                  -> "Out" pin, with PGLStripped = 1.0 on Stripped bushes
 *
 * Attributes written: PGLStripped (float 0/1 — pack into per-instance custom data float 0 via an
 * InstanceDataPackerByAttribute so the berry material masks the berries) and optionally
 * PGLHarvestKey (int64, debug).
 *
 * Typical setup:
 *   1. Voxel Sampler -> density/spacing filtering -> THIS NODE -> Static Mesh Spawner(s).
 *   2. Tag every spawner descriptor "PGLHarvest.<RowName>" (ComponentTags) and set
 *      NumCustomDataFloats = 4 (berry mask + hit wobble channels).
 *
 * Threading: the registry snapshot is grabbed on the game thread in PrepareData; Execute runs fully
 * async against that immutable snapshot. Non-cacheable by design — its output depends on mutable
 * game state, and runtime cell regen must re-evaluate it.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PGLPCGNODES_API UPGLHarvestStateFilterSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("PGLHarvestStateFilter")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PGLHarvestStateFilterSettings", "NodeTitle", "PGL Harvest State Filter"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Filter; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** Also write the registry key as an int64 PGLHarvestKey attribute (debugging / advanced wiring). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	bool bWriteKeyAttribute = false;
};

struct PGLPCGNODES_API FPGLHarvestStateFilterContext : public FPCGContext
{
	/** Immutable registry view grabbed on the game thread (PrepareData); read lock-free in Execute. */
	TSharedPtr<const FPGLHarvestSnapshot, ESPMode::ThreadSafe> Snapshot;
	double CellSizeXY = 50.0;
	double CellSizeZ = 400.0;
};

class PGLPCGNODES_API FPGLHarvestStateFilterElement : public IPCGElement
{
public:
	/** Main thread only while grabbing the snapshot (PrepareData); Execute is fully async. */
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override;
	/** Output depends on mutable game state — every (re)generation must re-evaluate. */
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual FPCGContext* CreateContext() override;
	virtual bool PrepareDataInternal(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
	virtual EPCGElementExecutionLoopMode ExecutionLoopMode(const UPCGSettings* Settings) const override { return EPCGElementExecutionLoopMode::SinglePrimaryPin; }
	virtual bool SupportsBasePointDataInputs(FPCGContext* InContext) const override { return true; }
};
