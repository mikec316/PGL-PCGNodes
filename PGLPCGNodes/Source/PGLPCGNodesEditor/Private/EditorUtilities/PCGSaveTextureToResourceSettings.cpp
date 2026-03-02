// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGSaveTextureToResourceSettings.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGTextureData.h"
#include "Data/PCGPointData.h"

#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureRenderTarget.h"
#include "Rendering/Texture2DResource.h"

#include "UObject/Package.h"
#include "Logging/LogMacros.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogPCGSaveTexture, Log, All);

TArray<FPCGPinProperties> UPCGSaveTextureToResourceSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Texture | EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGSaveTextureToResourceSettings::CreateElement() const
{
	return MakeShared<FPCGSaveTextureToResourceElement>();
}

/* static */ bool UPCGSaveTextureToResourceSettings::ExtractLinearFromPCGTextureData(
	const UPCGTextureData* InTexData, int32& OutW, int32& OutH, TArray<FLinearColor>& OutPixels, FString& OutWhyFailed)
{
	OutW = OutH = 0;
	OutPixels.Reset();

	if (!InTexData)
	{
		OutWhyFailed = TEXT("TextureData is null.");
		return false;
	}

	// Underlying texture may be null for procedural PCGTextureData
	UTexture* Underlying = const_cast<UTexture*>(InTexData->GetTexture());
	if (!Underlying)
	{
		OutWhyFailed = TEXT("PCGTextureData has no underlying UTexture.");
		return false;
	}

	if (UTexture2D* Tex2D = Cast<UTexture2D>(Underlying))
	{
		if (!Tex2D->GetPlatformData() || Tex2D->GetPlatformData()->Mips.Num() == 0)
		{
			OutWhyFailed = TEXT("Texture2D has no platform data/mips.");
			return false;
		}

		const FTexture2DMipMap& Mip0 = Tex2D->GetPlatformData()->Mips[0];
		OutW = Mip0.SizeX;
		OutH = Mip0.SizeY;

		const EPixelFormat SrcPF = Tex2D->GetPlatformData()->PixelFormat;
		if (SrcPF != PF_B8G8R8A8 && SrcPF != PF_R8G8B8A8)
		{
			OutWhyFailed = FString::Printf(TEXT("Unsupported source pixel format: %d. Expected 8-bit RGBA/BGRA."), (int32)SrcPF);
			return false;
		}

		const void* Data = Mip0.BulkData.LockReadOnly();
		if (!Data)
		{
			OutWhyFailed = TEXT("Failed to lock Texture2D bulk data.");
			return false;
		}

		const int64 NumPixels = (int64)OutW * (int64)OutH;
		OutPixels.SetNumUninitialized((int32)NumPixels);

		if (SrcPF == PF_B8G8R8A8)
		{
			const FColor* Src = static_cast<const FColor*>(Data);
			for (int64 i = 0; i < NumPixels; ++i)
			{
				OutPixels[(int32)i] = Tex2D->SRGB ? FLinearColor::FromSRGBColor(Src[i]) : FLinearColor(Src[i]);
			}
		}
		else // PF_R8G8B8A8 → swap to match FColor(BGRA) before conversion
		{
			const uint8* Src = static_cast<const uint8*>(Data);
			for (int64 i = 0; i < NumPixels; ++i)
			{
				const int64 idx = i * 4;
				const FColor AsBGRA(Src[idx + 2], Src[idx + 1], Src[idx + 0], Src[idx + 3]);
				OutPixels[(int32)i] = Tex2D->SRGB ? FLinearColor::FromSRGBColor(AsBGRA) : FLinearColor(AsBGRA);
			}
		}
		Mip0.BulkData.Unlock();

		return true;
	}
	else if (UTextureRenderTarget2D* RT = Cast<UTextureRenderTarget2D>(Underlying))
	{
		OutW = RT->SizeX;
		OutH = RT->SizeY;

		FTextureRenderTargetResource* RTRes = RT->GameThread_GetRenderTargetResource();
		if (!RTRes)
		{
			OutWhyFailed = TEXT("RenderTarget2D has no render resource.");
			return false;
		}

		// 1) Preferred: half-float readback
		{
			TArray<FFloat16Color> Half;
			if (RTRes->ReadFloat16Pixels(Half) && Half.Num() == OutW * OutH)
			{
				OutPixels.SetNumUninitialized(Half.Num());
				for (int32 i = 0; i < Half.Num(); ++i)
				{
					OutPixels[i] = FLinearColor(Half[i]); // convert half->linear
				}
				return true;
			}
		}

		// 2) Fallback: 8-bit readback, then convert to linear
		{
			TArray<FColor> Colors;
			FReadSurfaceDataFlags Flags(RCM_UNorm);
			Flags.SetLinearToGamma(false); // keep raw; FLinearColor(FColor) will handle SRGB->linear
			if (RTRes->ReadPixels(Colors, Flags) && Colors.Num() == OutW * OutH)
			{
				OutPixels.SetNumUninitialized(Colors.Num());
				for (int32 i = 0; i < Colors.Num(); ++i)
				{
					OutPixels[i] = FLinearColor(Colors[i]); // 8-bit -> linear
				}
				return true;
			}
		}

		OutWhyFailed = TEXT("Failed to read RenderTarget pixels (ReadFloat16Pixels/ReadPixels failed).");
		return false;
	}

	else
	{
		OutWhyFailed = FString::Printf(TEXT("Unsupported underlying texture type: %s"), *Underlying->GetClass()->GetName());
		return false;
	}
}

