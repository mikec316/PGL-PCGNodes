// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "VoxelToHLSL/PGLVoxelGraphTraverser.h"
#include "VoxelToHLSL/PGLVoxelPinTypeMapper.h"
#include "VoxelToHLSL/PGLISPCToHLSLTransformer.h"

#include "VoxelGraph.h"
#include "VoxelTerminalGraph.h"
#include "VoxelGraphCompiler.h"
#include "VoxelGraphCompileScope.h"
#include "VoxelGraphTracker.h"
#include "VoxelCompilationGraph.h"
#include "VoxelComputeNode.h"
#include "VoxelNode.h"
#include "VoxelPinType.h"
#include "VoxelPinValue.h"
#include "Nodes/VoxelFunctionInputNodes.h"
#include "Nodes/VoxelNode_FunctionOutput.h"
#include "Nodes/VoxelNode_Parameter.h"
#include "Nodes/VoxelNode_UFunction.h"
#include "Nodes/VoxelOutputNode.h"

using FCompNode = Voxel::Graph::FNode;
using FCompPin = Voxel::Graph::FPin;

bool FPGLVoxelGraphTraverser::Traverse(UVoxelGraph* VoxelGraph, const FPGLVoxelToHLSLOptions& Options)
{
	if (!VoxelGraph)
	{
		Warnings.Add(TEXT("Null VoxelGraph provided"));
		return false;
	}

	CurrentOptions = Options;

	// Reset state
	PinToVariableName.Empty();
	HLSLStatements.Empty();
	GraphInputs.Empty();
	GraphOutputs.Empty();
	Warnings.Empty();
	UnsupportedNodeTypes.Empty();
	RequiredHelpers = {};
	TotalNodes = 0;
	TranslatedNodes = 0;

	if (!VoxelGraph->HasMainTerminalGraph())
	{
		Warnings.Add(TEXT("VoxelGraph has no main terminal graph"));
		return false;
	}

	const UVoxelTerminalGraph& TerminalGraph = VoxelGraph->GetMainTerminalGraph();

	// Create compile scope (required by Voxel Plugin before using the compiler)
	FVoxelGraphCompileScope CompileScope(TerminalGraph, /*bEnableLogging=*/ false);

	// Create compiler and load the serialized graph
	FVoxelGraphCompiler Compiler(TerminalGraph);

	if (!Compiler.LoadSerializedGraph(FOnVoxelGraphChanged::Null(), FOnVoxelGraphChanged::Null()))
	{
		Warnings.Add(TEXT("Failed to load serialized graph"));
		return false;
	}

	// Run compiler passes to expand templates into concrete compute nodes
	Compiler.RemoveSplitPins();
	Compiler.FixPositionPins();
	Compiler.RemoveLocalVariables();
	Compiler.CollapseInputs();
	Compiler.ReplaceTemplates();
	Compiler.RemovePassthroughs();
	Compiler.RemoveNodesNotLinkedToQueryableNodes();

	// Gather all nodes
	TVoxelArray<FCompNode*> AllNodes = Compiler.GetNodesArray();
	TotalNodes = AllNodes.Num();

	if (AllNodes.Num() == 0)
	{
		Warnings.Add(TEXT("Graph has no nodes after compilation passes"));
		return false;
	}

	// Topological sort
	TArray<FCompNode*> SortedNodes = TopologicalSort(AllNodes);

	// First pass: assign variable names to all output pins
	for (FCompNode* Node : SortedNodes)
	{
		for (const FCompPin& Pin : Node->GetOutputPins())
		{
			const FString VarName = MakeVariableName(*Node, Pin.Name);
			PinToVariableName.Add(&Pin, VarName);
		}
	}

	// Second pass: process each node in order
	for (FCompNode* Node : SortedNodes)
	{
		ProcessNode(*Node, TerminalGraph);
	}

	return true;
}

TArray<FCompNode*> FPGLVoxelGraphTraverser::TopologicalSort(const TArray<FCompNode*>& Nodes)
{
	// Kahn's algorithm
	TMap<FCompNode*, int32> InDegree;
	TMap<FCompNode*, TArray<FCompNode*>> Adjacency;

	// Initialize
	for (FCompNode* Node : Nodes)
	{
		InDegree.FindOrAdd(Node, 0);
		Adjacency.FindOrAdd(Node);
	}

	// Build dependency edges: for each input pin, find source node
	TSet<FCompNode*> NodeSet(Nodes);
	for (FCompNode* Node : Nodes)
	{
		for (const FCompPin& InputPin : Node->GetInputPins())
		{
			for (const FCompPin& LinkedPin : InputPin.GetLinkedTo())
			{
				FCompNode* SourceNode = &LinkedPin.Node;
				if (NodeSet.Contains(SourceNode) && SourceNode != Node)
				{
					Adjacency.FindOrAdd(SourceNode).AddUnique(Node);
					InDegree.FindOrAdd(Node, 0)++;
				}
			}
		}
	}

	// Find initial zero-degree nodes
	TArray<FCompNode*> Queue;
	for (auto& Pair : InDegree)
	{
		if (Pair.Value == 0)
		{
			Queue.Add(Pair.Key);
		}
	}

	TArray<FCompNode*> Sorted;
	Sorted.Reserve(Nodes.Num());

	while (Queue.Num() > 0)
	{
		FCompNode* Current = Queue.Pop(EAllowShrinking::No);
		Sorted.Add(Current);

		if (const TArray<FCompNode*>* Neighbors = Adjacency.Find(Current))
		{
			for (FCompNode* Neighbor : *Neighbors)
			{
				int32& Degree = InDegree.FindChecked(Neighbor);
				Degree--;
				if (Degree == 0)
				{
					Queue.Add(Neighbor);
				}
			}
		}
	}

	if (Sorted.Num() != Nodes.Num())
	{
		Warnings.Add(FString::Printf(TEXT("Topological sort incomplete: %d of %d nodes sorted (possible cycle)"), Sorted.Num(), Nodes.Num()));
		// Add remaining nodes at the end
		for (FCompNode* Node : Nodes)
		{
			if (!Sorted.Contains(Node))
			{
				Sorted.Add(Node);
			}
		}
	}

	return Sorted;
}

