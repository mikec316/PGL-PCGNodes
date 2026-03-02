// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PGLGravity.h"

#include "PCGContext.h"
#include "PCGPin.h"

// PVE data + facades
#include "DataTypes/PVGrowthData.h"
#include "Facades/PVAttributesNames.h"
#include "Facades/PVBranchFacade.h"
#include "Facades/PVPointFacade.h"
#include "Facades/PVPlantFacade.h"
#include "Facades/PVFoliageFacade.h"

#define LOCTEXT_NAMESPACE "PGLGravitySettings"

// =============================================================================
//  Local constants — bud direction array indices (PVE private, redefined here)
// =============================================================================
namespace
{
	constexpr int32 BudDirLightOptimal    = 2;
	constexpr int32 BudDirLightSubOptimal = 3;
}

// =============================================================================
//  FOBB — Oriented Bounding Box (same as PCGGrowthGenerator.cpp)
// =============================================================================
namespace
{

struct FOBB
{
	FVector3f Center;
	FVector3f DirX, DirY, DirZ;          // unit axes
	float     HalfExtX, HalfExtY, HalfExtZ;
	bool      bValid = false;

	/** Construct from 8 corner positions matching the Viewport Mesh edge table. */
	void InitFromCorners(const FVector3f Corners[8])
	{
		const FVector3f EdgeX = Corners[1] - Corners[0];
		const FVector3f EdgeY = Corners[3] - Corners[0];
		const FVector3f EdgeZ = Corners[4] - Corners[0];

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

} // anonymous namespace

// =============================================================================
//  Gravity parameters — local mirror of PVE's FPVGravityParams
// =============================================================================
namespace
{

struct FGravityParams
{
	EPGLGravityMode Mode            = EPGLGravityMode::Gravity;
	float           Gravity         = 0.f;
	FVector3f       Direction       = FVector3f(0.f, 0.f, -1.f);
	float           AngleCorrection = 0.f;
	float           PhototropicBias = 0.f;
	float           AvoidStrength   = 0.f;
	float           AvoidMargin     = 0.f;
};

} // anonymous namespace

// =============================================================================
//  Static gravity helpers
// =============================================================================
namespace
{

/**
 * Build per-point phototropic direction array by blending light-optimal and
 * light-sub-optimal bud directions.
 */
void GeneratePhototropicData(
	const FGravityParams& Params,
	const PV::Facades::FBranchFacade& BranchFacade,
	const PV::Facades::FPointFacade& PointFacade,
	TArray<FVector3f>& OutDirections)
{
	FVector3f PrevVec = FVector3f::UpVector;
	OutDirections.Init(PrevVec, PointFacade.GetElementCount());

	for (int32 BranchIndex = 0; BranchIndex < BranchFacade.GetElementCount(); ++BranchIndex)
	{
		const TArray<int32>& BranchPoints = BranchFacade.GetPoints(BranchIndex);

		for (int32 i = BranchPoints.Num() - 1; i > 0; --i)
		{
			const int32 PointIndex = BranchPoints[i];
			const TArray<FVector3f>& BudDirs = PointFacade.GetBudDirection(PointIndex);

			if (BudDirs.Num() > BudDirLightSubOptimal)
			{
				const FVector3f& LightOpt    = BudDirs[BudDirLightOptimal];
				const FVector3f& LightSubOpt = BudDirs[BudDirLightSubOptimal];

				FVector3f PhotDir = FMath::Lerp(LightOpt, LightSubOpt, Params.PhototropicBias);
				PhotDir.Normalize();

				if (PhotDir.Length() < 0.001f)
				{
					PhotDir = PrevVec;
				}
				else
				{
					PrevVec = PhotDir;
				}

				if (OutDirections.IsValidIndex(PointIndex))
				{
					OutDirections[PointIndex] = PhotDir;
				}
			}
		}
	}
}

/**
 * Apply OBB avoidance to a single point position, modifying it in-place.
 */
void ApplyFrameAvoidance(
	FVector3f& Position,
	const FOBB& FrameOBB,
	float AvoidStrength,
	float AvoidMargin)
{
	if (!FrameOBB.bValid)
	{
		return;
	}

	FVector3f FaceNormal;
	const float SD = FrameOBB.SignedDistance(Position, FaceNormal);

	if (SD < AvoidMargin)
	{
		if (SD < 0.f)
		{
			// Hard-project interior points to surface + margin
			Position = FrameOBB.ProjectToSurface(Position, AvoidMargin);
		}
		else
		{
			// Soft push: ramps from 0 at margin boundary to 1 at surface
			const float Influence = 1.f - FMath::Clamp(SD / AvoidMargin, 0.f, 1.f);
			Position += FaceNormal * ((AvoidMargin - SD) * Influence * AvoidStrength);
		}
	}
}

/**
 * Recursive gravity application to a single branch and all of its descendants.
 * Mirrors the PVE FPVGravity::ApplyGravity recursive overload.
 */
void ApplyGravityRecursive(
	const int32 BranchIndex,
	const FGravityParams& Params,
	const TArray<FVector3f>& PhototropicDirections,
	const FOBB& FrameOBB,
	FManagedArrayCollection& OutCollection,
	FQuat4f TotalDownForce = FQuat4f::Identity,
	FVector3f PreviousPosition = FVector3f::ZeroVector,
	FVector3f PivotPosition = FVector3f::ZeroVector)
{
	namespace PVG = PV::GroupNames;
	namespace PVA = PV::AttributeNames;

	// --- CalculateDownForce lambda (same maths as PVE) ---
	const auto CalculateDownForce = [&Params, &PhototropicDirections](
		const FVector3f& InRelativeDirection,
		const int32 InPointIndex,
		const float InPScaleGradient) -> FQuat4f
	{
		const FVector3f PhotDir = PhototropicDirections.IsValidIndex(InPointIndex)
			? PhototropicDirections[InPointIndex]
			: FVector3f::UpVector;

		const FVector3f GravDir = (Params.Mode == EPGLGravityMode::Phototropic)
			? PhotDir
			: Params.Direction;

		float YDot = 1.0f - FMath::Abs(FVector3f::DotProduct(InRelativeDirection, GravDir));
		YDot = FMath::GetMappedRangeValueClamped(
			FVector2f(0.f, 1.f),
			FVector2f(FMath::Max(0.1f, 1.f - Params.AngleCorrection), 1.f),
			YDot);

		const float GravityImpact = (Params.Gravity / 10.f) * (1.f - InPScaleGradient) * YDot;
		const FVector3f GravForce = FMath::Lerp(InRelativeDirection, GravDir, GravityImpact).GetUnsafeNormal();
		return FQuat4f::FindBetween(InRelativeDirection, GravForce);
	};

	// --- Acquire facades ---
	PV::Facades::FBranchFacade  BranchFacade(OutCollection);
	PV::Facades::FPointFacade   PointFacade(OutCollection);
	PV::Facades::FFoliageFacade FoliageFacade(OutCollection);

	TManagedArray<FVector3f>& PointPositions = PointFacade.ModifyPositions();

	// Bud directions — accessed directly since FBudVectorsFacade is not exported
	TManagedArray<TArray<FVector3f>>* PointBudDirections =
		OutCollection.FindAttribute<TArray<FVector3f>>(PVA::BudDirection, PVG::PointGroup);

	const TArray<int32>& CurrentBranchPoints   = BranchFacade.GetPoints(BranchIndex);
	const TArray<int32>& CurrentBranchChildren = BranchFacade.GetChildren(BranchIndex);
	const TManagedArray<int32>& BranchNumbers  = BranchFacade.GetBranchNumbers();

	// Resolve child branch indices (same logic as PVE)
	TArray<int32> ChildrenIndices;
	ChildrenIndices.Reserve(CurrentBranchChildren.Num());

	for (const int32 Child : CurrentBranchChildren)
	{
		const int32 ChildIndex = BranchNumbers.Find(Child);

		if (ChildIndex != INDEX_NONE)
		{
			const int32 ParentIndex = BranchFacade.GetParentIndex(ChildIndex);
			if (ParentIndex == BranchIndex)
			{
				// Skip frame mesh wireframe branches
				if (BranchFacade.GetBranchHierarchyNumber(ChildIndex) == 99)
				{
					continue;
				}
				ChildrenIndices.Add(ChildIndex);
			}
		}
	}

	// --- Process first point (root of this branch) ---
	if (TotalDownForce != FQuat4f::Identity)
	[[likely]]
	{
		const int32 PointIndex = CurrentBranchPoints[0];
		FVector3f& Position = PointPositions[PointIndex];
		const float PScaleGradient = PointFacade.GetPointScaleGradient(PointIndex);

		const FVector3f RelativeDirection = TotalDownForce * (Position - PreviousPosition);
		const FQuat4f DownForce = CalculateDownForce(
			RelativeDirection.GetUnsafeNormal(), PointIndex, PScaleGradient);

		TotalDownForce = DownForce * TotalDownForce;
		PreviousPosition = Position;
		Position = PivotPosition + DownForce * RelativeDirection;

		// Frame mesh avoidance on gravity-displaced position
		ApplyFrameAvoidance(Position, FrameOBB, Params.AvoidStrength, Params.AvoidMargin);

		// Rotate bud directions (skip light optimal/sub-optimal)
		if (PointBudDirections)
		{
			TArray<FVector3f>& BudDirs = (*PointBudDirections)[PointIndex];
			for (int32 i = 0; i < BudDirs.Num(); ++i)
			{
				if (i == BudDirLightOptimal || i == BudDirLightSubOptimal)
				{
					continue;
				}
				BudDirs[i] = TotalDownForce * BudDirs[i];
			}
		}
	}
	else
	{
		PreviousPosition = PointPositions[CurrentBranchPoints[0]];
	}

	// --- Process remaining points along the branch ---
	const int32 MaxIndex = CurrentBranchPoints.Num() - 1;
	for (int32 Index = 1; Index <= MaxIndex; ++Index)
	{
		const int32 PointIndex         = CurrentBranchPoints[Index];
		const int32 PreviousPointIndex = CurrentBranchPoints[Index - 1];

		FVector3f& Position = PointPositions[PointIndex];
		PivotPosition = PointPositions[PreviousPointIndex];

		const float LengthFromRoot         = PointFacade.GetLengthFromRoot(PointIndex);
		const float PreviousLengthFromRoot = PointFacade.GetLengthFromRoot(PreviousPointIndex);

		const FVector3f RelativeDirection = TotalDownForce * (Position - PreviousPosition);
		const FVector3f Tangent = RelativeDirection.GetUnsafeNormal();

		const FQuat4f DownForce = CalculateDownForce(
			Tangent, PointIndex, PointFacade.GetPointScaleGradient(PointIndex));
		TotalDownForce = DownForce * TotalDownForce;

		// Update foliage attachment points between current and previous LFR
		const TArray<int32> FoliageEntryIds = FoliageFacade.GetFoliageEntryIdsForBranch(BranchIndex);
		for (const int32 FoliageEntryId : FoliageEntryIds)
		{
			PV::Facades::FFoliageEntryData Entry = FoliageFacade.GetFoliageEntry(FoliageEntryId);

			if (Entry.LengthFromRoot <= LengthFromRoot && Entry.LengthFromRoot > PreviousLengthFromRoot)
			{
				Entry.PivotPoint   = PivotPosition + DownForce * RelativeDirection;
				Entry.UpVector     = TotalDownForce * Entry.UpVector;
				Entry.NormalVector = TotalDownForce * Entry.NormalVector;

				// Apply frame avoidance to foliage pivot
				ApplyFrameAvoidance(Entry.PivotPoint, FrameOBB, Params.AvoidStrength, Params.AvoidMargin);

				FoliageFacade.SetFoliageEntry(FoliageEntryId, Entry);
			}
		}

		// Recurse into children attached between previous and current points
		for (const int32 ChildIndex : ChildrenIndices)
		{
			const TArray<int32>& ChildBranchPoints = BranchFacade.GetPoints(ChildIndex);
			if (ChildBranchPoints.Num() == 0)
			{
				continue;
			}

			const int32 FirstPointIndex = ChildBranchPoints[0];
			const float ChildLFR = PointFacade.GetLengthFromRoot(FirstPointIndex);
			if (ChildLFR <= LengthFromRoot && ChildLFR > PreviousLengthFromRoot)
			[[unlikely]]
			{
				ApplyGravityRecursive(
					ChildIndex,
					Params,
					PhototropicDirections,
					FrameOBB,
					OutCollection,
					TotalDownForce,
					PreviousPosition,
					PivotPosition);
			}
		}

		// Move the point
		PreviousPosition = Position;
		Position = PivotPosition + DownForce * RelativeDirection;

		// Frame mesh avoidance on gravity-displaced position
		ApplyFrameAvoidance(Position, FrameOBB, Params.AvoidStrength, Params.AvoidMargin);

		// Rotate bud directions (skip light optimal/sub-optimal)
		if (PointBudDirections)
		{
			TArray<FVector3f>& BudDirs = (*PointBudDirections)[PointIndex];
			for (int32 i = 0; i < BudDirs.Num(); ++i)
			{
				if (i == BudDirLightOptimal || i == BudDirLightSubOptimal)
				{
					continue;
				}
				BudDirs[i] = TotalDownForce * BudDirs[i];
			}
		}
	}
}

/**
 * Top-level gravity entry point.  Iterates trunks and dispatches the recursive
 * gravity function, skipping frame mesh wireframe branches (hierarchy == 99).
 */
void ApplyGravity(
	const FGravityParams& Params,
	const FOBB& FrameOBB,
	FManagedArrayCollection& OutCollection)
{
	const PV::Facades::FBranchFacade BranchFacade(OutCollection);
	const PV::Facades::FPointFacade  PointFacade(OutCollection);
	const PV::Facades::FPlantFacade  PlantFacade(OutCollection);

	if (!PointFacade.IsValid() || !BranchFacade.IsValid() || !PlantFacade.IsValid())
	{
		return;
	}

	// Generate per-point phototropic direction data if needed
	TArray<FVector3f> PhototropicDirections;
	if (Params.Mode == EPGLGravityMode::Phototropic)
	{
		GeneratePhototropicData(Params, BranchFacade, PointFacade, PhototropicDirections);
	}

	// Process each trunk (root branch), skipping frame mesh wireframe edges
	for (const int32 TrunkIndex : PlantFacade.GetTrunkIndices())
	{
		if (BranchFacade.GetBranchHierarchyNumber(TrunkIndex) == 99)
		{
			continue;
		}

		ApplyGravityRecursive(
			TrunkIndex,
			Params,
			PhototropicDirections,
			FrameOBB,
			OutCollection);
	}
}

} // anonymous namespace

// =============================================================================
//  Editor metadata
// =============================================================================

#if WITH_EDITOR

FText UPGLGravitySettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "PGL Gravity");
}

