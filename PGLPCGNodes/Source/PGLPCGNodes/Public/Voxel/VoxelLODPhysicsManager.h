// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelLODPhysicsManager.generated.h"

class UStaticMeshComponent;

USTRUCT()
struct FGuardedActorState
{
	GENERATED_BODY()

	TWeakObjectPtr<AActor> Actor;
	TArray<TWeakObjectPtr<UStaticMeshComponent>> Meshes;
	TArray<FVector> CachedLocations;
	TArray<FRotator> CachedRotations;
	bool bIsFrozen = true;
	FTimerHandle UnfreezeTimerHandle;
};

/**
 * Centralized manager that freezes/unfreezes physics on actors implementing
 * IVoxelPhysicsGuardable based on player distance to the voxel LOD 0->1 boundary.
 *
 * Place one in your level. On BeginPlay it scans for all actors that implement
 * the interface. Late-spawned actors can call RegisterActor.
 */
UCLASS()
class PGLPCGNODES_API AVoxelLODPhysicsManager : public AActor
{
	GENERATED_BODY()

public:
	AVoxelLODPhysicsManager();

	/** Distance at which physics is frozen. Set to just inside your LOD 0->1 boundary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
	float FreezeDistance = 5000.f;

	/** Buffer zone to prevent state flickering at the boundary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
	float Hysteresis = 500.f;

	/** Delay before unfreezing after re-entering the safe zone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
	float FreezeGracePeriod = 0.5f;

	/** Register a late-spawned actor that implements IVoxelPhysicsGuardable. */
	UFUNCTION(BlueprintCallable, Category = "Voxel")
	void RegisterActor(AActor* Actor);

	/** Unregister an actor so its physics is no longer managed. Does not change its current physics state. */
	UFUNCTION(BlueprintCallable, Category = "Voxel")
	void UnregisterActor(AActor* Actor);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	void FreezeActor(FGuardedActorState& State);
	void UnfreezeActor(FGuardedActorState& State);
	void InitializeActorState(FGuardedActorState& State, const FVector& PlayerLocation);

	TArray<FGuardedActorState> GuardedActors;
};
