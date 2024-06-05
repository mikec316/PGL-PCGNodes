#include "PGLPCGNode_SumAttributes.h"
#include "Data/PCGPointData.h"
#include "PCGPoint.h"
#include "PCGContext.h"
#include "PCGManagedResource.h"
#include "PCGPin.h"
#include "PCGComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PGLPCGNode_SumAttributes)

#define LOCTEXT_NAMESPACE "PGLSumAttributesNode"

UPGLSumAttributesNodeSettings::UPGLSumAttributesNodeSettings(const FObjectInitializer& ObjectInitializer)
    : UPCGSettings(ObjectInitializer)
{
}

TArray<FPCGPinProperties> UPGLSumAttributesNodeSettings::OutputPinProperties() const
{
    TArray<FPCGPinProperties> PinProperties;
    PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);

    return PinProperties;
}

FPCGElementPtr UPGLSumAttributesNodeSettings::CreateElement() const
{
    return MakeShared<FPGLSumAttributesNodeElement>();
}

bool FPGLSumAttributesNodeElement::CanExecuteOnlyOnMainThread(FPCGContext* Context) const
{
    return false;
}

bool FPGLSumAttributesNodeElement::IsCacheable(const UPCGSettings* InSettings) const
{
    return true;
}

bool FPGLSumAttributesNodeElement::ExecuteInternal(FPCGContext* Context) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(FPGLSumAttributesNodeElement::Execute);

    const UPGLSumAttributesNodeSettings* Settings = Context->GetInputSettings<UPGLSumAttributesNodeSettings>();
    check(Settings);

    FString OutputAttributeName = Settings->OutputAttribute;
    FString AttributeList = Settings->AttributeList;

    TArray<FString> AttributeNames;
    AttributeList.ParseIntoArray(AttributeNames, TEXT(","), true);

    TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
    TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

    for (const FPCGTaggedData& Input : Inputs)
    {
        const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);
        if (!SpatialData)
        {
            PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInputData", "Invalid input data"));
            continue;
        }

        UPCGPointData* PointData = const_cast<UPCGPointData*>(Cast<UPCGPointData>(SpatialData->ToPointData(Context)));
        if (!PointData)
        {
            PCGE_LOG(Error, GraphAndLog, LOCTEXT("UnableToGetPointData", "Unable to get point data from input"));
            continue;
        }

        TArray<FPCGPoint>& Points = PointData->GetMutablePoints();
        if (Points.Num() == 0)
        {
            PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoPointsFound", "No points found in the input data"));
            continue;
        }

        // Initialize new point data and metadata
        UPCGPointData* NewPointData = NewObject<UPCGPointData>(Context->SourceComponent->GetWorld());
        NewPointData->InitializeFromData(PointData, PointData->ConstMetadata());
        UPCGMetadata* MutableMetadata = NewPointData->MutableMetadata();
        TArray<FPCGPoint>& NewPoints = NewPointData->GetMutablePoints();
        NewPoints = Points;

        MutableMetadata->CreateDoubleAttribute(FName(*OutputAttributeName), 0.0, true, true);
        FPCGMetadataAttribute<double>* OutputAttribute = MutableMetadata->GetMutableTypedAttribute<double>(FName(*OutputAttributeName));
        if (!OutputAttribute)
        {
            PCGE_LOG(Error, GraphAndLog, LOCTEXT("NullAttribute", "Failed to get mutable double attribute"));
            continue;
        }

        for (FPCGPoint& Point : NewPoints)
        {
            double Sum = 0.0;
            const UPCGMetadata* Metadata = PointData->ConstMetadata();
            for (const FString& AttributeName : AttributeNames)
            {
                const FPCGMetadataAttributeBase* AttributeBase = Metadata->GetConstAttribute(FName(*AttributeName));
                if (AttributeBase)
                {
                    if (AttributeBase->GetTypeId() == PCG::Private::MetadataTypes<double>::Id)
                    {
                        const FPCGMetadataAttribute<double>* Attribute = static_cast<const FPCGMetadataAttribute<double>*>(AttributeBase);
                        if (Attribute->GetValueFromItemKey(Point.MetadataEntry) != PCGInvalidEntryKey)
                        {
                            Sum += Attribute->GetValue(Point.MetadataEntry);
                        }
                        else
                        {
                            PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("InvalidAttributeKey", "Invalid attribute key for attribute: {0}"), FText::FromString(AttributeName)));
                        }
                    }
                    else
                    {
                        PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("InvalidAttributeType", "Attribute {0} is not of type double"), FText::FromString(AttributeName)));
                    }
                }
                else
                {
                    PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("AttributeNotFound", "Attribute {0} not found"), FText::FromString(AttributeName)));
                }
            }
            OutputAttribute->SetValue(Point.MetadataEntry, Sum);
        }
        NewPointData->GetMutablePoints() = NewPoints; // Use the updated points directly
        FPCGTaggedData& Output = Outputs.Emplace_GetRef();
        Output.Data = NewPointData;
    }

    return true;
}

#undef LOCTEXT_NAMESPACE