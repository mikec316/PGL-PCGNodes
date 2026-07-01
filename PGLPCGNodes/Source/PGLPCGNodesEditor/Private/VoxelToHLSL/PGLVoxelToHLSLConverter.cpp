// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "VoxelToHLSL/PGLVoxelToHLSLConverter.h"
#include "VoxelToHLSL/PGLVoxelGraphTraverser.h"
#include "VoxelToHLSL/PGLHLSLDocumentBuilder.h"

#include "VoxelGraph.h"
#include "Compute/PCGComputeSource.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

FPGLHLSLTranslationResult UPGLVoxelToHLSLConverter::ConvertVoxelGraphToHLSL(
	UVoxelGraph* VoxelGraph,
	const FPGLVoxelToHLSLOptions& Options)
{
	FPGLHLSLTranslationResult Result;

	if (!VoxelGraph)
	{
		Result.Status = EPGLHLSLTranslationStatus::Failure;
		Result.Warnings.Add(TEXT("Null VoxelGraph provided"));
		return Result;
	}

	// Run the graph traverser
	FPGLVoxelGraphTraverser Traverser;
	if (!Traverser.Traverse(VoxelGraph, Options))
	{
		Result.Status = EPGLHLSLTranslationStatus::Failure;
		Result.Warnings = Traverser.GetWarnings();
		return Result;
	}

	// Build the HLSL document
	FPGLHLSLDocumentBuilder Builder;
	Builder.SetGraphName(VoxelGraph->GetName());
	Builder.SetInputPinName(Options.InputPinName);
	Builder.SetOutputPinName(Options.OutputPinName);
	Builder.SetInputs(Traverser.GetGraphInputs());
	Builder.SetOutputs(Traverser.GetGraphOutputs());
	Builder.SetStatements(Traverser.GetHLSLStatements());
	Builder.SetRequiredHelpers(Traverser.GetRequiredHelpers());
	Builder.SetWarnings(Traverser.GetWarnings());

	Result.GeneratedHLSL = Builder.Build();
	Result.Warnings = Traverser.GetWarnings();
	Result.UnsupportedNodeTypes.Append(Traverser.GetUnsupportedNodeTypes());
	Result.TotalNodes = Traverser.GetTotalNodes();
	Result.TranslatedNodes = Traverser.GetTranslatedNodes();

	// Collect output attribute declarations for user setup
	// Map HLSL types to PCG EPCGKernelAttributeType names for the "Attributes to Create" dropdown
	auto HLSLTypeToPCGTypeName = [](const FString& HLSLType) -> FString
	{
		if (HLSLType == TEXT("float"))  return TEXT("Float");
		if (HLSLType == TEXT("int"))    return TEXT("Int");
		if (HLSLType == TEXT("uint"))   return TEXT("Uint");
		if (HLSLType == TEXT("bool"))   return TEXT("Bool");
		if (HLSLType == TEXT("float2")) return TEXT("Float2");
		if (HLSLType == TEXT("float3")) return TEXT("Float3");
		if (HLSLType == TEXT("float4")) return TEXT("Float4");
		return TEXT("Float");
	};
	for (const FPGLGraphOutput& Output : Traverser.GetGraphOutputs())
	{
		Result.RequiredOutputAttributes.Add(FString::Printf(TEXT("%s (Type: %s)"),
			*Output.Name.ToString(), *HLSLTypeToPCGTypeName(Output.HLSLType)));
	}

	if (Result.UnsupportedNodeTypes.Num() > 0)
	{
		Result.Status = EPGLHLSLTranslationStatus::PartialSuccess;
	}
	else
	{
		Result.Status = EPGLHLSLTranslationStatus::Success;
	}

	return Result;
}

UPCGComputeSource* UPGLVoxelToHLSLConverter::SaveAsComputeSource(
	const FString& PackagePath,
	const FString& AssetName,
	const FString& HLSLSource)
{
	if (HLSLSource.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("PGLVoxelToHLSL: Cannot save empty HLSL source"));
		return nullptr;
	}

	const FString FullPath = PackagePath / AssetName;
	UPackage* Package = CreatePackage(*FullPath);
	if (!Package)
	{
		UE_LOG(LogTemp, Error, TEXT("PGLVoxelToHLSL: Failed to create package at %s"), *FullPath);
		return nullptr;
	}

	Package->FullyLoad();

	UPCGComputeSource* ComputeSource = NewObject<UPCGComputeSource>(
		Package,
		*AssetName,
		RF_Public | RF_Standalone | RF_Transactional);

	if (!ComputeSource)
	{
		UE_LOG(LogTemp, Error, TEXT("PGLVoxelToHLSL: Failed to create UPCGComputeSource"));
		return nullptr;
	}

#if WITH_EDITOR
	// UPCGComputeSource::SetShaderText was removed in UE 5.8. The 5.8 header also
	// declares a SetSource(...) but its definition is missing from PCGComputeSource.cpp,
	// so we use the IPCGCodeEditorTextProvider override SetSourceText() which is the
	// exported, fully-defined entry point.
	ComputeSource->SetSourceText(HLSLSource);
#endif

	// Notify the asset registry
	FAssetRegistryModule::AssetCreated(ComputeSource);

	// Mark the package dirty so it can be saved
	Package->MarkPackageDirty();

	// Save the package
	const FString PackageFileName = FPackageName::LongPackageNameToFilename(FullPath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, ComputeSource, *PackageFileName, SaveArgs);

	UE_LOG(LogTemp, Log, TEXT("PGLVoxelToHLSL: Saved PCG Compute Source to %s"), *FullPath);

	return ComputeSource;
}
