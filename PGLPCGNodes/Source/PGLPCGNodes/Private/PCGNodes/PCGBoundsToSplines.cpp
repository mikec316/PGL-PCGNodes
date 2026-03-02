// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PCGBoundsToSplines.h"
#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSplineData.h"
#include "Engine/World.h"

UPCGBoundsToSplinesSettings::UPCGBoundsToSplinesSettings(const FObjectInitializer& ObjectInitializer)
: UPCGSettings(ObjectInitializer)
{
}

EPCGDataType UPCGBoundsToSplinesSettings::GetCurrentPinTypes(const UPCGPin* InPin) const
{
check(InPin);
    
if (InPin->IsOutputPin())
{

    return EPCGDataType::Spline;
    
}

return Super::GetCurrentPinTypes(InPin);
}

TArray<FPCGPinProperties> UPCGBoundsToSplinesSettings::OutputPinProperties() const
{
TArray<FPCGPinProperties> PinProperties;
PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Spatial);
return PinProperties;
}

FPCGElementPtr UPCGBoundsToSplinesSettings::CreateElement() const
{
return MakeShared<FPCGBoundsToSplinesElement>();
}

bool FPCGBoundsToSplinesElement::CanExecuteOnlyOnMainThread(FPCGContext* Context) const
{
return true;
}

bool FPCGBoundsToSplinesElement::IsCacheable(const UPCGSettings* InSettings) const
{
return false;
}

