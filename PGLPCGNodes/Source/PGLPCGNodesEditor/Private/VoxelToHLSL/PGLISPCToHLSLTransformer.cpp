// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "VoxelToHLSL/PGLISPCToHLSLTransformer.h"

FString FPGLISPCToHLSLTransformer::Transform(
	const FString& ISPCCode,
	const TMap<FName, FString>& PinToVariable,
	FPGLHLSLRequiredHelpers& OutRequiredHelpers)
{
	FString Code = ISPCCode;

	// Step 1: Strip ISPC directives
	Code = CleanupDirectives(Code);

	// Step 2: Convert &{PinName} output pointer syntax to direct assignment
	Code = FixOutputPointers(Code, PinToVariable);

	// Step 3: Resolve {PinName} placeholders to actual HLSL variable names
	Code = ResolvePlaceholders(Code, PinToVariable);

	// Step 4: Remap ISPC-specific functions to HLSL equivalents
	Code = RemapFunctions(Code, OutRequiredHelpers);

	return Code;
}

FString FPGLISPCToHLSLTransformer::ResolvePlaceholders(const FString& Code, const TMap<FName, FString>& PinToVariable)
{
	FString Result = Code;

	for (const auto& Pair : PinToVariable)
	{
		const FString Placeholder = FString::Printf(TEXT("{%s}"), *Pair.Key.ToString());
		Result = Result.Replace(*Placeholder, *Pair.Value);
	}

	return Result;
}

FString FPGLISPCToHLSLTransformer::RemapFunctions(const FString& Code, FPGLHLSLRequiredHelpers& OutRequiredHelpers)
{
	FString Result = Code;

	// Hash functions — MurmurHash32 is no longer a simple function call.
	// For inline ISPC hash usage (rare outside noise), we leave it for the traverser
	// to handle via block macros. The noise functions are handled specially in the traverser.
	// For simple hash usage in non-noise compute nodes:
	if (Result.Contains(TEXT("MurmurHash32")))
	{
		OutRequiredHelpers.bNeedsHash = true;
		// Note: MurmurHash32 calls in non-noise code will be flagged for special handling
		// in the traverser. We don't do a simple name replace here because the macro
		// calling convention differs (block macro with DEST parameter).
	}

	// Random — same block-macro approach
	if (Result.Contains(TEXT("RandRange")))
	{
		OutRequiredHelpers.bNeedsRandRange = true;
		OutRequiredHelpers.bNeedsHash = true;
	}

	// Interpolation — these remain as expression macros (same calling convention)
	if (Result.Contains(TEXT("lerp_NaN")))
	{
		OutRequiredHelpers.bNeedsLerp = true;
		Result = Result.Replace(TEXT("lerp_NaN"), TEXT("pgl_lerp"));
	}
	if (Result.Contains(TEXT("SmoothStep_NaN")))
	{
		OutRequiredHelpers.bNeedsSmoothStep = true;
		Result = Result.Replace(TEXT("SmoothStep_NaN"), TEXT("pgl_smoothstep"));
	}
	if (Result.Contains(TEXT("BilinearInterpolation")))
	{
		OutRequiredHelpers.bNeedsBilerp = true;
		Result = Result.Replace(TEXT("BilinearInterpolation"), TEXT("pgl_bilerp"));
	}

	// Noise functions — flag what's needed but DON'T replace names.
	// These are handled specially as block macros in the traverser.
	if (Result.Contains(TEXT("GetPerlin2D")))
	{
		OutRequiredHelpers.bNeedsPerlin2D = true;
		OutRequiredHelpers.bNeedsHash = true;
	}
	if (Result.Contains(TEXT("GetSimplex2D")))
	{
		OutRequiredHelpers.bNeedsSimplex2D = true;
		OutRequiredHelpers.bNeedsHash = true;
	}
	if (Result.Contains(TEXT("GetCellularNoise2D")))
	{
		OutRequiredHelpers.bNeedsCellular2D = true;
		OutRequiredHelpers.bNeedsHash = true;
	}
	if (Result.Contains(TEXT("GetValue2D")))
	{
		OutRequiredHelpers.bNeedsValue2D = true;
		OutRequiredHelpers.bNeedsHash = true;
	}
	if (Result.Contains(TEXT("GetPerlin3D")))
	{
		OutRequiredHelpers.bNeedsPerlin3D = true;
		OutRequiredHelpers.bNeedsHash = true;
	}
	if (Result.Contains(TEXT("GetSimplex3D")))
	{
		OutRequiredHelpers.bNeedsSimplex3D = true;
		OutRequiredHelpers.bNeedsHash = true;
	}
	if (Result.Contains(TEXT("GetCellularNoise3D")))
	{
		OutRequiredHelpers.bNeedsCellular3D = true;
		OutRequiredHelpers.bNeedsHash = true;
	}
	if (Result.Contains(TEXT("GetValue3D")))
	{
		OutRequiredHelpers.bNeedsValue3D = true;
		OutRequiredHelpers.bNeedsHash = true;
	}
	if (Result.Contains(TEXT("GetTrueDistanceCellularNoise2D")))
	{
		OutRequiredHelpers.bNeedsCellular2D = true;
		OutRequiredHelpers.bNeedsHash = true;
	}
	if (Result.Contains(TEXT("GetTrueDistanceCellularNoise3D")))
	{
		OutRequiredHelpers.bNeedsCellular3D = true;
		OutRequiredHelpers.bNeedsHash = true;
	}

	// Quaternion functions — these are block macros, flagged for traverser handling
	if (Result.Contains(TEXT("MakeQuaternionFromEuler")))
	{
		OutRequiredHelpers.bNeedsQuaternionFromEuler = true;
	}
	if (Result.Contains(TEXT("MakeEulerFromQuaternion")))
	{
		OutRequiredHelpers.bNeedsEulerFromQuaternion = true;
	}
	if (Result.Contains(TEXT("MakeQuaternionFromX")))
	{
		OutRequiredHelpers.bNeedsQuaternionFromAxis = true;
	}
	if (Result.Contains(TEXT("MakeQuaternionFromY")))
	{
		OutRequiredHelpers.bNeedsQuaternionFromAxis = true;
	}
	if (Result.Contains(TEXT("MakeQuaternionFromZ")))
	{
		OutRequiredHelpers.bNeedsQuaternionFromAxis = true;
	}

	// Distance field
	if (Result.Contains(TEXT("GetDistanceFieldColor")))
	{
		OutRequiredHelpers.bNeedsDistanceFieldColor = true;
	}

	// ISPC constants that need translation
	Result = Result.Replace(TEXT("BIG_NUMBER"), TEXT("3.4e+38f"));
	Result = Result.Replace(TEXT("SMALL_NUMBER"), TEXT("1.e-8f"));

	return Result;
}

