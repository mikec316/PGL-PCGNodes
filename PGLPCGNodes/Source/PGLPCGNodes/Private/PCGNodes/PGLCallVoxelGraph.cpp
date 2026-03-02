// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PGLCallVoxelGraph.h"
#include "PCGNodes/PGLVoxelAttributeMarshalling.h"

#include "VoxelQuery.h"
#include "VoxelLayers.h"
#include "VoxelPointId.h"
#include "VoxelPCGGraph.h"
#include "VoxelPCGHelpers.h"
#include "VoxelPCGUtilities.h"
#include "VoxelGraphTracker.h"
#include "VoxelTerminalGraph.h"
#include "VoxelParameterView.h"
#include "VoxelPCGFunctionLibrary.h"
#include "VoxelGraphParametersView.h"
#include "VoxelTerminalGraphRuntime.h"
#include "VoxelOutputNode_OutputPoints.h"
#include "VoxelGraphParametersViewContext.h"

#include "Buffer/VoxelNameBuffer.h"
#include "Buffer/VoxelFloatBuffers.h"
#include "Buffer/VoxelDoubleBuffers.h"
#include "Buffer/VoxelSoftObjectPathBuffer.h"
#include "Graphs/VoxelStampGraphParameters.h"
#include "Surface/VoxelSurfaceTypeTable.h"
#include "Scatter/VoxelScatterFunctionLibrary.h"

#include "PCGComponent.h"
#include "Helpers/PCGAsync.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGHelpers.h"
#include "Helpers/PCGSettingsHelpers.h"

// ─────────────────────────────────────────────────────────────────────────────
// UPGLCallVoxelGraphSettings
// ─────────────────────────────────────────────────────────────────────────────

UPGLCallVoxelGraphSettings::UPGLCallVoxelGraphSettings()
{
}

#if WITH_EDITOR
UObject* UPGLCallVoxelGraphSettings::GetJumpTargetForDoubleClick() const
{
	return Graph ? Graph.Get() : Super::GetJumpTargetForDoubleClick();
}
#endif

FString UPGLCallVoxelGraphSettings::GetAdditionalTitleInformation() const
{
	if (!Graph)
	{
		return "Missing Graph";
	}

#if WITH_EDITOR
	if (!Graph->DisplayNameOverride.IsEmpty())
	{
		return Graph->DisplayNameOverride;
	}
#endif

	return FName::NameToDisplayString(Graph->GetName(), false);
}

TArray<FPCGPinProperties> UPGLCallVoxelGraphSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;

	Properties.Emplace_GetRef(
		"Points",
		EPCGDataType::Point,
		true,
		true).SetRequiredPin();

	Properties.Emplace(
		"Bounding Shape",
		EPCGDataType::Spatial,
		false,
		false);

	return Properties;
}

TArray<FPCGPinProperties> UPGLCallVoxelGraphSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);

	if (!Graph)
	{
		return Properties;
	}

	const UVoxelTerminalGraph* TerminalGraph = nullptr;
	for (UVoxelGraph* BaseGraph : Graph->GetBaseGraphs())
	{
		if (BaseGraph->HasMainTerminalGraph())
		{
			TerminalGraph = &BaseGraph->GetMainTerminalGraph();
			break;
		}
	}

	if (!TerminalGraph)
	{
		return Properties;
	}

	for (const auto& NodeIt : TerminalGraph->GetRuntime().GetSerializedGraph().NodeNameToNode)
	{
		const FVoxelOutputNode_OutputPoints* OutputPointsNode = NodeIt.Value.VoxelNode.GetPtr<FVoxelOutputNode_OutputPoints>();
		if (!OutputPointsNode)
		{
			continue;
		}

		for (const FName PinName : OutputPointsNode->PinNames)
		{
			Properties.Emplace(PinName, EPCGDataType::Point);
		}
	}

	return Properties;
}

#if WITH_EDITOR
TArray<FPCGSettingsOverridableParam> UPGLCallVoxelGraphSettings::GatherOverridableParams() const
{
	TArray<FPCGSettingsOverridableParam> OverridableParams = Super::GatherOverridableParams();
	if (const UScriptStruct* ScriptStruct = ParameterPinOverrides.GetPropertyBagStruct())
	{
		PCGSettingsHelpers::FPCGGetAllOverridableParamsConfig Config;
		Config.bExcludeSuperProperties = true;
		Config.ExcludePropertyFlags = CPF_DisableEditOnInstance;
		OverridableParams.Append(PCGSettingsHelpers::GetAllOverridableParams(ScriptStruct, Config));
	}

	return OverridableParams;
}
#endif

void UPGLCallVoxelGraphSettings::FixingOverridableParamPropertyClass(FPCGSettingsOverridableParam& Param) const
{
	if (!ParameterPinOverrides.IsValid() ||
		Param.PropertiesNames.IsEmpty())
	{
		Super::FixingOverridableParamPropertyClass(Param);
		return;
	}

	const UScriptStruct* ScriptStruct = ParameterPinOverrides.GetPropertyBagStruct();
	if (ScriptStruct &&
		ScriptStruct->FindPropertyByName(Param.PropertiesNames[0]))
	{
		Param.PropertyClass = ScriptStruct;
		return;
	}

	Super::FixingOverridableParamPropertyClass(Param);
}

