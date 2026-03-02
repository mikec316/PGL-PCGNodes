// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PVViewportMesh.h"

#include "PCGContext.h"
#include "PCGPin.h"

// PVE data + facades
#include "DataTypes/PVGrowthData.h"
#include "Facades/PVAttributesNames.h"
#include "Facades/PVBranchFacade.h"
#include "Facades/PVPointFacade.h"

#define LOCTEXT_NAMESPACE "PVViewportMeshSettings"

// =============================================================================
//  Editor metadata
// =============================================================================

#if WITH_EDITOR

FText UPVViewportMeshSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Viewport Mesh");
}

FText UPVViewportMeshSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Outputs a static mesh's bounding box as a wireframe skeleton\n"
		"(PVGrowthData).  Wire into a downstream PVE node to see the\n"
		"12-edge wireframe in the PVE viewport.\n\n"
		"Assign a mesh and set the transform to position the wireframe\n"
		"relative to the tree origin.\n\n"
		"The mesh reference is stored for future use by growth\n"
		"constraint nodes."
	);
}

#endif // WITH_EDITOR

// =============================================================================
//  Pins
// =============================================================================

TArray<FPCGPinProperties> UPVViewportMeshSettings::InputPinProperties() const
{
	// Source node — no inputs.
	return {};
}

FPCGElementPtr UPVViewportMeshSettings::CreateElement() const
{
	return MakeShared<FPVViewportMeshElement>();
}

// =============================================================================
//  Execution
// =============================================================================

