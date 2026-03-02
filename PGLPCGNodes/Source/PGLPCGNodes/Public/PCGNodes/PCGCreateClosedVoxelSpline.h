// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "VoxelStampBehavior.h"
#include "VoxelHeightBlendMode.h"
#include "VoxelVolumeBlendMode.h"
#include "Components/SplineComponent.h"
#include "PCGCreateClosedVoxelSpline.generated.h"

class UVoxelLayer;
class UVoxelClosedSplineGraph;
class UVoxelHeightClosedSplineGraph;

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PGLPCGNODES_API UPCGCreateClosedVoxelSplineSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGCreateClosedVoxelSplineSettings();

	//~ Begin UPCGSettings Interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("CreateClosedVoxelSpline")); }
	virtual FText GetDefaultNodeTitle() const override { return INVTEXT("Create Closed Voxel Spline"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

	virtual FString GetAdditionalTitleInformation() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~ End UPCGSettings Interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bVolumeSpline = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition = "!bVolumeSpline", EditConditionHides))
	TObjectPtr<UVoxelHeightClosedSplineGraph> HeightGraph;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PCG_Overridable, EditCondition = "!bVolumeSpline", EditConditionHides))
	EVoxelHeightBlendMode HeightBlendMode = EVoxelHeightBlendMode::Max;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition = "bVolumeSpline", EditConditionHides))
	TObjectPtr<UVoxelClosedSplineGraph> VolumeGraph;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PCG_Overridable, EditCondition = "bVolumeSpline", EditConditionHides))
	EVoxelVolumeBlendMode VolumeBlendMode = EVoxelVolumeBlendMode::Additive;

	// Spline point interpolation type applied to all points
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PCG_Overridable))
	TEnumAsByte<ESplinePointType::Type> SplinePointType = ESplinePointType::Curve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (Units = cm, ClampMin = 0))
	float Smoothness = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	int32 Priority = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	TObjectPtr<UVoxelLayer> Layer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	EVoxelStampBehavior StampBehavior = EVoxelStampBehavior::AffectAll;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGAttachOptions AttachOptions = EPCGAttachOptions::InFolder;
};

class FPCGCreateClosedVoxelSplineElement : public IPCGElement
{
protected:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
