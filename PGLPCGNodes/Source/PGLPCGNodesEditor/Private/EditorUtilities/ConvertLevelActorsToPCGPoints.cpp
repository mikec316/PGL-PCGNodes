// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "ConvertLevelActorsToPCGPoints.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "PCGContext.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/Info.h"
#include "Engine/Brush.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "VoxelStampActor.h"
#include "VoxelStamp.h"
#include "VoxelHeightStamp.h"
#include "VoxelStampComponent.h"
#include "VoxelGraph.h"
#include "VoxelParameter.h"
#include "VoxelPinType.h"
#include "VoxelPinValue.h"
#include "VoxelParameterOverridesOwner.h"
#include "Sculpt/VoxelSculptActorBase.h"

// Helper struct to group actors by class and graph
struct FActorGroupKey
{
    UClass* ActorClass = nullptr;
    UVoxelGraph* VoxelGraph = nullptr;

    bool operator==(const FActorGroupKey& Other) const
    {
        return ActorClass == Other.ActorClass && VoxelGraph == Other.VoxelGraph;
    }

    friend uint32 GetTypeHash(const FActorGroupKey& Key)
    {
        return HashCombine(GetTypeHash(Key.ActorClass), GetTypeHash(Key.VoxelGraph));
    }
};

// Helper function to remove _C suffix from Blueprint class names
static FString GetCleanClassName(UClass* Class)
{
    if (!Class)
    {
        return FString();
    }

    FString ClassName = Class->GetName();

    // Remove _C suffix from Blueprint classes
    if (ClassName.EndsWith(TEXT("_C")))
    {
        ClassName.LeftChopInline(2);
    }

    return ClassName;
}


