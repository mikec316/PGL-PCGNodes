// Copyright by Procgen Labs Ltd. All Rights Reserved.

// C++ port of VoxelPlugin ISPC cellular noise hash functions.
// These must produce bit-identical results to VoxelNoiseNodesImpl.isph
// so that this node's Voronoi grid matches TrueDistanceCellularNoise2D.

#pragma once

#include "CoreMinimal.h"

namespace PGLCellularNoise
{
	// Exact match to VoxelNoiseNodesImpl.isph lines 59-60
	static constexpr int32 NoisePrimes_X = 501125321;
	static constexpr int32 NoisePrimes_Y = 1136930381;

	// Exact match to VoxelNoiseNodesImpl.isph lines 135-141
	// Note: HashPrimesHB does NOT have the (Hash >> 15) ^ Hash step
	// that HashPrimes has. It returns the raw multiplied hash.
	FORCEINLINE int32 HashPrimesHB(const int32 Seed, const int32 A, const int32 B)
	{
		int32 Hash = Seed;
		Hash ^= (A ^ B);
		Hash *= 0x27d4eb2d;
		return Hash;
	}

	// Exact match to VoxelNoiseNodesImpl.isph lines 223-234
	// Returns a normalized 2D direction derived from the hash.
	FORCEINLINE FVector2f GetCellularDirection2D(
		const int32 Seed,
		const FIntPoint& HashPosition,
		const int32 IndexX,
		const int32 IndexY)
	{
		const int32 Hash = HashPrimesHB(
			Seed,
			HashPosition.X + IndexX * NoisePrimes_X,
			HashPosition.Y + IndexY * NoisePrimes_Y);

		// Extract two 16-bit values, center around zero, normalize
		FVector2f Dir(
			static_cast<float>((Hash >> 0) & 0xFFFF) - 0xFFFF / 2.0f,
			static_cast<float>((Hash >> 16) & 0xFFFF) - 0xFFFF / 2.0f);

		const float Len = Dir.Length();
		if (Len > SMALL_NUMBER)
		{
			Dir /= Len;
		}
		return Dir;
	}

	// Computes a jittered cell center position in grid-local space.
	// Matching VoxelNoiseNodesImpl.isph line 345:
	//   Center = float2(IndexX, IndexY) + ScaledJitter * Direction
	FORCEINLINE FVector2f GetCellCenter(
		const int32 Seed,
		const FIntPoint& HashPosition,
		const int32 IndexX,
		const int32 IndexY,
		const float ScaledJitter)
	{
		const FVector2f Dir = GetCellularDirection2D(Seed, HashPosition, IndexX, IndexY);
		return FVector2f(
			static_cast<float>(IndexX) + ScaledJitter * Dir.X,
			static_cast<float>(IndexY) + ScaledJitter * Dir.Y);
	}

	// Circumcenter of triangle (A, B, C) in 2D.
	// This is the Voronoi vertex where the three cells meet.
	// Returns false if the triangle is degenerate (collinear points).
	FORCEINLINE bool ComputeCircumcenter(
		const FVector2f& A,
		const FVector2f& B,
		const FVector2f& C,
		FVector2f& OutCenter)
	{
		const float ax = A.X, ay = A.Y;
		const float bx = B.X, by = B.Y;
		const float cx = C.X, cy = C.Y;

		const float D = 2.0f * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));

		if (FMath::Abs(D) < 1e-10f)
		{
			return false; // Degenerate (collinear points)
		}

		const float A2 = ax * ax + ay * ay;
		const float B2 = bx * bx + by * by;
		const float C2 = cx * cx + cy * cy;

		OutCenter.X = (A2 * (by - cy) + B2 * (cy - ay) + C2 * (ay - by)) / D;
		OutCenter.Y = (A2 * (cx - bx) + B2 * (ax - cx) + C2 * (bx - ax)) / D;
		return true;
	}
}
