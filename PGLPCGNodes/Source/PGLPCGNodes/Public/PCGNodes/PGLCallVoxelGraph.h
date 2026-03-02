// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelMinimal.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "VoxelNodeEvaluator.h"
#include "StructUtils/PropertyBag.h"
#include "VoxelParameterOverridesOwner.h"
#include "Async/PCGAsyncLoadingContext.h"
#include "Buffer/VoxelSoftObjectPathBuffer.h"
#include "PCGCallVoxelGraph.h" // For FVoxelPCGObjectAttributeType
#include "Data/PCGPointData.h"
#include "PCGNodes/PGLVoxelAttributeMarshalling.h" // For PGLVoxelMarshalling::FMetadataWriter
#include "PGLCallVoxelGraph.generated.h"

class UPCGPointData;
class UVoxelPCGGraph;
class FVoxelLayers;
class FVoxelSurfaceTypeTable;
struct FVoxelOutputNode_OutputPoints;
enum class EPCGMetadataTypes : uint8;

// ─────────────────────────────────────────────────────────────────────────────
// Settings
// ─────────────────────────────────────────────────────────────────────────────

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PGLPCGNODES_API UPGLCallVoxelGraphSettings
	: public UPCGSettings
	, public IVoxelParameterOverridesObjectOwner
{
	GENERATED_BODY()

public:
	UPGLCallVoxelGraphSettings();

	//~ Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return "PGLCallVoxelGraph"; }
	virtual FText GetDefaultNodeTitle() const override { return INVTEXT("PGL Call Voxel Graph"); }
	virtual FText GetNodeTooltipText() const override
	{
		return INVTEXT("Optimized: Calls a voxel graph with selective marshalling, pass-through detection, in-place output, and parallel conversion.");
	}
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Blueprint; }
	virtual EPCGChangeType GetChangeTypeForProperty(const FName& InPropertyName) const override
	{
		return Super::GetChangeTypeForProperty(InPropertyName) | EPCGChangeType::Cosmetic;
	}
	virtual UObject* GetJumpTargetForDoubleClick() const override;
#endif

	virtual bool CanCullTaskIfUnwired() const override { return false; }
	virtual bool HasDynamicPins() const override { return true; }
	virtual bool HasFlippedTitleLines() const override { return true; }
	virtual FString GetAdditionalTitleInformation() const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
#if WITH_EDITOR
	virtual TArray<FPCGSettingsOverridableParam> GatherOverridableParams() const override;
#endif
	virtual void FixingOverridableParamPropertyClass(FPCGSettingsOverridableParam& Param) const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~ End UPCGSettings interface

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
	virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface

	//~ Begin IVoxelParameterOverridesOwner Interface
	virtual UVoxelGraph* GetGraph() const override;
	virtual bool ShouldForceEnableOverride(const FGuid& Guid) const override;
	virtual FVoxelParameterOverrides& GetParameterOverrides() override;
	virtual void FixupParameterOverrides() override;
	//~ End IVoxelParameterOverridesOwner Interface

#if WITH_EDITOR
	void FixupOnChangedDelegate();
