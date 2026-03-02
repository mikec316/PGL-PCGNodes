// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGData.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGBoundsToSplines.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural))
class UPCGBoundsToSplinesSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGBoundsToSplinesSettings(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("BoundsToSplines")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGBoundsToSplinesSettings", "NodeTitle", "Bounds to Splines"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGBoundsToSplinesSettings", "NodeTooltip", "Generates six splines from a bounding box."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual EPCGDataType GetCurrentPinTypes(const UPCGPin* InPin) const override;

	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

class  FPCGBoundsToSplinesElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