FString FPGLVoxelGraphTraverser::MakeVariableName(const FCompNode& Node, const FName& PinName)
{
	const FString NodeId = SanitizeIdentifier(Node.NodeRef.NodeId.ToString());
	const FString PinStr = SanitizeIdentifier(PinName.ToString());
	return FString::Printf(TEXT("v_%s_%s"), *NodeId, *PinStr);
}

FString FPGLVoxelGraphTraverser::SanitizeIdentifier(const FString& Input)
{
	FString Result;
	Result.Reserve(Input.Len());

	for (int32 i = 0; i < Input.Len(); i++)
	{
		const TCHAR Ch = Input[i];
		if (FChar::IsAlnum(Ch) || Ch == '_')
		{
			Result.AppendChar(Ch);
		}
		else
		{
			Result.AppendChar('_');
		}
	}

	// Ensure doesn't start with a digit
	if (Result.Len() > 0 && FChar::IsDigit(Result[0]))
	{
		Result = TEXT("_") + Result;
	}

	return Result;
}

FString FPGLVoxelGraphTraverser::PinDefaultToHLSL(const FCompPin& Pin)
{
	const FVoxelPinValue& Default = Pin.GetDefaultValue();
	const FString HLSLType = FPGLVoxelPinTypeMapper::ToHLSLType(Pin.Type);

	// Try to use the serialized default value from the graph
	if (Default.IsValid())
	{
		const FString Exported = Default.ExportToString();
		if (!Exported.IsEmpty() && Exported != TEXT("()"))
		{
			// For simple scalar types, use the exported value directly
			switch (Pin.Type.GetInternalType())
			{
			case EVoxelPinInternalType::Float:
			{
				// Exported might be "10000.000000" → "10000.0f"
				float Val = FCString::Atof(*Exported);
				return FString::Printf(TEXT("%.6ff"), Val);
			}
			case EVoxelPinInternalType::Double:
			{
				float Val = FCString::Atof(*Exported);
				return FString::Printf(TEXT("%.6ff"), Val);
			}
			case EVoxelPinInternalType::Int32:
				return Exported;
			case EVoxelPinInternalType::Bool:
				return Exported.ToBool() ? TEXT("true") : TEXT("false");
			default:
				break;
			}
		}
	}

	// Fall back to type-based default
	return FPGLVoxelPinTypeMapper::GetDefaultLiteral(Pin.Type);
}

