// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "Export/PGLExportHelper.h"

#include "IAssetTools.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "PlanarCut.h"
#include "PVExportParams.h"
#include "PVWindSettings.h"
#include "SkinnedAssetCompiler.h"
#include "StaticMeshCompiler.h"
#include "UDynamicMesh.h"

#include "Animation/Skeleton.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"

#include "DynamicMesh/DynamicBoneAttribute.h"
#include "DynamicMesh/DynamicVertexSkinWeightsAttribute.h"
#include "DynamicMesh/MeshTransforms.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"

#include "Facades/PVBoneFacade.h"
#include "Facades/PVBranchFacade.h"
#include "Facades/PVFoliageFacade.h"
#include "Facades/PVPlantFacade.h"
#include "Facades/PVPointFacade.h"

#include "GeometryCollection/GeometryCollection.h"

#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBoneWeightFunctions.h"

#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"

#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "PGLExportHelper"

using namespace PGL::Export;

// =============================================================================
//  Progress tracking
// =============================================================================

namespace
{

float CalculateMeshExportStageWeight(
	EExportMeshStage Stage,
	EPVExportMeshType MeshType,
	bool bBuildNaniteAssemblies,
	bool bExportFoliage)
{
	const bool bSkeletal = (MeshType == EPVExportMeshType::SkeletalMesh);
	switch (Stage)
	{
	case EExportMeshStage::InitializingExportMesh:      return 1.0f;
	case EExportMeshStage::CombiningMeshes:             return (bBuildNaniteAssemblies || !bExportFoliage) ? 0.0f : 1.0f;
	case EExportMeshStage::CopyingMeshDataToOutputMesh: return 1.0f;
	case EExportMeshStage::RecreatingBoneTree:          return bSkeletal ? 0.2f : 0.0f;
	case EExportMeshStage::ComputingBoneWeights:        return bSkeletal ? 2.0f : 0.0f;
	case EExportMeshStage::CopyingSkinWeightProfiles:   return bSkeletal ? 0.1f : 0.0f;
	case EExportMeshStage::BuildingNaniteAssemblies:    return bBuildNaniteAssemblies ? 3.0f : 0.0f;
	case EExportMeshStage::ImportingDynamicWind:        return bSkeletal ? 0.1f : 0.0f;
	case EExportMeshStage::BuildingMesh:                return bBuildNaniteAssemblies ? 3.0f : 4.0f;
	case EExportMeshStage::Complete:                    return 0.5f;
	default: return 0.0f;
	}
}

float CalculateMeshExportProgress(
	EExportMeshStage CurrentStage,
	float StageProgress,
	EPVExportMeshType MeshType,
	bool bBuildNaniteAssemblies,
	bool bExportFoliage)
{
	float TotalWeight = 0.0f;
	float CompletedWeight = 0.0f;

	for (int32 i = 0; i < (int32)EExportMeshStage::Count; ++i)
	{
		const EExportMeshStage Stage = (EExportMeshStage)i;
		const float Weight = CalculateMeshExportStageWeight(Stage, MeshType, bBuildNaniteAssemblies, bExportFoliage);
		TotalWeight += Weight;

		if ((int32)Stage < (int32)CurrentStage)
		{
			CompletedWeight += Weight;
		}
		else if (Stage == CurrentStage)
		{
			CompletedWeight += Weight * FMath::Clamp(StageProgress, 0.0f, 1.0f);
		}
	}

	return (TotalWeight > 0.0f) ? (CompletedWeight / TotalWeight) : 0.0f;
}

FText GetMeshExportStageText(EExportMeshStage Stage)
{
	switch (Stage)
	{
	case EExportMeshStage::InitializingExportMesh:
		return LOCTEXT("Stage_Init", "Initializing export mesh...");
	case EExportMeshStage::CombiningMeshes:
		return LOCTEXT("Stage_Combine", "Combining meshes...");
	case EExportMeshStage::CopyingMeshDataToOutputMesh:
		return LOCTEXT("Stage_Copy", "Copying mesh data...");
	case EExportMeshStage::RecreatingBoneTree:
		return LOCTEXT("Stage_BoneTree", "Recreating bone tree...");
	case EExportMeshStage::ComputingBoneWeights:
		return LOCTEXT("Stage_Weights", "Computing bone weights...");
	case EExportMeshStage::CopyingSkinWeightProfiles:
		return LOCTEXT("Stage_SkinCopy", "Copying skin weight profiles...");
	case EExportMeshStage::BuildingNaniteAssemblies:
		return LOCTEXT("Stage_Nanite", "Building Nanite assemblies...");
	case EExportMeshStage::ImportingDynamicWind:
		return LOCTEXT("Stage_Wind", "Importing dynamic wind...");
	case EExportMeshStage::BuildingMesh:
		return LOCTEXT("Stage_Build", "Building mesh...");
	case EExportMeshStage::Complete:
		return LOCTEXT("Stage_Complete", "Export complete.");
	default:
		return FText::GetEmpty();
	}
}

// Helper: report progress through the callback, returning false if cancelled.
bool ReportProgress(
	const FStatusReportCallback& Callback,
	EExportMeshStage Stage,
	float StageProgress,
	EPVExportMeshType MeshType,
	bool bNanite,
	bool bFoliage)
{
	if (!Callback) return true;
	const float Overall = CalculateMeshExportProgress(Stage, StageProgress, MeshType, bNanite, bFoliage);
	return Callback(GetMeshExportStageText(Stage), Overall);
}

} // anonymous namespace