/* static */ bool UPCGSaveTextureToResourceSettings::BakeLinearFromPCGTextureData(
	const UPCGTextureData* InTexData,
	int32 Width,
	int32 Height,
	TArray<FLinearColor>& OutPixels,
	FString& OutWhyFailed)
{
	if (!InTexData)
	{
		OutWhyFailed = TEXT("BakeLinear: TextureData is null.");
		return false;
	}
	if (Width <= 0 || Height <= 0)
	{
		OutWhyFailed = TEXT("BakeLinear: Invalid bake dimensions.");
		return false;
	}

	// Probe sample to avoid PCG spam if CPU sampling is disabled
	FVector4 ProbeColor(0, 0, 0, 0);
	float ProbeDensity = 0.f;
	if (!InTexData->SamplePointLocal(FVector2D(0.5f, 0.5f), ProbeColor, ProbeDensity))
	{
		OutWhyFailed = TEXT("CPU sampling disabled for this PCGTextureData (Skip Readback to CPU).");
		return false;
	}

	OutPixels.SetNumUninitialized(Width * Height);

	for (int32 y = 0; y < Height; ++y)
	{
		const float v = (y + 0.5f) / float(Height);
		for (int32 x = 0; x < Width; ++x)
		{
			const float u = (x + 0.5f) / float(Width);

			FVector4 OutColorV4(0, 0, 0, 0);
			float OutDensity = 0.f;
			const bool bSampled = InTexData->SamplePointLocal(FVector2D(u, v), OutColorV4, OutDensity);
			FLinearColor C((float)OutColorV4.X, (float)OutColorV4.Y, (float)OutColorV4.Z, (float)OutColorV4.W);
			if (!bSampled)
			{
				C = FLinearColor::Black;
			}

			OutPixels[y * Width + x] = C;
		}
	}

	return true;
}