bool FPCGBoundsToSplinesElement::ExecuteInternal(FPCGContext* Context) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(FPCGBoundsToSplinesElement::Execute);
    
    const UPCGBoundsToSplinesSettings* Settings = Context->GetInputSettings<UPCGBoundsToSplinesSettings>();
    check(Settings);
    TArray<FPCGTaggedData> Outputs;
    FPCGTaggedData FloorOutputs;
    FPCGTaggedData CeilingOutputs;
    FPCGTaggedData Corner1Outputs;
    FPCGTaggedData Corner2Outputs;
    FPCGTaggedData Corner3Outputs;
    FPCGTaggedData Corner4Outputs;
    FString Floor = "Floor";
    FString Ceiling = "Ceiling";
    FString Corner1 = "Corner1";
    FString Corner2 = "Corner2";
    FString Corner3 = "Corner3";
    FString Corner4 = "Corner4";
    TSet<FString> FloorTag;
    TSet<FString> CeilingTag;
    TSet<FString> Corner1Tag;
    TSet<FString> Corner2Tag;
    TSet<FString> Corner3Tag;
    TSet<FString> Corner4Tag;
    CeilingTag.Add(Ceiling);
    FloorTag.Add(Floor);
    Corner1Tag.Add(Corner1);
    Corner2Tag.Add(Corner2);
    Corner3Tag.Add(Corner3);
    Corner4Tag.Add(Corner4);




    TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
    for (const FPCGTaggedData& Input : Inputs)
    {
        const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);
        if (!SpatialData)
        {
            continue;
        }
        const UPCGPointData* PointData = SpatialData->ToPointData(Context);
        const TArray<FPCGPoint>& Points = PointData->GetPoints();
    
        for (const FPCGPoint& Point : Points)
        {
            FTransform PointTransform = Point.Transform;
            FVector Min = Point.BoundsMin;
            FVector Max = Point.BoundsMax;

            FVector BottomLeftFront = PointTransform.TransformPosition(FVector(Min.X, Min.Y, Min.Z));
            FVector BottomRightFront = PointTransform.TransformPosition(FVector(Max.X, Min.Y, Min.Z));
            FVector BottomLeftBack = PointTransform.TransformPosition(FVector(Min.X, Max.Y, Min.Z));
            FVector BottomRightBack = PointTransform.TransformPosition(FVector(Max.X, Max.Y, Min.Z));

            FVector TopLeftFront = PointTransform.TransformPosition(FVector(Min.X, Min.Y, Max.Z));
            FVector TopRightFront = PointTransform.TransformPosition(FVector(Max.X, Min.Y, Max.Z));
            FVector TopLeftBack = PointTransform.TransformPosition(FVector(Min.X, Max.Y, Max.Z));
            FVector TopRightBack = PointTransform.TransformPosition(FVector(Max.X, Max.Y, Max.Z));

            TArray<FSplinePoint> FloorSplinePoints = {
                FSplinePoint(0, BottomLeftFront,ESplinePointType::Linear),
                FSplinePoint(1, BottomRightFront,ESplinePointType::Linear),
                FSplinePoint(2, BottomRightBack,ESplinePointType::Linear),
                FSplinePoint(3, BottomLeftBack,ESplinePointType::Linear),
                FSplinePoint(4, BottomLeftFront,ESplinePointType::Linear)
            };

            TArray<FSplinePoint> CeilingSplinePoints = {
                FSplinePoint(0, TopLeftFront,ESplinePointType::Linear),
                FSplinePoint(1, TopRightFront,ESplinePointType::Linear),
                FSplinePoint(2, TopRightBack,ESplinePointType::Linear),
                FSplinePoint(3, TopLeftBack,ESplinePointType::Linear),
                FSplinePoint(4, TopLeftFront,ESplinePointType::Linear)
            };

            TArray<FSplinePoint> CornerSplinePoints1 = {
                FSplinePoint(0, BottomLeftFront,ESplinePointType::Linear),
                FSplinePoint(1, TopLeftFront,ESplinePointType::Linear)
            };

            TArray<FSplinePoint> CornerSplinePoints2 = {
                FSplinePoint(0, BottomRightFront,ESplinePointType::Linear),
                FSplinePoint(1, TopRightFront,ESplinePointType::Linear)
            };

            TArray<FSplinePoint> CornerSplinePoints3 = {
                FSplinePoint(0, BottomLeftBack,ESplinePointType::Linear),
                FSplinePoint(1, TopLeftBack,ESplinePointType::Linear)
            };

            TArray<FSplinePoint> CornerSplinePoints4 = {
                FSplinePoint(0, BottomRightBack,ESplinePointType::Linear),
                FSplinePoint(1, TopRightBack,ESplinePointType::Linear)
            };

            auto CreateSplineData = [&Point](const TArray<FSplinePoint>& Points) -> UPCGSplineData* {
                UPCGSplineData* SplineData = NewObject<UPCGSplineData>();
                SplineData->Initialize(Points, false, FTransform::Identity);

                UPCGMetadata* Metadata = SplineData->MutableMetadata();

                    const FName AttributeName(TEXT("ParentPoint"));
                    Metadata->CreateTransformAttribute(AttributeName, Point.Transform,true,true);
                
            
                return SplineData;
            };
            
            FloorOutputs = (FPCGTaggedData(CreateSplineData(FloorSplinePoints)));
            FloorOutputs.Tags.Append(FloorTag);
            CeilingOutputs = (FPCGTaggedData(CreateSplineData(CeilingSplinePoints)));
            CeilingOutputs.Tags.Append(CeilingTag);
            Corner1Outputs = (FPCGTaggedData(CreateSplineData(CornerSplinePoints1)));
            Corner1Outputs.Tags.Append(Corner1Tag);
            Corner2Outputs = (FPCGTaggedData(CreateSplineData(CornerSplinePoints2)));
            Corner2Outputs.Tags.Append(Corner2Tag);
            Corner3Outputs = (FPCGTaggedData(CreateSplineData(CornerSplinePoints3)));
            Corner3Outputs.Tags.Append(Corner3Tag);
            Corner4Outputs = (FPCGTaggedData(CreateSplineData(CornerSplinePoints4)));
            Corner4Outputs.Tags.Append(Corner4Tag);


            Outputs.Add(FloorOutputs);
            Outputs.Add(CeilingOutputs);
            Outputs.Add(Corner1Outputs);
            Outputs.Add(Corner2Outputs);
            Outputs.Add(Corner3Outputs);
            Outputs.Add(Corner4Outputs);
            
        }
    }

    Context->OutputData.TaggedData.Append(Outputs);

    
    return true;
}