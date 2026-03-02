// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PCGSpawnActorWithAttributes.h"
#include "PCGContext.h"
#include "PCGComponent.h"
#include "Helpers/PCGHelpers.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGActorHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "PCGManagedResource.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

UPCGSpawnActorWithAttributesSettings::UPCGSpawnActorWithAttributesSettings()
{
	ActorClass = nullptr;
	ActorClassAttribute = NAME_None;
}

FPCGElementPtr UPCGSpawnActorWithAttributesSettings::CreateElement() const
{
	return MakeShared<FPCGSpawnActorWithAttributesElement>();
}

FPCGContext* FPCGSpawnActorWithAttributesElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGContext* Context = new FPCGContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;
	return Context;
}

bool FPCGSpawnActorWithAttributesElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSpawnActorWithAttributesElement::Execute);

	check(Context);

	// Ensure we're on the game thread for actor spawning
	if (!IsInGameThread())
	{
		PCGE_LOG(Error, GraphAndLog, NSLOCTEXT("PCGSpawnActorWithAttributes", "NotGameThread", "SpawnActorWithAttributes must execute on the game thread"));
		return true;
	}

	const UPCGSpawnActorWithAttributesSettings* Settings = Context->GetInputSettings<UPCGSpawnActorWithAttributesSettings>();
	check(Settings);

	if (!Settings->ActorClass.IsValid() && Settings->ActorClassAttribute == NAME_None)
	{
		PCGE_LOG(Error, GraphAndLog, NSLOCTEXT("PCGSpawnActorWithAttributes", "NoActorClass", "No actor class specified"));
		return true;
	}

	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (!SourceComponent)
	{
		PCGE_LOG(Error, GraphAndLog, NSLOCTEXT("PCGSpawnActorWithAttributes", "InvalidSourceComponent", "Invalid source component"));
		return true;
	}

	UWorld* World = SourceComponent->GetWorld();
	if (!World)
	{
		PCGE_LOG(Error, GraphAndLog, NSLOCTEXT("PCGSpawnActorWithAttributes", "InvalidWorld", "Invalid world"));
		return true;
	}

	// Process all input point data
	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	int32 TotalSpawnedActors = 0;
	TArray<TSoftObjectPtr<AActor>> AllSpawnedActors; // Collect all spawned actors

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
			PCGE_LOG(Warning, GraphAndLog, NSLOCTEXT("PCGSpawnActorWithAttributes", "NoMetadata", "Point data has no metadata"));
			continue;
		}

		// Iterate through all points
		for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
		{
			const FPCGPoint& Point = Points[PointIndex];
			FTransform PointTransform = Point.Transform;

			// Determine which actor class to spawn
			UClass* ClassToSpawn = nullptr;

			// Check if we should use an attribute for the class
			if (Settings->ActorClassAttribute != NAME_None)
			{
				// Try to get class from attribute
				const FPCGMetadataAttributeBase* ClassAttribute = Metadata->GetConstAttribute(Settings->ActorClassAttribute);
				if (ClassAttribute)
				{
					// Cast to typed attribute for FSoftClassPath
					if (const FPCGMetadataAttribute<FSoftClassPath>* TypedAttribute = static_cast<const FPCGMetadataAttribute<FSoftClassPath>*>(ClassAttribute))
					{
						FSoftClassPath ClassPath = TypedAttribute->GetValueFromItemKey(Point.MetadataEntry);
						ClassToSpawn = ClassPath.TryLoadClass<AActor>();
					}
				}
			}

			// Fallback to the default actor class
			if (!ClassToSpawn && Settings->ActorClass.IsValid())
			{
				ClassToSpawn = Settings->ActorClass.LoadSynchronous();
			}

			if (!ClassToSpawn)
			{
				PCGE_LOG(Warning, GraphAndLog, FText::Format(
					NSLOCTEXT("PCGSpawnActorWithAttributes", "NoValidClass", "No valid actor class found for point {0}"),
					FText::AsNumber(PointIndex)));
				continue;
			}

			// Spawn the actor
			AActor* SpawnedActor = SpawnActorWithAttributes(Context, Settings, ClassToSpawn, PointTransform, Metadata, Point);

			if (SpawnedActor)
			{
				TotalSpawnedActors++;
				AllSpawnedActors.Add(SpawnedActor); // Add to collection

				// Handle parenting if requested
				if (Settings->bAttachToParent && Settings->ParentActor.IsValid())
				{
					AActor* ParentActor = Settings->ParentActor.Get();
					if (ParentActor)
					{
						SpawnedActor->AttachToActor(ParentActor, FAttachmentTransformRules::KeepWorldTransform);
					}
				}
			}
		}
	}

	// Register all spawned actors with the PCG component for tracking (ONE managed resource for all actors)
	if (AllSpawnedActors.Num() > 0)
	{
		UPCGManagedActors* ManagedActors = NewObject<UPCGManagedActors>(SourceComponent);
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
			ManagedActors->GeneratedActors.Append(AllSpawnedActors);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
			SourceComponent->AddToManagedResources(ManagedActors);
	}

	PCGE_LOG(Verbose, LogOnly, FText::Format(
		NSLOCTEXT("PCGSpawnActorWithAttributes", "SpawnComplete", "Spawned {0} actors"),
		FText::AsNumber(TotalSpawnedActors)));

	return true;
}