// =============================================================================
//  Package / asset helpers
// =============================================================================

namespace
{

/** Find or create a UPackage for an asset path, optionally cleaning up stale objects. */
UPackage* CreateAssetPackage(const FString& PackagePath)
{
	UPackage* Package = FindPackage(nullptr, *PackagePath);
	if (!Package)
	{
		Package = CreatePackage(*PackagePath);
	}
	return Package;
}

/** Get a unique asset path if Append policy. */
FString GetValidAssetPath(const FPVExportParams& Params)
{
	FString DesiredPath = Params.GetOutputMeshPackagePath();

	if (Params.ReplacePolicy == EPVAssetReplacePolicy::Append)
	{
		FString UniqueName;
		FString UniquePackageName;
		IAssetTools::Get().CreateUniqueAssetName(DesiredPath, TEXT(""), UniquePackageName, UniqueName);
		return UniquePackageName;
	}

	return DesiredPath;
}

/** Extract material array from the collection's MaterialPath attribute. */
TArray<TObjectPtr<UMaterialInterface>> GetMaterialsFromCollection(const FManagedArrayCollection& Collection)
{
	TArray<TObjectPtr<UMaterialInterface>> Materials;

	static const FName MaterialGroupName = FGeometryCollection::MaterialGroup;
	static const FName MaterialPathAttrName("MaterialPath");

	if (!Collection.HasGroup(MaterialGroupName) ||
		!Collection.HasAttribute(MaterialPathAttrName, MaterialGroupName))
	{
		return Materials;
	}

	const TManagedArray<FString>& MaterialPaths =
		Collection.GetAttribute<FString>(MaterialPathAttrName, MaterialGroupName);

	for (const FString& Path : MaterialPaths)
	{
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *Path);
		Materials.Add(Mat);
	}

	return Materials;
}

} // anonymous namespace

// =============================================================================
//  Collection → DynamicMesh conversion
// =============================================================================

namespace
{

TObjectPtr<UDynamicMesh> CollectionToDynamicMesh(const FManagedArrayCollection& Collection)
{
	TObjectPtr<UDynamicMesh> DynMeshObj = NewObject<UDynamicMesh>();

	// Check for transforms — if none, return empty mesh
	static const FName TransformGroupName = FGeometryCollection::TransformGroup;
	if (!Collection.HasGroup(TransformGroupName) ||
		Collection.NumElements(TransformGroupName) == 0)
	{
		return DynMeshObj;
	}

	// Copy to FGeometryCollection for conversion
	TUniquePtr<FGeometryCollection> GeomCollection(Collection.NewCopy<FGeometryCollection>());

	// Get transforms
	static const FName TransformAttrName("Transform");
	const TManagedArray<FTransform3f>& BoneTransforms =
		GeomCollection->GetAttribute<FTransform3f>(TransformAttrName, TransformGroupName);

	// Build identity indices (convert all transforms)
	TArray<int32> TransformIndices;
	TransformIndices.Reserve(BoneTransforms.Num());
	for (int32 i = 0; i < BoneTransforms.Num(); ++i)
	{
		TransformIndices.Add(i);
	}

	// Convert
	FTransform TransformOut = FTransform::Identity;
	UE::Geometry::FDynamicMesh3 CombinedMesh;
	ConvertGeometryCollectionToDynamicMesh(
		CombinedMesh,
		TransformOut,
		/*bCenterPivot=*/ false,
		*GeomCollection,
		/*bWeldEdges=*/ true,
		BoneTransforms.GetConstArray(),
		/*bUseRelativeTransforms=*/ true,
		TransformIndices);

	DynMeshObj->SetMesh(MoveTemp(CombinedMesh));
	return DynMeshObj;
}

} // anonymous namespace

// =============================================================================
//  Foliage mesh combining
// =============================================================================

