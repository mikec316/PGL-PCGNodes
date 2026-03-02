// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PGLVoxelCubicSampler.h"
#include "VoxelLayer.h"
#include "VoxelQuery.h"
#include "VoxelLayers.h"
#include "VoxelPointId.h"
#include "PCGComponent.h"
#include "VoxelPointSet.h"
#include "VoxelPCGUtilities.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGHelpers.h"
#include "Data/PCGSpatialData.h"
#include "Buffer/VoxelFloatBuffers.h"
#include "Buffer/VoxelDoubleBuffers.h"
#include "Scatter/VoxelScatterUtilities.h"
#include "Surface/VoxelSurfaceTypeTable.h"
#include "Surface/VoxelSurfaceTypeBlendBuffer.h"
#include "Utilities/VoxelBufferMathUtilities.h"

FString UPGLVoxelCubicSamplerSettings::GetAdditionalTitleInformation() const
{
	FString Title = "Cubic";
	if (Layer.Layer)
	{
		Title += " (" + Layer.Layer->GetName() + ")";
	}
	return Title;
}

TArray<FPCGPinProperties> UPGLVoxelCubicSamplerSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;

	Properties.Emplace(
		"Bounding Shape",
		EPCGDataType::Spatial,
		false,
		false);

	Properties.Emplace(
		"Dependency",
		EPCGDataType::Any,
		true,
		true);

	return Properties;
}

TArray<FPCGPinProperties> UPGLVoxelCubicSamplerSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
	return Properties;
}

TSharedPtr<FVoxelPCGOutput> UPGLVoxelCubicSamplerSettings::CreateOutput(FPCGContext& Context) const
{
	VOXEL_FUNCTION_COUNTER();

	const UPCGComponent* Component = GetPCGComponent(Context);
	if (!Component)
	{
		return {};
	}

	if (!Layer.IsValid())
	{
		VOXEL_MESSAGE(Error, "Invalid layer");
		return {};
	}

	const FVoxelBox Bounds = INLINE_LAMBDA -> FVoxelBox
	{
		if (bUnbounded &&
			Context.InputData.GetInputCountByPin("Bounding Shape") == 0)
		{
			return FVoxelBox::Infinite;
		}

		FBox Result;
		{
			bool bOutUnionWasCreated = false;
			const UPCGSpatialData* BoundingShape = Context.InputData.GetSpatialUnionOfInputsByPin(&Context, "Bounding Shape", bOutUnionWasCreated);

			if (!BoundingShape)
			{
				ensure(!bOutUnionWasCreated);
				BoundingShape = Cast<UPCGSpatialData>(Component->GetActorPCGData());
			}

			if (!ensure(BoundingShape))
			{
				return {};
			}

			Result = BoundingShape->GetBounds();
		}

		if (!ensureVoxelSlow(Result.IsValid))
		{
			return {};
		}

		{
			const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Component->GetActorPCGData());
			if (!ensure(SpatialData))
			{
				return {};
			}

			const FBox ActorBounds = SpatialData->GetBounds();
			if (!ensure(ActorBounds.IsValid))
			{
				return {};
			}

			if (!Result.Intersect(ActorBounds))
			{
				return {};
			}

			Result = Result.Overlap(ActorBounds);
		}

		if (!ensure(Result.IsValid))
		{
			return {};
		}

		return FVoxelBox(Result);
	};

	if (!Bounds.IsValidAndNotEmpty())
	{
		return {};
	}

	UPCGPointData* PointData = NewObject<UPCGPointData>();
	Context.OutputData.TaggedData.Emplace_GetRef().Data = PointData;

	return MakeShared<FPGLVoxelCubicSamplerOutput>(
		FVoxelLayers::Get(Component->GetWorld()),
		FVoxelSurfaceTypeTable::Get(),
		Bounds,
		Layer,
		LOD,
		VoxelSize,
		ChunkSize,
		bSkipNaNFaces,
		bResolveSmartSurfaceTypes,
		PointData,
		FVoxelMetadataRef::GetUniqueValidRefs(MetadatasToQuery));
}

