// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGNodes/PGLBaseSettings.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Curves/CurveFloat.h"
#include "PCGGrowthGenerator.generated.h"

/** One entry in the foliage palette exposed on the Growth Generator node. */
USTRUCT(BlueprintType)
struct FPCGFoliageEntry
{
	GENERATED_BODY()

	/** Static mesh to scatter along branches. Leave null to disable this entry. */
	UPROPERTY(EditAnywhere, Category = "Foliage")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/**
	 * Number of mesh instances to place per 100 cm of branch length.
	 * Set to 0 to disable this entry entirely.
	 */
	UPROPERTY(EditAnywhere, Category = "Foliage",
		meta = (ClampMin = 0.0f, UIMin = 0.0f, UIMax = 20.0f))
	float DensityPer100cm = 1.0f;

	/** Uniform scale applied to every placed instance. */
	UPROPERTY(EditAnywhere, Category = "Foliage",
		meta = (ClampMin = 0.01f, UIMin = 0.1f, UIMax = 10.0f))
	float Scale = 1.0f;

	/**
	 * Minimum branch hierarchy level on which this mesh is placed.
	 * 0 = trunk and all branches.  1 = 1st-order branches and above.  2 = tips only.
	 */
	UPROPERTY(EditAnywhere, Category = "Foliage",
		meta = (ClampMin = 0, ClampMax = 2, UIMin = 0, UIMax = 2))
	int32 MinHierarchyLevel = 1;
};


