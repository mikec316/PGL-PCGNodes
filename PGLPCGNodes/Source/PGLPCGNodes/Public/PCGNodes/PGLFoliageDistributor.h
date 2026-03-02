// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGNodes/PGLBaseSettings.h"
#include "PVFloatRamp.h"
#include "Implementations/PVFoliage.h"   // EPhyllotaxyType, EPhyllotaxyFormation
#include "PGLFoliageDistributor.generated.h"

/**
 * PGL wrapper around PVE's Foliage Distributor node.
 *
 * Delegates execution to PVE's FPVFoliageDistributorElement at runtime via
 * UClass reflection while propagating FrameMeshPath / FrameMeshTransform
 * through the output collection.
 *
 * Input : UPVMeshData         (from Mesh Builder)
 * Output: UPVFoliageMeshData  (foliage instances ready for spawning)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PGLPCGNODES_API UPGLFoliageDistributorSettings : public UPGLBaseSettings
{
	GENERATED_BODY()

public:
	UPGLFoliageDistributorSettings();

#if WITH_EDITOR
	virtual FName        GetDefaultNodeName()  const override { return FName(TEXT("PGLFoliageDistributor")); }
	virtual FText        GetDefaultNodeTitle() const override;
	virtual FText        GetNodeTooltipText()  const override;
	virtual FLinearColor GetNodeTitleColor()   const override { return FLinearColor::Blue; }

	virtual TArray<EPVRenderType> GetDefaultRenderType()    const override { return { EPVRenderType::Mesh, EPVRenderType::Foliage }; }
	virtual TArray<EPVRenderType> GetSupportedRenderTypes() const override { return { EPVRenderType::PointData, EPVRenderType::Mesh, EPVRenderType::Foliage }; }
#endif

protected:
	virtual FPCGDataTypeIdentifier GetInputPinTypeIdentifier()  const override;
	virtual FPCGDataTypeIdentifier GetOutputPinTypeIdentifier() const override;
	virtual FPCGElementPtr         CreateElement()              const override;

public:

	// ---- Distribution Settings ----

	UPROPERTY(EditAnywhere, Category="Distribution Settings",
		meta=(ClampMin=0.0f, ClampMax=1.0f, UIMin=0.0f, UIMax=1.0f,
			Tooltip="Threshold controlling foliage release based on an ethylene-like signal.\nHigher Threshold = denser, bushier plant.\nLower Threshold = sparser, pruned structure."))
	float EthyleneThreshold = 0.85f;

	UPROPERTY(EditAnywhere, Category="Distribution Settings",
		meta=(Tooltip="Override default placement with custom distribution rules."))
	bool OverrideDistribution = true;

	UPROPERTY(EditAnywhere, Category="Distribution Settings",
		meta=(EditCondition="OverrideDistribution", ClampMin=0.0f, ClampMax=1.0f, UIMin=0.0f, UIMax=1.0f,
			Tooltip="Minimum distance between placed instances.\nHigher spacing yields a more open distribution."))
	float InstanceSpacing = 0.3f;

	UPROPERTY(EditAnywhere, Category="Distribution Settings",
		meta=(EditCondition="OverrideDistribution", XAxisMin=0.0f, XAxisMax=1.0f, YAxisMin=0.0f, YAxisMax=1.0f,
			Tooltip="Ramp that varies instance spacing across the height of the plant."))
	FPVFloatRamp InstanceSpacingRamp;

	UPROPERTY(EditAnywhere, Category="Distribution Settings",
		meta=(EditCondition="OverrideDistribution", ClampMin=0.0f, ClampMax=1.0f, UIMin=0.0f, UIMax=1.0f,
			Tooltip="Strength of the spacing ramp's influence."))
	float InstanceSpacingRampEffect = 0.0f;

	UPROPERTY(EditAnywhere, Category="Distribution Settings",
		meta=(EditCondition="OverrideDistribution", ClampMin=-1, ClampMax=1000,
			Tooltip="Caps the number of instances per branch. -1 = unlimited."))
	int32 MaxPerBranch = -1;

	// ---- Scale Settings ----

	UPROPERTY(EditAnywhere, Category="Scale Settings",
		meta=(EditCondition="OverrideDistribution", ClampMin=0.0f, UIMin=0.0f, UIMax=5.0f,
			Tooltip="Default size multiplier for placed foliage."))
	float BaseScale = 1.0f;

	UPROPERTY(EditAnywhere, Category="Scale Settings",
		meta=(EditCondition="OverrideDistribution", ClampMin=0.0f, ClampMax=1.0f, UIMin=0.0f, UIMax=1.0f,
			Tooltip="Impact of branch size on foliage size."))
	float BranchScaleImpact = 0.0f;

	UPROPERTY(EditAnywhere, Category="Scale Settings",
		meta=(EditCondition="OverrideDistribution", ClampMin=0.0f, UIMin=0.0f, UIMax=5.0f,
			Tooltip="Minimum allowed instance size."))
	float MinScale = 0.5f;

	UPROPERTY(EditAnywhere, Category="Scale Settings",
		meta=(EditCondition="OverrideDistribution", ClampMin=0.0f, UIMin=0.0f, UIMax=5.0f,
			Tooltip="Maximum allowed instance size."))
	float MaxScale = 1.0f;

	UPROPERTY(EditAnywhere, Category="Scale Settings",
		meta=(EditCondition="OverrideDistribution", ClampMin=0.0f, UIMin=0.0f, UIMax=5.0f,
			Tooltip="Lower bound for per-instance size variation."))
	float RandomScaleMin = 1.0f;

	UPROPERTY(EditAnywhere, Category="Scale Settings",
		meta=(EditCondition="OverrideDistribution", ClampMin=1.0f, UIMin=1.0f, UIMax=5.0f,
			Tooltip="Upper bound for per-instance size variation."))
	float RandomScaleMax = 1.0f;

	UPROPERTY(EditAnywhere, Category="Scale Settings",
		meta=(EditCondition="OverrideDistribution", XAxisMin=0.0f, XAxisMax=1.0f, YAxisMin=0.0f, YAxisMax=1.0f,
			Tooltip="Curve that varies scale across the height of the plant."))
	FPVFloatRamp ScaleRamp;

	// ---- Vector Settings ----

	UPROPERTY(EditAnywhere, Category="Vector Settings",
		meta=(EditCondition="OverrideDistribution",
			Tooltip="Use a custom axil angle instead of the default."))
	bool OverrideAxilAngle = true;

	UPROPERTY(EditAnywhere, Category="Vector Settings",
		meta=(EditCondition="OverrideDistribution", ClampMin=0.0f, ClampMax=90.0f, UIMin=0.0f, UIMax=90.0f,
			Tooltip="Base tilt from the parent axis (degrees)."))
	float AxilAngle = 45.0f;

	UPROPERTY(EditAnywhere, Category="Vector Settings",
		meta=(EditCondition="OverrideDistribution", XAxisMin=0.0f, XAxisMax=1.0f, YAxisMin=0.0f, YAxisMax=1.0f,
			Tooltip="Ramp that varies the axil angle across the height of the plant."))
	FPVFloatRamp AxilAngleRamp;

	UPROPERTY(EditAnywhere, Category="Vector Settings",
		meta=(EditCondition="OverrideDistribution", ClampMin=0.0f, ClampMax=90.0f, UIMin=0.0f, UIMax=90.0f,
			Tooltip="Max angle used when the ramp evaluates to 1."))
	float AxilAngleRampUpperValue = 45.0f;

	UPROPERTY(EditAnywhere, Category="Vector Settings",
		meta=(EditCondition="OverrideDistribution", ClampMin=0.0f, ClampMax=1.0f, UIMin=0.0f, UIMax=1.0f,
			Tooltip="Ramp influence strength for the axil angle."))
	float AxilAngleRampEffect = 0.0f;

	// ---- Phyllotaxy Settings ----

	UPROPERTY(EditAnywhere, Category="Phyllotaxy Settings",
		meta=(EditCondition="OverrideDistribution",
			Tooltip="Use custom phyllotaxy settings instead of defaults."))
	bool OverridePhyllotaxy = false;

	UPROPERTY(EditAnywhere, Category="Phyllotaxy Settings",
		meta=(EditCondition="OverrideDistribution",
			Tooltip="Pattern for arranging buds/leaves around the stem."))
	EPhyllotaxyType PhyllotaxyType = EPhyllotaxyType::Alternate;

	UPROPERTY(EditAnywhere, Category="Phyllotaxy Settings",
		meta=(EditCondition="OverrideDistribution && PhyllotaxyType == EPhyllotaxyType::Spiral", EditConditionHides))
	EPhyllotaxyFormation PhyllotaxyFormation = EPhyllotaxyFormation::Distichous;

	UPROPERTY(EditAnywhere, Category="Phyllotaxy Settings",
		meta=(EditCondition="OverrideDistribution", ClampMin=1, ClampMax=10, UIMin=1, UIMax=10,
			Tooltip="Minimum buds/leaves per node (Whorled phyllotaxy)."))
	int32 MinimumNodeBuds = 2;

	UPROPERTY(EditAnywhere, Category="Phyllotaxy Settings",
		meta=(EditCondition="OverrideDistribution", ClampMin=1, ClampMax=10, UIMin=1, UIMax=10,
			Tooltip="Maximum buds/leaves per node (Whorled phyllotaxy)."))
	int32 MaximumNodeBuds = 3;

	UPROPERTY(EditAnywhere, Category="Phyllotaxy Settings",
		meta=(EditCondition="OverrideDistribution", ClampMin=0.0f, ClampMax=360.0f, UIMin=0.0f, UIMax=360.0f,
			Tooltip="Extra divergence angle added to the phyllotaxy pattern (degrees)."))
	float PhyllotaxyAdditionalAngle = 0.0f;

	// ---- Misc Settings ----

	UPROPERTY(EditAnywhere, Category="Misc Settings",
		meta=(EditCondition="OverrideDistribution"))
	int32 RandomSeed = 123456;

	// ---- Condition Settings ----
	// These must match PVE's UPVFoliageDistributorSettings so they are
	// copied to the PVE settings object during delegation.  PVE's
	// ExecuteInternal mutates them before calling DistributeFoliage.

	UPROPERTY(EditAnywhere, Category="ConditionSettings",
		meta=(ShowOnlyInnerProperties, EditCondition="OverrideDistribution"))
	FPVFoliageDistributionConditions Conditions;

	UPROPERTY()
	FPVFoliageDistributionConditions DefaultConditions;

	UPROPERTY()
	FPVFoliageDistributionConditions CustomConditions;

	UPROPERTY()
	bool bCustomConditionSet = false;
};


class PGLPCGNODES_API FPGLFoliageDistributorElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return false; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
