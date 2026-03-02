// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGNodes/PGLBaseSettings.h"
#include "PVScaleBranches.generated.h"

/**
 * PVE-compatible node that scales all non-trunk branches uniformly.
 * Radius, position (relative to plant root), and accumulated lengths are multiplied
 * by BranchScale. The trunk is left completely unchanged.
 *
 * Add this node to a PVE graph via the PCG node palette (search "Scale Branches").
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PGLPCGNODES_API UPVScaleBranchesSettings : public UPGLBaseSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("ScaleBranches")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor::Yellow; }
#endif

	virtual FPCGElementPtr CreateElement() const override;

public:
	/** Scale multiplier applied to branch point radius, positions (relative to plant root), and lengths.
	 *  The trunk is not affected. Values below 1 shrink branches; values above 1 grow them. */
	UPROPERTY(EditAnywhere, Category = Settings,
		meta = (ClampMin = 0.01f, ClampMax = 100.0f, UIMin = 0.01f, UIMax = 10.0f,
			Tooltip = "Uniform scale multiplier for all non-trunk branches.\n\nScales branch radius, positions relative to the plant root, and accumulated lengths.\nThe trunk is left completely unchanged."))
	float BranchScale = 1.0f;

	/** When enabled, foliage instances attached to branches are scaled alongside the branches. */
	UPROPERTY(EditAnywhere, Category = Settings,
		meta = (Tooltip = "Also scale foliage instances that are attached to non-trunk branches."))
	bool bScaleFoliage = true;
};

class PGLPCGNODES_API FPVScaleBranchesElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
