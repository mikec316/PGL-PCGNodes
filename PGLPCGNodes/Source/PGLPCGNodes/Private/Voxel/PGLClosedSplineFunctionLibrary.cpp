// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "Voxel/PGLClosedSplineFunctionLibrary.h"
#include "Spline/VoxelSplineGraphParameters.h"
#include "Spline/VoxelSplineStamp.h"
#include "Components/SplineComponent.h"

namespace PGLClosedSplineUtils
{
	///////////////////////////////////////////////////////////////////////////
	// Cubic Hermite helpers (2D, matching UE's CubicInterp formula)
	///////////////////////////////////////////////////////////////////////////

	FORCEINLINE FVector2f CubicInterp2D(
		const FVector2f& P0,
		const FVector2f& T0,
		const FVector2f& P1,
		const FVector2f& T1,
		const float Alpha)
	{
		const float A2 = Alpha * Alpha;
		const float A3 = A2 * Alpha;

		return (2.f * A3 - 3.f * A2 + 1.f) * P0
			+ (A3 - 2.f * A2 + Alpha) * T0
			+ (-2.f * A3 + 3.f * A2) * P1
			+ (A3 - A2) * T1;
	}

	FORCEINLINE FVector2f CubicInterpDerivative2D(
		const FVector2f& P0,
		const FVector2f& T0,
		const FVector2f& P1,
		const FVector2f& T1,
		const float Alpha)
	{
		const float A2 = Alpha * Alpha;

		return (6.f * A2 - 6.f * Alpha) * P0
			+ (3.f * A2 - 4.f * Alpha + 1.f) * T0
			+ (-6.f * A2 + 6.f * Alpha) * P1
			+ (3.f * A2 - 2.f * Alpha) * T1;
	}

	///////////////////////////////////////////////////////////////////////////
	// Newton-Raphson closest-point-on-segment solver
	// (C++ port of VoxelSplineStampImpl.ispc FindNearestOnSegment2D)
	///////////////////////////////////////////////////////////////////////////