namespace
{

using FMeshInstanceCombinedCallback =
	TFunction<void(const FString& MeshPath, int32 InstanceID, int32 VertexStart, int32 VertexCount)>;

/** Static mesh → DynamicMesh3 conversion helper. */
static bool StaticMeshToDynamicMesh(
	const UStaticMesh* InStaticMesh,
	UE::Geometry::FDynamicMesh3& OutDynamicMesh)
{
	if (!InStaticMesh)
	{
		return false;
	}

	const FMeshDescription* SourceMesh = InStaticMesh->GetMeshDescription(0);
	if (!SourceMesh)
	{
		return false;
	}

	FMeshDescriptionToDynamicMesh Converter;
	Converter.Convert(SourceMesh, OutDynamicMesh, /*bCopyTangents=*/ true);
	return true;
}

/** Skeletal mesh → DynamicMesh3 conversion helper. */
static bool SkeletalMeshToDynamicMesh(
	const USkeletalMesh* InSkeletalMesh,
	int32 LodIndex,
	UE::Geometry::FDynamicMesh3& OutDynamicMesh)
{
	if (!InSkeletalMesh)
	{
		return false;
	}

	const FMeshDescription* SourceMesh = InSkeletalMesh->GetMeshDescription(LodIndex);
	if (!SourceMesh)
	{
		return false;
	}

	FMeshDescriptionToDynamicMesh Converter;
	Converter.Convert(SourceMesh, OutDynamicMesh, /*bCopyTangents=*/ true);
	return true;
}

/** Get list of used foliage mesh paths from collection. */
TArray<FString> GetUsedFoliage(const PV::Facades::FFoliageFacade& FoliageFacade)
{
	TArray<FString> UsedFoliage;
	const int32 NumNames = FoliageFacade.NumFoliageNames();
	for (int32 i = 0; i < NumNames; ++i)
	{
		const FString FoliageName = FoliageFacade.GetFoliageName(i);
		if (!FoliageName.IsEmpty())
		{
			UsedFoliage.AddUnique(FoliageName);
		}
	}
	return UsedFoliage;
}

/**
 * Combine all foliage instances in the collection into the provided DynamicMesh.
 * Materials from foliage meshes are appended to the Materials array.
 * OnMeshInstanceCombined is called for each instance with vertex range info.
 */
bool CombineMeshInstancesToDynamicMesh(
	const FManagedArrayCollection& Collection,
	UDynamicMesh* DynamicMesh,
	TArray<TObjectPtr<UMaterialInterface>>& Materials,
	const FMeshInstanceCombinedCallback& OnMeshInstanceCombined,
	const TFunction<bool(float)>& OnProgressUpdated)
{
	using namespace PV::Facades;
	using namespace UE::Geometry;

	FFoliageFacade FoliageFacade(Collection);
	if (!FoliageFacade.IsValid())
	{
		return true; // No foliage — not an error
	}

	const TArray<FString> UsedFoliage = GetUsedFoliage(FoliageFacade);
	if (UsedFoliage.IsEmpty())
	{
		return true;
	}

	// Phase 1: Convert each foliage type to a DynamicMesh3 template
	struct FMeshTemplate
	{
		FDynamicMesh3 Mesh;
		TArray<TObjectPtr<UMaterialInterface>> LocalMaterials;
		bool bValid = false;
	};

	TArray<FMeshTemplate> MeshTemplates;
	MeshTemplates.SetNum(UsedFoliage.Num());

	ParallelFor(UsedFoliage.Num(), [&](int32 Idx)
	{
		const FString& FoliageName = UsedFoliage[Idx];
		FMeshTemplate& Template = MeshTemplates[Idx];

		// Try static mesh first
		UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, *FoliageName);
		if (SM)
		{
			Template.bValid = StaticMeshToDynamicMesh(SM, Template.Mesh);
			for (const FStaticMaterial& Mat : SM->GetStaticMaterials())
			{
				Template.LocalMaterials.Add(Mat.MaterialInterface);
			}
			return;
		}

		// Fall back to skeletal mesh
		USkeletalMesh* SKM = LoadObject<USkeletalMesh>(nullptr, *FoliageName);
		if (SKM)
		{
			Template.bValid = SkeletalMeshToDynamicMesh(SKM, 0, Template.Mesh);
			// Remove bone attributes from skeletal mesh — we only want geometry
			if (Template.bValid && Template.Mesh.Attributes())
			{
				Template.Mesh.Attributes()->RemoveAttribute(TEXT("BoneIndex"));
				Template.Mesh.Attributes()->RemoveAttribute(TEXT("BoneParentIndex"));
			}
			for (const FSkeletalMaterial& Mat : SKM->GetMaterials())
			{
				Template.LocalMaterials.Add(Mat.MaterialInterface);
			}
		}
	});

	// Phase 2: Remap material IDs — accumulate all materials into a global list
	const int32 ExistingMaterialCount = Materials.Num();

	// Map from template index to material ID offset in the global list
	TArray<int32> TemplateMaterialOffset;
	TemplateMaterialOffset.SetNum(UsedFoliage.Num());

	for (int32 Idx = 0; Idx < UsedFoliage.Num(); ++Idx)
	{
		TemplateMaterialOffset[Idx] = Materials.Num();
		for (const TObjectPtr<UMaterialInterface>& Mat : MeshTemplates[Idx].LocalMaterials)
		{
			int32 ExistingIdx = Materials.Find(Mat);
			if (ExistingIdx == INDEX_NONE)
			{
				Materials.Add(Mat);
			}
		}
	}

	// Build a per-template material remap: local ID → global ID
	TArray<TArray<int32>> PerTemplateMaterialRemap;
	PerTemplateMaterialRemap.SetNum(UsedFoliage.Num());

	for (int32 Idx = 0; Idx < UsedFoliage.Num(); ++Idx)
	{
		const FMeshTemplate& Template = MeshTemplates[Idx];
		TArray<int32>& Remap = PerTemplateMaterialRemap[Idx];
		Remap.SetNum(Template.LocalMaterials.Num());

		for (int32 LocalIdx = 0; LocalIdx < Template.LocalMaterials.Num(); ++LocalIdx)
		{
			Remap[LocalIdx] = Materials.Find(Template.LocalMaterials[LocalIdx]);
		}
	}

