// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGElement.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "PCGSaveTextureToResourceSettings.generated.h"

class UPCGTextureData;
class UPCGPointData;

/** Where to save the pixels */
UENUM(BlueprintType)
enum class EPCGTextureSaveTarget : uint8
{
	TransientTexture = 0,
	AssetTexture = 1
};

UENUM(BlueprintType)
enum class EPCGTextureWriteMode : uint8
{
	CreateNew = 0,
	OverwriteExisting = 1
};

/** Output pixel encoding for the saved texture */
UENUM(BlueprintType)
enum class EPCGTextureOutputFormat : uint8
{
	BGRA8    UMETA(DisplayName = "BGRA8 (8-bit)"),
	RGBA16F  UMETA(DisplayName = "RGBA16f (Half Float)")
};

/** Save PCGTextureData or PCGPointData into a UTexture2D. */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class UPCGSaveTextureToResourceSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("Save Texture To Resource (PCG)"); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGSaveTextureToResource", "NodeTitle", "Save Texture To Resource (PCG)"); }
	virtual FText GetNodeTooltipText() const override
	{
		return NSLOCTEXT("PCGSaveTextureToResource", "NodeTooltip",
			"Saves data to a Texture2D. Accepts PCGTextureData (Texture/RT fast path or bake) "
			"or PCGPointData (rasterizes one point per texel using bounds mapping). "
			"Supports BGRA8 and RGBA16f outputs; assets persist by writing to Texture->Source.");
	}
#endif

	// Pins
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return {}; }

public:
	UPROPERTY(EditAnywhere, Category = "Output")
	EPCGTextureSaveTarget SaveTarget = EPCGTextureSaveTarget::TransientTexture;

	UPROPERTY(EditAnywhere, Category = "Output")
	EPCGTextureWriteMode WriteMode = EPCGTextureWriteMode::CreateNew;

	UPROPERTY(EditAnywhere, Category = "Output", meta = (EditCondition = "SaveTarget==EPCGTextureSaveTarget::TransientTexture && WriteMode==EPCGTextureWriteMode::OverwriteExisting", EditConditionHides))
	TObjectPtr<UTexture2D> TransientTextureToOverwrite = nullptr;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Output", meta = (EditCondition = "SaveTarget==EPCGTextureSaveTarget::AssetTexture && WriteMode==EPCGTextureWriteMode::OverwriteExisting", EditConditionHides))
	TObjectPtr<UTexture2D> AssetTextureToOverwrite = nullptr;

	UPROPERTY(EditAnywhere, Category = "Asset (Editor)", meta = (EditCondition = "SaveTarget==EPCGTextureSaveTarget::AssetTexture && WriteMode==EPCGTextureWriteMode::CreateNew", EditConditionHides))
	FString AssetPackagePath = TEXT("/Game/PCG/Exports");

	UPROPERTY(EditAnywhere, Category = "Asset (Editor)", meta = (EditCondition = "SaveTarget==EPCGTextureSaveTarget::AssetTexture && WriteMode==EPCGTextureWriteMode::CreateNew", EditConditionHides))
	FString AssetName = TEXT("PCG_SavedTexture");

	/** Save the asset package to disk after writing pixels. */
	UPROPERTY(EditAnywhere, Category = "Asset (Editor)")
	bool bAutoSavePackage = true;
#endif

	/** Output pixel encoding */
	UPROPERTY(EditAnywhere, Category = "Texture")
	EPCGTextureOutputFormat OutputFormat = EPCGTextureOutputFormat::RGBA16F;

	/** sRGB flag on the texture (note: HDR data is usually linear/sRGB=false) */
	UPROPERTY(EditAnywhere, Category = "Texture")
	bool bSRGB = false;

	/** Generate Mips (for exact data textures you usually want this OFF) */
	UPROPERTY(EditAnywhere, Category = "Texture")
	bool bGenerateMips = false;

	/** Resolution when baking/rasterizing points. */
	UPROPERTY(EditAnywhere, Category = "Resolution", meta = (ClampMin = "1"))
	int32 BakeWidth = 1024;

	UPROPERTY(EditAnywhere, Category = "Resolution", meta = (ClampMin = "1"))
	int32 BakeHeight = 1024;

	/** Allow CPU UV sampling when PCGTextureData has no underlying texture. */
	UPROPERTY(EditAnywhere, Category = "Texture Bake Fallback")
	bool bBakeIfNoUnderlyingTexture = true;

public:
	virtual FPCGElementPtr CreateElement() const override;

	// Helpers (now produce linear pixels; we choose encoding at write time)
	static bool ExtractLinearFromPCGTextureData(const UPCGTextureData* InTexData, int32& OutW, int32& OutH, TArray<FLinearColor>& OutPixels, FString& OutWhyFailed);
	static bool BakeLinearFromPCGTextureData(const UPCGTextureData* InTexData, int32 Width, int32 Height, TArray<FLinearColor>& OutPixels, FString& OutWhyFailed);
	static bool RasterizePointsToLinear(const UPCGPointData* PointData, int32 Width, int32 Height, TArray<FLinearColor>& OutPixels, FString& OutWhyFailed);

	/** Writes linear pixels to texture in the requested encoding (BGRA8 or RGBA16f). */
	static bool SaveLinearToTexture2D(UTexture2D*& InOutTexture, int32 Width, int32 Height, const TArray<FLinearColor>& LinearPixels, EPCGTextureOutputFormat Encoding, bool bSRGB, bool bBuildMips, bool bIsAsset, FString& OutWhyFailed);

#if WITH_EDITOR
	static bool CreateAssetTexture2D(UTexture2D*& OutTexture, const FString& PackagePath, const FString& Name, FString& OutWhyFailed);
#endif
};

class FPCGSaveTextureToResourceElement : public IPCGElement
{
public:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
