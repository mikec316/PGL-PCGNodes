// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PGLFloatRangeProperty.h"

#include "Data/PCGExData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Types/PCGExTypeOps.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PGLFloatRangeProperty)

// ---------------------------------------------------------------------------
// FPGLProperty_FloatRange — manual implementations (Value type != Output type)
//
// Value:  two separate floats (Min, Max)
// Output: FVector2D (X = Min, Y = Max)
// ---------------------------------------------------------------------------

bool FPGLProperty_FloatRange::InitializeOutput(
	const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName)
{
	OutputBuffer = OutputFacade->GetWritable<FVector2D>(
		OutputName, AsVector2D(), true, PCGExData::EBufferInit::Inherit);
	return OutputBuffer.IsValid();
}

void FPGLProperty_FloatRange::WriteOutput(int32 PointIndex) const
{
	check(OutputBuffer);
	OutputBuffer->SetValue(PointIndex, AsVector2D());
}

void FPGLProperty_FloatRange::WriteOutputFrom(int32 PointIndex, const FPCGExProperty* Source) const
{
	check(OutputBuffer);
	const FPGLProperty_FloatRange* Typed = static_cast<const FPGLProperty_FloatRange*>(Source);
	OutputBuffer->SetValue(PointIndex, Typed->AsVector2D());
}

void FPGLProperty_FloatRange::CopyValueFrom(const FPCGExProperty* Source)
{
	const FPGLProperty_FloatRange* Typed = static_cast<const FPGLProperty_FloatRange*>(Source);
	Min = Typed->Min;
	Max = Typed->Max;
}

FPCGMetadataAttributeBase* FPGLProperty_FloatRange::CreateMetadataAttribute(
	UPCGMetadata* Metadata, FName AttributeName) const
{
	return Metadata->CreateAttribute<FVector2D>(AttributeName, AsVector2D(), true, true);
}

void FPGLProperty_FloatRange::WriteMetadataValue(
	FPCGMetadataAttributeBase* Attribute, int64 EntryKey) const
{
	static_cast<FPCGMetadataAttribute<FVector2D>*>(Attribute)->SetValue(EntryKey, AsVector2D());
}

bool FPGLProperty_FloatRange::TryWriteValue(EPCGMetadataTypes TargetType, void* OutBuffer) const
{
	const FVector2D Projected = AsVector2D();
	PCGExTypeOps::FConversionTable::Convert(EPCGMetadataTypes::Vector2, &Projected, TargetType, OutBuffer);
	return true;
}
