// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"

#include "PGLRemoveDuplicatePoints.generated.h"

namespace PGLRemoveDuplicatePointsConstants
{
	const FName SourceLabel = TEXT("Source");
	const FName RemoveLabel = TEXT("Remove");
}

/**
 * Takes two sets of points (Source and Remove) and removes any points from
 * Source that are at identical (within tolerance) locations to points in Remove.
 *
 * Useful for subtracting foliage or placed instances from a point cloud.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PGLPCGNODES_API UPGLRemoveDuplicatePointsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("PGLRemoveDuplicatePoints")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Filter; }
	virtual FText GetNodeTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor(0.6f, 0.2f, 0.2f); }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;

public:
	/** Distance tolerance for considering two points as duplicates. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0.0"))
	double Tolerance = 1.0;
};

class PGLPCGNODES_API FPGLRemoveDuplicatePointsElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
