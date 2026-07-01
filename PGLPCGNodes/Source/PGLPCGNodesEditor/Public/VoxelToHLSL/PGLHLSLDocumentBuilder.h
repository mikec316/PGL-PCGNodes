// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelToHLSL/PGLISPCToHLSLTransformer.h"

struct FPGLGraphInput;
struct FPGLGraphOutput;

// Assembles sections of HLSL code into a final document suitable for PCG Compute Source
class PGLPCGNODESEDITOR_API FPGLHLSLDocumentBuilder
{
public:
	void SetGraphName(const FString& InGraphName) { GraphName = InGraphName; }
	void SetInputPinName(const FString& InPinName) { InputPinName = InPinName; }
	void SetOutputPinName(const FString& InPinName) { OutputPinName = InPinName; }

	void SetInputs(const TArray<FPGLGraphInput>& InInputs) { Inputs = InInputs; }
	void SetOutputs(const TArray<FPGLGraphOutput>& InOutputs) { Outputs = InOutputs; }
	void SetStatements(const TArray<FString>& InStatements) { Statements = InStatements; }
	void SetRequiredHelpers(const FPGLHLSLRequiredHelpers& InHelpers) { RequiredHelpers = InHelpers; }
	void SetWarnings(const TArray<FString>& InWarnings) { Warnings = InWarnings; }

	// Assemble and return the complete HLSL source document
	FString Build() const;

private:
	FString BuildHeader() const;
	FString BuildHelperFunctions() const;
	FString BuildOutputWrites() const;

	FString GraphName;
	FString InputPinName = TEXT("In");
	FString OutputPinName = TEXT("Out");
	TArray<FPGLGraphInput> Inputs;
	TArray<FPGLGraphOutput> Outputs;
	TArray<FString> Statements;
	FPGLHLSLRequiredHelpers RequiredHelpers;
	TArray<FString> Warnings;
};
