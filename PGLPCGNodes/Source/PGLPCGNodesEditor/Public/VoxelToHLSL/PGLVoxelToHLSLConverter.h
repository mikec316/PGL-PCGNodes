// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelToHLSL/PGLVoxelToHLSLTypes.h"
#include "PGLVoxelToHLSLConverter.generated.h"

class UVoxelGraph;
class UPCGComputeSource;

UCLASS()
class PGLPCGNODESEDITOR_API UPGLVoxelToHLSLConverter : public UObject
{
	GENERATED_BODY()

public:
	// Convert a Voxel Graph asset to HLSL suitable for PCG Compute Source
	UFUNCTION(BlueprintCallable, Category = "PGL|VoxelToHLSL")
	static FPGLHLSLTranslationResult ConvertVoxelGraphToHLSL(
		UVoxelGraph* VoxelGraph,
		const FPGLVoxelToHLSLOptions& Options);

	// Save generated HLSL as a UPCGComputeSource asset
	// Returns the created asset, or nullptr on failure
	UFUNCTION(BlueprintCallable, Category = "PGL|VoxelToHLSL")
	static UPCGComputeSource* SaveAsComputeSource(
		const FString& PackagePath,
		const FString& AssetName,
		const FString& HLSLSource);
};
