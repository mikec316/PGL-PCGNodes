// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "PGLVoxelGraphCollection.h"

#include "PGLStagingVoxelStamp.generated.h"

class UPCGManagedVoxelInstancedStampComponent;

namespace PGLStagingVoxelStampConstants
{
	const FName InputPointsLabel = TEXT("In");
	const FName InputMapLabel = TEXT("Map");
}

/**
 * Consumes VoxelGraph-typed collection entries from the staging pipeline and
 * spawns voxel stamps (height or volume graph stamps) at each point location.
 *
 * Inputs:
 *   In  - Points with PCGEx/CollectionEntry (int64 hash) attribute
 *   Map - Collection map (param data) from staging nodes
 *
 * For each point, resolves the collection entry hash to a FPGLVoxelGraphCollectionEntry,
 * extracts the VoxelGraph reference, parameter overrides, and stamp properties,
 * then creates instanced stamps via UVoxelInstancedStampComponent.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PGLPCGNODES_API UPGLStagingVoxelStampSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("PGLStagingVoxelStamp")); }
	virtual FText GetDefaultNodeTitle() const override { return INVTEXT("PGL Staging Voxel Stamp"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
	virtual FText GetNodeTooltipText() const override;
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;

public:
	/**
	 * Default stamp properties used when entries don't have bOverrideStampProperties enabled.
	 * The StampType here determines whether stamps are spawned as Height or Volume graph stamps.
	 */
	UPROPERTY(EditAnywhere, Category = "Stamp Properties")
	FPGLVoxelStampProperties DefaultStampProperties;

	/** Target actor to attach the stamp component to. If unset, uses the PCG component's owner. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TSoftObjectPtr<AActor> TargetActor;

	/**
	 * Functions to call on the target actor after stamps are spawned.
	 * Functions must be parameterless with the "CallInEditor" flag enabled.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FName> PostProcessFunctionNames;
};

struct PGLPCGNODES_API FPGLStagingVoxelStampContext : public FPCGContext
{
	bool bReuseCheckDone = false;
	bool bSkippedDueToReuse = false;
};

class PGLPCGNODES_API FPGLStagingVoxelStampElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual FPCGContext* CreateContext() override;
	virtual bool PrepareDataInternal(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
#if VOXEL_ENGINE_VERSION >= 506
	virtual bool SupportsBasePointDataInputs(FPCGContext* InContext) const override { return true; }
#endif

private:
	static UPCGManagedVoxelInstancedStampComponent* GetOrCreateManagedComponent(
		AActor* InTargetActor,
		UPCGComponent* InSourceComponent,
		uint64 SettingsUID);
};