/* static */ bool UPCGSaveTextureToResourceSettings::RasterizePointsToLinear(
	const UPCGPointData* PointData,
	int32 Width,
	int32 Height,
	TArray<FLinearColor>& OutPixels,
	FString& OutWhyFailed)
{
	OutPixels.Reset();

	if (!PointData)
	{
		OutWhyFailed = TEXT("PointData is null.");
		return false;
	}
	if (Width <= 0 || Height <= 0)
	{
		OutWhyFailed = TEXT("Invalid output resolution.");
		return false;
	}

	const TArray<FPCGPoint>& Points = PointData->GetPoints();
	const int32 N = Points.Num();
	if (N == 0)
	{
		OutWhyFailed = TEXT("PointData contains no points.");
		return false;
	}

	// Gather XY positions
	TArray<float> Xs; Xs.Reserve(N);
	TArray<float> Ys; Ys.Reserve(N);
	for (const FPCGPoint& P : Points)
	{
		const FVector L = P.Transform.GetLocation();
		Xs.Add(L.X); Ys.Add(L.Y);
	}

	// Build unique sorted coordinate lists with a tolerance
	auto BuildUniqueSorted = [](TArray<float>& Vals, TArray<float>& Unique)
		{
			Vals.Sort();
			Unique.Reset();
			if (Vals.Num() == 0) return;

			const float Range = FMath::Max(1e-6f, Vals.Last() - Vals[0]);
			const float Eps = Range * 1e-4f; // tolerance for quantizing coordinates

			float cur = Vals[0];
			Unique.Add(cur);
			for (int32 i = 1; i < Vals.Num(); ++i)
			{
				if (FMath::Abs(Vals[i] - cur) > Eps)
				{
					cur = Vals[i];
					Unique.Add(cur);
				}
			}
		};

	TArray<float> UniqueX, UniqueY;
	BuildUniqueSorted(Xs, UniqueX);
	BuildUniqueSorted(Ys, UniqueY);

	// Decide mapping mode
	const bool bLooksLikeGrid =
		(UniqueX.Num() >= 2 && UniqueY.Num() >= 2 &&
			UniqueX.Num() * UniqueY.Num() == N);

	// Prepare accumulators
	OutPixels.SetNumZeroed(Width * Height);
	TArray<int32> Counts; Counts.Init(0, Width * Height);

	auto PutPixel = [&](int32 ix, int32 iy, const FLinearColor& C)
		{
			ix = FMath::Clamp(ix, 0, Width - 1);
			iy = FMath::Clamp(iy, 0, Height - 1);
			const int32 idx = iy * Width + ix;
			OutPixels[idx] += C;
			Counts[idx] += 1;
		};

	if (bLooksLikeGrid)
	{
		// Map by index in the unique coordinate lists (exact texel addressing)
		// If the chosen Width/Height differ from the detected grid, we remap proportionally.
		const int32 GridW = UniqueX.Num();
		const int32 GridH = UniqueY.Num();

		// Fast binary-search index finder with tolerance
		const float RangeX = FMath::Max(1e-6f, UniqueX.Last() - UniqueX[0]);
		const float RangeY = FMath::Max(1e-6f, UniqueY.Last() - UniqueY[0]);
		const float EpsX = RangeX * 1e-4f;
		const float EpsY = RangeY * 1e-4f;

		auto FindIndex = [](const TArray<float>& U, float v, float eps)->int32
			{
				int32 lo = 0, hi = U.Num() - 1;
				while (lo <= hi)
				{
					const int32 mid = (lo + hi) >> 1;
					const float d = v - U[mid];
					if (FMath::Abs(d) <= eps) return mid;
					if (d > 0) lo = mid + 1; else hi = mid - 1;
				}
				// nearest
				lo = FMath::Clamp(lo, 0, U.Num() - 1);
				hi = FMath::Clamp(hi, 0, U.Num() - 1);
				return (FMath::Abs(v - U[lo]) < FMath::Abs(v - U[hi])) ? lo : hi;
			};

		for (const FPCGPoint& P : Points)
		{
			const FVector L = P.Transform.GetLocation();
			const int32 gx = FindIndex(UniqueX, L.X, EpsX);
			const int32 gy = FindIndex(UniqueY, L.Y, EpsY);

			// If output res differs from grid res, scale indices
			const int32 ix = (GridW == Width) ? gx : FMath::RoundToInt(float(gx) * float(Width - 1) / float(GridW - 1));
			const int32 iy = (GridH == Height) ? gy : FMath::RoundToInt(float(gy) * float(Height - 1) / float(GridH - 1));

			const auto Col4 = P.Color;
			const FLinearColor C((float)Col4.X, (float)Col4.Y, (float)Col4.Z, (float)Col4.W);

			PutPixel(ix, iy, C);
		}
	}
	else
	{
		// Fallback: round-to-nearest pixel using the detected overall bounds
		const float MinX = Xs.Num() ? Xs[0] : 0.f;
		const float MaxX = Xs.Num() ? Xs.Last() : 1.f;
		Ys.Sort();
		const float MinY = Ys.Num() ? Ys[0] : 0.f;
		const float MaxY = Ys.Num() ? Ys.Last() : 1.f;

		const float SizeX = FMath::Max(1e-6f, MaxX - MinX);
		const float SizeY = FMath::Max(1e-6f, MaxY - MinY);

		for (const FPCGPoint& P : Points)
		{
			const FVector L = P.Transform.GetLocation();

			const float u = (L.X - MinX) / SizeX;   // [0..1]
			const float v = (L.Y - MinY) / SizeY;   // [0..1]

			// Use ROUND to nearest pixel and scale by (W-1)/(H-1) so edges hit last texel
			const int32 ix = FMath::Clamp(FMath::RoundToInt(u * float(Width - 1)), 0, Width - 1);
			const int32 iy = FMath::Clamp(FMath::RoundToInt(v * float(Height - 1)), 0, Height - 1);

			const auto Col4 = P.Color;
			const FLinearColor C((float)Col4.X, (float)Col4.Y, (float)Col4.Z, (float)Col4.W);

			PutPixel(ix, iy, C);
		}
	}

	// Average overlaps
	for (int32 i = 0; i < OutPixels.Num(); ++i)
	{
		if (Counts[i] > 0) OutPixels[i] /= float(Counts[i]);
		else               OutPixels[i] = FLinearColor::Black;
	}

	return true;
}