FString UPGLVoxelCubicSamplerSettings::GetNodeDebugInfo() const
{
	return Super::GetNodeDebugInfo() + " [Layer: " + FString(Layer.Layer ? Layer.Layer->GetName() : "None") + "]";
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Face directions: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
static const FIntVector GFaceNormals[6] = {
	{ 1,  0,  0},
	{-1,  0,  0},
	{ 0,  1,  0},
	{ 0, -1,  0},
	{ 0,  0,  1},
	{ 0,  0, -1}
};

static const FVector3f GFaceNormalVectors[6] = {
	{ 1,  0,  0},
	{-1,  0,  0},
	{ 0,  1,  0},
	{ 0, -1,  0},
	{ 0,  0,  1},
	{ 0,  0, -1}
};

FVoxelFuture FPGLVoxelCubicSamplerOutput::Run() const
{
	return Voxel::AsyncTask([this]
	{
		if (!Layers->HasLayer(WeakLayer, GetDependencyCollector()))
		{
			return FVoxelFuture();
		}

		const FVoxelQuery Query(
			LOD,
			*Layers,
			*SurfaceTypeTable,
			GetDependencyCollector());

		// ─────────────────────────────────────────────────────────────────
		// Corner-sampling face generation.
		//
		// The regular mesher with blockiness samples the distance field at
		// integer grid positions (cell corners). The built-in cubic face
		// generator samples at cell centers (integer + 0.5), which causes
		// the solid/air classification to disagree by half a cell.
		//
		// We implement our own face generation that samples at corners
		// to match the regular mesher's behavior.
		// ─────────────────────────────────────────────────────────────────

		const FVoxelIntBox BoundsWithPadding = FVoxelIntBox::FromFloatBox_WithPadding(Bounds);
		const int32 ChunkSizeWorld = ChunkSize * VoxelSize;

		const FVoxelIntBox ChunkedBounds = FVoxelIntBox(
			FVoxelUtilities::DivideFloor(BoundsWithPadding.Min, ChunkSizeWorld) * ChunkSizeWorld,
			FVoxelUtilities::DivideCeil(BoundsWithPadding.Max, ChunkSizeWorld) * ChunkSizeWorld);

		// DataSize = ChunkSize + 2 for neighbor sampling at boundaries
		const int32 DataSize = ChunkSize + 2;
		const int32 TotalCells = DataSize * DataSize * DataSize;

		// Collect face points across all chunks
		TVoxelArray<FVoxelPointId> AllPointIds;
		TVoxelArray<double> AllPositionsX;
		TVoxelArray<double> AllPositionsY;
		TVoxelArray<double> AllPositionsZ;
		TVoxelArray<float> AllNormalsX;
		TVoxelArray<float> AllNormalsY;
		TVoxelArray<float> AllNormalsZ;
		TVoxelArray<FVoxelSurfaceTypeBlend> AllSurfaceTypes;

		for (int64 ChunkZ = ChunkedBounds.Min.Z; ChunkZ < ChunkedBounds.Max.Z; ChunkZ += ChunkSizeWorld)
		{
			for (int64 ChunkY = ChunkedBounds.Min.Y; ChunkY < ChunkedBounds.Max.Y; ChunkY += ChunkSizeWorld)
			{
				for (int64 ChunkX = ChunkedBounds.Min.X; ChunkX < ChunkedBounds.Max.X; ChunkX += ChunkSizeWorld)
				{
					if (Voxel::ShouldCancel())
					{
						return FVoxelFuture();
					}

					const FInt64Vector ChunkOffset(
						ChunkX / VoxelSize,
						ChunkY / VoxelSize,
						ChunkZ / VoxelSize);

					// Start includes 1-cell padding for neighbor lookups
					const FVector Start = FVector(ChunkOffset - FInt64Vector(1)) * VoxelSize;

					// ── Query distance at CORNER positions (integer grid) ──
					FVoxelDoubleVectorBuffer QueryPositions;
					QueryPositions.Allocate(TotalCells);

					int32 WriteIndex = 0;
					for (int32 Z = 0; Z < DataSize; Z++)
					{
						for (int32 Y = 0; Y < DataSize; Y++)
						{
							for (int32 X = 0; X < DataSize; X++)
							{
								// Sample at CORNERS — matching the regular mesher
								const FVector Position = Start + FVector(X, Y, Z) * VoxelSize;
								QueryPositions.Set(WriteIndex++, Position);
							}
						}
					}

					FVoxelSurfaceTypeBlendBuffer SurfaceTypeBuffer;
					SurfaceTypeBuffer.AllocateZeroed(TotalCells);

					// Query metadata at corners too
					TVoxelMap<FVoxelMetadataRef, TSharedRef<FVoxelBuffer>> ChunkMetadataToBuffer;
					for (const FVoxelMetadataRef& Metadata : MetadatasToQuery)
					{
						if (Metadata.IsValid())
						{
							ChunkMetadataToBuffer.Add_EnsureNew(
								Metadata,
								Metadata.MakeDefaultBuffer(TotalCells));
						}
					}

					const FVoxelFloatBuffer Distances = Query.SampleVolumeLayer(
						WeakLayer,
						QueryPositions,
						SurfaceTypeBuffer.View(),
						ChunkMetadataToBuffer);

					// ── Build solid/air grid from corner values ──
					auto GetGridIndex = [DataSize](int32 X, int32 Y, int32 Z) -> int32
					{
						return X + DataSize * (Y + DataSize * Z);
					};

					auto GetDistance = [&Distances, &GetGridIndex, DataSize](int32 X, int32 Y, int32 Z) -> float
					{
						if (X < 0 || Y < 0 || Z < 0 ||
							X >= DataSize || Y >= DataSize || Z >= DataSize)
						{
							return FVoxelUtilities::NaNf();
						}
						return Distances[GetGridIndex(X, Y, Z)];
					};

					auto IsSolid = [&GetDistance](int32 X, int32 Y, int32 Z) -> bool
					{
						const float D = GetDistance(X, Y, Z);
						return !FVoxelUtilities::IsNaN(D) && D < 0;
					};

					auto IsNaN = [&GetDistance](int32 X, int32 Y, int32 Z) -> bool
					{
						return FVoxelUtilities::IsNaN(GetDistance(X, Y, Z));
					};

					// ── Generate face points at solid/air boundaries ──
					// Iterate only the actual chunk cells [1, DataSize-1) (excluding padding)
					for (int32 Z = 1; Z < DataSize - 1; Z++)
					{
						for (int32 Y = 1; Y < DataSize - 1; Y++)
						{
							for (int32 X = 1; X < DataSize - 1; X++)
							{
								if (!IsSolid(X, Y, Z))
								{
									continue;
								}

								const int32 GridIndex = GetGridIndex(X, Y, Z);
								const FVoxelSurfaceType CellBlockType = SurfaceTypeBuffer[GridIndex].GetTopLayer().Type;

								for (int32 Face = 0; Face < 6; Face++)
								{
									const FIntVector& Normal = GFaceNormals[Face];
									const int32 NX = X + Normal.X;
									const int32 NY = Y + Normal.Y;
									const int32 NZ = Z + Normal.Z;

									if (!IsSolid(NX, NY, NZ) &&
										(!bSkipNaNFaces || !IsNaN(NX, NY, NZ)))
									{
										// Convert from grid coords (with padding) to chunk-local
										const int32 LocalX = X - 1;
										const int32 LocalY = Y - 1;
										const int32 LocalZ = Z - 1;

										// Face center: corner position + half-normal
										// The mesher with blockiness snaps vertices to cell centers,
									// so the face between solid corner S and air S+Normal is at S + Normal*0.5
										const FVector Position = FVector(ChunkOffset) * VoxelSize +
											FVector(LocalX + GFaceNormalVectors[Face].X * 0.5,
											        LocalY + GFaceNormalVectors[Face].Y * 0.5,
											        LocalZ + GFaceNormalVectors[Face].Z * 0.5) * VoxelSize;

										const uint64 PointId = FVoxelUtilities::MurmurHashMulti(
											GetSeed(),
											ChunkOffset.X + LocalX,
											ChunkOffset.Y + LocalY,
											ChunkOffset.Z + LocalZ,
											Face);

										AllPointIds.Add(PointId);
										AllPositionsX.Add(Position.X);
										AllPositionsY.Add(Position.Y);
										AllPositionsZ.Add(Position.Z);
										AllNormalsX.Add(GFaceNormalVectors[Face].X);
										AllNormalsY.Add(GFaceNormalVectors[Face].Y);
										AllNormalsZ.Add(GFaceNormalVectors[Face].Z);

										FVoxelSurfaceTypeBlend Blend;
										Blend.InitializeFromType(CellBlockType);
										AllSurfaceTypes.Add(Blend);
									}
								}
							}
						}
					}
				}
			}
		}

		const int32 Num = AllPointIds.Num();
		if (Num == 0)
		{
			return FVoxelFuture();
		}

		// ─────────────────────────────────────────────────────────────────
		// Build FVoxelPointSet from our generated face points
		// ─────────────────────────────────────────────────────────────────

		FVoxelPointIdBuffer PointIdBuffer;
		PointIdBuffer.Allocate(Num);
		FMemory::Memcpy(PointIdBuffer.View().GetData(), AllPointIds.GetData(), Num * sizeof(FVoxelPointId));

		FVoxelDoubleVectorBuffer Positions;
		Positions.Allocate(Num);
		FMemory::Memcpy(Positions.X.View().GetData(), AllPositionsX.GetData(), Num * sizeof(double));
		FMemory::Memcpy(Positions.Y.View().GetData(), AllPositionsY.GetData(), Num * sizeof(double));
		FMemory::Memcpy(Positions.Z.View().GetData(), AllPositionsZ.GetData(), Num * sizeof(double));

		FVoxelVectorBuffer Normals;
		Normals.Allocate(Num);
		FMemory::Memcpy(Normals.X.View().GetData(), AllNormalsX.GetData(), Num * sizeof(float));
		FMemory::Memcpy(Normals.Y.View().GetData(), AllNormalsY.GetData(), Num * sizeof(float));
		FMemory::Memcpy(Normals.Z.View().GetData(), AllNormalsZ.GetData(), Num * sizeof(float));

		FVoxelSurfaceTypeBlendBuffer SurfaceTypeBlendBuffer;
		SurfaceTypeBlendBuffer.Allocate(Num);
		FMemory::Memcpy(SurfaceTypeBlendBuffer.View().GetData(), AllSurfaceTypes.GetData(), Num * sizeof(FVoxelSurfaceTypeBlend));

		FVoxelPointSet PointSet;
		PointSet.SetNum(Num);
		PointSet.Add(FVoxelPointAttributes::Id, MakeSharedCopy(MoveTemp(PointIdBuffer)));
		PointSet.Add(FVoxelPointAttributes::Position, MakeSharedCopy(MoveTemp(Positions)));
		PointSet.Add(FVoxelPointAttributes::Rotation, MakeSharedCopy(FVoxelBufferMathUtilities::MakeQuaternionFromZ(Normals)));
		PointSet.Add(FVoxelPointAttributes::SurfaceTypes, MakeSharedCopy(MoveTemp(SurfaceTypeBlendBuffer)));

		// ─────────────────────────────────────────────────────────────────
		// Query metadata at face positions (same workaround as before)
		// ─────────────────────────────────────────────────────────────────
		if (MetadatasToQuery.Num() > 0)
		{
			const FVoxelDoubleVectorBuffer* PositionBufferForMeta =
				PointSet.Find<FVoxelDoubleVectorBuffer>(FVoxelPointAttributes::Position);

			if (ensure(PositionBufferForMeta))
			{
				TVoxelMap<FVoxelMetadataRef, TSharedRef<FVoxelBuffer>> MetadataToBuffer;
				MetadataToBuffer.Reserve(MetadatasToQuery.Num());

				for (const FVoxelMetadataRef& Metadata : MetadatasToQuery)
				{
					if (!Metadata.IsValid())
					{
						continue;
					}

					MetadataToBuffer.Add_EnsureNew(
						Metadata,
						Metadata.MakeDefaultBuffer(Num));
				}

				if (MetadataToBuffer.Num() > 0)
				{
					FVoxelSurfaceTypeBlendBuffer DummySurfaceTypes;
					DummySurfaceTypes.AllocateZeroed(Num);

					(void)Query.SampleVolumeLayer(
						WeakLayer,
						*PositionBufferForMeta,
						DummySurfaceTypes.View(),
						MetadataToBuffer);

					for (const auto& It : MetadataToBuffer)
					{
						PointSet.Add(It.Key.GetFName(), It.Value);
					}
				}
			}
		}

		// ─────────────────────────────────────────────────────────────────
		// Convert FVoxelPointSet → FPCGPoint array
		// ─────────────────────────────────────────────────────────────────

		TVoxelArray<FPCGPoint> Points;
		FVoxelUtilities::SetNumFast(Points, Num);

		const FVoxelPointIdBuffer* IdBuffer = PointSet.Find<FVoxelPointIdBuffer>(FVoxelPointAttributes::Id);
		const FVoxelDoubleVectorBuffer* PositionBuffer = PointSet.Find<FVoxelDoubleVectorBuffer>(FVoxelPointAttributes::Position);
		const FVoxelQuaternionBuffer* RotationBuffer = PointSet.Find<FVoxelQuaternionBuffer>(FVoxelPointAttributes::Rotation);
		const FVoxelSurfaceTypeBlendBuffer* SurfaceTypesBuffer = PointSet.Find<FVoxelSurfaceTypeBlendBuffer>(FVoxelPointAttributes::SurfaceTypes);

		if (!ensure(IdBuffer) ||
			!ensure(PositionBuffer) ||
			!ensure(RotationBuffer) ||
			!ensure(SurfaceTypesBuffer))
		{
			return FVoxelFuture();
		}

		const FBox PointBounds(FVector(-0.5 * VoxelSize), FVector(0.5 * VoxelSize));

		TVoxelMap<FVoxelSurfaceType, TVoxelArray<float>> SurfaceTypeToWeight;
		SurfaceTypeToWeight.Reserve(32);

		const FRandomStream Stream(GetSeed());
		for (int32 Index = 0; Index < Num; Index++)
		{
			FPCGPoint& Point = Points[Index];
			Point = FPCGPoint();

			Point.Transform = FTransform(
				FQuat((*RotationBuffer)[Index]),
				(*PositionBuffer)[Index]);

			Point.Density = Stream.FRand();
			Point.Seed = (*IdBuffer)[Index].PointId;
			Point.MetadataEntry = Index;

			Point.SetLocalBounds(PointBounds);

			for (const FVoxelSurfaceTypeBlendLayer& Layer : (*SurfaceTypesBuffer)[Index].GetLayers())
			{
				if (!SurfaceTypeToWeight.Contains(Layer.Type))
				{
					SurfaceTypeToWeight.Add_CheckNew(Layer.Type).SetNumZeroed(Num);
				}
				SurfaceTypeToWeight.FindChecked(Layer.Type)[Index] += Layer.Weight.ToFloat();
			}
		}

		return Voxel::GameTask([
			=,
			this,
			Points = MakeSharedCopy(MoveTemp(Points)),
			PointSet = MoveTemp(PointSet),
			SurfaceTypeToWeight = MoveTemp(SurfaceTypeToWeight)]
		{
			VOXEL_FUNCTION_COUNTER();
			check(IsInGameThread());

			UPCGPointData* PointData = WeakPointData.Resolve();
			if (!ensure(PointData))
			{
				return;
			}

			FVoxelPCGUtilities::AddPointsToMetadata(*PointData->Metadata, *Points);

			for (const auto& It : SurfaceTypeToWeight)
			{
				FVoxelPCGUtilities::AddAttribute<float>(
					*PointData->Metadata,
					*Points,
					It.Key.GetFName(),
					MakeVoxelArrayView(It.Value).LeftOf(Points->Num()));
			}

			FVoxelSurfaceType::ForeachSurfaceType([&](const FVoxelSurfaceType& Type)
			{
				FVoxelPCGUtilities::AddDefaultAttributeIfNeeded<float>(
					*PointData->Metadata,
					Type.GetFName());
			});

			for (const FVoxelMetadataRef MetadataToQuery : MetadatasToQuery)
			{
				const FVoxelBuffer* MetadataBuffer = PointSet.Find(MetadataToQuery.GetFName());
				if (!MetadataBuffer)
				{
					continue;
				}

				MetadataToQuery.AddToPCG(
					*PointData->Metadata,
					*Points,
					MetadataToQuery.GetFName(),
					*MetadataBuffer);
			}

			PointData->GetMutablePoints() = MoveTemp(*Points);
		});
	});
}
