// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "EditorUtilities/PGLScaleVisualizationComponent.h"

#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "MeshElementCollector.h"
#include "PrimitiveDrawInterface.h"

// =============================================================================
//  Scene Proxy
// =============================================================================

FPGLLineSceneProxy::FPGLLineSceneProxy(const UPGLLineBatchComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
	, LineComponent(InComponent)
{
}

FPrimitiveViewRelevance FPGLLineSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance ViewRelevance;
	ViewRelevance.bDrawRelevance = IsShown(View);
	ViewRelevance.bDynamicRelevance = true;
	ViewRelevance.bSeparateTranslucency = ViewRelevance.bNormalTranslucency = true;
	return ViewRelevance;
}

void FPGLLineSceneProxy::GetDynamicMeshElements(
	const TArray<const FSceneView*>& Views,
	const FSceneViewFamily& ViewFamily,
	uint32 VisibilityMap,
	FMeshElementCollector& Collector) const
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];

#if UE_ENABLE_DEBUG_DRAWING
			FPrimitiveDrawInterface* PDI = Collector.GetDebugPDI(ViewIndex);
#else
			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
#endif

			LineComponent->Draw(View, PDI);
		}
	}
}

SIZE_T FPGLLineSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

uint32 FPGLLineSceneProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + GetAllocatedSize();
}

bool FPGLLineSceneProxy::CanBeOccluded() const
{
	return false;
}

// =============================================================================
//  UPGLLineBatchComponent
// =============================================================================

UPGLLineBatchComponent::UPGLLineBatchComponent()
{
	bAutoActivate = true;
	bTickInEditor = true;
	PrimaryComponentTick.bCanEverTick = true;
	bUseEditorCompositing = true;
	SetGenerateOverlapEvents(false);
	bIgnoreStreamingManagerUpdate = true;

	BBox = FBox(ForceInit);
	Bounds = FBoxSphereBounds(FVector::ZeroVector, FVector::OneVector, 1);
}

void UPGLLineBatchComponent::InitBounds()
{
	BBox = FBox(ForceInit);
}

void UPGLLineBatchComponent::AddLine(
	const FVector& InStartPos,
	const FVector& InEndPos,
	const FLinearColor& InColor,
	ESceneDepthPriorityGroup InDepthPriorityGroup)
{
	Lines.Emplace(FPGLLineInfo{ InStartPos, InEndPos, InColor, InDepthPriorityGroup });

	BBox += InStartPos;
	BBox += InEndPos;
}

void UPGLLineBatchComponent::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) const
{
	for (const FPGLLineInfo& Line : Lines)
	{
		PDI->DrawLine(
			Line.StartPos,
			Line.EndPos,
			Line.Color,
			Line.DepthPriorityGroup,
			3.0f,
			0,
			true);
	}
}

void UPGLLineBatchComponent::Flush()
{
	Lines.Empty();
	BBox = FBox(ForceInit);
}

FPrimitiveSceneProxy* UPGLLineBatchComponent::CreateSceneProxy()
{
	return new FPGLLineSceneProxy(this);
}

FBoxSphereBounds UPGLLineBatchComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return FBoxSphereBounds(BBox).TransformBy(LocalToWorld);
}

// =============================================================================
//  UPGLScaleVisualizationComponent
// =============================================================================

UPGLScaleVisualizationComponent::UPGLScaleVisualizationComponent()
	: ScaleBounds(ForceInit)
{
	int32 ComponentIndex = 0;
	for (TObjectPtr<UTextRenderComponent>& Component : TextRenderComponents)
	{
		const FString ComponentName = TEXT("ScaleTextRenderComponent") + FString::FromInt(ComponentIndex++);
		Component = CreateDefaultSubobject<UTextRenderComponent>(*ComponentName);
		Component->SetGenerateOverlapEvents(false);
		Component->SetupAttachment(this);
	}
}

void UPGLScaleVisualizationComponent::SetScaleBounds(const FBoxSphereBounds& InBounds)
{
	ScaleBounds = InBounds;
	UpdateScaleVisualizations();
}

const TStaticArray<TObjectPtr<UTextRenderComponent>, 3>& UPGLScaleVisualizationComponent::GetTextRenderComponents() const
{
	return TextRenderComponents;
}

void UPGLScaleVisualizationComponent::OnRegister()
{
	Super::OnRegister();
	UpdateScaleVisualizations();
}

