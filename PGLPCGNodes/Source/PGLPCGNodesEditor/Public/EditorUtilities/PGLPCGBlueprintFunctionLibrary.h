// Copyright by Procgen Labs Ltd. All Rights Reserved.

// PGLPCGBlueprintFunctionLibrary.h

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ZoneShapeComponent.h"

#include "PGLPCGBlueprintFunctionLibrary.generated.h"

UCLASS()
class  UPGLPCGBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Creates an intersection for a spline shape.
	 * @param ShapeComp - The Zone Shape Component to modify.
	 * @param DestroyCoveredShape - Whether to destroy the covered shape.
	 */
	UFUNCTION(BlueprintCallable, Category = "Zone Shape")
	static void PGLCreateIntersectionForSplineShape(
		UZoneShapeComponent* ShapeComp,
		UZoneShapeComponent* TargetShapeComp,
		FVector Intersection,
		bool DestroyCoveredShape = true
	);




	UFUNCTION(BlueprintCallable, Category = "Zone Shape")
	static TArray<UZoneShapeComponent*> PGLBreakZoneShapeAtLocation(
		 UZoneShapeComponent*ShapeComp,
		const FVector& BreakLocation);
protected:
	struct FCreateIntersectionState
	{
		TWeakObjectPtr<UZoneShapeComponent> WeakTargetShapeComponent;
		int32 OverlappingSegmentIndex = INDEX_NONE;
		float OverlappingSegmentT = -1.0f;
		int32 ClosePointIndex = INDEX_NONE;
		FVector PreviewLocation = FVector::ZeroVector;
	};
	FCreateIntersectionState CreateIntersectionState;
	
};