	// Find the closest point on a single spline segment to a 2D query point.
	// Returns the squared distance; OutKey receives the full spline key.
	static float FindNearestOnSegment2D(
		const FVector2f& QueryPt,
		const ispc::FSegment& Segment,
		float& OutKey)
	{
		const FVector2f Point(Segment.Value.X, Segment.Value.Y);
		const FVector2f NextPoint(Segment.NextValue.X, Segment.NextValue.Y);
		const FVector2f LeaveTangent(Segment.LeaveTangent.X, Segment.LeaveTangent.Y);
		const FVector2f ArriveTangent(Segment.ArriveTangent.X, Segment.ArriveTangent.Y);

		// Constant interpolation: just check the two endpoints
		if (Segment.InterpMode == ispc::InterpMode_Constant)
		{
			const float DistA = FVector2f::DistSquared(QueryPt, Point);
			const float DistB = FVector2f::DistSquared(QueryPt, NextPoint);

			if (DistA < DistB)
			{
				OutKey = Segment.Key;
				return DistA;
			}
			else
			{
				OutKey = Segment.NextKey;
				return DistB;
			}
		}

		// Linear interpolation: project onto line segment
		if (Segment.InterpMode == ispc::InterpMode_Linear)
		{
			const float A = FVector2f::DotProduct(QueryPt - Point, NextPoint - Point);
			const float B = FVector2f::DistSquared(Point, NextPoint);
			const float Alpha = FMath::Clamp(A / B, 0.f, 1.f);

			const FVector2f ClosestPt = FMath::Lerp(Point, NextPoint, Alpha);
			OutKey = Segment.Key + Alpha;
			return FVector2f::DistSquared(ClosestPt, QueryPt);
		}

		// Cubic interpolation: multi-start Newton-Raphson
		float BestMoves[4];
		float BestValues[4];
		float BestSquaredDists[4];

		// Phase 1: Coarse search from 4 starting points
		for (int32 StartIdx = 0; StartIdx < 4; StartIdx++)
		{
			const float StartValue = StartIdx / 3.f;
			const float MinValue = FMath::Clamp(StartValue - 1.f / 3.f, 0.f, 1.f);
			const float MaxValue = FMath::Clamp(StartValue + 1.f / 3.f, 0.f, 1.f);

			FVector2f FoundPoint = CubicInterp2D(Point, LeaveTangent, NextPoint, ArriveTangent, StartValue);

			float Move;
			{
				const FVector2f Tangent = CubicInterpDerivative2D(Point, LeaveTangent, NextPoint, ArriveTangent, StartValue);
				const float TangentLenSq = Tangent.SizeSquared();
				Move = TangentLenSq > SMALL_NUMBER
					? FVector2f::DotProduct(QueryPt - FoundPoint, Tangent) / TangentLenSq
					: 0.f;
			}

			float Value = StartValue;

			for (int32 Iteration = 1; Iteration < 20; Iteration++)
			{
				const float NewValue = FMath::Clamp(Value + Move, MinValue, MaxValue);

				if (FMath::IsNearlyEqual(Value, NewValue, 1.e-2f))
				{
					break;
				}

				Value = NewValue;
				FoundPoint = CubicInterp2D(Point, LeaveTangent, NextPoint, ArriveTangent, Value);

				const FVector2f Tangent = CubicInterpDerivative2D(Point, LeaveTangent, NextPoint, ArriveTangent, Value);
				const float TangentLenSq = Tangent.SizeSquared();
				Move = TangentLenSq > SMALL_NUMBER
					? FVector2f::DotProduct(QueryPt - FoundPoint, Tangent) / TangentLenSq
					: 0.f;
			}

			BestMoves[StartIdx] = Move;
			BestValues[StartIdx] = Value;
			BestSquaredDists[StartIdx] = FVector2f::DistSquared(FoundPoint, QueryPt);
		}

		// Pick the best of the 4 starting points
		float Move = BestMoves[0];
		float Value = BestValues[0];
		float SquaredDist = BestSquaredDists[0];

		for (int32 i = 1; i < 4; i++)
		{
			if (BestSquaredDists[i] < SquaredDist)
			{
				Move = BestMoves[i];
				Value = BestValues[i];
				SquaredDist = BestSquaredDists[i];
			}
		}

		// Phase 2: Refinement from best approximation
		if (!FMath::IsNearlyZero(Move, 1.e-6f))
		{
			Value = FMath::Clamp(Value + Move, 0.f, 1.f);

			FVector2f FoundPoint = CubicInterp2D(Point, LeaveTangent, NextPoint, ArriveTangent, Value);

			{
				const FVector2f Tangent = CubicInterpDerivative2D(Point, LeaveTangent, NextPoint, ArriveTangent, Value);
				const float TangentLenSq = Tangent.SizeSquared();
				Move = TangentLenSq > SMALL_NUMBER
					? FVector2f::DotProduct(QueryPt - FoundPoint, Tangent) / TangentLenSq
					: 0.f;
			}

			for (int32 Iteration = 1; Iteration < 20; Iteration++)
			{
				const float NewValue = FMath::Clamp(Value + Move, 0.f, 1.f);

				if (FMath::IsNearlyEqual(Value, NewValue, 1.e-5f))
				{
					break;
				}

				Value = NewValue;
				FoundPoint = CubicInterp2D(Point, LeaveTangent, NextPoint, ArriveTangent, Value);

				const FVector2f Tangent = CubicInterpDerivative2D(Point, LeaveTangent, NextPoint, ArriveTangent, Value);
				const float TangentLenSq = Tangent.SizeSquared();
				Move = TangentLenSq > SMALL_NUMBER
					? FVector2f::DotProduct(QueryPt - FoundPoint, Tangent) / TangentLenSq
					: 0.f;
			}

			SquaredDist = FVector2f::DistSquared(FoundPoint, QueryPt);
		}

		OutKey = Segment.Key + Value;
		return SquaredDist;
	}

