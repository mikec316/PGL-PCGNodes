// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "Voxel/PGLSculptingComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Sculpt/Volume/VoxelSculptVolume.h"
#include "Sculpt/Volume/VoxelVolumeSculptBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"

UPGLSculptingComponent::UPGLSculptingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UPGLSculptingComponent::BeginPlay()
{
	Super::BeginPlay();

	// Cache player controller reference - component should be on a PlayerController
	PlayerController = Cast<APlayerController>(GetOwner());
	if (PlayerController)
	{
		bIsLocalPlayer = PlayerController->IsLocalController();
	}
}

void UPGLSculptingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clean up widgets
	if (BuildingWidget)
	{
		BuildingWidget->RemoveFromParent();
		BuildingWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UPGLSculptingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsLocalPlayer)
	{
		UpdateProcess();
	}
}

void UPGLSculptingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UPGLSculptingComponent, BuildingMode);
	DOREPLIFETIME(UPGLSculptingComponent, SculptObject);
}

// ==================== Public Functions ====================

void UPGLSculptingComponent::SetSculptingMode(EVP_SculptStyle NewMode)
{
	const EVP_SculptStyle OldMode = BuildingMode;
	BuildingMode = NewMode;

	// Enable/disable tick based on mode
	const bool bShouldTick = (NewMode != EVP_SculptStyle::None);
	SetComponentTickEnabled(bShouldTick);

	// Reset state when entering new mode
	if (NewMode != EVP_SculptStyle::None)
	{
		bIsLeftMouseDown = false;
		InitialHitResult = FHitResult();
	}

	if (OldMode != NewMode)
	{
		OnSculptingModeChanged.Broadcast(NewMode);
	}
}

void UPGLSculptingComponent::SetSculptingObject(const FPGLSculptingObjectSettings& NewSettings)
{
	SculptObject = NewSettings;
	SculptSize = NewSettings.SculptSize;
	SculptOffset = NewSettings.SculptOffset;
	OnTargetSculptingObjectChange.Broadcast(NewSettings);
}

void UPGLSculptingComponent::TrySculpt()
{
	if (!CheckBuildingRequirements())
	{
		// Send local feedback - requirements not met
		FPGLSculptingMessage Message(
			EPGLBuildingMode::Build,
			EPGLSculptingInteractionResult::InsufficientResources,
			FText::FromString(TEXT("Insufficient resources to sculpt"))
		);
		OnSculptingMessageReceived.Broadcast(Message);
		return;
	}

	if (!SculptPreset.bValid)
	{
		// No valid sculpt position
		FPGLSculptingMessage Message(
			EPGLBuildingMode::Build,
			EPGLSculptingInteractionResult::InvalidTarget,
			FText::FromString(TEXT("Invalid sculpt location"))
		);
		OnSculptingMessageReceived.Broadcast(Message);
		return;
	}

	// Build the transform from current sculpt preset
	BuildTransform = FTransform(FinalBuildRotation, SculptPreset.Centre, FVector::OneVector);

	// Call server RPC to perform the sculpt
	Server_FinishBuild(SculptObject.Handle, BuildTransform, TargetActor);
}

void UPGLSculptingComponent::CancelBuild()
{
	Server_TryCancelBuild();
	SetSculptingMode(EVP_SculptStyle::None);
}

bool UPGLSculptingComponent::GetTraceHitResult(FHitResult& OutHitResult)
{
	if (!PlayerController)
	{
		return false;
	}

	// Trace forward from player camera
	FVector CameraLocation;
	FRotator CameraRotation;
	PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);

	ForwardVector = CameraRotation.Vector();
	const FVector TraceStart = CameraLocation;
	const FVector TraceEnd = CameraLocation + (ForwardVector * TraceDistance);

	// Setup collision query
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActors(GetFloorIgnoringActors());
	QueryParams.bTraceComplex = true;

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const bool bHit = World->LineTraceSingleByChannel(OutHitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams);

	if (bHit)
	{
		HitResult = OutHitResult;
	}

	return bHit;
}

