#include "PGLExportToFoliage.h"

#include "PCGCommon.h"
#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGManagedResource.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "InstancedFoliageActor.h"
#include "FoliageType.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "FoliageInstancedStaticMeshComponent.h"  // For FFoliageInstance
#include "UObject/SoftObjectPath.h"

#define LOCTEXT_NAMESPACE "PGLExportToFoliage"

UPGLExportToFoliageSettings::UPGLExportToFoliageSettings(const FObjectInitializer& ObjectInitializer)
    : UPCGSettings(ObjectInitializer)
{
}

#if WITH_EDITOR
FText UPGLExportToFoliageSettings::GetNodeTooltipText() const
{
    return LOCTEXT("ExportToFoliageTooltip", "Adds Points to InstancedFoliageActor");
}
#endif

TArray<FPCGPinProperties> UPGLExportToFoliageSettings::OutputPinProperties() const
{
    TArray<FPCGPinProperties> PinProperties;
    PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);

    return PinProperties;
}

FPCGElementPtr UPGLExportToFoliageSettings::CreateElement() const
{
    return MakeShared<FPGLExportToFoliageElement>();
}

bool FPGLExportToFoliageElement::CanExecuteOnlyOnMainThread(FPCGContext* Context) const
{
    return false;
}

bool FPGLExportToFoliageElement::IsCacheable(const UPCGSettings* InSettings) const
{
    return true;
}

bool FPGLExportToFoliageElement::ExecuteInternal(FPCGContext* Context) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(FPGLExportToFoliageElement::Execute);

    const UPGLExportToFoliageSettings* Settings = Context->GetInputSettings<UPGLExportToFoliageSettings>();
    check(Settings);

    TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
    TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

    TMap<FString, TArray<FFoliageInstance>> FoliageInstancesMap;
    TArray<FPCGTaggedData> PointOutputs;

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

        const UPCGMetadata* Metadata = PointData->ConstMetadata();
        const FPCGMetadataAttributeBase* AttributeBase = Metadata->GetConstAttribute(*Settings->FoliageTypeAttributeName);

        if (!AttributeBase)
        {
            PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("AttributeNotFound", "Attribute '{0}' not found in metadata"), FText::FromString(Settings->FoliageTypeAttributeName)));
            continue;
        }

        if (AttributeBase->GetTypeId() != PCG::Private::MetadataTypes<FString>::Id)
        {
            PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InvalidAttributeType", "Attribute '{0}' is not of type FString"), FText::FromString(Settings->FoliageTypeAttributeName)));
            continue;
        }

        const FPCGMetadataAttribute<FString>* Attribute = static_cast<const FPCGMetadataAttribute<FString>*>(AttributeBase);

        const TArray<FPCGPoint>& Points = PointData->GetPoints();
        for (const FPCGPoint& Point : Points)
        {
            FString FoliageTypePath = Attribute->GetValueFromItemKey(Point.MetadataEntry);
            if (FoliageTypePath.IsEmpty())
            {
                PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("EmptyFoliageTypePath", "Foliage type path is empty for point at location {0}"), FText::FromString(Point.Transform.GetLocation().ToString())));
                continue;
            }

            FFoliageInstance FoliageInstance;
            FoliageInstance.Location = Point.Transform.GetLocation();
            FoliageInstance.Rotation = Point.Transform.GetRotation().Rotator();
            FoliageInstance.DrawScale3D = FVector3f(Point.Transform.GetScale3D());

            FoliageInstancesMap.FindOrAdd(FoliageTypePath).Add(FoliageInstance);
        }

        // Add the point data to the outputs
        FPCGTaggedData& PointOutput = PointOutputs.Emplace_GetRef();
        PointOutput.Data = PointData;
        PointOutput.Pin = Input.Pin;
    }

#if WITH_EDITOR
    UObject* WorldContextObject = GEngine->GetWorldFromContextObjectChecked(Context->SourceComponent.Get());

    if (WorldContextObject)
    {
        UWorld* World = WorldContextObject->GetWorld();
        if (World)
        {
            AInstancedFoliageActor* FoliageActor = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(World, true);
            if (FoliageActor)
            {
                for (const auto& Pair : FoliageInstancesMap)
                {
                    const FString& FoliageTypePath = Pair.Key;
                    const TArray<FFoliageInstance>& FoliageInstances = Pair.Value;

                    TArray<const FFoliageInstance*> FoliageInstancePtrs;
                    FoliageInstancePtrs.Reserve(FoliageInstances.Num());
                    for (const FFoliageInstance& Instance : FoliageInstances)
                    {
                        FoliageInstancePtrs.Add(&Instance);
                    }

                    UFoliageType* FoliageType = Cast<UFoliageType>(StaticLoadObject(UFoliageType::StaticClass(), nullptr, *FoliageTypePath));
                    if (FoliageType)
                    {
                        FFoliageInfo* FoliageInfo = nullptr;
                        UFoliageType* NewFoliageType = FoliageActor->AddFoliageType(FoliageType, &FoliageInfo);
                        if (FoliageInfo)
                        {
                            FoliageInfo->AddInstances(NewFoliageType, FoliageInstancePtrs);
                        }
                    }
                    else
                    {
                        PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("FoliageTypeNotFound", "Foliage type '{0}' could not be loaded"), FText::FromString(FoliageTypePath)));
                    }
                }
            }
        }
    }
#endif

    // Pass-through settings & exclusions
    Context->OutputData.TaggedData.Append(PointOutputs);
    Context->OutputData.TaggedData.Append(Context->InputData.GetAllSettings());

    return true;
}

#undef LOCTEXT_NAMESPACE