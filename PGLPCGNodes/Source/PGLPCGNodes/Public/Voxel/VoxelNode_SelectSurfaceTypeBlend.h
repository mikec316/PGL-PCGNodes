// Copyright by Procgen Labs Ltd. All Rights Reserved.

// Picks a surface type per-element from a PGL Voxel Surface Type Collection
// using weighted random distribution + optional criteria. Outputs a
// FVoxelSurfaceTypeBlendBuffer suitable for downstream surface blending.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelNode.h"
#include "VoxelObjectPinType.h"
#include "Buffer/VoxelBaseBuffers.h"
#include "Surface/VoxelSurfaceTypeBlendBuffer.h"

// Full include — DEFINE_VOXEL_OBJECT_PIN_TYPE references UPGLVoxelSurfaceTypeCollection::StaticClass()
#include "PGLVoxelSurfaceTypeCollection.h"

// Reused for EVoxelCollectionDistribution and EVoxelCriterionPropertyMode
#include "Voxel/VoxelNode_SelectMeshFromCollection.h"

#include "VoxelNode_SelectSurfaceTypeBlend.generated.h"

///////////////////////////////////////////////////////////////////////////////
// Pin type wrapper for UPGLVoxelSurfaceTypeCollection
///////////////////////////////////////////////////////////////////////////////

USTRUCT(DisplayName = "Voxel Surface Type Collection")
struct PGLPCGNODES_API FVoxelPCGExSurfaceTypeCollectionRef
{
	GENERATED_BODY()

	TVoxelObjectPtr<UPGLVoxelSurfaceTypeCollection> Object;

	FORCEINLINE bool operator==(const FVoxelPCGExSurfaceTypeCollectionRef& Other) const
	{
		return Object == Other.Object;
	}

	FORCEINLINE friend uint32 GetTypeHash(const FVoxelPCGExSurfaceTypeCollectionRef& Ref)
	{
		return GetTypeHash(Ref.Object);
	}
};

DECLARE_VOXEL_OBJECT_PIN_TYPE(FVoxelPCGExSurfaceTypeCollectionRef);

USTRUCT()
struct PGLPCGNODES_API FVoxelPCGExSurfaceTypeCollectionRefPinType : public FVoxelObjectPinType
{
	GENERATED_BODY()

	DEFINE_VOXEL_OBJECT_PIN_TYPE(FVoxelPCGExSurfaceTypeCollectionRef, UPGLVoxelSurfaceTypeCollection)
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
// Buffer source — which input pin's value the criterion compares against.
///////////////////////////////////////////////////////////////////////////////

UENUM(BlueprintType)
enum class EVoxelSurfaceTypeBufferSource : uint8
{
	/** Compare the entry's range against the Height input buffer. */
	Height UMETA(DisplayName = "Height"),

	/** Compare the entry's range against the Noise input buffer. */
	Noise  UMETA(DisplayName = "Noise"),
};

///////////////////////////////////////////////////////////////////////////////
// Criterion — filters eligible entries by comparing a per-element buffer
// value against a property-defined range on collection entries.
///////////////////////////////////////////////////////////////////////////////

USTRUCT(BlueprintType)
struct PGLPCGNODES_API FVoxelSurfaceTypeCriterion
{
	GENERATED_BODY()

	/** How the range is stored on collection entries. */
	UPROPERTY(EditAnywhere, Category = Settings)
	EVoxelCriterionPropertyMode PropertyMode = EVoxelCriterionPropertyMode::FloatRange;