void FPGLVoxelGraphTraverser::ProcessNode(FCompNode& Node, const UVoxelTerminalGraph& TerminalGraph)
{
	if (Node.Type == Voxel::Graph::ENodeType::Passthrough)
	{
		// Passthrough: forward input variable name to output
		if (Node.GetInputPins().Num() > 0 && Node.GetOutputPins().Num() > 0)
		{
			const FCompPin& InPin = Node.GetInputPin(0);
			const FCompPin& OutPin = Node.GetOutputPin(0);

			FString SourceVar;
			for (const FCompPin& LinkedPin : InPin.GetLinkedTo())
			{
				if (const FString* Found = PinToVariableName.Find(&LinkedPin))
				{
					SourceVar = *Found;
					break;
				}
			}

			if (!SourceVar.IsEmpty())
			{
				PinToVariableName.Add(&OutPin, SourceVar);
			}
		}
		return;
	}

	// Must be a Struct node
	check(Node.Type == Voxel::Graph::ENodeType::Struct);
	const FVoxelNode& VoxelNode = Node.GetVoxelNode();

	// Check if this is a function input node
	if (const FVoxelNode_FunctionInput* InputNode = CastStruct<FVoxelNode_FunctionInput>(VoxelNode))
	{
		// Find the function input definition from the terminal graph
		const FVoxelGraphFunctionInput* FuncInput = TerminalGraph.FindInput(InputNode->Guid);
		if (FuncInput && Node.GetOutputPins().Num() > 0)
		{
			const FCompPin& OutPin = Node.GetOutputPin(0);
			const FString VarName = PinToVariableName.FindRef(&OutPin);
			const FString HLSLType = FPGLVoxelPinTypeMapper::ToHLSLType(FuncInput->Type);
			const FString AttrName = FuncInput->Name.ToString();

			FPGLGraphInput GraphInput;
			GraphInput.Name = FuncInput->Name;
			GraphInput.HLSLType = HLSLType;
			GraphInput.VariableName = VarName;
			GraphInput.PCGReadExpression = FPGLVoxelPinTypeMapper::MakePCGReadExpression(
				FuncInput->Type, CurrentOptions.InputPinName, AttrName);
			GraphInputs.Add(GraphInput);

			if (CurrentOptions.bIncludeComments)
			{
				HLSLStatements.Add(FString::Printf(TEXT("// Graph Input: %s"), *AttrName));
			}
			HLSLStatements.Add(FString::Printf(TEXT("%s %s = %s;"),
				*HLSLType, *VarName, *GraphInput.PCGReadExpression));

			TranslatedNodes++;
		}
		return;
	}

	// Check for FunctionInput_WithDefaults (combined input/default/preview)
	if (const FVoxelNode_FunctionInput_WithDefaults* InputDefNode = CastStruct<FVoxelNode_FunctionInput_WithDefaults>(VoxelNode))
	{
		const FVoxelGraphFunctionInput* FuncInput = TerminalGraph.FindInput(InputDefNode->Guid);
		if (FuncInput)
		{
			const FCompPin* OutPin = Node.FindOutput(TEXT("Value"));
			if (OutPin)
			{
				const FString VarName = PinToVariableName.FindRef(OutPin);
				const FString HLSLType = FPGLVoxelPinTypeMapper::ToHLSLType(FuncInput->Type);
				const FString AttrName = FuncInput->Name.ToString();

				FPGLGraphInput GraphInput;
				GraphInput.Name = FuncInput->Name;
				GraphInput.HLSLType = HLSLType;
				GraphInput.VariableName = VarName;
				GraphInput.PCGReadExpression = FPGLVoxelPinTypeMapper::MakePCGReadExpression(
					FuncInput->Type, CurrentOptions.InputPinName, AttrName);
				GraphInputs.Add(GraphInput);

				if (CurrentOptions.bIncludeComments)
				{
					HLSLStatements.Add(FString::Printf(TEXT("// Graph Input: %s"), *AttrName));
				}
				HLSLStatements.Add(FString::Printf(TEXT("%s %s = %s;"),
					*HLSLType, *VarName, *GraphInput.PCGReadExpression));

				TranslatedNodes++;
			}
		}
		return;
	}

	// Check if this is a function output node
	if (const FVoxelNode_FunctionOutput* OutputNode = CastStruct<FVoxelNode_FunctionOutput>(VoxelNode))
	{
		const FVoxelGraphFunctionOutput* FuncOutput = TerminalGraph.FindOutput(OutputNode->Guid);
		if (FuncOutput && Node.GetInputPins().Num() > 0)
		{
			const FCompPin& InPin = Node.GetInputPin(0);
			FString SourceVar;

			// Find the connected source variable
			for (const FCompPin& LinkedPin : InPin.GetLinkedTo())
			{
				if (const FString* Found = PinToVariableName.Find(&LinkedPin))
				{
					SourceVar = *Found;
					break;
				}
			}

			if (SourceVar.IsEmpty())
			{
				// Use default value if no connection
				SourceVar = FPGLVoxelPinTypeMapper::GetDefaultLiteral(FuncOutput->Type);
			}

			const FString AttrName = FuncOutput->Name.ToString();

			FPGLGraphOutput GraphOutput;
			GraphOutput.Name = FuncOutput->Name;
			GraphOutput.HLSLType = FPGLVoxelPinTypeMapper::ToHLSLType(FuncOutput->Type);
			GraphOutput.SourceVariableName = SourceVar;
			GraphOutput.PCGWriteStatement = FPGLVoxelPinTypeMapper::MakePCGWriteStatement(
				FuncOutput->Type, CurrentOptions.OutputPinName, SourceVar, AttrName);
			GraphOutputs.Add(GraphOutput);

			TranslatedNodes++;
		}
		return;
	}

	// Check if this is a Parameter node (graph-level parameter → PCG attribute read)
	if (const FVoxelNode_Parameter* ParamNode = CastStruct<FVoxelNode_Parameter>(VoxelNode))
	{
		const FCompPin* OutPin = Node.FindOutput(TEXT("Value"));
		if (OutPin)
		{
			const FString VarName = PinToVariableName.FindRef(OutPin);
			const FString HLSLType = FPGLVoxelPinTypeMapper::ToHLSLType(OutPin->Type);
			const FString AttrName = ParamNode->ParameterName.ToString();

			FPGLGraphInput GraphInput;
			GraphInput.Name = ParamNode->ParameterName;
			GraphInput.HLSLType = HLSLType;
			GraphInput.VariableName = VarName;
			GraphInput.PCGReadExpression = FPGLVoxelPinTypeMapper::MakePCGReadExpression(
				OutPin->Type, CurrentOptions.InputPinName, AttrName);
			GraphInputs.Add(GraphInput);

			if (CurrentOptions.bIncludeComments)
			{
				HLSLStatements.Add(FString::Printf(TEXT("// Parameter: %s"), *AttrName));
			}
			HLSLStatements.Add(FString::Printf(TEXT("%s %s = %s;"),
				*HLSLType, *VarName, *GraphInput.PCGReadExpression));

			TranslatedNodes++;
		}
		return;
	}

	// Check if this is a UFunction node (position getters, math wrappers, etc.)
	if (const FVoxelNode_UFunction* UFuncNode = CastStruct<FVoxelNode_UFunction>(VoxelNode))
	{
		if (ProcessUFunctionNode(Node, *UFuncNode))
		{
			TranslatedNodes++;
			return;
		}
		// Fall through to unsupported if we couldn't handle it
	}

	// Check if this is a VoxelOutputNode (OutputHeight, OutputSurface, etc.)
	if (const FVoxelOutputNode* OutputNode = CastStruct<FVoxelOutputNode>(VoxelNode))
	{
		ProcessOutputNode(Node, *OutputNode);
		TranslatedNodes++;
		return;
	}

	// Try to handle as a FVoxelComputeNode (the main translatable type)
	const FVoxelComputeNode* ComputeNode = CastStruct<FVoxelComputeNode>(VoxelNode);
	if (ComputeNode)
	{
		// Call GenerateCode to get the ISPC expression
		FVoxelComputeNode::FCode Code;
		const FString ISPCCode = ComputeNode->GenerateCode(Code);

		if (ISPCCode.IsEmpty())
		{
			Warnings.Add(FString::Printf(TEXT("Empty GenerateCode() from node: %s"),
				*VoxelNode.GetStruct()->GetName()));
			return;
		}

		// Build pin-to-variable mapping for this node
		TMap<FName, FString> LocalPinMap;

		// Map input pins to source variables
		for (const FCompPin& InputPin : Node.GetInputPins())
		{
			LocalPinMap.Add(InputPin.Name, ResolveInputPin(InputPin));
		}

		// Map output pins to their variable names
		for (const FCompPin& OutputPin : Node.GetOutputPins())
		{
			if (const FString* VarName = PinToVariableName.Find(&OutputPin))
			{
				LocalPinMap.Add(OutputPin.Name, *VarName);
			}
		}

		// Check if this ISPC code contains block-macro functions (noise, hash, quat, etc.)
		// These need special emission because they use PGL_PERLIN_2D(DEST, SEED, POS) syntax
		if (FPGLISPCToHLSLTransformer::ContainsBlockMacroFunction(ISPCCode))
		{
			if (ProcessBlockMacroComputeNode(Node, ISPCCode, LocalPinMap))
			{
				TranslatedNodes++;
				return;
			}
			// Fall through to normal processing if block macro handling fails
		}

		// Transform ISPC to HLSL (for non-block-macro code)
		const FString HLSLCode = FPGLISPCToHLSLTransformer::Transform(ISPCCode, LocalPinMap, RequiredHelpers);

		// Split into individual statements (separated by ;)
		TArray<FString> Statements;
		HLSLCode.ParseIntoArray(Statements, TEXT(";"), true);

		if (CurrentOptions.bIncludeComments)
		{
			HLSLStatements.Add(FString::Printf(TEXT("// Node: %s"),
				*VoxelNode.GetStruct()->GetName()));
		}

		// For simple single-assignment expressions, declare the output variable
		if (Statements.Num() == 1 && Node.GetOutputPins().Num() == 1)
		{
			const FCompPin& OutPin = Node.GetOutputPin(0);
			const FString* VarName = PinToVariableName.Find(&OutPin);
			const FString HLSLType = FPGLVoxelPinTypeMapper::ToHLSLType(OutPin.Type);

			FString Statement = Statements[0].TrimStartAndEnd();
			// The statement should be: varName = expression
			// We need to declare it: type varName = expression;
			HLSLStatements.Add(FString::Printf(TEXT("%s %s;"), *HLSLType, *Statement));
		}
		else
		{
			// Multiple statements or multiple outputs
			// Pre-declare output variables
			for (const FCompPin& OutputPin : Node.GetOutputPins())
			{
				const FString* VarName = PinToVariableName.Find(&OutputPin);
				const FString HLSLType = FPGLVoxelPinTypeMapper::ToHLSLType(OutputPin.Type);
				if (VarName)
				{
					HLSLStatements.Add(FString::Printf(TEXT("%s %s = %s;"),
						*HLSLType, **VarName,
						*FPGLVoxelPinTypeMapper::GetDefaultLiteral(OutputPin.Type)));
				}
			}

			// Emit each statement
			for (const FString& Statement : Statements)
			{
				FString Trimmed = Statement.TrimStartAndEnd();
				if (!Trimmed.IsEmpty())
				{
					HLSLStatements.Add(Trimmed + TEXT(";"));
				}
			}
		}

		TranslatedNodes++;
		return;
	}

	// Unsupported node type
	const FString NodeTypeName = VoxelNode.GetStruct()->GetName();
	UnsupportedNodeTypes.AddUnique(NodeTypeName);
	Warnings.Add(FString::Printf(TEXT("Unsupported node type: %s"), *NodeTypeName));

	if (CurrentOptions.bIncludeComments)
	{
		HLSLStatements.Add(FString::Printf(TEXT("// UNSUPPORTED: %s (outputs zeroed)"), *NodeTypeName));
	}

	// Declare output pins with default values so downstream nodes can still compile
	for (const FCompPin& OutputPin : Node.GetOutputPins())
	{
		if (const FString* VarName = PinToVariableName.Find(&OutputPin))
		{
			const FString HLSLType = FPGLVoxelPinTypeMapper::ToHLSLType(OutputPin.Type);
			HLSLStatements.Add(FString::Printf(TEXT("%s %s = %s;"),
				*HLSLType, **VarName,
				*FPGLVoxelPinTypeMapper::GetDefaultLiteral(OutputPin.Type)));
		}
	}
}

