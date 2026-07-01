// Copyright by Procgen Labs Ltd. All Rights Reserved.

// Picks a mesh per-point from a PCGEx Mesh Collection using weighted
// random distribution seeded by point ID.  Writes the result into the
// FVoxelPointAttributes::Mesh attribute so it can be consumed directly
// by a downstream ScatterMesh node.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelNode.h"
#include "VoxelPointSet.h"
#include "VoxelObjectPinType.h"
#include "Buffer/VoxelBaseBuffers.h"
#include "Buffer/VoxelGraphStaticMeshBuffer.h"
#include "Collections/PCGExMeshCollection.h"

#include "VoxelNode_SelectMeshFromCollection.generated.h"

///////////////////////////////////////////////////////////////////////////////
// Pin type wrapper for UPCGExMeshCollection
///////////////////////////////////////////////////////////////////////////////

USTRUCT(DisplayName = "Mesh Collection")
struct PGLPCGNODES_API FVoxelPCGExMeshCollectionRef
{
	GENERATED_BODY()

	TVoxelObjectPtr<UPCGExMeshCollection> Object;

	FORCEINLINE bool operator==(const FVoxelPCGExMeshCollectionRef& Other) const
	{
		return Object == Other.Object;
	}

	FORCEINLINE friend uint32 GetTypeHash(const FVoxelPCGExMeshCollectionRef& Ref)
	{
		return GetTypeHash(Ref.Object);
	}
};

DECLARE_VOXEL_OBJECT_PIN_TYPE(FVoxelPCGExMeshCollectionRef);

USTRUCT()
struct PGLPCGNODES_API FVoxelPCGExMeshCollectionRefPinType : public FVoxelObjectPinType
{
	GENERATED_BODY()

	DEFINE_VOXEL_OBJECT_PIN_TYPE(FVoxelPCGExMeshCollectionRef, UPCGExMeshCollection)
	{
		if (bSetObject)
		{
			OutObject = Struct.Object;
		}
		else
		{
			Struct.Object = &InObject;
		}
	}
};

///////////////////////////////////////////////////////////////////////////////
// Distribution mode
///////////////////////////////////////////////////////////////////////////////

UENUM()
enum class EVoxelCollectionDistribution : uint8
{
	/** Weighted random — respects each entry's Weight value. */
	WeightedRandom UMETA(DisplayName = "Weighted Random"),

	/** Uniform random — all entries equally likely regardless of Weight. */
	Random UMETA(DisplayName = "Uniform Random"),
};

///////////////////////////////////////////////////////////////////////////////
// Criteria — filter eligible entries by comparing point attributes to
// property overrides on collection entries.
///////////////////////////////////////////////////////////////////////////////

/** Which voxel point attribute supplies the per-point test value. */
UENUM(BlueprintType)
enum class EVoxelPointValueSource : uint8
{
	/** World-space Z position (height). */
	PositionZ UMETA(DisplayName = "Height (Position Z)"),

	/** Surface steepness (FVoxelPointAttributes::Steepness buffer). */
	Steepness UMETA(DisplayName = "Steepness"),

	/** Point density value (FVoxelPointAttributes::Density buffer). */
	Density UMETA(DisplayName = "Density"),

	/** A named float attribute on the point. */
	CustomFloatAttribute UMETA(DisplayName = "Custom Float Attribute"),
};

/** How the entry's range is stored in the collection property overrides. */
UENUM(BlueprintType)
enum class EVoxelCriterionPropertyMode : uint8
{
	/** A single FPGLProperty_FloatRange property defines both bounds. */
	FloatRange UMETA(DisplayName = "Float Range"),

	/** Two separate Float properties define the lower and upper bounds. */
	SeparateMinMax UMETA(DisplayName = "Separate Min/Max"),

	/** A single Float property compared for equality (within tolerance). */
	FloatEquals UMETA(DisplayName = "Float Equals"),

	/** A single Float property compared with a configurable operator (>, <, >=, <=, ==, !=). */
	FloatComparison UMETA(DisplayName = "Float Comparison"),
};

/** Comparison operator applied between the point value and the entry's float property. */
UENUM(BlueprintType)
enum class EVoxelCriterionFloatOperator : uint8
{
	Greater        UMETA(DisplayName = "Greater Than (>)"),
	GreaterOrEqual UMETA(DisplayName = "Greater Than or Equal (>=)"),
	Less           UMETA(DisplayName = "Less Than (<)"),
	LessOrEqual    UMETA(DisplayName = "Less Than or Equal (<=)"),
	Equal          UMETA(DisplayName = "Equal (==)"),
	NotEqual       UMETA(DisplayName = "Not Equal (!=)"),
};

/**
 * One distribution criterion: maps a range on collection entries to a voxel
 * point value source. An entry passes the criterion if the point's value
 * falls within the range.
 */
USTRUCT(BlueprintType)
struct PGLPCGNODES_API FVoxelMeshCriterion
{
	GENERATED_BODY()

	/** How the range is stored on collection entries. */
	UPROPERTY(EditAnywhere, Category = Settings)
	EVoxelCriterionPropertyMode PropertyMode = EVoxelCriterionPropertyMode::FloatRange;