	// Phase 3: Generate instances and append to DynamicMesh
	FDynamicMesh3& CombinedMesh = DynamicMesh->GetMeshRef();
	const int32 NumInstances = FoliageFacade.NumFoliageEntries();

	for (int32 InstIdx = 0; InstIdx < NumInstances; ++InstIdx)
	{
		// Find which template this instance uses
		const FFoliageEntryData EntryData = FoliageFacade.GetFoliageEntry(InstIdx);
		const FString InstanceFoliageName = FoliageFacade.GetFoliageName(EntryData.NameId);
		const int32 TemplateIdx = UsedFoliage.Find(InstanceFoliageName);
		if (TemplateIdx == INDEX_NONE || !MeshTemplates[TemplateIdx].bValid)
		{
			continue;
		}

		// Clone the template mesh
		FDynamicMesh3 InstanceMesh(MeshTemplates[TemplateIdx].Mesh);

		// Apply foliage transform
		const FTransform Transform = FoliageFacade.GetFoliageTransform(InstIdx);
		const FTransformSRT3d GeoTransform(Transform);
		MeshTransforms::ApplyTransform(InstanceMesh, GeoTransform, true);

		// Remap material IDs
		const TArray<int32>& Remap = PerTemplateMaterialRemap[TemplateIdx];
		if (Remap.Num() > 0)
		{
			FDynamicMeshMaterialAttribute* MaterialIDs = InstanceMesh.Attributes()->GetMaterialID();
			if (MaterialIDs)
			{
				for (int32 Tid : InstanceMesh.TriangleIndicesItr())
				{
					int32 OldMatID = 0;
					MaterialIDs->GetValue(Tid, &OldMatID);
					if (Remap.IsValidIndex(OldMatID))
					{
						MaterialIDs->SetValue(Tid, Remap[OldMatID]);
					}
				}
			}
		}

		// Record vertex count before append
		const int32 VertexStart = CombinedMesh.VertexCount();

		// Append
		CombinedMesh.AppendWithOffsets(InstanceMesh);

		const int32 NewVertexCount = CombinedMesh.VertexCount() - VertexStart;

		if (OnMeshInstanceCombined)
		{
			OnMeshInstanceCombined(InstanceFoliageName, InstIdx, VertexStart, NewVertexCount);
		}

		// Progress
		if (OnProgressUpdated)
		{
			const float Progress = (float)(InstIdx + 1) / (float)NumInstances;
			if (!OnProgressUpdated(Progress))
			{
				return false; // Cancelled
			}
		}
	}

	return true;
}

} // anonymous namespace

// =============================================================================
//  GeometryScript copy options
// =============================================================================

namespace
{

FGeometryScriptCopyMeshToAssetOptions GetCopyMeshToAssetOptions(
	const TArray<TObjectPtr<UMaterialInterface>>& InMaterials,
	const ENaniteShapePreservation InShapePreservation)
{
	FGeometryScriptCopyMeshToAssetOptions Options;
	Options.bEmitTransaction = false;
	Options.bDeferMeshPostEditChange = true;
	Options.bEnableRecomputeTangents = false;
	Options.bReplaceMaterials = true;
	Options.NewMaterials = InMaterials;
	Options.bApplyNaniteSettings = true;
	Options.NewNaniteSettings.bEnabled = true;
	Options.NewNaniteSettings.ShapePreservation = InShapePreservation;
	return Options;
}

} // anonymous namespace

// =============================================================================
//  Static mesh collision helper
// =============================================================================

namespace
{

void AddCollisionToStaticMesh(
	const FManagedArrayCollection& Collection,
	TObjectPtr<UStaticMesh> ExportMesh)
{
	if (!ExportMesh)
	{
		return;
	}

	UBodySetup* BodySetup = ExportMesh->GetBodySetup();
	if (!BodySetup)
	{
		ExportMesh->CreateBodySetup();
		BodySetup = ExportMesh->GetBodySetup();
	}

	if (!BodySetup)
	{
		return;
	}

	// Extract vertices from the geometry collection
	static const FName VertexGroupName("Vertices");
	static const FName VertexAttrName("Vertex");
	static const FName TransformGroupName = FGeometryCollection::TransformGroup;

	if (!Collection.HasGroup(VertexGroupName))
	{
		return;
	}

	const TManagedArray<FVector3f>& Vertices =
		Collection.GetAttribute<FVector3f>(VertexAttrName, VertexGroupName);

	TArray<FVector> ConvexVertices;
	ConvexVertices.Reserve(Vertices.Num());
	for (const FVector3f& V : Vertices)
	{
		ConvexVertices.Add(FVector(V));
	}

	if (ConvexVertices.IsEmpty())
	{
		return;
	}

	FKConvexElem ConvexElem;
	ConvexElem.VertexData = MoveTemp(ConvexVertices);
	ConvexElem.UpdateElemBox();

	BodySetup->AggGeom.ConvexElems.Add(MoveTemp(ConvexElem));
	BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	BodySetup->CreatePhysicsMeshes();
}

} // anonymous namespace

// =============================================================================
//  ExportCollectionToStaticMesh
// =============================================================================

