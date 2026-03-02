// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Voxel/PGLSculptingTypes.h"
#include "Voxel/PGLVoxelBlueprintLibrary.h"
#include "PGLSculptingComponent.generated.h"

// Forward declarations
class APlayerController;
class AVoxelSculptVolume;
class UUserWidget;

// ==================== Event Dispatchers ====================

/** Called when sculpting mode changes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSculptingModeChanged, EVP_SculptStyle, NewMode);

/** Called when a sculpting message is received (feedback to player) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSculptingMessageReceived, const FPGLSculptingMessage&, Message);

/** Called when any sculpting interaction completes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSculptingInteractionComplete);

/** Called when a sculpt operation completes successfully */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSculptCompleted, const FTransform&, SculptTransform);

/** Called when a removal operation completes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRemovingCompleted, AActor*, RemovedActor);

/** Called when a destruction operation completes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDestructionCompleted, AActor*, DestroyedActor);

/** Called when an upgrade operation completes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUpgradingCompleted);

/** Called when a rotation operation completes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRotationCompleted, const FRotator&, NewRotation);

/** Called when target actor changes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTargetActorChanged, AActor*, OldTarget, AActor*, NewTarget);

/** Called when the sculpting object settings change */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetSculptingObjectChange, const FPGLSculptingObjectSettings&, NewObject);

