// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGNodes/PGLBaseSettings.h"
#include "PGLFoliagePaletteSettings.generated.h"

/**
 * PGL replacement for PVE's Foliage Palette node.
 *
 * The PVE version (UPVFoliagePaletteSettings) marks its FoliageMeshes array
 * with EditFixedSize, preventing users from adding or removing entries via the
 * Details panel.  This PGL version drops that restriction so that the foliage
 * mesh list can be freely edited.
 *
 * Functionality:
 *   - When bOverrideFoliage is OFF the array is auto-populated from the
 *     upstream collection's FFoliageFacade names (read-only mirror).
 *   - When bOverrideFoliage is ON the user may add, remove, or replace entries.
 *     The updated list is written back to the FFoliageFacade on execution.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PGLPCGNODES_API UPGLFoliagePaletteSettings : public UPGLBaseSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName        GetDefaultNodeName()  const override { return FName(TEXT("PGLFoliagePalette")); }
	virtual FText        GetDefaultNodeTitle() const override;
	virtual FText        GetNodeTooltipText()  const override;
	virtual FLinearColor GetNodeTitleColor()   const override { return FLinearColor::Green; }

	virtual TArray<EPVRenderType> GetDefaultRenderType()    const override { return TArray{EPVRenderType::FoliageGrid}; }
	virtual TArray<EPVRenderType> GetSupportedRenderTypes() const override { return TArray{EPVRenderType::FoliageGrid, EPVRenderType::Foliage, EPVRenderType::Mesh}; }

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual FPCGDataTypeIdentifier GetInputPinTypeIdentifier()  const override;
	virtual FPCGDataTypeIdentifier GetOutputPinTypeIdentifier() const override;
	virtual FPCGElementPtr         CreateElement()              const override;

public:
	UPROPERTY(EditAnywhere, Category = "Meshes", meta = (InlineEditConditionToggle))
	bool bOverrideFoliage = false;

	/**
	 * List of foliage meshes.
	 *
	 * When bOverrideFoliage is OFF the array mirrors the upstream collection
	 * (entries are read-only in that mode via EditCondition).
	 *
	 * When bOverrideFoliage is ON entries can be freely added, removed, or
	 * replaced.  Each entry's index corresponds to the FFoliageFacade NameId
	 * used by foliage instances in the collection.
	 */
	UPROPERTY(EditAnywhere, Category = "Meshes",
		meta = (NoResetToDefault, EditCondition = "bOverrideFoliage",
			AllowedClasses = "/Script/Engine.StaticMesh,/Script/Engine.SkeletalMesh"))
	TArray<TSoftObjectPtr<UObject>> FoliageMeshes;
};


class PGLPCGNODES_API FPGLFoliagePaletteElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
