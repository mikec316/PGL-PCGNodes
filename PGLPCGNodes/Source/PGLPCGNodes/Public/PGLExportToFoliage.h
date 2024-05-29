#pragma once

#include "PCGElement.h"
#include "PCGSettings.h"
#include "Data/PCGPointData.h"

#include "PGLExportToFoliage.generated.h"

struct FPCGContext;

/** PCG node that processes point data and adds foliage instances */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPGLExportToFoliageSettings : public UPCGSettings
{
    GENERATED_BODY()

public:
    UPGLExportToFoliageSettings(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return FName(TEXT("PGLExportToFoliage")); }
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PGLExportToFoliageSettings", "NodeTitle", "PGLExport To Foliage"); }
    virtual FText GetNodeTooltipText() const override;
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

    virtual TArray<FPCGPinProperties> InputPinProperties() const override { return Super::DefaultPointInputPinProperties(); }
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
    virtual FPCGElementPtr CreateElement() const override;

public:
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
    FString FoliageTypeAttributeName;
};

class FPGLExportToFoliageElement : public IPCGElement
{
protected:
    virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override;
    virtual bool IsCacheable(const UPCGSettings* InSettings) const override;
    virtual bool ExecuteInternal(FPCGContext* Context) const override;
};