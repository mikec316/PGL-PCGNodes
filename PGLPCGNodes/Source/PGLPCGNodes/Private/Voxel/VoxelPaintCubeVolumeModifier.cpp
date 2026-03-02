// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "Voxel/VoxelPaintCubeVolumeModifier.h"
#include "Sculpt/Volume/VoxelVolumeSculptCanvasWriter.h"
#include "Surface/VoxelSurfaceTypeBlend.h"
#include "Buffer/VoxelBaseBuffers.h"

TSharedPtr<IVoxelVolumeModifierRuntime> FVoxelPaintCubeVolumeModifier::GetRuntime() const
{
	VOXEL_FUNCTION_COUNTER();
	check(IsInGameThread());

	return MakeShared<FVoxelPaintCubeVolumeModifierRuntime>(
		Center,
		Size,
		Rotation,
		Strength,
		Mode,
		FVoxelSurfaceType(SurfaceTypeToPaint),
		MetadatasToPaint.CreateRuntime());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelBox FVoxelPaintCubeVolumeModifierRuntime::GetBounds() const
{
	// Create a bounding box from center and size
	return FVoxelBox(Center - Size * 0.5f, Center + Size * 0.5f);
}

void FVoxelPaintCubeVolumeModifierRuntime::Apply(FVoxelVolumeSculptCanvasWriter& Writer) const
{
	VOXEL_FUNCTION_COUNTER();

	const FIntVector VoxelSize = Writer.Size;
	const FMatrix IndexToWorld = Writer.IndexToWorld;
	const FVoxelIntBox Indices = Writer.Indices;

	TVoxelOptional<FVoxelVolumeSculptCanvasWriter::FSurfaceTypes> SurfaceTypes;
	if (!SurfaceType.IsNull())
	{
		SurfaceTypes = Writer.GetSurfaceTypes();
	}

	TVoxelArray<FVoxelVolumeSculptCanvasWriter::FMetadata> Metadatas;
	for (const auto& It : MetadataOverrides->MetadataToValue)
	{
		Metadatas.Add(Writer.GetMetadata(It.Key));
	}

	FVoxelFloatBuffer Alphas;
	FVoxelInt32Buffer Indirection;

	Alphas.Allocate(Indices.Count_int32());
	Indirection.Allocate(Indices.Count_int32());

	FVoxelCounter32 AtomicWriteIndex;

	const FVector HalfSize = Size * 0.5f;

	Voxel::ParallelFor(Indices.GetZ(), [&](const int32 IndexZ)
	{
		for (int32 IndexY = Indices.Min.Y; IndexY < Indices.Max.Y; IndexY++)
		{
			for (int32 IndexX = Indices.Min.X; IndexX < Indices.Max.X; IndexX++)
			{
				const int32 Index = FVoxelUtilities::Get3DIndex<int32>(VoxelSize, IndexX, IndexY, IndexZ);
				const FVector Position = IndexToWorld.TransformPosition(FVector(IndexX, IndexY, IndexZ));

				// Transform position to cube's local space
				const FVector PositionFromCenter = Rotation.UnrotateVector(Position - Center);

				// Calculate distance to cube (using signed distance field)
				const FVector Q = PositionFromCenter.GetAbs() - HalfSize;
				const double Length = FVector::Max(Q, FVector::ZeroVector).Size();
				const float DistanceToCube = Length + FMath::Min(0.f, FMath::Max3(Q.X, Q.Y, Q.Z));

				// Only paint inside the cube (or close to it based on strength)
				if (DistanceToCube > 0.f)
				{
					continue;
				}

				// Alpha is based on distance to cube edge and strength
				// Inside the cube: alpha = strength
				// Near edges: can add falloff if desired
				float Alpha = Strength;
				Alpha = FMath::Clamp(Alpha, 0.f, 1.f);

				if (Alpha == 0.f)
				{
					continue;
				}

				if (Mode == EVoxelSculptMode::Remove)
				{
					if (SurfaceTypes)
					{
						float& SurfaceTypeAlpha = SurfaceTypes->Alphas[Index];

						SurfaceTypeAlpha = FMath::Lerp(
							SurfaceTypeAlpha,
							0.f,
							Alpha);
					}

					for (const FVoxelVolumeSculptCanvasWriter::FMetadata& Metadata : Metadatas)
					{
						float& MetadataAlpha = Metadata.Alphas[Index];

						MetadataAlpha = FMath::Lerp(
							MetadataAlpha,
							0.f,
							Alpha);
					}

					continue;
				}
				checkVoxelSlow(Mode == EVoxelSculptMode::Add);

				if (SurfaceTypes)
				{
					float& SurfaceTypeAlpha = SurfaceTypes->Alphas[Index];

					SurfaceTypeAlpha = FMath::Lerp(
						SurfaceTypeAlpha,
						1.f,
						Alpha);
				}

				for (const FVoxelVolumeSculptCanvasWriter::FMetadata& Metadata : Metadatas)
				{
					float& MetadataAlpha = Metadata.Alphas[Index];

					MetadataAlpha = FMath::Lerp(
						MetadataAlpha,
						1.f,
						Alpha);
				}

				const int32 WriteIndex = AtomicWriteIndex.Increment_ReturnOld();

				Alphas.Set(WriteIndex, Alpha);
				Indirection.Set(WriteIndex, Index);
			}
		}
	});

	const int32 Num = AtomicWriteIndex.Get();

	if (Num == 0)
	{
		return;
	}

	Alphas.ShrinkTo(Num);
	Indirection.ShrinkTo(Num);

	if (SurfaceTypes)
	{
		VOXEL_SCOPE_COUNTER_NUM("SurfaceTypes", Num);

		Voxel::ParallelFor(Num, [&](const int32 Index)
		{
			FVoxelSurfaceTypeBlend& SurfaceTypeBlend = SurfaceTypes->Blends[Indirection[Index]];

			FVoxelSurfaceTypeBlend::Lerp(
				SurfaceTypeBlend,
				SurfaceTypeBlend,
				SurfaceType,
				Alphas[Index]);
		});
	}

	for (const auto& It : MetadataOverrides->MetadataToValue)
	{
		VOXEL_SCOPE_COUNTER_FORMAT("Metadata %s Num=%d", *It.Key.GetName(), Num);

		It.Key.IndirectBlend(
			Indirection.View(),
			*It.Value.Constant,
			Alphas,
			*Writer.GetMetadata(It.Key).Buffer);
	}
}
