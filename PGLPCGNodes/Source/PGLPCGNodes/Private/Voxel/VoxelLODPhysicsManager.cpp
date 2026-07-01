// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "Voxel/VoxelLODPhysicsManager.h"
#include "Voxel/VoxelPhysicsGuardable.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "EngineUtils.h"

AVoxelLODPhysicsManager::AVoxelLODPhysicsManager()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AVoxelLODPhysicsManager::BeginPlay()
{
	Super::BeginPlay();

	// Determine player location for initial state evaluation
	FVector PlayerLocation = FVector::ZeroVector;
	bool bHasPlayer = false;

	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (const APawn* Pawn = PC->GetPawn())
		{
			PlayerLocation = Pawn->GetActorLocation();
			bHasPlayer = true;
		}
	}

	// Scan world for all actors implementing the interface
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->Implements<UVoxelPhysicsGuardable>())
		{
			FGuardedActorState State;
			State.Actor = Actor;
			State.Meshes.Reset();

			TArray<UStaticMeshComponent*> Meshes = IVoxelPhysicsGuardable::Execute_GetGuardedMeshComponents(Actor);
			for (UStaticMeshComponent* Mesh : Meshes)
			{
				if (Mesh)
				{
					State.Meshes.Add(Mesh);
				}
			}

			if (State.Meshes.Num() > 0)
			{
				InitializeActorState(State, bHasPlayer ? PlayerLocation : FVector(BIG_NUMBER));
				GuardedActors.Add(MoveTemp(State));
			}
		}
	}
}

void AVoxelLODPhysicsManager::RegisterActor(AActor* Actor)
{
	if (!Actor || !Actor->Implements<UVoxelPhysicsGuardable>())
	{
		return;
	}

	// Don't double-register
	for (const FGuardedActorState& Existing : GuardedActors)
	{
		if (Existing.Actor == Actor)
		{
			return;
		}
	}

	FGuardedActorState State;
	State.Actor = Actor;

	TArray<UStaticMeshComponent*> Meshes = IVoxelPhysicsGuardable::Execute_GetGuardedMeshComponents(Actor);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (Mesh)
		{
			State.Meshes.Add(Mesh);
		}
	}

	if (State.Meshes.Num() == 0)
	{
		return;
	}

	// Evaluate initial state
	FVector PlayerLocation(BIG_NUMBER);
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (const APawn* Pawn = PC->GetPawn())
		{
			PlayerLocation = Pawn->GetActorLocation();
		}
	}

	InitializeActorState(State, PlayerLocation);
	GuardedActors.Add(MoveTemp(State));
}

void AVoxelLODPhysicsManager::UnregisterActor(AActor* Actor)
{
	for (int32 i = GuardedActors.Num() - 1; i >= 0; --i)
	{
		if (GuardedActors[i].Actor == Actor)
		{
			GetWorld()->GetTimerManager().ClearTimer(GuardedActors[i].UnfreezeTimerHandle);
			GuardedActors.RemoveAtSwap(i);
			return;
		}
	}
}

void AVoxelLODPhysicsManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->GetPawn()) return;

	const FVector PlayerLocation = PC->GetPawn()->GetActorLocation();

	for (int32 i = GuardedActors.Num() - 1; i >= 0; --i)
	{
		FGuardedActorState& State = GuardedActors[i];

		// Clean up dead actors
		if (!State.Actor.IsValid())
		{
			GetWorld()->GetTimerManager().ClearTimer(State.UnfreezeTimerHandle);
			GuardedActors.RemoveAtSwap(i);
			continue;
		}

		const float Dist = FVector::Dist(State.Actor->GetActorLocation(), PlayerLocation);

		if (!State.bIsFrozen && Dist > (FreezeDistance - Hysteresis))
		{
			FreezeActor(State);
		}
		else if (State.bIsFrozen && Dist < (FreezeDistance - Hysteresis * 2.f))
		{
			if (!GetWorld()->GetTimerManager().IsTimerActive(State.UnfreezeTimerHandle))
			{
				TWeakObjectPtr<AActor> WeakActor = State.Actor;
				GetWorld()->GetTimerManager().SetTimer(
					State.UnfreezeTimerHandle,
					FTimerDelegate::CreateWeakLambda(this, [this, WeakActor]()
					{
						for (FGuardedActorState& S : GuardedActors)
						{
							if (S.Actor == WeakActor)
							{
								UnfreezeActor(S);
								return;
							}
						}
					}),
					FreezeGracePeriod, false);
			}
		}
	}
}

void AVoxelLODPhysicsManager::InitializeActorState(FGuardedActorState& State, const FVector& PlayerLocation)
{
	// Cache transforms and freeze
	State.CachedLocations.SetNum(State.Meshes.Num());
	State.CachedRotations.SetNum(State.Meshes.Num());

	for (int32 j = 0; j < State.Meshes.Num(); ++j)
	{
		if (UStaticMeshComponent* Mesh = State.Meshes[j].Get())
		{
			State.CachedLocations[j] = Mesh->GetComponentLocation();
			State.CachedRotations[j] = Mesh->GetComponentRotation();
			Mesh->SetSimulatePhysics(false);
			Mesh->SetWorldLocationAndRotation(State.CachedLocations[j], State.CachedRotations[j]);
		}
	}
	State.bIsFrozen = true;

	// If player is close enough, unfreeze immediately
	if (State.Actor.IsValid())
	{
		const float Dist = FVector::Dist(State.Actor->GetActorLocation(), PlayerLocation);
		if (Dist < (FreezeDistance - Hysteresis * 2.f))
		{
			UnfreezeActor(State);
		}
	}
}

void AVoxelLODPhysicsManager::FreezeActor(FGuardedActorState& State)
{
	if (State.bIsFrozen) return;

	State.CachedLocations.SetNum(State.Meshes.Num());
	State.CachedRotations.SetNum(State.Meshes.Num());

	for (int32 j = 0; j < State.Meshes.Num(); ++j)
	{
		if (UStaticMeshComponent* Mesh = State.Meshes[j].Get())
		{
			State.CachedLocations[j] = Mesh->GetComponentLocation();
			State.CachedRotations[j] = Mesh->GetComponentRotation();
			Mesh->SetSimulatePhysics(false);
			Mesh->SetWorldLocationAndRotation(State.CachedLocations[j], State.CachedRotations[j]);
		}
	}

	State.bIsFrozen = true;
	GetWorld()->GetTimerManager().ClearTimer(State.UnfreezeTimerHandle);
}

void AVoxelLODPhysicsManager::UnfreezeActor(FGuardedActorState& State)
{
	if (!State.bIsFrozen) return;

	for (int32 j = 0; j < State.Meshes.Num(); ++j)
	{
		if (UStaticMeshComponent* Mesh = State.Meshes[j].Get())
		{
			Mesh->SetSimulatePhysics(true);
			Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		}
	}

	State.bIsFrozen = false;
}
