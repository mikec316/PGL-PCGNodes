#pragma once

#include "PCGElement.h"
#include "PCGSettings.h"
#include "Data/PCGPointData.h"

#include "PGLExportPointsToPCGAsset.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural))
class UPGLExportPointsToPCGAssetSettings : public UPCGSettings
{
    GENERATED_BODY()

public:
    UPGLExportPointsToPCGAssetSettings(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return FName(TEXT("ExportPointsToPCGAsset")); }
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PGLExportPointsToPCGAssetSettings", "NodeTitle", "Export Points to PCG Asset"); }
    virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PGLExportPointsToPCGAssetSettings", "NodeTooltip", "Exports PCG points to a PCG Asset"); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

    virtual TArray<FPCGPinProperties> InputPinProperties() const override { return Super::DefaultPointInputPinProperties(); }
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
    FString AssetName;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
    FString AssetPath;

protected:
    virtual FPCGElementPtr CreateElement() const override;
};

class FPGLExportPointsToPCGAssetElement : public IPCGElement
{
protected:
    virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override;
    virtual bool IsCacheable(const UPCGSettings* InSettings) const override;
    virtual bool ExecuteInternal(FPCGContext* Context) const override;

private:
    static UPackage* CreateOrUpdatePCGAsset(const FString& AssetName, const FString& AssetPath, const TArray<FPCGTaggedData>& TaggedData);
};