// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGContext.h"
#include "Fitting/PCGExFitting.h"

#include "PGLStagingDistribute.generated.h"

class UPCGExAssetCollection;

/**
 * Where the node gets its collection(s) from.
 */
UENUM(BlueprintType)
enum class EPGLCollectionSource : uint8
{
	/** Use a single collection reference from settings. */
	Asset UMETA(DisplayName = "Asset"),

	/** Read a collection path from a per-point attribute (supports multiple collections). */
	PointAttribute UMETA(DisplayName = "Point Attribute"),
};

/**
 * How to extract the test value from each point.
 */
UENUM(BlueprintType)
enum class EPGLPointValueSource : uint8
{
	/** World-space Z position (height). */
	PositionZ UMETA(DisplayName = "Height (Position Z)"),

	/** Angle between point normal and world up, in degrees (0 = flat, 90 = vertical wall, 180 = ceiling). */
	SlopeAngle UMETA(DisplayName = "Slope Angle (degrees)"),

	/** Dot product of point normal with world up: 1 = flat, 0 = vertical, -1 = ceiling. */
	NormalDotUp UMETA(DisplayName = "Normal Dot Up (1=flat, 0=wall)"),

	/** Point density value. */
	Density UMETA(DisplayName = "Density"),

	/** A named attribute on the point. */
	Attribute UMETA(DisplayName = "Custom Attribute"),

	/**
	 * Post-selection spatial filter: the "value" is the distance from this point to the nearest
	 * already-assigned point with the same entry. Points that violate the criterion range are
	 * removed after entry selection. Use FloatRange or SeparateMinMax to define the distance bounds
	 * per entry (e.g. a MinSpacing property on the entry). The first assigned point for each entry
	 * always passes (no same-entry neighbours yet).
	 */
	MinDistanceToSameEntry UMETA(DisplayName = "Min Distance To Same Entry"),
};

/**
 * How the entry's range is stored in the collection property overrides.
 */
UENUM(BlueprintType)
enum class EPGLCriterionPropertyMode : uint8
{
	/** Two separate Float properties define the lower and upper bounds. */
	SeparateMinMax UMETA(DisplayName = "Separate Min/Max"),

	/** A single Float Range property defines both bounds. */
	FloatRange UMETA(DisplayName = "Float Range"),

	/** A single property compared for equality (within tolerance), also supports bool, int, ObjectPath. */
	FloatEquals UMETA(DisplayName = "Value Equals"),
};

/**
 * One distribution criterion: maps a range on collection entries to a point
 * value source. An entry passes the criterion if the point's value falls
 * within the range.
 *
 * The range can come from two separate Float properties (Min/Max) or a
 * single Float Range property.
 */
USTRUCT(BlueprintType)
struct PGLPCGNODES_API FPGLStagingCriterion
{
	GENERATED_BODY()

	/** How the range is stored on collection entries. */
	UPROPERTY(EditAnywhere, Category = Settings)
	EPGLCriterionPropertyMode PropertyMode = EPGLCriterionPropertyMode::FloatRange;

	/** Property name for the Float Range property (FloatRange mode). */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "PropertyMode == EPGLCriterionPropertyMode::FloatRange", EditConditionHides))
	FName RangePropertyName = NAME_None;

	/** Property name on the collection entry for the lower bound (Separate Min/Max mode). */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "PropertyMode == EPGLCriterionPropertyMode::SeparateMinMax", EditConditionHides))
	FName MinPropertyName = NAME_None;

	/** Property name on the collection entry for the upper bound (Separate Min/Max mode). */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "PropertyMode == EPGLCriterionPropertyMode::SeparateMinMax", EditConditionHides))
	FName MaxPropertyName = NAME_None;

	/** Property name for the Float property to compare for equality (FloatEquals mode). */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "PropertyMode == EPGLCriterionPropertyMode::FloatEquals", EditConditionHides))
	FName EqualsPropertyName = NAME_None;

	/** Tolerance for floating point equality comparison (FloatEquals mode). Ignored if comparing against non-float*/
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "PropertyMode == EPGLCriterionPropertyMode::FloatEquals", EditConditionHides))
	float EqualsTolerance = 0.001f;

	/** What value to read from each point for comparison. */
	UPROPERTY(EditAnywhere, Category = Settings)
	EPGLPointValueSource PointSource = EPGLPointValueSource::PositionZ;

	/** Attribute name when PointSource is set to Custom Attribute. */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "PointSource == EPGLPointValueSource::Attribute", EditConditionHides))
	FName CustomAttributeName = NAME_None;

	/**
	 * When true, entries that lack the range properties for this criterion
	 * are excluded (fail the test). When false, missing properties are treated
	 * as unbounded and the entry passes.
	 */
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bExcludeIfMissing = false;
};