AActor* FPCGSpawnActorWithAttributesElement::SpawnActorWithAttributes(
	FPCGContext* Context,
	const UPCGSpawnActorWithAttributesSettings* Settings,
	UClass* ActorClass,
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

	// Spawn the actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.bNoFail = true;
	SpawnParams.Owner = SourceComponent->GetOwner();

	AActor* SpawnedActor = World->SpawnActor<AActor>(ActorClass, Transform, SpawnParams);
	if (!SpawnedActor)
	{
		PCGE_LOG(Warning, GraphAndLog, FText::Format(
			NSLOCTEXT("PCGSpawnActorWithAttributes", "SpawnFailed", "Failed to spawn actor of class {0}"),
			FText::FromString(ActorClass->GetName())));
		return nullptr;
	}

	// Tag actor as PCG-generated for proper tracking and cleanup
	SpawnedActor->Tags.AddUnique(PCGHelpers::DefaultPCGTag);

	// Attach to PCG component's owner for proper cleanup
	AActor* ParentActor = SourceComponent->GetOwner();
	if (ParentActor)
	{
		SpawnedActor->AttachToActor(ParentActor, FAttachmentTransformRules::KeepWorldTransform);
	}

	if (!Metadata)
	{
		return SpawnedActor;
	}

	// Get all attributes from the metadata
	TArray<FName> AttributeNames;
	TArray<EPCGMetadataTypes> AttributeTypes;
	Metadata->GetAttributes(AttributeNames, AttributeTypes);

	int32 MatchedProperties = 0;

	// Iterate through all actor properties
	for (TFieldIterator<FProperty> PropIt(ActorClass); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!Property)
		{
			continue;
		}

		// Check if property is editable
		if (Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance) ||
			!Property->HasAnyPropertyFlags(CPF_Edit))
		{
			continue;
		}

		FName PropertyName = Property->GetFName();

		// Check if we have a matching attribute
		const FPCGMetadataAttributeBase* Attribute = Metadata->GetConstAttribute(PropertyName);
		if (!Attribute)
		{
			continue;
		}

		// Try to set the property from the attribute
		if (TrySetPropertyFromAttribute(Context, SpawnedActor, Property, Metadata, Point, PropertyName, Settings->bEnableDebugLogging))
		{
			MatchedProperties++;
		}
	}

	if (Settings->bEnableDebugLogging && MatchedProperties > 0)
	{
		PCGE_LOG(Verbose, LogOnly, FText::Format(
			NSLOCTEXT("PCGSpawnActorWithAttributes", "PropertiesMatched", "Matched {0} properties for actor {1}"),
			FText::AsNumber(MatchedProperties),
			FText::FromString(SpawnedActor->GetName())));
	}

	return SpawnedActor;
}

