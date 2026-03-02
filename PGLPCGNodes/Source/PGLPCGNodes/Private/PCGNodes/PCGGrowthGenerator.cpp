// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PCGGrowthGenerator.h"

#include "PCGContext.h"
#include "PCGPin.h"

// PVE data + facades
#include "DataTypes/PVGrowthData.h"
#include "Facades/PVAttributesNames.h"
#include "Facades/PVBranchFacade.h"
#include "Facades/PVFoliageFacade.h"
#include "Facades/PVPointFacade.h"

#define LOCTEXT_NAMESPACE "PCGGrowthGeneratorSettings"

// =============================================================================
//  Internal helpers (translation-unit local)
// =============================================================================

namespace
{

/** All data the generator needs about one branch in the skeleton. */
struct FBranchWork
{
	int32      BranchIdx;               // row index in the Primitives group
	int32      HierarchyLevel;          // 0 = trunk, 1 = 1st-order, 2 = 2nd-order
	int32      ParentBranchIdx;         // INDEX_NONE for the trunk
	float      LengthFromRootAtAttach;  // cumulative length from tree root to attachment point
	FVector3f  AttachPos;               // world position where this branch starts
	FVector3f  GrowDir;                 // unit growth direction
	float      Length;                  // branch length in cm
	float      BaseRadius;              // radius at attachment point
	float      TipRadius;              // radius at tip
	int32      GlobalPointStart;        // first row in the Points group for this branch
	int32      SegmentCount;            // number of segments (points = SegmentCount + 1)
	TArray<int32>    ChildBranchIndices;  // filled during BFS planning
	TArray<FVector3f> CurvedPoints;       // SegmentCount+1 world positions; filled in Phase 1.5
};

/** Sample a unit vector uniformly inside a cone of given half-angle around Axis. */
FVector3f VRandCone3f(FRandomStream& Rng, const FVector3f& Axis, float HalfAngleDeg)
{
	const FVector AxisD(Axis.X, Axis.Y, Axis.Z);
	const FVector Result = Rng.VRandCone(AxisD, FMath::DegreesToRadians(HalfAngleDeg));
	return FVector3f((float)Result.X, (float)Result.Y, (float)Result.Z);
}

/** Oriented bounding box reconstructed from 8 corner positions (Viewport Mesh layout). */
struct FOBB
{
	FVector3f Center;
	FVector3f DirX, DirY, DirZ;          // unit axes
	float     HalfExtX, HalfExtY, HalfExtZ;
	bool      bValid = false;

	/** Construct from 8 corner positions matching the Viewport Mesh edge table. */
	void InitFromCorners(const FVector3f Corners[8])
	{
		const FVector3f EdgeX = Corners[1] - Corners[0]; // bottom MinY edge along X
		const FVector3f EdgeY = Corners[3] - Corners[0]; // bottom MinX edge along Y
		const FVector3f EdgeZ = Corners[4] - Corners[0]; // vertical edge along Z

		HalfExtX = EdgeX.Length() * 0.5f;
		HalfExtY = EdgeY.Length() * 0.5f;
		HalfExtZ = EdgeZ.Length() * 0.5f;

		if (HalfExtX < KINDA_SMALL_NUMBER ||
			HalfExtY < KINDA_SMALL_NUMBER ||
			HalfExtZ < KINDA_SMALL_NUMBER)
		{
			bValid = false;
			return;
		}

		DirX = EdgeX / (HalfExtX * 2.f);
		DirY = EdgeY / (HalfExtY * 2.f);
		DirZ = EdgeZ / (HalfExtZ * 2.f);
		Center = Corners[0] + (EdgeX + EdgeY + EdgeZ) * 0.5f;
		bValid = true;
	}

	/**
	 * Signed distance from P to the OBB surface and the outward face normal.
	 * Returns: negative = inside, positive = outside.
	 */
	float SignedDistance(const FVector3f& P, FVector3f& OutNormal) const
	{
		const FVector3f Delta = P - Center;
		const float ProjX = FVector3f::DotProduct(Delta, DirX);
		const float ProjY = FVector3f::DotProduct(Delta, DirY);
		const float ProjZ = FVector3f::DotProduct(Delta, DirZ);

		const float DistX = FMath::Abs(ProjX) - HalfExtX;
		const float DistY = FMath::Abs(ProjY) - HalfExtY;
		const float DistZ = FMath::Abs(ProjZ) - HalfExtZ;

		if (DistX >= DistY && DistX >= DistZ)
		{
			OutNormal = DirX * FMath::Sign(ProjX);
			return DistX;
		}
		else if (DistY >= DistZ)
		{
			OutNormal = DirY * FMath::Sign(ProjY);
			return DistY;
		}
		else
		{
			OutNormal = DirZ * FMath::Sign(ProjZ);
			return DistZ;
		}
	}

	/** Project an interior point to the nearest face + margin outward. */
	FVector3f ProjectToSurface(const FVector3f& P, float Margin) const
	{
		FVector3f Normal;
		const float SD = SignedDistance(P, Normal);
		if (SD < 0.f)
		{
			return P + Normal * (-SD + Margin);
		}
		return P;
	}
};

} // namespace

// =============================================================================
//  Constructor — set up default curve keys
// =============================================================================

UPCGGrowthGeneratorSettings::UPCGGrowthGeneratorSettings()
{
	// Default trunk taper: linear (0,0) → (1,1) — identical to FMath::Lerp.
	FRichCurve* Curve = TrunkTaperCurve.GetRichCurve();
	Curve->AddKey(0.f, 0.f);
	Curve->AddKey(1.f, 1.f);
}

// =============================================================================
//  Editor metadata
// =============================================================================

#if WITH_EDITOR

FText UPCGGrowthGeneratorSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Growth Generator");
}

FText UPCGGrowthGeneratorSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Procedurally generates a UPVGrowthData skeleton (trunk + optional\n"
		"hierarchical sub-branches) from in-node parameters.\n\n"
		"Connect the output to PVE's Mesh Builder 'Skeleton' pin.\n"
		"Can also be chained through PVCutTrunk / PVScaleBranches / PVTrunkTips.\n\n"
		"Optional 'Frame Mesh' input: connect a Viewport Mesh node to constrain\n"
		"growth around the mesh bounding box.  Branches will avoid the mesh,\n"
		"sliding along its surface when they approach too closely.\n\n"
		"BranchGenerations = 0 → trunk only.\n"
		"BranchGenerations = 1 → trunk + 1st-order branches.\n"
		"BranchGenerations = 2 → trunk + 1st and 2nd-order branches."
	);
}