bool FPGLVoxelGraphTraverser::ProcessUFunctionNode(FCompNode& Node, const FVoxelNode_UFunction& UFuncNode)
{
	// Prefer the actual UFunction name (C++ identifier); fall back to CachedName (display name)
	FString FuncNameStr;
	if (const UFunction* Func = UFuncNode.GetFunction())
	{
		FuncNameStr = Func->GetName();
	}
	else
	{
		FuncNameStr = UFuncNode.GetCachedName().ToString();
	}

	// ── Position getters → read PCG point position ──
	if (FuncNameStr == TEXT("GetPosition2D") || FuncNameStr == TEXT("GetPosition2D_Double"))
	{
		const FCompPin* OutPin = Node.FindOutput(TEXT("ReturnValue"));
		if (OutPin)
		{
			const FString VarName = PinToVariableName.FindRef(OutPin);
			if (CurrentOptions.bIncludeComments)
			{
				HLSLStatements.Add(TEXT("// Position 2D (from PCG point transform)"));
			}
			HLSLStatements.Add(FString::Printf(
				TEXT("float2 %s = %s_GetPosition(%s_DataIndex, ElementIndex).xy;"),
				*VarName,
				*CurrentOptions.InputPinName, *CurrentOptions.InputPinName));
		}
		return true;
	}

	if (FuncNameStr == TEXT("GetPosition3D") || FuncNameStr == TEXT("GetPosition3D_Double"))
	{
		const FCompPin* OutPin = Node.FindOutput(TEXT("ReturnValue"));
		if (OutPin)
		{
			const FString VarName = PinToVariableName.FindRef(OutPin);
			if (CurrentOptions.bIncludeComments)
			{
				HLSLStatements.Add(TEXT("// Position 3D (from PCG point transform)"));
			}
			HLSLStatements.Add(FString::Printf(
				TEXT("float3 %s = %s_GetPosition(%s_DataIndex, ElementIndex);"),
				*VarName,
				*CurrentOptions.InputPinName, *CurrentOptions.InputPinName));
		}
		return true;
	}

	// ── Simple math/construction functions → inline HLSL ──

	// MakeBox2DFromRadius(Radius) → float4(center.xy, extent.xy) — for PCG use we skip
	// and just pass through the radius as a float
	if (FuncNameStr.Contains(TEXT("MakeBox2DFromRadius")))
	{
		// Box2D isn't a native HLSL type; output a float4(-R, -R, R, R)
		const FCompPin* OutPin = Node.FindOutput(TEXT("ReturnValue"));
		if (OutPin)
		{
			const FString VarName = PinToVariableName.FindRef(OutPin);
			FString RadiusVar = TEXT("0.0f");
			const FCompPin* RadiusPin = Node.FindInput(TEXT("Radius"));
			if (RadiusPin)
			{
				RadiusVar = ResolveInputPin(*RadiusPin);
			}
			if (CurrentOptions.bIncludeComments)
			{
				HLSLStatements.Add(TEXT("// MakeBox2DFromRadius → float4(-R, -R, R, R)"));
			}
			// Always float4 for Box2D, regardless of pin type resolution
			HLSLStatements.Add(FString::Printf(
				TEXT("float4 %s = float4(-%s, -%s, %s, %s);"),
				*VarName, *RadiusVar, *RadiusVar, *RadiusVar, *RadiusVar));
		}
		return true;
	}

	// Generic fallback: for any UFunction with a single return value,
	// try to map common names to HLSL builtins
	static const TMap<FString, FString> SimpleUFuncMap = {
		{ TEXT("Abs"), TEXT("abs") },
		{ TEXT("Sin"), TEXT("sin") },
		{ TEXT("Cos"), TEXT("cos") },
		{ TEXT("Tan"), TEXT("tan") },
		{ TEXT("Asin"), TEXT("asin") },
		{ TEXT("Acos"), TEXT("acos") },
		{ TEXT("Atan"), TEXT("atan") },
		{ TEXT("Atan2"), TEXT("atan2") },
		{ TEXT("Sqrt"), TEXT("sqrt") },
		{ TEXT("Exp"), TEXT("exp") },
		{ TEXT("Log"), TEXT("log") },
		{ TEXT("Log2"), TEXT("log2") },
		{ TEXT("Ceil"), TEXT("ceil") },
		{ TEXT("Floor"), TEXT("floor") },
		{ TEXT("Round"), TEXT("round") },
		{ TEXT("Frac"), TEXT("frac") },
		{ TEXT("Sign"), TEXT("sign") },
		{ TEXT("Pow"), TEXT("pow") },
		{ TEXT("Min"), TEXT("min") },
		{ TEXT("Max"), TEXT("max") },
		{ TEXT("Clamp"), TEXT("clamp") },
		{ TEXT("Saturate"), TEXT("saturate") },
		{ TEXT("Lerp"), TEXT("lerp") },
		{ TEXT("Length"), TEXT("length") },
		{ TEXT("Distance"), TEXT("distance") },
		{ TEXT("Normalize"), TEXT("normalize") },
		{ TEXT("Dot"), TEXT("dot") },
		{ TEXT("Cross"), TEXT("cross") },
	};

	if (const FString* HLSLFunc = SimpleUFuncMap.Find(FuncNameStr))
	{
		const FCompPin* OutPin = Node.FindOutput(TEXT("ReturnValue"));
		if (OutPin)
		{
			// Collect input arguments in order
			TArray<FString> Args;
			for (const FCompPin& InputPin : Node.GetInputPins())
			{
				FString Arg;
				for (const FCompPin& LinkedPin : InputPin.GetLinkedTo())
				{
					if (const FString* Found = PinToVariableName.Find(&LinkedPin))
					{
						Arg = *Found;
						break;
					}
				}
				if (Arg.IsEmpty())
				{
					Arg = FPGLVoxelPinTypeMapper::GetDefaultLiteral(InputPin.Type);
				}
				Args.Add(Arg);
			}

			const FString VarName = PinToVariableName.FindRef(OutPin);
			const FString HLSLType = FPGLVoxelPinTypeMapper::ToHLSLType(OutPin->Type);
			const FString ArgStr = FString::Join(Args, TEXT(", "));

			if (CurrentOptions.bIncludeComments)
			{
				HLSLStatements.Add(FString::Printf(TEXT("// UFunction: %s → %s"), *FuncNameStr, **HLSLFunc));
			}
			HLSLStatements.Add(FString::Printf(TEXT("%s %s = %s(%s);"),
				*HLSLType, *VarName, **HLSLFunc, *ArgStr));
			return true;
		}
	}

	// Not a recognized UFunction — fall back to unsupported
	Warnings.Add(FString::Printf(TEXT("Unhandled UFunction: %s"), *FuncNameStr));
	return false;
}

