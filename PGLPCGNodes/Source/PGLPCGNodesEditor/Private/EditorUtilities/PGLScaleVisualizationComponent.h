// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "SceneView.h"
#include "Components/PrimitiveComponent.h"
#include "Containers/StaticArray.h"
#include "PGLScaleVisualizationComponent.generated.h"

class UTextRenderComponent;
class UPGLLineBatchComponent;

// ---------------------------------------------------------------------------
//  Scene proxy for PGL line batch rendering
// ---------------------------------------------------------------------------
class FPGLLineSceneProxy final : public FPrimitiveSceneProxy
{
public:
	FPGLLineSceneProxy(const UPGLLineBatchComponent* InComponent);

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap,
		FMeshElementCollector& Collector) const override;

	virtual SIZE_T GetTypeHash() const override;
	virtual uint32 GetMemoryFootprint() const override;
	virtual bool CanBeOccluded() const override;

private:
	const TObjectPtr<const UPGLLineBatchComponent> LineComponent;
};

// ---------------------------------------------------------------------------
//  Line info
// ---------------------------------------------------------------------------
struct FPGLLineInfo
{
	FVector StartPos;
	FVector EndPos;
	FLinearColor Color;
	ESceneDepthPriorityGroup DepthPriorityGroup = SDPG_World;
};

// ---------------------------------------------------------------------------
//  Line batch component — draws persistent lines in the viewport
// ---------------------------------------------------------------------------
UCLASS(MinimalAPI)
class UPGLLineBatchComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UPGLLineBatchComponent();

	void InitBounds();
	void AddLine(
		const FVector& InStartPos,
		const FVector& InEndPos,
		const FLinearColor& InColor = FLinearColor::White,
		ESceneDepthPriorityGroup InDepthPriorityGroup = SDPG_World);

	void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) const;
	void Flush();

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

protected:
	FBox BBox;
	TArray<FPGLLineInfo> Lines;
};

// ---------------------------------------------------------------------------
//  Scale visualization — draws 3 coloured axis lines with dimension labels
// ---------------------------------------------------------------------------
UCLASS(MinimalAPI)
class UPGLScaleVisualizationComponent : public UPGLLineBatchComponent
{
	GENERATED_BODY()

public:
	UPGLScaleVisualizationComponent();

	void SetScaleBounds(const FBoxSphereBounds& InBounds);
	const TStaticArray<TObjectPtr<UTextRenderComponent>, 3>& GetTextRenderComponents() const;

	virtual void OnRegister() override;

	void UpdateScaleVisualizations();

private:
	FBoxSphereBounds ScaleBounds;
	TStaticArray<TObjectPtr<UTextRenderComponent>, 3> TextRenderComponents = { nullptr, nullptr, nullptr };
};