	///////////////////////////////////////////////////////////////////////////
	// Find nearest point across all segments
	///////////////////////////////////////////////////////////////////////////

	struct FNearestResult
	{
		float SquaredDistance = MAX_flt;
		float Key = 0.f;
	};

	static FNearestResult FindNearest2D(
		const FVector2f& QueryPt,
		const TConstVoxelArrayView<FVoxelSplineSegment>& Segments)
	{
		FNearestResult Best;

		for (const FVoxelSplineSegment& Segment : Segments)
		{
			float LocalKey;
			const float LocalDistSq = FindNearestOnSegment2D(QueryPt, Segment.Segment, LocalKey);

			if (LocalDistSq < Best.SquaredDistance)
			{
				Best.SquaredDistance = LocalDistSq;
				Best.Key = LocalKey;
			}
		}

		return Best;
	}

	///////////////////////////////////////////////////////////////////////////
	// Cull segments by 2D bounding box intersection
	///////////////////////////////////////////////////////////////////////////

	static TVoxelArray<FVoxelSplineSegment> CullSegments2D(
		const TConstVoxelArrayView<FVoxelSplineSegment>& AllSegments,
		const FVector2f& BoundsMin,
		const FVector2f& BoundsMax)
	{
		TVoxelArray<FVoxelSplineSegment> Result;
		Result.Reserve(AllSegments.Num());

		for (const FVoxelSplineSegment& Segment : AllSegments)
		{
			// 2D AABB intersection test (XY only)
			if (Segment.Bounds.Max.X < BoundsMin.X ||
				Segment.Bounds.Min.X > BoundsMax.X ||
				Segment.Bounds.Max.Y < BoundsMin.Y ||
				Segment.Bounds.Min.Y > BoundsMax.Y)
			{
				continue;
			}

			Result.Add(Segment);
		}

		// Fallback: if all segments were culled, use all
		if (Result.Num() == 0)
		{
			Result.Append(AllSegments.GetData(), AllSegments.Num());
		}

		return Result;
	}

	///////////////////////////////////////////////////////////////////////////
	// Coarse polyline for ray casting (sign determination only)
	///////////////////////////////////////////////////////////////////////////

	static constexpr int32 RayCastSamplesPerSegment = 4;

	static TVoxelArray<FVector2f> SampleSplinePolyline2D(
		const FSplineCurves& SplineCurves,
		const int32 NumSegments,
		const int32 SamplesPerSeg = RayCastSamplesPerSegment)
	{
		const int32 TotalSamples = NumSegments * SamplesPerSeg;

		TVoxelArray<FVector2f> Polyline;
		Polyline.Reserve(TotalSamples);

		for (int32 Seg = 0; Seg < NumSegments; Seg++)
		{
			for (int32 Sub = 0; Sub < SamplesPerSeg; Sub++)
			{
				const float Key = float(Seg) + float(Sub) / float(SamplesPerSeg);
				const FVector Pos = SplineCurves.Position.Eval(Key);
				Polyline.Add(FVector2f(Pos.X, Pos.Y));
			}
		}

		return Polyline;
	}

