// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PCGSpawnVoxelSculptActor.h"
#include "PCGContext.h"
#include "PCGComponent.h"
#include "Helpers/PCGHelpers.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "PCGManagedResource.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "UObject/SoftObjectPath.h"

// VoxelSculpt includes
#include "Sculpt/Height/VoxelSculptHeight.h"
#include "Sculpt/Height/VoxelSculptHeightAsset.h"
#include "Sculpt/Height/VoxelHeightSculptStamp.h"
#include "Sculpt/Volume/VoxelSculptVolume.h"
#include "Sculpt/Volume/VoxelSculptVolumeAsset.h"
#include "Sculpt/Volume/VoxelVolumeSculptStamp.h"
#include "VoxelStampBehavior.h"
#include "VoxelHeightBlendMode.h"
#include "VoxelVolumeBlendMode.h"
#include "VoxelLayer.h"

UPCGSpawnVoxelSculptActorSettings::UPCGSpawnVoxelSculptActorSettings()
{
	DefaultActorType = EPCGVoxelSculptActorType::Height;
	ActorTypeAttribute = NAME_None;
	ExternalAssetPathAttribute = "ExternalAssetPath";

	// Layer properties
	HeightLayerPathAttribute = "HeightLayerPath";
	DefaultHeightLayer = nullptr;
	VolumeLayerPathAttribute = "VolumeLayerPath";
	DefaultVolumeLayer = nullptr;

	// Height properties
	ScaleXYAttribute = "ScaleXY";
	DefaultScaleXY = 100.0f;
	StoreRelativeHeightsAttribute = "StoreRelativeHeights";
	bDefaultStoreRelativeHeights = true;
	HeightMaxErrorPercentageAttribute = "MaxErrorPercentage";
	DefaultHeightMaxErrorPercentage = 0.005f;
	HeightNearMaxLODAttribute = "NearMaxLOD";
	DefaultHeightNearMaxLOD = 3;
	HeightMidMaxLODAttribute = "MidMaxLOD";
	DefaultHeightMidMaxLOD = 6;

	// Volume properties
	ScaleAttribute = "Scale";
	DefaultScale = 100.0f;
	StoreMovableDistancesAttribute = "StoreMovableDistances";
	bDefaultStoreMovableDistances = true;
	VolumeMaxErrorPercentageAttribute = "MaxErrorPercentage";
	DefaultVolumeMaxErrorPercentage = 0.005f;
	VolumeNearMaxLODAttribute = "NearMaxLOD";
	DefaultVolumeNearMaxLOD = 3;
	VolumeMidMaxLODAttribute = "MidMaxLOD";
	DefaultVolumeMidMaxLOD = 6;

	// Common properties
	IsInfiniteAttribute = "IsInfinite";
	bDefaultIsInfinite = false;

	// Common stamp properties
	BehaviorAttribute = "Behavior";
	DefaultBehavior = static_cast<uint8>(EVoxelStampBehavior::AffectAll);
	PriorityAttribute = "Priority";
	DefaultPriority = 0;
	SmoothnessAttribute = "Smoothness";
	DefaultSmoothness = 100.0f;

	// Height-specific stamp properties
	HeightBlendModeAttribute = "BlendMode";
	DefaultHeightBlendMode = static_cast<uint8>(EVoxelHeightBlendMode::Max);
	HeightPaddingMultiplierAttribute = "HeightPaddingMultiplier";
	DefaultHeightPaddingMultiplier = 0.1f;

	// Volume-specific stamp properties
	VolumeBlendModeAttribute = "BlendMode";
	DefaultVolumeBlendMode = static_cast<uint8>(EVoxelVolumeBlendMode::Additive);
	BoundsExtensionMultiplierAttribute = "BoundsExtensionMultiplier";
	DefaultBoundsExtensionMultiplier = 1.0f;
	MaximumBoundsExtensionAttribute = "MaximumBoundsExtension";
	DefaultMaximumBoundsExtension = 1000.0f;
}

FPCGElementPtr UPCGSpawnVoxelSculptActorSettings::CreateElement() const
{
	return MakeShared<FPCGSpawnVoxelSculptActorElement>();
}

TArray<FPCGPinProperties> UPCGSpawnVoxelSculptActorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point);
	return Properties;
}

TArray<FPCGPinProperties> UPCGSpawnVoxelSculptActorSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
	return Properties;
}

FPCGContext* FPCGSpawnVoxelSculptActorElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGContext* Context = new FPCGContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;
	return Context;
}

bool FPCGSpawnVoxelSculptActorElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSpawnVoxelSculptActorElement::Execute);

	check(Context);

	if (!IsInGameThread())
	{
		PCGE_LOG(Error, GraphAndLog, NSLOCTEXT("PCGSpawnVoxelSculptActor", "NotGameThread", "SpawnVoxelSculptActor must execute on the game thread"));
		return true;
	}

	const UPCGSpawnVoxelSculptActorSettings* Settings = Context->GetInputSettings<UPCGSpawnVoxelSculptActorSettings>();
	check(Settings);

	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (!SourceComponent)
	{
		PCGE_LOG(Error, GraphAndLog, NSLOCTEXT("PCGSpawnVoxelSculptActor", "InvalidSourceComponent", "Invalid source component"));
		return true;
	}

	UWorld* World = SourceComponent->GetWorld();
	if (!World)
	{
		PCGE_LOG(Error, GraphAndLog, NSLOCTEXT("PCGSpawnVoxelSculptActor", "InvalidWorld", "Invalid world"));
		return true;
	}

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	int32 TotalSpawnedActors = 0;

	// Determine root actor for attachment
	AActor* RootActor = Settings->RootActor.Get();
	if (!RootActor)
	{
		RootActor = SourceComponent->GetOwner();
	}

	// Prepare actor tags
	TArray<FName> NewActorTags = GetNewActorTags(Context, RootActor, Settings);

	// Prepare folder path for attachment
	FString GeneratedActorsFolderPath = RootActor ? RootActor->GetFName().ToString() : FString();

	// Create the managed actors resource upfront
	UPCGManagedActors* ManagedActors = NewObject<UPCGManagedActors>(SourceComponent);
