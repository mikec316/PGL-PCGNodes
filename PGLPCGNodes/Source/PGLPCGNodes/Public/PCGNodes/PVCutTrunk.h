// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGNodes/PGLBaseSettings.h"
#include "PVCutTrunk.generated.h"

/**
 * PVE-compatible node that cuts the trunk to a [TrunkStart, CutLength] range
 * measured in LengthFromRoot units from the root.
 *
 * Trunk points outside the range are removed, along with any branches that
 * were attached to those portions (and their full sub-trees).
 *
 * Original behaviour (TrunkStart = 0):
 *   Keeps the bottom portion of the trunk up to CutLength — identical to the
 *   previous single-cut behaviour.
 *
 * Range behaviour (TrunkStart > 0):
 *   Keeps only the segment [TrunkStart, CutLength], producing a floating
 *   trunk chunk useful for gameplay interactions (e.g. chopping mechanics).
 *   Use the Trunk Tips node with bAddBottomCap to seal both cut faces.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PGLPCGNODES_API UPVCutTrunkSettings : public UPGLBaseSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("CutTrunk")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor::Yellow; }
#endif

	virtual FPCGElementPtr CreateElement() const override;

public:
	/**
	 * Distance from root at which the BOTTOM cut is made (LengthFromRoot units).
	 * Trunk points BELOW this distance are removed.
	 *
	 * Default 0 = no bottom cut (original behaviour: trunk kept from root upward).
	 * Set > 0 together with CutLength to isolate a mid-trunk chunk.
	 */
	UPROPERTY(EditAnywhere, Category = Settings,
		meta = (ClampMin = 0.0f, UIMin = 0.0f,
			Tooltip = "Distance from root where the bottom cut is made.\n0 = no bottom cut (original behaviour).\nSet > 0 to remove the trunk below this point and produce a floating chunk."))
	float TrunkStart = 0.0f;

	/**
	 * Distance from root at which the TOP cut is made (LengthFromRoot units).
	 * Trunk points ABOVE this distance are removed, along with any branches
	 * attached to that portion (and their full sub-trees).
	 */
	UPROPERTY(EditAnywhere, Category = Settings,
		meta = (ClampMin = 0.0f, UIMin = 0.0f,
			Tooltip = "How far from the root to keep the trunk (LengthFromRoot units).\nTrunk points above this distance are removed together with their sub-trees."))
	float CutLength = 200.0f;
};

class PGLPCGNODES_API FPVCutTrunkElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