namespace
{

bool ExportCollectionToStaticMesh(
	TObjectPtr<UStaticMesh> ExportMesh,
	const FManagedArrayCollection& Collection,
	ENaniteShapePreservation InShapePreservation,
	bool bShouldExportFoliage,
	bool bCollision,
	TArray<UPackage*>& OutModifiedPackages,
	const FStatusReportCallback& StatusReportCallback)
{
	const EPVExportMeshType MeshType = EPVExportMeshType::StaticMesh;
	const bool bNanite = false; // Nanite assemblies handled separately if needed

	// --- Stage: InitializingExportMesh ---
	if (!ReportProgress(StatusReportCallback, EExportMeshStage::InitializingExportMesh,
			0.0f, MeshType, bNanite, bShouldExportFoliage))
	{
		return false;
	}

	FManagedArrayCollection ExportedCollection;
	Collection.CopyTo(&ExportedCollection);

	TObjectPtr<UDynamicMesh> DynamicMesh = CollectionToDynamicMesh(ExportedCollection);
	if (!DynamicMesh || DynamicMesh->GetMeshRef().TriangleCount() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("PGL Export: Failed to convert collection to dynamic mesh."));
		return false;
	}

	// Collision (static mesh only)
	if (bCollision)
	{
		AddCollisionToStaticMesh(ExportedCollection, ExportMesh);
	}

	// Get materials
	TArray<TObjectPtr<UMaterialInterface>> Materials = GetMaterialsFromCollection(ExportedCollection);

	if (!ReportProgress(StatusReportCallback, EExportMeshStage::InitializingExportMesh,
			1.0f, MeshType, bNanite, bShouldExportFoliage))
	{
		return false;
	}

	// --- Stage: CombiningMeshes ---
	if (bShouldExportFoliage)
	{
		if (!ReportProgress(StatusReportCallback, EExportMeshStage::CombiningMeshes,
				0.0f, MeshType, bNanite, bShouldExportFoliage))
		{
			return false;
		}

		const bool bCombineResult = CombineMeshInstancesToDynamicMesh(
			ExportedCollection,
			DynamicMesh,
			Materials,
			/*OnMeshInstanceCombined=*/ nullptr,
			[&](float Progress) -> bool
			{
				return ReportProgress(StatusReportCallback, EExportMeshStage::CombiningMeshes,
					Progress, MeshType, bNanite, bShouldExportFoliage);
			});

		if (!bCombineResult)
		{
			return false;
		}
	}

	// --- Stage: CopyingMeshDataToOutputMesh ---
	if (!ReportProgress(StatusReportCallback, EExportMeshStage::CopyingMeshDataToOutputMesh,
			0.0f, MeshType, bNanite, bShouldExportFoliage))
	{
		return false;
	}

	FGeometryScriptCopyMeshToAssetOptions Options = GetCopyMeshToAssetOptions(Materials, InShapePreservation);

	EGeometryScriptOutcomePins Output;
	UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(
		DynamicMesh, ExportMesh, Options, /*LODInfo=*/ {}, Output);

	if (Output != EGeometryScriptOutcomePins::Success)
	{
		UE_LOG(LogTemp, Error, TEXT("PGL Export: CopyMeshToStaticMesh failed."));
		return false;
	}

	if (!ReportProgress(StatusReportCallback, EExportMeshStage::CopyingMeshDataToOutputMesh,
			1.0f, MeshType, bNanite, bShouldExportFoliage))
	{
		return false;
	}

	// --- Stage: BuildingMesh ---
	if (!ReportProgress(StatusReportCallback, EExportMeshStage::BuildingMesh,
			0.0f, MeshType, bNanite, bShouldExportFoliage))
	{
		return false;
	}

	ExportMesh->Build(/*bSilent=*/ true);
	FStaticMeshCompilingManager::Get().FinishAllCompilation();

	if (!ReportProgress(StatusReportCallback, EExportMeshStage::BuildingMesh,
			1.0f, MeshType, bNanite, bShouldExportFoliage))
	{
		return false;
	}

	// --- Stage: Complete ---
	ReportProgress(StatusReportCallback, EExportMeshStage::Complete,
		1.0f, MeshType, bNanite, bShouldExportFoliage);

	return true;
}

} // anonymous namespace

// =============================================================================
//  ExportCollectionToSkeletalMesh
// =============================================================================

namespace
{

bool ExportCollectionToSkeletalMesh(
	TObjectPtr<USkeletalMesh> ExportMesh,
	const FManagedArrayCollection& Collection,
	ENaniteShapePreservation InShapePreservation,
	TObjectPtr<const UPVWindSettings> InWindSettings,
	TObjectPtr<UPhysicsAsset> InPhysicsAsset,
	bool bShouldExportFoliage,
	EPVCollisionGeneration CollisionGeneration,
	TArray<UPackage*>& OutModifiedPackages,
	const FStatusReportCallback& StatusReportCallback)
{
	using namespace PV::Facades;

	const EPVExportMeshType MeshType = EPVExportMeshType::SkeletalMesh;
	const bool bNanite = false; // Nanite assemblies deferred to Phase 6

	// --- Stage: InitializingExportMesh ---
	if (!ReportProgress(StatusReportCallback, EExportMeshStage::InitializingExportMesh,
			0.0f, MeshType, bNanite, bShouldExportFoliage))
	{
		return false;
	}

	FManagedArrayCollection ExportedCollection;
	Collection.CopyTo(&ExportedCollection);

	TObjectPtr<UDynamicMesh> DynamicMesh = CollectionToDynamicMesh(ExportedCollection);
	if (!DynamicMesh || DynamicMesh->GetMeshRef().TriangleCount() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("PGL Export: Failed to convert collection to dynamic mesh."));
		return false;
	}