bool FPCGSpawnActorWithAttributesElement::TrySetPropertyFromAttribute(
	FPCGContext* Context,
	AActor* Actor,
	FProperty* Property,
	const UPCGMetadata* Metadata,
	const FPCGPoint& Point,
	const FName& AttributeName,
	bool bEnableLogging) const
{
	const FPCGMetadataAttributeBase* Attribute = Metadata->GetConstAttribute(AttributeName);

	if (!Attribute)
	{
		return false;
	}

	bool bSuccess = false;

	// Match by type and set the property
	if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		if (const FPCGMetadataAttribute<float>* TypedAttribute = static_cast<const FPCGMetadataAttribute<float>*>(Attribute))
		{
			float Value = TypedAttribute->GetValueFromItemKey(Point.MetadataEntry);
			bSuccess = SetFloatProperty(Actor, Property, Value);
		}
	}
	else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
	{
		if (const FPCGMetadataAttribute<double>* TypedAttribute = static_cast<const FPCGMetadataAttribute<double>*>(Attribute))
		{
			double Value = TypedAttribute->GetValueFromItemKey(Point.MetadataEntry);
			bSuccess = SetDoubleProperty(Actor, Property, Value);
		}
		else if (const FPCGMetadataAttribute<float>* FloatTypedAttribute = static_cast<const FPCGMetadataAttribute<float>*>(Attribute))
		{
			float Value = FloatTypedAttribute->GetValueFromItemKey(Point.MetadataEntry);
			bSuccess = SetDoubleProperty(Actor, Property, static_cast<double>(Value));
		}
	}
	else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		if (const FPCGMetadataAttribute<int32>* TypedAttribute = static_cast<const FPCGMetadataAttribute<int32>*>(Attribute))
		{
			int32 Value = TypedAttribute->GetValueFromItemKey(Point.MetadataEntry);
			bSuccess = SetIntProperty(Actor, Property, Value);
		}
	}
	else if (FInt64Property* Int64Prop = CastField<FInt64Property>(Property))
	{
		if (const FPCGMetadataAttribute<int64>* TypedAttribute = static_cast<const FPCGMetadataAttribute<int64>*>(Attribute))
		{
			int64 Value = TypedAttribute->GetValueFromItemKey(Point.MetadataEntry);
			bSuccess = SetInt64Property(Actor, Property, Value);
		}
	}
	else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		if (const FPCGMetadataAttribute<bool>* TypedAttribute = static_cast<const FPCGMetadataAttribute<bool>*>(Attribute))
		{
			bool Value = TypedAttribute->GetValueFromItemKey(Point.MetadataEntry);
			bSuccess = SetBoolProperty(Actor, Property, Value);
		}
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		if (StructProp->Struct == TBaseStructure<FVector>::Get())
		{
			if (const FPCGMetadataAttribute<FVector>* TypedAttribute = static_cast<const FPCGMetadataAttribute<FVector>*>(Attribute))
			{
				FVector Value = TypedAttribute->GetValueFromItemKey(Point.MetadataEntry);
				bSuccess = SetVectorProperty(Actor, Property, Value);
			}
		}
		else if (StructProp->Struct == TBaseStructure<FVector4>::Get())
		{
			if (const FPCGMetadataAttribute<FVector4>* TypedAttribute = static_cast<const FPCGMetadataAttribute<FVector4>*>(Attribute))
			{
				FVector4 Value = TypedAttribute->GetValueFromItemKey(Point.MetadataEntry);
				bSuccess = SetVector4Property(Actor, Property, Value);
			}
		}
		else if (StructProp->Struct == TBaseStructure<FQuat>::Get())
		{
			if (const FPCGMetadataAttribute<FQuat>* TypedAttribute = static_cast<const FPCGMetadataAttribute<FQuat>*>(Attribute))
			{
				FQuat Value = TypedAttribute->GetValueFromItemKey(Point.MetadataEntry);
				bSuccess = SetQuatProperty(Actor, Property, Value);
			}
		}
		else if (StructProp->Struct == TBaseStructure<FRotator>::Get())
		{
			if (const FPCGMetadataAttribute<FRotator>* TypedAttribute = static_cast<const FPCGMetadataAttribute<FRotator>*>(Attribute))
			{
				FRotator Value = TypedAttribute->GetValueFromItemKey(Point.MetadataEntry);
				bSuccess = SetRotatorProperty(Actor, Property, Value);
			}
		}
	}
	else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		if (const FPCGMetadataAttribute<FString>* TypedAttribute = static_cast<const FPCGMetadataAttribute<FString>*>(Attribute))
		{
			FString Value = TypedAttribute->GetValueFromItemKey(Point.MetadataEntry);
			bSuccess = SetStringProperty(Actor, Property, Value);
		}
	}
	else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		if (const FPCGMetadataAttribute<FName>* TypedAttribute = static_cast<const FPCGMetadataAttribute<FName>*>(Attribute))
		{
			FName Value = TypedAttribute->GetValueFromItemKey(Point.MetadataEntry);
			bSuccess = SetNameProperty(Actor, Property, Value);
		}
	}

	if (bSuccess && bEnableLogging)
	{
		PCGE_LOG(Verbose, LogOnly, FText::Format(
			NSLOCTEXT("PCGSpawnActorWithAttributes", "PropertySet", "Set property '{0}' on actor '{1}'"),
			FText::FromName(AttributeName),
			FText::FromString(Actor->GetName())));
	}

	return bSuccess;
}

bool FPCGSpawnActorWithAttributesElement::SetFloatProperty(AActor* Actor, FProperty* Property, float Value) const
{
	FFloatProperty* FloatProp = CastField<FFloatProperty>(Property);
	if (FloatProp)
	{
		void* PropertyAddress = FloatProp->ContainerPtrToValuePtr<void>(Actor);
		FloatProp->SetPropertyValue(PropertyAddress, Value);
		return true;
	}
	return false;
}

