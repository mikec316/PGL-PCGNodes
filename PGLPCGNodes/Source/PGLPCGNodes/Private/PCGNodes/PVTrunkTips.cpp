// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PVTrunkTips.h"

#include "DataTypes/PVGrowthData.h"
#include "DataTypes/PVMeshData.h"
#include "Facades/PVBranchFacade.h"
#include "Facades/PVPlantFacade.h"
#include "Facades/PVPointFacade.h"
#include "PCGContext.h"
#include "PCGPin.h"

// FGeometryCollection static helpers (CHAOS_API exported)
#include "GeometryCollection/GeometryCollection.h"
// FGeometryCollectionSection — material render sections
#include "GeometryCollection/GeometryCollectionSection.h"
// Inline UV helpers operating directly on FManagedArrayCollection
#include "GeometryCollection/Facades/CollectionUVFacade.h"

#define LOCTEXT_NAMESPACE "PVTrunkTipsSettings"

// ---------------------------------------------------------------------------
//  Pin label constant
// ---------------------------------------------------------------------------

const FName UPVTrunkTipsSettings::SkeletonPinLabel = FName(TEXT("Skeleton"));

// ---------------------------------------------------------------------------
//  Editor-only
// ---------------------------------------------------------------------------

#if WITH_EDITOR

FText UPVTrunkTipsSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Trunk Tips");
}

FText UPVTrunkTipsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Appends a flat disk cap at the trunk cut cross-section to seal the\n"
		"open hole left by the Cut Trunk node.\n\n"
		"Connect the Mesh Builder output to \"In\" and the same skeleton\n"
		"(Cut Trunk output) to \"Skeleton\".\n\n"
		"Press Ctrl + L to lock / unlock node output."
	);
}

#endif

// ---------------------------------------------------------------------------
//  Pins
// ---------------------------------------------------------------------------

TArray<FPCGPinProperties> UPVTrunkTipsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Props;

	// Primary: the already-built mesh from the standard Mesh Builder
	{
		FPCGPinProperties& Pin = Props.Emplace_GetRef(
			PCGPinConstants::DefaultInputLabel,
			FPCGDataTypeIdentifier{ FPVDataTypeInfoMesh::AsId() });
		Pin.SetRequiredPin();
		Pin.SetAllowMultipleConnections(false);
		Pin.bAllowMultipleData = false;
	}

	// Secondary: the skeleton to find the trunk tip
	{
		FPCGPinProperties& Pin = Props.Emplace_GetRef(
			SkeletonPinLabel,
			FPCGDataTypeIdentifier{ FPVDataTypeInfoGrowth::AsId() });
		Pin.SetRequiredPin();
		Pin.SetAllowMultipleConnections(false);
		Pin.bAllowMultipleData = false;
	}

	return Props;
}

TArray<FPCGPinProperties> UPVTrunkTipsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Props;
	[[maybe_unused]] FPCGPinProperties& Pin = Props.Emplace_GetRef(
		PCGPinConstants::DefaultOutputLabel,
		FPCGDataTypeIdentifier{ FPVDataTypeInfoMesh::AsId() });
	return Props;
}

FPCGElementPtr UPVTrunkTipsSettings::CreateElement() const
{
	return MakeShared<FPVTrunkTipsElement>();
}

// ---------------------------------------------------------------------------
//  Execution
// ---------------------------------------------------------------------------