#if WITH_EDITOR
	ManagedActors->SetIsPreview(SourceComponent->IsInPreviewMode());
#endif
	ManagedActors->bSupportsReset = true;

	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGPointData* PointData = Cast<UPCGPointData>(Input.Data);
		if (!PointData)
		{
			continue;
		}

		const TArray<FPCGPoint>& Points = PointData->GetPoints();
		const UPCGMetadata* Metadata = PointData->Metadata;

		if (!Metadata)
		{
			PCGE_LOG(Warning, GraphAndLog, NSLOCTEXT("PCGSpawnVoxelSculptActor", "NoMetadata", "Point data has no metadata"));
			continue;
		}

		for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
		{
			const FPCGPoint& Point = Points[PointIndex];
			FTransform PointTransform = Point.Transform;

			// Determine actor type for this point
			EPCGVoxelSculptActorType ActorType = DetermineActorType(Settings, Metadata, Point);

			AVoxelSculptActorBase* SpawnedActor = SpawnAndConfigureActor(Context, Settings, ActorType, PointTransform, Metadata, Point);

			if (SpawnedActor)
			{
				TotalSpawnedActors++;

				// Apply actor tags
				SpawnedActor->Tags.Append(NewActorTags);

				// Handle attachment based on settings
				PCGHelpers::AttachToParent(SpawnedActor, RootActor, Settings->AttachOptions, Context, GeneratedActorsFolderPath);

				// Add to managed actors immediately
				ManagedActors->GetMutableGeneratedActors().AddUnique(SpawnedActor);

				if (Settings->bEnableDebugLogging)
				{
					PCGE_LOG(Verbose, LogOnly, FText::Format(
						NSLOCTEXT("PCGSpawnVoxelSculptActor", "ActorSpawned", "Spawned {0} at point {1}"),
						FText::FromString(SpawnedActor->GetName()),
						FText::AsNumber(PointIndex)));
				}
			}
		}
	}

	// Register the managed resource with the component
	SourceComponent->AddToManagedResources(ManagedActors);

	PCGE_LOG(Verbose, LogOnly, FText::Format(
		NSLOCTEXT("PCGSpawnVoxelSculptActor", "SpawnComplete", "Spawned {0} VoxelSculpt actors"),
		FText::AsNumber(TotalSpawnedActors)));

	return true;
}

EPCGVoxelSculptActorType FPCGSpawnVoxelSculptActorElement::DetermineActorType(
	const UPCGSpawnVoxelSculptActorSettings* Settings,
	const UPCGMetadata* Metadata,
	const FPCGPoint& Point) const
{
	// Check if we should read from attribute
	if (Settings->ActorTypeAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* TypeAttribute = Metadata->GetConstAttribute(Settings->ActorTypeAttribute);
		if (TypeAttribute)
		{
			FString TypeString;

			if (TypeAttribute->GetTypeId() == PCG::Private::MetadataTypes<FString>::Id)
			{
				const FPCGMetadataAttribute<FString>* StringAttribute = static_cast<const FPCGMetadataAttribute<FString>*>(TypeAttribute);
				TypeString = StringAttribute->GetValueFromItemKey(Point.MetadataEntry);
			}
			else if (TypeAttribute->GetTypeId() == PCG::Private::MetadataTypes<FName>::Id)
			{
				const FPCGMetadataAttribute<FName>* NameAttribute = static_cast<const FPCGMetadataAttribute<FName>*>(TypeAttribute);
				TypeString = NameAttribute->GetValueFromItemKey(Point.MetadataEntry).ToString();
			}

			if (!TypeString.IsEmpty())
			{
				if (TypeString.Equals(TEXT("Height"), ESearchCase::IgnoreCase))
				{
					return EPCGVoxelSculptActorType::Height;
				}
				else if (TypeString.Equals(TEXT("Volume"), ESearchCase::IgnoreCase))
				{
					return EPCGVoxelSculptActorType::Volume;
				}
			}
		}
	}

	// Fall back to default
	return Settings->DefaultActorType;
}

