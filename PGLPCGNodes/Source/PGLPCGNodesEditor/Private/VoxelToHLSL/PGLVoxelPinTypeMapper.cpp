// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "VoxelToHLSL/PGLVoxelPinTypeMapper.h"

FString FPGLVoxelPinTypeMapper::ToHLSLType(const FVoxelPinType& VoxelType)
{
	const FVoxelPinType Inner = VoxelType.IsBuffer() ? VoxelType.GetInnerType() : VoxelType;

	switch (Inner.GetInternalType())
	{
	case EVoxelPinInternalType::Bool:   return TEXT("bool");
	case EVoxelPinInternalType::Float:  return TEXT("float");
	case EVoxelPinInternalType::Double: return TEXT("float"); // HLSL has no double on most GPU targets
	case EVoxelPinInternalType::Int32:  return TEXT("int");
	case EVoxelPinInternalType::Int64:  return TEXT("int"); // Downcast, with warning
	case EVoxelPinInternalType::UInt16: return TEXT("uint");
	case EVoxelPinInternalType::Byte:   return TEXT("uint");
	case EVoxelPinInternalType::Struct:
	{
		const UScriptStruct* Struct = Inner.GetStruct();
		if (!Struct)
		{
			return TEXT("float");
		}
		const FName StructName = Struct->GetFName();
		if (StructName == NAME_Vector || StructName == TEXT("Vector"))
		{
			return TEXT("float3");
		}
		if (StructName == TEXT("Vector2D"))
		{
			return TEXT("float2");
		}
		if (StructName == TEXT("Vector4") || StructName == TEXT("Quat") || StructName == TEXT("LinearColor"))
		{
			return TEXT("float4");
		}
		if (StructName == TEXT("IntVector"))
		{
			return TEXT("int3");
		}
		if (StructName == TEXT("IntVector2"))
		{
			return TEXT("int2");
		}
		if (StructName == TEXT("IntVector4"))
		{
			return TEXT("int4");
		}
		// FVoxelSeed is an int32 wrapper used for noise/hash seed pins
		if (StructName == TEXT("VoxelSeed"))
		{
			return TEXT("int");
		}
		return TEXT("float"); // Fallback for unknown structs
	}
	default:
		return TEXT("float");
	}
}

FString FPGLVoxelPinTypeMapper::GetDefaultLiteral(const FVoxelPinType& VoxelType, const FString& DefaultValue)
{
	const FString HLSLType = ToHLSLType(VoxelType);

	if (HLSLType == TEXT("float"))
	{
		if (DefaultValue.IsEmpty()) return TEXT("0.0f");
		return DefaultValue + TEXT("f");
	}
	if (HLSLType == TEXT("int") || HLSLType == TEXT("uint"))
	{
		if (DefaultValue.IsEmpty()) return TEXT("0");
		return DefaultValue;
	}
	if (HLSLType == TEXT("bool"))
	{
		if (DefaultValue.IsEmpty()) return TEXT("false");
		return DefaultValue.ToBool() ? TEXT("true") : TEXT("false");
	}
	if (HLSLType == TEXT("float2"))
	{
		return TEXT("float2(0.0f, 0.0f)");
	}
	if (HLSLType == TEXT("float3"))
	{
		return TEXT("float3(0.0f, 0.0f, 0.0f)");
	}
	if (HLSLType == TEXT("float4"))
	{
		return TEXT("float4(0.0f, 0.0f, 0.0f, 0.0f)");
	}
	if (HLSLType == TEXT("int2"))
	{
		return TEXT("int2(0, 0)");
	}
	if (HLSLType == TEXT("int3"))
	{
		return TEXT("int3(0, 0, 0)");
	}
	if (HLSLType == TEXT("int4"))
	{
		return TEXT("int4(0, 0, 0, 0)");
	}

	return TEXT("0.0f");
}

bool FPGLVoxelPinTypeMapper::IsTranslatable(const FVoxelPinType& VoxelType)
{
	const FVoxelPinType Inner = VoxelType.IsBuffer() ? VoxelType.GetInnerType() : VoxelType;

	switch (Inner.GetInternalType())
	{
	case EVoxelPinInternalType::Bool:
	case EVoxelPinInternalType::Float:
	case EVoxelPinInternalType::Double:
	case EVoxelPinInternalType::Int32:
	case EVoxelPinInternalType::Int64:
	case EVoxelPinInternalType::UInt16:
	case EVoxelPinInternalType::Byte:
		return true;
	case EVoxelPinInternalType::Struct:
	{
		const UScriptStruct* Struct = Inner.GetStruct();
		if (!Struct) return false;
		const FName StructName = Struct->GetFName();
		return StructName == NAME_Vector
			|| StructName == TEXT("Vector")
			|| StructName == TEXT("Vector2D")
			|| StructName == TEXT("Vector4")
			|| StructName == TEXT("Quat")
			|| StructName == TEXT("LinearColor")
			|| StructName == TEXT("IntVector")
			|| StructName == TEXT("IntVector2")
			|| StructName == TEXT("IntVector4")
			|| StructName == TEXT("VoxelSeed");
	}
	default:
		return false;
	}
}

FString FPGLVoxelPinTypeMapper::GetPCGAccessorSuffix(const FVoxelPinType& VoxelType)
{
	const FString HLSLType = ToHLSLType(VoxelType);

	if (HLSLType == TEXT("float"))  return TEXT("Float");
	if (HLSLType == TEXT("int"))    return TEXT("Int");
	if (HLSLType == TEXT("uint"))   return TEXT("Uint");
	if (HLSLType == TEXT("bool"))   return TEXT("Bool");
	if (HLSLType == TEXT("float2")) return TEXT("Float2");
	if (HLSLType == TEXT("float3")) return TEXT("Float3");
	if (HLSLType == TEXT("float4")) return TEXT("Float4");
	if (HLSLType == TEXT("int2"))   return TEXT("Float2"); // PCG stores int vectors as float vectors
	if (HLSLType == TEXT("int3"))   return TEXT("Float3");
	if (HLSLType == TEXT("int4"))   return TEXT("Float4");

	return TEXT("Float");
}

FString FPGLVoxelPinTypeMapper::MakePCGReadExpression(
	const FVoxelPinType& VoxelType,
	const FString& PinName,
	const FString& AttributeName)
{
	const FString Suffix = GetPCGAccessorSuffix(VoxelType);
	return FString::Printf(TEXT("%s_Get%s(%s_DataIndex, ElementIndex, '%s')"),
		*PinName, *Suffix, *PinName, *AttributeName);
}

FString FPGLVoxelPinTypeMapper::MakePCGWriteStatement(
	const FVoxelPinType& VoxelType,
	const FString& PinName,
	const FString& VariableName,
	const FString& AttributeName)
{
	const FString Suffix = GetPCGAccessorSuffix(VoxelType);
	return FString::Printf(TEXT("%s_Set%s(%s_DataIndex, ElementIndex, '%s', %s);"),
		*PinName, *Suffix, *PinName, *AttributeName, *VariableName);
}
