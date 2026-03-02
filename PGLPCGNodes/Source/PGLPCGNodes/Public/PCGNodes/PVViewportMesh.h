// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGNodes/PGLBaseSettings.h"
#include "Engine/StaticMesh.h"
#include "PVViewportMesh.generated.h"

/**
 * Outputs a PVGrowthData skeleton representing a static mesh's bounding box as
 * 12 wireframe edges.  The mesh reference is stored for future use by growth
 * constraint nodes.
 *
 * This is a source node (no inputs).  Wire its output into any PVE node that
 * accepts a Growth skeleton to see the 12-edge wireframe in the PVE viewport.
 *
 * Typical setup:
 *   1. Add this node to the PVE graph.
 *   2. Assign a static mesh (e.g. a wall, rock, or blocking volume).
 *   3. Set MeshTransform to position it relative to the tree origin.
 *   4. Wire the output to a downstream PVE node and inspect that node.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PGLPCGNODES_API UPVViewportMeshSettings : public UPGLBaseSettings
{
	GENERATED_BODY()

public:

#if WITH_EDITOR
	virtual FName            GetDefaultNodeName()  const override { return FName(TEXT("ViewportMesh")); }
	virtual FText            GetDefaultNodeTitle() const override;
	virtual FText            GetNodeTooltipText()  const override;
	virtual FLinearColor     GetNodeTitleColor()   const override { return FLinearColor(0.8f, 0.4f, 0.1f, 1.f); }

	virtual TArray<EPVRenderType> GetDefaultRenderType()    const override { return { EPVRenderType::PointData, EPVRenderType::Mesh }; }
	virtual TArray<EPVRenderType> GetSupportedRenderTypes() const override { return { EPVRenderType::PointData, EPVRenderType::Mesh }; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties()  const override;
	virtual FPCGElementPtr            CreateElement()       const override;

public:

	// -------------------------------------------------------------------------
	//  Mesh
	// -------------------------------------------------------------------------

	/** Static mesh whose bounding box is displayed in the PVE viewport. */
	UPROPERTY(EditAnywhere, Category = "Mesh")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/**
	 * Local transform applied to the mesh bounding box.
	 * Position, rotation, and scale are all relative to the tree origin (world zero
	 * in the PVE preview scene).
	 */
	UPROPERTY(EditAnywhere, Category = "Mesh")
	FTransform MeshTransform = FTransform::Identity;
};


class PGLPCGNODES_API FPVViewportMeshElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return false; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
