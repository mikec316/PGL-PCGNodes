#pragma once

#include "PCGElement.h"
#include "PCGSettings.h"
#include "Elements/PCGPointOperationElementBase.h"
#include "PGLPCGNode_SumAttributes.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural))
class UPGLSumAttributesNodeSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPGLSumAttributesNodeSettings(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("PGLSumAttributesNode")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PGLSumAttributesNodeSettings", "NodeTitle", "Sum Attributes Node"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PGLSumAttributesNodeSettings", "NodeTooltip", "Sums specified double attributes and stores the result in a new attribute"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return Super::DefaultPointInputPinProperties(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
	FString OutputAttribute;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
	FString AttributeList;

protected:
	virtual FPCGElementPtr CreateElement() const override;
};

class FPGLSumAttributesNodeElement : public IPCGElement
{
protected:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override;
};