FText UPGLGravitySettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Applies gravity or phototropic bending to a PVE skeleton.\n\n"
		"Reimplements the native PVE Gravity algorithm with an optional\n"
		"\"Frame Mesh\" input for OBB-based branch avoidance.\n\n"
		"Branches with hierarchy 99 (frame mesh wireframe) are exempt\n"
		"from gravity and remain in place.\n\n"
		"Wire a Growth Generator (or any PVE skeleton) into the default\n"
		"input.  Optionally wire a Viewport Mesh into the Frame Mesh pin.");
}

#endif // WITH_EDITOR

// =============================================================================
//  Pins
// =============================================================================

TArray<FPCGPinProperties> UPGLGravitySettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Props;

	// Default input — skeleton data
	[[maybe_unused]] FPCGPinProperties& InPin = Props.Emplace_GetRef(
		PCGPinConstants::DefaultInputLabel,
		FPCGDataTypeIdentifier{ FPVDataTypeInfoGrowth::AsId() });

	// Optional Frame Mesh input
	[[maybe_unused]] FPCGPinProperties& FramePin = Props.Emplace_GetRef(
		FName(TEXT("Frame Mesh")),
		FPCGDataTypeIdentifier{ FPVDataTypeInfoGrowth::AsId() });
	FramePin.SetAllowMultipleConnections(false);
	FramePin.bAllowMultipleData = false;

	return Props;
}

