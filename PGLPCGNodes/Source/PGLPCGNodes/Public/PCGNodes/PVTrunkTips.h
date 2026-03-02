// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGNodes/PGLBaseSettings.h"
#include "DataTypes/PVGrowthData.h"  // FPVDataTypeInfoGrowth
#include "DataTypes/PVMeshData.h"    // FPVDataTypeInfoMesh
#include "PVTrunkTips.generated.h"

/**
 * Post-process node that appends a flat disk cap at each trunk's cut cross-section
 * to seal the open hole left by the Cut Trunk node.
 *
 * Sits AFTER the standard Mesh Builder in the PVE graph:
 *     Cut Trunk  →  Mesh Builder  →  Trunk Tips  →  Output
 *
 * "In"       pin  :  UPVMeshData   — mesh produced by the Mesh Builder.
 * "Skeleton" pin  :  UPVGrowthData — the same skeleton used by the Mesh Builder,
 *                                    needed to locate trunk tip position, radius
 *                                    and direction.
 * Output          :  UPVMeshData   — the mesh with a cap disk appended.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PGLPCGNODES_API UPVTrunkTipsSettings : public UPGLBaseSettings
{
	GENERATED_BODY()

public:

#if WITH_EDITOR
	virtual FName        GetDefaultNodeName()   const override { return FName(TEXT("TrunkTips")); }
	virtual FText        GetDefaultNodeTitle()  const override;
	virtual FText        GetNodeTooltipText()   const override;
	virtual FLinearColor GetNodeTitleColor()    const override { return FLinearColor(FColor::Cyan); }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties()  const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;

	/** Name of the secondary input pin that carries the UPVGrowthData skeleton. */
	static const FName SkeletonPinLabel;

public:

	/** When true, a flat disk cap is appended at the TOP of the cut trunk (the CutLength end). */
	UPROPERTY(EditAnywhere, Category = "Cap Settings",
		meta = (Tooltip = "Adds a cap disk at the top cut face (CutLength end) to seal the hole left by the Cut Trunk node."))
	bool bAddCap = true;

	/**
	 * When true, also caps the BOTTOM of the cut trunk (the TrunkStart end).
	 * Enable this when Cut Trunk has a non-zero TrunkStart to produce a fully
	 * sealed floating chunk for gameplay interactions such as chopping.
	 */
	UPROPERTY(EditAnywhere, Category = "Cap Settings",
		meta = (Tooltip = "Adds a cap disk at the bottom cut face (TrunkStart end). Enable when Cut Trunk has TrunkStart > 0 to fully seal a floating trunk chunk."))
	bool bAddBottomCap = false;

	/** Number of radial segments for cap disks. Higher = smoother circle. */
	UPROPERTY(EditAnywhere, Category = "Cap Settings",
		meta = (EditCondition = "bAddCap || bAddBottomCap", ClampMin = 3, UIMin = 3, UIMax = 64,
			Tooltip = "Number of radial segments around each cap disk."))
	int32 CapDivisions = 8;

	/**
	 * Material slot index assigned to the cap disk triangles.
	 *
	 * Slot 0 re-uses the bark material slot (default).  Set to 1, 2, … if you
	 * have a spare slot available on the spawned GeometryCollection component and
	 * want a separate draw call for the cap.
	 *
	 * NOTE: PVE's plant asset editor does not expose extra material slots.
	 * If you cannot assign a material to the target slot via PVE or a PCG
	 * material override, use CapVertexColor instead — it works without any
	 * extra slots.
	 */
	UPROPERTY(EditAnywhere, Category = "Cap Settings",
		meta = (EditCondition = "bAddCap || bAddBottomCap", ClampMin = 0, UIMin = 0, UIMax = 8,
			Tooltip = "Material slot index for cap disk triangles. Only useful if the spawned component has a material assigned to this slot. Use CapVertexColor for a slot-free alternative."))
	int32 CapMaterialSlot = 0;

	/**
	 * Vertex colour written to every cap disk vertex.
	 *
	 * A slot-free way to drive a different look for the cap inside the existing
	 * bark material — no extra PVE material slots needed.
	 *
	 * All original trunk/bark vertices have vertex colour (0,0,0,0).
	 * Cap vertices receive this value, so a simple mask in the material works:
	 *
	 *     Lerp( BarkTexture, CapTexture, VertexColor.R )
	 *
	 * Default (1,0,0,1) → R=1 on cap, R=0 on bark.  Adjust channels to taste.
	 * Set all components to 0 to disable vertex colour marking.
	 */
	UPROPERTY(EditAnywhere, Category = "Cap Settings",
		meta = (EditCondition = "bAddCap || bAddBottomCap",
			Tooltip = "Vertex colour written to cap disk vertices (trunk vertices are left at 0,0,0,0). Sample Vertex Color in your bark material to differentiate cap faces without needing an extra material slot."))
	FLinearColor CapVertexColor = FLinearColor(1.f, 0.f, 0.f, 1.f);
};

class PGLPCGNODES_API FPVTrunkTipsElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
