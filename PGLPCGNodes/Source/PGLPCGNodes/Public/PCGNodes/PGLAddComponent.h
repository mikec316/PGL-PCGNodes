// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "Elements/PCGSpawnActor.h"

#include "PGLAddComponent.generated.h"

namespace PGLAddComponentConstants
{
	const FName InputPointsLabel = TEXT("In");
	const FName InputMapLabel = TEXT("Map");
}

/**
 * Consumes Component-typed collection entries from the staging pipeline and
 * creates component instances on the target actor at each point location.
 *
 * Inputs:
 *   In  - Points with PCGEx/CollectionEntry (int64 hash) attribute
 *   Map - Collection map (param data) from staging nodes
 *
 * For each point, resolves the collection entry hash to a FPGLComponentCollectionEntry,
 * loads the component class, creates an instance via NewObject, and attaches it to the
 * target actor. Scene components are positioned at the point's transform.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PGLPCGNODES_API UPGLAddComponentSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("PGLAddComponent")); }
	virtual FText GetDefaultNodeTitle() const override { return INVTEXT("PGL Add Component"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
	virtual FText GetNodeTooltipText() const override;
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;

public:
	/** Target actor to attach components to. If unset, uses the PCG component's owner. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TSoftObjectPtr<AActor> TargetActor;

	/** Controls where spawned components appear in the Outliner. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGAttachOptions AttachOptions = EPCGAttachOptions::Attached;

	/**
	 * Functions to call on the target actor after components are added.
	 * Functions must be parameterless with the "CallInEditor" flag enabled.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FName> PostProcessFunctionNames;

	/** Attribute name for the output component reference (FSoftObjectPath). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName ComponentReferenceAttributeName = TEXT("ComponentReference");

	/** Whether to enable detailed logging */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Debug)
	bool bEnableDebugLogging = false;
};

class PGLPCGNODES_API FPGLAddComponentElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