FString FPGLVoxelGraphTraverser::ResolveInputPin(const FCompPin& InputPin)
{
	// Check for connections
	for (const FCompPin& LinkedPin : InputPin.GetLinkedTo())
	{
		if (const FString* Found = PinToVariableName.Find(&LinkedPin))
		{
			return *Found;
		}
	}

	// Use pin's actual default value if not connected
	return PinDefaultToHLSL(InputPin);
}

bool FPGLVoxelGraphTraverser::ProcessBlockMacroComputeNode(
	FCompNode& Node, const FString& ISPCCode, const TMap<FName, FString>& LocalPinMap)
{
	const FVoxelNode& VoxelNode = Node.GetVoxelNode();

	// Get output variable name (most block-macro nodes have a single "Value" or "ReturnValue" output)
	const FCompPin* OutPin = Node.FindOutput(TEXT("Value"));
	if (!OutPin && Node.GetOutputPins().Num() > 0)
	{
		OutPin = &Node.GetOutputPin(0);
	}
	if (!OutPin) return false;

	const FString OutVar = PinToVariableName.FindRef(OutPin);
	const FString OutType = FPGLVoxelPinTypeMapper::ToHLSLType(OutPin->Type);
	if (OutVar.IsEmpty()) return false;

	// Helper to look up a pin value from LocalPinMap by name
	auto GetPin = [&](const FString& PinName) -> FString
	{
		if (const FString* Val = LocalPinMap.Find(FName(*PinName)))
		{
			return *Val;
		}
		return TEXT("0");
	};

	// ─── Noise functions ───
	// Pattern: {Value} = GetPerlin2D({Seed}, {Position} / {FeatureScale}) * {Amplitude}
	// We emit: float OutVar; PGL_PERLIN_2D(OutVar, seed, pos / featureScale); OutVar *= amplitude;

	auto EmitNoiseNode = [&](const FString& MacroName, const FString& ISPCFuncName, bool bIs3D) -> bool
	{
		if (!ISPCCode.Contains(ISPCFuncName)) return false;

		// Flag required helpers (done by transformer's RemapFunctions, but let's be safe)
		FPGLISPCToHLSLTransformer::Transform(ISPCCode, LocalPinMap, RequiredHelpers);

		const FString Seed = GetPin(TEXT("Seed"));
		const FString Position = GetPin(TEXT("Position"));
		const FString FeatureScale = GetPin(TEXT("FeatureScale"));
		const FString Amplitude = GetPin(TEXT("Amplitude"));

		if (CurrentOptions.bIncludeComments)
		{
			HLSLStatements.Add(FString::Printf(TEXT("// Noise: %s"), *ISPCFuncName));
		}

		// Declare output
		HLSLStatements.Add(FString::Printf(TEXT("float %s;"), *OutVar));

		// Position / FeatureScale expression
		FString PosExpr = Position;
		if (FeatureScale != TEXT("0") && FeatureScale != TEXT("0.0f") && FeatureScale != TEXT("1.000000f"))
		{
			PosExpr = FString::Printf(TEXT("(%s / %s)"), *Position, *FeatureScale);
		}

		// Emit the macro call
		HLSLStatements.Add(FString::Printf(TEXT("%s(%s, %s, %s)"), *MacroName, *OutVar, *Seed, *PosExpr));

		// Apply amplitude multiplier
		if (Amplitude != TEXT("1.000000f") && Amplitude != TEXT("1.0f") && Amplitude != TEXT("1"))
		{
			HLSLStatements.Add(FString::Printf(TEXT("%s *= %s;"), *OutVar, *Amplitude));
		}

		return true;
	};

	// Check each noise type
	if (EmitNoiseNode(TEXT("PGL_PERLIN_2D"), TEXT("GetPerlin2D"), false)) return true;
	if (EmitNoiseNode(TEXT("PGL_PERLIN_3D"), TEXT("GetPerlin3D"), true)) return true;
	if (EmitNoiseNode(TEXT("PGL_SIMPLEX_2D"), TEXT("GetSimplex2D"), false)) return true;
	if (EmitNoiseNode(TEXT("PGL_SIMPLEX_3D"), TEXT("GetSimplex3D"), true)) return true;
	if (EmitNoiseNode(TEXT("PGL_VALUE_2D"), TEXT("GetValue2D"), false)) return true;
	if (EmitNoiseNode(TEXT("PGL_VALUE_3D"), TEXT("GetValue3D"), true)) return true;

	// Cellular noise has a Jitter parameter
	auto EmitCellularNode = [&](const FString& MacroName, const FString& ISPCFuncName) -> bool
	{
		if (!ISPCCode.Contains(ISPCFuncName)) return false;

		FPGLISPCToHLSLTransformer::Transform(ISPCCode, LocalPinMap, RequiredHelpers);

		const FString Seed = GetPin(TEXT("Seed"));
		const FString Position = GetPin(TEXT("Position"));
		const FString FeatureScale = GetPin(TEXT("FeatureScale"));
		const FString Amplitude = GetPin(TEXT("Amplitude"));
		const FString Jitter = GetPin(TEXT("Jitter"));

		if (CurrentOptions.bIncludeComments)
		{
			HLSLStatements.Add(FString::Printf(TEXT("// Cellular Noise: %s"), *ISPCFuncName));
		}

		HLSLStatements.Add(FString::Printf(TEXT("float %s;"), *OutVar));

		FString PosExpr = Position;
		if (FeatureScale != TEXT("0") && FeatureScale != TEXT("0.0f") && FeatureScale != TEXT("1.000000f"))
		{
			PosExpr = FString::Printf(TEXT("(%s / %s)"), *Position, *FeatureScale);
		}

		FString JitterVal = Jitter.IsEmpty() ? TEXT("1.0f") : Jitter;
		HLSLStatements.Add(FString::Printf(TEXT("%s(%s, %s, %s, %s)"), *MacroName, *OutVar, *Seed, *PosExpr, *JitterVal));

		if (Amplitude != TEXT("1.000000f") && Amplitude != TEXT("1.0f") && Amplitude != TEXT("1"))
		{
			HLSLStatements.Add(FString::Printf(TEXT("%s *= %s;"), *OutVar, *Amplitude));
		}

		return true;
	};

	if (EmitCellularNode(TEXT("PGL_CELLULAR_2D"), TEXT("GetCellularNoise2D"))) return true;
	if (EmitCellularNode(TEXT("PGL_CELLULAR_3D"), TEXT("GetCellularNoise3D"))) return true;
	if (EmitCellularNode(TEXT("PGL_CELLULAR_2D"), TEXT("GetTrueDistanceCellularNoise2D"))) return true;
	if (EmitCellularNode(TEXT("PGL_CELLULAR_3D"), TEXT("GetTrueDistanceCellularNoise3D"))) return true;

	// ─── Hash / Random ───
	if (ISPCCode.Contains(TEXT("MurmurHash32")))
	{
		RequiredHelpers.bNeedsHash = true;

		// For hash nodes, the pattern is typically: {ReturnValue} = MurmurHash32({A})
		// or {ReturnValue} = MurmurHash32({A}, {B})
		if (CurrentOptions.bIncludeComments)
		{
			HLSLStatements.Add(TEXT("// MurmurHash32 → PGL_HASH block macro"));
		}

		const FString A = GetPin(TEXT("A"));
		const FString B = GetPin(TEXT("B"));

		HLSLStatements.Add(FString::Printf(TEXT("uint %s;"), *OutVar));

		if (LocalPinMap.Contains(FName(TEXT("B"))))
		{
			HLSLStatements.Add(FString::Printf(TEXT("PGL_HASH2(%s, %s, %s)"), *OutVar, *A, *B));
		}
		else
		{
			HLSLStatements.Add(FString::Printf(TEXT("PGL_HASH(%s, %s)"), *OutVar, *A));
		}
		return true;
	}

	if (ISPCCode.Contains(TEXT("RandRange")))
	{
		RequiredHelpers.bNeedsRandRange = true;
		RequiredHelpers.bNeedsHash = true;

		if (CurrentOptions.bIncludeComments)
		{
			HLSLStatements.Add(TEXT("// RandRange → PGL_RAND_RANGE block macro"));
		}

		const FString Seed = GetPin(TEXT("Seed"));
		const FString Range = GetPin(TEXT("Range"));

		HLSLStatements.Add(FString::Printf(TEXT("float %s;"), *OutVar));
		HLSLStatements.Add(FString::Printf(TEXT("PGL_RAND_RANGE(%s, %s, %s)"), *OutVar, *Seed, *Range));
		return true;
	}

	// ─── Quaternion functions ───
	if (ISPCCode.Contains(TEXT("MakeQuaternionFromEuler")))
	{
		RequiredHelpers.bNeedsQuaternionFromEuler = true;
		const FString Euler = GetPin(TEXT("Euler"));
		if (CurrentOptions.bIncludeComments)
		{
			HLSLStatements.Add(TEXT("// QuatFromEuler → PGL_QUAT_FROM_EULER block macro"));
		}
		HLSLStatements.Add(FString::Printf(TEXT("float4 %s;"), *OutVar));
		HLSLStatements.Add(FString::Printf(TEXT("PGL_QUAT_FROM_EULER(%s, %s)"), *OutVar, *Euler));
		return true;
	}

	if (ISPCCode.Contains(TEXT("MakeEulerFromQuaternion")))
	{
		RequiredHelpers.bNeedsEulerFromQuaternion = true;
		const FString Quat = GetPin(TEXT("Quaternion"));
		if (CurrentOptions.bIncludeComments)
		{
			HLSLStatements.Add(TEXT("// EulerFromQuat → PGL_EULER_FROM_QUAT block macro"));
		}
		HLSLStatements.Add(FString::Printf(TEXT("float3 %s;"), *OutVar));
		HLSLStatements.Add(FString::Printf(TEXT("PGL_EULER_FROM_QUAT(%s, %s)"), *OutVar, *Quat));
		return true;
	}

	if (ISPCCode.Contains(TEXT("MakeQuaternionFromX")))
	{
		RequiredHelpers.bNeedsQuaternionFromAxis = true;
		const FString Angle = GetPin(TEXT("AngleDeg"));
		HLSLStatements.Add(FString::Printf(TEXT("float4 %s;"), *OutVar));
		HLSLStatements.Add(FString::Printf(TEXT("PGL_QUAT_FROM_AXIS_X(%s, %s)"), *OutVar, *Angle));
		return true;
	}
	if (ISPCCode.Contains(TEXT("MakeQuaternionFromY")))
	{
		RequiredHelpers.bNeedsQuaternionFromAxis = true;
		const FString Angle = GetPin(TEXT("AngleDeg"));
		HLSLStatements.Add(FString::Printf(TEXT("float4 %s;"), *OutVar));
		HLSLStatements.Add(FString::Printf(TEXT("PGL_QUAT_FROM_AXIS_Y(%s, %s)"), *OutVar, *Angle));
		return true;
	}
	if (ISPCCode.Contains(TEXT("MakeQuaternionFromZ")))
	{
		RequiredHelpers.bNeedsQuaternionFromAxis = true;
		const FString Angle = GetPin(TEXT("AngleDeg"));
		HLSLStatements.Add(FString::Printf(TEXT("float4 %s;"), *OutVar));
		HLSLStatements.Add(FString::Printf(TEXT("PGL_QUAT_FROM_AXIS_Z(%s, %s)"), *OutVar, *Angle));
		return true;
	}

	// ─── Distance Field Color ───
	if (ISPCCode.Contains(TEXT("GetDistanceFieldColor")))
	{
		RequiredHelpers.bNeedsDistanceFieldColor = true;
		const FString Distance = GetPin(TEXT("Distance"));
		HLSLStatements.Add(FString::Printf(TEXT("float4 %s;"), *OutVar));
		HLSLStatements.Add(FString::Printf(TEXT("PGL_DISTANCE_FIELD_COLOR(%s, %s)"), *OutVar, *Distance));
		return true;
	}

	return false;
}