FPCGElementPtr UPGLCallVoxelGraphSettings::CreateElement() const
{
	return MakeShared<FPGLCallVoxelGraphElement>();
}

///////////////////////////////////////////////////////////////////////////////
// UObject lifecycle
///////////////////////////////////////////////////////////////////////////////

void UPGLCallVoxelGraphSettings::PostLoad()
{
	VOXEL_FUNCTION_COUNTER();
	Super::PostLoad();
	FixupParameterOverrides();
#if WITH_EDITOR
	FixupOnChangedDelegate();
#endif
}

void UPGLCallVoxelGraphSettings::PostInitProperties()
{
	VOXEL_FUNCTION_COUNTER();
	Super::PostInitProperties();
	FixupParameterOverrides();
#if WITH_EDITOR
	FixupOnChangedDelegate();
#endif
}

void UPGLCallVoxelGraphSettings::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	SerializeVoxelVersion(Ar);
}

#if WITH_EDITOR
void UPGLCallVoxelGraphSettings::PostEditUndo()
{
	VOXEL_FUNCTION_COUNTER();
	Super::PostEditUndo();
	FixupParameterOverrides();
	FixupOnChangedDelegate();
}

void UPGLCallVoxelGraphSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	VOXEL_FUNCTION_COUNTER();
	Super::PostEditChangeProperty(PropertyChangedEvent);
	FixupParameterOverrides();
	FixupOnChangedDelegate();
}
#endif

///////////////////////////////////////////////////////////////////////////////
// IVoxelParameterOverridesOwner
///////////////////////////////////////////////////////////////////////////////

UVoxelGraph* UPGLCallVoxelGraphSettings::GetGraph() const
{
	return Graph;
}

bool UPGLCallVoxelGraphSettings::ShouldForceEnableOverride(const FGuid& Guid) const
{
	return true;
}

FVoxelParameterOverrides& UPGLCallVoxelGraphSettings::GetParameterOverrides()
{
	return ParameterOverrides;
}

