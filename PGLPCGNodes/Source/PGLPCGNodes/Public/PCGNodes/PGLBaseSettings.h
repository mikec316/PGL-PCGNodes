// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "DataTypes/PVData.h"
#include "PGLBaseSettings.generated.h"

/**
 * Abstract base settings for PGL nodes that process PVE growth data.
 *
 * Replicates the public interface of PVE's UPVBaseSettings (which is not
 * exported and therefore cannot be safely inherited in a modular build).
 *
 * Provides:
 *  - Standard growth-data input/output pin definitions
 *  - Render-type tracking for future PGL-specific data visualization
 *  - Common GetType() = Spatial, HasExecutionDependencyPin() = false
 *
 * Subclasses override GetInputPinTypeIdentifier / GetOutputPinTypeIdentifier
 * to customize pin data types, or override InputPinProperties / OutputPinProperties
 * entirely for multi-pin or non-standard layouts.
 */
UCLASS(BlueprintType, Abstract, HideCategories=(Debug), ClassGroup = (Procedural))
class PGLPCGNODES_API UPGLBaseSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	bool bLockInspection = false;

	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }

	// ---------------------------------------------------------------------------
	//  Render-type interface — mirrors UPVBaseSettings for future PGL visualization
	// ---------------------------------------------------------------------------
	virtual TArray<EPVRenderType> GetDefaultRenderType() const { return TArray<EPVRenderType>(); }
	virtual TArray<EPVRenderType> GetSupportedRenderTypes() const { return GetDefaultRenderType(); }
	virtual TArray<EPVRenderType> GetCurrentRenderTypes() const { return CurrentRenderType; }
	virtual void SetCurrentRenderType(TArray<EPVRenderType> InRenderTypes);
#endif

protected:
	// ---------------------------------------------------------------------------
	//  PCG pin / element interface
	// ---------------------------------------------------------------------------
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	virtual bool HasExecutionDependencyPin() const override { return false; }

	/** Override to customize the data type for the default input pin. */
	virtual FPCGDataTypeIdentifier GetInputPinTypeIdentifier() const;

	/** Override to customize the data type for the default output pin. */
	virtual FPCGDataTypeIdentifier GetOutputPinTypeIdentifier() const;

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<EPVRenderType> CurrentRenderType;
#endif
};

// ---------------------------------------------------------------------------
//  Base element — no-op; subclass elements override ExecuteInternal.
// ---------------------------------------------------------------------------
class FPGLBaseElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
