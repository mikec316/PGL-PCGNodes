// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGNodes/PGLBaseSettings.h"
#include "PGLGravity.generated.h"

/**
 * Gravity behaviour mode — mirrors the native PVE Gravity node's EGravityMode
 * but defined locally since the PVE enum is not exported.
 */
UENUM(BlueprintType)
enum class EPGLGravityMode : uint8
{
	/** Branches bend under simulated gravitational pull. */
	Gravity,
	/** Branches orient toward light (phototropism). */
	Phototropic
};

/**
 * Applies gravity / phototropic bending to a PVGrowthData skeleton, optionally
 * constraining branch positions around an OBB frame mesh.
 *
 * This is a reimplementation of the native PVE Gravity node with two additions:
 *   1. An optional "Frame Mesh" input pin that provides an OBB for branch avoidance.
 *   2. Branches with BranchHierarchyNumber == 99 (frame mesh wireframe edges) are
 *      exempt from gravity — they stay in place while the tree bends.
 *
 * Typical setup:
 *   1. Wire Growth Generator (or any PVE skeleton) into the default input.
 *   2. Optionally wire a Viewport Mesh into "Frame Mesh".
 *   3. Adjust Gravity strength, Direction, AngleCorrection.
 *   4. Wire the output into the Mesh Builder or further PVE nodes.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PGLPCGNODES_API UPGLGravitySettings : public UPGLBaseSettings
{
	GENERATED_BODY()

public:

#if WITH_EDITOR
	virtual FName            GetDefaultNodeName()  const override { return FName(TEXT("PGLGravity")); }
	virtual FText            GetDefaultNodeTitle() const override;
	virtual FText            GetNodeTooltipText()  const override;
	virtual FLinearColor     GetNodeTitleColor()   const override { return FLinearColor::Yellow; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties()  const override;
	virtual FPCGElementPtr            CreateElement()       const override;

public:

	// -------------------------------------------------------------------------
	//  Gravity
	// -------------------------------------------------------------------------

	/**
	 * Selects gravity behaviour.
	 * Gravity: branches bend under a directional force (default: world down).
	 * Phototropic: branches orient toward light directions stored in bud vectors.
	 */
	UPROPERTY(EditAnywhere, Category = "Gravity")
	EPGLGravityMode Mode = EPGLGravityMode::Gravity;

	/**
	 * Strength of gravity / phototropic effect (0 = none, 10 = extreme).
	 * Higher values yield more pronounced branch curvature.
	 */
	UPROPERTY(EditAnywhere, Category = "Gravity",
		meta = (ClampMin = 0.0f, ClampMax = 10.0f, UIMin = 0.0f, UIMax = 1.0f))
	float Gravity = 0.0f;

	/**
	 * Direction vector for gravity bending (e.g. world down).
	 * Only used when Mode == Gravity.
	 */
	UPROPERTY(EditAnywhere, Category = "Gravity",
		meta = (EditCondition = "Mode == EPGLGravityMode::Gravity"))
	FVector3f Direction = FVector3f(0.f, 0.f, -1.f);

	/**
	 * Preserves the initial branch angle during bending.
	 * 0 = no correction (full gravity). 1 = maximum angle preservation.
	 */
	UPROPERTY(EditAnywhere, Category = "Gravity",
		meta = (ClampMin = 0.0f, ClampMax = 1.0f, UIMin = 0.0f, UIMax = 1.0f))
	float AngleCorrection = 0.0f;

	/**
	 * Bias between light-optimal (0) and light-sub-optimal / shadow avoidance (1).
	 * Only used when Mode == Phototropic.
	 */
	UPROPERTY(EditAnywhere, Category = "Gravity",
		meta = (ClampMin = 0.0f, ClampMax = 1.0f, UIMin = 0.0f, UIMax = 1.0f,
			EditCondition = "Mode == EPGLGravityMode::Phototropic"))
	float PhototropicBias = 0.0f;

	// -------------------------------------------------------------------------
	//  Frame Mesh Avoidance
	// -------------------------------------------------------------------------

	/**
	 * Blend factor for direction correction when gravity-displaced branches enter
	 * the frame mesh's influence radius.
	 * 0 = no avoidance (branches pass through).
	 * 1 = maximum deflection away from the mesh surface.
	 *
	 * Only takes effect when the "Frame Mesh" input pin is connected.
	 */
	UPROPERTY(EditAnywhere, Category = "Frame Mesh",
		meta = (ClampMin = 0.0f, ClampMax = 1.0f, UIMin = 0.0f, UIMax = 1.0f))
	float FrameAvoidanceStrength = 0.8f;

	/**
	 * Minimum clearance (cm) from the mesh surface.  Branches within this
	 * distance are gradually pushed away; branches that penetrate the mesh
	 * are hard-projected to the nearest face plus this margin.
	 *
	 * Only takes effect when the "Frame Mesh" input pin is connected.
	 */
	UPROPERTY(EditAnywhere, Category = "Frame Mesh",
		meta = (ClampMin = 0.0f, UIMin = 0.0f, UIMax = 100.0f))
	float FrameAvoidanceMargin = 10.0f;
};


class PGLPCGNODES_API FPGLGravityElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return false; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