void UPGLCallVoxelGraphSettings::FixupParameterOverrides()
{
	IVoxelParameterOverridesObjectOwner::FixupParameterOverrides();

	TArray<FPropertyBagPropertyDesc> NewProperties;
	if (const TSharedPtr<FVoxelGraphParametersView> ParametersView = GetParametersView())
	{
		TSet<FName> UsedNames;
		for (const FGuid Guid : Graph->GetParameters())
		{
			const FVoxelParameterView* ParameterView = ParametersView->FindByGuid(Guid);
			if (!ParameterView)
			{
				continue;
			}

			FPropertyBagPropertyDesc PropertyDesc = ParameterView->GetType().GetPropertyBagDesc();
			if (PropertyDesc.ValueType == EPropertyBagPropertyType::None)
			{
				continue;
			}

			FName ParameterName = FName(SlugStringForValidName(ParameterView->GetName().ToString()));
			while (UsedNames.Contains(ParameterName))
			{
				ParameterName.SetNumber(ParameterName.GetNumber() + 1);
			}

			UsedNames.Add(ParameterName);

			PropertyDesc.ID = ParameterView->Guid;
			PropertyDesc.Name = FName(ParameterName);
#if WITH_EDITOR
			PropertyDesc.MetaData.Emplace("DisplayName", ParameterView->GetName().ToString());
			PropertyDesc.MetaData.Emplace("Tooltip", ParameterView->GetDescription());
#endif

			NewProperties.Add(PropertyDesc);
		}
	}

	if (NewProperties.Num() == ParameterPinOverrides.GetNumPropertiesInBag())
	{
		bool bIsSame = true;
		for (const FPropertyBagPropertyDesc& PropertyDesc : NewProperties)
		{
			const FPropertyBagPropertyDesc* ExistingPropertyDesc = ParameterPinOverrides.FindPropertyDescByName(PropertyDesc.Name);
			if (!ExistingPropertyDesc)
			{
				bIsSame = false;
				break;
			}

			if (PropertyDesc.ValueType != ExistingPropertyDesc->ValueType ||
				PropertyDesc.ValueTypeObject != ExistingPropertyDesc->ValueTypeObject ||
				PropertyDesc.ContainerTypes != ExistingPropertyDesc->ContainerTypes)
			{
				bIsSame = false;
				break;
			}

#if WITH_EDITOR
			if (PropertyDesc.MetaData.Num() != ExistingPropertyDesc->MetaData.Num())
			{
				bIsSame = false;
				break;
			}

			for (int32 Index = 0; Index < PropertyDesc.MetaData.Num(); Index++)
			{
				const auto& MetadataA = PropertyDesc.MetaData[Index];
				const auto& MetadataB = ExistingPropertyDesc->MetaData[Index];
				if (MetadataA.Key != MetadataB.Key ||
					MetadataA.Value != MetadataB.Value)
				{
					bIsSame = false;
					break;
				}
			}

			if (!bIsSame)
			{
				break;
			}
#endif
		}

		if (bIsSame)
		{
			return;
		}
	}

	ParameterPinOverrides.Reset();
	ParameterPinOverrides.AddProperties(NewProperties);

	InitializeCachedOverridableParams(true);

#if WITH_EDITOR
	OnSettingsChangedDelegate.Broadcast(this, EPCGChangeType::Settings);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// Editor delegates
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
void UPGLCallVoxelGraphSettings::FixupOnChangedDelegate()
{
	OnTerminalGraphChangedPtr.Reset();

	if (!Graph)
	{
		return;
	}

	OnTerminalGraphChangedPtr = MakeSharedVoid();

	for (UVoxelGraph* BaseGraph : Graph->GetBaseGraphs())
	{
		GVoxelGraphTracker->OnTerminalGraphChanged(*BaseGraph).Add(FOnVoxelGraphChanged::Make(OnTerminalGraphChangedPtr, this, [this]
		{
			OnSettingsChangedDelegate.Broadcast(this, EPCGChangeType::Settings);
		}));

		if (!BaseGraph->HasMainTerminalGraph())
		{
			continue;
		}

		GVoxelGraphTracker->OnEdGraphChanged(BaseGraph->GetMainTerminalGraph().GetEdGraph()).Add(FOnVoxelGraphChanged::Make(OnTerminalGraphChangedPtr, this, [this]
		{
			OnSettingsChangedDelegate.Broadcast(this, EPCGChangeType::Settings);
		}));
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////
// FPGLCallVoxelGraphContext
///////////////////////////////////////////////////////////////////////////////

void FPGLCallVoxelGraphContext::InitializeUserParametersStruct()
{
	ParameterPinOverrides.Reset();

	const UPGLCallVoxelGraphSettings* Settings = GetInputSettings<UPGLCallVoxelGraphSettings>();
	check(Settings);

	const TArray<FPCGSettingsOverridableParam>& OverridableParams = Settings->OverridableParams();
	const FConstStructView UserParametersView = Settings->ParameterPinOverrides.GetValue();

	if (OverridableParams.IsEmpty() ||
		!UserParametersView.IsValid())
	{
		return;
	}

	bool bHasParamConnected = false;
	for (const FPCGSettingsOverridableParam& OverridableParam : OverridableParams)
	{
		if (!InputData.GetParamsByPin(OverridableParam.Label).IsEmpty())
		{
			bHasParamConnected = true;
			break;
		}
	}

	if (!InputData.GetParamsByPin(PCGPinConstants::DefaultParamsLabel).IsEmpty())
	{
		bHasParamConnected = true;
	}

	if (!bHasParamConnected)
	{
		return;
	}

	ParameterPinOverrides.InitializeFromBagStruct(Settings->ParameterPinOverrides.GetPropertyBagStruct());

	const TSharedPtr<FVoxelGraphParametersView> ParametersView = Settings->GetParametersView();
	if (!ParametersView)
	{
		return;
	}

	for (const FVoxelParameterView* ParameterView : ParametersView->GetChildren())
	{
		const FVoxelParameter Parameter = ParameterView->GetParameter();
		const FPropertyBagPropertyDesc* Desc = ParameterPinOverrides.FindPropertyDescByName(Parameter.Name);
		if (!Desc)
		{
			continue;
		}

		void* TargetAddress = ParameterPinOverrides.GetMutableValue().GetMemory() + Desc->CachedProperty->GetOffset_ForInternal();
		ParameterView->GetDefaultValue().ExportToProperty(*Desc->CachedProperty, TargetAddress);
	}
}

void* FPGLCallVoxelGraphContext::GetUnsafeExternalContainerForOverridableParam(const FPCGSettingsOverridableParam& InParam)
{
	return ParameterPinOverrides.GetMutableValue().GetMemory();
}

void FPGLCallVoxelGraphContext::AddExtraStructReferencedObjects(FReferenceCollector& Collector)
{
	ParameterPinOverrides.AddStructReferencedObjects(Collector);
}

///////////////////////////////////////////////////////////////////////////////
// FPGLCallVoxelGraphElement
///////////////////////////////////////////////////////////////////////////////

#if VOXEL_ENGINE_VERSION >= 506
FPCGContext* FPGLCallVoxelGraphElement::Initialize(const FPCGInitializeElementParams& InParams)
#else
FPCGContext* FPGLCallVoxelGraphElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
#endif
{
	FPGLCallVoxelGraphContext* Context = new FPGLCallVoxelGraphContext();

#if VOXEL_ENGINE_VERSION >= 506
	Context->InitFromParams(InParams);
#else
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;
#endif

	Context->InitializeUserParametersStruct();

	return Context;
}

bool FPGLCallVoxelGraphElement::IsCacheable(const UPCGSettings* InSettings) const
{
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// PrepareDataInternal
///////////////////////////////////////////////////////////////////////////////

bool FPGLCallVoxelGraphElement::PrepareDataInternal(FPCGContext* InContext) const
{
	VOXEL_FUNCTION_COUNTER();
	ensure(IsInGameThread());

	FPGLCallVoxelGraphContext* Context = static_cast<FPGLCallVoxelGraphContext*>(InContext);
	check(Context);

	if (Context->WasLoadRequested())
	{
		return true;
	}

	UPCGComponent* Component = GetPCGComponent(*Context);
	if (!ensure(Component))
	{
		return true;
	}

	const UPGLCallVoxelGraphSettings* Settings = Context->GetInputSettings<UPGLCallVoxelGraphSettings>();
	check(Settings);

	if (!Settings->Graph)
	{
		return true;
	}

	// ── Compute bounds ───────────────────────────────────────────────────────

	const FVoxelBox Bounds = INLINE_LAMBDA -> FVoxelBox
	{
		if (Context->InputData.GetInputCountByPin("Bounding Shape") == 0)
		{
			return {};
		}

		FBox Result;
		{
			bool bOutUnionWasCreated = false;
			const UPCGSpatialData* BoundingShape = Context->InputData.GetSpatialUnionOfInputsByPin(Context, "Bounding Shape", bOutUnionWasCreated);

			if (!BoundingShape)
			{
				ensure(!bOutUnionWasCreated);
				BoundingShape = Cast<UPCGSpatialData>(Component->GetActorPCGData());
			}

			if (!ensure(BoundingShape))
			{
				return {};
			}

			Result = BoundingShape->GetBounds();
		}

		if (!ensureVoxelSlow(Result.IsValid))
		{
			return {};
		}

		{
			const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Component->GetActorPCGData());
			if (!ensure(SpatialData))
			{
				return {};
			}

			const FBox ActorBounds = SpatialData->GetBounds();
			if (!ensure(ActorBounds.IsValid))
			{
				return {};
			}

			if (!Result.Intersect(ActorBounds))
			{
				return {};
			}

			Result = Result.Overlap(ActorBounds);
		}

		if (!ensure(Result.IsValid))
		{
			return {};
		}

		return FVoxelBox(Result);
	};

	Context->Bounds = Bounds;

	// ── Apply parameter pin overrides ────────────────────────────────────────

	INLINE_LAMBDA
	{
		if (Settings == Context->GetOriginalSettings<UPGLCallVoxelGraphSettings>() ||
			!Context->ParameterPinOverrides.IsValid())
		{
			return;
		}

		UPGLCallVoxelGraphSettings* MutableSettings = ConstCast(Settings);
		const FInstancedPropertyBag& PinOverrides = Context->ParameterPinOverrides;

		const TSharedPtr<FVoxelGraphParametersView> ParametersView = MutableSettings->GetParametersView();
		const UPropertyBag* PropertyBagStruct = PinOverrides.GetPropertyBagStruct();
		if (!PropertyBagStruct ||
			!PinOverrides.GetValue().IsValid() ||
			!ParametersView)
		{
			return;
		}

		for (const FPropertyBagPropertyDesc& PropertyDesc : PropertyBagStruct->GetPropertyDescs())
		{
			if (!ensure(PropertyDesc.CachedProperty))
			{
				continue;
			}

			FVoxelPinValue Value;

			if (const void* Memory = PinOverrides.GetValue().GetMemory() + PropertyDesc.CachedProperty->GetOffset_ForInternal())
			{
				Value = FVoxelPinValue::MakeFromProperty(*PropertyDesc.CachedProperty, Memory);
			}

			if (!Value.IsValid())
			{
				continue;
			}

			const FVoxelParameterView* ParameterView = ParametersView->FindByGuid(PropertyDesc.ID);
			if (!ParameterView)
			{
				continue;
			}

			MutableSettings->SetParameter(ParameterView->GetName(), Value);
		}
	};

	// ── Create voxel graph environment ───────────────────────────────────────

	UWorld* World = Component->GetWorld();
	ensure(World);

	FTransform LocalToWorld;
	if (const AActor* TargetActor = Context->GetTargetActor(nullptr))
	{
		LocalToWorld = TargetActor->ActorToWorld();
	}

	Context->DependencyCollector = MakeShared<FVoxelDependencyCollector>(STATIC_FNAME("PGLCallVoxelGraph"));

	const TSharedPtr<FVoxelGraphEnvironment> Environment = FVoxelGraphEnvironment::Create(
		Component,
		*Settings,
		LocalToWorld,
		*Context->DependencyCollector);

	if (!Environment)
	{
		return true;
	}

	Context->Evaluator = FVoxelNodeEvaluator::Create<FVoxelOutputNode_OutputPoints>(Environment.ToSharedRef());

	if (!Context->Evaluator)
	{
		return true;
	}

	// ── Determine which output pins to query ─────────────────────────────────

#if !WITH_EDITOR
	if (Context->Node &&
		Context->Node->IsOutputPinConnected(PCGPinConstants::DefaultOutputLabel))
#endif
	{
		Context->PinsToQuery.Add_EnsureNew(VOXEL_PIN_NAME(FVoxelOutputNode_OutputPoints, PointsPin));
	}

	for (const FName PinName : Context->Evaluator->PinNames)
	{
#if !WITH_EDITOR
		if (Context->Node &&
			Context->Node->IsOutputPinConnected(PinName))
#endif
		{
			Context->PinsToQuery.Add(PinName);
		}
	}

	if (Context->PinsToQuery.Num() == 0)
	{
		return true;
	}

	Context->Layers = FVoxelLayers::Get(World);
	Context->SurfaceTypeTable = FVoxelSurfaceTypeTable::Get();

	// ── OPTIMIZATION: Build attribute whitelist ──────────────────────────────

	if (Settings->bSelectiveMarshalling && Settings->CustomAttributeWhitelist.Num() > 0)
	{
		Context->bUseWhitelist = true;
		for (const FName& Name : Settings->CustomAttributeWhitelist)
		{
			Context->AttributeWhitelist.Add(Name);
		}
	}

	// ── Collect input point data and create output slots ─────────────────────

	const TArray<FPCGTaggedData> PointDatas = Context->InputData.GetInputsByPin("Points");
	if (PointDatas.Num() == 0)
	{
		return true;
	}

	// Collect soft objects to pre-load
	TSet<FName> AttributesToLoad;
	for (const FVoxelPCGObjectAttributeType& Type : Settings->ObjectAttributeToType)
	{
		if (Type.Type.Is<FVoxelSoftObjectPath>())
		{
			continue;
		}

		AttributesToLoad.Add(Type.Attribute.GetName());
	}

	TSet<FSoftObjectPath> ObjectsToLoad;
	TArray<FPCGTaggedData>& Outputs = InContext->OutputData.TaggedData;
	for (const FPCGTaggedData& TaggedData : PointDatas)
	{
		const UPCGPointData* PointData = Cast<UPCGPointData>(TaggedData.Data);
		if (!PointData)
		{
			continue;
		}

		// Collect soft object paths for async loading
		INLINE_LAMBDA
		{
			const UPCGMetadata* Metadata = PointData->Metadata;
			if (!Metadata || AttributesToLoad.Num() == 0)
			{
				return;
			}

			const TConstVoxelArrayView<FPCGPoint> Points = PointData->GetPoints();

			for (const FName AttributeName : AttributesToLoad)
			{
				const FPCGMetadataAttributeBase* MetadataAttribute = Metadata->GetConstAttribute(AttributeName);
				if (!MetadataAttribute)
				{
					continue;
				}

				EPCGMetadataTypes AttributeType = EPCGMetadataTypes::Unknown;
				if (MetadataAttribute->GetTypeId() < static_cast<uint16>(EPCGMetadataTypes::Unknown))
				{
					AttributeType = static_cast<EPCGMetadataTypes>(MetadataAttribute->GetTypeId());
				}

				if (AttributeType != EPCGMetadataTypes::SoftObjectPath)
				{
					continue;
				}

				const auto CopyAttribute = [&]<typename T>(const FPCGMetadataAttribute<T>& Attribute)
				{
					if constexpr (std::is_same_v<T, FSoftObjectPath>)
					{
						for (int32 Index = 0; Index < Points.Num(); Index++)
						{
							ObjectsToLoad.Add(Attribute.GetValueFromItemKey(Points[Index].MetadataEntry));
						}
					}
					else
					{
						ensure(false);
					}
				};

				FVoxelPCGUtilities::SwitchOnAttribute(
					AttributeType,
					*MetadataAttribute,
					CopyAttribute);
			}
		};

		Context->InPointData.Add(PointData);

		TVoxelMap<FName, TVoxelObjectPtr<UPCGPointData>> OutputPins;
		for (FName PinName : Context->PinsToQuery)
		{
			UPCGPointData* OutPointData = NewObject<UPCGPointData>();
			OutPointData->InitializeFromData(PointData);
			FPCGTaggedData& OutTaggedData = Outputs.Emplace_GetRef();
			OutTaggedData.Tags = TaggedData.Tags;
			OutTaggedData.Data = OutPointData;
			if (PinName == VOXEL_PIN_NAME(FVoxelOutputNode_OutputPoints, PointsPin))
			{
				OutTaggedData.Pin = PCGPinConstants::DefaultOutputLabel;
			}
			else
			{
				OutTaggedData.Pin = PinName;
			}

			OutputPins.Add_EnsureNew(PinName, OutPointData);
		}

		Context->OutPointData.Add(OutputPins);
	}

	if (!ensure(Context->InPointData.Num() > 0) ||
		!ensure(Context->InPointData.Num() == Context->OutPointData.Num()))
	{
		return true;
	}

	if (ObjectsToLoad.Num() > 0)
	{
		return Context->RequestResourceLoad(Context, ObjectsToLoad.Array(), !Settings->bSynchronousLoad);
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
// ExecuteInternal
///////////////////////////////////////////////////////////////////////////////

bool FPGLCallVoxelGraphElement::ExecuteInternal(FPCGContext* InContext) const
{
	VOXEL_FUNCTION_COUNTER();

	FPGLCallVoxelGraphContext* Context = static_cast<FPGLCallVoxelGraphContext*>(InContext);
	check(Context);

	if (!Context->Evaluator ||
		!ensureVoxelSlow(Context->InPointData.Num() == Context->OutPointData.Num()))
	{
		return true;
	}

	const UPGLCallVoxelGraphSettings* Settings = Context->GetInputSettings<UPGLCallVoxelGraphSettings>();
	check(Settings);

	// ── Check if async compute is already in-flight ──────────────────────────

	if (Context->FuturePoints.Num() > 0)
	{
		Voxel::FlushGameTasks();

		for (const FVoxelFuture& Future : Context->FuturePoints)
		{
			if (!Future.IsComplete())
			{
				return false;
			}
		}

		return true;
	}

	ensure(!Context->bIsPaused);
	Context->bIsPaused = true;

	const TFunction<void(int32)> Resume = MakeWeakPtrLambda(Context->SharedVoid, [Context](int32 FutureIndex)
	{
		check(IsInGameThread());

		for (int32 Index = 0; Index < Context->FuturePoints.Num(); Index++)
		{
			if (Index == FutureIndex)
			{
				continue;
			}

			if (!Context->FuturePoints[Index].IsComplete())
			{
				return;
			}
		}

		ensure(Context->bIsPaused);
		Context->bIsPaused = false;
	});

	// ── Build object type mapping ────────────────────────────────────────────

	TMap<FName, FVoxelPinType> AttributeToObjectType;
	for (const FVoxelPCGObjectAttributeType& Type : Settings->ObjectAttributeToType)
	{
		if (Type.Type.Is<FVoxelSoftObjectPath>())
		{
			continue;
		}

		AttributeToObjectType.Add(Type.Attribute.GetName(), Type.Type);
	}

	// ── Process each input point data ────────────────────────────────────────

	for (int32 PointDataIndex = 0; PointDataIndex < Context->InPointData.Num(); PointDataIndex++)
	{
		TVoxelMap<FName, EPCGMetadataTypes> AttributeNameToType;
		TVoxelMap<FName, TSharedPtr<FVoxelBuffer>> AttributeNameToBuffer;

		const UPCGPointData* InData = Context->InPointData[PointDataIndex];
		const TConstVoxelArrayView<FPCGPoint> Points = InData->GetPoints();
		const int32 NumPoints = Points.Num();

		if (const UPCGMetadata* Metadata = InData->Metadata)
		{
			TArray<FName> AttributeNames;
			TArray<EPCGMetadataTypes> AttributeTypes;
			Metadata->GetAttributes(AttributeNames, AttributeTypes);
			check(AttributeNames.Num() == AttributeTypes.Num());

			for (int32 AttributeIndex = 0; AttributeIndex < AttributeNames.Num(); AttributeIndex++)
			{
				const FName AttributeName = AttributeNames[AttributeIndex];
				const EPCGMetadataTypes AttributeType = AttributeTypes[AttributeIndex];

				// ── OPTIMIZATION: Selective marshalling ──────────────────
				if (Context->bUseWhitelist && !Context->AttributeWhitelist.Contains(AttributeName))
				{
					// Still record the type so reverse conversion knows the original type
					AttributeNameToType.Add_EnsureNew(AttributeName, AttributeType);
					continue;
				}

				AttributeNameToType.Add_EnsureNew(AttributeName, AttributeType);

				const FPCGMetadataAttributeBase* Attribute = Metadata->GetConstAttribute(AttributeName);
				if (!ensureVoxelSlow(Attribute))
				{
					continue;
				}

				// Parallelized bulk conversion using FPCGPoint MetadataEntry
				TSharedPtr<FVoxelBuffer> Buffer = PGLVoxelMarshalling::PCGAttributeToVoxelBuffer(
					*Attribute,
					AttributeType,
					Points,
					NumPoints,
					AttributeName,
					AttributeToObjectType);

				if (Buffer)
				{
					AttributeNameToBuffer.Add_EnsureNew(AttributeName, Buffer);
				}
			}
		}

		// ── Snapshot input buffers for pass-through detection ────────────
		TVoxelMap<FName, TSharedPtr<const FVoxelBuffer>> InputSnapshot;
		if (Settings->bPassThroughDetection)
		{
			for (const auto& It : AttributeNameToBuffer)
			{
				InputSnapshot.Add_EnsureNew(It.Key, It.Value);
			}
		}

		// ── Dispatch async compute ───────────────────────────────────────
		// Copy FPCGPoint array into a TSharedPtr for async dispatch.
		// TSharedPtr::operator*() const returns non-const ref, allowing
		// MoveTemp in a const lambda (Voxel::AsyncTask requires const operator()).

		Context->FuturePoints.Add(Voxel::AsyncTask([
			Evaluator = Context->Evaluator,
			DependencyCollector = Context->DependencyCollector.ToSharedRef(),
			Bounds = Context->Bounds,
			PinsToQuery = Context->PinsToQuery,
			Layers = Context->Layers.ToSharedRef(),
			SurfaceTypeTable = Context->SurfaceTypeTable.ToSharedRef(),
			Points = MakeSharedCopy(TVoxelArray<FPCGPoint>(InData->GetPoints())),
			AttributeNameToType = MoveTemp(AttributeNameToType),
			AttributeNameToBuffer = MoveTemp(AttributeNameToBuffer),
			PinToOutPointData = Context->OutPointData[PointDataIndex],
			bPassThroughDetection = Settings->bPassThroughDetection,
			InputSnapshot = MoveTemp(InputSnapshot)]
		{
			return Compute(
				Evaluator,
				DependencyCollector,
				Bounds,
				PinsToQuery,
				Layers,
				SurfaceTypeTable,
				MoveTemp(*Points),
				AttributeNameToType,
				AttributeNameToBuffer,
				PinToOutPointData,
				bPassThroughDetection,
				InputSnapshot);
		})
		.Then_GameThread([=]
		{
			Resume(PointDataIndex);
		}));
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////
// Compute (worker thread)
///////////////////////////////////////////////////////////////////////////////

FVoxelFuture FPGLCallVoxelGraphElement::Compute(
	const TVoxelNodeEvaluator<FVoxelOutputNode_OutputPoints>& Evaluator,
	const TSharedRef<FVoxelDependencyCollector>& DependencyCollector,
	const FVoxelBox& Bounds,
	const TVoxelSet<FName>& PinsToQuery,
	const TSharedRef<FVoxelLayers>& Layers,
	const TSharedRef<FVoxelSurfaceTypeTable>& SurfaceTypeTable,
	TVoxelArray<FPCGPoint> Points,
	const TVoxelMap<FName, EPCGMetadataTypes>& AttributeNameToType,
	const TVoxelMap<FName, TSharedPtr<FVoxelBuffer>>& AttributeNameToBuffer,
	const TVoxelMap<FName, TVoxelObjectPtr<UPCGPointData>>& PinToOutPointData,
	bool bPassThroughDetection,
	const TVoxelMap<FName, TSharedPtr<const FVoxelBuffer>>& InputBufferSnapshot)
{
	VOXEL_FUNCTION_COUNTER();

	const int32 NumPoints = Points.Num();

	// ── Build FVoxelPointSet with custom attribute buffers ───────────────────

	const TSharedRef<FVoxelPointSet> PointSet = MakeShared<FVoxelPointSet>();

	for (const auto& It : AttributeNameToBuffer)
	{
		PointSet->Add(It.Key, It.Value.ToSharedRef());
	}

	// ── Build standard attributes from FPCGPoint array (ParallelFor) ────────

	PGLVoxelMarshalling::BuildStandardAttributes(Points, NumPoints, *PointSet);

	// ── Execute voxel graph ──────────────────────────────────────────────────

	FVoxelGraphContext Context = Evaluator.MakeContext(*DependencyCollector);

	const FVoxelQuery VoxelQuery(
		0,
		*Layers,
		*SurfaceTypeTable,
		*DependencyCollector);

	FVoxelGraphQueryImpl& Query = Context.MakeQuery();
	Query.AddParameter<FVoxelGraphParameters::FQuery>(VoxelQuery);
	Query.AddParameter<FVoxelGraphParameters::FPointSet>().Value = PointSet;
	Query.AddParameter<FVoxelGraphParameters::FPointSetChunkBounds>(Bounds, false);

	// ── Query output pins and build results on worker thread ─────────────────

	TVoxelMap<FName, TSharedPtr<TVoxelArray<FPCGPoint>>> PinToNewPoints;
	TVoxelMap<FName, TVoxelArray<PGLVoxelMarshalling::FMetadataWriter>> PinToMetadataWriters;

	TVoxelArray<const FVoxelNode::TPinRef_Input<FVoxelPointSet>*> Pins;
	Pins.Add(&Evaluator->PointsPin);
	for (const FVoxelNode::TPinRef_Input<FVoxelPointSet>& Pin : Evaluator->InputPins)
	{
		Pins.Add(&Pin);
	}

	// Count pins to process (for last-pin MoveTemp optimization)
	int32 NumPinsToProcess = 0;
	for (const auto* Pin : Pins)
	{
		if (PinsToQuery.Contains(Pin->GetName()))
		{
			NumPinsToProcess++;
		}
	}

	int32 PinsProcessed = 0;
	for (const FVoxelNode::TPinRef_Input<FVoxelPointSet>* Pin : Pins)
	{
		if (!PinsToQuery.Contains(Pin->GetName()))
		{
			continue;
		}

		TVoxelArray<PGLVoxelMarshalling::FMetadataWriter>& MetadataWriters = PinToMetadataWriters.Add_EnsureNew(Pin->GetName());

		const TSharedRef<const FVoxelPointSet> NewPointSet = Pin->GetSynchronous(Query);
		const int32 OutputNumPoints = NewPointSet->Num();

		// ── Create owned FPCGPoint array for this pin ────────────────────
		// Last pin takes ownership of input copy (avoids one memcpy).
		// Earlier pins get fresh copies so the original stays intact.
		PinsProcessed++;
		TSharedPtr<TVoxelArray<FPCGPoint>> PinPoints;
		if (PinsProcessed == NumPinsToProcess)
		{
			PinPoints = MakeShared<TVoxelArray<FPCGPoint>>(MoveTemp(Points));
		}
		else
		{
			PinPoints = MakeShared<TVoxelArray<FPCGPoint>>(Points);
		}

		// ── Write standard attributes in-place on worker thread ──────────
		// Only overwrites properties whose voxel buffer pointer differs from
		// input (pass-through detection). If output count differs, resizes
		// and writes all properties.
		PGLVoxelMarshalling::WriteStandardAttributesToPoints(*PointSet, *NewPointSet, *PinPoints, OutputNumPoints);

		PinToNewPoints.Add_EnsureNew(Pin->GetName(), PinPoints);

		// ── Convert custom attributes to metadata writers ────────────────
		// Uses buffer ref capture (no intermediate TVoxelArray copy) and
		// shared entry keys (extracted once in GameTask).

		for (const auto& AttributeIt : NewPointSet->GetAttributes())
		{
			FName AttributeName = AttributeIt.Key;
			const TSharedPtr<const FVoxelBuffer>& OutputBuffer = AttributeIt.Value;

			// Skip standard attributes (already handled by WriteStandardAttributesToPoints)
			if (AttributeName == FVoxelPointAttributes::Position ||
				AttributeName == FVoxelPointAttributes::Rotation ||
				AttributeName == FVoxelPointAttributes::Scale ||
				AttributeName == FVoxelPointAttributes::Density ||
				AttributeName == FVoxelPointAttributes::BoundsMin ||
				AttributeName == FVoxelPointAttributes::BoundsMax ||
				AttributeName == FVoxelPointAttributes::Color ||
				AttributeName == FVoxelPointAttributes::Steepness ||
				AttributeName == FVoxelPointAttributes::Id)
			{
				continue;
			}

			// Sanitize attribute name
			if (!FPCGMetadataAttributeBase::IsValidName(AttributeName.ToString()))
			{
				FString SanitizedAttributeName = AttributeName.ToString();
				FPCGMetadataAttributeBase::SanitizeName(SanitizedAttributeName);
				AttributeName = FName(SanitizedAttributeName);
			}

			// Pass-through detection for custom attributes
			if (bPassThroughDetection && OutputBuffer)
			{
				const TSharedPtr<const FVoxelBuffer>* InputBuffer = InputBufferSnapshot.Find(AttributeIt.Key);
				if (InputBuffer && InputBuffer->Get() == OutputBuffer.Get())
				{
					// Buffer pointer identical — attribute was not modified, skip conversion
					continue;
				}
			}

			// Convert buffer to metadata writer (captures buffer ref directly, no intermediate copy)
			PGLVoxelMarshalling::FMetadataWriter Writer = PGLVoxelMarshalling::VoxelBufferToPCGWriter(
				AttributeName,
				OutputBuffer,
				OutputNumPoints,
				AttributeNameToType.FindRef(AttributeIt.Key));

			if (Writer)
			{
				MetadataWriters.Add(MoveTemp(Writer));
			}
		}
	}

	// ── Move results to UPCGPointData on game thread ─────────────────────────

	return Voxel::GameTask([
		PinToOutPointData,
		PinToNewPoints = MakeSharedCopy(MoveTemp(PinToNewPoints)),
		PinToMetadataWriters = MakeSharedCopy(MoveTemp(PinToMetadataWriters))]
	{
		VOXEL_FUNCTION_COUNTER();
		check(IsInGameThread());

		for (auto& It : PinToOutPointData)
		{
			UPCGPointData* OutPointData = It.Value.Resolve();
			if (!ensureVoxelSlow(OutPointData))
			{
				continue;
			}

			TSharedPtr<TVoxelArray<FPCGPoint>>* PinPointsPtr = PinToNewPoints->Find(It.Key);
			const TVoxelArray<PGLVoxelMarshalling::FMetadataWriter>* MetadataWriters = PinToMetadataWriters->Find(It.Key);
			if (!ensure(PinPointsPtr) ||
				!ensure(MetadataWriters))
			{
				continue;
			}

			TVoxelArray<FPCGPoint>& PinPoints = **PinPointsPtr;
			const int32 OutputNumPoints = PinPoints.Num();

			// Set up metadata entries (modifies Points[i].MetadataEntry in-place)
			FVoxelPCGUtilities::AddPointsToMetadata(*OutPointData->Metadata, PinPoints);

			// Run metadata writers with shared entry keys (extracted ONCE)
			if (!MetadataWriters->IsEmpty())
			{
				TVoxelArray<PCGMetadataEntryKey> SharedEntryKeys;
				FVoxelUtilities::SetNumFast(SharedEntryKeys, OutputNumPoints);
				for (int32 Index = 0; Index < OutputNumPoints; Index++)
				{
					SharedEntryKeys[Index] = PinPoints[Index].MetadataEntry;
				}

				for (const PGLVoxelMarshalling::FMetadataWriter& Writer : *MetadataWriters)
				{
					Writer(*OutPointData->Metadata, SharedEntryKeys);
				}
			}

			// Move points to output (O(1) pointer swap)
			OutPointData->GetMutablePoints() = MoveTemp(PinPoints);
		}
	});
}