AVoxelSculptActorBase* FPCGSpawnVoxelSculptActorElement::SpawnAndConfigureActor(
	FPCGContext* Context,
	const UPCGSpawnVoxelSculptActorSettings* Settings,
	EPCGVoxelSculptActorType ActorType,
	const FTransform& Transform,
	const UPCGMetadata* Metadata,
	const FPCGPoint& Point) const
{
	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (!SourceComponent)
	{
		return nullptr;
	}

	UWorld* World = SourceComponent->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Determine actor class to spawn
	TSubclassOf<AVoxelSculptActorBase> ActorClass;
	if (ActorType == EPCGVoxelSculptActorType::Height)
	{
		ActorClass = AVoxelSculptHeight::StaticClass();
	}
	else
	{
		ActorClass = AVoxelSculptVolume::StaticClass();
	}

	// Spawn the actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.bNoFail = true;
	SpawnParams.Owner = SourceComponent->GetOwner();

	AVoxelSculptActorBase* SpawnedActor = World->SpawnActor<AVoxelSculptActorBase>(ActorClass, Transform, SpawnParams);
	if (!SpawnedActor)
	{
		PCGE_LOG(Warning, GraphAndLog, NSLOCTEXT("PCGSpawnVoxelSculptActor", "SpawnFailed", "Failed to spawn VoxelSculpt actor"));
		return nullptr;
	}

	// Get the external asset path from attribute
	FSoftObjectPath ExternalAssetPath;
	if (Settings->ExternalAssetPathAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* PathAttribute = Metadata->GetConstAttribute(Settings->ExternalAssetPathAttribute);
		if (PathAttribute)
		{
			if (PathAttribute->GetTypeId() == PCG::Private::MetadataTypes<FString>::Id)
			{
				const FPCGMetadataAttribute<FString>* StringAttribute = static_cast<const FPCGMetadataAttribute<FString>*>(PathAttribute);
				FString PathString = StringAttribute->GetValueFromItemKey(Point.MetadataEntry);
				ExternalAssetPath = FSoftObjectPath(PathString);
			}
			else if (PathAttribute->GetTypeId() == PCG::Private::MetadataTypes<FSoftObjectPath>::Id)
			{
				const FPCGMetadataAttribute<FSoftObjectPath>* SoftPathAttribute = static_cast<const FPCGMetadataAttribute<FSoftObjectPath>*>(PathAttribute);
				ExternalAssetPath = SoftPathAttribute->GetValueFromItemKey(Point.MetadataEntry);
			}
		}
	}

	// Configure stamp properties FIRST, before setting ExternalAsset
	if (ActorType == EPCGVoxelSculptActorType::Height)
	{
		ConfigureHeightStampProperties(Cast<AVoxelSculptHeight>(SpawnedActor), Settings, Metadata, Point);
	}
	else
	{
		ConfigureVolumeStampProperties(Cast<AVoxelSculptVolume>(SpawnedActor), Settings, Metadata, Point);
	}

	// Set the ExternalAsset if a path was provided
	if (ExternalAssetPath.IsValid())
	{
		SetExternalAsset(SpawnedActor, ActorType, ExternalAssetPath, Context, Settings->bEnableDebugLogging);
	}

	return SpawnedActor;
}

bool FPCGSpawnVoxelSculptActorElement::SetExternalAsset(
	AVoxelSculptActorBase* Actor,
	EPCGVoxelSculptActorType ActorType,
	const FSoftObjectPath& AssetPath,
	FPCGContext* Context,
	bool bEnableLogging) const
{
	if (!Actor)
	{
		return false;
	}

	// Load the asset synchronously
	UObject* LoadedAsset = AssetPath.TryLoad();
	if (!LoadedAsset)
	{
		PCGE_LOG(Warning, GraphAndLog, FText::Format(
			NSLOCTEXT("PCGSpawnVoxelSculptActor", "FailedToLoadAsset", "Failed to load external asset from path: {0}"),
			FText::FromString(AssetPath.ToString())));
		return false;
	}

	// Validate asset type matches actor type
	bool bIsValidType = false;
	FString ExpectedTypeName;
	FString ActualTypeName = LoadedAsset->GetClass()->GetName();

	if (ActorType == EPCGVoxelSculptActorType::Height)
	{
		ExpectedTypeName = TEXT("VoxelSculptHeightAsset");
		UVoxelSculptHeightAsset* HeightAsset = Cast<UVoxelSculptHeightAsset>(LoadedAsset);
		if (HeightAsset)
		{
			AVoxelSculptHeight* HeightActor = Cast<AVoxelSculptHeight>(Actor);
			if (HeightActor)
			{
				HeightActor->GetComponent().ExternalAsset = HeightAsset;
				bIsValidType = true;
			}
		}
	}
	else // Volume
	{
		ExpectedTypeName = TEXT("VoxelSculptVolumeAsset");
		UVoxelSculptVolumeAsset* VolumeAsset = Cast<UVoxelSculptVolumeAsset>(LoadedAsset);
		if (VolumeAsset)
		{
			AVoxelSculptVolume* VolumeActor = Cast<AVoxelSculptVolume>(Actor);
			if (VolumeActor)
			{
				VolumeActor->GetComponent().ExternalAsset = VolumeAsset;
				bIsValidType = true;
			}
		}
	}

	if (!bIsValidType)
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(
			NSLOCTEXT("PCGSpawnVoxelSculptActor", "AssetTypeMismatch", "Asset type mismatch: Expected {0} but got {1}. Actor type is {2}."),
			FText::FromString(ExpectedTypeName),
			FText::FromString(ActualTypeName),
			ActorType == EPCGVoxelSculptActorType::Height ? FText::FromString(TEXT("Height")) : FText::FromString(TEXT("Volume"))));
		return false;
	}

	if (bEnableLogging)
	{
		UE_LOG(LogPCG, Verbose, TEXT("PCGSpawnVoxelSculptActor: Set ExternalAsset on %s to %s"), *Actor->GetName(), *LoadedAsset->GetName());
	}

	return true;
}

