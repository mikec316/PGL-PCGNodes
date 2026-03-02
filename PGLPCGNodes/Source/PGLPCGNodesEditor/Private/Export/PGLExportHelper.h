// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FPVExportParams;
struct FManagedArrayCollection;

/**
 * PGL Export Helper — replicates the PVE export pipeline for PGL graphs.
 *
 * PV::Export::ExportCollectionAsMesh() is private to ProceduralVegetationEditor,
 * so we replicate the critical path here using only exported engine/PVE APIs.
 */
namespace PGL::Export
{
	/** Result of an export operation. */
	enum class EExportResult : uint8
	{
		Success,
		Fail,
		Canceled,
		Skipped
	};

	/**
	 * Progress/cancellation callback.
	 * @param Stage    Human-readable stage description
	 * @param Progress 0.0–1.0 overall progress
	 * @return         false to cancel the export
	 */
	using FStatusReportCallback = TFunction<bool(const FText& Stage, float Progress)>;

	/**
	 * Export an FManagedArrayCollection as a mesh asset.
	 *
	 * Dispatches to static or skeletal mesh path based on ExportParams.ExportMeshType.
	 * Handles package creation, asset replacement, and cleanup.
	 *
	 * @param Collection           The PVE collection to export
	 * @param ExportParams         Export configuration (path, name, mesh type, nanite, etc.)
	 * @param OutCreatedAssets     Receives paths of created assets
	 * @param StatusReportCallback Progress/cancellation callback
	 * @return                     Export result
	 */
	EExportResult ExportCollectionAsMesh(
		const FManagedArrayCollection& Collection,
		const FPVExportParams& ExportParams,
		TArray<FString>& OutCreatedAssets,
		const FStatusReportCallback& StatusReportCallback);

	// -------------------------------------------------------------------------
	//  Export stages — used for progress tracking
	// -------------------------------------------------------------------------

	enum class EExportMeshStage : uint8
	{
		InitializingExportMesh,
		CombiningMeshes,
		CopyingMeshDataToOutputMesh,
		RecreatingBoneTree,
		ComputingBoneWeights,
		CopyingSkinWeightProfiles,
		BuildingNaniteAssemblies,
		ImportingDynamicWind,
		BuildingMesh,
		Complete,
		Count
	};

} // namespace PGL::Export
