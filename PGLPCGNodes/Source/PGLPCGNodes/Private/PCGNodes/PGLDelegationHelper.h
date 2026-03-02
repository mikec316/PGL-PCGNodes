// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGData.h"

struct FPCGContext;
class UPCGSettings;

/**
 * Shared helper functions for PGL wrapper nodes that delegate execution to
 * PVE elements at runtime via UClass reflection.
 */
namespace PGLDelegation
{
	/**
	 * Copy properties from one UStruct data block to another, matched by name
	 * and type.  Works across different UStruct types as long as field names and
	 * FProperty types match.
	 */
	void CopyMatchingStructProperties(
		const void* SrcData, const UStruct* SrcStruct,
		void* DstData,       const UStruct* DstStruct);

	/**
	 * Copy top-level (non-inherited) UObject properties by matching name+type.
	 * Only iterates properties declared by SrcDeclaredClass (not its parents).
	 */
	void CopyMatchingObjectProperties(
		const UObject* Src, UObject* Dst,
		const UClass* SrcDeclaredClass);

	/**
	 * Read FrameMeshPath + FrameMeshTransform from the first UPVData input.
	 */
	void CaptureFrameMeshFromInput(
		const FPCGDataCollection& InputData,
		FString& OutPath, FTransform& OutTransform);

	/**
	 * Ensure FrameMeshPath + FrameMeshTransform exist in every UPVData output.
	 */
	void EnsureFrameMeshInOutput(
		FPCGDataCollection& OutputData,
		const FString& Path, const FTransform& Transform);

	/**
	 * Execute a PVE element by creating a temp node with PVE settings,
	 * initialising a sub-context, and driving Execute() until completion.
	 * Returns true on success.
	 */
	bool DelegateToPVEElement(
		FPCGContext* CallerContext,
		UPCGSettings* PVESettings,
		FPCGDataCollection& OutOutput);

} // namespace PGLDelegation