void FPCGSpawnVoxelSculptActorElement::ConfigureHeightStampProperties(
	AVoxelSculptHeight* Actor,
	const UPCGSpawnVoxelSculptActorSettings* Settings,
	const UPCGMetadata* Metadata,
	const FPCGPoint& Point) const
{
	if (!Actor)
	{
		return;
	}

	UVoxelSculptHeightComponent& Component = Actor->GetComponent();
	FVoxelHeightSculptStampRef StampRef = Component.GetStamp();
	FVoxelHeightSculptStamp* Stamp = StampRef.operator->();

	if (!Stamp)
	{
		UE_LOG(LogPCG, Warning, TEXT("PCGSpawnVoxelSculptActor: Failed to get Height stamp from component"));
		return;
	}

	// Configure common stamp properties (from FVoxelStamp base)

	// Configure Behavior
	uint8 Behavior = Settings->DefaultBehavior;
	if (Settings->BehaviorAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* BehaviorAttribute = Metadata->GetConstAttribute(Settings->BehaviorAttribute);
		if (BehaviorAttribute)
		{
			if (BehaviorAttribute->GetTypeId() == PCG::Private::MetadataTypes<int32>::Id)
			{
				const FPCGMetadataAttribute<int32>* IntAttribute = static_cast<const FPCGMetadataAttribute<int32>*>(BehaviorAttribute);
				Behavior = static_cast<uint8>(IntAttribute->GetValueFromItemKey(Point.MetadataEntry));
			}
			else if (BehaviorAttribute->GetTypeId() == PCG::Private::MetadataTypes<int64>::Id)
			{
				const FPCGMetadataAttribute<int64>* Int64Attribute = static_cast<const FPCGMetadataAttribute<int64>*>(BehaviorAttribute);
				Behavior = static_cast<uint8>(Int64Attribute->GetValueFromItemKey(Point.MetadataEntry));
			}
		}
	}
	Stamp->Behavior = static_cast<EVoxelStampBehavior>(Behavior);

	// Configure Priority
	int32 Priority = Settings->DefaultPriority;
	if (Settings->PriorityAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* PriorityAttribute = Metadata->GetConstAttribute(Settings->PriorityAttribute);
		if (PriorityAttribute)
		{
			if (PriorityAttribute->GetTypeId() == PCG::Private::MetadataTypes<int32>::Id)
			{
				const FPCGMetadataAttribute<int32>* IntAttribute = static_cast<const FPCGMetadataAttribute<int32>*>(PriorityAttribute);
				Priority = IntAttribute->GetValueFromItemKey(Point.MetadataEntry);
			}
			else if (PriorityAttribute->GetTypeId() == PCG::Private::MetadataTypes<int64>::Id)
			{
				const FPCGMetadataAttribute<int64>* Int64Attribute = static_cast<const FPCGMetadataAttribute<int64>*>(PriorityAttribute);
				Priority = static_cast<int32>(Int64Attribute->GetValueFromItemKey(Point.MetadataEntry));
			}
		}
	}
	Stamp->Priority = Priority;

	// Configure Smoothness
	float Smoothness = Settings->DefaultSmoothness;
	if (Settings->SmoothnessAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* SmoothnessAttribute = Metadata->GetConstAttribute(Settings->SmoothnessAttribute);
		if (SmoothnessAttribute)
		{
			if (SmoothnessAttribute->GetTypeId() == PCG::Private::MetadataTypes<float>::Id)
			{
				const FPCGMetadataAttribute<float>* FloatAttribute = static_cast<const FPCGMetadataAttribute<float>*>(SmoothnessAttribute);
				Smoothness = FloatAttribute->GetValueFromItemKey(Point.MetadataEntry);
			}
			else if (SmoothnessAttribute->GetTypeId() == PCG::Private::MetadataTypes<double>::Id)
			{
				const FPCGMetadataAttribute<double>* DoubleAttribute = static_cast<const FPCGMetadataAttribute<double>*>(SmoothnessAttribute);
				Smoothness = static_cast<float>(DoubleAttribute->GetValueFromItemKey(Point.MetadataEntry));
			}
		}
	}
	Stamp->Smoothness = FMath::Max(0.0f, Smoothness);

	// Configure Height-specific properties

	// Configure Layer
	UVoxelHeightLayer* Layer = Settings->DefaultHeightLayer.Get();
	if (Settings->HeightLayerPathAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* LayerAttribute = Metadata->GetConstAttribute(Settings->HeightLayerPathAttribute);
		if (LayerAttribute && LayerAttribute->GetTypeId() == PCG::Private::MetadataTypes<FString>::Id)
		{
			const FPCGMetadataAttribute<FString>* StringAttribute = static_cast<const FPCGMetadataAttribute<FString>*>(LayerAttribute);
			FString LayerPathString = StringAttribute->GetValueFromItemKey(Point.MetadataEntry);

			if (!LayerPathString.IsEmpty())
			{
				FSoftObjectPath LayerPath(LayerPathString);
				if (UObject* LoadedLayer = LayerPath.TryLoad())
				{
					Layer = Cast<UVoxelHeightLayer>(LoadedLayer);
					if (!Layer && Settings->bEnableDebugLogging)
					{
						UE_LOG(LogPCG, Warning, TEXT("PCGSpawnVoxelSculptActor: Loaded object is not a UVoxelHeightLayer: %s"), *LayerPathString);
					}
				}
			}
		}
	}
	Stamp->Layer = Layer;

	// Configure BlendMode
	uint8 BlendMode = Settings->DefaultHeightBlendMode;
	if (Settings->HeightBlendModeAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* BlendModeAttribute = Metadata->GetConstAttribute(Settings->HeightBlendModeAttribute);
		if (BlendModeAttribute)
		{
			if (BlendModeAttribute->GetTypeId() == PCG::Private::MetadataTypes<int32>::Id)
			{
				const FPCGMetadataAttribute<int32>* IntAttribute = static_cast<const FPCGMetadataAttribute<int32>*>(BlendModeAttribute);
				BlendMode = static_cast<uint8>(IntAttribute->GetValueFromItemKey(Point.MetadataEntry));
			}
			else if (BlendModeAttribute->GetTypeId() == PCG::Private::MetadataTypes<int64>::Id)
			{
				const FPCGMetadataAttribute<int64>* Int64Attribute = static_cast<const FPCGMetadataAttribute<int64>*>(BlendModeAttribute);
				BlendMode = static_cast<uint8>(Int64Attribute->GetValueFromItemKey(Point.MetadataEntry));
			}
		}
	}
	Stamp->BlendMode = static_cast<EVoxelHeightBlendMode>(BlendMode);

	// Configure HeightPaddingMultiplier
	float HeightPaddingMultiplier = Settings->DefaultHeightPaddingMultiplier;
	if (Settings->HeightPaddingMultiplierAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* PaddingAttribute = Metadata->GetConstAttribute(Settings->HeightPaddingMultiplierAttribute);
		if (PaddingAttribute)
		{
			if (PaddingAttribute->GetTypeId() == PCG::Private::MetadataTypes<float>::Id)
			{
				const FPCGMetadataAttribute<float>* FloatAttribute = static_cast<const FPCGMetadataAttribute<float>*>(PaddingAttribute);
				HeightPaddingMultiplier = FloatAttribute->GetValueFromItemKey(Point.MetadataEntry);
			}
			else if (PaddingAttribute->GetTypeId() == PCG::Private::MetadataTypes<double>::Id)
			{
				const FPCGMetadataAttribute<double>* DoubleAttribute = static_cast<const FPCGMetadataAttribute<double>*>(PaddingAttribute);
				HeightPaddingMultiplier = static_cast<float>(DoubleAttribute->GetValueFromItemKey(Point.MetadataEntry));
			}
		}
	}
	Stamp->HeightPaddingMultiplier = FMath::Clamp(HeightPaddingMultiplier, 0.0f, 5.0f);

	// Configure ScaleXY
	float ScaleXY = Settings->DefaultScaleXY;
	if (Settings->ScaleXYAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* ScaleAttribute = Metadata->GetConstAttribute(Settings->ScaleXYAttribute);
		if (ScaleAttribute)
		{
			if (ScaleAttribute->GetTypeId() == PCG::Private::MetadataTypes<float>::Id)
			{
				const FPCGMetadataAttribute<float>* FloatAttribute = static_cast<const FPCGMetadataAttribute<float>*>(ScaleAttribute);
				ScaleXY = FloatAttribute->GetValueFromItemKey(Point.MetadataEntry);
			}
			else if (ScaleAttribute->GetTypeId() == PCG::Private::MetadataTypes<double>::Id)
			{
				const FPCGMetadataAttribute<double>* DoubleAttribute = static_cast<const FPCGMetadataAttribute<double>*>(ScaleAttribute);
				ScaleXY = static_cast<float>(DoubleAttribute->GetValueFromItemKey(Point.MetadataEntry));
			}
		}
	}
	Stamp->ScaleXY = ScaleXY;

	// Configure bStoreRelativeHeights
	bool bStoreRelativeHeights = Settings->bDefaultStoreRelativeHeights;
	if (Settings->StoreRelativeHeightsAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* StoreAttribute = Metadata->GetConstAttribute(Settings->StoreRelativeHeightsAttribute);
		if (StoreAttribute && StoreAttribute->GetTypeId() == PCG::Private::MetadataTypes<bool>::Id)
		{
			const FPCGMetadataAttribute<bool>* BoolAttribute = static_cast<const FPCGMetadataAttribute<bool>*>(StoreAttribute);
			bStoreRelativeHeights = BoolAttribute->GetValueFromItemKey(Point.MetadataEntry);
		}
	}
	Stamp->bStoreRelativeHeights = bStoreRelativeHeights;

	// Configure bIsInfinite
	bool bIsInfinite = Settings->bDefaultIsInfinite;
	if (Settings->IsInfiniteAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* InfiniteAttribute = Metadata->GetConstAttribute(Settings->IsInfiniteAttribute);
		if (InfiniteAttribute && InfiniteAttribute->GetTypeId() == PCG::Private::MetadataTypes<bool>::Id)
		{
			const FPCGMetadataAttribute<bool>* BoolAttribute = static_cast<const FPCGMetadataAttribute<bool>*>(InfiniteAttribute);
			bIsInfinite = BoolAttribute->GetValueFromItemKey(Point.MetadataEntry);
		}
	}
	Stamp->bIsInfinite = bIsInfinite;

	// Configure MaxErrorPercentage
	float MaxErrorPercentage = Settings->DefaultHeightMaxErrorPercentage;
	if (Settings->HeightMaxErrorPercentageAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* ErrorAttribute = Metadata->GetConstAttribute(Settings->HeightMaxErrorPercentageAttribute);
		if (ErrorAttribute)
		{
			if (ErrorAttribute->GetTypeId() == PCG::Private::MetadataTypes<float>::Id)
			{
				const FPCGMetadataAttribute<float>* FloatAttribute = static_cast<const FPCGMetadataAttribute<float>*>(ErrorAttribute);
				MaxErrorPercentage = FloatAttribute->GetValueFromItemKey(Point.MetadataEntry);
			}
			else if (ErrorAttribute->GetTypeId() == PCG::Private::MetadataTypes<double>::Id)
			{
				const FPCGMetadataAttribute<double>* DoubleAttribute = static_cast<const FPCGMetadataAttribute<double>*>(ErrorAttribute);
				MaxErrorPercentage = static_cast<float>(DoubleAttribute->GetValueFromItemKey(Point.MetadataEntry));
			}
		}
	}

	// Configure NearMaxLOD
	int32 NearMaxLOD = Settings->DefaultHeightNearMaxLOD;
	if (Settings->HeightNearMaxLODAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* LODAttribute = Metadata->GetConstAttribute(Settings->HeightNearMaxLODAttribute);
		if (LODAttribute)
		{
			if (LODAttribute->GetTypeId() == PCG::Private::MetadataTypes<int32>::Id)
			{
				const FPCGMetadataAttribute<int32>* IntAttribute = static_cast<const FPCGMetadataAttribute<int32>*>(LODAttribute);
				NearMaxLOD = IntAttribute->GetValueFromItemKey(Point.MetadataEntry);
			}
			else if (LODAttribute->GetTypeId() == PCG::Private::MetadataTypes<int64>::Id)
			{
				const FPCGMetadataAttribute<int64>* Int64Attribute = static_cast<const FPCGMetadataAttribute<int64>*>(LODAttribute);
				NearMaxLOD = static_cast<int32>(Int64Attribute->GetValueFromItemKey(Point.MetadataEntry));
			}
		}
	}
	Stamp->NearMaxLOD = NearMaxLOD;

	// Configure MidMaxLOD
	int32 MidMaxLOD = Settings->DefaultHeightMidMaxLOD;
	if (Settings->HeightMidMaxLODAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* LODAttribute = Metadata->GetConstAttribute(Settings->HeightMidMaxLODAttribute);
		if (LODAttribute)
		{
			if (LODAttribute->GetTypeId() == PCG::Private::MetadataTypes<int32>::Id)
			{
				const FPCGMetadataAttribute<int32>* IntAttribute = static_cast<const FPCGMetadataAttribute<int32>*>(LODAttribute);
				MidMaxLOD = IntAttribute->GetValueFromItemKey(Point.MetadataEntry);
			}
			else if (LODAttribute->GetTypeId() == PCG::Private::MetadataTypes<int64>::Id)
			{
				const FPCGMetadataAttribute<int64>* Int64Attribute = static_cast<const FPCGMetadataAttribute<int64>*>(LODAttribute);
				MidMaxLOD = static_cast<int32>(Int64Attribute->GetValueFromItemKey(Point.MetadataEntry));
			}
		}
	}
	Stamp->MidMaxLOD = MidMaxLOD;
}

