// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelMetadataOverrides.h"
#include "Surface/VoxelSurfaceType.h"
#include "Sculpt/VoxelSculptMode.h"
#include "Sculpt/Volume/VoxelVolumeModifier.h"
#include "VoxelPaintCubeVolumeModifier.generated.h"

/**
 * Modifier for painting surface types on a cube-shaped volume
 */
USTRUCT()
struct PGLPCGNODES_API FVoxelPaintCubeVolumeModifier : public FVoxelVolumeModifier
{
	GENERATED_BODY()
	GENERATED_VIRTUAL_STRUCT_BODY()

	UPROPERTY()
	FVector Center = FVector(ForceInit);

	UPROPERTY()
	FVector Size = FVector(ForceInit);

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY()
	TObjectPtr<UVoxelSurfaceTypeInterface> SurfaceTypeToPaint;

	UPROPERTY()
	FVoxelMetadataOverrides MetadatasToPaint;

	UPROPERTY()
	float Strength = 0.f;

	UPROPERTY()
	EVoxelSculptMode Mode = {};

	//~ Begin FVoxelVolumeModifier Interface
	virtual TSharedPtr<IVoxelVolumeModifierRuntime> GetRuntime() const override;
	//~ End FVoxelVolumeModifier Interface
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

/**
 * Runtime implementation of cube paint modifier
 */
class PGLPCGNODES_API FVoxelPaintCubeVolumeModifierRuntime : public IVoxelVolumeModifierRuntime
{
public:
	const FVector Center;
	const FVector Size;
	const FRotator Rotation;
	const float Strength;
	const EVoxelSculptMode Mode;
	const FVoxelSurfaceType SurfaceType;
	const TSharedRef<FVoxelRuntimeMetadataOverrides> MetadataOverrides;

	FVoxelPaintCubeVolumeModifierRuntime(
		const FVector& Center,
		const FVector& Size,
		const FRotator& Rotation,
		const float Strength,
		const EVoxelSculptMode Mode,
		const FVoxelSurfaceType& SurfaceType,
		const TSharedRef<FVoxelRuntimeMetadataOverrides>& MetadataOverrides)
		: Center(Center)
		, Size(Size)
		, Rotation(Rotation)
		, Strength(Strength)
		, Mode(Mode)
		, SurfaceType(SurfaceType)
		, MetadataOverrides(MetadataOverrides)
	{
	}

public:
	//~ Begin IVoxelVolumeModifierRuntime Interface
	virtual FVoxelBox GetBounds() const override;
	virtual void Apply(FVoxelVolumeSculptCanvasWriter& Writer) const override;
	//~ End IVoxelVolumeModifierRuntime Interface
};