TArray<FPCGTaggedData> UConvertLevelActorsToPCGPoints::ConvertLevelActorsToPCGPoints(UObject* WorldContextObject)
{
    TArray<FPCGTaggedData> OutputData;

    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("ConvertLevelActorsToPCGPoints: No valid world found"));
        return OutputData;
    }

    // Step 1: Build a list of actors grouped by class AND voxel graph (if applicable)
    TMap<FActorGroupKey, TArray<AActor*>> ActorsByGroup;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor && IsValid(Actor))
        {
            // Exclude actors with the "PCGExclude" tag
            if (Actor->Tags.Contains(FName("PCGExclude")))
            {
                continue;
            }

            // Skip WorldSettings, Info actors, and Brushes
            if (Actor->IsA(AWorldSettings::StaticClass()) ||
                Actor->IsA(AInfo::StaticClass()) ||
                Actor->IsA(ABrush::StaticClass()))
            {
                continue;
            }

            // Only include actors that were placed in the editor (not runtime spawned)
            if (!Actor->IsActorBeingDestroyed() &&
                Actor->GetLevel() &&
                !Actor->bIsEditorPreviewActor &&
                Actor->IsEditable() &&
                !Actor->IsTemplate() &&
                !Actor->HasAnyFlags(RF_Transient))
            {
                UClass* ActorClass = Actor->GetClass();

                FActorGroupKey GroupKey;
                GroupKey.ActorClass = ActorClass;

                // If this is a VoxelStamp actor, get its graph for grouping
                if (ActorClass->IsChildOf(AVoxelStampActor::StaticClass()))
                {
                    AVoxelStampActor* StampActor = Cast<AVoxelStampActor>(Actor);
                    if (StampActor)
                    {
                        FVoxelStampRef StampRef = StampActor->GetStamp();
                        if (StampRef.IsValid())
                        {
                            const FVoxelStamp* Stamp = StampRef.operator->();
                            if (UObject* GraphAsset = Stamp->GetAsset())
                            {
                                GroupKey.VoxelGraph = Cast<UVoxelGraph>(GraphAsset);
                            }
                        }
                    }
                }
                // VoxelSculptActorBase doesn't have graphs, so just group by class

                ActorsByGroup.FindOrAdd(GroupKey).Add(Actor);
            }
        }
    }

    // Step 2-8: For each unique actor group (class + graph combination)
    for (const TPair<FActorGroupKey, TArray<AActor*>>& GroupPair : ActorsByGroup)
    {
        const FActorGroupKey& GroupKey = GroupPair.Key;
        UClass* ActorClass = GroupKey.ActorClass;
        UVoxelGraph* VoxelGraph = GroupKey.VoxelGraph;
        const TArray<AActor*>& Actors = GroupPair.Value;

        if (Actors.Num() == 0)
            continue;

        // Step 2: Create a new PCGPointData object
        UPCGPointData* PointData = NewObject<UPCGPointData>();

        // Step 3: Create/Get PCGMetadata Object
        UPCGMetadata* Metadata = PointData->MutableMetadata();
        check(Metadata);

        // Step 4: Build list of overridable properties and create attributes
        TMap<FName, FProperty*> PropertyMap;

        // Categories to exclude
        TArray<FName> ExcludedCategories = {
            FName(TEXT("Rendering")),
            FName(TEXT("Collision")),
            FName(TEXT("WorldPartition")),
            FName(TEXT("HLOD")),
            FName(TEXT("Cooking")),
            FName(TEXT("Physics")),
            FName(TEXT("Replication")),
            FName(TEXT("Actor")),
            FName(TEXT("Input")),
            FName(TEXT("Level Instance")),
            FName(TEXT("Networking"))
        };

        // Check if this is a VoxelStamp or VoxelSculpt actor class
        bool bIsVoxelStamp = ActorClass->IsChildOf(AVoxelStampActor::StaticClass());
        bool bIsVoxelSculpt = ActorClass->IsChildOf(AVoxelSculptActorBase::StaticClass());

        // Iterate through all properties of the actor class
        for (TFieldIterator<FProperty> PropIt(ActorClass); PropIt; ++PropIt)
        {
            FProperty* Property = *PropIt;

            if (Property && (Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible)))
            {
                FString CategoryString = Property->GetMetaData(TEXT("Category"));
                FName CategoryName = FName(*CategoryString);

                if (ExcludedCategories.Contains(CategoryName))
                {
                    continue;
                }

                FName PropertyName = Property->GetFName();
                PropertyMap.Add(PropertyName, Property);

                // Create attributes based on property type
                if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
                {
                    Metadata->CreateFloatAttribute(PropertyName, 0.0f, true);
                }
                else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
                {
                    Metadata->CreateDoubleAttribute(PropertyName, 0.0, true);
                }
                else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
                {
                    Metadata->CreateInteger32Attribute(PropertyName, 0, false);
                }
                else if (FInt64Property* Int64Prop = CastField<FInt64Property>(Property))
                {
                    Metadata->CreateInteger64Attribute(PropertyName, 0, false);
                }
                else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
                {
                    Metadata->CreateInteger32Attribute(PropertyName, 0, false);
                }
                else if (FStrProperty* StringProp = CastField<FStrProperty>(Property))
                {
                    Metadata->CreateStringAttribute(PropertyName, FString(), false);
                }
                else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
                {
                    Metadata->CreateNameAttribute(PropertyName, FName(), false);
                }
                else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
                {
                    Metadata->CreateInteger32Attribute(PropertyName, 0, false);
                }
                else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
                {
                    Metadata->CreateInteger32Attribute(PropertyName, 0, false);
                }
                else if (FObjectProperty* ObjectProp = CastField<FObjectProperty>(Property))
                {
                    Metadata->CreateAttribute<FSoftObjectPath>(PropertyName, FSoftObjectPath(), false, true);
                }
                else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
                {
                    if (StructProp->Struct == TBaseStructure<FVector>::Get())
                    {
                        Metadata->CreateVectorAttribute(PropertyName, FVector::ZeroVector, true);
                    }
                    else if (StructProp->Struct == TBaseStructure<FVector4>::Get())
                    {
                        Metadata->CreateVector4Attribute(PropertyName, FVector4::Zero(), true);
                    }
                    else if (StructProp->Struct == TBaseStructure<FVector2D>::Get())
                    {
                        Metadata->CreateVector2Attribute(PropertyName, FVector2D::ZeroVector, true);
                    }
                    else if (StructProp->Struct == TBaseStructure<FRotator>::Get())
                    {
                        Metadata->CreateRotatorAttribute(PropertyName, FRotator::ZeroRotator, true);
                    }
                    else if (StructProp->Struct == TBaseStructure<FQuat>::Get())
                    {
                        Metadata->CreateQuatAttribute(PropertyName, FQuat::Identity, true);
                    }
                    else if (StructProp->Struct == TBaseStructure<FTransform>::Get())
                    {
                        Metadata->CreateTransformAttribute(PropertyName, FTransform::Identity, true);
                    }
                    else if (StructProp->Struct == TBaseStructure<FDataTableRowHandle>::Get())
                    {
                        // Create two attributes: one for the DataTable path, one for the RowName
                        FName DataTableAttrName = FName(*(PropertyName.ToString() + TEXT("_DataTable")));
                        FName RowNameAttrName = FName(*(PropertyName.ToString() + TEXT("_RowName")));

                        Metadata->CreateStringAttribute(DataTableAttrName, FString(), false);
                        Metadata->CreateNameAttribute(RowNameAttrName, FName(), false);
                    }
                }
            }
        }

        // If this is a VoxelStamp, add voxel-specific attributes
        if (bIsVoxelStamp)
        {
            // FVoxelStamp properties
            Metadata->CreateInteger32Attribute(FName(TEXT("Stamp_Behavior")), 0, false);
            Metadata->CreateInteger32Attribute(FName(TEXT("Stamp_Priority")), 0, false);
            Metadata->CreateFloatAttribute(FName(TEXT("Stamp_Smoothness")), 0.0f, true);
            Metadata->CreateTransformAttribute(FName(TEXT("Stamp_Transform")), FTransform::Identity, true);

            // FVoxelHeightStamp properties (if applicable)
            Metadata->CreateInteger32Attribute(FName(TEXT("Stamp_BlendMode")), 0, false);
            Metadata->CreateStringAttribute(FName(TEXT("Stamp_Layer")), FString(), false);
            Metadata->CreateFloatAttribute(FName(TEXT("Stamp_HeightPaddingMultiplier")), 0.0f, true);
            Metadata->CreateStringAttribute(FName(TEXT("Stamp_Graph")), FString(), false);

            // If we have a specific VoxelGraph for this group, create attributes for its parameters
            if (VoxelGraph)
            {
                // Iterate through all parameters in the graph
                VoxelGraph->ForeachParameter([&](const FGuid& Guid, const FVoxelParameter& Parameter)
                    {
                        FName ParameterName = Parameter.Name;
                        FName PrefixedPropertyName = FName(*(FString(TEXT("GraphParam_")) + ParameterName.ToString()));

                        // Create attributes based on parameter type
                        const FVoxelPinType& PinType = Parameter.Type;

                        if (PinType.IsWildcard())
                        {
                            // Skip wildcard types
                            return;
                        }

                        const FVoxelPinType InnerType = PinType.GetInnerType();

                        if (InnerType == FVoxelPinType::Make<float>() || InnerType == FVoxelPinType::Make<double>())
                        {
                            Metadata->CreateFloatAttribute(PrefixedPropertyName, 0.0f, true);
                        }
                        else if (InnerType == FVoxelPinType::Make<int32>() || InnerType == FVoxelPinType::Make<int64>())
                        {
                            Metadata->CreateInteger32Attribute(PrefixedPropertyName, 0, false);
                        }
                        else if (InnerType == FVoxelPinType::Make<bool>())
                        {
                            Metadata->CreateInteger32Attribute(PrefixedPropertyName, 0, false);
                        }
                        else if (InnerType == FVoxelPinType::Make<FName>())
                        {
                            Metadata->CreateNameAttribute(PrefixedPropertyName, FName(), false);
                        }
                        // Skip FString - causes compilation issues
                        else if (InnerType == FVoxelPinType::Make<FVector>())
                        {
                            Metadata->CreateVectorAttribute(PrefixedPropertyName, FVector::ZeroVector, true);
                        }
                        else if (InnerType == FVoxelPinType::Make<FRotator>())
                        {
                            Metadata->CreateRotatorAttribute(PrefixedPropertyName, FRotator::ZeroRotator, true);
                        }
                        else if (InnerType == FVoxelPinType::Make<FLinearColor>() || InnerType == FVoxelPinType::Make<FColor>())
                        {
                            Metadata->CreateVector4Attribute(PrefixedPropertyName, FVector4::Zero(), true);
                        }
                    });
            }
        }

        // If this is a VoxelSculpt, add sculpt-specific attributes
        if (bIsVoxelSculpt)
        {
            // Store as FSoftObjectPath for proper asset reference tracking
            Metadata->CreateAttribute<FSoftObjectPath>(FName(TEXT("Sculpt_ExternalAsset")), FSoftObjectPath(), false, true);
        }

        // Collect all unique tag-based attributes from all actors of this class
        TSet<FName> TagAttributeNames;
        for (AActor* Actor : Actors)
        {
            if (!Actor)
                continue;

            for (const FName& Tag : Actor->Tags)
            {
                FString TagString = Tag.ToString();
                int32 ColonIndex;
                if (TagString.FindChar(':', ColonIndex))
                {
                    FString AttributeName = TagString.Left(ColonIndex);
                    TagAttributeNames.Add(FName(*AttributeName));
                }
            }
        }

        // Create float attributes for all tag-based attributes
        for (const FName& TagAttributeName : TagAttributeNames)
        {
            Metadata->CreateFloatAttribute(TagAttributeName, 1.0f, true);
        }

        // Create an attribute to store the actor class as a soft class path
        Metadata->CreateAttribute<FSoftClassPath>(FName(TEXT("ActorClass")), FSoftClassPath(), false, true);

        // Step 5: Create array of PCGPoints
        TArray<FPCGPoint> Points;
        Points.Reserve(Actors.Num());

        // Step 6: For each actor, create a point and set attributes
        for (AActor* Actor : Actors)
        {
            if (!Actor)
                continue;

            // Create PCGPoint using actor transform
            FPCGPoint& Point = Points.Add_GetRef(FPCGPoint());
            Point.Transform = Actor->GetActorTransform();
            Point.SetLocalBounds(Actor->GetComponentsBoundingBox(true));

            // Create metadata entry for this point
            PCGMetadataEntryKey EntryKey = Metadata->AddEntry();
            Point.MetadataEntry = EntryKey;

            // Set the ActorClass attribute as a soft class path
            FPCGMetadataAttribute<FSoftClassPath>* ActorClassAttr = static_cast<FPCGMetadataAttribute<FSoftClassPath>*>(Metadata->GetMutableAttribute(FName(TEXT("ActorClass"))));
            if (ActorClassAttr)
            {
                FSoftClassPath SoftClassPath(ActorClass);
                ActorClassAttr->SetValue(EntryKey, SoftClassPath);
            }

            // Set attribute values from actor properties
            for (const TPair<FName, FProperty*>& PropPair : PropertyMap)
            {
                FName PropertyName = PropPair.Key;
                FProperty* Property = PropPair.Value;

                void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Actor);

                if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
                {
                    float Value = FloatProp->GetPropertyValue(ValuePtr);
                    FPCGMetadataAttribute<float>* Attribute = static_cast<FPCGMetadataAttribute<float>*>(Metadata->GetMutableAttribute(PropertyName));
                    if (Attribute)
                    {
                        Attribute->SetValue(EntryKey, Value);
                    }
                }
                else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
                {
                    double Value = DoubleProp->GetPropertyValue(ValuePtr);
                    FPCGMetadataAttribute<double>* Attribute = static_cast<FPCGMetadataAttribute<double>*>(Metadata->GetMutableAttribute(PropertyName));
                    if (Attribute)
                    {
                        Attribute->SetValue(EntryKey, Value);
                    }
                }
                else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
                {
                    int32 Value = IntProp->GetPropertyValue(ValuePtr);
                    FPCGMetadataAttribute<int32>* Attribute = static_cast<FPCGMetadataAttribute<int32>*>(Metadata->GetMutableAttribute(PropertyName));
                    if (Attribute)
                    {
                        Attribute->SetValue(EntryKey, Value);
                    }
                }
                else if (FInt64Property* Int64Prop = CastField<FInt64Property>(Property))
                {
                    int64 Value = Int64Prop->GetPropertyValue(ValuePtr);
                    FPCGMetadataAttribute<int64>* Attribute = static_cast<FPCGMetadataAttribute<int64>*>(Metadata->GetMutableAttribute(PropertyName));
                    if (Attribute)
                    {
                        Attribute->SetValue(EntryKey, Value);
                    }
                }
                else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
                {
                    bool Value = BoolProp->GetPropertyValue(ValuePtr);
                    FPCGMetadataAttribute<int32>* Attribute = static_cast<FPCGMetadataAttribute<int32>*>(Metadata->GetMutableAttribute(PropertyName));
                    if (Attribute)
                    {
                        Attribute->SetValue(EntryKey, Value ? 1 : 0);
                    }
                }
                else if (FStrProperty* StringProp = CastField<FStrProperty>(Property))
                {
                    FString Value = StringProp->GetPropertyValue(ValuePtr);
                    FPCGMetadataAttribute<FString>* Attribute = static_cast<FPCGMetadataAttribute<FString>*>(Metadata->GetMutableAttribute(PropertyName));
                    if (Attribute)
                    {
                        Attribute->SetValue(EntryKey, Value);
                    }
                }
                else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
                {
                    FName Value = NameProp->GetPropertyValue(ValuePtr);
                    FPCGMetadataAttribute<FName>* Attribute = static_cast<FPCGMetadataAttribute<FName>*>(Metadata->GetMutableAttribute(PropertyName));
                    if (Attribute)
                    {
                        Attribute->SetValue(EntryKey, Value);
                    }
                }
                else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
                {
                    // Get enum value as integer
                    int64 EnumValue = EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
                    FPCGMetadataAttribute<int32>* Attribute = static_cast<FPCGMetadataAttribute<int32>*>(Metadata->GetMutableAttribute(PropertyName));
                    if (Attribute)
                    {
                        Attribute->SetValue(EntryKey, static_cast<int32>(EnumValue));
                    }
                }
                else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
                {
                    uint8 Value = ByteProp->GetPropertyValue(ValuePtr);
                    FPCGMetadataAttribute<int32>* Attribute = static_cast<FPCGMetadataAttribute<int32>*>(Metadata->GetMutableAttribute(PropertyName));
                    if (Attribute)
                    {
                        Attribute->SetValue(EntryKey, static_cast<int32>(Value));
                    }
                }
                else if (FObjectProperty* ObjectProp = CastField<FObjectProperty>(Property))
                {
                    UObject* ObjectValue = ObjectProp->GetObjectPropertyValue(ValuePtr);
                    FSoftObjectPath SoftObjectPath(ObjectValue);
                    FPCGMetadataAttribute<FSoftObjectPath>* Attribute = static_cast<FPCGMetadataAttribute<FSoftObjectPath>*>(Metadata->GetMutableAttribute(PropertyName));
                    if (Attribute)
                    {
                        Attribute->SetValue(EntryKey, SoftObjectPath);
                    }
                }
                else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
                {
                    if (StructProp->Struct == TBaseStructure<FVector>::Get())
                    {
                        FVector Value = *static_cast<FVector*>(ValuePtr);
                        FPCGMetadataAttribute<FVector>* Attribute = static_cast<FPCGMetadataAttribute<FVector>*>(Metadata->GetMutableAttribute(PropertyName));
                        if (Attribute)
                        {
                            Attribute->SetValue(EntryKey, Value);
                        }
                    }
                    else if (StructProp->Struct == TBaseStructure<FVector4>::Get())
                    {
                        FVector4 Value = *static_cast<FVector4*>(ValuePtr);
                        FPCGMetadataAttribute<FVector4>* Attribute = static_cast<FPCGMetadataAttribute<FVector4>*>(Metadata->GetMutableAttribute(PropertyName));
                        if (Attribute)
                        {
                            Attribute->SetValue(EntryKey, Value);
                        }
                    }
                    else if (StructProp->Struct == TBaseStructure<FVector2D>::Get())
                    {
                        FVector2D Value = *static_cast<FVector2D*>(ValuePtr);
                        FPCGMetadataAttribute<FVector2D>* Attribute = static_cast<FPCGMetadataAttribute<FVector2D>*>(Metadata->GetMutableAttribute(PropertyName));
                        if (Attribute)
                        {
                            Attribute->SetValue(EntryKey, Value);
                        }
                    }
                    else if (StructProp->Struct == TBaseStructure<FRotator>::Get())
                    {
                        FRotator Value = *static_cast<FRotator*>(ValuePtr);
                        FPCGMetadataAttribute<FRotator>* Attribute = static_cast<FPCGMetadataAttribute<FRotator>*>(Metadata->GetMutableAttribute(PropertyName));
                        if (Attribute)
                        {
                            Attribute->SetValue(EntryKey, Value);
                        }
                    }
                    else if (StructProp->Struct == TBaseStructure<FQuat>::Get())
                    {
                        FQuat Value = *static_cast<FQuat*>(ValuePtr);
                        FPCGMetadataAttribute<FQuat>* Attribute = static_cast<FPCGMetadataAttribute<FQuat>*>(Metadata->GetMutableAttribute(PropertyName));
                        if (Attribute)
                        {
                            Attribute->SetValue(EntryKey, Value);
                        }
                    }
                    else if (StructProp->Struct == TBaseStructure<FTransform>::Get())
                    {
                        FTransform Value = *static_cast<FTransform*>(ValuePtr);
                        FPCGMetadataAttribute<FTransform>* Attribute = static_cast<FPCGMetadataAttribute<FTransform>*>(Metadata->GetMutableAttribute(PropertyName));
                        if (Attribute)
                        {
                            Attribute->SetValue(EntryKey, Value);
                        }
                    }
                    else if (StructProp->Struct == TBaseStructure<FDataTableRowHandle>::Get())
                    {
                        const FDataTableRowHandle* RowHandle = static_cast<const FDataTableRowHandle*>(ValuePtr);

                        // Extract DataTable path
                        FString DataTablePath = FString(TEXT("None"));
                        if (RowHandle->DataTable)
                        {
                            DataTablePath = RowHandle->DataTable->GetPathName();
                        }

                        // Extract RowName
                        FName RowName = RowHandle->RowName;

                        // Set the DataTable attribute
                        FName DataTableAttrName = FName(*(PropertyName.ToString() + TEXT("_DataTable")));
                        FPCGMetadataAttribute<FString>* DataTableAttr = static_cast<FPCGMetadataAttribute<FString>*>(Metadata->GetMutableAttribute(DataTableAttrName));
                        if (DataTableAttr)
                        {
                            DataTableAttr->SetValue(EntryKey, DataTablePath);
                        }

                        // Set the RowName attribute
                        FName RowNameAttrName = FName(*(PropertyName.ToString() + TEXT("_RowName")));
                        FPCGMetadataAttribute<FName>* RowNameAttr = static_cast<FPCGMetadataAttribute<FName>*>(Metadata->GetMutableAttribute(RowNameAttrName));
                        if (RowNameAttr)
                        {
                            RowNameAttr->SetValue(EntryKey, RowName);
                        }
                    }
                }
            }

            // For VoxelStamp actors, get the stamp data
            if (bIsVoxelStamp)
            {
                AVoxelStampActor* StampActor = Cast<AVoxelStampActor>(Actor);
                if (StampActor)
                {
                    FVoxelStampRef StampRef = StampActor->GetStamp();

                    if (StampRef.IsValid())
                    {
                        const FVoxelStamp* Stamp = StampRef.operator->();

                        // Set FVoxelStamp properties
                        FPCGMetadataAttribute<int32>* BehaviorAttr = static_cast<FPCGMetadataAttribute<int32>*>(Metadata->GetMutableAttribute(FName(TEXT("Stamp_Behavior"))));
                        if (BehaviorAttr)
                        {
                            BehaviorAttr->SetValue(EntryKey, static_cast<int32>(Stamp->Behavior));
                        }

                        FPCGMetadataAttribute<int32>* PriorityAttr = static_cast<FPCGMetadataAttribute<int32>*>(Metadata->GetMutableAttribute(FName(TEXT("Stamp_Priority"))));
                        if (PriorityAttr)
                        {
                            PriorityAttr->SetValue(EntryKey, Stamp->Priority);
                        }

                        FPCGMetadataAttribute<float>* SmoothnessAttr = static_cast<FPCGMetadataAttribute<float>*>(Metadata->GetMutableAttribute(FName(TEXT("Stamp_Smoothness"))));
                        if (SmoothnessAttr)
                        {
                            SmoothnessAttr->SetValue(EntryKey, Stamp->Smoothness);
                        }

                        FPCGMetadataAttribute<FTransform>* TransformAttr = static_cast<FPCGMetadataAttribute<FTransform>*>(Metadata->GetMutableAttribute(FName(TEXT("Stamp_Transform"))));
                        if (TransformAttr)
                        {
                            TransformAttr->SetValue(EntryKey, Stamp->Transform);
                        }

                        // Check if it's a FVoxelHeightStamp
                        if (const FVoxelHeightStamp* HeightStamp = Stamp->As<FVoxelHeightStamp>())
                        {
                            FPCGMetadataAttribute<int32>* BlendModeAttr = static_cast<FPCGMetadataAttribute<int32>*>(Metadata->GetMutableAttribute(FName(TEXT("Stamp_BlendMode"))));
                            if (BlendModeAttr)
                            {
                                BlendModeAttr->SetValue(EntryKey, static_cast<int32>(HeightStamp->BlendMode));
                            }

                            FPCGMetadataAttribute<FString>* LayerAttr = static_cast<FPCGMetadataAttribute<FString>*>(Metadata->GetMutableAttribute(FName(TEXT("Stamp_Layer"))));
                            if (LayerAttr && HeightStamp->Layer)
                            {
                                LayerAttr->SetValue(EntryKey, HeightStamp->Layer->GetName());
                            }

                            FPCGMetadataAttribute<float>* HeightPaddingAttr = static_cast<FPCGMetadataAttribute<float>*>(Metadata->GetMutableAttribute(FName(TEXT("Stamp_HeightPaddingMultiplier"))));
                            if (HeightPaddingAttr)
                            {
                                HeightPaddingAttr->SetValue(EntryKey, HeightStamp->HeightPaddingMultiplier);
                            }
                        }

                        // Try to get graph from GetAsset() for any stamp type
                        if (UObject* GraphAsset = Stamp->GetAsset())
                        {
                            // Store graph name
                            FPCGMetadataAttribute<FString>* GraphAttr = static_cast<FPCGMetadataAttribute<FString>*>(Metadata->GetMutableAttribute(FName(TEXT("Stamp_Graph"))));
                            if (GraphAttr)
                            {
                                GraphAttr->SetValue(EntryKey, GraphAsset->GetName());
                            }

                            // Check if it's a UVoxelGraph and read parameters
                            if (UVoxelGraph* ActorVoxelGraph = Cast<UVoxelGraph>(GraphAsset))
                            {
                                FVoxelParameterOverrides* ParameterOverridesPtr = nullptr;

                                // The stamp struct has a ParameterOverrides property!
                                UScriptStruct* StampStruct = Stamp->GetStruct();

                                // Find the ParameterOverrides property on the stamp
                                FProperty* ParameterOverridesProperty = StampStruct->FindPropertyByName(FName(TEXT("ParameterOverrides")));

                                if (ParameterOverridesProperty)
                                {
                                    // Get the ParameterOverrides from the stamp instance
                                    void* PropertyValuePtr = ParameterOverridesProperty->ContainerPtrToValuePtr<void>(const_cast<FVoxelStamp*>(Stamp));

                                    // Cast to FVoxelParameterOverrides
                                    ParameterOverridesPtr = static_cast<FVoxelParameterOverrides*>(PropertyValuePtr);
                                }
                                else
                                {
                                    // Fallback to graph defaults
                                    ParameterOverridesPtr = &ActorVoxelGraph->GetParameterOverrides();
                                }

                                FVoxelParameterOverrides& ParameterOverrides = *ParameterOverridesPtr;

                                // Iterate through all parameters
                                ActorVoxelGraph->ForeachParameter([&](const FGuid& Guid, const FVoxelParameter& Parameter)
                                    {
                                        FName ParameterName = Parameter.Name;
                                        FName PrefixedPropertyName = FName(*(FString(TEXT("GraphParam_")) + ParameterName.ToString()));

                                        const FVoxelPinType& PinType = Parameter.Type;

                                        if (PinType.IsWildcard())
                                        {
                                            return;
                                        }

                                        // Get the parameter value - check overrides first, then use default
                                        const FVoxelParameterValueOverride* Override = ParameterOverrides.GuidToValueOverride.Find(Guid);
                                        const FVoxelPinValue* OverrideValuePtr = (Override && Override->bEnable) ? &Override->Value : nullptr;

                                        const FVoxelPinType InnerType = PinType.GetInnerType();

                                        if (InnerType == FVoxelPinType::Make<float>())
                                        {
                                            float Value = OverrideValuePtr ? OverrideValuePtr->Get<float>() : 0.0f;
                                            FPCGMetadataAttribute<float>* Attribute = static_cast<FPCGMetadataAttribute<float>*>(Metadata->GetMutableAttribute(PrefixedPropertyName));
                                            if (Attribute)
                                            {
                                                Attribute->SetValue(EntryKey, Value);
                                            }
                                        }
                                        else if (InnerType == FVoxelPinType::Make<double>())
                                        {
                                            double Value = OverrideValuePtr ? OverrideValuePtr->Get<double>() : 0.0;
                                            FPCGMetadataAttribute<float>* Attribute = static_cast<FPCGMetadataAttribute<float>*>(Metadata->GetMutableAttribute(PrefixedPropertyName));
                                            if (Attribute)
                                            {
                                                Attribute->SetValue(EntryKey, static_cast<float>(Value));
                                            }
                                        }
                                        else if (InnerType == FVoxelPinType::Make<int32>())
                                        {
                                            int32 Value = OverrideValuePtr ? OverrideValuePtr->Get<int32>() : 0;
                                            FPCGMetadataAttribute<int32>* Attribute = static_cast<FPCGMetadataAttribute<int32>*>(Metadata->GetMutableAttribute(PrefixedPropertyName));
                                            if (Attribute)
                                            {
                                                Attribute->SetValue(EntryKey, Value);
                                            }
                                        }
                                        else if (InnerType == FVoxelPinType::Make<int64>())
                                        {
                                            int64 Value = OverrideValuePtr ? OverrideValuePtr->Get<int64>() : 0;
                                            FPCGMetadataAttribute<int32>* Attribute = static_cast<FPCGMetadataAttribute<int32>*>(Metadata->GetMutableAttribute(PrefixedPropertyName));
                                            if (Attribute)
                                            {
                                                Attribute->SetValue(EntryKey, static_cast<int32>(Value));
                                            }
                                        }
                                        else if (InnerType == FVoxelPinType::Make<bool>())
                                        {
                                            bool Value = OverrideValuePtr ? OverrideValuePtr->Get<bool>() : false;
                                            FPCGMetadataAttribute<int32>* Attribute = static_cast<FPCGMetadataAttribute<int32>*>(Metadata->GetMutableAttribute(PrefixedPropertyName));
                                            if (Attribute)
                                            {
                                                Attribute->SetValue(EntryKey, Value ? 1 : 0);
                                            }
                                        }
                                        else if (InnerType == FVoxelPinType::Make<FName>())
                                        {
                                            FName Value = OverrideValuePtr ? OverrideValuePtr->Get<FName>() : FName();
                                            FPCGMetadataAttribute<FName>* Attribute = static_cast<FPCGMetadataAttribute<FName>*>(Metadata->GetMutableAttribute(PrefixedPropertyName));
                                            if (Attribute)
                                            {
                                                Attribute->SetValue(EntryKey, Value);
                                            }
                                        }
                                        // Skip FString - causes compilation issues with TBaseStructure<FString>
                                        else if (InnerType == FVoxelPinType::Make<FVector>())
                                        {
                                            FVector Value = OverrideValuePtr ? OverrideValuePtr->Get<FVector>() : FVector::ZeroVector;
                                            FPCGMetadataAttribute<FVector>* Attribute = static_cast<FPCGMetadataAttribute<FVector>*>(Metadata->GetMutableAttribute(PrefixedPropertyName));
                                            if (Attribute)
                                            {
                                                Attribute->SetValue(EntryKey, Value);
                                            }
                                        }
                                        else if (InnerType == FVoxelPinType::Make<FRotator>())
                                        {
                                            FRotator Value = OverrideValuePtr ? OverrideValuePtr->Get<FRotator>() : FRotator::ZeroRotator;
                                            FPCGMetadataAttribute<FRotator>* Attribute = static_cast<FPCGMetadataAttribute<FRotator>*>(Metadata->GetMutableAttribute(PrefixedPropertyName));
                                            if (Attribute)
                                            {
                                                Attribute->SetValue(EntryKey, Value);
                                            }
                                        }
                                        else if (InnerType == FVoxelPinType::Make<FLinearColor>())
                                        {
                                            FLinearColor Value = OverrideValuePtr ? OverrideValuePtr->Get<FLinearColor>() : FLinearColor::Black;
                                            FPCGMetadataAttribute<FVector4>* Attribute = static_cast<FPCGMetadataAttribute<FVector4>*>(Metadata->GetMutableAttribute(PrefixedPropertyName));
                                            if (Attribute)
                                            {
                                                Attribute->SetValue(EntryKey, FVector4(Value.R, Value.G, Value.B, Value.A));
                                            }
                                        }
                                        else if (InnerType == FVoxelPinType::Make<FColor>())
                                        {
                                            FColor Value = OverrideValuePtr ? OverrideValuePtr->Get<FColor>() : FColor::Black;
                                            FLinearColor LinearColor = Value.ReinterpretAsLinear();
                                            FPCGMetadataAttribute<FVector4>* Attribute = static_cast<FPCGMetadataAttribute<FVector4>*>(Metadata->GetMutableAttribute(PrefixedPropertyName));
                                            if (Attribute)
                                            {
                                                Attribute->SetValue(EntryKey, FVector4(LinearColor.R, LinearColor.G, LinearColor.B, LinearColor.A));
                                            }
                                        }
                                    });
                            }
                        }
                    }
                }
            }

            // For VoxelSculpt actors, get the External Asset
            if (bIsVoxelSculpt)
            {
                AVoxelSculptActorBase* SculptActor = Cast<AVoxelSculptActorBase>(Actor);
                if (SculptActor)
                {
                    // The ExternalAsset is on the component, not the actor
                    // Try to find a component property (could be "Component" or similar)
                    UClass* SculptClass = SculptActor->GetClass();

                    // Look for component properties that might contain the ExternalAsset
                    for (TFieldIterator<FProperty> PropIt(SculptClass); PropIt; ++PropIt)
                    {
                        FProperty* Property = *PropIt;

                        // Check if it's an object property pointing to a component
                        if (FObjectProperty* ObjectProp = CastField<FObjectProperty>(Property))
                        {
                            // Get the component
                            void* ComponentPtr = ObjectProp->ContainerPtrToValuePtr<void>(SculptActor);
                            UObject* ComponentObject = ObjectProp->GetObjectPropertyValue(ComponentPtr);

                            if (ComponentObject)
                            {
                                // Now look for ExternalAsset on the component
                                UClass* ComponentClass = ComponentObject->GetClass();
                                FProperty* ExternalAssetProperty = ComponentClass->FindPropertyByName(FName(TEXT("ExternalAsset")));

                                if (ExternalAssetProperty)
                                {
                                    void* ExternalAssetPtr = ExternalAssetProperty->ContainerPtrToValuePtr<void>(ComponentObject);
                                    FSoftObjectPath AssetPath;

                                    if (FObjectProperty* ExternalAssetObjProp = CastField<FObjectProperty>(ExternalAssetProperty))
                                    {
                                        UObject* ExternalAsset = ExternalAssetObjProp->GetObjectPropertyValue(ExternalAssetPtr);
                                        AssetPath = FSoftObjectPath(ExternalAsset);
                                    }
                                    else if (FSoftObjectProperty* SoftObjectProp = CastField<FSoftObjectProperty>(ExternalAssetProperty))
                                    {
                                        FSoftObjectPtr SoftObject = SoftObjectProp->GetPropertyValue(ExternalAssetPtr);
                                        AssetPath = SoftObject.ToSoftObjectPath();
                                    }

                                    FPCGMetadataAttribute<FSoftObjectPath>* ExternalAssetAttr = static_cast<FPCGMetadataAttribute<FSoftObjectPath>*>(Metadata->GetMutableAttribute(FName(TEXT("Sculpt_ExternalAsset"))));
                                    if (ExternalAssetAttr)
                                    {
                                        ExternalAssetAttr->SetValue(EntryKey, AssetPath);
                                    }

                                    // Found and processed ExternalAsset, break out
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            // Process actor tags and set tag-based float attributes
            for (const FName& Tag : Actor->Tags)
            {
                FString TagString = Tag.ToString();
                int32 ColonIndex;

                // Check if tag contains a colon
                if (TagString.FindChar(':', ColonIndex))
                {
                    FString AttributeName = TagString.Left(ColonIndex);
                    FString ValueString = TagString.RightChop(ColonIndex + 1);

                    // Try to convert the right side to a float
                    float FloatValue = FCString::Atof(*ValueString);

                    // Set the attribute value
                    FPCGMetadataAttribute<float>* Attribute = static_cast<FPCGMetadataAttribute<float>*>(Metadata->GetMutableAttribute(FName(*AttributeName)));
                    if (Attribute)
                    {
                        Attribute->SetValue(EntryKey, FloatValue);
                    }
                }
            }
        }

        // Step 7: Set the array of points to the PCGPointData
        PointData->SetPoints(Points);

        // Step 8: Make PCGPointData into TaggedData
        FPCGTaggedData& TaggedData = OutputData.Emplace_GetRef();
        TaggedData.Pin = TEXT("Point Out");  // Output to the "Point Out" pin
        TaggedData.Data = PointData;

        // Add actor class name as a tag (without _C suffix)
        TaggedData.Tags.Add(GetCleanClassName(ActorClass));

        // Add parent class name as a tag (without _C suffix)
        UClass* ParentClass = ActorClass->GetSuperClass();
        if (ParentClass)
        {
            TaggedData.Tags.Add(GetCleanClassName(ParentClass));
        }

        // Add graph name as a tag if this group has a specific graph
        if (VoxelGraph)
        {
            TaggedData.Tags.Add(FString::Printf(TEXT("Graph_%s"), *VoxelGraph->GetName()));
        }
    }

    return OutputData;
}

void UConvertLevelActorsToPCGPoints::SetSculptAssetFromObject(
    AVoxelSculptHeight* SculptActor,
    UObject* AssetObject)
{
    VOXEL_FUNCTION_COUNTER();
    if (!SculptActor)
    {
        VOXEL_MESSAGE(Error, "SculptActor is invalid");
        return;
    }

    // Cast the object to the asset type
    UVoxelSculptHeightAsset* Asset = Cast<UVoxelSculptHeightAsset>(AssetObject);

    if (!AssetObject)
    {
        // Object was null, set empty sculpt data
        SculptActor->SetSculptData(MakeVoxelBulkRef(MakeShared<FVoxelSculptHeightData>()));
    }
    else if (!Asset)
    {
        // Object was not null but cast failed - invalid type
        VOXEL_MESSAGE(Error, "AssetObject is not a valid UVoxelSculptHeightAsset");
        return;
    }
    else
    {
        // Cast succeeded, use the asset
        SculptActor->SetSculptData(Asset->GetData(), Asset->GetBulkLoader());
    }
}