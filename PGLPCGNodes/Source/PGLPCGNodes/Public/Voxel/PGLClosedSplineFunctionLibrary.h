// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelFunctionLibrary.h"
#include "Buffer/VoxelFloatBuffers.h"
#include "PGLClosedSplineFunctionLibrary.generated.h"

UCLASS()
class PGLPCGNODES_API UPGLClosedSplineFunctionLibrary : public UVoxelFunctionLibrary
{
	GENERATED_BODY()

public:
	// Returns the signed distance from the closed spline projected onto the XY plane.
	// Negative = inside the spline loop, Positive = outside.
	UFUNCTION(Category = "Stamp|Closed Spline", meta = (AllowList = "ClosedSpline, HeightClosedSpline"))
	FVoxelFloatBuffer GetSignedDistanceFromClosedSpline2D(
		UPARAM(meta = (PositionPin)) const FVoxelVector2DBuffer& Position) const;

	// Returns the closest spline key for a 2D position.
	UFUNCTION(Category = "Stamp|Closed Spline", meta = (AllowList = "ClosedSpline, HeightClosedSpline"))
	FVoxelFloatBuffer GetClosestSplineKey2D(
		UPARAM(meta = (PositionPin)) const FVoxelVector2DBuffer& Position) const;

	// Returns position along the spline at a given key.
	UFUNCTION(Category = "Stamp|Closed Spline", meta = (AllowList = "ClosedSpline, HeightClosedSpline"))
	FVoxelVectorBuffer GetPositionAlongSpline(
		UPARAM(meta = (SplineKeyPin)) const FVoxelFloatBuffer& SplineKey) const;

	// Returns the spline length.
	UFUNCTION(Category = "Stamp|Closed Spline", meta = (AllowList = "ClosedSpline, HeightClosedSpline"))
	float GetSplineLength() const;

	// Returns the last valid spline key (at the spline's end point).
	UFUNCTION(Category = "Stamp|Closed Spline", meta = (AllowList = "ClosedSpline, HeightClosedSpline"))
	float GetLastSplineKey() const;

	// Returns how far along the spline we currently are (between 0 and GetSplineLength).
	UFUNCTION(Category = "Stamp|Closed Spline", meta = (AllowList = "ClosedSpline, HeightClosedSpline"))
	FVoxelFloatBuffer GetDistanceAlongSpline(
		UPARAM(meta = (SplineKeyPin)) const FVoxelFloatBuffer& SplineKey) const;

	// Returns the full transform (position, rotation, scale) along the spline at a given key.
	UFUNCTION(Category = "Stamp|Closed Spline", meta = (AllowList = "ClosedSpline, HeightClosedSpline"))
	FVoxelTransformBuffer GetTransformAlongSpline(
		UPARAM(meta = (SplineKeyPin)) const FVoxelFloatBuffer& SplineKey) const;
};
