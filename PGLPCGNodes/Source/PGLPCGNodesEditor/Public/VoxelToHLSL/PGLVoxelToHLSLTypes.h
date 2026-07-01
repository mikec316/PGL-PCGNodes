// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PGLVoxelToHLSLTypes.generated.h"

UENUM(BlueprintType)
enum class EPGLHLSLTranslationStatus : uint8
{
	Success,
	PartialSuccess,
	Failure
};

USTRUCT(BlueprintType)
struct PGLPCGNODESEDITOR_API FPGLVoxelToHLSLOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	bool bIncludeComments = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	bool bIncludeNoiseLibrary = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	bool bIncludeQuaternionHelpers = true;

	// Name of the input pin in the PCG HLSL node (e.g. "In")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	FString InputPinName = TEXT("In");

	// Name of the output pin in the PCG HLSL node (e.g. "Out")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	FString OutputPinName = TEXT("Out");
};

USTRUCT(BlueprintType)
struct PGLPCGNODESEDITOR_API FPGLHLSLTranslationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	EPGLHLSLTranslationStatus Status = EPGLHLSLTranslationStatus::Failure;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FString GeneratedHLSL;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TArray<FString> Warnings;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TArray<FString> UnsupportedNodeTypes;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 TotalNodes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 TranslatedNodes = 0;

	// Output attributes that must be added to the Custom HLSL node's
	// Output Pin > GPU Properties > "Attributes to Create"
	// Format: "Name (Type)" e.g. "Height (Float)"
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TArray<FString> RequiredOutputAttributes;
};
