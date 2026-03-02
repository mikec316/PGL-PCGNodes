// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Sculpt/VoxelSculptMode.h"
#include "PGLSculptingTypes.generated.h"

// Forward declarations
class UTexture2D;
class UStaticMesh;
class UVoxelVolumeSculptGraph;
class UVoxelSurfaceTypeInterface;

/**
 * Sculpting style modes - determines how the sculpting interaction works
 */
UENUM(BlueprintType)
enum class EVP_SculptStyle : uint8
{
	None            UMETA(DisplayName = "None"),
	FreeSculpt      UMETA(DisplayName = "Free Sculpt"),
	SculptSetPiece  UMETA(DisplayName = "Sculpt Set Piece")
};

/**
 * Building interaction modes - the type of building action being performed
 */
UENUM(BlueprintType)
enum class EPGLBuildingMode : uint8
{
	None     UMETA(DisplayName = "None"),
	Build    UMETA(DisplayName = "Build"),
	Repair   UMETA(DisplayName = "Repair"),
	Upgrade  UMETA(DisplayName = "Upgrade"),
	Destruct UMETA(DisplayName = "Destruct"),
	Remove   UMETA(DisplayName = "Remove"),
	Rotate   UMETA(DisplayName = "Rotate")
};

/**
 * Result of a sculpting interaction
 */
UENUM(BlueprintType)
enum class EPGLSculptingInteractionResult : uint8
{
	Success              UMETA(DisplayName = "Success"),
	Failed               UMETA(DisplayName = "Failed"),
	InsufficientResources UMETA(DisplayName = "Insufficient Resources"),
	InvalidTarget        UMETA(DisplayName = "Invalid Target"),
	PermissionDenied     UMETA(DisplayName = "Permission Denied")
};

/**
 * Item data structure for inventory/resource tracking
 */
USTRUCT(BlueprintType)
struct PGLPCGNODES_API FPGLItem
{
	GENERATED_BODY()

	/** Reference to item definition in data table */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FDataTableRowHandle ItemHandle;

	/** Stack amount */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	int32 Amount = 0;

	/** Current charges (for consumable items) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	int32 Charges = 0;

	/** Current durability (0.0 - MaxDurability) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	float Durability = 1.0f;

	/** Decay rate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	float Decay = 0.0f;

	/** Check if this item reference is valid */
	bool IsValid() const
	{
		return !ItemHandle.RowName.IsNone() && ItemHandle.DataTable != nullptr;
	}

	/** Default constructor */
	FPGLItem() = default;
};

/**
 * Settings for a sculpting object/prefab - defines what gets sculpted and how
 */
USTRUCT(BlueprintType)
struct PGLPCGNODES_API FPGLSculptingObjectSettings
{
	GENERATED_BODY()

	/** Reference to settings definition in data table */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sculpting")
	FDataTableRowHandle Handle;

	/** Display name for UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sculpting")
	FText Name;

	/** Icon for UI display */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sculpting")
	TObjectPtr<UTexture2D> Icon;

	/** Description for UI tooltips */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sculpting")
	FText Description;

	/** Maximum durability of the sculpted object */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sculpting")
	float MaxDurability = 100.0f;

	/** Whether this object can receive damage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sculpting")
	bool bCanBeDamaged = true;

	/** Optional voxel graph for complex sculpting operations */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sculpting|Voxel")
	TObjectPtr<UVoxelVolumeSculptGraph> VoxelSculptGraph;

	/** Surface type to apply when sculpting */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sculpting|Voxel")
	TObjectPtr<UVoxelSurfaceTypeInterface> VoxelSurfaceType;

	/** Sculpting style to use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sculpting")
	EVP_SculptStyle SculptStyle = EVP_SculptStyle::FreeSculpt;

	/** Sculpt mode (Add/Remove) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sculpting")
	EVoxelSculptMode SculptMode = EVoxelSculptMode::Add;

	/** Required item to perform this sculpt */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sculpting|Requirements")
	FPGLItem RequiredItem;

	/** Size of the sculpt operation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sculpting")
	FVector SculptSize = FVector(50.0f, 50.0f, 50.0f);

	/** Offset from hit location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sculpting")
	FVector SculptOffset = FVector::ZeroVector;

	/** Check if this settings reference is valid */
	bool IsValid() const
	{
		return !Handle.RowName.IsNone();
	}

	/** Default constructor */
	FPGLSculptingObjectSettings() = default;
};

/**
 * Message data for sculpting feedback - sent from server to client
 */
USTRUCT(BlueprintType)
struct PGLPCGNODES_API FPGLSculptingMessage
{
	GENERATED_BODY()

	/** The building mode this message relates to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
	EPGLBuildingMode BuildingMode = EPGLBuildingMode::None;

	/** Result of the interaction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
	EPGLSculptingInteractionResult InteractionResult = EPGLSculptingInteractionResult::Success;

	/** Human-readable message for UI display */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
	FText Message;

	/** Default constructor */
	FPGLSculptingMessage() = default;

	/** Convenience constructor */
	FPGLSculptingMessage(EPGLBuildingMode InMode, EPGLSculptingInteractionResult InResult, const FText& InMessage)
		: BuildingMode(InMode)
		, InteractionResult(InResult)
		, Message(InMessage)
	{
	}
};