bool FPVTrunkTipsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPVTrunkTipsElement::Execute);
	check(InContext);

	const UPVTrunkTipsSettings* Settings = InContext->GetInputSettings<UPVTrunkTipsSettings>();
	check(Settings);

	// -------------------------------------------------------------------------
	//  Validate inputs
	// -------------------------------------------------------------------------
	const TArray<FPCGTaggedData>& MeshInputs = InContext->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	const TArray<FPCGTaggedData>& SkelInputs  = InContext->InputData.GetInputsByPin(UPVTrunkTipsSettings::SkeletonPinLabel);

	if (MeshInputs.IsEmpty() || SkelInputs.IsEmpty())
	{
		return true;
	}

	const UPVMeshData*   MeshData = Cast<UPVMeshData>  (MeshInputs[0].Data);
	const UPVGrowthData* SkelData = Cast<UPVGrowthData>(SkelInputs[0].Data);

	if (!MeshData || !SkelData)
	{
		PCGLog::InputOutput::LogInvalidInputDataError(InContext);
		return true;
	}

	// -------------------------------------------------------------------------
	//  Deep-copy the mesh collection so we can modify it safely
	// -------------------------------------------------------------------------
	FManagedArrayCollection OutCollection = MeshData->GetCollection();

	// -------------------------------------------------------------------------
	//  Optionally append cap disks for each plant trunk
	//  bAddCap       = top cap  (CutLength end, normal points away from root)
	//  bAddBottomCap = bottom cap (TrunkStart end, normal points toward root)
	// -------------------------------------------------------------------------
	if (Settings->bAddCap || Settings->bAddBottomCap)
	{
		const FManagedArrayCollection& SkelCollection = SkelData->GetCollection();

		const PV::Facades::FPlantFacade  PlantFacade (SkelCollection);
		const PV::Facades::FBranchFacade BranchFacade(SkelCollection);
		const PV::Facades::FPointFacade  PointFacade (SkelCollection);

		if (PlantFacade.IsValid() && BranchFacade.IsValid() && PointFacade.IsValid())
		{
			const int32 N = FMath::Max(3, Settings->CapDivisions);

			// ------------------------------------------------------------------
			//  Shared helper: appends one flat disk cap to OutCollection.
			//
			//  CenterPos     — world-space position of the cap center vertex
			//  CapNormal     — outward-facing normal (determines face orientation)
			//  CapRadius     — radius of the disk
			//  bFlipWinding  — when true, triangle winding is reversed so the
			//                  front face matches a downward-pointing normal
			//                  (used for the bottom cap)
			// ------------------------------------------------------------------
			auto AppendCapDisk = [&](
				const FVector3f& CenterPos,
				const FVector3f& CapNormal,
				float            CapRadius,
				bool             bFlipWinding)
			{
				// Orthonormal tangent frame in the plane of the cap
				const FVector3f RefAxis  = (FMath::Abs(CapNormal.Z) < 0.99f)
					? FVector3f(0.f, 0.f, 1.f) : FVector3f(1.f, 0.f, 0.f);
				const FVector3f TangentX = FVector3f::CrossProduct(CapNormal, RefAxis).GetSafeNormal();
				const FVector3f TangentY = FVector3f::CrossProduct(CapNormal, TangentX);

				// Snapshot counts before adding new elements
				const int32 VBase = OutCollection.NumElements(FGeometryCollection::VerticesGroup);
				const int32 FBase = OutCollection.NumElements(FGeometryCollection::FacesGroup);

				OutCollection.AddElements(N + 1, FGeometryCollection::VerticesGroup);
				OutCollection.AddElements(N,     FGeometryCollection::FacesGroup);

				// ----------------------------------------------------------
				//  Vertex attributes
				// ----------------------------------------------------------
				TManagedArray<FVector3f>* Verts    = OutCollection.FindAttributeTyped<FVector3f>(FGeometryCollection::VertexPositionAttribute, FGeometryCollection::VerticesGroup);
				TManagedArray<FVector3f>* Norms    = OutCollection.FindAttributeTyped<FVector3f>(FGeometryCollection::VertexNormalAttribute,   FGeometryCollection::VerticesGroup);
				TManagedArray<int32>*     BoneMaps = OutCollection.FindAttributeTyped<int32>    (FGeometryCollection::VertexBoneMapAttribute,   FGeometryCollection::VerticesGroup);

				// Inherit the bone from vertex 0 (trunk root bone)
				const int32 TrunkBone = (BoneMaps && VBase > 0) ? (*BoneMaps)[0] : 0;

				// Center vertex
				if (Verts)    (*Verts)   [VBase] = CenterPos;
				if (Norms)    (*Norms)   [VBase] = CapNormal;
				if (BoneMaps) (*BoneMaps)[VBase] = TrunkBone;

				// Ring vertices
				for (int32 i = 0; i < N; ++i)
				{
					const float     Angle   = 2.f * PI * static_cast<float>(i) / static_cast<float>(N);
					const FVector3f RingPos = CenterPos + CapRadius * (FMath::Cos(Angle) * TangentX + FMath::Sin(Angle) * TangentY);
					const int32     vi      = VBase + 1 + i;
					if (Verts)    (*Verts)   [vi] = RingPos;
					if (Norms)    (*Norms)   [vi] = CapNormal;
					if (BoneMaps) (*BoneMaps)[vi] = TrunkBone;
				}

				// ----------------------------------------------------------
				//  UVs — circular planar layout (skipped if attribute absent)
				// ----------------------------------------------------------
				if (TManagedArray<FVector2f>* UVLayer = GeometryCollection::UV::FindUVLayer(OutCollection, 0))
				{
					(*UVLayer)[VBase] = FVector2f(0.5f, 0.5f);
					for (int32 i = 0; i < N; ++i)
					{
						const float Angle = 2.f * PI * static_cast<float>(i) / static_cast<float>(N);
						(*UVLayer)[VBase + 1 + i] = FVector2f(
							0.5f + 0.5f * FMath::Cos(Angle),
							0.5f + 0.5f * FMath::Sin(Angle));
					}
				}

				// ----------------------------------------------------------
				//  Vertex colour — mark cap vertices for material masking.
				//  Trunk vertices stay at (0,0,0,0); cap vertices receive
				//  CapVertexColor.  Attribute is created here if absent.
				// ----------------------------------------------------------
				{
					static const FName ColorAttr(TEXT("Color"));
					TManagedArray<FLinearColor>* VColors =
						OutCollection.FindAttributeTyped<FLinearColor>(
							ColorAttr, FGeometryCollection::VerticesGroup);
					if (!VColors)
					{
						TManagedArray<FLinearColor>& Added =
							OutCollection.AddAttribute<FLinearColor>(
								ColorAttr, FGeometryCollection::VerticesGroup);
						VColors = &Added;
					}
					if (VColors)
					{
						const FLinearColor C = Settings->CapVertexColor;
						(*VColors)[VBase] = C;
						for (int32 j = 0; j < N; ++j)
							(*VColors)[VBase + 1 + j] = C;
					}
				}

				// ----------------------------------------------------------
				//  Face attributes
				//  Top cap:    CCW winding seen from CapNormal direction
				//  Bottom cap: reversed winding (bFlipWinding = true) so the
				//              front face still points outward (away from chunk)
				// ----------------------------------------------------------
				{
					TManagedArray<FIntVector>* FaceIdx  = OutCollection.FindAttributeTyped<FIntVector>(FGeometryCollection::FaceIndicesAttribute,  FGeometryCollection::FacesGroup);
					TManagedArray<bool>*       FaceVis  = OutCollection.FindAttributeTyped<bool>      (FGeometryCollection::FaceVisibleAttribute,  FGeometryCollection::FacesGroup);
					TManagedArray<int32>*      FaceMatID= OutCollection.FindAttributeTyped<int32>     (FGeometryCollection::MaterialIDAttribute,    FGeometryCollection::FacesGroup);
					TManagedArray<int32>*      FaceMatIx= OutCollection.FindAttributeTyped<int32>     (FGeometryCollection::MaterialIndexAttribute, FGeometryCollection::FacesGroup);

					const int32 CapMatID = Settings->CapMaterialSlot;
					const int32 CapMatIx = Settings->CapMaterialSlot;

					for (int32 i = 0; i < N; ++i)
					{
						const int32 fi  = FBase + i;
						const int32 iA  = VBase + 1 + i;
						const int32 iB  = VBase + 1 + (i + 1) % N;
						// Standard:  (center, ring[i],   ring[i+1]) — CCW from CapNormal side
						// Flipped:   (center, ring[i+1], ring[i]  ) — CCW from -CapNormal side
						if (FaceIdx)   (*FaceIdx) [fi] = bFlipWinding
							? FIntVector(VBase, iB, iA)
							: FIntVector(VBase, iA, iB);
						if (FaceVis)   (*FaceVis) [fi] = true;
						if (FaceMatID) (*FaceMatID)[fi] = CapMatID;
						if (FaceMatIx) (*FaceMatIx)[fi] = CapMatIx;
					}
				}

				// ----------------------------------------------------------
				//  Geometry group — extend the piece whose face range ends at
				//  FBase to include the new cap faces and vertices.
				// ----------------------------------------------------------
				{
					const int32 NumGeoms = OutCollection.NumElements(FGeometryCollection::GeometryGroup);
					if (NumGeoms > 0)
					{
						TManagedArray<int32>* GFStart = OutCollection.FindAttributeTyped<int32>(FGeometryCollection::FaceStartAttribute,   FGeometryCollection::GeometryGroup);
						TManagedArray<int32>* GFCount = OutCollection.FindAttributeTyped<int32>(FGeometryCollection::FaceCountAttribute,   FGeometryCollection::GeometryGroup);
						TManagedArray<int32>* GVCount = OutCollection.FindAttributeTyped<int32>(FGeometryCollection::VertexCountAttribute, FGeometryCollection::GeometryGroup);

						if (GFStart && GFCount && GVCount)
						{
							int32 BestIdx   = 0;
							int32 BestDelta = INT_MAX;
							for (int32 gi = 0; gi < NumGeoms; ++gi)
							{
								const int32 Delta = FMath::Abs(FBase - ((*GFStart)[gi] + (*GFCount)[gi]));
								if (Delta < BestDelta) { BestDelta = Delta; BestIdx = gi; }
							}
							(*GFCount)[BestIdx] += N;
							(*GVCount)[BestIdx] += N + 1;
						}
					}
				}

				// ----------------------------------------------------------
				//  Material sections — extend or add to cover the cap faces.
				//  Do NOT call ReindexMaterials (it destroys PVE's sections).
				// ----------------------------------------------------------
				{
					static const FName SectionsAttr(TEXT("Sections"));
					TManagedArray<FGeometryCollectionSection>* Sections =
						OutCollection.FindAttributeTyped<FGeometryCollectionSection>(
							SectionsAttr, FGeometryCollection::MaterialGroup);
					if (Sections && Sections->Num() > 0)
					{
						const int32 CapMatID = Settings->CapMaterialSlot;

						// Find the section immediately before the cap in the index buffer
						int32 BestSecIdx = 0;
						int32 BestDelta  = INT_MAX;
						for (int32 si = 0; si < Sections->Num(); ++si)
						{
							const int32 SectionEnd = (*Sections)[si].FirstIndex + (*Sections)[si].NumTriangles * 3;
							const int32 Delta = FMath::Abs(FBase * 3 - SectionEnd);
							if (Delta < BestDelta) { BestDelta = Delta; BestSecIdx = si; }
						}

						if ((*Sections)[BestSecIdx].MaterialID == CapMatID)
						{
							(*Sections)[BestSecIdx].NumTriangles  += N;
							(*Sections)[BestSecIdx].MaxVertexIndex =
								FMath::Max((*Sections)[BestSecIdx].MaxVertexIndex, VBase + N);
						}
						else
						{
							const int32 NewSecIdx = OutCollection.NumElements(FGeometryCollection::MaterialGroup);
							OutCollection.AddElements(1, FGeometryCollection::MaterialGroup);
							Sections = OutCollection.FindAttributeTyped<FGeometryCollectionSection>(
								SectionsAttr, FGeometryCollection::MaterialGroup);
							if (Sections)
							{
								FGeometryCollectionSection& S = (*Sections)[NewSecIdx];
								S.MaterialID     = CapMatID;
								S.FirstIndex     = FBase * 3;
								S.NumTriangles   = N;
								S.MinVertexIndex = VBase;
								S.MaxVertexIndex = VBase + N;
							}
						}
					}
				}
			}; // AppendCapDisk

			// ------------------------------------------------------------------
			//  Per-plant: find top and bottom trunk points, then call AppendCapDisk
			// ------------------------------------------------------------------
			const TMap<int32, int32> PlantToTrunk = PlantFacade.GetPlantNumbersToTrunkIndicesMap();

			for (const TPair<int32, int32>& Pair : PlantToTrunk)
			{
				const int32 TrunkIdx = Pair.Value;
				if (TrunkIdx == INDEX_NONE) continue;

				const TArray<int32>& TrunkPts = BranchFacade.GetPoints(TrunkIdx);
				if (TrunkPts.IsEmpty()) continue;

				// Find the tip (max LengthFromRoot) and its immediate predecessor,
				// and the base (min LengthFromRoot) and its immediate successor.
				int32 TipPtIdx    = INDEX_NONE, TipPredIdx  = INDEX_NONE;
				int32 BasePtIdx   = INDEX_NONE, BaseSuccIdx = INDEX_NONE;
				float MaxLen = -FLT_MAX, MaxLen2 = -FLT_MAX;
				float MinLen =  FLT_MAX, MinLen2 =  FLT_MAX;

				for (const int32 PtIdx : TrunkPts)
				{
					const float Len = PointFacade.GetLengthFromRoot(PtIdx);

					// Track top two by length (tip + its predecessor)
					if (Len > MaxLen)
					{
						TipPredIdx = TipPtIdx; MaxLen2 = MaxLen;
						TipPtIdx   = PtIdx;    MaxLen  = Len;
					}
					else if (Len > MaxLen2)
					{
						TipPredIdx = PtIdx;    MaxLen2 = Len;
					}

					// Track bottom two by length (base + its successor)
					if (Len < MinLen)
					{
						BaseSuccIdx = BasePtIdx; MinLen2 = MinLen;
						BasePtIdx   = PtIdx;     MinLen  = Len;
					}
					else if (Len < MinLen2)
					{
						BaseSuccIdx = PtIdx;     MinLen2 = Len;
					}
				}

				// ---- Top cap ------------------------------------------------
				if (Settings->bAddCap && TipPtIdx != INDEX_NONE)
				{
					const FVector3f TipPos    = PointFacade.GetPosition(TipPtIdx);
					const float     TipRadius = PointFacade.GetPointScale(TipPtIdx);

					// Normal = trunk growth direction at the top cut (pointing away from root)
					FVector3f TopNormal = FVector3f(0.f, 0.f, 1.f);
					if (TipPredIdx != INDEX_NONE)
					{
						const FVector3f Dir = (TipPos - PointFacade.GetPosition(TipPredIdx)).GetSafeNormal();
						if (!Dir.IsNearlyZero()) TopNormal = Dir;
					}

					AppendCapDisk(TipPos, TopNormal, TipRadius, /*bFlipWinding=*/false);
				}

				// ---- Bottom cap ---------------------------------------------
				if (Settings->bAddBottomCap && BasePtIdx != INDEX_NONE)
				{
					const FVector3f BasePos    = PointFacade.GetPosition(BasePtIdx);
					const float     BaseRadius = PointFacade.GetPointScale(BasePtIdx);

					// Normal = opposite of trunk growth direction at the bottom cut
					// (points toward root = outward from the bottom face of the chunk)
					FVector3f BottomNormal = FVector3f(0.f, 0.f, -1.f);
					if (BaseSuccIdx != INDEX_NONE)
					{
						const FVector3f Dir = (PointFacade.GetPosition(BaseSuccIdx) - BasePos).GetSafeNormal();
						if (!Dir.IsNearlyZero()) BottomNormal = -Dir; // negate: points toward root
					}

					AppendCapDisk(BasePos, BottomNormal, BaseRadius, /*bFlipWinding=*/true);
				}
			} // per-plant loop
		}
	}

	// -------------------------------------------------------------------------
	//  Emit output
	// -------------------------------------------------------------------------
	UPVMeshData* OutMeshData = FPCGContext::NewObject_AnyThread<UPVMeshData>(InContext);
	OutMeshData->Initialize(MoveTemp(OutCollection));

	FPCGTaggedData& Output = InContext->OutputData.TaggedData.Emplace_GetRef();
	Output.Data = OutMeshData;
	Output.Pin  = PCGPinConstants::DefaultOutputLabel;

	return true;
}

#undef LOCTEXT_NAMESPACE