bool FPCGSpawnActorWithAttributesElement::SetDoubleProperty(AActor* Actor, FProperty* Property, double Value) const
{
	FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property);
	if (DoubleProp)
	{
		void* PropertyAddress = DoubleProp->ContainerPtrToValuePtr<void>(Actor);
		DoubleProp->SetPropertyValue(PropertyAddress, Value);
		return true;
	}
	return false;
}

bool FPCGSpawnActorWithAttributesElement::SetIntProperty(AActor* Actor, FProperty* Property, int32 Value) const
{
	FIntProperty* IntProp = CastField<FIntProperty>(Property);
	if (IntProp)
	{
		void* PropertyAddress = IntProp->ContainerPtrToValuePtr<void>(Actor);
		IntProp->SetPropertyValue(PropertyAddress, Value);
		return true;
	}
	return false;
}

bool FPCGSpawnActorWithAttributesElement::SetInt64Property(AActor* Actor, FProperty* Property, int64 Value) const
{
	FInt64Property* Int64Prop = CastField<FInt64Property>(Property);
	if (Int64Prop)
	{
		void* PropertyAddress = Int64Prop->ContainerPtrToValuePtr<void>(Actor);
		Int64Prop->SetPropertyValue(PropertyAddress, Value);
		return true;
	}
	return false;
}

bool FPCGSpawnActorWithAttributesElement::SetBoolProperty(AActor* Actor, FProperty* Property, bool Value) const
{
	FBoolProperty* BoolProp = CastField<FBoolProperty>(Property);
	if (BoolProp)
	{
		void* PropertyAddress = BoolProp->ContainerPtrToValuePtr<void>(Actor);
		BoolProp->SetPropertyValue(PropertyAddress, Value);
		return true;
	}
	return false;
}

bool FPCGSpawnActorWithAttributesElement::SetVectorProperty(AActor* Actor, FProperty* Property, const FVector& Value) const
{
	FStructProperty* StructProp = CastField<FStructProperty>(Property);
	if (StructProp && StructProp->Struct == TBaseStructure<FVector>::Get())
	{
		void* PropertyAddress = StructProp->ContainerPtrToValuePtr<void>(Actor);
		*static_cast<FVector*>(PropertyAddress) = Value;
		return true;
	}
	return false;
}

bool FPCGSpawnActorWithAttributesElement::SetVector4Property(AActor* Actor, FProperty* Property, const FVector4& Value) const
{
	FStructProperty* StructProp = CastField<FStructProperty>(Property);
	if (StructProp && StructProp->Struct == TBaseStructure<FVector4>::Get())
	{
		void* PropertyAddress = StructProp->ContainerPtrToValuePtr<void>(Actor);
		*static_cast<FVector4*>(PropertyAddress) = Value;
		return true;
	}
	return false;
}

bool FPCGSpawnActorWithAttributesElement::SetQuatProperty(AActor* Actor, FProperty* Property, const FQuat& Value) const
{
	FStructProperty* StructProp = CastField<FStructProperty>(Property);
	if (StructProp && StructProp->Struct == TBaseStructure<FQuat>::Get())
	{
		void* PropertyAddress = StructProp->ContainerPtrToValuePtr<void>(Actor);
		*static_cast<FQuat*>(PropertyAddress) = Value;
		return true;
	}
	return false;
}

bool FPCGSpawnActorWithAttributesElement::SetRotatorProperty(AActor* Actor, FProperty* Property, const FRotator& Value) const
{
	FStructProperty* StructProp = CastField<FStructProperty>(Property);
	if (StructProp && StructProp->Struct == TBaseStructure<FRotator>::Get())
	{
		void* PropertyAddress = StructProp->ContainerPtrToValuePtr<void>(Actor);
		*static_cast<FRotator*>(PropertyAddress) = Value;
		return true;
	}
	return false;
}

bool FPCGSpawnActorWithAttributesElement::SetStringProperty(AActor* Actor, FProperty* Property, const FString& Value) const
{
	FStrProperty* StrProp = CastField<FStrProperty>(Property);
	if (StrProp)
	{
		void* PropertyAddress = StrProp->ContainerPtrToValuePtr<void>(Actor);
		StrProp->SetPropertyValue(PropertyAddress, Value);
		return true;
	}
	return false;
}

bool FPCGSpawnActorWithAttributesElement::SetNameProperty(AActor* Actor, FProperty* Property, const FName& Value) const
{
	FNameProperty* NameProp = CastField<FNameProperty>(Property);
	if (NameProp)
	{
		void* PropertyAddress = NameProp->ContainerPtrToValuePtr<void>(Actor);
		NameProp->SetPropertyValue(PropertyAddress, Value);
		return true;
	}
	return false;
}