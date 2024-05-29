#include "PGLExportPointsToPCGAsset.h"
#include "PGLPCGAssetExporter.h"
#include "Data/PCGPointData.h"
#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGManagedResource.h"
#include "PCGPin.h"
#include "PCGDataAsset.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "PCGAssetExporter.h"
#include "UObject/Package.h"
#include "PCGEditor/Public/PCGAssetExporterUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "FileHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PGLExportPointsToPCGAsset)

#define LOCTEXT_NAMESPACE "PGLExportPointsToPCGAsset"

UPGLExportPointsToPCGAssetSettings::UPGLExportPointsToPCGAssetSettings(const FObjectInitializer& ObjectInitializer)
    : UPCGSettings(ObjectInitializer)
{
}

TArray<FPCGPinProperties> UPGLExportPointsToPCGAssetSettings::OutputPinProperties() const
{
    TArray<FPCGPinProperties> PinProperties;
    PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Spatial);

    return PinProperties;
}

FPCGElementPtr UPGLExportPointsToPCGAssetSettings::CreateElement() const
{
    return MakeShared<FPGLExportPointsToPCGAssetElement>();
}

bool FPGLExportPointsToPCGAssetElement::CanExecuteOnlyOnMainThread(FPCGContext* Context) const
{
    return false;
}

bool FPGLExportPointsToPCGAssetElement::IsCacheable(const UPCGSettings* InSettings) const
{
    return true;
}

bool FPGLExportPointsToPCGAssetElement::ExecuteInternal(FPCGContext* Context) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(FPGLExportPointsToPCGAssetElement::Execute);

    const UPGLExportPointsToPCGAssetSettings* Settings = Context->GetInputSettings<UPGLExportPointsToPCGAssetSettings>();
    check(Settings);

    TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
    TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

    TArray<FPCGTaggedData> TaggedData;

    for (const FPCGTaggedData& Input : Inputs)
    {
        const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);

        if (!SpatialData)
        {
            PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInputData", "Invalid input data"));
            continue;
        }

        const UPCGPointData* PointData = SpatialData->ToPointData(Context);

        if (!PointData)
        {
            PCGE_LOG(Error, GraphAndLog, LOCTEXT("UnableToGetPointData", "Unable to get point data from input"));
            continue;
        }

        // Log number of points in input PointData
        UE_LOG(LogTemp, Verbose, TEXT("Input PointData has %d points"), PointData->GetPoints().Num());

        // Collect tagged data
        FPCGTaggedData NewTaggedData;
        NewTaggedData.Data = PointData;
        NewTaggedData.Tags = Input.Tags; // Ensure tags are copied
        TaggedData.Add(NewTaggedData);

        // Pass-through the input data
        FPCGTaggedData& Output = Outputs.Add_GetRef(Input);
        Output.Data = PointData;
    }

    // Create the PCG asset
    FPCGAssetExporterParameters ExporterParams;
    ExporterParams.AssetName = Settings->AssetName;
    ExporterParams.AssetPath = Settings->AssetPath;
    ExporterParams.bSaveOnExportEnded = true;
    ExporterParams.bOpenSaveDialog = false; // Skip context window

    // Ensure the asset path starts with '/'
    if (!ExporterParams.AssetPath.StartsWith(TEXT("/")))
    {
        ExporterParams.AssetPath = TEXT("/") + ExporterParams.AssetPath;
    }

    // Log asset creation parameters
    UE_LOG(LogTemp, Verbose, TEXT("Creating or updating asset: %s/%s"), *ExporterParams.AssetPath, *ExporterParams.AssetName);

    // Create an instance of the concrete exporter
    UPGLPCGAssetExporter* Exporter = NewObject<UPGLPCGAssetExporter>();
    Exporter->TaggedData = TaggedData; // Assign tagged data to exporter

    UPackage* AssetPackage = UPCGAssetExporterUtils::CreateAsset(Exporter, ExporterParams);
    if (!AssetPackage)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create or update the PCG asset"));
        return false;
    }

    // Find the newly created PCGDataAsset
    UPCGDataAsset* PCGDataAsset = FindObject<UPCGDataAsset>(AssetPackage, *ExporterParams.AssetName);
    if (PCGDataAsset)
    {
        UE_LOG(LogTemp, Verbose, TEXT("PCG asset found and updated"));

        PCGDataAsset->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(PCGDataAsset);

        // Save the package
        if (ExporterParams.bSaveOnExportEnded)
        {
            TArray<UPackage*> PackagesToSave;
            PackagesToSave.Add(AssetPackage);
            FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false, false);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to find the created PCG asset"));
        return false;
    }

    // Pass-through settings & exclusions
    Context->OutputData.TaggedData.Append(Context->InputData.GetAllSettings());

    return true;
}

#undef LOCTEXT_NAMESPACE