#if WITH_EDITOR
/* static */ bool UPCGSaveTextureToResourceSettings::CreateAssetTexture2D(
	UTexture2D*& OutTexture,
	const FString& PackagePath,
	const FString& Name,
	FString& OutWhyFailed)
{
	if (!FPackageName::IsValidLongPackageName(PackagePath))
	{
		OutWhyFailed = FString::Printf(TEXT("Invalid package path: %s"), *PackagePath);
		return false;
	}

	const FString PackageName = PackagePath / Name;
	UPackage* Pkg = CreatePackage(*PackageName);
	if (!Pkg)
	{
		OutWhyFailed = TEXT("Failed to create package.");
		return false;
	}

	OutTexture = NewObject<UTexture2D>(Pkg, *Name, RF_Public | RF_Standalone);
	if (!OutTexture)
	{
		OutWhyFailed = TEXT("Failed to new UTexture2D in package.");
		return false;
	}

	OutTexture->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(OutTexture);
	return true;
}
#endif // WITH_EDITOR

bool UPCGSaveTextureToResourceSettings::SaveLinearToTexture2D(
	UTexture2D*& InOutTexture,
	int32 Width,
	int32 Height,
	const TArray<FLinearColor>& LinearPixels,
	EPCGTextureOutputFormat Encoding,
	bool bSRGB,
	bool bBuildMips,
	bool bIsAsset,
	FString& OutWhyFailed)
{
	if (Width <= 0 || Height <= 0)
	{
		OutWhyFailed = TEXT("Invalid target dimensions.");
		return false;
	}
	if (LinearPixels.Num() != Width * Height)
	{
		OutWhyFailed = TEXT("Pixel buffer size doesn't match dimensions.");
		return false;
	}

	// Ensure a texture object exists
	if (!InOutTexture)
	{
		// Create a shell; format will be set by Source/PlatformData below
		InOutTexture = UTexture2D::CreateTransient(Width, Height, PF_FloatRGBA);
		if (!InOutTexture)
		{
			OutWhyFailed = TEXT("Failed to create UTexture2D.");
			return false;
		}
	}

	// Common sampler settings
	InOutTexture->SRGB = bSRGB;
	InOutTexture->CompressionSettings = (Encoding == EPCGTextureOutputFormat::RGBA16F) ? TC_HDR : TC_Default;
#if WITH_EDITOR
	InOutTexture->MipGenSettings = bBuildMips ? TMGS_FromTextureGroup : TMGS_NoMipmaps;
#endif

#if WITH_EDITOR
	if (bIsAsset)
	{
		InOutTexture->Modify();

		if (Encoding == EPCGTextureOutputFormat::RGBA16F)
		{
			// Persistent: write Source as RGBA16f
			InOutTexture->Source.Init(Width, Height, /*NumSlices*/1, /*NumMips*/1, TSF_RGBA16F);

			TArray<FFloat16Color> Half;
			Half.SetNumUninitialized(Width * Height);
			for (int32 i = 0; i < Width * Height; ++i)
			{
				Half[i] = FFloat16Color(LinearPixels[i]);
			}

			uint8* Dest = InOutTexture->Source.LockMip(0);
			if (!Dest)
			{
				OutWhyFailed = TEXT("Failed to lock Texture->Source mip for write (RGBA16f).");
				return false;
			}
			FMemory::Memcpy(Dest, Half.GetData(), (int64)Width * (int64)Height * (int64)sizeof(FFloat16Color));
			InOutTexture->Source.UnlockMip(0);
		}
		else
		{
			// Persistent: write Source as BGRA8
			InOutTexture->Source.Init(Width, Height, /*NumSlices*/1, /*NumMips*/1, TSF_BGRA8);

			TArray<FColor> S8;
			S8.SetNumUninitialized(Width * Height);
			for (int32 i = 0; i < Width * Height; ++i)
			{
				S8[i] = bSRGB ? LinearPixels[i].ToFColorSRGB() : LinearPixels[i].ToFColor(false);
			}

			uint8* Dest = InOutTexture->Source.LockMip(0);
			if (!Dest)
			{
				OutWhyFailed = TEXT("Failed to lock Texture->Source mip for write (BGRA8).");
				return false;
			}
			FMemory::Memcpy(Dest, S8.GetData(), (int64)Width * (int64)Height * (int64)sizeof(FColor));
			InOutTexture->Source.UnlockMip(0);
		}

		// Rebuild runtime from Source and persist
		InOutTexture->SetPlatformData(nullptr);
		InOutTexture->PostEditChange();
		InOutTexture->MarkPackageDirty();
		return true;
	}
#endif // WITH_EDITOR

	// -------- Transient path: write PlatformData directly --------
	const EPixelFormat PF = (Encoding == EPCGTextureOutputFormat::RGBA16F) ? PF_FloatRGBA : PF_B8G8R8A8;

	if (!InOutTexture->GetPlatformData())
	{
		FTexturePlatformData* PlatInit = new FTexturePlatformData();
		PlatInit->SizeX = Width;
		PlatInit->SizeY = Height;
		PlatInit->PixelFormat = PF;
		InOutTexture->SetPlatformData(PlatInit);
	}
	FTexturePlatformData* Plat = InOutTexture->GetPlatformData();

	if (Plat->PixelFormat != PF)
	{
		Plat->PixelFormat = PF;
		Plat->Mips.Empty();
	}

	if (Plat->Mips.Num() == 0)
	{
		Plat->Mips.Add(new FTexture2DMipMap());
	}
	FTexture2DMipMap& Mip0 = Plat->Mips[0];
	Mip0.SizeX = Width;
	Mip0.SizeY = Height;

	if (PF == PF_FloatRGBA)
	{
		TArray<FFloat16Color> Half;
		Half.SetNumUninitialized(Width * Height);
		for (int32 i = 0; i < Width * Height; ++i)
		{
			Half[i] = FFloat16Color(LinearPixels[i]);
		}

		Mip0.BulkData.Lock(LOCK_READ_WRITE);
		void* Data = Mip0.BulkData.Realloc((int64)Width * (int64)Height * (int64)sizeof(FFloat16Color));
		if (!Data) { Mip0.BulkData.Unlock(); OutWhyFailed = TEXT("BulkData alloc failed (RGBA16f)."); return false; }
		FMemory::Memcpy(Data, Half.GetData(), (int64)Width * (int64)Height * (int64)sizeof(FFloat16Color));
		Mip0.BulkData.Unlock();
	}
	else
	{
		TArray<FColor> S8;
		S8.SetNumUninitialized(Width * Height);
		for (int32 i = 0; i < Width * Height; ++i)
		{
			S8[i] = bSRGB ? LinearPixels[i].ToFColorSRGB() : LinearPixels[i].ToFColor(false);
		}

		Mip0.BulkData.Lock(LOCK_READ_WRITE);
		void* Data = Mip0.BulkData.Realloc((int64)Width * (int64)Height * (int64)sizeof(FColor));
		if (!Data) { Mip0.BulkData.Unlock(); OutWhyFailed = TEXT("BulkData alloc failed (BGRA8)."); return false; }
		FMemory::Memcpy(Data, S8.GetData(), (int64)Width * (int64)Height * (int64)sizeof(FColor));
		Mip0.BulkData.Unlock();
	}

	InOutTexture->UpdateResource();
	return true;
}