#endif // WITH_EDITOR

// =============================================================================
//  Pins
// =============================================================================

TArray<FPCGPinProperties> UPCGGrowthGeneratorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Props;
	[[maybe_unused]] FPCGPinProperties& Pin = Props.Emplace_GetRef(
		FName(TEXT("Frame Mesh")),
		FPCGDataTypeIdentifier{ FPVDataTypeInfoGrowth::AsId() });
	// Optional pin — do NOT call SetRequiredPin()
	Pin.SetAllowMultipleConnections(false);
	Pin.bAllowMultipleData = false;
	return Props;
}

FPCGElementPtr UPCGGrowthGeneratorSettings::CreateElement() const
{
	return MakeShared<FPCGGrowthGeneratorElement>();
}

// =============================================================================
//  Execution
// =============================================================================

bool FPCGGrowthGeneratorElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGrowthGeneratorElement::Execute);
	check(Context);

	const UPCGGrowthGeneratorSettings* Settings =
		Context->GetInputSettings<UPCGGrowthGeneratorSettings>();
	check(Settings);

	// Namespace aliases to keep attribute/group name access concise
	namespace PVG = PV::GroupNames;
	namespace PVA = PV::AttributeNames;

	// -------------------------------------------------------------------------
	//  Read optional Frame Mesh input → extract OBB for growth avoidance
	// -------------------------------------------------------------------------
	FOBB FrameOBB;
	FString CapturedFrameMeshPath;
	FTransform CapturedFrameMeshTransform = FTransform::Identity;
	{
		const TArray<FPCGTaggedData> FrameInputs =
			Context->InputData.GetInputsByPin(FName(TEXT("Frame Mesh")));

		if (FrameInputs.Num() > 0)
		{
			const UPVGrowthData* FrameData = Cast<UPVGrowthData>(FrameInputs[0].Data);
			if (FrameData)
			{
				const FManagedArrayCollection& FrameCol = FrameData->GetCollection();

				// Capture frame mesh visualization attributes for output propagation
				const TManagedArray<FString>* FMPathAttr =
					FrameCol.FindAttribute<FString>(FName("FrameMeshPath"), PVG::DetailsGroup);
				if (FMPathAttr && FMPathAttr->Num() > 0)
				{
					CapturedFrameMeshPath = (*FMPathAttr)[0];
				}
				const TManagedArray<FTransform>* FMTransformAttr =
					FrameCol.FindAttribute<FTransform>(FName("FrameMeshTransform"), PVG::DetailsGroup);
				if (FMTransformAttr && FMTransformAttr->Num() > 0)
				{
					CapturedFrameMeshTransform = (*FMTransformAttr)[0];
				}

				const TManagedArray<FVector3f>* FramePos =
					FrameCol.FindAttribute<FVector3f>(PVA::PointPosition, PVG::PointGroup);

				if (FramePos && FramePos->Num() >= 24)
				{
					// Extract 8 unique corners from the 24-point / 12-edge layout.
					// Edge table: {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
					// Corner 0=Point[0], 1=Point[1], 2=Point[3], 3=Point[5],
					// 4=Point[8], 5=Point[9], 6=Point[11], 7=Point[13]
					FVector3f Corners[8];
					Corners[0] = (*FramePos)[0];
					Corners[1] = (*FramePos)[1];
					Corners[2] = (*FramePos)[3];
					Corners[3] = (*FramePos)[5];
					Corners[4] = (*FramePos)[8];
					Corners[5] = (*FramePos)[9];
					Corners[6] = (*FramePos)[11];
					Corners[7] = (*FramePos)[13];

					FrameOBB.InitFromCorners(Corners);

					if (!FrameOBB.bValid)
					{
						PCGE_LOG(Warning, GraphAndLog,
							LOCTEXT("DegenerateOBB",
								"Growth Generator: Frame Mesh bounding box is degenerate "
								"(zero-length edge). Avoidance disabled."));
					}
				}
				else
				{
					PCGE_LOG(Warning, GraphAndLog,
						LOCTEXT("InsufficientPoints",
							"Growth Generator: Frame Mesh input has fewer than 24 points. "
							"Expected a Viewport Mesh output. Avoidance disabled."));
				}
			}
		}
	}

	// -------------------------------------------------------------------------
	//  Clamp / derive parameters
	// -------------------------------------------------------------------------
	const float TrunkLength     = FMath::Max(1.0f,  Settings->TrunkLength);
	const float TrunkBaseRadius = FMath::Max(0.01f, Settings->TrunkBaseRadius);
	const float TrunkTipRadius  = FMath::Clamp(Settings->TrunkTipRadius, 0.0f, TrunkBaseRadius);
	const int32 TrunkSegments   = FMath::Max(2, Settings->TrunkSegments);
	const float TrunkLean       = Settings->TrunkLean;

	// Lean direction in XY-plane (normalised; fall back to X-axis if zero)
	const FVector2D LeanAxis2D  = Settings->TrunkLeanAxis.IsNearlyZero()
		? FVector2D(1.0, 0.0)
		: Settings->TrunkLeanAxis.GetSafeNormal();
	const FVector3f LeanDir     = FVector3f((float)LeanAxis2D.X, (float)LeanAxis2D.Y, 0.f);

	const int32 BranchGenerations = FMath::Clamp(Settings->BranchGenerations, 0, 2);
	const int32 BranchesPerNode   = FMath::Max(1, Settings->BranchesPerNode);
	const float BranchLengthScale = FMath::Max(0.05f, Settings->BranchLengthScale);
	const float BranchRadiusScale = FMath::Max(0.05f, Settings->BranchRadiusScale);
	const int32 BranchSegments    = FMath::Max(2, Settings->BranchSegments);
	const float SpreadHalfAngle   = FMath::Clamp(Settings->BranchSpreadAngle, 5.0f, 90.0f);
	const float AttachMin         = FMath::Clamp(Settings->BranchAttachMin, 0.0f, 1.0f);
	const float AttachMax         = FMath::Max(AttachMin + 0.01f,
	                                    FMath::Clamp(Settings->BranchAttachMax, 0.0f, 1.0f));
	// Branch radii are independent of trunk geometry.
	// BranchTaperRatio (tip/base) is derived from branch-specific params and
	// reused for all branch generations to keep proportions consistent.
	const float BranchBase       = FMath::Max(0.01f, Settings->BranchBaseRadius);
	const float BranchTip        = FMath::Clamp(Settings->BranchTipRadius, 0.0f, BranchBase);
	const float BranchTaperRatio = BranchTip / BranchBase;

	// Frame Mesh avoidance parameters
	const float AvoidStrength = FMath::Clamp(Settings->FrameAvoidanceStrength, 0.f, 1.f);
	const float AvoidMargin   = FMath::Max(0.f, Settings->FrameAvoidanceMargin);

	// -------------------------------------------------------------------------
	//  Phase 1: BFS — plan all branches
	// -------------------------------------------------------------------------
	TArray<FBranchWork> Branches;

	// Pre-reserve: 1 (trunk) + N^1 + N^2 branches
	{
		int32 MaxPossible = 1;
		int32 LevelCount  = 1;
		for (int32 g = 0; g < BranchGenerations; ++g) { LevelCount *= BranchesPerNode; MaxPossible += LevelCount; }
		Branches.Reserve(MaxPossible);
	}

	// Trunk
	{
		FBranchWork& Trunk            = Branches.Emplace_GetRef();
		Trunk.BranchIdx               = 0;
		Trunk.HierarchyLevel          = 0;
		Trunk.ParentBranchIdx         = INDEX_NONE;
		Trunk.LengthFromRootAtAttach  = 0.0f;
		Trunk.AttachPos               = FVector3f::ZeroVector;
		Trunk.GrowDir                 = FVector3f(0.f, 0.f, 1.f);
		Trunk.Length                  = TrunkLength;
		Trunk.BaseRadius              = TrunkBaseRadius;
		Trunk.TipRadius               = TrunkTipRadius;
		Trunk.SegmentCount            = TrunkSegments;
		// GlobalPointStart filled after planning
	}

	if (BranchGenerations > 0)
	{
		FRandomStream Rng(Settings->RandomSeed);

		int32 GenStart = 0;   // first branch index in the current parent generation
		int32 GenEnd   = 1;   // one-past-last branch in the current parent generation

		for (int32 Gen = 1; Gen <= BranchGenerations; ++Gen)
		{
			const int32 NextGenStart = Branches.Num();

			for (int32 ParentIdx = GenStart; ParentIdx < GenEnd; ++ParentIdx)
			{
				for (int32 ci = 0; ci < BranchesPerNode; ++ci)
				{
					// Read parent data into locals BEFORE any Emplace_GetRef that
					// might reallocate the array and invalidate pointers/refs.
					const FVector3f PAttach = Branches[ParentIdx].AttachPos;
					const FVector3f PDir    = Branches[ParentIdx].GrowDir;
					const float     PLen    = Branches[ParentIdx].Length;
					const float     PBaseR  = Branches[ParentIdx].BaseRadius;
					const float     PLFR    = Branches[ParentIdx].LengthFromRootAtAttach;
					const int32     PLevel  = Branches[ParentIdx].HierarchyLevel;

					// Random attachment fraction along parent
					const float Frac = AttachMin + Rng.GetFraction() * (AttachMax - AttachMin);
					const float CLFR = PLFR + PLen * Frac;

					// Attachment world position — add lean offset when parent is the trunk.
					// Phase 3 applies LeanDir * TrunkLean * t to trunk point positions, so
					// CAttach must include that same offset to keep child branches flush.
					// Straight-line attachment estimate — Phase 1.5 overwrites Child.AttachPos
				// with the true interpolated position on the parent's curved path, so the
				// lean offset previously added here is no longer needed.
				FVector3f CAttach = PAttach + PDir * (PLen * Frac);

					// Child growth direction: random cone around parent direction
					const FVector3f CDir = VRandCone3f(Rng, PDir, SpreadHalfAngle);

					// Child dimensions.
					// 1st-order branches (parent is trunk, PLevel==0) use BranchBase directly
					// so their radius is independent of TrunkBaseRadius.
					// 2nd-order branches scale from their parent's base via BranchRadiusScale.
					const float CLen    = PLen * BranchLengthScale;
					const float CBaseR  = (PLevel == 0) ? BranchBase : PBaseR * BranchRadiusScale;
					const float CTipR   = CBaseR * BranchTaperRatio;

					const int32 ChildIdx = Branches.Num();

					// Register child on parent BEFORE the Emplace that may reallocate
					Branches[ParentIdx].ChildBranchIndices.Add(ChildIdx);

					FBranchWork& Child             = Branches.Emplace_GetRef();
					Child.BranchIdx                = ChildIdx;
					Child.HierarchyLevel           = Gen;
					Child.ParentBranchIdx          = ParentIdx;
					Child.LengthFromRootAtAttach   = CLFR;
					Child.AttachPos                = CAttach;
					Child.GrowDir                  = CDir;
					Child.Length                   = CLen;
					Child.BaseRadius               = CBaseR;
					Child.TipRadius                = CTipR;
					Child.SegmentCount             = BranchSegments;
				}
			}

			GenStart = NextGenStart;
			GenEnd   = Branches.Num();
		}
	}

	// -------------------------------------------------------------------------
	//  Phase 1.5: Compute curved paths for every branch
	// -------------------------------------------------------------------------
	// Process in array order (= BFS order), so parents are always processed before
	// their children. Each branch performs a segment-by-segment random walk that
	// accumulates small directional perturbations, producing organic curves.
	//
	// Lean for the trunk is baked into CurvedPoints here, replacing the separate
	// lean additions that used to live in Phase 1 (CAttach) and Phase 3 (Pos).
	// Every child's AttachPos is overwritten with the interpolated position on the
	// parent's curved path, keeping all junctions perfectly flush.
	// -------------------------------------------------------------------------
	{
		const float TrunkCurve  = FMath::Max(0.f, Settings->TrunkCurvatureStrength);
		const float BranchCurve = FMath::Max(0.f, Settings->BranchCurvatureStrength);
		FRandomStream CurveRng(Settings->RandomSeed + 314159);

		for (int32 BranchArrayIdx = 0; BranchArrayIdx < Branches.Num(); ++BranchArrayIdx)
		{
			FBranchWork& B      = Branches[BranchArrayIdx];
			const bool bIsTrunk = (B.HierarchyLevel == 0);
			const float MaxAng  = bIsTrunk ? TrunkCurve : BranchCurve;
			const int32 NumPts  = B.SegmentCount + 1;
			const float SegLen  = B.Length / FMath::Max(1, B.SegmentCount);

			B.CurvedPoints.SetNum(NumPts);
			B.CurvedPoints[0] = B.AttachPos;

			FVector3f CurDir = B.GrowDir;
			FVector3f CurPos = B.AttachPos;

			for (int32 si = 0; si < B.SegmentCount; ++si)
			{
				// Perturb growth direction for this step
				if (MaxAng > 0.f)
				{
					CurDir = VRandCone3f(CurveRng, CurDir, MaxAng);
				}

				// Advance: curved direction + per-step lean contribution for trunk
				FVector3f Step = CurDir * SegLen;
				if (bIsTrunk && TrunkLean != 0.f)
				{
					Step += LeanDir * (TrunkLean / (float)B.SegmentCount);
				}

				FVector3f NewPos = CurPos + Step;

				// --- Frame Mesh avoidance ---
				if (FrameOBB.bValid)
				{
					FVector3f FaceNormal;
					const float SD = FrameOBB.SignedDistance(NewPos, FaceNormal);

					if (SD < AvoidMargin)
					{
						// Influence ramps from 0 (at Margin boundary) to 1 (at/inside surface)
						const float Influence = (AvoidMargin > 0.f)
							? 1.f - FMath::Clamp(SD / AvoidMargin, 0.f, 1.f)
							: 1.f;

						// Redirect growth direction away from the mesh surface
						const float DotNorm = FVector3f::DotProduct(CurDir, FaceNormal);
						if (DotNorm < 0.f)
						{
							// Remove inward component to slide along the surface
							FVector3f SlideDir = CurDir - FaceNormal * DotNorm;
							if (!SlideDir.IsNearlyZero())
							{
								SlideDir.Normalize();
								CurDir = FMath::Lerp(CurDir, SlideDir, Influence * AvoidStrength);
								CurDir.Normalize();
							}
							else
							{
								// Growth exactly anti-normal — deflect perpendicular
								const FVector3f RefAxis = (FMath::Abs(FaceNormal.Z) < 0.9f)
									? FVector3f(0.f, 0.f, 1.f) : FVector3f(1.f, 0.f, 0.f);
								CurDir = FVector3f::CrossProduct(FaceNormal, RefAxis).GetSafeNormal();
							}

							// Recompute step with corrected direction
							Step = CurDir * SegLen;
							if (bIsTrunk && TrunkLean != 0.f)
							{
								Step += LeanDir * (TrunkLean / (float)B.SegmentCount);
							}
							NewPos = CurPos + Step;
						}

						// Hard constraint: if still inside, project to nearest face + margin
						FVector3f PostNormal;
						const float PostSD = FrameOBB.SignedDistance(NewPos, PostNormal);
						if (PostSD < 0.f)
						{
							NewPos = FrameOBB.ProjectToSurface(NewPos, AvoidMargin);
						}
					}
				}

				CurPos = NewPos;
				B.CurvedPoints[si + 1] = CurPos;
			}

			// Overwrite each child's AttachPos with the correctly interpolated position
			// on this branch's curved path. Guarantees flush branch junctions.
			for (const int32 ChildIdx : B.ChildBranchIndices)
			{
				FBranchWork& Child     = Branches[ChildIdx];
				const float  Frac      = (B.Length > 0.f)
					? FMath::Clamp(
						(Child.LengthFromRootAtAttach - B.LengthFromRootAtAttach) / B.Length,
						0.f, 1.f)
					: 0.f;
				const float  FracScaled = Frac * (float)B.SegmentCount;
				const int32  IdxLow     = FMath::Clamp(FMath::FloorToInt(FracScaled), 0, NumPts - 2);
				const float  Alpha      = FracScaled - (float)IdxLow;
				Child.AttachPos = FMath::Lerp(
					B.CurvedPoints[IdxLow], B.CurvedPoints[IdxLow + 1], Alpha);
			}
		}
	}

	// Assign contiguous GlobalPointStart offsets
	{
		int32 Running = 0;
		for (FBranchWork& B : Branches)
		{
			B.GlobalPointStart = Running;
			Running += B.SegmentCount + 1;
		}
	}

	int32 TotalPoints = 0;
	for (const FBranchWork& B : Branches) TotalPoints += B.SegmentCount + 1;
	const int32 TotalBranches = Branches.Num();

	if (TotalPoints == 0 || TotalBranches == 0)
	{
		PCGE_LOG(Error, GraphAndLog,
			LOCTEXT("NoBranches", "Growth Generator produced no branches — check settings."));
		return true;
	}

	// -------------------------------------------------------------------------
	//  Phase 2: Allocate FManagedArrayCollection
	// -------------------------------------------------------------------------
	FManagedArrayCollection Collection;

	// --- Points group --- (all 16 required by FPointFacade::IsValid())
	Collection.AddGroup(PVG::PointGroup);
	Collection.AddElements(TotalPoints, PVG::PointGroup);

	TManagedArray<FVector3f>& PositionArr  = Collection.AddAttribute<FVector3f>(PVA::PointPosition,      PVG::PointGroup);
	TManagedArray<float>&     LFRootArr    = Collection.AddAttribute<float>    (PVA::LengthFromRoot,     PVG::PointGroup);
	TManagedArray<float>&     LFSeedArr    = Collection.AddAttribute<float>    (PVA::LengthFromSeed,     PVG::PointGroup);
	TManagedArray<float>&     ScaleArr     = Collection.AddAttribute<float>    (PVA::PointScale,         PVG::PointGroup);
	TManagedArray<float>&     PScaleGradA  = Collection.AddAttribute<float>    (PVA::PointScaleGradient, PVG::PointGroup);
	TManagedArray<float>&     HullGradA    = Collection.AddAttribute<float>    (PVA::HullGradient,       PVG::PointGroup);
	TManagedArray<float>&     TrunkGradA   = Collection.AddAttribute<float>    (PVA::MainTrunkGradient,  PVG::PointGroup);
	TManagedArray<float>&     GroundGradA  = Collection.AddAttribute<float>    (PVA::GroundGradient,     PVG::PointGroup);
	TManagedArray<float>&     BranchGradA  = Collection.AddAttribute<float>    (PVA::BranchGradient,     PVG::PointGroup);
	TManagedArray<float>&     PlantGradA   = Collection.AddAttribute<float>    (PVA::PlantGradient,      PVG::PointGroup);
	TManagedArray<int32>&     BudNumA      = Collection.AddAttribute<int32>    (PVA::BudNumber,          PVG::PointGroup);
	TManagedArray<float>&     NjordA       = Collection.AddAttribute<float>    (PVA::NjordPixelIndex,    PVG::PointGroup);

	// TArray attributes — must be populated per-point with minimum element counts
	// (PVE mesh builder hard-asserts sizes at build time)
	TManagedArray<TArray<FVector3f>>& BudDirArr   = Collection.AddAttribute<TArray<FVector3f>>(PVA::BudDirection,     PVG::PointGroup);
	TManagedArray<TArray<float>>&     BudHormArr  = Collection.AddAttribute<TArray<float>>    (PVA::BudHormoneLevels, PVG::PointGroup);
	TManagedArray<TArray<float>>&     BudLightArr = Collection.AddAttribute<TArray<float>>    (PVA::BudLightDetected, PVG::PointGroup);
	TManagedArray<TArray<int32>>&     BudDevArr   = Collection.AddAttribute<TArray<int32>>    (PVA::BudDevelopment,   PVG::PointGroup);

	// Optional UV attributes — needed for correct bark texture mapping in the Mesh Builder
	TManagedArray<float>&     UVvArr    = Collection.AddAttribute<float>    (PVA::TextureCoordV,       PVG::PointGroup);
	TManagedArray<float>&     UVuOffArr = Collection.AddAttribute<float>    (PVA::TextureCoordUOffset, PVG::PointGroup);
	TManagedArray<FVector2f>& UVuRngArr = Collection.AddAttribute<FVector2f>(PVA::URange,              PVG::PointGroup);

	// --- Primitives (Branch) group --- (all 8 required by FBranchFacade::IsValid())
	Collection.AddGroup(PVG::BranchGroup);
	Collection.AddElements(TotalBranches, PVG::BranchGroup);

	TManagedArray<TArray<int32>>& BPtsArr    = Collection.AddAttribute<TArray<int32>>(PVA::BranchPoints,          PVG::BranchGroup);
	TManagedArray<TArray<int32>>& ParentsArr = Collection.AddAttribute<TArray<int32>>(PVA::BranchParents,         PVG::BranchGroup);
	TManagedArray<TArray<int32>>& ChildArr   = Collection.AddAttribute<TArray<int32>>(PVA::BranchChildren,        PVG::BranchGroup);
	TManagedArray<int32>&         BNumArr    = Collection.AddAttribute<int32>        (PVA::BranchNumber,          PVG::BranchGroup);
	TManagedArray<int32>&         BHierArr   = Collection.AddAttribute<int32>        (PVA::BranchHierarchyNumber, PVG::BranchGroup);
	TManagedArray<int32>&         BParNumArr = Collection.AddAttribute<int32>        (PVA::BranchParentNumber,    PVG::BranchGroup);
	TManagedArray<int32>&         BPlantArr  = Collection.AddAttribute<int32>        (PVA::PlantNumber,           PVG::BranchGroup);
	TManagedArray<int32>&         BSrcBudArr = Collection.AddAttribute<int32>        (PVA::BranchSourceBudNumber, PVG::BranchGroup);

	// branchUVMat: PVMeshBuilder.cpp:1055 reads GetBranchUVMaterial(branch) as MaterialID.
	// Absent -> INDEX_NONE (-1) -> no bark material applied. 0 = first material slot (correct default).
	Collection.AddAttribute<int32>(PVA::BranchUVMaterial,            PVG::BranchGroup);

	// branchSimulationGroupIndex: consumed by PVE simulation nodes. Default 0.
	Collection.AddAttribute<int32>(PVA::BranchSimulationGroupIndex,  PVG::BranchGroup);

	// --- Details group --- (optional but avoids null-accessor warnings in FBranchFacade)
	Collection.AddGroup(PVG::DetailsGroup);
	Collection.AddElements(1, PVG::DetailsGroup);
	// TrunkMaterialPath: PVE skeleton schema stores ONE material path.
	// GenerationMaterials[0] (the trunk/bark material) is written here and is the path
	// the Mesh Builder loads for all generations when bOverrideMaterial = false.
	TManagedArray<FString>& TrunkMatArr = Collection.AddAttribute<FString>(PVA::TrunkMaterialPath, PVG::DetailsGroup);
	const TArray<TSoftObjectPtr<UMaterialInterface>>& GenMats = Settings->GenerationMaterials;
	if (GenMats.IsValidIndex(0) && !GenMats[0].IsNull())
	{
		TrunkMatArr[0] = GenMats[0].ToSoftObjectPath().ToString();
	}

	// TrunkURange: FBranchFacade uses TManagedArrayAccessor<TArray<FVector2f>> — must be TArray<FVector2f>.
	// PVMeshBuilderSettings.cpp iterates this array; one entry = one generation material slot.
	// N entries → N slots → branchUVMat per branch (driven by BudDevelopment[0] = hierarchy level)
	// correctly indexes each slot without any extra Mesh Builder configuration.
	// If GenerationMaterials is empty, fall back to a single [0,1] entry for generation 0.
	TManagedArray<TArray<FVector2f>>& TrunkURng = Collection.AddAttribute<TArray<FVector2f>>(PVA::TrunkURange, PVG::DetailsGroup);
	const int32 NumMatSlots = FMath::Max(1, GenMats.Num());
	for (int32 mi = 0; mi < NumMatSlots; ++mi)
	{
		TrunkURng[0].Add(FVector2f(0.f, 1.f));
	}

	// LeafPhyllotaxy: required by FPVFoliage::DistributeFoliage() which hard-reads indices [1],[3],[4],[6],[7].
	// Accessor is TManagedArrayAccessor<TArray<float>> on Details[0]; IsValid() crashes if attribute is absent.
	TManagedArray<TArray<float>>& LeafPhyllotaxyArr = Collection.AddAttribute<TArray<float>>(PVA::LeafPhyllotaxy, PVG::DetailsGroup);
	LeafPhyllotaxyArr[0].Init(0.f, 8);

	// FoliagePath: read by the PVE foliage preset loader. Empty string = no preset (correct default).
	Collection.AddAttribute<FString>(PVA::FoliagePath, PVG::DetailsGroup);

	// Frame mesh attributes — propagate from Frame Mesh input for viewport rendering
	if (!CapturedFrameMeshPath.IsEmpty())
	{
		TManagedArray<FString>& FMP =
			Collection.AddAttribute<FString>(FName("FrameMeshPath"), PVG::DetailsGroup);
		FMP[0] = CapturedFrameMeshPath;

		TManagedArray<FTransform>& FMT =
			Collection.AddAttribute<FTransform>(FName("FrameMeshTransform"), PVG::DetailsGroup);
		FMT[0] = CapturedFrameMeshTransform;
	}

	// --- PlantProfiles group ---
	// PVMeshBuilder.cpp reads this for radial cross-section shaping via FPlantProfileFacade.
	// 101-float array: each index (0-100) is a radial multiplier at that circumference position.
	// 1.0f everywhere = perfect circle (same result as the fallback when this group is absent,
	// but adding it makes the output compatible with the Mesh Builder's Plant Profile selector).
	Collection.AddGroup(PVG::PlantProfilesGroup);
	Collection.AddElements(1, PVG::PlantProfilesGroup);
	TManagedArray<TArray<float>>& ProfilePointsArr =
		Collection.AddAttribute<TArray<float>>(PVA::ProfilePoints, PVG::PlantProfilesGroup);
	ProfilePointsArr[0].Init(1.0f, 101);

	// -------------------------------------------------------------------------
	//  Phase 3: Fill Points
	// -------------------------------------------------------------------------

	// Normalised light direction written to BudDirection[2] (optimal) and [3] (sub-optimal).
	// PVGravity reads these and blends with its PhototropicBias to bend branch growth toward light.
	// PVFoliage reads [2] to orient leaf normals toward the light source.
	const FVector   LDNorm    = Settings->LightDirection.IsNearlyZero()
		? FVector(0.0, 0.0, 1.0) : Settings->LightDirection.GetSafeNormal();
	const FVector3f LightDir3f((float)LDNorm.X, (float)LDNorm.Y, (float)LDNorm.Z);

	// Trunk taper curve: evaluated once per trunk point to remap the linear t parameter
	// into a non-linear interpolation factor. Falls back to linear t if curve has no keys.
	const FRichCurve* TrunkTaperRichCurve = Settings->TrunkTaperCurve.GetRichCurveConst();
	const bool bHasTrunkTaperCurve = TrunkTaperRichCurve && TrunkTaperRichCurve->GetNumKeys() > 0;

	for (const FBranchWork& B : Branches)
	{
		const bool bIsTrunk = (B.HierarchyLevel == 0);

		// Ethylene for every point on this branch: look up per-hierarchy level.
		// High ethylene → foliage suppressed (PVE Foliage Distributor: place when Ethylene < Threshold).
		const float EthyleneForBranch = Settings->EthyleneLevels.IsValidIndex(B.HierarchyLevel)
			? FMath::Clamp(Settings->EthyleneLevels[B.HierarchyLevel], 0.f, 1.f)
			: 0.f;

		for (int32 i = 0; i <= B.SegmentCount; ++i)
		{
			const float   t       = (B.SegmentCount > 0)
				? static_cast<float>(i) / static_cast<float>(B.SegmentCount)
				: 0.f;
			const int32   pi      = B.GlobalPointStart + i;

			// Position from pre-curved path (Phase 1.5). Lean is baked in for the trunk.
			FVector3f Pos = B.CurvedPoints[i];

			// Local tangent direction at this point (used for BudDir[0] and up-vector).
			// Derived from consecutive CurvedPoints rather than the uniform branch GrowDir,
			// giving PVGravity and PVFoliage accurate per-segment orientation data.
			const FVector3f SegDir = (i < B.SegmentCount)
				? (B.CurvedPoints[i + 1] - B.CurvedPoints[i]).GetSafeNormal()
				: (i > 0)
					? (B.CurvedPoints[i] - B.CurvedPoints[i - 1]).GetSafeNormal()
					: B.GrowDir;

			const float LFRoot    = B.LengthFromRootAtAttach + B.Length * t;
			const float LFSeed    = B.Length * t;
			// Trunk uses the taper curve to remap t → non-linear interpolation factor.
		// Branches keep linear taper (t unchanged).
		const float TaperT = (bIsTrunk && bHasTrunkTaperCurve)
			? FMath::Clamp(TrunkTaperRichCurve->Eval(t), 0.f, 1.f)
			: t;
		const float Scale     = FMath::Lerp(B.BaseRadius, B.TipRadius, TaperT);
			const float PlantGrad = FMath::Clamp(LFRoot / TrunkLength, 0.f, 1.f);

			PositionArr [pi] = Pos;
			LFRootArr   [pi] = LFRoot;
			LFSeedArr   [pi] = LFSeed;
			ScaleArr    [pi] = Scale;
			PScaleGradA [pi] = (TrunkBaseRadius > 0.f) ? (Scale / TrunkBaseRadius) : 0.f;
			HullGradA   [pi] = 1.0f;
			TrunkGradA  [pi] = bIsTrunk ? 1.0f : 0.0f;
			GroundGradA [pi] = FMath::Clamp(1.0f - PlantGrad, 0.f, 1.f);
			BranchGradA [pi] = t;
			PlantGradA  [pi] = PlantGrad;
			BudNumA     [pi] = 0;
			NjordA      [pi] = 0.f;

			// UV bark mapping: v = normalised length along branch
			UVvArr  [pi] = LFSeed;
			UVuOffArr[pi] = 0.f;
			UVuRngArr[pi] = FVector2f(0.f, 1.f);

			// budDevelopment: [0]=Generation, [1]=unused, [2]=Age
			// PVMaterialSettings.cpp:89 reads [0] for EUVMaterialMode::Generation and
			// [2] for EUVMaterialMode::Age to decide which material slot each branch gets.
			// With all zeros, MinGeneration==MaxGeneration==0 → normalisation is 0/0
			// → every branch receives the same material regardless of Repeat/Fit mode.
			BudDevArr[pi].Init(0, 3);
			BudDevArr[pi][0] = B.HierarchyLevel;  // Generation: trunk=0, 1st-order=1, 2nd-order=2
			BudDevArr[pi][2] = B.HierarchyLevel;  // Age proxy (reversed range in PVMaterialSettings)

			// budHormoneLevels: [0-3]=reserved hormones, [4]=Ethylene
			// PVCarve.cpp:92 asserts Num() >= 5.
			// Ethylene: high = foliage suppressed; low = foliage placed (PVE Foliage Distributor).
			BudHormArr[pi].Init(0.f, 5);
			BudHormArr[pi][4] = EthyleneForBranch;

			// budLightDetected: only accessed via .Num() guards — 1 element is safe
			BudLightArr[pi].Init(0.f, 1);

			// budDirection layout (TArray<FVector3f>, >= 6 elements required by PVMeshBuilder):
			// [0] = Apical aim direction
			// [1] = Axillary direction (zero-initialised, unused here)
			// [2] = Light Optimal direction   -> PVGravity uses for phototropic bending
			// [3] = Light Sub-Optimal direction -> PVGravity blends [2]/[3] via PhototropicBias
			// [4] = (zero-initialised, unused)
			// [5] = Up / orientation reference vector
			{
				TArray<FVector3f>& Dirs = BudDirArr[pi];
				Dirs.SetNum(6); // [1],[4] zero-initialised; we set the rest explicitly
				Dirs[0] = SegDir;      // apical aim — per-segment tangent, not uniform GrowDir
				Dirs[2] = LightDir3f;  // phototropism: light optimal direction
				Dirs[3] = LightDir3f;  // phototropism: light sub-optimal (same source)
				const FVector3f RefAxis = (FMath::Abs(SegDir.Z) < 0.99f)
					? FVector3f(0.f, 0.f, 1.f) : FVector3f(1.f, 0.f, 0.f);
				Dirs[5] = FVector3f::CrossProduct(SegDir, RefAxis).GetSafeNormal();
			}
		}
	}

	// -------------------------------------------------------------------------
	//  Phase 4: Fill Branches
	// -------------------------------------------------------------------------
	for (const FBranchWork& B : Branches)
	{
		const int32 bi = B.BranchIdx;

		// Build the contiguous point index list for this branch
		TArray<int32>& PtsEntry = BPtsArr[bi];
		PtsEntry.Reserve(B.SegmentCount + 1);
		for (int32 i = 0; i <= B.SegmentCount; ++i)
		{
			PtsEntry.Add(B.GlobalPointStart + i);
		}

		// PVE convention: branch numbers are 1-based (trunk = 1, children = 2, 3, …).
		// branchParentNumber = 0 for the trunk (the "no parent" sentinel).
		// children/parents TArrays store branch NUMBERS (1-based), not array indices.
		// FPlantFacade::IsTrunkIndex(i) checks BranchParentNumber[i] == 0; using -1
		// breaks that guard and causes FindChecked crashes in PVCarve / PVSimulation.

		// Parent reference (trunk has no parent)
		if (B.ParentBranchIdx != INDEX_NONE)
		{
			ParentsArr[bi].Add(B.ParentBranchIdx + 1);  // 1-based branch number
		}

		for (const int32 ChildIdx : B.ChildBranchIndices)
		{
			ChildArr[bi].Add(ChildIdx + 1);              // 1-based branch number
		}
		BNumArr   [bi] = B.BranchIdx + 1;                                             // 1-based
		BHierArr  [bi] = B.HierarchyLevel;
		BParNumArr[bi] = (B.ParentBranchIdx != INDEX_NONE) ? (B.ParentBranchIdx + 1) : 0; // 0 = trunk sentinel
		BPlantArr [bi] = 0;
		BSrcBudArr[bi] = 0;
	}

	// -------------------------------------------------------------------------
	//  Phase 4.5: Append frame mesh wireframe to the output collection
	//  SkeletonRenderer draws ALL branches (no filtering), so these 12 edges
	//  will render as visible wireframe lines alongside the tree skeleton.
	//  PointScale = 0 makes the Mesh Builder emit invisible degenerate geometry.
	// -------------------------------------------------------------------------
	if (FrameOBB.bValid)
	{
		// Reconstruct 8 OBB corners from centre + half-extents along each axis
		const FVector3f HX = FrameOBB.DirX * FrameOBB.HalfExtX;
		const FVector3f HY = FrameOBB.DirY * FrameOBB.HalfExtY;
		const FVector3f HZ = FrameOBB.DirZ * FrameOBB.HalfExtZ;
		const FVector3f C  = FrameOBB.Center;

		FVector3f FC[8];
		FC[0] = C - HX - HY - HZ;   // (MinX, MinY, MinZ)
		FC[1] = C + HX - HY - HZ;   // (MaxX, MinY, MinZ)
		FC[2] = C + HX + HY - HZ;   // (MaxX, MaxY, MinZ)
		FC[3] = C - HX + HY - HZ;   // (MinX, MaxY, MinZ)
		FC[4] = C - HX - HY + HZ;   // (MinX, MinY, MaxZ)
		FC[5] = C + HX - HY + HZ;   // (MaxX, MinY, MaxZ)
		FC[6] = C + HX + HY + HZ;   // (MaxX, MaxY, MaxZ)
		FC[7] = C - HX + HY + HZ;   // (MinX, MaxY, MaxZ)

		static constexpr int32 FEdges[12][2] = {
			{0,1},{1,2},{2,3},{3,0},   // bottom face
			{4,5},{5,6},{6,7},{7,4},   // top face
			{0,4},{1,5},{2,6},{3,7}    // vertical edges
		};

		constexpr int32 FramePointCount  = 24;  // 2 per edge
		constexpr int32 FrameBranchCount = 12;

		// Extend existing groups — AddElements appends to the end
		const int32 FramePointStart  = Collection.NumElements(PVG::PointGroup);
		const int32 FrameBranchStart = Collection.NumElements(PVG::BranchGroup);
		Collection.AddElements(FramePointCount,  PVG::PointGroup);
		Collection.AddElements(FrameBranchCount, PVG::BranchGroup);

		// --- Fill frame mesh points ---
		for (int32 e = 0; e < 12; ++e)
		{
			for (int32 v = 0; v < 2; ++v)
			{
				const int32 pi = FramePointStart + e * 2 + v;

				PositionArr [pi] = FC[FEdges[e][v]];
				LFRootArr   [pi] = 0.f;
				LFSeedArr   [pi] = 0.f;
				ScaleArr    [pi] = 0.f;         // invisible to Mesh Builder
				PScaleGradA [pi] = 0.f;
				HullGradA   [pi] = 0.f;
				TrunkGradA  [pi] = 0.f;
				GroundGradA [pi] = 0.f;
				BranchGradA [pi] = static_cast<float>(v);
				PlantGradA  [pi] = 0.f;
				BudNumA     [pi] = 0;
				NjordA      [pi] = 0.f;

				UVvArr   [pi] = 0.f;
				UVuOffArr[pi] = 0.f;
				UVuRngArr[pi] = FVector2f(0.f, 1.f);

				BudDevArr  [pi].Init(0, 3);
				BudDevArr  [pi][0] = 99;         // generation 99 — no material slot conflict
				BudHormArr [pi].Init(0.f, 5);
				BudHormArr [pi][4] = 1.0f;       // max ethylene — foliage suppressed
				BudLightArr[pi].Init(0.f, 1);

				TArray<FVector3f>& Dirs = BudDirArr[pi];
				Dirs.SetNumZeroed(6);             // all zero — no phototropism
			}
		}

		// --- Fill frame mesh branches ---
		for (int32 e = 0; e < 12; ++e)
		{
			const int32 bi = FrameBranchStart + e;

			BPtsArr   [bi] = { FramePointStart + e * 2, FramePointStart + e * 2 + 1 };
			// ParentsArr[bi] and ChildArr[bi] left default-empty (standalone edges)
			BNumArr   [bi] = TotalBranches + e + 1;  // continues 1-based numbering
			BHierArr  [bi] = 99;                     // distinct from tree (0/1/2)
			BParNumArr[bi] = 0;                      // standalone — no parent
			BPlantArr [bi] = 0;
			BSrcBudArr[bi] = 0;
		}
	}

	// -------------------------------------------------------------------------
	//  Phase 5: Foliage scatter
	//  Only runs when FoliageEntries contains at least one valid entry.
	// -------------------------------------------------------------------------
	if (Settings->FoliageEntries.Num() > 0)
	{
		// Build a deduplicated list of mesh asset paths.
		// NameId in FFoliageEntryData is an index into this list (= FoliageNames group).
		TArray<FString> MeshPaths;
		TArray<int32>   EntryToNameId;
		EntryToNameId.SetNum(Settings->FoliageEntries.Num());

		for (int32 ei = 0; ei < Settings->FoliageEntries.Num(); ++ei)
		{
			const FPCGFoliageEntry& E = Settings->FoliageEntries[ei];
			if (!E.Mesh.IsNull() && E.DensityPer100cm > 0.0f)
			{
				const FString Path = E.Mesh.ToSoftObjectPath().ToString();
				int32 Idx = MeshPaths.IndexOfByKey(Path);
				if (Idx == INDEX_NONE) { Idx = MeshPaths.Num(); MeshPaths.Add(Path); }
				EntryToNameId[ei] = Idx;
			}
			else
			{
				EntryToNameId[ei] = INDEX_NONE; // null mesh or zero density -- skip
			}
		}

		if (MeshPaths.Num() > 0)
		{
			// Mutable constructor calls DefineSchema automatically,
			// creating the Foliage and FoliageNames groups with 0 initial elements.
			PV::Facades::FFoliageFacade FoliageFacade(Collection);

			// Register all unique mesh paths as named foliage types.
			FoliageFacade.SetFoliageNames(MeshPaths);

			FRandomStream FolRng(Settings->RandomSeed + 1337);

			for (const FBranchWork& B : Branches)
			{
				if (B.Length <= 0.f) { continue; }

				TArray<int32> BranchFolIds;

				for (int32 ei = 0; ei < Settings->FoliageEntries.Num(); ++ei)
				{
					const FPCGFoliageEntry& E      = Settings->FoliageEntries[ei];
					const int32             NameId = EntryToNameId[ei];

					if (NameId == INDEX_NONE)                    { continue; }
					if (B.HierarchyLevel < E.MinHierarchyLevel) { continue; }

					const int32 Count = FMath::Max(0,
						FMath::RoundToInt(E.DensityPer100cm * B.Length * 0.01f));
					if (Count == 0) { continue; }

					for (int32 ii = 0; ii < Count; ++ii)
					{
						const float     t          = FolRng.GetFraction();
						// Interpolate along the pre-curved path for accurate pivot placement.
						const float     FracScaled = t * (float)B.SegmentCount;
						const int32     IdxLow     = FMath::Clamp(
							FMath::FloorToInt(FracScaled), 0, B.CurvedPoints.Num() - 2);
						const float     Alpha      = FracScaled - (float)IdxLow;
						const FVector3f Pivot      = FMath::Lerp(
							B.CurvedPoints[IdxLow], B.CurvedPoints[IdxLow + 1], Alpha);
						// Local tangent direction at the foliage position.
						const FVector3f FolDir     =
							(B.CurvedPoints[IdxLow + 1] - B.CurvedPoints[IdxLow]).GetSafeNormal();
						const FVector3f RefUp      = (FMath::Abs(FolDir.Z) < 0.99f)
							? FVector3f(0.f, 0.f, 1.f) : FVector3f(1.f, 0.f, 0.f);
						const FVector3f UpVec      =
							FVector3f::CrossProduct(FolDir, RefUp).GetSafeNormal();

						PV::Facades::FFoliageEntryData Data;
						Data.NameId         = NameId;
						Data.BranchId       = B.BranchIdx;
						Data.PivotPoint     = Pivot;
						Data.UpVector       = UpVec;
						Data.NormalVector   = FolDir;
						Data.Scale          = E.Scale;
						Data.LengthFromRoot = B.LengthFromRootAtAttach + B.Length * t;
						Data.ParentBoneID   = INDEX_NONE;

						BranchFolIds.Add(FoliageFacade.AddFoliageEntry(Data));
					}
				}

				if (BranchFolIds.Num() > 0)
				{
					FoliageFacade.SetFoliageIdsArray(B.BranchIdx, BranchFolIds);
				}
			}
		}
	}

	// -------------------------------------------------------------------------
	//  Phase 6: Validate and emit
	// -------------------------------------------------------------------------
	{
		PV::Facades::FPointFacade  PF(Collection);
		PV::Facades::FBranchFacade BF(Collection);

		if (!PF.IsValid() || !BF.IsValid())
		{
			PCGE_LOG(Error, GraphAndLog,
				LOCTEXT("InvalidCollection",
					"Generated FManagedArrayCollection failed PVE validation. "
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