void FPCGSpawnVoxelSculptActorElement::ConfigureVolumeStampProperties(
	AVoxelSculptVolume* Actor,
	const UPCGSpawnVoxelSculptActorSettings* Settings,
	const UPCGMetadata* Metadata,
	const FPCGPoint& Point) const
{
	if (!Actor)
	{
		return;
	}

	UVoxelSculptVolumeComponent& Component = Actor->GetComponent();
	FVoxelVolumeSculptStampRef StampRef = Component.GetStamp();
	FVoxelVolumeSculptStamp* Stamp = StampRef.operator->();

	if (!Stamp)
	{
		UE_LOG(LogPCG, Warning, TEXT("PCGSpawnVoxelSculptActor: Failed to get Volume stamp from component"));
		return;
	}

	// Configure common stamp properties (from FVoxelStamp base)

	// Configure Behavior
	uint8 Behavior = Settings->DefaultBehavior;
	if (Settings->BehaviorAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* BehaviorAttribute = Metadata->GetConstAttribute(Settings->BehaviorAttribute);
		if (BehaviorAttribute)
		{
			if (BehaviorAttribute->GetTypeId() == PCG::Private::MetadataTypes<int32>::Id)
			{
				const FPCGMetadataAttribute<int32>* IntAttribute = static_cast<const FPCGMetadataAttribute<int32>*>(BehaviorAttribute);
				Behavior = static_cast<uint8>(IntAttribute->GetValueFromItemKey(Point.MetadataEntry));
			}
			else if (BehaviorAttribute->GetTypeId() == PCG::Private::MetadataTypes<int64>::Id)
			{
				const FPCGMetadataAttribute<int64>* Int64Attribute = static_cast<const FPCGMetadataAttribute<int64>*>(BehaviorAttribute);
				Behavior = static_cast<uint8>(Int64Attribute->GetValueFromItemKey(Point.MetadataEntry));
			}
		}
	}
	Stamp->Behavior = static_cast<EVoxelStampBehavior>(Behavior);

	// Configure Priority
	int32 Priority = Settings->DefaultPriority;
	if (Settings->PriorityAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* PriorityAttribute = Metadata->GetConstAttribute(Settings->PriorityAttribute);
		if (PriorityAttribute)
		{
			if (PriorityAttribute->GetTypeId() == PCG::Private::MetadataTypes<int32>::Id)
			{
				const FPCGMetadataAttribute<int32>* IntAttribute = static_cast<const FPCGMetadataAttribute<int32>*>(PriorityAttribute);
				Priority = IntAttribute->GetValueFromItemKey(Point.MetadataEntry);
			}
			else if (PriorityAttribute->GetTypeId() == PCG::Private::MetadataTypes<int64>::Id)
			{
				const FPCGMetadataAttribute<int64>* Int64Attribute = static_cast<const FPCGMetadataAttribute<int64>*>(PriorityAttribute);
				Priority = static_cast<int32>(Int64Attribute->GetValueFromItemKey(Point.MetadataEntry));
			}
		}
	}
	Stamp->Priority = Priority;

	// Configure Smoothness
	float Smoothness = Settings->DefaultSmoothness;
	if (Settings->SmoothnessAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* SmoothnessAttribute = Metadata->GetConstAttribute(Settings->SmoothnessAttribute);
		if (SmoothnessAttribute)
		{
			if (SmoothnessAttribute->GetTypeId() == PCG::Private::MetadataTypes<float>::Id)
			{
				const FPCGMetadataAttribute<float>* FloatAttribute = static_cast<const FPCGMetadataAttribute<float>*>(SmoothnessAttribute);
				Smoothness = FloatAttribute->GetValueFromItemKey(Point.MetadataEntry);
			}
			else if (SmoothnessAttribute->GetTypeId() == PCG::Private::MetadataTypes<double>::Id)
			{
				const FPCGMetadataAttribute<double>* DoubleAttribute = static_cast<const FPCGMetadataAttribute<double>*>(SmoothnessAttribute);
				Smoothness = static_cast<float>(DoubleAttribute->GetValueFromItemKey(Point.MetadataEntry));
			}
		}
	}
	Stamp->Smoothness = FMath::Max(0.0f, Smoothness);

	// Configure Volume-specific properties

	// Configure Layer
	UVoxelVolumeLayer* Layer = Settings->DefaultVolumeLayer.Get();
	if (Settings->VolumeLayerPathAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* LayerAttribute = Metadata->GetConstAttribute(Settings->VolumeLayerPathAttribute);
		if (LayerAttribute && LayerAttribute->GetTypeId() == PCG::Private::MetadataTypes<FString>::Id)
		{
			const FPCGMetadataAttribute<FString>* StringAttribute = static_cast<const FPCGMetadataAttribute<FString>*>(LayerAttribute);
			FString LayerPathString = StringAttribute->GetValueFromItemKey(Point.MetadataEntry);

			if (!LayerPathString.IsEmpty())
			{
				FSoftObjectPath LayerPath(LayerPathString);
				if (UObject* LoadedLayer = LayerPath.TryLoad())
				{
					Layer = Cast<UVoxelVolumeLayer>(LoadedLayer);
					if (!Layer && Settings->bEnableDebugLogging)
					{
						UE_LOG(LogPCG, Warning, TEXT("PCGSpawnVoxelSculptActor: Loaded object is not a UVoxelVolumeLayer: %s"), *LayerPathString);
					}
				}
			}
		}
	}
	Stamp->Layer = Layer;

	// Configure BlendMode
	uint8 BlendMode = Settings->DefaultVolumeBlendMode;
	if (Settings->VolumeBlendModeAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* BlendModeAttribute = Metadata->GetConstAttribute(Settings->VolumeBlendModeAttribute);
		if (BlendModeAttribute)
		{
			if (BlendModeAttribute->GetTypeId() == PCG::Private::MetadataTypes<int32>::Id)
			{
				const FPCGMetadataAttribute<int32>* IntAttribute = static_cast<const FPCGMetadataAttribute<int32>*>(BlendModeAttribute);
				BlendMode = static_cast<uint8>(IntAttribute->GetValueFromItemKey(Point.MetadataEntry));
			}
			else if (BlendModeAttribute->GetTypeId() == PCG::Private::MetadataTypes<int64>::Id)
			{
				const FPCGMetadataAttribute<int64>* Int64Attribute = static_cast<const FPCGMetadataAttribute<int64>*>(BlendModeAttribute);
				BlendMode = static_cast<uint8>(Int64Attribute->GetValueFromItemKey(Point.MetadataEntry));
			}
		}
	}
	Stamp->BlendMode = static_cast<EVoxelVolumeBlendMode>(BlendMode);

	// Configure BoundsExtensionMultiplier
	float BoundsExtensionMultiplier = Settings->DefaultBoundsExtensionMultiplier;
	if (Settings->BoundsExtensionMultiplierAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* ExtensionAttribute = Metadata->GetConstAttribute(Settings->BoundsExtensionMultiplierAttribute);
		if (ExtensionAttribute)
		{
			if (ExtensionAttribute->GetTypeId() == PCG::Private::MetadataTypes<float>::Id)
			{
				const FPCGMetadataAttribute<float>* FloatAttribute = static_cast<const FPCGMetadataAttribute<float>*>(ExtensionAttribute);
				BoundsExtensionMultiplier = FloatAttribute->GetValueFromItemKey(Point.MetadataEntry);
			}
			else if (ExtensionAttribute->GetTypeId() == PCG::Private::MetadataTypes<double>::Id)
			{
				const FPCGMetadataAttribute<double>* DoubleAttribute = static_cast<const FPCGMetadataAttribute<double>*>(ExtensionAttribute);
				BoundsExtensionMultiplier = static_cast<float>(DoubleAttribute->GetValueFromItemKey(Point.MetadataEntry));
			}
		}
	}
	Stamp->BoundsExtensionMultiplier = FMath::Clamp(BoundsExtensionMultiplier, 0.0f, 5.0f);

	// Configure MaximumBoundsExtension
	float MaximumBoundsExtension = Settings->DefaultMaximumBoundsExtension;
	if (Settings->MaximumBoundsExtensionAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* MaxExtensionAttribute = Metadata->GetConstAttribute(Settings->MaximumBoundsExtensionAttribute);
		if (MaxExtensionAttribute)
		{
			if (MaxExtensionAttribute->GetTypeId() == PCG::Private::MetadataTypes<float>::Id)
			{
				const FPCGMetadataAttribute<float>* FloatAttribute = static_cast<const FPCGMetadataAttribute<float>*>(MaxExtensionAttribute);
				MaximumBoundsExtension = FloatAttribute->GetValueFromItemKey(Point.MetadataEntry);
			}
			else if (MaxExtensionAttribute->GetTypeId() == PCG::Private::MetadataTypes<double>::Id)
			{
				const FPCGMetadataAttribute<double>* DoubleAttribute = static_cast<const FPCGMetadataAttribute<double>*>(MaxExtensionAttribute);
				MaximumBoundsExtension = static_cast<float>(DoubleAttribute->GetValueFromItemKey(Point.MetadataEntry));
			}
		}
	}
	Stamp->MaximumBoundsExtension = FMath::Max(0.0f, MaximumBoundsExtension);

	// Configure Scale
	float Scale = Settings->DefaultScale;
	if (Settings->ScaleAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* ScaleAttribute = Metadata->GetConstAttribute(Settings->ScaleAttribute);
		if (ScaleAttribute)
		{
			if (ScaleAttribute->GetTypeId() == PCG::Private::MetadataTypes<float>::Id)
			{
				const FPCGMetadataAttribute<float>* FloatAttribute = static_cast<const FPCGMetadataAttribute<float>*>(ScaleAttribute);
				Scale = FloatAttribute->GetValueFromItemKey(Point.MetadataEntry);
			}
			else if (ScaleAttribute->GetTypeId() == PCG::Private::MetadataTypes<double>::Id)
			{
				const FPCGMetadataAttribute<double>* DoubleAttribute = static_cast<const FPCGMetadataAttribute<double>*>(ScaleAttribute);
				Scale = static_cast<float>(DoubleAttribute->GetValueFromItemKey(Point.MetadataEntry));
			}
		}
	}
	Stamp->Scale = Scale;

	// Configure bStoreMovableDistances
	bool bStoreMovableDistances = Settings->bDefaultStoreMovableDistances;
	if (Settings->StoreMovableDistancesAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* StoreAttribute = Metadata->GetConstAttribute(Settings->StoreMovableDistancesAttribute);
		if (StoreAttribute && StoreAttribute->GetTypeId() == PCG::Private::MetadataTypes<bool>::Id)
		{
			const FPCGMetadataAttribute<bool>* BoolAttribute = static_cast<const FPCGMetadataAttribute<bool>*>(StoreAttribute);
			bStoreMovableDistances = BoolAttribute->GetValueFromItemKey(Point.MetadataEntry);
		}
	}
	Stamp->bStoreMovableDistances = bStoreMovableDistances;

	// Configure bIsInfinite
	bool bIsInfinite = Settings->bDefaultIsInfinite;
	if (Settings->IsInfiniteAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* InfiniteAttribute = Metadata->GetConstAttribute(Settings->IsInfiniteAttribute);
		if (InfiniteAttribute && InfiniteAttribute->GetTypeId() == PCG::Private::MetadataTypes<bool>::Id)
		{
			const FPCGMetadataAttribute<bool>* BoolAttribute = static_cast<const FPCGMetadataAttribute<bool>*>(InfiniteAttribute);
			bIsInfinite = BoolAttribute->GetValueFromItemKey(Point.MetadataEntry);
		}
	}
	Stamp->bIsInfinite = bIsInfinite;

	// Configure MaxErrorPercentage
	float MaxErrorPercentage = Settings->DefaultVolumeMaxErrorPercentage;
	if (Settings->VolumeMaxErrorPercentageAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* ErrorAttribute = Metadata->GetConstAttribute(Settings->VolumeMaxErrorPercentageAttribute);
		if (ErrorAttribute)
		{
			if (ErrorAttribute->GetTypeId() == PCG::Private::MetadataTypes<float>::Id)
			{
				const FPCGMetadataAttribute<float>* FloatAttribute = static_cast<const FPCGMetadataAttribute<float>*>(ErrorAttribute);
				MaxErrorPercentage = FloatAttribute->GetValueFromItemKey(Point.MetadataEntry);
			}
			else if (ErrorAttribute->GetTypeId() == PCG::Private::MetadataTypes<double>::Id)
			{
				const FPCGMetadataAttribute<double>* DoubleAttribute = static_cast<const FPCGMetadataAttribute<double>*>(ErrorAttribute);
				MaxErrorPercentage = static_cast<float>(DoubleAttribute->GetValueFromItemKey(Point.MetadataEntry));
			}
		}
	}
	Stamp->MaxErrorPercentage = FMath::Clamp(MaxErrorPercentage, 0.0f, 1.0f);

	// Configure NearMaxLOD
	int32 NearMaxLOD = Settings->DefaultVolumeNearMaxLOD;
	if (Settings->VolumeNearMaxLODAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* LODAttribute = Metadata->GetConstAttribute(Settings->VolumeNearMaxLODAttribute);
		if (LODAttribute)
		{
			if (LODAttribute->GetTypeId() == PCG::Private::MetadataTypes<int32>::Id)
			{
				const FPCGMetadataAttribute<int32>* IntAttribute = static_cast<const FPCGMetadataAttribute<int32>*>(LODAttribute);
				NearMaxLOD = IntAttribute->GetValueFromItemKey(Point.MetadataEntry);
			}
			else if (LODAttribute->GetTypeId() == PCG::Private::MetadataTypes<int64>::Id)
			{
				const FPCGMetadataAttribute<int64>* Int64Attribute = static_cast<const FPCGMetadataAttribute<int64>*>(LODAttribute);
				NearMaxLOD = static_cast<int32>(Int64Attribute->GetValueFromItemKey(Point.MetadataEntry));
			}
		}
	}
	Stamp->NearMaxLOD = NearMaxLOD;

	// Configure MidMaxLOD
	int32 MidMaxLOD = Settings->DefaultVolumeMidMaxLOD;
	if (Settings->VolumeMidMaxLODAttribute != NAME_None)
	{
		const FPCGMetadataAttributeBase* LODAttribute = Metadata->GetConstAttribute(Settings->VolumeMidMaxLODAttribute);
		if (LODAttribute)
		{
			if (LODAttribute->GetTypeId() == PCG::Private::MetadataTypes<int32>::Id)
			{
				const FPCGMetadataAttribute<int32>* IntAttribute = static_cast<const FPCGMetadataAttribute<int32>*>(LODAttribute);
				MidMaxLOD = IntAttribute->GetValueFromItemKey(Point.MetadataEntry);
			}
			else if (LODAttribute->GetTypeId() == PCG::Private::MetadataTypes<int64>::Id)
			{
				const FPCGMetadataAttribute<int64>* Int64Attribute = static_cast<const FPCGMetadataAttribute<int64>*>(LODAttribute);
				MidMaxLOD = static_cast<int32>(Int64Attribute->GetValueFromItemKey(Point.MetadataEntry));
			}
		}
	}
	Stamp->MidMaxLOD = MidMaxLOD;
}

TArray<FName> FPCGSpawnVoxelSculptActorElement::GetNewActorTags(
	FPCGContext* Context,
	AActor* TargetActor,
	const UPCGSpawnVoxelSculptActorSettings* Settings) const
{
	TArray<FName> NewActorTags;

	// Inherit tags from target actor if requested
	if (Settings->bInheritActorTags && TargetActor)
	{
		NewActorTags = TargetActor->Tags;
	}

	// Always add the default PCG actor tag
	NewActorTags.AddUnique(PCGHelpers::DefaultPCGActorTag);

	// Add any additional tags specified in settings
	for (const FName& AdditionalTag : Settings->TagsToAddOnActors)
	{
		NewActorTags.AddUnique(AdditionalTag);
	}

	return NewActorTags;
}