bool UPGLSculptingComponent::UpdateTargetActor(const FHitResult& InHitResult, bool bSucceed, AActor*& OutTargetActor)
{
	if (!bSucceed)
	{
		OutTargetActor = nullptr;
		SculptPreset.bValid = false;
		return false;
	}

	// Check for left mouse button state (for drag initialization)
	bool bLeftMouseDownNow = false;
	if (PlayerController)
	{
		bLeftMouseDownNow = PlayerController->IsInputKeyDown(EKeys::LeftMouseButton);
	}

	// Store initial hit result when mouse button is first pressed (start of drag)
	if (bLeftMouseDownNow && !bIsLeftMouseDown)
	{
		InitialHitResult = InHitResult;
	}
	bIsLeftMouseDown = bLeftMouseDownNow;

	// Use initial hit if we're dragging, otherwise use current hit for both
	const FHitResult& StartHit = bIsLeftMouseDown ? InitialHitResult : InHitResult;
	const FHitResult& CurrentHit = InHitResult;

	// Determine if we're in remove mode (shift key)
	const bool bRemoveMode = IsRemoveModeActive();

	// Compute sculpt params using existing library function
	SculptPreset = UPGLVoxelBlueprintLibrary::ComputeSculptParamsFromHitResult(
		StartHit,
		CurrentHit,
		GridSize,
		GetCurrentSculptSize(),
		bRemoveMode,
		false // bPaintOnly
	);

	// Update sculpt location for external access
	SculptLocation = SculptPreset.Centre;

	// Determine preview color based on mode
	const FLinearColor PreviewColor = bRemoveMode ? FLinearColor::Red : FLinearColor::Green;

	// Draw preview box
	DrawSculptPreviewBox(PreviewColor);

	// Update target actor
	AActor* OldTarget = TargetActor;
	OutTargetActor = InHitResult.GetActor();
	TargetActor = OutTargetActor;

	// Check if placement is valid
	bCanBeBuilt = SculptPreset.bValid && SculptVolume != nullptr;

	// Fire event if target changed
	if (OldTarget != TargetActor)
	{
		OnTargetActorChanged.Broadcast(OldTarget, TargetActor);
	}

	return true;
}

void UPGLSculptingComponent::ShowBuildingMenu()
{
	if (!bIsLocalPlayer || !BuildingWidgetClass)
	{
		return;
	}

	if (!BuildingWidget)
	{
		BuildingWidget = CreateWidget<UUserWidget>(PlayerController, BuildingWidgetClass);
	}

	if (BuildingWidget && !BuildingWidget->IsInViewport())
	{
		BuildingWidget->AddToViewport();
	}
}

void UPGLSculptingComponent::HideBuildingMenu()
{
	if (BuildingWidget && BuildingWidget->IsInViewport())
	{
		BuildingWidget->RemoveFromParent();
	}
}

void UPGLSculptingComponent::PrintDebugInformation()
{
	if (!GEngine)
	{
		return;
	}

	const FString ModeStr = StaticEnum<EVP_SculptStyle>()->GetNameStringByValue(static_cast<int64>(BuildingMode));
	const FString DebugStr = FString::Printf(
		TEXT("Sculpting | Mode: %s | CanBuild: %s | Location: %s | Size: %s"),
		*ModeStr,
		bCanBeBuilt ? TEXT("Yes") : TEXT("No"),
		*SculptPreset.Centre.ToString(),
		*SculptPreset.Size.ToString()
	);

	DebugInformation = FText::FromString(DebugStr);

	// Print to screen for local player
	if (bIsLocalPlayer)
	{
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan, DebugStr);
	}
}

