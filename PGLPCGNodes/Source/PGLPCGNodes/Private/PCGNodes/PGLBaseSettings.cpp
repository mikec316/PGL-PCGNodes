// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PGLBaseSettings.h"

#include "PCGContext.h"
#include "DataTypes/PVGrowthData.h"
#include "Helpers/PVUtilities.h"

// =============================================================================
//  Render-type management (editor-only)
// =============================================================================

#if WITH_EDITOR
void UPGLBaseSettings::SetCurrentRenderType(TArray<EPVRenderType> InRenderTypes)
{
	CurrentRenderType = InRenderTypes;
	PostEditChange();
}
#endif

// =============================================================================
//  Pin properties — standard single-pin growth-data I/O
// =============================================================================

TArray<FPCGPinProperties> UPGLBaseSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;

	FPCGPinProperties& Pin = Properties.Emplace_GetRef(
		PCGPinConstants::DefaultInputLabel, GetInputPinTypeIdentifier());
	Pin.SetRequiredPin();
	Pin.SetAllowMultipleConnections(false);
	Pin.bAllowMultipleData = false;

	return Properties;
}

TArray<FPCGPinProperties> UPGLBaseSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;

	FPCGPinProperties& Pin = Properties.Emplace_GetRef(
		PCGPinConstants::DefaultOutputLabel, GetOutputPinTypeIdentifier());
	Pin.SetAllowMultipleConnections(true);
	Pin.bAllowMultipleData = false;

	return Properties;
}

// =============================================================================
//  Pin type identifiers — default to PVE growth data
// =============================================================================

FPCGDataTypeIdentifier UPGLBaseSettings::GetInputPinTypeIdentifier() const
{
	return FPCGDataTypeIdentifier{ FPVDataTypeInfoGrowth::AsId() };
}

FPCGDataTypeIdentifier UPGLBaseSettings::GetOutputPinTypeIdentifier() const
{
	return FPCGDataTypeIdentifier{ FPVDataTypeInfoGrowth::AsId() };
}

// =============================================================================
//  Element
// =============================================================================

FPCGElementPtr UPGLBaseSettings::CreateElement() const
{
	return MakeShared<FPGLBaseElement>();
}

bool FPGLBaseElement::ExecuteInternal(FPCGContext* Context) const
{
	return true;
}
