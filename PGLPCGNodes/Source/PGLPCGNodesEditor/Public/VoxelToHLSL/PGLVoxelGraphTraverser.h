// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelToHLSL/PGLVoxelToHLSLTypes.h"
#include "VoxelToHLSL/PGLISPCToHLSLTransformer.h"

class UVoxelGraph;
class UVoxelTerminalGraph;
struct FVoxelNode_UFunction;
struct FVoxelOutputNode;

namespace Voxel::Graph
{
	class FPin;
	class FNode;
	class FGraph;
}

// Represents a graph input that becomes a PCG attribute read
struct FPGLGraphInput
{
	FName Name;
	FString HLSLType;
	FString VariableName;
	FString PCGReadExpression;
};

// Represents a graph output that becomes a PCG attribute write
struct FPGLGraphOutput
{
	FName Name;
	FString HLSLType;
	FString SourceVariableName;
	FString PCGWriteStatement;
};

// Traverses a Voxel Graph compilation graph, extracts GenerateCode() from each
// FVoxelComputeNode, and emits ordered HLSL statements.
class PGLPCGNODESEDITOR_API FPGLVoxelGraphTraverser
{
public:
	// Run the full traversal on a voxel graph. Returns false on critical failure.
	bool Traverse(UVoxelGraph* VoxelGraph, const FPGLVoxelToHLSLOptions& Options);

	// Results
	const TArray<FString>& GetHLSLStatements() const { return HLSLStatements; }
	const TArray<FPGLGraphInput>& GetGraphInputs() const { return GraphInputs; }
	const TArray<FPGLGraphOutput>& GetGraphOutputs() const { return GraphOutputs; }
	const TArray<FString>& GetWarnings() const { return Warnings; }
	const TArray<FString>& GetUnsupportedNodeTypes() const { return UnsupportedNodeTypes; }
	const FPGLHLSLRequiredHelpers& GetRequiredHelpers() const { return RequiredHelpers; }
	int32 GetTotalNodes() const { return TotalNodes; }
	int32 GetTranslatedNodes() const { return TranslatedNodes; }

private:
	// Maps compilation graph pin -> HLSL variable name
	TMap<const Voxel::Graph::FPin*, FString> PinToVariableName;

	TArray<FString> HLSLStatements;
	TArray<FPGLGraphInput> GraphInputs;
	TArray<FPGLGraphOutput> GraphOutputs;
	TArray<FString> Warnings;
	TArray<FString> UnsupportedNodeTypes;
	FPGLHLSLRequiredHelpers RequiredHelpers;
	FPGLVoxelToHLSLOptions CurrentOptions;
	int32 TotalNodes = 0;
	int32 TranslatedNodes = 0;

	// Topological sort of compilation graph nodes
	TArray<Voxel::Graph::FNode*> TopologicalSort(const TArray<Voxel::Graph::FNode*>& Nodes);

	// Generate a sanitized HLSL variable name from a node+pin
	FString MakeVariableName(const Voxel::Graph::FNode& Node, const FName& PinName);

	// Sanitize a string to be a valid HLSL identifier
	static FString SanitizeIdentifier(const FString& Input);

	// Convert a compilation-graph pin's default value to an HLSL literal
	FString PinDefaultToHLSL(const Voxel::Graph::FPin& Pin);

	// Process a single node and emit HLSL statements
	void ProcessNode(Voxel::Graph::FNode& Node, const UVoxelTerminalGraph& TerminalGraph);

	// Handle UFunction nodes (position getters, math wrappers). Returns true if handled.
	bool ProcessUFunctionNode(Voxel::Graph::FNode& Node, const FVoxelNode_UFunction& UFuncNode);

	// Handle VoxelOutputNode types (OutputHeight, OutputSurface, etc.)
	void ProcessOutputNode(Voxel::Graph::FNode& Node, const FVoxelOutputNode& OutputNode);

	// Handle FVoxelComputeNode whose ISPC code uses block-macro functions
	// (noise, hash, quat, etc.). Returns true if handled.
	bool ProcessBlockMacroComputeNode(Voxel::Graph::FNode& Node, const FString& ISPCCode,
		const TMap<FName, FString>& LocalPinMap);

	// Resolve a single input pin to its source variable or default
	FString ResolveInputPin(const Voxel::Graph::FPin& InputPin);
};