/**
 * Component that enables players to sculpt voxel terrain.
 * Attach to PlayerController. Requires player to have specific items to sculpt.
 * Right-click opens sculpting menu, left-click performs sculpt operation.
 *
 * Features:
 * - Network replicated for multiplayer support
 * - Preview box visualization while placing
 * - Configurable grid snapping
 * - Event dispatchers for UI binding
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PGLPCGNODES_API UPGLSculptingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPGLSculptingComponent();

protected:
	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UActorComponent Interface

public:
	// ==================== Event Dispatchers ====================

	/** Called when sculpting mode changes */
	UPROPERTY(BlueprintAssignable, Category = "Sculpting|Events")
	FOnSculptingModeChanged OnSculptingModeChanged;

	/** Called when a sculpting message is received (feedback to player) */
	UPROPERTY(BlueprintAssignable, Category = "Sculpting|Events")
	FOnSculptingMessageReceived OnSculptingMessageReceived;

	/** Called when any sculpting interaction completes */
	UPROPERTY(BlueprintAssignable, Category = "Sculpting|Events")
	FOnSculptingInteractionComplete OnSculptingInteractionComplete;

	/** Called when a sculpt operation completes successfully */
	UPROPERTY(BlueprintAssignable, Category = "Sculpting|Events")
	FOnSculptCompleted OnSculptCompleted;

	/** Called when a removal operation completes */
	UPROPERTY(BlueprintAssignable, Category = "Sculpting|Events")
	FOnRemovingCompleted OnRemovingCompleted;

	/** Called when a destruction operation completes */
	UPROPERTY(BlueprintAssignable, Category = "Sculpting|Events")
	FOnDestructionCompleted OnDestructionCompleted;

	/** Called when an upgrade operation completes */
	UPROPERTY(BlueprintAssignable, Category = "Sculpting|Events")
	FOnUpgradingCompleted OnUpgradingCompleted;

	/** Called when a rotation operation completes */
	UPROPERTY(BlueprintAssignable, Category = "Sculpting|Events")
	FOnRotationCompleted OnRotationCompleted;

	/** Called when target actor changes */
	UPROPERTY(BlueprintAssignable, Category = "Sculpting|Events")
	FOnTargetActorChanged OnTargetActorChanged;

	/** Called when the sculpting object settings change */
	UPROPERTY(BlueprintAssignable, Category = "Sculpting|Events")
	FOnTargetSculptingObjectChange OnTargetSculptingObjectChange;

	// ==================== Configuration ====================

	/** The voxel sculpt volume to modify */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sculpting|Config")
	TObjectPtr<AVoxelSculptVolume> SculptVolume;

	/** Mana/resource cost per sculpt operation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sculpting|Config")
	float SculptManaCost = 10.0f;

	/** Grid size for snapping (should match voxel grid) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sculpting|Config", meta = (ClampMin = "1.0"))
	float GridSize = 50.0f;

	/** Default sculpt size when no object is selected */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sculpting|Config")
	FVector DefaultSculptSize = FVector(50.0f, 50.0f, 50.0f);

	/** Maximum trace distance for hit detection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sculpting|Config")
	float TraceDistance = 10000.0f;

	/** Class for the building UI widget */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sculpting|UI")
	TSubclassOf<UUserWidget> BuildingWidgetClass;

	// ==================== State (Replicated) ====================

	/** Current building/sculpting mode */
	UPROPERTY(ReplicatedUsing = OnRep_BuildingMode, BlueprintReadOnly, Category = "Sculpting|State")
	EVP_SculptStyle BuildingMode = EVP_SculptStyle::None;

	/** Current sculpting object settings */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Sculpting|State")
	FPGLSculptingObjectSettings SculptObject;

	// ==================== State (Local) ====================

	/** Cached player controller reference */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpting|State")
	TObjectPtr<APlayerController> PlayerController;

	/** Is this component owned by the local player? */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpting|State")
	bool bIsLocalPlayer = false;

	/** Current Z rotation input for building placement */
	UPROPERTY(BlueprintReadWrite, Category = "Sculpting|Input")
	float InputBuildingRotationZ = 0.0f;

	/** Current Z offset input for building placement */
	UPROPERTY(BlueprintReadWrite, Category = "Sculpting|Input")
	float InputBuildingOffsetZ = 0.0f;

	/** Forward/backward offset input */
	UPROPERTY(BlueprintReadWrite, Category = "Sculpting|Input")
	float InputBuildingOffsetFV = 0.0f;

	/** Current build transform */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpting|State")
	FTransform BuildTransform;

	/** Current target actor under cursor */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpting|State")
	TObjectPtr<AActor> TargetActor;

	/** Target building object reference */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpting|State")
	TObjectPtr<UObject> TargetBuildingObject;

	/** Whether current placement is valid */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpting|State")
	bool bCanBeBuilt = false;

	/** Current floor display level */
	UPROPERTY(BlueprintReadWrite, Category = "Sculpting|State")
	int32 DisplayedFloor = 0;

	/** Last selected building object handle */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpting|State")
	FDataTableRowHandle LastBuildingObjectHandle;

	/** Current building widget instance */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpting|UI")
	TObjectPtr<UUserWidget> BuildingWidget;

	/** Debug text for development */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpting|Debug")
	FText DebugInformation;

	// ==================== Sculpt Calculation State ====================

	/** Current sculpt target location */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpting|Calculation")
	FVector SculptLocation = FVector::ZeroVector;

	/** Forward vector from camera */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpting|Calculation")
	FVector ForwardVector = FVector::ForwardVector;

	/** Current sculpt offset */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpting|Calculation")
	FVector SculptOffset = FVector::ZeroVector;

	/** Current sculpt size */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpting|Calculation")
	FVector SculptSize = FVector(50.0f, 50.0f, 50.0f);

	/** Resource cost for current build */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpting|Calculation")
	int32 BuildCost = 0;

	/** Lock X axis during sculpt */
	UPROPERTY(BlueprintReadWrite, Category = "Sculpting|Calculation")
	bool bLockX = false;

	/** Sculpt location with XY locked */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpting|Calculation")
	FVector SculptLocation_LockedXY = FVector::ZeroVector;

	/** Build zone transform */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpting|Calculation")
	FTransform BuildZoneTransform;

	/** Final computed build location */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpting|Calculation")
	FVector FinalBuildLocation = FVector::ZeroVector;

	/** Build transform rotation component */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpting|Calculation")
	FRotator BuildTransformRotation = FRotator::ZeroRotator;

	/** Swap X/Y axes for rotation */
	UPROPERTY(BlueprintReadWrite, Category = "Sculpting|Calculation")
	bool bSwapXY = false;

	/** Final computed build rotation */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpting|Calculation")
	FRotator FinalBuildRotation = FRotator::ZeroRotator;

	/** Current sculpt parameters (uses existing struct from plugin) */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpting|Calculation")
	FPGLSculptParams SculptPreset;

	/** Current hit result from trace */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpting|Calculation")
	FHitResult HitResult;

	/** Initial hit result (for drag operations) */
	UPROPERTY(BlueprintReadOnly, Category = "Sculpting|Calculation")
	FHitResult InitialHitResult;

	// ==================== Public Functions ====================

	/**
	 * Set the sculpting mode
	 * @param NewMode The new sculpting style to use
	 */
	UFUNCTION(BlueprintCallable, Category = "Sculpting")
	void SetSculptingMode(EVP_SculptStyle NewMode);

	/**
	 * Set the current sculpting object settings
	 * @param NewSettings The settings for the object to sculpt
	 */
	UFUNCTION(BlueprintCallable, Category = "Sculpting")
	void SetSculptingObject(const FPGLSculptingObjectSettings& NewSettings);

	/**
	 * Attempt to perform a sculpt operation
	 * Checks requirements and executes if valid
	 */
	UFUNCTION(BlueprintCallable, Category = "Sculpting")
	void TrySculpt();

	/**
	 * Cancel the current build/sculpt operation
	 */
	UFUNCTION(BlueprintCallable, Category = "Sculpting")
	void CancelBuild();

	/**
	 * Get trace hit result for placement - traces forward from player camera
	 * @param OutHitResult The resulting hit
	 * @return True if hit was successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Sculpting")
	bool GetTraceHitResult(FHitResult& OutHitResult);

	/**
	 * Update the target actor based on hit result
	 * @param InHitResult The hit result to process
	 * @param bSucceed Whether the trace was successful
	 * @param OutTargetActor The actor that was hit (if any)
	 * @return True if target was updated successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Sculpting")
	bool UpdateTargetActor(const FHitResult& InHitResult, bool bSucceed, AActor*& OutTargetActor);

	/**
	 * Show the building menu UI
	 */
	UFUNCTION(BlueprintCallable, Category = "Sculpting|UI")
	void ShowBuildingMenu();

	/**
	 * Hide the building menu UI
	 */
	UFUNCTION(BlueprintCallable, Category = "Sculpting|UI")
	void HideBuildingMenu();

	/**
	 * Print debug information to screen
	 */
	UFUNCTION(BlueprintCallable, Category = "Sculpting|Debug")
	void PrintDebugInformation();

	/**
	 * Check if building requirements are met
	 * @return True if the player has required resources
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Sculpting")
	bool CheckBuildingRequirements() const;

protected:
	// ==================== Server RPCs ====================

	/** Server: Attempt to start building an object */
	UFUNCTION(Server, Reliable, WithValidation, Category = "Sculpting|Network")
	void Server_TryStartBuildObject(FDataTableRowHandle BuildingObjectHandle, FTransform InBuildTransform, AActor* InTargetActor);

	/** Server: Attempt to cancel the current build */
	UFUNCTION(Server, Reliable, WithValidation, Category = "Sculpting|Network")
	void Server_TryCancelBuild();

	/** Server: Finish the build operation - performs the actual sculpt */
	UFUNCTION(Server, Reliable, WithValidation, Category = "Sculpting|Network")
	void Server_FinishBuild(FDataTableRowHandle BuildingObjectHandle, FTransform InBuildTransform, AActor* InTargetActor);

	// ==================== Client RPCs ====================

	/** Client: Receive building message/feedback */
	UFUNCTION(Client, Reliable, Category = "Sculpting|Network")
	void Client_SendBuildingMessage(EPGLBuildingMode InBuildingMode, EPGLSculptingInteractionResult InteractionResult, const FText& Message);

	/** Client: Change the building widget class */
	UFUNCTION(Client, Reliable, Category = "Sculpting|Network")
	void Client_ChangeBuildingWidget(TSubclassOf<UUserWidget> NewWidgetClass);

	// ==================== Replication Callbacks ====================

	UFUNCTION()
	void OnRep_BuildingMode();

	// ==================== Internal Functions ====================

	/** Main update loop for sculpting process - called from Tick when in sculpt mode */
	void UpdateProcess();

	/** Draw the sculpt preview box */
	void DrawSculptPreviewBox(const FLinearColor& Color);

	/** Complete building requirements (consume resources, etc.) */
	void CompleteBuildingRequirements();

	/** Get actors to ignore during trace (owner, pawn, etc.) */
	TArray<AActor*> GetFloorIgnoringActors() const;

	/** Get the current sculpt size (from object settings or default) */
	FVector GetCurrentSculptSize() const;

	/** Check if remove mode is active (shift key held) */
	bool IsRemoveModeActive() const;

private:
	/** Track if left mouse is currently down (for drag operations) */
	bool bIsLeftMouseDown = false;

	/** Previous target actor for change detection */
	TWeakObjectPtr<AActor> PreviousTargetActor;
};