bool FPGLISPCToHLSLTransformer::ContainsBlockMacroFunction(const FString& ISPCCode)
{
	// Noise functions
	if (ISPCCode.Contains(TEXT("GetPerlin2D")) || ISPCCode.Contains(TEXT("GetPerlin3D"))) return true;
	if (ISPCCode.Contains(TEXT("GetSimplex2D")) || ISPCCode.Contains(TEXT("GetSimplex3D"))) return true;
	if (ISPCCode.Contains(TEXT("GetCellularNoise2D")) || ISPCCode.Contains(TEXT("GetCellularNoise3D"))) return true;
	if (ISPCCode.Contains(TEXT("GetValue2D")) || ISPCCode.Contains(TEXT("GetValue3D"))) return true;
	if (ISPCCode.Contains(TEXT("GetTrueDistanceCellularNoise2D")) || ISPCCode.Contains(TEXT("GetTrueDistanceCellularNoise3D"))) return true;
	// Hash / Random
	if (ISPCCode.Contains(TEXT("MurmurHash32"))) return true;
	if (ISPCCode.Contains(TEXT("RandRange"))) return true;
	// Quaternion
	if (ISPCCode.Contains(TEXT("MakeQuaternionFromEuler")) || ISPCCode.Contains(TEXT("MakeEulerFromQuaternion"))) return true;
	if (ISPCCode.Contains(TEXT("MakeQuaternionFromX")) || ISPCCode.Contains(TEXT("MakeQuaternionFromY")) || ISPCCode.Contains(TEXT("MakeQuaternionFromZ"))) return true;
	// Distance field
	if (ISPCCode.Contains(TEXT("GetDistanceFieldColor"))) return true;

	return false;
}

FString FPGLISPCToHLSLTransformer::CleanupDirectives(const FString& Code)
{
	FString Result = Code;

	// Remove ISPC performance warning suppression
	Result = Result.Replace(TEXT("IGNORE_PERF_WARNING\n"), TEXT(""));
	Result = Result.Replace(TEXT("IGNORE_PERF_WARNING"), TEXT(""));

	return Result;
}

FString FPGLISPCToHLSLTransformer::FixOutputPointers(const FString& Code, const TMap<FName, FString>& PinToVariable)
{
	FString Result = Code;

	// ISPC uses &{PinName} for output parameters passed by pointer.
	// In HLSL, we just use the variable directly (no pointers on GPU).
	// Replace &{PinName} with {PinName} - the function will write to a local variable.
	for (const auto& Pair : PinToVariable)
	{
		const FString PointerRef = FString::Printf(TEXT("&{%s}"), *Pair.Key.ToString());
		const FString DirectRef = FString::Printf(TEXT("{%s}"), *Pair.Key.ToString());
		Result = Result.Replace(*PointerRef, *DirectRef);
	}

	return Result;
}
