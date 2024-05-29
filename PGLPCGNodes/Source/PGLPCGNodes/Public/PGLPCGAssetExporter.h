#pragma once

#include "PCGAssetExporter.h"
#include "PGLPCGAssetExporter.generated.h"

UCLASS()
class UPGLPCGAssetExporter : public UPCGAssetExporter
{
    GENERATED_BODY()

public:
    TArray<FPCGTaggedData> TaggedData;

protected:
    virtual bool BP_ExportToAsset_Implementation(UPCGDataAsset* Asset) override;
};