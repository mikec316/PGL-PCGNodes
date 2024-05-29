#include "PGLPCGAssetExporter.h"
#include "Data/PCGPointData.h"
#include "PCGDataAsset.h"

bool UPGLPCGAssetExporter::BP_ExportToAsset_Implementation(UPCGDataAsset* Asset)
{
    if (!Asset)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid PCGDataAsset"));
        return false;
    }

    FPCGDataCollection& DataCollection = Asset->Data;
    DataCollection.TaggedData.Reset();

    // Collect tagged data from the context and populate the asset
    for (const FPCGTaggedData& Data : TaggedData)
    {
        const UPCGPointData* PointData = Cast<UPCGPointData>(Data.Data);
        if (PointData)
        {
            // Create new UPCGPointData and copy points
            UPCGPointData* NewPointData = NewObject<UPCGPointData>(Asset);
            NewPointData->GetMutablePoints() = PointData->GetPoints();

            // Copy metadata if available
            if (PointData->Metadata)
            {
                NewPointData->MutableMetadata()->Initialize(PointData->Metadata);
            }

            // Add new tagged data entry and copy the tags
            FPCGTaggedData& TaggedDataEntry = DataCollection.TaggedData.Emplace_GetRef();
            TaggedDataEntry.Data = NewPointData;
            NewPointData->Flatten();
            TaggedDataEntry.Tags = Data.Tags; // Ensure tags are copied
        }
    }

    Asset->MarkPackageDirty();
    return true;
}