	// Ray casting test: is the point inside the closed polyline? (XY plane)
	static bool IsInsidePolyline2D(
		const FVector2f& QueryPt,
		const TConstVoxelArrayView<FVector2f>& Polyline)
	{
		int32 CrossingCount = 0;
		const int32 NumEdges = Polyline.Num();

		for (int32 i = 0; i < NumEdges; i++)
		{
			const FVector2f& A = Polyline[i];
			const FVector2f& B = Polyline[(i + 1) % NumEdges];

			if ((A.Y <= QueryPt.Y && B.Y > QueryPt.Y) ||
				(B.Y <= QueryPt.Y && A.Y > QueryPt.Y))
			{
				const float T = (QueryPt.Y - A.Y) / (B.Y - A.Y);
				const float IntersectX = A.X + T * (B.X - A.X);
				if (QueryPt.X < IntersectX)
				{
					CrossingCount++;
				}
			}
		}

		return (CrossingCount % 2) != 0;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelFloatBuffer UPGLClosedSplineFunctionLibrary::GetSignedDistanceFromClosedSpline2D(
	const FVoxelVector2DBuffer& InPosition) const
{
	const FVoxelGraphParameters::FSplineStamp* Parameter = Query->FindParameter<FVoxelGraphParameters::FSplineStamp>();
	if (!Parameter)
	{
		if (Query.IsPreview())
		{
			VOXEL_MESSAGE(Error, "{0}: No Spline Preview Data node found in Editor Graph", this);
		}
		else
		{
			VOXEL_MESSAGE(Error, "{0}: No spline data", this);
		}
		return DefaultBuffer;
	}

	if (!Parameter->bClosedLoop)
	{
		VOXEL_MESSAGE(Error, "{0}: Spline must be a closed loop for signed distance", this);
		return DefaultBuffer;
	}

	FVoxelVector2DBuffer Position = InPosition;
	Position.ExpandConstants(Position.Num());

	const FSplineCurves& SplineCurves = *Parameter->SplineCurves;
	const int32 NumPoints = SplineCurves.Position.Points.Num();
	const int32 NumSegments = NumPoints; // Closed loop: NumSegments == NumPoints

	VOXEL_FUNCTION_COUNTER_NUM(Position.Num());

	// --- AABB culling: compute query bounds and cull segments ---
	FVector2f BoundsMin(MAX_flt, MAX_flt);
	FVector2f BoundsMax(-MAX_flt, -MAX_flt);

	for (int32 i = 0; i < Position.Num(); i++)
	{
		BoundsMin.X = FMath::Min(BoundsMin.X, Position.X[i]);
		BoundsMin.Y = FMath::Min(BoundsMin.Y, Position.Y[i]);
		BoundsMax.X = FMath::Max(BoundsMax.X, Position.X[i]);
		BoundsMax.Y = FMath::Max(BoundsMax.Y, Position.Y[i]);
	}

	const TVoxelArray<FVoxelSplineSegment> CulledSegments =
		PGLClosedSplineUtils::CullSegments2D(Parameter->Segments, BoundsMin, BoundsMax);

	const TConstVoxelArrayView<FVoxelSplineSegment> SegmentsView = CulledSegments;

	// --- Coarse polyline for sign determination ---
	const TVoxelArray<FVector2f> RayCastPolyline =
		PGLClosedSplineUtils::SampleSplinePolyline2D(SplineCurves, NumSegments);

	const TConstVoxelArrayView<FVector2f> RayCastPolylineView = RayCastPolyline;

	// --- Compute signed distance per position ---
	FVoxelFloatBuffer Result;
	Result.Allocate(Position.Num());

	for (int32 Index = 0; Index < Position.Num(); Index++)
	{
		const FVector2f QueryPt(Position.X[Index], Position.Y[Index]);

		// Unsigned distance via Newton-Raphson on cubic segments
		const PGLClosedSplineUtils::FNearestResult Nearest =
			PGLClosedSplineUtils::FindNearest2D(QueryPt, SegmentsView);

		const float UnsignedDist = FMath::Sqrt(Nearest.SquaredDistance);

		// Sign via coarse polyline ray cast
		const bool bInside =
			PGLClosedSplineUtils::IsInsidePolyline2D(QueryPt, RayCastPolylineView);

		Result.Set(Index, bInside ? -UnsignedDist : UnsignedDist);
	}

	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelFloatBuffer UPGLClosedSplineFunctionLibrary::GetClosestSplineKey2D(
	const FVoxelVector2DBuffer& InPosition) const
{
	const FVoxelGraphParameters::FSplineStamp* Parameter = Query->FindParameter<FVoxelGraphParameters::FSplineStamp>();
	if (!Parameter)
	{
		if (Query.IsPreview())
		{
			VOXEL_MESSAGE(Error, "{0}: No Spline Preview Data node found in Editor Graph", this);
		}
		else
		{
			VOXEL_MESSAGE(Error, "{0}: No spline data", this);
		}
		return DefaultBuffer;
	}

	FVoxelVector2DBuffer Position = InPosition;
	Position.ExpandConstants(Position.Num());

	VOXEL_FUNCTION_COUNTER_NUM(Position.Num());

	// --- AABB culling ---
	FVector2f BoundsMin(MAX_flt, MAX_flt);
	FVector2f BoundsMax(-MAX_flt, -MAX_flt);

	for (int32 i = 0; i < Position.Num(); i++)
	{
		BoundsMin.X = FMath::Min(BoundsMin.X, Position.X[i]);
		BoundsMin.Y = FMath::Min(BoundsMin.Y, Position.Y[i]);
		BoundsMax.X = FMath::Max(BoundsMax.X, Position.X[i]);
		BoundsMax.Y = FMath::Max(BoundsMax.Y, Position.Y[i]);
	}

	const TVoxelArray<FVoxelSplineSegment> CulledSegments =
		PGLClosedSplineUtils::CullSegments2D(Parameter->Segments, BoundsMin, BoundsMax);

	const TConstVoxelArrayView<FVoxelSplineSegment> SegmentsView = CulledSegments;

	// --- Newton-Raphson closest key per position ---
	FVoxelFloatBuffer Result;
	Result.Allocate(Position.Num());

	for (int32 Index = 0; Index < Position.Num(); Index++)
	{
		const FVector2f QueryPt(Position.X[Index], Position.Y[Index]);

		const PGLClosedSplineUtils::FNearestResult Nearest =
			PGLClosedSplineUtils::FindNearest2D(QueryPt, SegmentsView);

		Result.Set(Index, Nearest.Key);
	}

	return Result;
}

FVoxelVectorBuffer UPGLClosedSplineFunctionLibrary::GetPositionAlongSpline(
	const FVoxelFloatBuffer& SplineKey) const
{
	const FVoxelGraphParameters::FSplineStamp* Parameter = Query->FindParameter<FVoxelGraphParameters::FSplineStamp>();
	if (!Parameter)
	{
		if (Query.IsPreview())
		{
			VOXEL_MESSAGE(Error, "{0}: No Spline Preview Data node found in Editor Graph", this);
		}
		else
		{
			VOXEL_MESSAGE(Error, "{0}: No spline data", this);
		}
		return DefaultBuffer;
	}

	const FSplineCurves& SplineCurves = *Parameter->SplineCurves;

	VOXEL_FUNCTION_COUNTER_NUM(SplineKey.Num());

	FVoxelVectorBuffer Result;
	Result.Allocate(SplineKey.Num());

	for (int32 Index = 0; Index < SplineKey.Num(); Index++)
	{
		const float Key = SplineKey[Index];
		Result.Set(Index, FVector3f(SplineCurves.Position.Eval(Key)));
	}

	return Result;
}

float UPGLClosedSplineFunctionLibrary::GetSplineLength() const
{
	const FVoxelGraphParameters::FSplineStamp* Parameter = Query->FindParameter<FVoxelGraphParameters::FSplineStamp>();
	if (!Parameter)
	{
		if (Query.IsPreview())
		{
			VOXEL_MESSAGE(Error, "{0}: No Spline Preview Data node found in Editor Graph", this);
		}
		else
		{
			VOXEL_MESSAGE(Error, "{0}: No spline data", this);
		}
		return 0.f;
	}

	return Parameter->SplineLength;
}

float UPGLClosedSplineFunctionLibrary::GetLastSplineKey() const
{
	const FVoxelGraphParameters::FSplineStamp* Parameter = Query->FindParameter<FVoxelGraphParameters::FSplineStamp>();
	if (!Parameter)
	{
		if (Query.IsPreview())
		{
			VOXEL_MESSAGE(Error, "{0}: No Spline Preview Data node found in Editor Graph", this);
		}
		else
		{
			VOXEL_MESSAGE(Error, "{0}: No spline data", this);
		}
		return 0.f;
	}

	const int32 NumPoints = Parameter->SplineCurves->Position.Points.Num();
	return float(NumPoints - 1);
}

FVoxelFloatBuffer UPGLClosedSplineFunctionLibrary::GetDistanceAlongSpline(
	const FVoxelFloatBuffer& SplineKey) const
{
	const FVoxelGraphParameters::FSplineStamp* Parameter = Query->FindParameter<FVoxelGraphParameters::FSplineStamp>();
	if (!Parameter)
	{
		if (Query.IsPreview())
		{
			VOXEL_MESSAGE(Error, "{0}: No Spline Preview Data node found in Editor Graph", this);
		}
		else
		{
			VOXEL_MESSAGE(Error, "{0}: No spline data", this);
		}
		return DefaultBuffer;
	}

	const FSplineCurves& SplineCurves = *Parameter->SplineCurves;
	const int32 NumPoints = SplineCurves.Position.Points.Num();
	const int32 NumSegments = Parameter->bClosedLoop ? NumPoints : NumPoints - 1;

	FVoxelFloatBuffer Result;
	Result.Allocate(SplineKey.Num());

	for (int32 Index = 0; Index < SplineKey.Num(); Index++)
	{
		const float Key = SplineKey[Index];
		if (Key < 0)
		{
			Result.Set(Index, 0.f);
			continue;
		}

		if (Key >= NumSegments)
		{
			Result.Set(Index, SplineCurves.GetSplineLength());
			continue;
		}

		const int32 PointIndex = FMath::FloorToInt(Key);
		const float Fraction = Key - PointIndex;
		const int32 ReparamPointIndex = PointIndex * Parameter->ReparamStepsPerSegment;
		const float Distance = SplineCurves.ReparamTable.Points[ReparamPointIndex].InVal;

		Result.Set(Index, Distance + SplineCurves.GetSegmentLength(
			PointIndex, Fraction, Parameter->bClosedLoop, FVector::OneVector));
	}

	return Result;
}

FVoxelTransformBuffer UPGLClosedSplineFunctionLibrary::GetTransformAlongSpline(
	const FVoxelFloatBuffer& SplineKey) const
{
	const FVoxelGraphParameters::FSplineStamp* Parameter = Query->FindParameter<FVoxelGraphParameters::FSplineStamp>();
	if (!Parameter)
	{
		if (Query.IsPreview())
		{
			VOXEL_MESSAGE(Error, "{0}: No Spline Preview Data node found in Editor Graph", this);
		}
		else
		{
			VOXEL_MESSAGE(Error, "{0}: No spline data", this);
		}
		return DefaultBuffer;
	}

	const FSplineCurves& SplineCurves = *Parameter->SplineCurves;

	FVoxelTransformBuffer Result;
	Result.Allocate(SplineKey.Num());

	for (int32 Index = 0; Index < SplineKey.Num(); Index++)
	{
		const float Key = SplineKey[Index];

		const FVector Position = SplineCurves.Position.Eval(Key);

		FQuat Rotation = SplineCurves.Rotation.Eval(Key);
		{
			Rotation.Normalize();

			const FVector Direction = SplineCurves.Position.EvalDerivative(Key, FVector::ZeroVector).GetSafeNormal();
			const FVector UpVector = Rotation.RotateVector(FVector::UpVector);

			Rotation = FRotationMatrix::MakeFromXZ(Direction, UpVector).ToQuat();
		}

		const FVector Scale = SplineCurves.Scale.Eval(Key, FVector(1.f));

		Result.Set(Index, FTransform3f(
			FQuat4f(Rotation),
			FVector3f(Position),
			FVector3f(Scale)));
	}

	return Result;
}
