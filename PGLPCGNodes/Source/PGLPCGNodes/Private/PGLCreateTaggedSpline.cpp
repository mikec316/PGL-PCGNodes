#include "PGLCreateTaggedSpline.h"

#include "PCGCommon.h"
#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGManagedResource.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSplineData.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "Math/UnitConversion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PGLCreateTaggedSpline)

#define LOCTEXT_NAMESPACE "PGLCreateTaggedSpline"

UPGLCreateTaggedSplineSettings::UPGLCreateTaggedSplineSettings(const FObjectInitializer& ObjectInitializer)
    : UPCGSettings(ObjectInitializer)
{
    if (PCGHelpers::IsNewObjectAndNotDefault(this))
    {
        AttachOptions = EPCGAttachOptions::InFolder;
    }
}


TArray<FPCGPinProperties> UPGLCreateTaggedSplineSettings::OutputPinProperties() const
{
    TArray<FPCGPinProperties> PinProperties;
    PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Spline);

    return PinProperties;
}

FPCGElementPtr UPGLCreateTaggedSplineSettings::CreateElement() const
{
    return MakeShared<FPGLCreateTaggedSplineElement>();
}

bool FPGLCreateTaggedSplineElement::CanExecuteOnlyOnMainThread(FPCGContext* Context) const
{
    const UPGLCreateTaggedSplineSettings* Settings = Context->GetInputSettings<UPGLCreateTaggedSplineSettings>();
    return Settings && Settings->Mode == EPGLCreateTaggedSplineMode::CreateNewActor;
}

bool FPGLCreateTaggedSplineElement::IsCacheable(const UPCGSettings* InSettings) const
{
    const UPGLCreateTaggedSplineSettings* Settings = Cast<const UPGLCreateTaggedSplineSettings>(InSettings);
    return !Settings || Settings->Mode == EPGLCreateTaggedSplineMode::CreateDataOnly;
}

