// Copyright by Procgen Labs Ltd. All Rights Reserved.

// ZoneShapeBlueprintFunctionLibrary.cpp

#include "EditorUtilities/PGLPCGBlueprintFunctionLibrary.h"


#include "ZoneGraphSettings.h"
#include "ZoneShapeActor.h"
#include "Engine/World.h"
#include "CoreMinimal.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SceneView.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "Styling/AppStyle.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Curves/BezierUtilities.h"
#include "ZoneShapeUtilities.h"
#include "ScopedTransaction.h"
 // For FVoxelSculptHeightData

#include "ZoneShapeComponent.h"

#include "Modules/ModuleManager.h"
#include "SceneManagement.h"


#include "ZoneGraphTypes.h"

void SetPolygonPointLaneProfileToMatchSpline(FZoneShapePoint& Point, UZoneShapeComponent* Polygon, UZoneShapeComponent* Spline)
{
    Point.Type = FZoneShapePointType::LaneProfile;
    const FZoneLaneProfileRef ShapeComponent0LaneProfileRef = Spline->GetCommonLaneProfile();
    const int32 ProfileIndex = Polygon->AddUniquePerPointLaneProfile(ShapeComponent0LaneProfileRef);
    if (ProfileIndex != INDEX_NONE)
    {
        Point.LaneProfile = (uint8)ProfileIndex;
    }
}


void SetPointPositionRotation(
    FZoneShapePoint& Point,
    const FTransform& SourceTransform,
    const FVector& TargetPointWorldPosition,
    const FVector& TargetPointWorldNormal)
{
    Point.Position = SourceTransform.InverseTransformPosition(TargetPointWorldPosition);
    FVector Normal = SourceTransform.InverseTransformVector(TargetPointWorldNormal);
    Point.Rotation = FRotationMatrix::MakeFromX(Normal).Rotator();
}

void SplitSegment(const int32 InSegmentIndex, const float SegmentSplitT, UZoneShapeComponent* ShapeComp) 
{
	if (!ShapeComp)
	{
		ShapeComp ;
	}

	check(ShapeComp != nullptr);
	check(InSegmentIndex != INDEX_NONE);
	check(InSegmentIndex >= 0);
	check(InSegmentIndex < ShapeComp->GetNumSegments());

	ShapeComp->Modify();
	if (AActor* Owner = ShapeComp->GetOwner())
	{
		Owner->Modify();
	}

	TArray<FZoneShapePoint>& ShapePoints = ShapeComp->GetMutablePoints();
	const int32 NumPoints = ShapePoints.Num();
	const int32 StartPointIdx = InSegmentIndex;
	const int32 EndPointIdx = (InSegmentIndex + 1) % NumPoints;
	const FZoneShapePoint& StartPoint = ShapePoints[StartPointIdx];
	const FZoneShapePoint& EndPoint = ShapePoints[EndPointIdx];

	FVector StartPosition(ForceInitToZero), StartControlPoint(ForceInitToZero), EndControlPoint(ForceInitToZero), EndPosition(ForceInitToZero);
	UE::ZoneShape::Utilities::GetCubicBezierPointsFromShapeSegment(StartPoint, EndPoint, FMatrix::Identity, StartPosition, StartControlPoint, EndControlPoint, EndPosition);

	FZoneShapePoint NewPoint;
	NewPoint.Position = UE::CubicBezier::Eval(StartPosition, StartControlPoint, EndControlPoint, EndPosition, SegmentSplitT);


	// Set new point type based on neighbors
	if (StartPoint.Type == FZoneShapePointType::AutoBezier || EndPoint.Type == FZoneShapePointType::AutoBezier)
	{
		// Auto bezier handles will be updated in UpdateShape()
		NewPoint.Type = FZoneShapePointType::AutoBezier;
	}
	else if (StartPoint.Type == FZoneShapePointType::Bezier || EndPoint.Type == FZoneShapePointType::Bezier)
	{
		// Initial Bezier handles are created below, after insert.
		NewPoint.Type = FZoneShapePointType::Bezier;
	}
	else
	{
		NewPoint.Type = FZoneShapePointType::Sharp;
		NewPoint.TangentLength = 0.0f;
	}

	const int32 NewPointIndex = InSegmentIndex + 1;

	ShapePoints.Insert(NewPoint, NewPointIndex);

	// Create sane default tangent for Bezier points.
	if (NewPoint.Type == FZoneShapePointType::Bezier)
	{
		ShapeComp->UpdatePointRotationAndTangent(NewPointIndex);
	}

	// Set selection to new point

	ShapeComp->UpdateShape();

	GEditor->RedrawLevelEditingViewports(true);
}
double GetClockwiseAngle(const FVector& P)
{
	return -FMath::Atan2(P.X, -P.Y);
}