#endif

	// ── Graph reference ──────────────────────────────────────────────────────

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
	TObjectPtr<UVoxelPCGGraph> Graph;

	/** Assign types to object attributes so they can be resolved inside the voxel graph. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", DisplayName = "Object Attributes Mapping")
	TArray<FVoxelPCGObjectAttributeType> ObjectAttributeToType;

	/** Force synchronous object loading instead of async. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Debug")
	bool bSynchronousLoad = false;

	// ── Optimization settings ────────────────────────────────────────────────

	/**
	 * When enabled, only converts PCG attributes listed in CustomAttributeWhitelist
	 * (or all if whitelist is empty). Standard point attributes are always converted.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Optimization")
	bool bSelectiveMarshalling = true;

	/**
	 * Explicit list of custom attribute names to marshal into the voxel graph.
	 * Leave empty to marshal all custom attributes (when bSelectiveMarshalling
	 * is false) or to attempt automatic detection.
	 * Standard attributes (Position, Rotation, Scale, etc.) are always included.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Optimization",
		meta = (EditCondition = "bSelectiveMarshalling"))
	TArray<FName> CustomAttributeWhitelist;

	/**
	 * When enabled, compares input/output buffer pointers after voxel graph execution.
	 * Attributes that pass through unchanged skip reverse conversion entirely.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Optimization")
	bool bPassThroughDetection = true;

	/**
	 * Point count threshold for chunked parallel processing.
	 * When > 0 and point count exceeds this, points are split into chunks and
	 * processed in parallel. Set to 0 to disable.
	 * WARNING: Only use when your voxel graph processes points independently
	 * (no inter-point dependencies like neighbor lookups).
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Optimization",
		meta = (ClampMin = 0, UIMin = 0, UIMax = 500000))
	int32 ChunkThreshold = 0;

	// ── Parameter overrides (internal) ───────────────────────────────────────

	UPROPERTY()
	FVoxelParameterOverrides ParameterOverrides;

	UPROPERTY()
	FInstancedPropertyBag ParameterPinOverrides;

#if WITH_EDITOR
	FSharedVoidPtr OnTerminalGraphChangedPtr;
#endif
};

// ─────────────────────────────────────────────────────────────────────────────
// Context
// ─────────────────────────────────────────────────────────────────────────────

struct FPGLCallVoxelGraphContext : public FPCGContext, public IPCGAsyncLoadingContext
{
	// Voxel graph evaluation
	TSharedPtr<FVoxelDependencyCollector> DependencyCollector;
	TVoxelNodeEvaluator<FVoxelOutputNode_OutputPoints> Evaluator;
	TSharedPtr<FVoxelLayers> Layers;
	TSharedPtr<FVoxelSurfaceTypeTable> SurfaceTypeTable;
	FVoxelBox Bounds;

	// Input/Output data — uses UPCGPointData (FPCGPoint arrays, universally compatible)
	TArray<const UPCGPointData*> InPointData;
	TArray<TVoxelMap<FName, TVoxelObjectPtr<UPCGPointData>>> OutPointData;
	TVoxelSet<FName> PinsToQuery;

	// Async execution
	TArray<FVoxelFuture> FuturePoints;
	FSharedVoidRef SharedVoid = MakeSharedVoid();

	// Parameter pin overrides
	FInstancedPropertyBag ParameterPinOverrides;
	void InitializeUserParametersStruct();

	// ── Optimization state ───────────────────────────────────────────────────

	/** Pre-computed set of custom attribute names to marshal (empty = marshal all). */
	TVoxelSet<FName> AttributeWhitelist;
	bool bUseWhitelist = false;

	//~ Begin FPCGContext Interface
	virtual void* GetUnsafeExternalContainerForOverridableParam(const FPCGSettingsOverridableParam& InParam) override;
	virtual void AddExtraStructReferencedObjects(FReferenceCollector& Collector) override;
	//~ End FPCGContext Interface
};

// ─────────────────────────────────────────────────────────────────────────────
// Element
// ─────────────────────────────────────────────────────────────────────────────

class PGLPCGNODES_API FPGLCallVoxelGraphElement : public IPCGElement
{
public:
	//~ Begin IPCGElement Interface
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override;

#if VOXEL_ENGINE_VERSION >= 506
	virtual FPCGContext* Initialize(const FPCGInitializeElementParams& InParams) override;
#else
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
#endif

	virtual bool PrepareDataInternal(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
	//~ End IPCGElement Interface

private:
	/** Compute function that runs on worker threads.
	 *  Takes an owned copy of the FPCGPoint array (modified in-place for output).
	 *  Output is moved to UPCGPointData in a lightweight GameTask. */
	static FVoxelFuture Compute(
		const TVoxelNodeEvaluator<FVoxelOutputNode_OutputPoints>& Evaluator,
		const TSharedRef<FVoxelDependencyCollector>& DependencyCollector,
		const FVoxelBox& Bounds,
		const TVoxelSet<FName>& PinsToQuery,
		const TSharedRef<FVoxelLayers>& Layers,
		const TSharedRef<FVoxelSurfaceTypeTable>& SurfaceTypeTable,
		TVoxelArray<FPCGPoint> Points,
		const TVoxelMap<FName, EPCGMetadataTypes>& AttributeNameToType,
		const TVoxelMap<FName, TSharedPtr<FVoxelBuffer>>& AttributeNameToBuffer,
		const TVoxelMap<FName, TVoxelObjectPtr<UPCGPointData>>& PinToOutPointData,
		bool bPassThroughDetection,
		const TVoxelMap<FName, TSharedPtr<const FVoxelBuffer>>& InputBufferSnapshot);
};