void FPGLVoxelGraphTraverser::ProcessOutputNode(FCompNode& Node, const FVoxelOutputNode& OutputNode)
{
	const FString NodeTypeName = OutputNode.GetStruct()->GetName();

	if (CurrentOptions.bIncludeComments)
	{
		HLSLStatements.Add(FString::Printf(TEXT("// Output Node: %s"), *NodeTypeName));
	}

	// Iterate all input pins on the output node and write recognized ones to PCG attributes
	for (const FCompPin& InputPin : Node.GetInputPins())
	{
		const FName PinName = InputPin.Name;
		const FString PinNameStr = PinName.ToString();

		// Skip pins that aren't meaningful for PCG output
		// (e.g. SurfaceType, HeightRange, advanced display pins)
		if (PinNameStr == TEXT("SurfaceType") ||
			PinNameStr == TEXT("HeightRange") ||
			PinNameStr == TEXT("RelativeHeightRange") ||
			PinNameStr == TEXT("Alpha") ||
			PinNameStr == TEXT("EnableLayerOverride") ||
			PinNameStr == TEXT("LayerOverride") ||
			PinNameStr == TEXT("EnableBlendModeOverride") ||
			PinNameStr == TEXT("BlendModeOverride") ||
			PinNameStr == TEXT("Bounds"))
		{
			continue;
		}

		// Find the source variable connected to this input
		FString SourceVar;
		for (const FCompPin& LinkedPin : InputPin.GetLinkedTo())
		{
			if (const FString* Found = PinToVariableName.Find(&LinkedPin))
			{
				SourceVar = *Found;
				break;
			}
		}

		if (SourceVar.IsEmpty())
		{
			// Not connected — skip writing this output
			continue;
		}

		// Map the pin to a PCG attribute write
		// For Height output, write to a "Height" attribute
		const FString AttrName = PinNameStr;

		FPGLGraphOutput GraphOutput;
		GraphOutput.Name = PinName;
		GraphOutput.HLSLType = FPGLVoxelPinTypeMapper::ToHLSLType(InputPin.Type);
		GraphOutput.SourceVariableName = SourceVar;
		GraphOutput.PCGWriteStatement = FPGLVoxelPinTypeMapper::MakePCGWriteStatement(
			InputPin.Type, CurrentOptions.OutputPinName, SourceVar, AttrName);
		GraphOutputs.Add(GraphOutput);
	}
}
