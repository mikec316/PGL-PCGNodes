// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Tracks which HLSL helper functions are needed by the generated code
struct PGLPCGNODESEDITOR_API FPGLHLSLRequiredHelpers
{
	bool bNeedsHash = false;
	bool bNeedsRandRange = false;
	bool bNeedsLerp = false;
	bool bNeedsSmoothStep = false;
	bool bNeedsBilerp = false;
	bool bNeedsPerlin2D = false;
	bool bNeedsPerlin3D = false;
	bool bNeedsSimplex2D = false;
	bool bNeedsSimplex3D = false;
	bool bNeedsCellular2D = false;
	bool bNeedsCellular3D = false;
	bool bNeedsValue2D = false;
	bool bNeedsValue3D = false;
	bool bNeedsQuaternionFromEuler = false;
	bool bNeedsEulerFromQuaternion = false;
	bool bNeedsQuaternionFromAxis = false;
	bool bNeedsDistanceFieldColor = false;

	bool HasAnyNoise() const
	{
		return bNeedsPerlin2D || bNeedsPerlin3D
			|| bNeedsSimplex2D || bNeedsSimplex3D
			|| bNeedsCellular2D || bNeedsCellular3D
			|| bNeedsValue2D || bNeedsValue3D;
	}

	bool HasAnyQuaternion() const
	{
		return bNeedsQuaternionFromEuler || bNeedsEulerFromQuaternion || bNeedsQuaternionFromAxis;
	}
};

// Transforms ISPC-style code from FVoxelComputeNode::GenerateCode() into valid HLSL.
// Resolves {PinName} placeholders, remaps functions, and tracks required helpers.
class PGLPCGNODESEDITOR_API FPGLISPCToHLSLTransformer
{
public:
	// Transform a single ISPC code string into HLSL.
	// PinToVariable maps pin names to their resolved HLSL variable names.
	// OutRequiredHelpers is updated with any helper functions referenced.
	static FString Transform(
		const FString& ISPCCode,
		const TMap<FName, FString>& PinToVariable,
		FPGLHLSLRequiredHelpers& OutRequiredHelpers);

	// Returns true if the ISPC code contains a function call that requires
	// block-macro handling (noise, hash, quat, distance field, random).
	// These cannot be emitted as simple expression assignments and must be
	// handled specially by the traverser.
	static bool ContainsBlockMacroFunction(const FString& ISPCCode);

private:
	static FString ResolvePlaceholders(const FString& Code, const TMap<FName, FString>& PinToVariable);
	static FString RemapFunctions(const FString& Code, FPGLHLSLRequiredHelpers& OutRequiredHelpers);
	static FString CleanupDirectives(const FString& Code);
	static FString FixOutputPointers(const FString& Code, const TMap<FName, FString>& PinToVariable);
};