// ---------------- Element ----------------

bool FPCGSaveTextureToResourceElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);
	const UPCGSaveTextureToResourceSettings* Settings = Context->GetInputSettings<UPCGSaveTextureToResourceSettings>();
	if (!Settings)
	{
		UE_LOG(LogPCGSaveTexture, Error, TEXT("Missing settings."));
		return true;
	}

	const TArray<FPCGTaggedData>& TaggedInputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	const UPCGPointData* PointData = nullptr;
	const UPCGTextureData* TextureData = nullptr;

	for (const FPCGTaggedData& Tagged : TaggedInputs)
	{
		PointData = Cast<UPCGPointData>(Tagged.Data);
		if (PointData) break;
	}
	if (!PointData)
	{
		for (const FPCGTaggedData& Tagged : TaggedInputs)
		{
			TextureData = Cast<UPCGTextureData>(Tagged.Data);
			if (TextureData) break;
		}
	}

	int32 W = 0, H = 0;
	TArray<FLinearColor> LinearPixels;

	if (PointData)
	{
		// Choose resolution (your grid or settings)
		W = Settings->BakeWidth;
		H = Settings->BakeHeight;

		FString WhyPoints;
		if (!UPCGSaveTextureToResourceSettings::RasterizePointsToLinear(PointData, W, H, LinearPixels, WhyPoints))
		{
			UE_LOG(LogPCGSaveTexture, Error, TEXT("Point rasterization failed: %s"), *WhyPoints);
			return true;
		}
	}
	else if (TextureData)
	{
		FString Why;
		if (!UPCGSaveTextureToResourceSettings::ExtractLinearFromPCGTextureData(TextureData, W, H, LinearPixels, Why))
		{
			if (Settings->bBakeIfNoUnderlyingTexture)
			{
				W = Settings->BakeWidth;
				H = Settings->BakeHeight;
				FString WhyBake;
				if (!UPCGSaveTextureToResourceSettings::BakeLinearFromPCGTextureData(TextureData, W, H, LinearPixels, WhyBake))
				{
					UE_LOG(LogPCGSaveTexture, Error, TEXT("Failed to bake from PCGTextureData: %s"), *WhyBake);
					return true;
				}
			}
			else
			{
				UE_LOG(LogPCGSaveTexture, Error, TEXT("Failed to read pixels: %s"), *Why);
				return true;
			}
		}
	}
	else
	{
		UE_LOG(LogPCGSaveTexture, Error, TEXT("No compatible input: provide PCGPointData or PCGTextureData."));
		return true;
	}

	// Prepare destination
	UTexture2D* TargetTexture = nullptr;
	bool bIsAsset = false;

	if (Settings->SaveTarget == EPCGTextureSaveTarget::TransientTexture)
	{
		if (Settings->WriteMode == EPCGTextureWriteMode::OverwriteExisting)
		{
			TargetTexture = Settings->TransientTextureToOverwrite;
			if (!TargetTexture)
			{
				UE_LOG(LogPCGSaveTexture, Error, TEXT("OverwriteExisting selected but TransientTextureToOverwrite is null."));
				return true;
			}
		}
	}
