

#include "PCGCreateSplineMesh.h"

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

#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Math/UnitConversion.h"
#include "Engine/SplineMeshActor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGCreateSplineMesh)

#define LOCTEXT_NAMESPACE "PCGCreateSplineMesh"

UPCGCreateSplineMeshSettings::UPCGCreateSplineMeshSettings(const FObjectInitializer& ObjectInitializer)
	: UPCGSettings(ObjectInitializer)
{
	if (PCGHelpers::IsNewObjectAndNotDefault(this))
	{
		AttachOptions = EPCGAttachOptions::InFolder;
	}
}

#if WITH_EDITOR
FText UPCGCreateSplineMeshSettings::GetNodeTooltipText() const
{
	return LOCTEXT("CreateSplineTooltip", "Creates PCG spline data from the input PCG point data, in a sequential order.");
}
#endif

TArray<FPCGPinProperties> UPCGCreateSplineMeshSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Spline);

	return PinProperties;
}

FPCGElementPtr UPCGCreateSplineMeshSettings::CreateElement() const
{
	return MakeShared<FPCGCreateSplineMeshElement>();
}


bool FPCGCreateSplineMeshElement::IsCacheable(const UPCGSettings* InSettings) const
{
	const UPCGCreateSplineMeshSettings* Settings = Cast<const UPCGCreateSplineMeshSettings>(InSettings);
	return !Settings;
}

bool FPCGCreateSplineMeshElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCreateSplineMeshelement::Execute);

	const UPCGCreateSplineMeshSettings* Settings = Context->GetInputSettings<UPCGCreateSplineMeshSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;


	// Parse the "TagsToAdd" string and split it into individual tags
	TArray<FString> TagsToAddArray;

	AActor* TargetActor = Settings->TargetActor.Get() ? Settings->TargetActor.Get() : Context->GetTargetActor(nullptr);
	if (!TargetActor)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidTargetActor", "Invalid target actor. Ensure TargetActor member is initialized when creating SpatialData."));
		return false;
	}


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

		const UPCGPointData* PointData = SpatialData->ToPointData(Context);


		if (!PointData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("UnableToGetPointData", "Unable to get point data from input"));
			continue;
		}

		// Retrieve metadata using attribute names from settings

		const FPCGMetadataAttribute<FVector>* StartTangentAttribute = PointData->Metadata->GetMutableTypedAttribute<FVector>(Settings->StartTangentAttributeName);
		const FPCGMetadataAttribute<FString>* StaticMeshAttribute = PointData->Metadata->GetMutableTypedAttribute<FString>(Settings->StaticMeshAttributeName);
		const FPCGMetadataAttribute<FFloat16>* StartRollAttribute = PointData->Metadata->GetMutableTypedAttribute<FFloat16>(Settings->StartRollAttributeName);
		const FPCGMetadataAttribute<FVector2D>* StartScaleAttribute = PointData->Metadata->GetMutableTypedAttribute<FVector2D>(Settings->StartScaleAttributeName);



		UPCGSplineData* SplineData = NewObject<UPCGSplineData>();

		FActorSpawnParameters ActorSpawnParams;
		TargetActor = UPCGActorHelpers::SpawnDefaultActor(TargetActor->GetWorld(), TargetActor->GetLevel(), AActor::StaticClass(), TargetActor->GetTransform(), ActorSpawnParams);

		if (!TargetActor)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("FailedToCreateActor", "Failed to create actor to hold the spline"));
			continue;
		}

		UPCGManagedActors* ManagedActors = NewObject<UPCGManagedActors>(Context->SourceComponent.Get());

		FTransform Transform = TargetActor->GetTransform();

		TargetActor->Tags = TargetActor->Tags;
		TargetActor->Tags.AddUnique(PCGHelpers::DefaultPCGActorTag);
		SplineData->TargetActor = TargetActor;

		ManagedActors->GeneratedActors.Add(TargetActor);
		Context->SourceComponent->AddToManagedResources(ManagedActors);

		const TArray<FPCGPoint>& Points = PointData->GetPoints();
		const FPCGMetadataAttribute<FString>* MeshAttribute = PointData->Metadata->GetMutableTypedAttribute<FString>("Mesh");
		TArray<FSplinePoint> SplinePoints;
		SplinePoints.Reserve(Points.Num());

		const FTransform SplineActorTransform = TargetActor->GetTransform();

		USplineMeshComponent* SplineMeshComponent = nullptr;

		for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
		{
			const FPCGPoint& Point = Points[PointIndex];
			const FTransform& PointTransform = Point.Transform;
			const FVector LocalPosition = PointTransform.GetLocation() - SplineActorTransform.GetLocation();
			const FPCGPoint& NextPoint = (PointIndex + 1 < Points.Num()) ? Points[PointIndex + 1] : Point;


			UStaticMesh* Mesh = nullptr;
			if (StaticMeshAttribute) {
				FString MeshPath = StaticMeshAttribute->GetValueFromItemKey(Point.MetadataEntry);
				if (!MeshPath.IsEmpty()) {
					FSoftObjectPath SoftMeshPath(MeshPath);
					TSoftObjectPtr<UStaticMesh> MeshSoftRef(SoftMeshPath);
					Mesh = MeshSoftRef.LoadSynchronous();
				}
			}

			if (!Mesh) {
				// Handle the case where mesh is not loaded properly
				PCGE_LOG(Error, GraphAndLog, LOCTEXT("FailedToLoadMesh", "Failed to load mesh from metadata path"));
				continue;
			}


			UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(TargetActor);

			PointTransform.GetRotation().Rotator(),
				PointTransform.GetScale3D();
			;

			SplineMeshComponent = NewObject<USplineMeshComponent>(TargetActor);
			SplineMeshComponent->ComponentTags.Add(Context->SourceComponent.Get()->GetFName());
			SplineMeshComponent->ComponentTags.Add(PCGHelpers::DefaultPCGTag);


			for (const FString& Tag : TagsToAddArray)
			{
				if (!Tag.IsEmpty())
				{
					SplineMeshComponent->ComponentTags.Add(*Tag);
				}
			}

			// Set start and end properties based on point data
			FVector StartTangent = StartTangentAttribute ? StartTangentAttribute->GetValueFromItemKey(Point.MetadataEntry) : FVector::ZeroVector;
			FVector EndTangent = StartTangentAttribute ? StartTangentAttribute->GetValueFromItemKey(NextPoint.MetadataEntry) : FVector::ZeroVector;
			FFloat16 StartRoll = StartRollAttribute ? StartRollAttribute->GetValueFromItemKey(Point.MetadataEntry) : FFloat16(0.0f);
			FVector2D StartScale = StartScaleAttribute ? StartScaleAttribute->GetValueFromItemKey(Point.MetadataEntry) : FVector2D::ZeroVector;
			FFloat16 EndRoll = StartRollAttribute ? StartRollAttribute->GetValueFromItemKey(NextPoint.MetadataEntry) : FFloat16(0.0f);
			FVector2D EndScale = StartScaleAttribute ? StartScaleAttribute->GetValueFromItemKey(NextPoint.MetadataEntry) : FVector2D::ZeroVector;

			FVector StartPos = LocalPosition;
			FVector EndPos = NextPoint.Transform.GetTranslation();


			SplineMeshComponent->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent, true);
			SplineMeshComponent->SetStaticMesh(Mesh);
			SplineMeshComponent->SetStartRollDegrees(StartRoll, true);
			SplineMeshComponent->SetEndRollDegrees(EndRoll, true);
			SplineMeshComponent->SetStartScale(StartScale, true);
			SplineMeshComponent->SetEndScale(EndScale, true);

			SplineMeshComponent->RegisterComponent();
			if (TargetActor) {
				SplineMeshComponent->AttachToComponent(TargetActor->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
			}
			else {
				SplineMeshComponent->AttachToComponent(TargetActor->GetRootComponent(), FAttachmentTransformRules(EAttachmentRule::KeepRelative, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false));
			}

			TargetActor->AddInstanceComponent(SplineMeshComponent);

			UPCGManagedComponent* ManagedComponent = NewObject<UPCGManagedComponent>(Context->SourceComponent.Get());
			ManagedComponent->GeneratedComponent = SplineMeshComponent;
			Context->SourceComponent->AddToManagedResources(ManagedComponent);
		}
		// Add each tag to the SplineMeshComponent's tags
		if (SplineMeshComponent)
		{
			for (const FString& Tag : TagsToAddArray)
			{
				if (!Tag.IsEmpty())
				{
					SplineMeshComponent->ComponentTags.Add(*Tag);
				}
			}
		}

		// Execute PostProcess Functions
		if (TargetActor)
		{
			for (UFunction* Function : PCGHelpers::FindUserFunctions(TargetActor->GetClass(), Settings->PostProcessFunctionNames, { UPCGFunctionPrototypes::GetPrototypeWithNoParams() }, Context))
			{
				TargetActor->ProcessEvent(Function, nullptr);
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