	TArray<TObjectPtr<UMaterialInterface>> Materials = GetMaterialsFromCollection(ExportedCollection);

	// --- Bone creation ---
	// Note: must use PV::Facades::FBoneNode to avoid collision with ::FBoneNode from Animation/Skeleton.h
	using FPVBoneNode = PV::Facades::FBoneNode;

	FBoneFacade BoneFacade(ExportedCollection);
	TArray<FPVBoneNode> BoneNodes;

	if (BoneFacade.IsValid())
	{
		BoneNodes = BoneFacade.GetBoneDataFromCollection();
	}

	if (BoneNodes.IsEmpty())
	{
		// Create bone data from scratch (full density)
		BoneFacade.CreateBoneData(BoneNodes, 0.0f);
	}

	if (BoneNodes.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("PGL Export: No bone data available for skeletal mesh."));
		return false;
	}

	// Assign bone IDs to foliage instances
	if (bShouldExportFoliage)
	{
		FFoliageFacade FoliageFacadeForBones(ExportedCollection);
		if (FoliageFacadeForBones.IsValid())
		{
			// Simplified assignment: assign each foliage instance to nearest bone on its branch
			FPointFacade PointFacade(ExportedCollection);
			for (int32 Id = 0; Id < FoliageFacadeForBones.NumFoliageEntries(); ++Id)
			{
				const auto EntryData = FoliageFacadeForBones.GetFoliageEntry(Id);
				const int32 BranchId = EntryData.BranchId;
				const float FoliageLFR = EntryData.LengthFromRoot;

				// Find best bone on this branch
				int32 BestBoneIdx = 0; // Default to root
				for (int32 BoneIdx = 0; BoneIdx < BoneNodes.Num(); ++BoneIdx)
				{
					if (BoneNodes[BoneIdx].BranchIndex == BranchId)
					{
						BestBoneIdx = BoneIdx;
						break; // Simplified: take first bone on branch
					}
				}

				FoliageFacadeForBones.SetParentBoneID(Id, BestBoneIdx);
			}
		}
	}

	// Enable bones on dynamic mesh
	UE::Geometry::FDynamicMeshAttributeSet* MeshAttributes =
		DynamicMesh->GetMeshPtr()->Attributes();
	check(MeshAttributes);

	const int32 BoneCount = BoneNodes.Num();
	MeshAttributes->EnableBones(BoneCount);

	auto* BoneNames = MeshAttributes->GetBoneNames();
	auto* BonePoses = MeshAttributes->GetBonePoses();
	auto* BoneParentIndices = MeshAttributes->GetBoneParentIndices();

	for (int32 i = 0; i < BoneCount; ++i)
	{
		BoneNames->SetValue(i, BoneNodes[i].BoneName);
		BonePoses->SetValue(i, BoneNodes[i].BoneTransform);
		BoneParentIndices->SetValue(i, BoneNodes[i].ParentBoneIndex);
	}

	// Get vertex bone IDs
	TArray<int32> VertexBoneIDs;
	BoneFacade.GetPointIds(VertexBoneIDs);

	if (!ReportProgress(StatusReportCallback, EExportMeshStage::InitializingExportMesh,
			1.0f, MeshType, bNanite, bShouldExportFoliage))
	{
		return false;
	}

	// --- Stage: CombiningMeshes ---
	int32 FoliageVertexStart = DynamicMesh->GetMeshRef().VertexCount();
	if (bShouldExportFoliage)
	{
		if (!ReportProgress(StatusReportCallback, EExportMeshStage::CombiningMeshes,
				0.0f, MeshType, bNanite, bShouldExportFoliage))
		{
			return false;
		}

		FFoliageFacade FoliageFacade(ExportedCollection);

		const bool bCombineResult = CombineMeshInstancesToDynamicMesh(
			ExportedCollection,
			DynamicMesh,
			Materials,
			[&](const FString& MeshPath, int32 InstanceID, int32 VertexStart, int32 VertexCount)
			{
				// Assign parent bone ID for foliage vertices
				const int32 ParentBoneID = FoliageFacade.GetParentBoneID(InstanceID);
				for (int32 v = VertexStart; v < VertexStart + VertexCount; ++v)
				{
					VertexBoneIDs.SetNum(FMath::Max(VertexBoneIDs.Num(), v + 1));
					VertexBoneIDs[v] = ParentBoneID;
				}
			},
			[&](float Progress) -> bool
			{
				return ReportProgress(StatusReportCallback, EExportMeshStage::CombiningMeshes,
					Progress, MeshType, bNanite, bShouldExportFoliage);
			});

		if (!bCombineResult)
		{
			return false;
		}
	}

	// --- Stage: CopyingMeshDataToOutputMesh ---
	if (!ReportProgress(StatusReportCallback, EExportMeshStage::CopyingMeshDataToOutputMesh,
			0.0f, MeshType, bNanite, bShouldExportFoliage))
	{
		return false;
	}

	FGeometryScriptCopyMeshToAssetOptions Options = GetCopyMeshToAssetOptions(Materials, InShapePreservation);

	EGeometryScriptOutcomePins Output;
	UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToSkeletalMesh(
		DynamicMesh, ExportMesh, Options, /*LODInfo=*/ {}, Output);

	if (Output != EGeometryScriptOutcomePins::Success)
	{
		UE_LOG(LogTemp, Error, TEXT("PGL Export: CopyMeshToSkeletalMesh failed."));
		return false;
	}

	if (!ReportProgress(StatusReportCallback, EExportMeshStage::CopyingMeshDataToOutputMesh,
			1.0f, MeshType, bNanite, bShouldExportFoliage))
	{
		return false;
	}

	// --- Stage: RecreatingBoneTree ---
	if (!ReportProgress(StatusReportCallback, EExportMeshStage::RecreatingBoneTree,
			0.0f, MeshType, bNanite, bShouldExportFoliage))
	{
		return false;
	}

	USkeleton* Skeleton = ExportMesh->GetSkeleton();
	if (Skeleton)
	{
		Skeleton->RecreateBoneTree(ExportMesh);
	}

	if (!ReportProgress(StatusReportCallback, EExportMeshStage::RecreatingBoneTree,
			1.0f, MeshType, bNanite, bShouldExportFoliage))
	{
		return false;
	}

	// --- Stage: ComputingBoneWeights ---
	if (!ReportProgress(StatusReportCallback, EExportMeshStage::ComputingBoneWeights,
			0.0f, MeshType, bNanite, bShouldExportFoliage))
	{
		return false;
	}

	{
		FGeometryScriptBoneWeightProfile SkinProfile;
		FGeometryScriptSmoothBoneWeightsOptions SmoothOptions;
		SmoothOptions.DistanceWeighingType = EGeometryScriptSmoothBoneWeightsType::DirectDistance;
		SmoothOptions.MaxInfluences = 3;
		SmoothOptions.Stiffness = 0.5f;

		UGeometryScriptLibrary_MeshBoneWeightFunctions::ComputeSmoothBoneWeights(
			DynamicMesh, Skeleton, SmoothOptions, SkinProfile);
	}

	if (!ReportProgress(StatusReportCallback, EExportMeshStage::ComputingBoneWeights,
			1.0f, MeshType, bNanite, bShouldExportFoliage))
	{
		return false;
	}

	// --- Stage: CopyingSkinWeightProfiles ---
	if (!ReportProgress(StatusReportCallback, EExportMeshStage::CopyingSkinWeightProfiles,
			0.0f, MeshType, bNanite, bShouldExportFoliage))
	{
		return false;
	}

	{
		const FName ProfileName(TEXT("Default"));
		FGeometryScriptCopySkinWeightProfileToAssetOptions SkinWeightOptions;
		SkinWeightOptions.bOverwriteExistingProfile = true;
		SkinWeightOptions.bEmitTransaction = false;
		SkinWeightOptions.bDeferMeshPostEditChange = true;
		UGeometryScriptLibrary_StaticMeshFunctions::CopySkinWeightProfileToSkeletalMesh(
			DynamicMesh, ExportMesh, ProfileName, ProfileName, SkinWeightOptions, /*LODInfo=*/ {}, Output);
	}

	if (!ReportProgress(StatusReportCallback, EExportMeshStage::CopyingSkinWeightProfiles,
			1.0f, MeshType, bNanite, bShouldExportFoliage))
	{
		return false;
	}

	// --- Stage: BuildingMesh ---
	if (!ReportProgress(StatusReportCallback, EExportMeshStage::BuildingMesh,
			0.0f, MeshType, bNanite, bShouldExportFoliage))
	{
		return false;
	}

	ExportMesh->Build();
	FSkinnedAssetCompilingManager::Get().FinishAllCompilation();

	if (!ReportProgress(StatusReportCallback, EExportMeshStage::BuildingMesh,
			1.0f, MeshType, bNanite, bShouldExportFoliage))
	{
		return false;
	}

	// --- Stage: Complete ---
	ReportProgress(StatusReportCallback, EExportMeshStage::Complete,
		1.0f, MeshType, bNanite, bShouldExportFoliage);

	return true;
}

} // anonymous namespace