void UPGLScaleVisualizationComponent::UpdateScaleVisualizations()
{
	if (!GetWorld())
	{
		return;
	}

	Flush();

	const auto WorldToMetersScale = GetWorld()->GetWorldSettings()->WorldToMeters;
	const double TextSize =
		(ScaleBounds.BoxExtent.X + ScaleBounds.BoxExtent.Y + ScaleBounds.BoxExtent.Z)
		* 20.0f / 3.0f / WorldToMetersScale;

	// X axis — Red
	{
		FVector StartPosition = ScaleBounds.Origin;
		StartPosition.X -= ScaleBounds.BoxExtent.X;
		StartPosition.Y -= ScaleBounds.BoxExtent.Y;
		StartPosition.Z -= ScaleBounds.BoxExtent.Z;

		FVector EndPosition = ScaleBounds.Origin;
		EndPosition.X += ScaleBounds.BoxExtent.X;
		EndPosition.Y -= ScaleBounds.BoxExtent.Y;
		EndPosition.Z -= ScaleBounds.BoxExtent.Z;

		const double Length = (ScaleBounds.BoxExtent.X * 2.0f) / WorldToMetersScale;
		UTextRenderComponent* TextComp = TextRenderComponents[0];

		TextComp->SetWorldLocation(EndPosition);
		TextComp->SetWorldRotation(FRotator(0, 90, 0));
		TextComp->SetTextRenderColor(FLinearColor::Red.ToFColorSRGB());
		TextComp->SetHorizontalAlignment(EHTA_Left);
		TextComp->SetVerticalAlignment(EVRTA_TextBottom);
		TextComp->SetWorldSize(TextSize);
		TextComp->SetText(FText::FromString(FString::Printf(TEXT("%.2fm"), Length)));

		AddLine(StartPosition, EndPosition, FLinearColor::Red);
	}

	// Y axis — Green
	{
		FVector StartPosition = ScaleBounds.Origin;
		StartPosition.X -= ScaleBounds.BoxExtent.X;
		StartPosition.Y -= ScaleBounds.BoxExtent.Y;
		StartPosition.Z -= ScaleBounds.BoxExtent.Z;

		FVector EndPosition = ScaleBounds.Origin;
		EndPosition.X -= ScaleBounds.BoxExtent.X;
		EndPosition.Y += ScaleBounds.BoxExtent.Y;
		EndPosition.Z -= ScaleBounds.BoxExtent.Z;

		const double Length = (ScaleBounds.BoxExtent.Y * 2.0f) / WorldToMetersScale;
		UTextRenderComponent* TextComp = TextRenderComponents[1];

		TextComp->SetWorldLocation(EndPosition);
		TextComp->SetTextRenderColor(FColor::Green);
		TextComp->SetHorizontalAlignment(EHTA_Right);
		TextComp->SetVerticalAlignment(EVRTA_TextBottom);
		TextComp->SetWorldSize(TextSize);
		TextComp->SetText(FText::FromString(FString::Printf(TEXT("%.2fm"), Length)));

		AddLine(StartPosition, EndPosition, FLinearColor::Green);
	}

	// Z axis — Blue
	{
		FVector StartPosition = ScaleBounds.Origin;
		StartPosition.X -= ScaleBounds.BoxExtent.X;
		StartPosition.Y -= ScaleBounds.BoxExtent.Y;
		StartPosition.Z -= ScaleBounds.BoxExtent.Z;

		FVector EndPosition = ScaleBounds.Origin;
		EndPosition.X -= ScaleBounds.BoxExtent.X;
		EndPosition.Y -= ScaleBounds.BoxExtent.Y;
		EndPosition.Z += ScaleBounds.BoxExtent.Z;

		const double Length = (ScaleBounds.BoxExtent.Z * 2.0f) / WorldToMetersScale;
		UTextRenderComponent* TextComp = TextRenderComponents[2];

		TextComp->SetWorldLocation(EndPosition);
		TextComp->SetWorldRotation(FRotator(0, 45, 0));
		TextComp->SetTextRenderColor(FLinearColor::Blue.ToFColorSRGB());
		TextComp->SetHorizontalAlignment(EHTA_Center);
		TextComp->SetVerticalAlignment(EVRTA_TextBottom);
		TextComp->SetWorldSize(TextSize);
		TextComp->SetText(FText::FromString(FString::Printf(TEXT("%.2fm"), Length)));

		AddLine(StartPosition, EndPosition, FLinearColor::Blue);
	}
}
