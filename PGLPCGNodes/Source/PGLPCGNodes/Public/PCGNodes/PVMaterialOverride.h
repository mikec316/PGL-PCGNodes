// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PVMaterialOverride.generated.h"

/**
 * Post-spawn node that finds AGeometryCollectionActor instances in the world
 * (filtered by an optional actor tag) and calls SetMaterial(SlotIndex, Material)
 * on their UGeometryCollectionComponent.
 *
 * Intended use: place DOWNSTREAM of PVE's output node in the PCG graph so that
 * PVE has already spawned the GC actors before this node executes.  The node
 * passes all input data straight through unchanged to keep the data flow intact.
 *
 * Typical setup with TrunkTips:
 *   1.  Add a custom actor tag (e.g. "MyTree") to PVE's output node settings.
 *   2.  Set CapMaterial = your cross-section material asset.
 *   3.  Set MaterialSlotIndex = 1  (the extra slot created by TrunkTips).
 *   4.  Set ActorTag = "MyTree".
 *   5.  Wire this node downstream so it executes after PVE's spawner.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PGLPCGNODES_API UPVMaterialOverrideSettings : public UPCGSettings
{
	GENERATED_BODY()

public:

#if WITH_EDITOR
	virtual FName            GetDefaultNodeName()  const override { return FName(TEXT("MaterialOverride")); }
	virtual FText            GetDefaultNodeTitle() const override;
	virtual FText            GetNodeTooltipText()  const override;
	virtual EPCGSettingsType GetType()             const override { return EPCGSettingsType::Spatial; }
	virtual FLinearColor     GetNodeTitleColor()   const override { return FLinearColor(FColor::Orange); }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties()  const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr            CreateElement()       const override;

public:

	/**
	 * Material to assign to MaterialSlotIndex on every matching
	 * GeometryCollectionComponent.  If null the node logs a warning and exits.
	 */
	UPROPERTY(EditAnywhere, Category = "Material Override",
		meta = (Tooltip = "Material to assign to MaterialSlotIndex on every matching GeometryCollectionComponent. Must not be null."))
	TSoftObjectPtr<UMaterialInterface> CapMaterial;

	/**
	 * Index of the component material slot to overwrite.
	 * Default 1 targets the extra cap slot created by the Trunk Tips node.
	 * Slot 0 is the bark material managed by PVE — overwriting it affects all bark.
	 */
	UPROPERTY(EditAnywhere, Category = "Material Override",
		meta = (ClampMin = 0, UIMin = 0, UIMax = 8,
			Tooltip = "Material slot index to overwrite on the GeometryCollectionComponent.\nDefault 1 = the cap slot created by the Trunk Tips node."))
	int32 MaterialSlotIndex = 1;

	/**
	 * Only AGeometryCollectionActors that carry this actor tag are affected.
	 * Leave empty to affect EVERY GeometryCollectionActor in the world
	 * (a warning is logged when no tag is set).
	 */
	UPROPERTY(EditAnywhere, Category = "Material Override",
		meta = (Tooltip = "Actor tag filter. Only GC actors with this tag are affected. Leave empty to target all GC actors in the world (use with caution)."))
	FName ActorTag;
};


class PGLPCGNODES_API FPVMaterialOverrideElement : public IPCGElement
{
public:
	/** Must run on the game thread — UActorComponent API is not thread-safe. */
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	/** World state is read at runtime; cached results would be stale after re-generation. */
	virtual bool IsCacheable(const UPCGSettings* InSettings)     const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
