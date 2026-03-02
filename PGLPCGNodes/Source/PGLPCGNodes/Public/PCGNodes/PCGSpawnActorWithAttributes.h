// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGElement.h"
#include "PCGSpawnActorWithAttributes.generated.h"

// Forward declarations
class UPCGPointData;
class AActor;
class UPCGComponent;
class FPCGMetadataAttributeBase;

/**
 * Custom PCG node that spawns actors and automatically maps PCG attributes
 * to matching actor properties by name and type
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class UPCGSpawnActorWithAttributesSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGSpawnActorWithAttributesSettings();

	//~Begin UPCGSettings interface
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	/** The actor class to spawn */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	TSoftClassPtr<AActor> ActorClass;

	/** Attribute name that contains the actor class to spawn (overrides ActorClass if set) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	FName ActorClassAttribute;

	/** Whether to attach spawned actors to the target actor */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	bool bAttachToParent = false;

	/** Option to specify the parent actor */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (EditCondition = "bAttachToParent"))
	TSoftObjectPtr<AActor> ParentActor;

	/** Whether to enable detailed logging of attribute matching */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debug")
	bool bEnableDebugLogging = false;

protected:
#if WITH_EDITOR
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGSpawnActorWithAttributesSettings", "NodeTitle", "Spawn Actor With Attributes"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif
};

class FPCGSpawnActorWithAttributesElement : public IPCGElement
{
public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

	// Force execution on game thread since we're spawning actors
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	/** Spawns an actor and applies attributes to matching properties */
	AActor* SpawnActorWithAttributes(
		FPCGContext* Context,
		const UPCGSpawnActorWithAttributesSettings* Settings,
		UClass* ActorClass,
		const FTransform& Transform,
		const UPCGMetadata* Metadata,
		const FPCGPoint& Point) const;

	/** Tries to set a property value from a PCG attribute */
	bool TrySetPropertyFromAttribute(
		FPCGContext* Context,
		AActor* Actor,
		FProperty* Property,
		const UPCGMetadata* Metadata,
		const FPCGPoint& Point,
		const FName& AttributeName,
		bool bEnableLogging) const;

	/** Type-specific property setters */
	bool SetFloatProperty(AActor* Actor, FProperty* Property, float Value) const;
	bool SetDoubleProperty(AActor* Actor, FProperty* Property, double Value) const;
	bool SetIntProperty(AActor* Actor, FProperty* Property, int32 Value) const;
	bool SetInt64Property(AActor* Actor, FProperty* Property, int64 Value) const;
	bool SetBoolProperty(AActor* Actor, FProperty* Property, bool Value) const;
	bool SetVectorProperty(AActor* Actor, FProperty* Property, const FVector& Value) const;
	bool SetVector4Property(AActor* Actor, FProperty* Property, const FVector4& Value) const;
	bool SetQuatProperty(AActor* Actor, FProperty* Property, const FQuat& Value) const;
	bool SetRotatorProperty(AActor* Actor, FProperty* Property, const FRotator& Value) const;
	bool SetStringProperty(AActor* Actor, FProperty* Property, const FString& Value) const;
	bool SetNameProperty(AActor* Actor, FProperty* Property, const FName& Value) const;
};