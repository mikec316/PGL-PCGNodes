// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelPinType.h"

// Maps Voxel Plugin pin types to HLSL type strings and PCG accessors
class PGLPCGNODESEDITOR_API FPGLVoxelPinTypeMapper
{
public:
	// Convert a voxel pin type to its HLSL equivalent (e.g. float, int, float3, float4)
	static FString ToHLSLType(const FVoxelPinType& VoxelType);

	// Convert a voxel default value string to an HLSL literal
	static FString GetDefaultLiteral(const FVoxelPinType& VoxelType, const FString& DefaultValue = TEXT(""));

	// Returns true if this voxel type can be represented in HLSL
	static bool IsTranslatable(const FVoxelPinType& VoxelType);

	// Generate a PCG attribute read expression
	// e.g. "In_GetFloat(In_DataIndex, ElementIndex, 'Density')"
	static FString MakePCGReadExpression(
		const FVoxelPinType& VoxelType,
		const FString& PinName,
		const FString& AttributeName);

	// Generate a PCG attribute write statement
	// e.g. "Out_SetFloat(Out_DataIndex, ElementIndex, value, 'Density');"
	static FString MakePCGWriteStatement(
		const FVoxelPinType& VoxelType,
		const FString& PinName,
		const FString& VariableName,
		const FString& AttributeName);

private:
	static FString GetPCGAccessorSuffix(const FVoxelPinType& VoxelType);
};