	/** Property name for the Float Range property (FloatRange mode). */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "PropertyMode == EVoxelCriterionPropertyMode::FloatRange", EditConditionHides))
	FName RangePropertyName = NAME_None;

	/** Property name for the lower bound (Separate Min/Max mode). */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "PropertyMode == EVoxelCriterionPropertyMode::SeparateMinMax", EditConditionHides))
	FName MinPropertyName = NAME_None;

	/** Property name for the upper bound (Separate Min/Max mode). */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "PropertyMode == EVoxelCriterionPropertyMode::SeparateMinMax", EditConditionHides))
	FName MaxPropertyName = NAME_None;

	/** Property name for the Float property compared for equality (FloatEquals mode). */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "PropertyMode == EVoxelCriterionPropertyMode::FloatEquals", EditConditionHides))
	FName EqualsPropertyName = NAME_None;

	/** Tolerance for floating point equality comparison (FloatEquals mode). */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "PropertyMode == EVoxelCriterionPropertyMode::FloatEquals", EditConditionHides))
	float EqualsTolerance = 0.001f;

	/** Property name for the Float property used as the comparison threshold (FloatComparison mode). */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "PropertyMode == EVoxelCriterionPropertyMode::FloatComparison", EditConditionHides))
	FName ComparisonPropertyName = NAME_None;

	/** Comparison operator applied between the buffer value and the entry's threshold (FloatComparison mode). */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "PropertyMode == EVoxelCriterionPropertyMode::FloatComparison", EditConditionHides))
	EVoxelCriterionFloatOperator ComparisonOperator = EVoxelCriterionFloatOperator::Greater;

	/** Tolerance for Equal/NotEqual comparison operators (FloatComparison mode). */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "PropertyMode == EVoxelCriterionPropertyMode::FloatComparison && (ComparisonOperator == EVoxelCriterionFloatOperator::Equal || ComparisonOperator == EVoxelCriterionFloatOperator::NotEqual)", EditConditionHides))
	float ComparisonTolerance = 0.001f;

	/** Which input buffer supplies the per-element comparison value. */
	UPROPERTY(EditAnywhere, Category = Settings)
	EVoxelSurfaceTypeBufferSource BufferSource = EVoxelSurfaceTypeBufferSource::Height;

	/** When true, entries lacking this criterion's properties fail the test. */
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bExcludeIfMissing = false;
};

///////////////////////////////////////////////////////////////////////////////
// Node
///////////////////////////////////////////////////////////////////////////////

// Picks a surface type per-element from a PGL Voxel Surface Type Collection.
// Output is a single-layer FVoxelSurfaceTypeBlendBuffer (Weight=1) carrying
// the picked type — feed it into MakeSurfaceTypeBlend or downstream blend
// nodes for compositing.
//
// When Criteria are defined, only entries whose property-override ranges
// contain the per-element buffer value are eligible for selection.
//
// Typical wiring:
//   [Heightmap] -> Height ─┐
//   [Noise]     -> Noise   ├─> [SelectSurfaceTypeBlend] -> SurfaceTypeBlend
//   [Collection]-> Coll    ┘
USTRUCT(Category = "Surface Type", DisplayName = "Select Surface Type Blend From Collection",
	meta = (Keywords = "surface type collection pcgex height noise criteria blend"))
struct PGLPCGNODES_API FVoxelNode_SelectSurfaceTypeBlend : public FVoxelNode
{
	GENERATED_BODY()
	GENERATED_VOXEL_NODE_BODY()

	// Per-element height value (typically world Z).
	VOXEL_INPUT_PIN(FVoxelFloatBuffer, Height, nullptr);

	// Per-element noise value used as a secondary criterion source.
	VOXEL_INPUT_PIN(FVoxelFloatBuffer, Noise, nullptr);

	// PGL Voxel Surface Type Collection to pick from.
	VOXEL_INPUT_PIN(FVoxelPCGExSurfaceTypeCollectionRef, Collection, nullptr);

	// Seed for deterministic per-element randomisation.
	VOXEL_INPUT_PIN(FVoxelSeed, Seed, nullptr, AdvancedDisplay);

	// How entries are selected when multiple candidates pass the criteria.
	UPROPERTY(EditAnywhere, Category = "Settings")
	EVoxelCollectionDistribution Distribution = EVoxelCollectionDistribution::WeightedRandom;

	/** Criteria that filter eligible entries per element.
	 *  When empty, all entries are eligible. */
	UPROPERTY(EditAnywhere, Category = "Criteria")
	TArray<FVoxelSurfaceTypeCriterion> Criteria;

	// Single-layer surface type blend buffer (one type per element, Weight=1).
	VOXEL_OUTPUT_PIN(FVoxelSurfaceTypeBlendBuffer, SurfaceTypeBlend);

	//~ Begin FVoxelNode Interface
	virtual void Compute(FVoxelGraphQuery Query) const override;
	//~ End FVoxelNode Interface
};