#if WITH_EDITOR
	else
	{
		if (Settings->WriteMode == EPCGTextureWriteMode::OverwriteExisting)
		{
			TargetTexture = Settings->AssetTextureToOverwrite;
			if (!TargetTexture)
			{
				UE_LOG(LogPCGSaveTexture, Error, TEXT("OverwriteExisting selected but AssetTextureToOverwrite is null."));
				return true;
			}
		}
		else
		{
			FString Why;
			if (!UPCGSaveTextureToResourceSettings::CreateAssetTexture2D(TargetTexture, Settings->AssetPackagePath, Settings->AssetName, Why))
			{
				UE_LOG(LogPCGSaveTexture, Error, TEXT("Failed to create asset texture: %s"), *Why);
				return true;
			}
		}
		bIsAsset = true;
	}
#endif

	// Write (BGRA8 or RGBA16f)
	FString WriteWhy;
	if (!UPCGSaveTextureToResourceSettings::SaveLinearToTexture2D(
		/*InOut*/TargetTexture, W, H, LinearPixels,
		Settings->OutputFormat, Settings->bSRGB, Settings->bGenerateMips, bIsAsset, WriteWhy))
	{
		UE_LOG(LogPCGSaveTexture, Error, TEXT("Failed to write pixels to texture: %s"), *WriteWhy);
		return true;
	}

#if WITH_EDITOR
	if (bIsAsset && TargetTexture)
	{
		TargetTexture->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(TargetTexture); // idempotent

		if (Settings->bAutoSavePackage)
		{
			UPackage* Pkg = TargetTexture->GetOutermost();
			const FString Filename = FPackageName::LongPackageNameToFilename(Pkg->GetName(), FPackageName::GetAssetPackageExtension());
			FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Pkg, TargetTexture, *Filename, Args))
			{
				UE_LOG(LogPCGSaveTexture, Warning, TEXT("Auto-save failed for package %s"), *Pkg->GetName());
			}
		}
	}
#endif

	UE_LOG(LogPCGSaveTexture, Log, TEXT("Saved %dx%d pixels (%s) to %s"),
		W, H,
		(Settings->OutputFormat == EPCGTextureOutputFormat::RGBA16F ? TEXT("RGBA16f") : TEXT("BGRA8")),
		*GetNameSafe(TargetTexture));
	return true;
}
