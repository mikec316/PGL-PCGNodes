// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VoxelPhysicsGuardable.generated.h"

class UStaticMeshComponent;

UINTERFACE(Blueprintable, MinimalAPI)
class UVoxelPhysicsGuardable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implement this interface on any actor whose static mesh physics should be
 * managed by AVoxelLODPhysicsManager. Override GetGuardedMeshComponents to
 * return the meshes that should be frozen/unfrozen based on player distance.
 */
class PGLPCGNODES_API IVoxelPhysicsGuardable
{
	GENERATED_BODY()

public:
	/** Return the mesh components whose physics simulation should be guarded. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Voxel")
	TArray<UStaticMeshComponent*> GetGuardedMeshComponents() const;
};
