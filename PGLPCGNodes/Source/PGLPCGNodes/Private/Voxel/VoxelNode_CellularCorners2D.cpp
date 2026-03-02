// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "Voxel/VoxelNode_CellularCorners2D.h"
#include "Voxel/CellularNoiseUtilities.h"
#include "VoxelPointId.h"
#include "Buffer/VoxelFloatBuffers.h"
#include "Buffer/VoxelDoubleBuffers.h"
#include "Buffer/VoxelIntegerBuffers.h"
#include <algorithm>

void FVoxelNode_CellularCorners2D::Compute(const FVoxelGraphQuery Query) const
{
	const TValue<FVoxelPointSet> Points = InPin.Get(Query);
	const TValue<float> FeatureScale = FeatureScalePin.Get(Query);
	const TValue<float> Jitter = JitterPin.Get(Query);
	const TValue<FVoxelSeed> Seed = SeedPin.Get(Query);

	VOXEL_GRAPH_WAIT(Points, FeatureScale, Jitter, Seed)
	{
		if (Points->Num() == 0)
		{
			return;
		}

		if (FeatureScale <= 0.f)
		{
			VOXEL_MESSAGE(Error, "{0}: FeatureScale must be positive", this);
			return;
		}

		FVoxelNodeStatScope StatScope(*this, Points->Num());

		const FVoxelPointIdBuffer* ParentIds = Points->Find<FVoxelPointIdBuffer>(FVoxelPointAttributes::Id);
		if (!ParentIds)
		{
			VOXEL_MESSAGE(Error, "{0}: Missing attribute Id", this);
			return;
		}

		const FVoxelDoubleVectorBuffer* ParentPositions = Points->Find<FVoxelDoubleVectorBuffer>(FVoxelPointAttributes::Position);
		if (!ParentPositions)
		{
			VOXEL_MESSAGE(Error, "{0}: Missing attribute Position", this);
			return;
		}

		const FVoxelPointRandom ChildIdRandom(Seed, STATIC_HASH("CellularCorners2D_ChildId"));

		const float ScaledJitter = Jitter / 4.f;
		const float InvFeatureScale = 1.0f / FeatureScale;
		const int32 SeedValue = Seed; // FVoxelSeed has operator int32()

		// --- Phase 1: Compute corners for each parent point ---

		// Per-parent corner data. Uses fixed-size array so the struct is
		// trivially destructible (required by TVoxelArray).
		// A Voronoi cell in a 4x4 grid has at most ~12 vertices in practice.
		static constexpr int32 MaxCornersPerCell = 16;

		struct FParentCornerData
		{
			FVector2f Corners[MaxCornersPerCell];
			int32 NumCorners = 0;
			float FloorX = 0.f;
			float FloorY = 0.f;
		};

		TVoxelArray<FParentCornerData> AllCorners;
		FVoxelUtilities::SetNumFast(AllCorners, Points->Num());

		TVoxelArray<int32> AllNumChildren;
		FVoxelUtilities::SetNumFast(AllNumChildren, Points->Num());

		int32 TotalNum = 0;

		// Number of cells in each dimension of the search window
		// Matches TrueDistanceCellularNoise2D: indices -1 to 2 (inclusive) = 4 per axis
		static constexpr int32 GridMin = -1;
		static constexpr int32 GridMax = 2;
		static constexpr int32 GridSize = GridMax - GridMin + 1; // 4
		static constexpr int32 NumCells = GridSize * GridSize;   // 16

		{
			VOXEL_SCOPE_COUNTER("ComputeCorners");

			for (int32 ParentIndex = 0; ParentIndex < Points->Num(); ParentIndex++)
			{
				const FVector ParentPos = (*ParentPositions)[ParentIndex];

				// Convert to grid space (2D, X/Y only)
				const float GridX = static_cast<float>(ParentPos.X) * InvFeatureScale;
				const float GridY = static_cast<float>(ParentPos.Y) * InvFeatureScale;

				const float FloorX = FMath::Floor(GridX);
				const float FloorY = FMath::Floor(GridY);
				const float LocalX = GridX - FloorX;
				const float LocalY = GridY - FloorY;

				const FIntPoint HashPosition(
					static_cast<int32>(FloorX) * PGLCellularNoise::NoisePrimes_X,
					static_cast<int32>(FloorY) * PGLCellularNoise::NoisePrimes_Y);

				// Collect all cell centers in the 4x4 neighborhood
				struct FCellInfo
				{
					FVector2f Center; // grid-local coordinates
				};

				FCellInfo AllCellInfos[NumCells];
				int32 CellCount = 0;

				float MinDistSq = MAX_flt;
				int32 ClosestIdx = 0;

				for (int32 IX = GridMin; IX <= GridMax; IX++)
				{
					for (int32 IY = GridMin; IY <= GridMax; IY++)
					{
						const FVector2f Center = PGLCellularNoise::GetCellCenter(
							SeedValue, HashPosition, IX, IY, ScaledJitter);

						const float DX = Center.X - LocalX;
						const float DY = Center.Y - LocalY;
						const float DistSq = DX * DX + DY * DY;

						AllCellInfos[CellCount] = { Center };

						if (DistSq < MinDistSq)
						{
							MinDistSq = DistSq;
							ClosestIdx = CellCount;
						}
						CellCount++;
					}
				}

				const FVector2f MyCenter = AllCellInfos[ClosestIdx].Center;

				// --- Phase 2: Find all valid Voronoi vertices ---
				// A Voronoi vertex is the circumcenter of (MyCenter, CellB, CellC)
				// where no other cell center is closer to the circumcenter.

				FVector2f Vertices[MaxCornersPerCell];
				int32 NumVertices = 0;

				for (int32 i = 0; i < CellCount; i++)
				{
					if (i == ClosestIdx)
					{
						continue;
					}

					for (int32 j = i + 1; j < CellCount; j++)
					{
						if (j == ClosestIdx)
						{
							continue;
						}

						FVector2f Circumcenter;
						if (!PGLCellularNoise::ComputeCircumcenter(
							MyCenter, AllCellInfos[i].Center, AllCellInfos[j].Center,
							Circumcenter))
						{
							continue; // Degenerate triangle
						}

						// Validate: no other cell center should be closer
						// to the circumcenter than the three defining cells
						const float DistToMy = FVector2f::DistSquared(Circumcenter, MyCenter);

						bool bValid = true;
						for (int32 k = 0; k < CellCount; k++)
						{
							if (k == ClosestIdx || k == i || k == j)
							{
								continue;
							}

							const float DistToK = FVector2f::DistSquared(
								Circumcenter, AllCellInfos[k].Center);

							// If another cell is strictly closer, this is not a valid vertex
							if (DistToK < DistToMy - 1e-4f)
							{
								bValid = false;
								break;
							}
						}

						if (!bValid)
						{
							continue;
						}

						// Deduplicate: check if we already have this vertex
						bool bDuplicate = false;
						for (int32 v = 0; v < NumVertices; v++)
						{
							if (FVector2f::DistSquared(Vertices[v], Circumcenter) < 1e-6f)
							{
								bDuplicate = true;
								break;
							}
						}

						if (!bDuplicate && NumVertices < MaxCornersPerCell)
						{
							Vertices[NumVertices++] = Circumcenter;
						}
					}
				}

				// Sort vertices in CCW angular order around the cell center
				std::sort(Vertices, Vertices + NumVertices,
					[&MyCenter](const FVector2f& A, const FVector2f& B)
					{
						return FMath::Atan2(A.Y - MyCenter.Y, A.X - MyCenter.X)
							 < FMath::Atan2(B.Y - MyCenter.Y, B.X - MyCenter.X);
					});

				FParentCornerData& CornerData = AllCorners[ParentIndex];
				FMemory::Memcpy(CornerData.Corners, Vertices, NumVertices * sizeof(FVector2f));
				CornerData.NumCorners = NumVertices;
				CornerData.FloorX = FloorX;
				CornerData.FloorY = FloorY;
				AllNumChildren[ParentIndex] = NumVertices;
				TotalNum += NumVertices;
			}
		}

		if (TotalNum == 0)
		{
			return;
		}

		// --- Phase 3: Build output point set ---

		FVoxelPointIdBuffer Id;
		FVoxelDoubleVectorBuffer Position;
		FVoxelInt32Buffer CornerIndex;

		Id.Allocate(TotalNum);
		Position.Allocate(TotalNum);
		CornerIndex.Allocate(TotalNum);

		{
			VOXEL_SCOPE_COUNTER("PopulateOutputs");

			int32 WriteIndex = 0;
			for (int32 ParentIndex = 0; ParentIndex < Points->Num(); ParentIndex++)
			{
				const FVector ParentPos = (*ParentPositions)[ParentIndex];
				const FVoxelPointId ParentId = (*ParentIds)[ParentIndex];
				const FParentCornerData& CornerData = AllCorners[ParentIndex];

				for (int32 CI = 0; CI < CornerData.NumCorners; CI++)
				{
					const FVector2f& GridLocalCorner = CornerData.Corners[CI];

					// Convert grid-local corner back to world space
					const double WorldX = (static_cast<double>(CornerData.FloorX)
						+ static_cast<double>(GridLocalCorner.X))
						* static_cast<double>(FeatureScale);
					const double WorldY = (static_cast<double>(CornerData.FloorY)
						+ static_cast<double>(GridLocalCorner.Y))
						* static_cast<double>(FeatureScale);

					const FVoxelPointId ChildId = ChildIdRandom.MakeId(ParentId, CI);

					Id.Set(WriteIndex, ChildId);
					Position.Set(WriteIndex, FVector(WorldX, WorldY, ParentPos.Z));
					CornerIndex.Set(WriteIndex, CI);

					WriteIndex++;
				}
			}
			ensure(WriteIndex == TotalNum);
		}

		// Build result point set following the ScatterPoints pattern
		const TSharedRef<FVoxelPointSet> Result = MakeShared<FVoxelPointSet>();
		Result->SetNum(TotalNum);

		// Replicate parent attributes to children
		TVoxelMap<FName, TSharedPtr<const FVoxelBuffer>> NewBuffers;
		NewBuffers.Reserve(Points->GetAttributes().Num());

		for (const auto& It : Points->GetAttributes())
		{
			const TSharedRef<const FVoxelBuffer> Buffer = It.Value->Replicate(AllNumChildren, TotalNum);
			NewBuffers.Add_EnsureNew(It.Key, Buffer);
		}

		// First add all existing buffers (replicated to match child count)
		for (const auto& It : NewBuffers)
		{
			Result->Add(It.Key, It.Value.ToSharedRef());
		}

		// Then store originals under Parent.{Name}
		for (const auto& It : NewBuffers)
		{
			Result->Add(FVoxelPointAttributes::MakeParent(It.Key), It.Value.ToSharedRef());
		}

		// Override with our computed child attributes
		Result->Add(FVoxelPointAttributes::Id, MoveTemp(Id));
		Result->Add(FVoxelPointAttributes::Position, MoveTemp(Position));
		Result->Add(FName("CornerIndex"), MoveTemp(CornerIndex));

		OutPin.Set(Query, Result);
	};
}
