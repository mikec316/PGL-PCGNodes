// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGExProperty.h"

#include "PGLFloatRangeProperty.generated.h"

// Forward-declare the buffer template from PCGExCore
namespace PCGExData { template <typename T> class TBuffer; }

/**
 * A PCGEx property type storing a float range (min/max pair).
 *
 * Add this to a collection's property schema to define per-entry ranges
 * (e.g. height, slope, temperature) that the PGL Staging Distribute node
 * can test against point attributes.
 *
 * When output to point attributes, the range is written as FVector2D
 * (X = Min, Y = Max).
 */
USTRUCT(BlueprintType, DisplayName = "Float Range")
struct PGLPCGNODES_API FPGLProperty_FloatRange : public FPCGExProperty
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	float Min = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	float Max = 1.0f;

protected:
	// Output as FVector2D (Min → X, Max → Y)
	TSharedPtr<PCGExData::TBuffer<FVector2D>> OutputBuffer;

public:
	virtual bool InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName) override;
	virtual void WriteOutput(int32 PointIndex) const override;
	virtual void WriteOutputFrom(int32 PointIndex, const FPCGExProperty* Source) const override;
	virtual void CopyValueFrom(const FPCGExProperty* Source) override;
	virtual bool SupportsOutput() const override { return true; }
	virtual EPCGMetadataTypes GetOutputType() const override { return EPCGMetadataTypes::Vector2; }
	virtual FName GetTypeName() const override { return FName("FloatRange"); }
	virtual FPCGMetadataAttributeBase* CreateMetadataAttribute(UPCGMetadata* Metadata, FName AttributeName) const override;
	virtual void WriteMetadataValue(FPCGMetadataAttributeBase* Attribute, int64 EntryKey) const override;
	virtual bool TryWriteValue(EPCGMetadataTypes TargetType, void* OutBuffer) const override;

	/** Convenience: get the range as a FVector2D. */
	FORCEINLINE FVector2D AsVector2D() const { return FVector2D(Min, Max); }

	/** Test whether a value falls within [Min, Max]. */
	FORCEINLINE bool Contains(float Value) const { return Value >= Min && Value <= Max; }
};