bool UPGLSculptingComponent::CheckBuildingRequirements() const
{
	// Check if we have a valid sculpt volume
	if (!SculptVolume)
	{
		return false;
	}

	// Check if sculpt preset is valid
	if (!SculptPreset.bValid)
	{
		return false;
	}

	// TODO: Integrate with inventory system to check for required items
	// For now, return true if we have a valid location and volume
	// In a real implementation, you would check:
	// - SculptObject.RequiredItem against player's inventory
	// - Resource costs (mana, materials, etc.)

	return true;
}

// ==================== Server RPCs ====================

void UPGLSculptingComponent::Server_TryStartBuildObject_Implementation(
	FDataTableRowHandle BuildingObjectHandle,
	FTransform InBuildTransform,
	AActor* InTargetActor)
{
	// Store the building object handle
	LastBuildingObjectHandle = BuildingObjectHandle;
	BuildTransform = InBuildTransform;

	// Send acknowledgment to client
	Client_SendBuildingMessage(
		EPGLBuildingMode::Build,
		EPGLSculptingInteractionResult::Success,
		FText::FromString(TEXT("Build started"))
	);
}

bool UPGLSculptingComponent::Server_TryStartBuildObject_Validate(
	FDataTableRowHandle BuildingObjectHandle,
	FTransform InBuildTransform,
	AActor* InTargetActor)
{
	// Basic validation
	return true;
}

void UPGLSculptingComponent::Server_TryCancelBuild_Implementation()
{
	// Reset building state
	LastBuildingObjectHandle = FDataTableRowHandle();

	Client_SendBuildingMessage(
		EPGLBuildingMode::None,
		EPGLSculptingInteractionResult::Success,
		FText::FromString(TEXT("Build cancelled"))
	);
}

bool UPGLSculptingComponent::Server_TryCancelBuild_Validate()
{
	return true;
}

void UPGLSculptingComponent::Server_FinishBuild_Implementation(
	FDataTableRowHandle BuildingObjectHandle,
	FTransform InBuildTransform,
	AActor* InTargetActor)
{
	// Send initial feedback
	Client_SendBuildingMessage(
		EPGLBuildingMode::Build,
		EPGLSculptingInteractionResult::Success,
		FText::FromString(TEXT("Sculpting..."))
	);

	// Execute sculpt operation on the voxel volume
	if (SculptVolume && SculptPreset.bValid)
	{
		const FVector Centre = InBuildTransform.GetLocation();
		const FVector Size = SculptPreset.Size;
		const FRotator Rotation = InBuildTransform.Rotator();

		// Sculpt the geometry (add or remove voxel material)
		UVoxelVolumeSculptBlueprintLibrary::SculptCube(
			SculptVolume,
			Centre,
			Size,
			Rotation,
			0.0f, // Roundness
			SculptPreset.SculptMode,
			0.0f  // Smoothness
		);

		// Paint the surface type if specified
		if (SculptObject.VoxelSurfaceType)
		{
			UPGLVoxelBlueprintLibrary::PaintCube(
				SculptVolume,
				Centre,
				Size,
				Rotation,
				1.0f, // Strength
				SculptPreset.SculptMode,
				SculptObject.VoxelSurfaceType,
				FVoxelMetadataOverrides()
			);
		}
	}

	// Complete requirements (consume resources)
	CompleteBuildingRequirements();

	// Send completion feedback
	Client_SendBuildingMessage(
		EPGLBuildingMode::Build,
		EPGLSculptingInteractionResult::Success,
		FText::FromString(TEXT("Sculpt complete!"))
	);

	// Broadcast completion events (these will fire on the server)
	OnSculptCompleted.Broadcast(InBuildTransform);
	OnSculptingInteractionComplete.Broadcast();
}

bool UPGLSculptingComponent::Server_FinishBuild_Validate(
	FDataTableRowHandle BuildingObjectHandle,
	FTransform InBuildTransform,
	AActor* InTargetActor)
{
	// Validate that we have a sculpt volume
	return SculptVolume != nullptr;
}