namespace PGLStagingDistributeConstants
{
	const FName OutputLabel = TEXT("Out");
	const FName MapLabel = TEXT("Map");
}

/**
 * Distributes asset collection entries to points by testing named property
 * overrides on each entry against point attributes (height, slope, etc.).
 *
 * Supports multiple collections: either a single asset reference or a per-point
 * attribute containing collection paths.
 *
 * The user defines range properties on collection entries — either a single
 * Float Range property or separate Float Min/Max properties — then adds
 * matching criteria here. Only entries whose ranges contain the point's value
 * are eligible. Among eligible entries, selection uses weighted random based
 * on entry Weight.
 *
 * Points that match no entry are excluded from the output.
 *
 * Output:
 *   Out pin — matched points with PCGEx/CollectionEntry (int64) attribute
 *   Map pin — Collection Map attribute set (compatible with PCGEx pipeline)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PGLPCGNODES_API UPGLStagingDistributeSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
	virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor(0.2f, 0.4f, 0.8f); }
	virtual void GetStaticTrackedKeys(FPCGSelectionKeyToSettingsMap& OutKeysToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const override;
	virtual bool CanDynamicallyTrackKeys() const override { return true; }
#endif
	virtual bool UseSeed() const override { return true; }

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;

public:
	/** Where to get the collection(s) from. */
	UPROPERTY(EditAnywhere, Category = "Collection")
	EPGLCollectionSource CollectionSource = EPGLCollectionSource::Asset;

	/** The PCGEx asset collection to distribute from (Asset mode). */
	UPROPERTY(EditAnywhere, Category = "Collection", meta = (PCG_Overridable, EditCondition = "CollectionSource == EPGLCollectionSource::Asset", EditConditionHides))
	TSoftObjectPtr<UPCGExAssetCollection> Collection;

	/** Attribute on each point containing the collection asset path (Point Attribute mode). */
	UPROPERTY(EditAnywhere, Category = "Collection", meta = (PCG_Overridable, EditCondition = "CollectionSource == EPGLCollectionSource::PointAttribute", EditConditionHides))
	FName CollectionAttributeName = FName("CollectionPath");

	/** Criteria that filter which entries are eligible for each point. */
	UPROPERTY(EditAnywhere, Category = "Criteria")
	TArray<FPGLStagingCriterion> Criteria;

	/**
	 * Controls when per-entry random variations (offset, rotation, scale) are applied.
	 * The actual variation ranges come from each collection entry's Variations settings.
	 * "Before fitting" applies variations before bounds calculations;
	 * "After fitting" applies them to the final transform.
	 */
	UPROPERTY(EditAnywhere, Category = "Variations", meta=(PCG_NotOverridable))
	FPCGExFittingVariationsDetails Variations;

	/** If enabled, points matched to empty entries (no valid staging bounds) are removed from the output. */
	UPROPERTY(EditAnywhere, Category = "Settings|Output", meta = (PCG_Overridable))
	bool bPruneEmptyPoints = true;

	/** When enabled, updates each output point's local bounds to match the staging bounds of the assigned collection entry. */
	UPROPERTY(EditAnywhere, Category = "Settings|Bounds", meta = (PCG_NotOverridable))
	bool bUpdateBoundsFromEntries = false;

	/** When enabled, only entries matching the per-point category attribute are eligible. */
	UPROPERTY(EditAnywhere, Category = "Category")
	bool bUseCategory = false;

	/** Attribute on each point containing the category name (FName or FString). */
	UPROPERTY(EditAnywhere, Category = "Category", meta = (PCG_Overridable, EditCondition = "bUseCategory", EditConditionHides))
	FName CategoryAttributeName = FName("Category");
};

/**
 * Custom context that holds collection data loaded during PrepareData (main thread),
 * so that ExecuteInternal can run off the main thread without blocking on asset loads.
 */
struct PGLPCGNODES_API FPGLStagingDistributeContext : public FPCGContext
{
	TSharedPtr<struct FPGLStagingDistributePreparedData> PreparedData;
};

class FPGLStagingDistributeElement : public IPCGElement
{
protected:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override;
	virtual FPCGContext* CreateContext() override;
	virtual bool PrepareDataInternal(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual EPCGElementExecutionLoopMode ExecutionLoopMode(const UPCGSettings* Settings) const override { return EPCGElementExecutionLoopMode::SinglePrimaryPin; }
};