/**
 * Procedurally generates a UPVGrowthData skeleton (trunk + optional hierarchical
 * sub-branches) from in-node parameters, without requiring a PVE plant asset.
 *
 * The output connects directly to PVE's Mesh Builder input pin in any PCG graph,
 * enabling fully custom tree shapes driven by parameter tweaks rather than a
 * PVE growth simulation.
 *
 * Typical setup:
 *   1. Add this node as a source (no input pin required).
 *   2. Set TrunkLength, radii, and BranchGenerations to taste.
 *   3. Wire the output to PVE's Mesh Builder "Skeleton" pin.
 *   4. Optionally chain through PVCutTrunk / PVScaleBranches / PVTrunkTips.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PGLPCGNODES_API UPCGGrowthGeneratorSettings : public UPGLBaseSettings
{
	GENERATED_BODY()

public:

	UPCGGrowthGeneratorSettings();

#if WITH_EDITOR
	virtual FName            GetDefaultNodeName()  const override { return FName(TEXT("GrowthGenerator")); }
	virtual FText            GetDefaultNodeTitle() const override;
	virtual FText            GetNodeTooltipText()  const override;
	virtual FLinearColor     GetNodeTitleColor()   const override { return FLinearColor(0.1f, 0.6f, 0.1f, 1.f); }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties()  const override;
	virtual FPCGElementPtr            CreateElement()       const override;

public:

	// -------------------------------------------------------------------------
	//  Trunk
	// -------------------------------------------------------------------------

	/** Total trunk height in centimetres. */
	UPROPERTY(EditAnywhere, Category = "Trunk",
		meta = (ClampMin = 1.0f, UIMin = 10.0f, UIMax = 2000.0f))
	float TrunkLength = 300.0f;

	/** Cross-section radius at the base of the trunk. */
	UPROPERTY(EditAnywhere, Category = "Trunk",
		meta = (ClampMin = 0.1f, UIMin = 0.5f, UIMax = 200.0f))
	float TrunkBaseRadius = 20.0f;

	/** Cross-section radius at the top of the trunk. Clamped to <= TrunkBaseRadius. */
	UPROPERTY(EditAnywhere, Category = "Trunk",
		meta = (ClampMin = 0.0f, UIMin = 0.0f, UIMax = 200.0f))
	float TrunkTipRadius = 5.0f;

	/**
	 * Controls how the trunk radius transitions from TrunkBaseRadius to TrunkTipRadius.
	 * X = position along trunk (0 = base, 1 = tip).
	 * Y = interpolation factor (0 = base radius, 1 = tip radius).
	 * Default: linear (0,0) → (1,1) — standard conical taper.
	 * Reshape for barrel trunks, wine-bottle profiles, etc.
	 */
	UPROPERTY(EditAnywhere, Category = "Trunk")
	FRuntimeFloatCurve TrunkTaperCurve;

	/** Number of segments along the trunk. Point count = Segments + 1. */
	UPROPERTY(EditAnywhere, Category = "Trunk",
		meta = (ClampMin = 2, UIMin = 2, UIMax = 32))
	int32 TrunkSegments = 8;

	/**
	 * Lateral offset (cm) at the trunk tip relative to the base.
	 * 0 = perfectly vertical.
	 */
	UPROPERTY(EditAnywhere, Category = "Trunk",
		meta = (UIMin = -500.0f, UIMax = 500.0f))
	float TrunkLean = 0.0f;

	/**
	 * XY-plane direction of the lean. Normalised internally.
	 * Only used when TrunkLean != 0.
	 */
	UPROPERTY(EditAnywhere, Category = "Trunk")
	FVector2D TrunkLeanAxis = FVector2D(1.0, 0.0);

	/**
	 * Maximum angular deviation per trunk segment (degrees).
	 * Each segment is randomly rotated within a cone of this half-angle from the
	 * previous segment's direction, creating a natural organic sweep along the trunk.
	 * 0 = perfectly straight trunk.
	 */
	UPROPERTY(EditAnywhere, Category = "Trunk",
		meta = (ClampMin = 0.0f, UIMin = 0.0f, UIMax = 30.0f))
	float TrunkCurvatureStrength = 5.0f;

	// -------------------------------------------------------------------------
	//  Branches
	// -------------------------------------------------------------------------

	/**
	 * Number of branch hierarchy levels to generate.
	 * 0 = trunk only, 1 = trunk + 1st-order branches, 2 = … + 2nd-order.
	 */
	UPROPERTY(EditAnywhere, Category = "Branches",
		meta = (ClampMin = 0, ClampMax = 2, UIMin = 0, UIMax = 2))
	int32 BranchGenerations = 1;

	/** Number of child branches spawned per parent branch. */
	UPROPERTY(EditAnywhere, Category = "Branches",
		meta = (ClampMin = 1, UIMin = 1, UIMax = 8,
			EditCondition = "BranchGenerations > 0"))
	int32 BranchesPerNode = 3;

	/** Child branch length as a fraction of its parent's length. */
	UPROPERTY(EditAnywhere, Category = "Branches",
		meta = (ClampMin = 0.05f, ClampMax = 1.0f, UIMin = 0.1f, UIMax = 1.0f,
			EditCondition = "BranchGenerations > 0"))
	float BranchLengthScale = 0.5f;

	/**
	 * Cross-section radius (cm) at the base of 1st-order branches.
	 * Set independently of TrunkBaseRadius so trunk and branch thickness
	 * can be dialled in separately.
	 */
	UPROPERTY(EditAnywhere, Category = "Branches",
		meta = (ClampMin = 0.1f, UIMin = 0.5f, UIMax = 200.0f,
			EditCondition = "BranchGenerations > 0"))
	float BranchBaseRadius = 7.0f;

	/**
	 * Cross-section radius (cm) at the tip of 1st-order branches.
	 * Clamped to <= BranchBaseRadius internally.
	 * The resulting taper ratio (tip/base) is inherited by all branch generations.
	 */
	UPROPERTY(EditAnywhere, Category = "Branches",
		meta = (ClampMin = 0.0f, UIMin = 0.0f, UIMax = 200.0f,
			EditCondition = "BranchGenerations > 0"))
	float BranchTipRadius = 1.75f;

	/**
	 * Base radius of each successive branch generation as a fraction of its
	 * parent's base radius.  Only affects 2nd-order branches and beyond —
	 * 1st-order base radius is set directly via BranchBaseRadius above.
	 */
	UPROPERTY(EditAnywhere, Category = "Branches",
		meta = (ClampMin = 0.05f, ClampMax = 1.0f, UIMin = 0.05f, UIMax = 1.0f,
			EditCondition = "BranchGenerations > 1"))
	float BranchRadiusScale = 0.35f;

	/** Number of segments per branch. Point count = Segments + 1. */
	UPROPERTY(EditAnywhere, Category = "Branches",
		meta = (ClampMin = 2, UIMin = 2, UIMax = 16,
			EditCondition = "BranchGenerations > 0"))
	int32 BranchSegments = 4;

	/**
	 * Half-angle (degrees) of the cone used to orient branches away from their
	 * parent's growth direction. Larger values give more horizontal branching.
	 */
	UPROPERTY(EditAnywhere, Category = "Branches",
		meta = (ClampMin = 5.0f, ClampMax = 90.0f, UIMin = 5.0f, UIMax = 90.0f,
			EditCondition = "BranchGenerations > 0"))
	float BranchSpreadAngle = 45.0f;

	/** Minimum attachment fraction along the parent branch (0 = root end, 1 = tip). */
	UPROPERTY(EditAnywhere, Category = "Branches",
		meta = (ClampMin = 0.0f, ClampMax = 1.0f, UIMin = 0.0f, UIMax = 1.0f,
			EditCondition = "BranchGenerations > 0"))
	float BranchAttachMin = 0.2f;

	/** Maximum attachment fraction along the parent branch (0 = root end, 1 = tip). */
	UPROPERTY(EditAnywhere, Category = "Branches",
		meta = (ClampMin = 0.0f, ClampMax = 1.0f, UIMin = 0.0f, UIMax = 1.0f,
			EditCondition = "BranchGenerations > 0"))
	float BranchAttachMax = 0.8f;

	/**
	 * Maximum angular deviation per branch segment (degrees).
	 * Same random-walk mechanism as TrunkCurvatureStrength, applied to every sub-branch.
	 * Branches typically look more natural with a slightly higher value than the trunk.
	 * 0 = straight branches.
	 */
	UPROPERTY(EditAnywhere, Category = "Branches",
		meta = (ClampMin = 0.0f, UIMin = 0.0f, UIMax = 30.0f,
			EditCondition = "BranchGenerations > 0"))
	float BranchCurvatureStrength = 8.0f;

	/** Deterministic random seed. Different values give different branch layouts. */
	UPROPERTY(EditAnywhere, Category = "Branches")
	int32 RandomSeed = 42;

	// -------------------------------------------------------------------------
	//  Material
	// -------------------------------------------------------------------------

	/**
	 * Bark materials per branch generation.
	 *   [0] = trunk material  (written to TrunkMaterialPath — the ONE path the PVE
	 *          Mesh Builder loads when bOverrideMaterial = false)
	 *   [1] = 1st-order branch material  (stored; enables correct generation-slot count)
	 *   [2] = 2nd-order / tip material   (stored; enables correct generation-slot count)
	 *
	 * The number of entries controls how many UV material slots the Mesh Builder creates,
	 * so per-generation branchUVMat assignment (Repeat / Fit / Generation mode) works
	 * correctly without any extra Mesh Builder configuration.
	 *
	 * Note: the PVE skeleton schema stores only one material path, so the Mesh Builder
	 * applies GenerationMaterials[0] to ALL generations when bOverrideMaterial is false.
	 * For distinct materials per generation, enable bOverrideMaterial on the Mesh Builder
	 * node and configure it directly — the generation slot count will already be correct.
	 */
	UPROPERTY(EditAnywhere, Category = "Material")
	TArray<TSoftObjectPtr<UMaterialInterface>> GenerationMaterials;

	// -------------------------------------------------------------------------
	//  Foliage
	// -------------------------------------------------------------------------

	/**
	 * Foliage meshes to scatter along branches.
	 * Each entry specifies a mesh, density (per 100 cm), scale, and minimum
	 * branch hierarchy level.  Leave empty for a bare skeleton.
	 */
	UPROPERTY(EditAnywhere, Category = "Foliage")
	TArray<FPCGFoliageEntry> FoliageEntries;

	// -------------------------------------------------------------------------
	//  Phototropism
	// -------------------------------------------------------------------------

	/**
	 * World-space direction toward the primary light source (e.g. the sun).
	 * Normalised internally.  Stored in BudDirection[2] (light optimal) and
	 * BudDirection[3] (light sub-optimal) per skeleton point.
	 *
	 * Downstream PVE Gravity / Simulation nodes read these directions and blend
	 * branch growth toward light using their own PhototropicBias parameter.
	 * PVE Foliage Distributor also uses [2] to orient leaf normals toward light.
	 *
	 * (0, 0, 1) = overhead sun (no horizontal bias, the default).
	 */
	UPROPERTY(EditAnywhere, Category = "Phototropism")
	FVector LightDirection = FVector(0.0, 0.0, 1.0);

	// -------------------------------------------------------------------------
	//  Growth
	// -------------------------------------------------------------------------

	/**
	 * Ethylene level per branch hierarchy (index 0 = trunk, 1 = 1st-order,
	 * 2 = 2nd-order).  Range 0–1.  Stored in BudHormoneLevels[4] per point.
	 *
	 * PVE Foliage Distributor places foliage only where:
	 *   EthyleneLevel < EthyleneThreshold   (set on the Foliage Distributor node)
	 *
	 * High ethylene → foliage suppressed (old / interior wood).
	 * Low ethylene  → foliage placed (young / exterior shoot tips).
	 *
	 * Defaults represent a typical broadleaf tree: no foliage on the trunk,
	 * sparse on 1st-order branches, dense on 2nd-order shoot tips.
	 * Raise the EthyleneThreshold on the Foliage Distributor to 0.6–0.9 to
	 * see the gradient take effect.
	 */
	UPROPERTY(EditAnywhere, Category = "Growth",
		meta = (ClampMin = 0.0f, ClampMax = 1.0f, UIMin = 0.0f, UIMax = 1.0f))
	TArray<float> EthyleneLevels = TArray<float>{ 0.9f, 0.5f, 0.1f };

	// -------------------------------------------------------------------------
	//  Frame Mesh Avoidance
	// -------------------------------------------------------------------------

	/**
	 * Blend factor for direction correction when branches enter the frame mesh's
	 * influence radius.  0 = no avoidance (growth passes through the mesh),
	 * 1 = maximum deflection away from the mesh surface.
	 *
	 * Only takes effect when the "Frame Mesh" input pin is connected.
	 */
	UPROPERTY(EditAnywhere, Category = "Frame Mesh",
		meta = (ClampMin = 0.0f, ClampMax = 1.0f, UIMin = 0.0f, UIMax = 1.0f))
	float FrameAvoidanceStrength = 0.8f;

	/**
	 * Minimum clearance (cm) from the mesh surface.  Branches within this
	 * distance are gradually steered away; branches that penetrate the mesh
	 * are hard-projected to the nearest face plus this margin.
	 *
	 * Only takes effect when the "Frame Mesh" input pin is connected.
	 */
	UPROPERTY(EditAnywhere, Category = "Frame Mesh",
		meta = (ClampMin = 0.0f, UIMin = 0.0f, UIMax = 100.0f))
	float FrameAvoidanceMargin = 10.0f;
};


class PGLPCGNODES_API FPCGGrowthGeneratorElement : public IPCGElement
{
public:
	/** No world access needed — can run on any thread. */
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return false; }
	/** Output depends on settings only; safe to cache when settings hash matches. */
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