// ==================== Client RPCs ====================

void UPGLSculptingComponent::Client_SendBuildingMessage_Implementation(
	EPGLBuildingMode InBuildingMode,
	EPGLSculptingInteractionResult InteractionResult,
	const FText& Message)
{
	FPGLSculptingMessage SculptMessage(InBuildingMode, InteractionResult, Message);
	OnSculptingMessageReceived.Broadcast(SculptMessage);
}

void UPGLSculptingComponent::Client_ChangeBuildingWidget_Implementation(
	TSubclassOf<UUserWidget> NewWidgetClass)
{
	// Remove current widget if exists
	if (BuildingWidget)
	{
		BuildingWidget->RemoveFromParent();
		BuildingWidget = nullptr;
	}

	// Set new widget class
	BuildingWidgetClass = NewWidgetClass;
}

// ==================== Replication Callbacks ====================

void UPGLSculptingComponent::OnRep_BuildingMode()
{
	// Update tick state based on replicated mode
	const bool bShouldTick = (BuildingMode != EVP_SculptStyle::None);
	SetComponentTickEnabled(bShouldTick);

	OnSculptingModeChanged.Broadcast(BuildingMode);
}

// ==================== Internal Functions ====================

void UPGLSculptingComponent::UpdateProcess()
{
	if (!bIsLocalPlayer)
	{
		return;
	}

	// Print debug info if enabled
	PrintDebugInformation();

	switch (BuildingMode)
	{
	case EVP_SculptStyle::None:
		// Do nothing - tick should be disabled in this state
		break;

	case EVP_SculptStyle::FreeSculpt:
	case EVP_SculptStyle::SculptSetPiece:
		{
			// Perform trace and update target
			FHitResult LocalHitResult;
			const bool bHitSuccess = GetTraceHitResult(LocalHitResult);

			AActor* OutTargetActor = nullptr;
			UpdateTargetActor(LocalHitResult, bHitSuccess, OutTargetActor);
		}
		break;
	}
}

void UPGLSculptingComponent::DrawSculptPreviewBox(const FLinearColor& Color)
{
	if (!SculptPreset.bValid)
	{
		return;
	}

	// Use existing library function for consistent preview rendering
	UPGLVoxelBlueprintLibrary::DrawSculptPreviewBox(
		this,
		SculptPreset,
		Color,
		2.0f,  // Thickness
		0.0f   // Lifetime of 0 = one frame
	);
}

void UPGLSculptingComponent::CompleteBuildingRequirements()
{
	// TODO: Integrate with inventory system to consume required items
	// This would typically:
	// 1. Remove SculptObject.RequiredItem from player inventory
	// 2. Deduct mana/resources based on SculptManaCost
	// 3. Update any building progress tracking
}

TArray<AActor*> UPGLSculptingComponent::GetFloorIgnoringActors() const
{
	TArray<AActor*> IgnoredActors;

	// Ignore owner (PlayerController)
	if (AActor* Owner = GetOwner())
	{
		IgnoredActors.Add(Owner);
	}

	// Ignore player pawn
	if (PlayerController)
	{
		if (APawn* Pawn = PlayerController->GetPawn())
		{
			IgnoredActors.Add(Pawn);
		}
	}

	return IgnoredActors;
}

FVector UPGLSculptingComponent::GetCurrentSculptSize() const
{
	// Use object settings if valid, otherwise use default
	if (SculptObject.IsValid() && !SculptObject.SculptSize.IsNearlyZero())
	{
		return SculptObject.SculptSize;
	}
	return DefaultSculptSize;
}

bool UPGLSculptingComponent::IsRemoveModeActive() const
{
	if (!PlayerController)
	{
		return false;
	}

	// Check if either shift key is held
	return PlayerController->IsInputKeyDown(EKeys::LeftShift) ||
	       PlayerController->IsInputKeyDown(EKeys::RightShift);
}