	/** Property name for the Float Range property (FloatRange mode). */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "PropertyMode == EVoxelCriterionPropertyMode::FloatRange", EditConditionHides))
	FName RangePropertyName = NAME_None;

	/** Property name on the collection entry for the lower bound (Separate Min/Max mode). */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "PropertyMode == EVoxelCriterionPropertyMode::SeparateMinMax", EditConditionHides))
	FName MinPropertyName = NAME_None;

	/** Property name on the collection entry for the upper bound (Separate Min/Max mode). */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "PropertyMode == EVoxelCriterionPropertyMode::SeparateMinMax", EditConditionHides))
	FName MaxPropertyName = NAME_None;

	/** Property name for the Float property to compare for equality (FloatEquals mode). */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "PropertyMode == EVoxelCriterionPropertyMode::FloatEquals", EditConditionHides))
	FName EqualsPropertyName = NAME_None;

	/** Tolerance for floating point equality comparison (FloatEquals mode). */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "PropertyMode == EVoxelCriterionPropertyMode::FloatEquals", EditConditionHides))
	float EqualsTolerance = 0.001f;

	/** Property name for the Float property used as the comparison threshold (FloatComparison mode). */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "PropertyMode == EVoxelCriterionPropertyMode::FloatComparison", EditConditionHides))
	FName ComparisonPropertyName = NAME_None;

	/** Comparison operator applied between the point value and the entry's threshold (FloatComparison mode). */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "PropertyMode == EVoxelCriterionPropertyMode::FloatComparison", EditConditionHides))
	EVoxelCriterionFloatOperator ComparisonOperator = EVoxelCriterionFloatOperator::Greater;

	/** Tolerance for Equal/NotEqual comparison operators (FloatComparison mode). */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "PropertyMode == EVoxelCriterionPropertyMode::FloatComparison && (ComparisonOperator == EVoxelCriterionFloatOperator::Equal || ComparisonOperator == EVoxelCriterionFloatOperator::NotEqual)", EditConditionHides))
	float ComparisonTolerance = 0.001f;

	/** What value to read from each point for comparison. */
	UPROPERTY(EditAnywhere, Category = Settings)
	EVoxelPointValueSource PointSource = EVoxelPointValueSource::PositionZ;

	/** Attribute name when PointSource is set to Custom Float Attribute. */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "PointSource == EVoxelPointValueSource::CustomFloatAttribute", EditConditionHides))
	FName CustomAttributeName = NAME_None;

	/**
	 * When true, entries that lack the range properties for this criterion
	 * are excluded (fail the test). When false, missing properties are treated
	 * as unbounded and the entry passes.
	 */
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bExcludeIfMissing = false;
};

///////////////////////////////////////////////////////////////////////////////
// Node
///////////////////////////////////////////////////////////////////////////////

// Picks a mesh per-point from a PCGEx Mesh Collection and writes it
// into the Mesh attribute on the output point set.
//
// When Criteria are defined, only collection entries whose property-override
// ranges contain the point's attribute value are eligible for selection.
//
// Typical wiring:
//   [GeneratePoints] -> [SelectMeshFromCollection] -> [ScatterMesh]
USTRUCT(Category = "Point", DisplayName = "Select Mesh From Collection",
	meta = (Keywords = "mesh collection pcgex distribute random weighted criteria"))
struct PGLPCGNODES_API FVoxelNode_SelectMeshFromCollection : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	// Input point set — must carry an Id attribute.
	VOXEL_INPUT_PIN(FVoxelPointSet, In, nullptr);

	// The PCGEx mesh collection to pick from.
	VOXEL_INPUT_PIN(FVoxelPCGExMeshCollectionRef, Collection, nullptr);

	// Seed for deterministic per-point randomisation.
	VOXEL_INPUT_PIN(FVoxelSeed, Seed, nullptr, AdvancedDisplay);

	// How entries are selected from the collection.
	UPROPERTY(EditAnywhere, Category = "Settings")
	EVoxelCollectionDistribution Distribution = EVoxelCollectionDistribution::WeightedRandom;

	/** Criteria that filter which entries are eligible for each point.
	 *  When empty, all entries are eligible (original behavior). */
	UPROPERTY(EditAnywhere, Category = "Criteria")
	TArray<FVoxelMeshCriterion> Criteria;

	/** When enabled, applies the picked entry's FPCGExFittingVariations
	 *  (random offset, rotation jitter, scale jitter) to the output
	 *  point's Position, Rotation and Scale attributes.
	 *  Variation settings are resolved per-entry via GetVariations(),
	 *  respecting entry-local vs collection-global override rules. */
	UPROPERTY(EditAnywhere, Category = "Variations")
	bool bApplyVariations = false;

	// Output point set — identical to In but with Mesh attribute set
	// (and optionally Position/Rotation/Scale modified by entry variations).
	VOXEL_OUTPUT_PIN(FVoxelPointSet, Out);

	//~ Begin FVoxelNode Interface
	virtual void Compute(FVoxelGraphQuery Query) const override;
	//~ End FVoxelNode Interface
};