bool FPGLCreateTaggedSplineElement::ExecuteInternal(FPCGContext* Context) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(FPGLCreateTaggedSplineElement::Execute);

    const UPGLCreateTaggedSplineSettings* Settings = Context->GetInputSettings<UPGLCreateTaggedSplineSettings>();
    check(Settings);

    TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
    TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

    EPGLCreateTaggedSplineMode Mode = Settings->Mode;

    if (Mode == EPGLCreateTaggedSplineMode::CreateNewActor)
    {
        const bool bHasAuthority = !Context->SourceComponent.IsValid() || (Context->SourceComponent->GetOwner() && Context->SourceComponent->GetOwner()->HasAuthority());
        if (!bHasAuthority)
        {
            Mode = EPGLCreateTaggedSplineMode::CreateDataOnly;
        }
    }

    // Parse the "TagsToAdd" string and split it into individual tags
    TArray<FString> TagsToAddArray;

    for (const FPCGTaggedData& Input : Inputs)
    {
        const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);

        for (const FString& Tag : Input.Tags)
        {
            TagsToAddArray.Add(Tag);
        }

        if (!SpatialData)
        {
            PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInputData", "Invalid input data"));
            continue;
        }

        AActor* TargetActor = Settings->TargetActor.Get() ? Settings->TargetActor.Get() : Context->GetTargetActor(nullptr);
        if (!TargetActor)
        {
            PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidTargetActor", "Invalid target actor. Ensure TargetActor member is initialized when creating SpatialData."));
            continue;
        }

        const UPCGPointData* PointData = SpatialData->ToPointData(Context);

        if (!PointData)
        {
            PCGE_LOG(Error, GraphAndLog, LOCTEXT("UnableToGetPointData", "Unable to get point data from input"));
            continue;
        }

        const FPCGMetadataAttribute<FVector>* ArriveTangentAttribute = nullptr;
        const FPCGMetadataAttribute<FVector>* LeaveTangentAttribute = nullptr;

        if (Settings->bApplyCustomTangents)
        {
            const UPCGMetadata* PointMetadata = PointData->ConstMetadata();
            check(PointMetadata);

            FName LocalArriveTangentName = ((Settings->ArriveTangentAttribute == NAME_None) ? PointMetadata->GetLatestAttributeNameOrNone() : Settings->ArriveTangentAttribute);
            FName LocalLeaveTangentName = ((Settings->LeaveTangentAttribute == NAME_None) ? PointMetadata->GetLatestAttributeNameOrNone() : Settings->LeaveTangentAttribute);

            const FText AttributeMissingOrNotVector = LOCTEXT("AttributeMissingOrNotVector", "Attribute '{0}' does not exist or is not a vector");

            const FPCGMetadataAttributeBase* ArriveTangentBaseAttribute = PointMetadata->GetConstAttribute(LocalArriveTangentName);
            if (!ArriveTangentBaseAttribute || ArriveTangentBaseAttribute->GetTypeId() != PCG::Private::MetadataTypes<FVector>::Id)
            {
                PCGE_LOG(Error, GraphAndLog, FText::Format(AttributeMissingOrNotVector, FText::FromString(LocalArriveTangentName.ToString())));
                continue;
            }

            const FPCGMetadataAttributeBase* LeaveTangentBaseAttribute = PointMetadata->GetConstAttribute(LocalLeaveTangentName);
            if (!LeaveTangentBaseAttribute || LeaveTangentBaseAttribute->GetTypeId() != PCG::Private::MetadataTypes<FVector>::Id)
            {
                PCGE_LOG(Error, GraphAndLog, FText::Format(AttributeMissingOrNotVector, FText::FromString(LocalLeaveTangentName.ToString())));
                continue;
            }

            ArriveTangentAttribute = static_cast<const FPCGMetadataAttribute<FVector>*>(ArriveTangentBaseAttribute);
            LeaveTangentAttribute = static_cast<const FPCGMetadataAttribute<FVector>*>(LeaveTangentBaseAttribute);
        }

        UPCGSplineData* SplineData = NewObject<UPCGSplineData>();
        AActor* SplineActor = TargetActor;

        if (Settings->Mode == EPGLCreateTaggedSplineMode::CreateDataOnly)
        {
            // Nothing
        }
        else if (Settings->Mode == EPGLCreateTaggedSplineMode::CreateNewActor)
        {
            // TODO: check ownership of target actor like in the spawn actor?
            // TODO: allow template?
            FActorSpawnParameters ActorSpawnParams;
            SplineActor = UPCGActorHelpers::SpawnDefaultActor(TargetActor->GetWorld(), TargetActor->GetLevel(), AActor::StaticClass(), TargetActor->GetTransform(), ActorSpawnParams);

            if (!SplineActor)
            {
                PCGE_LOG(Error, GraphAndLog, LOCTEXT("FailedToCreateActor", "Failed to create actor to hold the spline"));
                continue;
            }

            UPCGManagedActors* ManagedActors = NewObject<UPCGManagedActors>(Context->SourceComponent.Get());

            FTransform Transform = TargetActor->GetTransform();

            SplineActor->Tags = TargetActor->Tags;
            SplineActor->Tags.AddUnique(PCGHelpers::DefaultPCGActorTag);
            PCGHelpers::AttachToParent(SplineActor, TargetActor, Settings->AttachOptions);
            SplineData->TargetActor = SplineActor;

            ManagedActors->GeneratedActors.Add(SplineActor);
            Context->SourceComponent->AddToManagedResources(ManagedActors);
        }

        const TArray<FPCGPoint>& Points = PointData->GetPoints();
        TArray<FSplinePoint> SplinePoints;
        SplinePoints.Reserve(Points.Num());

        const FTransform SplineActorTransform = SplineActor->GetTransform();

        ESplinePointType::Type PointType = ESplinePointType::Curve;
        if (Settings->bLinear)
        {
            PointType = ESplinePointType::Linear;
        }
        else if (Settings->bApplyCustomTangents)
        {
            PointType = ESplinePointType::CurveCustomTangent;
        }

        for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
        {
            const FPCGPoint& Point = Points[PointIndex];
            const FTransform& PointTransform = Point.Transform;
            const FVector LocalPosition = PointTransform.GetLocation() - SplineActorTransform.GetLocation();

            SplinePoints.Emplace(static_cast<float>(PointIndex),
                LocalPosition,
                ArriveTangentAttribute ? ArriveTangentAttribute->GetValueFromItemKey(Point.MetadataEntry) : FVector::ZeroVector,
                LeaveTangentAttribute ? LeaveTangentAttribute->GetValueFromItemKey(Point.MetadataEntry) : FVector::ZeroVector,
                PointTransform.GetRotation().Rotator(),
                PointTransform.GetScale3D(),
                PointType);
        }

        SplineData->Initialize(SplinePoints, Settings->bClosedLoop, FTransform(SplineActorTransform.GetLocation()));

        USplineComponent* SplineComponent = nullptr;

        // Add each tag to the SplineComponent's tags
        if (SplineComponent)
        {
            for (const FString& Tag : TagsToAddArray)
            {
                if (!Tag.IsEmpty())
                {
                    SplineComponent->ComponentTags.Add(*Tag);
                }
            }
        }

        if (Settings->Mode != EPGLCreateTaggedSplineMode::CreateDataOnly)
        {
            SplineComponent = NewObject<USplineComponent>(SplineActor);
            SplineComponent->ComponentTags.Add(Context->SourceComponent.Get()->GetFName());
            SplineComponent->ComponentTags.Add(PCGHelpers::DefaultPCGTag);

            for (const FString& Tag : TagsToAddArray)
            {
                if (!Tag.IsEmpty())
                {
                    SplineComponent->ComponentTags.Add(*Tag);
                }
            }

            SplineData->ApplyTo(SplineComponent);

            SplineComponent->RegisterComponent();
            SplineActor->AddInstanceComponent(SplineComponent);
            SplineComponent->AttachToComponent(SplineActor->GetRootComponent(), FAttachmentTransformRules(EAttachmentRule::KeepRelative, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false));

            UPCGManagedComponent* ManagedComponent = NewObject<UPCGManagedComponent>(Context->SourceComponent.Get());
            ManagedComponent->GeneratedComponent = SplineComponent;
            Context->SourceComponent->AddToManagedResources(ManagedComponent);
        }

        // Execute PostProcess Functions
        if (SplineActor)
        {
            for (UFunction* Function : PCGHelpers::FindUserFunctions(SplineActor->GetClass(), Settings->PostProcessFunctionNames, { UPCGFunctionPrototypes::GetPrototypeWithNoParams() }, Context))
            {
                SplineActor->ProcessEvent(Function, nullptr);
            }
        }

        FPCGTaggedData& Output = Outputs.Add_GetRef(Input);
        Output.Data = SplineData;
    }

    // Pass-through settings & exclusions
    Context->OutputData.TaggedData.Append(Context->InputData.GetAllSettings());

    return true;
}

#undef LOCTEXT_NAMESPACE