// =============================================================================
//  Public API: ExportCollectionAsMesh
// =============================================================================

EExportResult PGL::Export::ExportCollectionAsMesh(
	const FManagedArrayCollection& Collection,
	const FPVExportParams& ExportParams,
	TArray<FString>& OutCreatedAssets,
	const FStatusReportCallback& StatusReportCallback)
{
	// Validate
	FString ValidationError;
	if (!ExportParams.Validate(ValidationError))
	{
		UE_LOG(LogTemp, Error, TEXT("PGL Export: Validation failed — %s"), *ValidationError);
		return EExportResult::Fail;
	}

	// Resolve asset path (handles Append policy)
	const FString MeshPackagePath = GetValidAssetPath(ExportParams);

	// Check Ignore policy
	if (ExportParams.ReplacePolicy == EPVAssetReplacePolicy::Ignore)
	{
		if (FPackageName::DoesPackageExist(MeshPackagePath))
		{
			UE_LOG(LogTemp, Log, TEXT("PGL Export: Asset already exists at '%s' — skipping (Ignore policy)."), *MeshPackagePath);
			return EExportResult::Skipped;
		}
	}

	// --- Create / find packages ---
	UPackage* MeshPackage = CreateAssetPackage(MeshPackagePath);
	if (!MeshPackage)
	{
		UE_LOG(LogTemp, Error, TEXT("PGL Export: Failed to create package at '%s'."), *MeshPackagePath);
		return EExportResult::Fail;
	}

	const FString MeshAssetName = FPackageName::GetShortName(MeshPackagePath);
	TArray<UPackage*> ModifiedPackages;
	ModifiedPackages.Add(MeshPackage);

	bool bSuccess = false;

	if (ExportParams.ExportMeshType == EPVExportMeshType::StaticMesh)
	{
		// --- Static mesh export ---
		UStaticMesh* ExportMesh = FindObject<UStaticMesh>(MeshPackage, *MeshAssetName);
		if (!ExportMesh)
		{
			ExportMesh = NewObject<UStaticMesh>(MeshPackage, *MeshAssetName,
				RF_Public | RF_Standalone | RF_Transactional);
		}

		bSuccess = ExportCollectionToStaticMesh(
			ExportMesh,
			Collection,
			ExportParams.NaniteShapePreservation,
			ExportParams.bShouldExportFoliage,
			ExportParams.bCollision,
			ModifiedPackages,
			StatusReportCallback);

		if (bSuccess)
		{
			OutCreatedAssets.Add(ExportMesh->GetPathName());
		}
	}
	else
	{
		// --- Skeletal mesh export ---

		// Create skeleton package
		const FString SkeletonPackagePath = ExportParams.GetOutputSkeletonPackagePath();
		UPackage* SkeletonPackage = CreateAssetPackage(SkeletonPackagePath);
		const FString SkeletonAssetName = FPackageName::GetShortName(SkeletonPackagePath);

		USkeleton* Skeleton = FindObject<USkeleton>(SkeletonPackage, *SkeletonAssetName);
		if (!Skeleton)
		{
			Skeleton = NewObject<USkeleton>(SkeletonPackage, *SkeletonAssetName,
				RF_Public | RF_Standalone | RF_Transactional);
		}
		ModifiedPackages.Add(SkeletonPackage);

		// Create skeletal mesh
		USkeletalMesh* ExportMesh = FindObject<USkeletalMesh>(MeshPackage, *MeshAssetName);
		if (!ExportMesh)
		{
			ExportMesh = NewObject<USkeletalMesh>(MeshPackage, *MeshAssetName,
				RF_Public | RF_Standalone | RF_Transactional);
		}
		ExportMesh->SetSkeleton(Skeleton);

		// Physics asset (optional)
		TObjectPtr<UPhysicsAsset> PhysicsAsset;
		if (ExportParams.IsCollisionEnable())
		{
			const FString PhysicsPackagePath = ExportParams.GetOutputPhysicsAssetPackagePath();
			UPackage* PhysicsPackage = CreateAssetPackage(PhysicsPackagePath);
			const FString PhysicsAssetName = FPackageName::GetShortName(PhysicsPackagePath);

			PhysicsAsset = FindObject<UPhysicsAsset>(PhysicsPackage, *PhysicsAssetName);
			if (!PhysicsAsset)
			{
				PhysicsAsset = NewObject<UPhysicsAsset>(PhysicsPackage, *PhysicsAssetName,
					RF_Public | RF_Standalone | RF_Transactional);
			}
			ExportMesh->SetPhysicsAsset(PhysicsAsset);
			ModifiedPackages.Add(PhysicsPackage);
		}

		bSuccess = ExportCollectionToSkeletalMesh(
			ExportMesh,
			Collection,
			ExportParams.NaniteShapePreservation,
			ExportParams.WindSettings,
			PhysicsAsset,
			ExportParams.bShouldExportFoliage,
			ExportParams.CollisionGeneration,
			ModifiedPackages,
			StatusReportCallback);

		if (bSuccess)
		{
			OutCreatedAssets.Add(ExportMesh->GetPathName());
			OutCreatedAssets.Add(Skeleton->GetPathName());
			if (PhysicsAsset)
			{
				OutCreatedAssets.Add(PhysicsAsset->GetPathName());
			}
		}
	}

	if (bSuccess)
	{
		// Mark packages dirty and register with asset registry
		for (UPackage* Package : ModifiedPackages)
		{
			if (Package)
			{
				Package->MarkPackageDirty();
			}
		}

		// Notify asset registry
		IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
		for (const FString& AssetPath : OutCreatedAssets)
		{
			UObject* Asset = FindObject<UObject>(nullptr, *AssetPath);
			if (Asset)
			{
				AssetRegistry.AssetCreated(Asset);
			}
		}

		UE_LOG(LogTemp, Log, TEXT("PGL Export: Successfully exported %d asset(s) to '%s'."),
			OutCreatedAssets.Num(), *MeshPackagePath);
		return EExportResult::Success;
	}

	UE_LOG(LogTemp, Error, TEXT("PGL Export: Export failed for '%s'."), *MeshPackagePath);
	return EExportResult::Fail;
}

#undef LOCTEXT_NAMESPACE