FPCGElementPtr UPGLGravitySettings::CreateElement() const
{
	return MakeShared<FPGLGravityElement>();
}

// =============================================================================
//  Execution
// =============================================================================

bool FPGLGravityElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPGLGravityElement::Execute);
	check(Context);

	const UPGLGravitySettings* Settings =
		Context->GetInputSettings<UPGLGravitySettings>();
	check(Settings);

	namespace PVG = PV::GroupNames;
	namespace PVA = PV::AttributeNames;

	// -------------------------------------------------------------------------
	//  Read optional Frame Mesh input → extract OBB
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
								"PGL Gravity: Frame Mesh bounding box is degenerate "
								"(zero-length edge). Avoidance disabled."));
					}
				}
				else
				{
					PCGE_LOG(Warning, GraphAndLog,
						LOCTEXT("InsufficientPoints",
							"PGL Gravity: Frame Mesh input has fewer than 24 points. "
							"Expected a Viewport Mesh output. Avoidance disabled."));
				}
			}
		}
	}

	// -------------------------------------------------------------------------
	//  Build gravity parameters
	// -------------------------------------------------------------------------
	FGravityParams Params;
	Params.Mode            = Settings->Mode;
	Params.Gravity         = Settings->Gravity;
	Params.Direction       = Settings->Direction;
	Params.AngleCorrection = Settings->AngleCorrection;
	Params.PhototropicBias = Settings->PhototropicBias;
	Params.AvoidStrength   = FMath::Clamp(Settings->FrameAvoidanceStrength, 0.f, 1.f);
	Params.AvoidMargin     = FMath::Max(0.f, Settings->FrameAvoidanceMargin);

	// -------------------------------------------------------------------------
	//  Process each input skeleton
	// -------------------------------------------------------------------------
	for (const FPCGTaggedData& Input :
		Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel))
	{
		const UPVGrowthData* InputData = Cast<UPVGrowthData>(Input.Data);
		if (!InputData)
		{
			PCGLog::InputOutput::LogInvalidInputDataError(Context);
			return true;
		}

		FManagedArrayCollection OutCollection = InputData->GetCollection();

		if (Settings->Gravity != 0.0f)
		{
			ApplyGravity(Params, FrameOBB, OutCollection);
		}

		// Propagate frame mesh attributes for viewport rendering
		if (!CapturedFrameMeshPath.IsEmpty())
		{
			// AddAttribute returns existing mutable ref if attribute already exists
			TManagedArray<FString>& FMP =
				OutCollection.AddAttribute<FString>(FName("FrameMeshPath"), PVG::DetailsGroup);
			FMP[0] = CapturedFrameMeshPath;

			TManagedArray<FTransform>& FMT =
				OutCollection.AddAttribute<FTransform>(FName("FrameMeshTransform"), PVG::DetailsGroup);
			FMT[0] = CapturedFrameMeshTransform;
		}

		UPVGrowthData* OutData = FPCGContext::NewObject_AnyThread<UPVGrowthData>(Context);
		OutData->Initialize(MoveTemp(OutCollection));

		FPCGTaggedData& D = Context->OutputData.TaggedData.Emplace_GetRef();
		D.Data = OutData;
		D.Pin  = PCGPinConstants::DefaultOutputLabel;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