bool ComparePoints(const FVector& P1, const FVector& P2)
{
	return GetClockwiseAngle(P1) > GetClockwiseAngle(P2);
}
void SortPolygonPointsCounterclockwise(UZoneShapeComponent* PolygonShapeComp)
{
	if (PolygonShapeComp->GetShapeType() != FZoneShapeType::Polygon)
	{
		return;
	}

	TArray<FZoneShapePoint>& Points = PolygonShapeComp->GetMutablePoints();

	// Compute the center
	FVector Center = FVector::ZeroVector;
	for (const FZoneShapePoint& Point : Points)
	{
		Center += Point.Position;
	}
	Center /= Points.Num();

	Points.Sort([Center](const FZoneShapePoint& Point1, const FZoneShapePoint& Point2) {
		return ComparePoints(Point1.Position - Center, Point2.Position - Center);
	});
}


void UPGLPCGBlueprintFunctionLibrary::PGLCreateIntersectionForSplineShape(
    UZoneShapeComponent* ShapeComp,
    UZoneShapeComponent* TargetShapeComp,
    FVector Intersection,
    bool DestroyCoveredShape /*= true*/)
{
	if (!ShapeComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid Zone Shape Component provided."));
		return;
	}

	if (!TargetShapeComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid Zone Shape Component provided."));
		return;
	}

	const UZoneGraphSettings* ZoneGraphSettings = GetDefault<UZoneGraphSettings>();
	if (!ZoneGraphSettings)
	{
		UE_LOG(LogTemp, Warning, TEXT("ZoneGraphSettings not found."));
		return;
	}

	// Get the Target Shape Component from somewhere (example: through some logic or input)
  
	if (!TargetShapeComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("Target Shape Component not found."));
		return;
	}
	
	const FZoneLaneProfile* LaneProfile = ZoneGraphSettings->GetLaneProfileByRef(ShapeComp->GetCommonLaneProfile());
	double HalfLanesTotalWidth = LaneProfile ? LaneProfile->GetLanesTotalWidth() * 0.5 : 0.0;

	// Get overlapping position on the target segment

	TConstArrayView<FZoneShapePoint> ZoneShapePointsA = ShapeComp->GetPoints();
	TConstArrayView<FZoneShapePoint> ZoneShapePointsB = TargetShapeComp->GetPoints();

	FZoneShapePoint ClosestPointA;
	FZoneShapePoint ClosestPointB;
	int32 PointBIndex;
	float ClosestDistanceSquared = INFINITY;

	for(int i = 0; i < ZoneShapePointsA.Num(); ++i)
	{
		const FZoneShapePoint& Pt = ZoneShapePointsA[i];
		float DistanceSquared = FVector::DistSquared(Pt.Position, Intersection);
    
		// If the calculated distance is smaller than the current closest distance
		if (DistanceSquared < ClosestDistanceSquared)
		{
			// Update the closest distance and closest point
			ClosestDistanceSquared = DistanceSquared;
			ClosestPointA = Pt;
			
		}
	}

	ClosestDistanceSquared = INFINITY;
	for(int i = 0; i < ZoneShapePointsB.Num(); ++i)
	{
		const FZoneShapePoint& Pt = ZoneShapePointsB[i];
		float DistanceSquared = FVector::DistSquared(Pt.Position, Intersection);
    
		// If the calculated distance is smaller than the current closest distance
		if (DistanceSquared < ClosestDistanceSquared)
		{
			// Update the closest distance and closest point
			ClosestDistanceSquared = DistanceSquared;
			PointBIndex = i;
			ClosestPointB = Pt;
		}
	}

	TArray<UZoneShapeComponent*> ShapeComponents = PGLBreakZoneShapeAtLocation(ShapeComp, Intersection);
	
	const FTransform& ShapeCompTransform = ShapeComp->GetComponentTransform();
	const FTransform& TargetShapeCompTransform = TargetShapeComp->GetComponentTransform();

	FActorSpawnParameters SpawnParams;
	AZoneShape* IntersectionShapeActor = ShapeComp->GetWorld()->SpawnActor<AZoneShape>(AZoneShape::StaticClass(), ShapeCompTransform, SpawnParams);
	UZoneShapeComponent* IntersectionShapeComponent = IntersectionShapeActor->GetComponentByClass<UZoneShapeComponent>();
	IntersectionShapeComponent->SetShapeType(FZoneShapeType::Polygon);
	const FTransform& IntersectionTransform = IntersectionShapeComponent->GetComponentTransform();
	FVector Normal = ShapeComp->GetShapeConnectorByPointIndex(PointBIndex)->Normal;
    
	if (ShapeComponents.Num() == 1)
	{
		const FZoneShapeConnector* TargetConnector = TargetShapeComp->GetShapeConnectorByPointIndex(PointBIndex);

		// Compute the intersection location from the connector position and normal
		FVector TargetNormal = TargetConnector->Normal;
		
    
    
		FVector TargetWorldNormal = TargetShapeComp->GetComponentTransform().TransformVector(TargetConnector->Normal);
		IntersectionShapeActor->SetActorLocation(Intersection + TargetWorldNormal * HalfLanesTotalWidth);
   
		TArray<FZoneShapePoint>& Points0 = ShapeComponents[0]->GetMutablePoints();
		int32 Index0 = Points0.Num() - 1;
		FVector Normal0 = ShapeComponents[0]->GetShapeConnectorByPointIndex(Index0)->Normal;
		Points0.Last().Position -= Normal0 * HalfLanesTotalWidth;
		ShapeComponents[0]->UpdateShape();
    
		TArray<FZoneShapePoint>& Points = IntersectionShapeComponent->GetMutablePoints();
		SetPolygonPointLaneProfileToMatchSpline(Points[0], IntersectionShapeComponent, ShapeComponents[0]);
		TConstArrayView<FZoneShapePoint> TargetPoints = TargetShapeComp->GetPoints();
		const FTransform Shape0Transform = ShapeComponents[0]->GetComponentTransform();
		const FVector Point0WorldPosition = Shape0Transform.TransformPosition(TargetPoints[PointBIndex].Position);
		const FVector Point0WorldNormal = Shape0Transform.TransformVector(TargetNormal);
		SetPointPositionRotation(Points[0], IntersectionTransform, Point0WorldPosition, Point0WorldNormal);

		SetPolygonPointLaneProfileToMatchSpline(Points[1], IntersectionShapeComponent, ShapeComp);
		ClosestPointA.Position -= Normal * HalfLanesTotalWidth;
		const FVector Point1WorldPosition = ShapeCompTransform.TransformPosition(ClosestPointA.Position);
		const FVector Point1WorldNormal = ShapeCompTransform.TransformVector(Normal);
		SetPointPositionRotation(Points[1], IntersectionTransform, Point1WorldPosition, Point1WorldNormal);



		// Update shapes after modifications
		ShapeComp->UpdateShape();
		IntersectionShapeComponent->UpdateShape();
	}
	else if (ShapeComponents.Num() == 2)
	{
		// Cut the intersected shape
		const FTransform& TargetTransform = TargetShapeComp->GetComponentTransform();
		FBox Bounds = FBox::BuildAABB(TargetTransform.TransformPosition(Intersection), FVector(HalfLanesTotalWidth));
		// Move points
		const FTransform& ShapeTransform0 = ShapeComponents[0]->GetComponentTransform();
		const FTransform& ShapeTransform1 = ShapeComponents[1]->GetComponentTransform();
		TArray<FZoneShapePoint>& Points0 = ShapeComponents[0]->GetMutablePoints();
		int32 Index0 = Points0.Num() - 1;
		for (int32 i = Index0 - 1; i > 1; i--)
		{
			if (!Bounds.IsInside(ShapeTransform0.TransformPosition(Points0[i].Position)))
			{
				continue;
			}
			Points0.RemoveAt(i);
		}
		Index0 = Points0.Num() - 1;
		if (ShapeComponents[0])
		{
			FVector Normal0 = ShapeComponents[0]->GetShapeConnectorByPointIndex(Index0)->Normal;
			FVector Offset = Normal0 * HalfLanesTotalWidth;
			if (Points0.Num() == 2)
			{
				double Length = FVector::Dist(Points0[0].Position, Points0[1].Position);
				if (Length < HalfLanesTotalWidth * 2)
				{
					Offset = Normal0 * Length * 1;
				}
				else
				{
					Offset = (Normal0 * HalfLanesTotalWidth)*2;
				}
			}
			Points0.Last().Position -= Offset;
			ShapeComponents[0]->UpdateShape();
		}

		TArray<FZoneShapePoint>& Points1 = ShapeComponents[1]->GetMutablePoints();
		int32 Index1 = 0;
		for (int32 i = Index1 + 1; i < (Points1.Num() - 2); i++)
		{
			if (!Bounds.IsInside(ShapeTransform1.TransformPosition(Points1[i].Position)))
			{
				continue;
			}
			Points1.RemoveAt(i);
		}
		if (ShapeComponents[1])
		{
			FVector Normal1 = ShapeComponents[1]->GetShapeConnectorByPointIndex(Index1)->Normal;
			FVector Offset = Normal1 * HalfLanesTotalWidth;
			if (Points1.Num() == 2)
			{
				double Length = FVector::Dist(Points1[0].Position, Points1[1].Position);
				if (Length < HalfLanesTotalWidth * 2)
				{
					Offset= Normal1 * Length * .5;
				}
			
				else
				{
					Offset = (Normal1 * HalfLanesTotalWidth)*2;
				}
			}
			Points1[Index1].Position -= Offset;
			
			ShapeComponents[1]->UpdateShape();
		}

		// Create intersection with the same profile
		IntersectionShapeActor->SetActorLocation(Intersection);

		// Get points. Set positions. Set profile.
		TArray<FZoneShapePoint>& Points = IntersectionShapeComponent->GetMutablePoints();
		if (ShapeComponents[0] && ShapeComponents[1])
		{
			Points.Add(FZoneShapePoint(Points[1]));
		}
		TArray<FZoneShapePoint>& TargetPoints = TargetShapeComp->GetMutablePoints();
		// Connect

		
		int32 IntersectionPointIndex = 0;
		if (ShapeComponents[0])
		{
			SetPolygonPointLaneProfileToMatchSpline(Points[IntersectionPointIndex], IntersectionShapeComponent, ShapeComponents[0]);

			const FVector PointWorldPosition = ShapeTransform0.TransformPosition(Points0.Last().Position);
			const FZoneShapeConnector* Connector0 = ShapeComponents[0]->GetShapeConnectorByPointIndex(Points0.Num() - 1);
			const FVector PointWorldNormal = ShapeTransform0.TransformVector(Connector0->Normal);
			SetPointPositionRotation(Points[IntersectionPointIndex], IntersectionTransform, PointWorldPosition , PointWorldNormal);
			IntersectionPointIndex++;
		}

		if (ShapeComponents[1])
		{
			SetPolygonPointLaneProfileToMatchSpline(Points[IntersectionPointIndex], IntersectionShapeComponent, ShapeComponents[1]);

			const FVector PointWorldPosition = ShapeTransform1.TransformPosition(Points1[0].Position);
			const FZoneShapeConnector* Connector1 = ShapeComponents[1]->GetShapeConnectorByPointIndex(0);
			const FVector PointWorldNormal = ShapeTransform1.TransformVector(Connector1->Normal);
			SetPointPositionRotation(Points[IntersectionPointIndex], IntersectionTransform, PointWorldPosition , PointWorldNormal);
			IntersectionPointIndex++;
		}

		FVector Normal1 = TargetShapeComp->GetShapeConnectorByPointIndex(PointBIndex)->Normal;
		FVector Offset = (Normal1 * HalfLanesTotalWidth)*2;

		
		SetPolygonPointLaneProfileToMatchSpline(Points[IntersectionPointIndex], IntersectionShapeComponent, TargetShapeComp);
		ClosestPointB.Position -= Normal * HalfLanesTotalWidth;
		ShapeComp->UpdateShape();
		const FVector WorldNormal = TargetShapeCompTransform.TransformVector(Normal);
		Intersection = TargetShapeCompTransform.TransformPosition(ClosestPointB.Position);
		SetPointPositionRotation(Points[IntersectionPointIndex], IntersectionTransform, Intersection - Offset, WorldNormal);
		SetPointPositionRotation(TargetPoints[PointBIndex], TargetShapeCompTransform,Intersection - Offset, WorldNormal);

		SortPolygonPointsCounterclockwise(IntersectionShapeComponent);

		// Update point positions
		TargetShapeComp->UpdateShape(); // Update shape
		IntersectionShapeComponent->UpdateShape();
	}
}
TArray<UZoneShapeComponent*> UPGLPCGBlueprintFunctionLibrary::PGLBreakZoneShapeAtLocation(
    UZoneShapeComponent* ShapeComp,
    const FVector& BreakLocation)
{
	check(ShapeComp != nullptr);

    // Get the world transform of the shape component
    const FTransform& ShapeCompTransform = ShapeComp->GetComponentTransform();
    TArray<FZoneShapePoint>& Points = ShapeComp->GetMutablePoints();  // Get mutable points


    // Find the closest segment index to the break location
    int32 ClosestSegmentIndex = -1;
    float ClosestDistanceSquared = FLT_MAX;
    float SegmentT = 0.0f;

    TArray<UZoneShapeComponent*> ShapeComponents;
    ShapeComponents.Add(ShapeComp);

	ShapeComp->Modify();
	if (AActor* Owner = ShapeComp->GetOwner())
	{
		Owner->Modify();
	}

	


    for (int32 i = 0; i < Points.Num() - 1; i++)
    {
        FVector StartPosition = ShapeCompTransform.TransformPosition(Points[i].Position);
        FVector EndPosition = ShapeCompTransform.TransformPosition(Points[i + 1].Position);
        FVector ClosestPointOnSegment = FMath::ClosestPointOnSegment(BreakLocation, StartPosition, EndPosition);
        float DistanceSquared = FVector::DistSquared(BreakLocation, ClosestPointOnSegment);
    
        if (DistanceSquared < ClosestDistanceSquared)
        {
            ClosestDistanceSquared = DistanceSquared;
            ClosestSegmentIndex = i;
            SegmentT = FVector::Dist(StartPosition, ClosestPointOnSegment) / FVector::Dist(StartPosition, EndPosition);
        }
    }

	
    

    // Split the segment at the calculated point
    FVector NewPointPosition;
    FZoneShapePoint NewPoint;
    NewPoint.Position = ShapeCompTransform.InverseTransformPosition(NewPointPosition);
    Points.Insert(NewPoint, ClosestSegmentIndex + 1);
    ShapeComp->UpdateShape();  // Update the original shape after modification

    // Create a new Zone Shape Component and transfer the points after the split point
    AZoneShape* NewZoneShapeActor = ShapeComp->GetWorld()->SpawnActor<AZoneShape>(AZoneShape::StaticClass(), ShapeCompTransform);
    UZoneShapeComponent* NewZoneShapeComp = NewZoneShapeActor->FindComponentByClass<UZoneShapeComponent>();
    if (NewZoneShapeComp)
    {
        // Get mutable points of the new shape
        TArray<FZoneShapePoint>& NewPoints = NewZoneShapeComp->GetMutablePoints();
    	
    	NewPoints.Empty();
        NewPoints.Append(Points.GetData() + ClosestSegmentIndex + 1 , Points.Num() - (ClosestSegmentIndex + 1) );
        NewZoneShapeComp->UpdateShape();
    }

    ShapeComponents.Add(NewZoneShapeComp);

    // Remove the points from the original component after the split point
    Points.SetNum(ClosestSegmentIndex + 2);
    ShapeComp->UpdateShape();  // Update the shape again to apply the removal of points

    return ShapeComponents;
}