bool FPVViewportMeshElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPVViewportMeshElement::Execute);
	check(Context);

	const UPVViewportMeshSettings* Settings =
		Context->GetInputSettings<UPVViewportMeshSettings>();
	check(Settings);

	// Early out if no mesh assigned
	if (Settings->Mesh.IsNull())
	{
		return true;
	}

	// Load mesh to read its bounding box
	UStaticMesh* LoadedMesh = Settings->Mesh.LoadSynchronous();
	if (!LoadedMesh)
	{
		PCGE_LOG(Warning, GraphAndLog,
			LOCTEXT("MeshLoadFailed", "Viewport Mesh: failed to load the assigned static mesh."));
		return true;
	}

	const FBox LocalBox = LoadedMesh->GetBoundingBox();

	// -------------------------------------------------------------------------
	//  Build 8 corners, apply MeshTransform
	// -------------------------------------------------------------------------
	const FTransform& Xform = Settings->MeshTransform;

	FVector3f Corners[8];
	{
		const FVector& Mn = LocalBox.Min;
		const FVector& Mx = LocalBox.Max;

		const FVector LocalCorners[8] = {
			{ Mn.X, Mn.Y, Mn.Z }, { Mx.X, Mn.Y, Mn.Z },
			{ Mx.X, Mx.Y, Mn.Z }, { Mn.X, Mx.Y, Mn.Z },
			{ Mn.X, Mn.Y, Mx.Z }, { Mx.X, Mn.Y, Mx.Z },
			{ Mx.X, Mx.Y, Mx.Z }, { Mn.X, Mx.Y, Mx.Z }
		};

		for (int32 i = 0; i < 8; ++i)
		{
			const FVector W = Xform.TransformPosition(LocalCorners[i]);
			Corners[i] = FVector3f((float)W.X, (float)W.Y, (float)W.Z);
		}
	}

	// 12 edges of a box
	static constexpr int32 Edges[12][2] = {
		{0, 1}, {1, 2}, {2, 3}, {3, 0},   // bottom face
		{4, 5}, {5, 6}, {6, 7}, {7, 4},   // top face
		{0, 4}, {1, 5}, {2, 6}, {3, 7}    // vertical edges
	};

	constexpr int32 TotalBranches = 12;
	constexpr int32 TotalPoints   = 24;  // 2 per edge

	// -------------------------------------------------------------------------
	//  Build FManagedArrayCollection (same schema as Growth Generator)
	// -------------------------------------------------------------------------
	namespace PVG = PV::GroupNames;
	namespace PVA = PV::AttributeNames;

	FManagedArrayCollection Collection;

	// --- Points group --- (all 16 required by FPointFacade::IsValid())
	Collection.AddGroup(PVG::PointGroup);
	Collection.AddElements(TotalPoints, PVG::PointGroup);

	TManagedArray<FVector3f>& PosArr    = Collection.AddAttribute<FVector3f>(PVA::PointPosition,      PVG::PointGroup);
	TManagedArray<float>&     LFRArr    = Collection.AddAttribute<float>    (PVA::LengthFromRoot,     PVG::PointGroup);
	TManagedArray<float>&     LFSArr    = Collection.AddAttribute<float>    (PVA::LengthFromSeed,     PVG::PointGroup);
	TManagedArray<float>&     ScaleA    = Collection.AddAttribute<float>    (PVA::PointScale,         PVG::PointGroup);
	TManagedArray<float>&     PSGradA   = Collection.AddAttribute<float>    (PVA::PointScaleGradient, PVG::PointGroup);
	TManagedArray<float>&     HullGA    = Collection.AddAttribute<float>    (PVA::HullGradient,       PVG::PointGroup);
	TManagedArray<float>&     TrunkGA   = Collection.AddAttribute<float>    (PVA::MainTrunkGradient,  PVG::PointGroup);
	TManagedArray<float>&     GroundGA  = Collection.AddAttribute<float>    (PVA::GroundGradient,     PVG::PointGroup);
	TManagedArray<float>&     BranchGA  = Collection.AddAttribute<float>    (PVA::BranchGradient,     PVG::PointGroup);
	TManagedArray<float>&     PlantGA   = Collection.AddAttribute<float>    (PVA::PlantGradient,      PVG::PointGroup);
	TManagedArray<int32>&     BudNumA   = Collection.AddAttribute<int32>    (PVA::BudNumber,          PVG::PointGroup);
	TManagedArray<float>&     NjordA    = Collection.AddAttribute<float>    (PVA::NjordPixelIndex,    PVG::PointGroup);

	TManagedArray<TArray<FVector3f>>& BudDirA   = Collection.AddAttribute<TArray<FVector3f>>(PVA::BudDirection,     PVG::PointGroup);
	TManagedArray<TArray<float>>&     BudHormA  = Collection.AddAttribute<TArray<float>>    (PVA::BudHormoneLevels, PVG::PointGroup);
	TManagedArray<TArray<float>>&     BudLightA = Collection.AddAttribute<TArray<float>>    (PVA::BudLightDetected, PVG::PointGroup);
	TManagedArray<TArray<int32>>&     BudDevA   = Collection.AddAttribute<TArray<int32>>    (PVA::BudDevelopment,   PVG::PointGroup);

	// UV attributes
	Collection.AddAttribute<float>    (PVA::TextureCoordV,       PVG::PointGroup);
	Collection.AddAttribute<float>    (PVA::TextureCoordUOffset, PVG::PointGroup);
	Collection.AddAttribute<FVector2f>(PVA::URange,              PVG::PointGroup);

	// --- Fill points (2 per edge) ---
	for (int32 ei = 0; ei < TotalBranches; ++ei)
	{
		for (int32 vi = 0; vi < 2; ++vi)
		{
			const int32 pi = ei * 2 + vi;

			PosArr  [pi] = Corners[Edges[ei][vi]];
			LFRArr  [pi] = 0.f;
			LFSArr  [pi] = 0.f;
			ScaleA  [pi] = 1.f;
			PSGradA [pi] = 0.f;
			HullGA  [pi] = 0.f;
			TrunkGA [pi] = 0.f;
			GroundGA[pi] = 0.f;
			BranchGA[pi] = static_cast<float>(vi);
			PlantGA [pi] = 0.f;
			BudNumA [pi] = 0;
			NjordA  [pi] = 0.f;

			BudDirA  [pi].SetNum(6);          // zero-initialised
			BudHormA [pi].Init(0.f, 5);
			BudLightA[pi].Init(0.f, 1);
			BudDevA  [pi].Init(0, 3);
		}
	}

	// --- Branches group --- (all 8 required by FBranchFacade::IsValid())
	Collection.AddGroup(PVG::BranchGroup);
	Collection.AddElements(TotalBranches, PVG::BranchGroup);

	TManagedArray<TArray<int32>>& BPtsA    = Collection.AddAttribute<TArray<int32>>(PVA::BranchPoints,          PVG::BranchGroup);
	TManagedArray<TArray<int32>>& ParentsA = Collection.AddAttribute<TArray<int32>>(PVA::BranchParents,         PVG::BranchGroup);
	/* ChildA   */ Collection.AddAttribute<TArray<int32>>(PVA::BranchChildren,        PVG::BranchGroup);
	TManagedArray<int32>&         BNumA    = Collection.AddAttribute<int32>        (PVA::BranchNumber,          PVG::BranchGroup);
	TManagedArray<int32>&         BHierA   = Collection.AddAttribute<int32>        (PVA::BranchHierarchyNumber, PVG::BranchGroup);
	TManagedArray<int32>&         BParNA   = Collection.AddAttribute<int32>        (PVA::BranchParentNumber,    PVG::BranchGroup);
	TManagedArray<int32>&         BPlantA  = Collection.AddAttribute<int32>        (PVA::PlantNumber,           PVG::BranchGroup);
	/* BSrcBud  */ Collection.AddAttribute<int32>        (PVA::BranchSourceBudNumber, PVG::BranchGroup);

	Collection.AddAttribute<int32>(PVA::BranchUVMaterial,           PVG::BranchGroup);
	Collection.AddAttribute<int32>(PVA::BranchSimulationGroupIndex, PVG::BranchGroup);

	for (int32 bi = 0; bi < TotalBranches; ++bi)
	{
		BPtsA [bi] = { bi * 2, bi * 2 + 1 };
		BNumA [bi] = bi + 1;   // 1-based
		BHierA[bi] = 0;
		BParNA[bi] = 0;        // trunk sentinel — standalone edges, no parent
		BPlantA[bi] = 0;
		// ParentsA[bi] and ChildA[bi] left default-empty (standalone edges)
	}

	// --- Details group ---
	Collection.AddGroup(PVG::DetailsGroup);
	Collection.AddElements(1, PVG::DetailsGroup);

	// Store the mesh asset path so downstream constraint nodes can load it
	TManagedArray<FString>& TrunkMatA =
		Collection.AddAttribute<FString>(PVA::TrunkMaterialPath, PVG::DetailsGroup);
	TrunkMatA[0] = Settings->Mesh.ToSoftObjectPath().ToString();

	// Frame mesh attributes — consumed by visualization for downstream nodes
	TManagedArray<FString>& FrameMeshPathA =
		Collection.AddAttribute<FString>(FName("FrameMeshPath"), PVG::DetailsGroup);
	FrameMeshPathA[0] = Settings->Mesh.ToSoftObjectPath().ToString();

	TManagedArray<FTransform>& FrameMeshTransformA =
		Collection.AddAttribute<FTransform>(FName("FrameMeshTransform"), PVG::DetailsGroup);
	FrameMeshTransformA[0] = Settings->MeshTransform;

	TManagedArray<TArray<FVector2f>>& URngA =
		Collection.AddAttribute<TArray<FVector2f>>(PVA::TrunkURange, PVG::DetailsGroup);
	URngA[0].Add(FVector2f(0.f, 1.f));

	TManagedArray<TArray<float>>& PhylloA =
		Collection.AddAttribute<TArray<float>>(PVA::LeafPhyllotaxy, PVG::DetailsGroup);
	PhylloA[0].Init(0.f, 8);

	Collection.AddAttribute<FString>(PVA::FoliagePath, PVG::DetailsGroup);

	// --- PlantProfiles group ---
	Collection.AddGroup(PVG::PlantProfilesGroup);
	Collection.AddElements(1, PVG::PlantProfilesGroup);
	TManagedArray<TArray<float>>& ProfileA =
		Collection.AddAttribute<TArray<float>>(PVA::ProfilePoints, PVG::PlantProfilesGroup);
	ProfileA[0].Init(1.0f, 101);

	// -------------------------------------------------------------------------
	//  Validate and emit
	// -------------------------------------------------------------------------
	{
		PV::Facades::FPointFacade  PF(Collection);
		PV::Facades::FBranchFacade BF(Collection);

		if (!PF.IsValid() || !BF.IsValid())
		{
			PCGE_LOG(Error, GraphAndLog,
				LOCTEXT("InvalidCollection",
					"Viewport Mesh: generated collection failed PVE validation. "
					"At least one required attribute is missing or has the wrong type."));
			return true;
		}
	}

	UPVGrowthData* Out = FPCGContext::NewObject_AnyThread<UPVGrowthData>(Context);
	Out->Initialize(MoveTemp(Collection));

	FPCGTaggedData& D = Context->OutputData.TaggedData.Emplace_GetRef();
	D.Data = Out;
	D.Pin  = PCGPinConstants::DefaultOutputLabel;

	return true;
}

#undef LOCTEXT_NAMESPACE
