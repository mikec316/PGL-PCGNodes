// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGElement.h"
#include "PCGCommon.h"
#include "Elements/PCGSpawnActor.h"
#include "PCGSpawnVoxelSculptActor.generated.h"

// Forward declarations
class UPCGPointData;
class AActor;
class UPCGComponent;
class AVoxelSculptHeight;
class AVoxelSculptVolume;
class AVoxelSculptActorBase;
class UVoxelHeightLayer;
class UVoxelVolumeLayer;

// Voxel enums
enum class EVoxelStampBehavior : uint8;
enum class EVoxelHeightBlendMode : uint8;
enum class EVoxelVolumeBlendMode : uint8;

UENUM(BlueprintType)
enum class EPCGVoxelSculptActorType : uint8
{
	Height UMETA(DisplayName = "Height Sculpt"),
	Volume UMETA(DisplayName = "Volume Sculpt")
};

/**
 * PCG node that spawns VoxelSculpt actors (Height or Volume) and configures their properties
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PGLPCGNODES_API UPCGSpawnVoxelSculptActorSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGSpawnVoxelSculptActorSettings();

	//~Begin UPCGSettings interface
	virtual FPCGElementPtr CreateElement() const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

	/** Default actor type to spawn if no attribute is specified */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	EPCGVoxelSculptActorType DefaultActorType = EPCGVoxelSculptActorType::Height;

	/** If set, will read the actor type from this attribute (expects "Height" or "Volume") */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	FName ActorTypeAttribute = NAME_None;

	/** Attribute name that contains the Soft Object Path for the ExternalAsset */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	FName ExternalAssetPathAttribute = "ExternalAssetPath";

	/** Attribute name for Height Layer path (expects FSoftObjectPath as string) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Height Settings")
	FName HeightLayerPathAttribute = "HeightLayerPath";

	/** Default Height Layer to use if no attribute is specified */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Height Settings")
	TSoftObjectPtr<UVoxelHeightLayer> DefaultHeightLayer;

	/** Attribute name for Volume Layer path (expects FSoftObjectPath as string) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Volume Settings")
	FName VolumeLayerPathAttribute = "VolumeLayerPath";

	/** Default Volume Layer to use if no attribute is specified */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Volume Settings")
	TSoftObjectPtr<UVoxelVolumeLayer> DefaultVolumeLayer;

	// Height Sculpt Properties
	/** Attribute name for ScaleXY value (Height only) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Height Settings")
	FName ScaleXYAttribute = "ScaleXY";

	/** Default ScaleXY value for Height sculpts */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Height Settings")
	float DefaultScaleXY = 100.0f;

	/** Attribute name for bStoreRelativeHeights (Height only) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Height Settings")
	FName StoreRelativeHeightsAttribute = "StoreRelativeHeights";

	/** Default bStoreRelativeHeights value for Height sculpts */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Height Settings")
	bool bDefaultStoreRelativeHeights = true;

	/** Attribute name for MaxErrorPercentage (Height only) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Height Settings|Advanced")
	FName HeightMaxErrorPercentageAttribute = "MaxErrorPercentage";

	/** Default MaxErrorPercentage value for Height sculpts */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Height Settings|Advanced", meta = (UIMin = 0.0001, UIMax = 0.01, ClampMin = 0, ClampMax = 1))
	float DefaultHeightMaxErrorPercentage = 0.005f;

	/** Attribute name for NearMaxLOD (Height only) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Height Settings|Advanced")
	FName HeightNearMaxLODAttribute = "NearMaxLOD";

	/** Default NearMaxLOD value for Height sculpts */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Height Settings|Advanced")
	int32 DefaultHeightNearMaxLOD = 3;

	/** Attribute name for MidMaxLOD (Height only) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Height Settings|Advanced")
	FName HeightMidMaxLODAttribute = "MidMaxLOD";

	/** Default MidMaxLOD value for Height sculpts */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Height Settings|Advanced")
	int32 DefaultHeightMidMaxLOD = 6;

	// Volume Sculpt Properties
	/** Attribute name for Scale value (Volume only) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Volume Settings")
	FName ScaleAttribute = "Scale";

	/** Default Scale value for Volume sculpts */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Volume Settings")
	float DefaultScale = 100.0f;

	/** Attribute name for bStoreMovableDistances (Volume only) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Volume Settings")
	FName StoreMovableDistancesAttribute = "StoreMovableDistances";

	/** Default bStoreMovableDistances value for Volume sculpts */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Volume Settings")
	bool bDefaultStoreMovableDistances = true;

	/** Attribute name for MaxErrorPercentage (Volume only) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Volume Settings|Advanced")
	FName VolumeMaxErrorPercentageAttribute = "MaxErrorPercentage";

	/** Default MaxErrorPercentage value for Volume sculpts */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Volume Settings|Advanced", meta = (UIMin = 0.0001, UIMax = 0.01, ClampMin = 0, ClampMax = 1))
	float DefaultVolumeMaxErrorPercentage = 0.005f;

	/** Attribute name for NearMaxLOD (Volume only) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Volume Settings|Advanced")
	FName VolumeNearMaxLODAttribute = "NearMaxLOD";

	/** Default NearMaxLOD value for Volume sculpts */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Volume Settings|Advanced")
	int32 DefaultVolumeNearMaxLOD = 3;

	/** Attribute name for MidMaxLOD (Volume only) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Volume Settings|Advanced")
	FName VolumeMidMaxLODAttribute = "MidMaxLOD";

	/** Default MidMaxLOD value for Volume sculpts */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Volume Settings|Advanced")
	int32 DefaultVolumeMidMaxLOD = 6;

	// Common Properties
	/** Attribute name for bIsInfinite */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	FName IsInfiniteAttribute = "IsInfinite";

	/** Default bIsInfinite value if attribute is not provided */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	bool bDefaultIsInfinite = false;

	// Common Stamp Properties (from FVoxelStamp base)
	/** Attribute name for Behavior */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Stamp Settings")
	FName BehaviorAttribute = "Behavior";

	/** Default Behavior value (bitflags: AffectShape=1, AffectSurfaceType=2, AffectMetadata=4, AffectAll=7) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Stamp Settings")
	uint8 DefaultBehavior = 7; // AffectAll

	/** Attribute name for Priority */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Stamp Settings")
	FName PriorityAttribute = "Priority";

	/** Default Priority value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Stamp Settings")
	int32 DefaultPriority = 0;

	/** Attribute name for Smoothness */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Stamp Settings")
	FName SmoothnessAttribute = "Smoothness";

	/** Default Smoothness value (in cm) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Stamp Settings", meta = (Units = cm, ClampMin = 0))
	float DefaultSmoothness = 100.0f;

	// Height Stamp Specific Properties
	/** Attribute name for Height BlendMode (0=Max, 1=Min, 2=Override) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Height Settings")
	FName HeightBlendModeAttribute = "BlendMode";

	/** Default Height BlendMode (0=Max, 1=Min, 2=Override) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Height Settings")
	uint8 DefaultHeightBlendMode = 0; // Max

	/** Attribute name for HeightPaddingMultiplier */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Height Settings|Advanced")
	FName HeightPaddingMultiplierAttribute = "HeightPaddingMultiplier";

	/** Default HeightPaddingMultiplier value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Height Settings|Advanced", meta = (UIMin = 0, UIMax = 5))
	float DefaultHeightPaddingMultiplier = 0.1f;

	// Volume Stamp Specific Properties
	/** Attribute name for Volume BlendMode (0=Additive, 1=Subtractive, 2=Intersect, 3=Override) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Volume Settings")
	FName VolumeBlendModeAttribute = "BlendMode";

	/** Default Volume BlendMode (0=Additive, 1=Subtractive, 2=Intersect, 3=Override) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Volume Settings")
	uint8 DefaultVolumeBlendMode = 0; // Additive

	/** Attribute name for BoundsExtensionMultiplier */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Volume Settings|Advanced")
	FName BoundsExtensionMultiplierAttribute = "BoundsExtensionMultiplier";

	/** Default BoundsExtensionMultiplier value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Volume Settings|Advanced", meta = (UIMin = 0, UIMax = 5))
	float DefaultBoundsExtensionMultiplier = 1.0f;

	/** Attribute name for MaximumBoundsExtension */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Volume Settings|Advanced")
	FName MaximumBoundsExtensionAttribute = "MaximumBoundsExtension";

	/** Default MaximumBoundsExtension value (in cm) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Volume Settings|Advanced", meta = (ClampMin = 0))
	float DefaultMaximumBoundsExtension = 1000.0f;

	/** Controls where spawned actors will appear in the Outliner. Note that attaching actors to an actor couples their streaming. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	EPCGAttachOptions AttachOptions = EPCGAttachOptions::Attached;

	/** Root actor to attach/parent spawned actors to (if AttachOptions != NotAttached) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
	TSoftObjectPtr<AActor> RootActor;

	/** Controls whether to trigger generation on any PCG components found on the spawned actors */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	EPCGSpawnActorGenerationTrigger GenerationTrigger = EPCGSpawnActorGenerationTrigger::Default;

	/** Whether to inherit actor tags from the PCG component's owner */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	bool bInheritActorTags = false;

	/** Additional tags to add to spawned actors */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	TArray<FName> TagsToAddOnActors;

	/** Whether to enable detailed logging */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debug")
	bool bEnableDebugLogging = false;

