// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"

#include "PGLGetFoliageInstances.generated.h"

/**
 * Reads the level's AInstancedFoliageActor and outputs one point per foliage
 * instance. Each point carries the instance transform plus a MeshReference
 * (FSoftObjectPath) attribute identifying the source static mesh.
 *
 * Outputs one tagged data per foliage type, tagged with the mesh asset name.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PGLPCGNODES_API UPGLGetFoliageInstancesSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("PGLGetFoliageInstances")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
	virtual FText GetNodeTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor(0.2f, 0.6f, 0.2f); }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;

public:
	/** If set, only output instances whose foliage type uses this mesh. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TSoftObjectPtr<UStaticMesh> FilterMesh;

	/** Attribute name for the mesh soft-object-path written to each point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName MeshAttributeName = TEXT("MeshReference");

	/** Attribute name for the foliage type soft-object-path written to each point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName FoliageTypeAttributeName = TEXT("FoliageType");

	/** Whether to output a single merged data or one data per foliage type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bSplitByFoliageType = false;
};

class PGLPCGNODES_API FPGLGetFoliageInstancesElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