protected:
#if WITH_EDITOR
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGSpawnVoxelSculptActorSettings", "NodeTitle", "Spawn Voxel Sculpt Actor"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif
};

class FPCGSpawnVoxelSculptActorElement : public IPCGElement
{
public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

	// Force execution on game thread since we're spawning actors
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	/** Determines the actor type from attribute or settings */
	EPCGVoxelSculptActorType DetermineActorType(
		const UPCGSpawnVoxelSculptActorSettings* Settings,
		const UPCGMetadata* Metadata,
		const FPCGPoint& Point) const;

	/** Spawns and configures a VoxelSculpt actor */
	AVoxelSculptActorBase* SpawnAndConfigureActor(
		FPCGContext* Context,
		const UPCGSpawnVoxelSculptActorSettings* Settings,
		EPCGVoxelSculptActorType ActorType,
		const FTransform& Transform,
		const UPCGMetadata* Metadata,
		const FPCGPoint& Point) const;

	/** Sets the ExternalAsset property and validates type compatibility */
	bool SetExternalAsset(
		AVoxelSculptActorBase* Actor,
		EPCGVoxelSculptActorType ActorType,
		const FSoftObjectPath& AssetPath,
		FPCGContext* Context,
		bool bEnableLogging) const;

	/** Configures stamp properties for Height sculpt */
	void ConfigureHeightStampProperties(
		AVoxelSculptHeight* Actor,
		const UPCGSpawnVoxelSculptActorSettings* Settings,
		const UPCGMetadata* Metadata,
		const FPCGPoint& Point) const;

	/** Configures stamp properties for Volume sculpt */
	void ConfigureVolumeStampProperties(
		AVoxelSculptVolume* Actor,
		const UPCGSpawnVoxelSculptActorSettings* Settings,
		const UPCGMetadata* Metadata,
		const FPCGPoint& Point) const;

	/** Gets the list of tags to apply to spawned actors */
	TArray<FName> GetNewActorTags(
		FPCGContext* Context,
		AActor* TargetActor,
		const UPCGSpawnVoxelSculptActorSettings* Settings) const;
};
