// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PGLVoxelAttributeMarshalling.h"

#include "Async/ParallelFor.h"
#include "VoxelPointId.h"
#include "VoxelPinValue.h"
#include "VoxelObjectPinType.h"
#include "VoxelPCGUtilities.h"
#include "Buffer/VoxelFloatBuffers.h"
#include "Buffer/VoxelDoubleBuffers.h"
#include "Buffer/VoxelNameBuffer.h"
#include "Buffer/VoxelSoftObjectPathBuffer.h"

namespace PGLVoxelMarshalling
{

// ─────────────────────────────────────────────────────────────────────────────
// Internal: AddAttribute helper using pre-extracted entry keys
// ─────────────────────────────────────────────────────────────────────────────

namespace Internal
{
	/**
	 * Write values to a metadata attribute using pre-extracted entry keys.
	 * The entry keys array is shared across all attribute writers to avoid
	 * redundant per-attribute extraction.
	 */
	template<typename T>
	static void AddAttribute(
		UPCGMetadata& Metadata,
		const TConstVoxelArrayView<PCGMetadataEntryKey>& EntryKeys,
		int32 NumPoints,
		const FName Name,
		const TConstVoxelArrayView<T> Values,
		const T DefaultValue = FVoxelUtilities::MakeSafe<T>(),
		const bool bAllowInterpolation = true)
	{
		VOXEL_FUNCTION_COUNTER();

		if (!ensure(NumPoints == Values.Num()) ||
			!ensure(NumPoints == EntryKeys.Num()))
		{
			return;
		}

		FPCGMetadataAttribute<T>* Attribute = Metadata.FindOrCreateAttribute<T>(Name, DefaultValue, bAllowInterpolation, true);
		if (!ensure(Attribute))
		{
			return;
		}

		const TArray<PCGMetadataValueKey> ValueKeys = Attribute->AddValues(Values);
		Attribute->SetValuesFromValueKeys(EntryKeys, ValueKeys);
	}

	/**
	 * Write values from a typed voxel buffer to a metadata attribute.
	 * Reads from the buffer directly — no intermediate TVoxelArray copy
	 * (except the final conversion array for AddValues).
	 */
	template<typename BufferType, typename T>
	static void AddAttributeFromBuffer(
		UPCGMetadata& Metadata,
		const TConstVoxelArrayView<PCGMetadataEntryKey>& EntryKeys,
		int32 NumPoints,
		const FName Name,
		const BufferType& TypedBuffer,
		const T DefaultValue = FVoxelUtilities::MakeSafe<T>(),
		const bool bAllowInterpolation = true)
	{
		VOXEL_FUNCTION_COUNTER();

		if (!ensure(NumPoints == EntryKeys.Num()))
		{
			return;
		}

		// Build values array from buffer (single pass)
		TVoxelArray<T> Values;
		if constexpr (std::is_trivially_destructible_v<T>)
		{
			FVoxelUtilities::SetNumFast(Values, NumPoints);
		}
		else
		{
			FVoxelUtilities::SetNum(Values, NumPoints);
		}

		for (int32 Index = 0; Index < NumPoints; Index++)
		{
			Values[Index] = T(TypedBuffer[Index]);
		}

		FPCGMetadataAttribute<T>* Attribute = Metadata.FindOrCreateAttribute<T>(Name, DefaultValue, bAllowInterpolation, true);
		if (!ensure(Attribute))
		{
			return;
		}

		const TArray<PCGMetadataValueKey> ValueKeys = Attribute->AddValues(Values);
		Attribute->SetValuesFromValueKeys(EntryKeys, ValueKeys);
	}
} // namespace Internal

// ─────────────────────────────────────────────────────────────────────────────
// BuildStandardAttributes — FPCGPoint array → Voxel buffers
// ParallelFor batching for better throughput than sequential loop.
// ─────────────────────────────────────────────────────────────────────────────

void BuildStandardAttributes(
	TConstVoxelArrayView<FPCGPoint> Points,
	int32 NumPoints,
	FVoxelPointSet& OutPointSet)
{
	VOXEL_FUNCTION_COUNTER();

	OutPointSet.SetNum(NumPoints);

	FVoxelDoubleVectorBuffer Position;
	Position.Allocate(NumPoints);
	FVoxelQuaternionBuffer Rotation;
	Rotation.Allocate(NumPoints);
	FVoxelVectorBuffer Scale;
	Scale.Allocate(NumPoints);
	FVoxelFloatBuffer Density;
	Density.Allocate(NumPoints);
	FVoxelVectorBuffer BoundsMin;
	BoundsMin.Allocate(NumPoints);
	FVoxelVectorBuffer BoundsMax;
	BoundsMax.Allocate(NumPoints);
	FVoxelLinearColorBuffer Color;
	Color.Allocate(NumPoints);
	FVoxelFloatBuffer Steepness;
	Steepness.Allocate(NumPoints);
	FVoxelPointIdBuffer Id;
	Id.Allocate(NumPoints);

	// Single parallelized pass — good cache locality reading FPCGPoint (contiguous struct)
	const int32 NumBatches = FMath::DivideAndRoundUp(NumPoints, ParallelBatchSize);
	ParallelFor(NumBatches, [&](int32 BatchIndex)
	{
		const int32 Start = BatchIndex * ParallelBatchSize;
		const int32 End = FMath::Min(Start + ParallelBatchSize, NumPoints);

		for (int32 Index = Start; Index < End; Index++)
		{
			const FPCGPoint& Point = Points[Index];

			Position.Set(Index, Point.Transform.GetTranslation());
			Rotation.Set(Index, FQuat4f(Point.Transform.GetRotation()));
			Scale.Set(Index, FVector3f(Point.Transform.GetScale3D()));
			Density.Set(Index, Point.Density);
			BoundsMin.Set(Index, FVector3f(Point.BoundsMin));
			BoundsMax.Set(Index, FVector3f(Point.BoundsMax));
			Color.Set(Index, FLinearColor(Point.Color));
			Steepness.Set(Index, Point.Steepness);
			Id.Set(Index, FVoxelPointId(Point.Seed));
		}
	});

	OutPointSet.Add(FVoxelPointAttributes::Position, MoveTemp(Position));
	OutPointSet.Add(FVoxelPointAttributes::Rotation, MoveTemp(Rotation));
	OutPointSet.Add(FVoxelPointAttributes::Scale, MoveTemp(Scale));
	OutPointSet.Add(FVoxelPointAttributes::Density, MoveTemp(Density));
	OutPointSet.Add(FVoxelPointAttributes::BoundsMin, MoveTemp(BoundsMin));
	OutPointSet.Add(FVoxelPointAttributes::BoundsMax, MoveTemp(BoundsMax));
	OutPointSet.Add(FVoxelPointAttributes::Color, MoveTemp(Color));
	OutPointSet.Add(FVoxelPointAttributes::Steepness, MoveTemp(Steepness));
	OutPointSet.Add(FVoxelPointAttributes::Id, MoveTemp(Id));
}

// ─────────────────────────────────────────────────────────────────────────────
// WriteStandardAttributesToPoints — Voxel buffers → FPCGPoint array in-place
// Runs on WORKER THREAD. Only overwrites changed properties (pass-through).
// ─────────────────────────────────────────────────────────────────────────────

void WriteStandardAttributesToPoints(
	const FVoxelPointSet& InputPointSet,
	const FVoxelPointSet& OutputPointSet,
	TVoxelArray<FPCGPoint>& Points,
	int32 OutputNumPoints)
{
	VOXEL_FUNCTION_COUNTER();

	// ── Handle size change ──────────────────────────────────────────────────
	const bool bSizeChanged = (OutputNumPoints != Points.Num());
	if (bSizeChanged)
	{
		FVoxelUtilities::SetNum(Points, OutputNumPoints);
	}

	// ── Fetch input and output buffer pointers ──────────────────────────────

	const FVoxelDoubleVectorBuffer* InPosBuf = InputPointSet.Find<FVoxelDoubleVectorBuffer>(FVoxelPointAttributes::Position);
	const FVoxelQuaternionBuffer* InRotBuf = InputPointSet.Find<FVoxelQuaternionBuffer>(FVoxelPointAttributes::Rotation);
	const FVoxelVectorBuffer* InScaleBuf = InputPointSet.Find<FVoxelVectorBuffer>(FVoxelPointAttributes::Scale);
	const FVoxelFloatBuffer* InDensityBuf = InputPointSet.Find<FVoxelFloatBuffer>(FVoxelPointAttributes::Density);
	const FVoxelVectorBuffer* InBoundsMinBuf = InputPointSet.Find<FVoxelVectorBuffer>(FVoxelPointAttributes::BoundsMin);
	const FVoxelVectorBuffer* InBoundsMaxBuf = InputPointSet.Find<FVoxelVectorBuffer>(FVoxelPointAttributes::BoundsMax);
	const FVoxelLinearColorBuffer* InColorBuf = InputPointSet.Find<FVoxelLinearColorBuffer>(FVoxelPointAttributes::Color);
	const FVoxelFloatBuffer* InSteepnessBuf = InputPointSet.Find<FVoxelFloatBuffer>(FVoxelPointAttributes::Steepness);
	const FVoxelPointIdBuffer* InIdBuf = InputPointSet.Find<FVoxelPointIdBuffer>(FVoxelPointAttributes::Id);

	const FVoxelDoubleVectorBuffer* OutPosBuf = OutputPointSet.Find<FVoxelDoubleVectorBuffer>(FVoxelPointAttributes::Position);
	const FVoxelQuaternionBuffer* OutRotBuf = OutputPointSet.Find<FVoxelQuaternionBuffer>(FVoxelPointAttributes::Rotation);
	const FVoxelVectorBuffer* OutScaleBuf = OutputPointSet.Find<FVoxelVectorBuffer>(FVoxelPointAttributes::Scale);
	const FVoxelFloatBuffer* OutDensityBuf = OutputPointSet.Find<FVoxelFloatBuffer>(FVoxelPointAttributes::Density);
	const FVoxelVectorBuffer* OutBoundsMinBuf = OutputPointSet.Find<FVoxelVectorBuffer>(FVoxelPointAttributes::BoundsMin);
	const FVoxelVectorBuffer* OutBoundsMaxBuf = OutputPointSet.Find<FVoxelVectorBuffer>(FVoxelPointAttributes::BoundsMax);
	const FVoxelLinearColorBuffer* OutColorBuf = OutputPointSet.Find<FVoxelLinearColorBuffer>(FVoxelPointAttributes::Color);
	const FVoxelFloatBuffer* OutSteepnessBuf = OutputPointSet.Find<FVoxelFloatBuffer>(FVoxelPointAttributes::Steepness);
	const FVoxelPointIdBuffer* OutIdBuf = OutputPointSet.Find<FVoxelPointIdBuffer>(FVoxelPointAttributes::Id);

	// ── Detect which properties actually changed ────────────────────────────
	// If size changed, ALL properties must be written (can't rely on in-place data)
	const bool bWritePosition   = bSizeChanged || (OutPosBuf != InPosBuf);
	const bool bWriteRotation   = bSizeChanged || (OutRotBuf != InRotBuf);
	const bool bWriteScale      = bSizeChanged || (OutScaleBuf != InScaleBuf);
	const bool bWriteDensity    = bSizeChanged || (OutDensityBuf != InDensityBuf);
	const bool bWriteBoundsMin  = bSizeChanged || (OutBoundsMinBuf != InBoundsMinBuf);
	const bool bWriteBoundsMax  = bSizeChanged || (OutBoundsMaxBuf != InBoundsMaxBuf);
	const bool bWriteColor      = bSizeChanged || (OutColorBuf != InColorBuf);
	const bool bWriteSteepness  = bSizeChanged || (OutSteepnessBuf != InSteepnessBuf);
	const bool bWriteSeed       = bSizeChanged || (OutIdBuf != InIdBuf);

	const bool bAnyWrite = bWritePosition || bWriteRotation || bWriteScale ||
		bWriteDensity || bWriteBoundsMin || bWriteBoundsMax ||
		bWriteColor || bWriteSteepness || bWriteSeed;

	if (!bAnyWrite)
	{
		return;
	}

	// ── ParallelFor — only write changed properties ─────────────────────────
	const int32 Num = OutputNumPoints;
	const int32 NumBatches = FMath::DivideAndRoundUp(Num, ParallelBatchSize);
	ParallelFor(NumBatches, [&](int32 BatchIndex)
	{
		const int32 Start = BatchIndex * ParallelBatchSize;
		const int32 End = FMath::Min(Start + ParallelBatchSize, Num);

		for (int32 Index = Start; Index < End; Index++)
		{
			FPCGPoint& Point = Points[Index];

			if (bWritePosition && OutPosBuf)   { Point.Transform.SetTranslation((*OutPosBuf)[Index]); }
			if (bWriteRotation && OutRotBuf)   { Point.Transform.SetRotation(FQuat((*OutRotBuf)[Index])); }
			if (bWriteScale && OutScaleBuf)    { Point.Transform.SetScale3D(FVector((*OutScaleBuf)[Index])); }
			if (bWriteDensity && OutDensityBuf)     { Point.Density = (*OutDensityBuf)[Index]; }
			if (bWriteBoundsMin && OutBoundsMinBuf) { Point.BoundsMin = FVector((*OutBoundsMinBuf)[Index]); }
			if (bWriteBoundsMax && OutBoundsMaxBuf) { Point.BoundsMax = FVector((*OutBoundsMaxBuf)[Index]); }
			if (bWriteColor && OutColorBuf)         { Point.Color = FVector4((*OutColorBuf)[Index]); }
			if (bWriteSteepness && OutSteepnessBuf) { Point.Steepness = (*OutSteepnessBuf)[Index]; }
			if (bWriteSeed && OutIdBuf)             { Point.Seed = static_cast<int32>((*OutIdBuf)[Index].PointId); }
		}
	});
}

// ─────────────────────────────────────────────────────────────────────────────
// PCGAttributeToVoxelBuffer — Metadata → Voxel buffer (FPCGPoint MetadataEntry)
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FVoxelBuffer> PCGAttributeToVoxelBuffer(
	const FPCGMetadataAttributeBase& Attribute,
	EPCGMetadataTypes AttributeType,
	TConstVoxelArrayView<FPCGPoint> Points,
	int32 NumPoints,
	const FName& AttributeName,
	const TMap<FName, FVoxelPinType>& AttributeToObjectType)
{
	VOXEL_SCOPE_COUNTER_FORMAT("CopyAttribute %s", *AttributeName.ToString());

	TSharedPtr<FVoxelBuffer> Result;

	const auto CopyAttribute = [&]<typename T>(const FPCGMetadataAttribute<T>& TypedAttribute)
	{
		const int32 Num = NumPoints;

		if constexpr (
			std::is_same_v<T, bool> ||
			std::is_same_v<T, float> ||
			std::is_same_v<T, double> ||
			std::is_same_v<T, int32> ||
			std::is_same_v<T, int64>)
		{
			TVoxelBufferTypeSafe<T> Buffer;
			Buffer.Allocate(Num);

			const int32 NumBatches = FMath::DivideAndRoundUp(Num, ParallelBatchSize);
			ParallelFor(NumBatches, [&](int32 BatchIndex)
			{
				const int32 Start = BatchIndex * ParallelBatchSize;
				const int32 End = FMath::Min(Start + ParallelBatchSize, Num);
				for (int32 Index = Start; Index < End; Index++)
				{
					Buffer.Set(Index, TypedAttribute.GetValueFromItemKey(Points[Index].MetadataEntry));
				}
			});

			Result = MakeSharedCopy(MoveTemp(Buffer));
		}
		else if constexpr (
			std::is_same_v<T, FVector2D> ||
			std::is_same_v<T, FVector> ||
			std::is_same_v<T, FQuat> ||
			std::is_same_v<T, FTransform>)
		{
			TVoxelDoubleBufferTypeSafe<T> Buffer;
			Buffer.Allocate(Num);

			const int32 NumBatches = FMath::DivideAndRoundUp(Num, ParallelBatchSize);
			ParallelFor(NumBatches, [&](int32 BatchIndex)
			{
				const int32 Start = BatchIndex * ParallelBatchSize;
				const int32 End = FMath::Min(Start + ParallelBatchSize, Num);
				for (int32 Index = Start; Index < End; Index++)
				{
					Buffer.Set(Index, TypedAttribute.GetValueFromItemKey(Points[Index].MetadataEntry));
				}
			});

			Result = MakeSharedCopy(MoveTemp(Buffer));
		}
		else if constexpr (std::is_same_v<T, FVector4>)
		{
			FVoxelDoubleLinearColorBuffer Buffer;
			Buffer.Allocate(Num);

			for (int32 Index = 0; Index < Num; Index++)
			{
				Buffer.Set(Index, FVoxelDoubleLinearColor(TypedAttribute.GetValueFromItemKey(Points[Index].MetadataEntry)));
			}

			Result = MakeSharedCopy(MoveTemp(Buffer));
		}
		else if constexpr (
			std::is_same_v<T, FString> ||
			std::is_same_v<T, FName>)
		{
			FVoxelNameBuffer Buffer;
			Buffer.Allocate(Num);

			for (int32 Index = 0; Index < Num; Index++)
			{
				Buffer.Set(Index, FName(TypedAttribute.GetValueFromItemKey(Points[Index].MetadataEntry)));
			}

			Result = MakeSharedCopy(MoveTemp(Buffer));
		}
		else if constexpr (std::is_same_v<T, FRotator>)
		{
			FVoxelDoubleVectorBuffer Buffer;
			Buffer.Allocate(Num);

			for (int32 Index = 0; Index < Num; Index++)
			{
				const FRotator& Value = TypedAttribute.GetValueFromItemKey(Points[Index].MetadataEntry);
				Buffer.Set(Index, FVector(Value.Pitch, Value.Yaw, Value.Roll));
			}

			Result = MakeSharedCopy(MoveTemp(Buffer));
		}
		else if constexpr (std::is_same_v<T, FSoftObjectPath>)
		{
			if (const FVoxelPinType* PinType = AttributeToObjectType.Find(AttributeName))
			{
				TSharedRef<FVoxelBuffer> ObjectBuffer = FVoxelBuffer::MakeEmpty(*PinType);
				ObjectBuffer->Allocate(Num);

				for (int32 Index = 0; Index < Num; Index++)
				{
					UObject* Object = TypedAttribute.GetValueFromItemKey(Points[Index].MetadataEntry).ResolveObject();
					FVoxelPinValue PinValue = FVoxelPinValue::Make(Object);
					PinValue.Fixup(PinType->GetExposedType());
					ObjectBuffer->SetGeneric(Index, FVoxelPinType::MakeRuntimeValue(*PinType, PinValue, {}));
				}

				Result = ObjectBuffer;
			}
			else
			{
				FVoxelSoftObjectPathBuffer Buffer;
				Buffer.Allocate(Num);

				for (int32 Index = 0; Index < Num; Index++)
				{
					Buffer.Set(Index, TypedAttribute.GetValueFromItemKey(Points[Index].MetadataEntry));
				}

				Result = MakeSharedCopy(MoveTemp(Buffer));
			}
		}
		else if constexpr (std::is_same_v<T, FSoftClassPath>)
		{
			FVoxelSoftClassPathBuffer Buffer;
			Buffer.Allocate(Num);

			for (int32 Index = 0; Index < Num; Index++)
			{
				Buffer.Set(Index, TypedAttribute.GetValueFromItemKey(Points[Index].MetadataEntry));
			}

			Result = MakeSharedCopy(MoveTemp(Buffer));
		}
		else
		{
			// Unsupported type
		}
	};

	FVoxelPCGUtilities::SwitchOnAttribute(
		AttributeType,
		Attribute,
		CopyAttribute);

	return Result;
}

// ─────────────────────────────────────────────────────────────────────────────
// VoxelBufferToPCGWriter — Voxel buffer → Metadata writer
// Captures buffer ref directly (no intermediate TVoxelArray).
// Writers receive pre-extracted entry keys from shared array.
// ─────────────────────────────────────────────────────────────────────────────

FMetadataWriter VoxelBufferToPCGWriter(
	const FName& AttributeName,
	const TSharedPtr<const FVoxelBuffer>& Buffer,
	int32 NumPoints,
	EPCGMetadataTypes OriginalType)
{
	VOXEL_SCOPE_COUNTER_FORMAT("ConvertAttribute %s", *AttributeName.ToString());

	if (!Buffer)
	{
		return {};
	}

#if PLATFORM_ANDROID
	// Template-heavy code doesn't compile on Android
	return {};
#else
	// Special case: Rotator stored as FVoxelDoubleVectorBuffer
	if (OriginalType == EPCGMetadataTypes::Rotator)
	{
		if (Buffer->IsA<FVoxelDoubleVectorBuffer>())
		{
			return [AttributeName, Buffer, NumPoints](
				UPCGMetadata& Metadata,
				const TConstVoxelArrayView<PCGMetadataEntryKey>& EntryKeys)
			{
				const FVoxelDoubleVectorBuffer& VecBuffer = Buffer->AsChecked<FVoxelDoubleVectorBuffer>();
				TVoxelArray<FRotator> Values;
				FVoxelUtilities::SetNumFast(Values, NumPoints);
				for (int32 Index = 0; Index < NumPoints; Index++)
				{
					Values[Index] = FRotator(
						VecBuffer.X[Index],
						VecBuffer.Y[Index],
						VecBuffer.Z[Index]);
				}
				Internal::AddAttribute<FRotator>(Metadata, EntryKeys, NumPoints, AttributeName, Values);
			};
		}
	}

	// Generic typed conversion helper — captures buffer shared_ptr, reads in writer
	const auto ProcessImpl = [&]<typename BufferType, typename Type>() -> FMetadataWriter
	{
		if (!ensure(Buffer->AsChecked<BufferType>().IsConstant() || Buffer->AsChecked<BufferType>().Num() == NumPoints))
		{
			return {};
		}

		return [AttributeName, Buffer, NumPoints](
			UPCGMetadata& Metadata,
			const TConstVoxelArrayView<PCGMetadataEntryKey>& EntryKeys)
		{
			const BufferType& TypedBuffer = Buffer->AsChecked<BufferType>();
			Internal::AddAttributeFromBuffer<BufferType, Type>(
				Metadata, EntryKeys, NumPoints, AttributeName, TypedBuffer);
		};
	};

	// Scalar types
	{
		const auto Process = [&]<typename Type>() -> FMetadataWriter
		{
			if (!Buffer->IsA<TVoxelBufferType<Type>>())
			{
				return {};
			}
			return ProcessImpl.template operator()<TVoxelBufferType<Type>, Type>();
		};

		if (auto W = Process.template operator()<bool>()) { return W; }
		if (auto W = Process.template operator()<float>()) { return W; }
		if (auto W = Process.template operator()<double>()) { return W; }
		if (auto W = Process.template operator()<int32>()) { return W; }
		if (auto W = Process.template operator()<int64>()) { return W; }
	}

	// Vector/compound types
	{
		const auto Process = [&]<typename Type>() -> FMetadataWriter
		{
			if (Buffer->IsA<TVoxelBufferType<Type>>())
			{
				return ProcessImpl.template operator()<TVoxelBufferType<Type>, Type>();
			}
			if (Buffer->IsA<TVoxelDoubleBufferType<Type>>())
			{
				return ProcessImpl.template operator()<TVoxelDoubleBufferType<Type>, Type>();
			}
			return {};
		};

		if (auto W = Process.template operator()<FVector2D>()) { return W; }
		if (auto W = Process.template operator()<FVector>()) { return W; }
		if (auto W = Process.template operator()<FQuat>()) { return W; }
		if (auto W = Process.template operator()<FTransform>()) { return W; }
	}

	// PointId buffer
	if (Buffer->IsA<FVoxelPointIdBuffer>())
	{
		return [AttributeName, Buffer, NumPoints](
			UPCGMetadata& Metadata,
			const TConstVoxelArrayView<PCGMetadataEntryKey>& EntryKeys)
		{
			const FVoxelPointIdBuffer& IdBuffer = Buffer->AsChecked<FVoxelPointIdBuffer>();
			TVoxelArray<int64> Values;
			FVoxelUtilities::SetNumFast(Values, NumPoints);
			for (int32 Index = 0; Index < NumPoints; Index++)
			{
				Values[Index] = IdBuffer[Index].PointId;
			}
			Internal::AddAttribute<int64>(Metadata, EntryKeys, NumPoints, AttributeName, Values);
		};
	}

	// Color buffers
	if (Buffer->IsA<FVoxelLinearColorBuffer>())
	{
		return ProcessImpl.template operator()<FVoxelLinearColorBuffer, FVector4>();
	}
	if (Buffer->IsA<FVoxelDoubleLinearColorBuffer>())
	{
		return ProcessImpl.template operator()<FVoxelDoubleLinearColorBuffer, FVector4>();
	}

	// Soft path buffers
	if (Buffer->IsA<FVoxelSoftObjectPathBuffer>())
	{
		return ProcessImpl.template operator()<FVoxelSoftObjectPathBuffer, FSoftObjectPath>();
	}
	if (Buffer->IsA<FVoxelSoftClassPathBuffer>())
	{
		return ProcessImpl.template operator()<FVoxelSoftClassPathBuffer, FSoftClassPath>();
	}

	// Name buffer -> FName or FString depending on original type
	if (Buffer->IsA<FVoxelNameBuffer>())
	{
		if (OriginalType == EPCGMetadataTypes::String)
		{
			return [AttributeName, Buffer, NumPoints](
				UPCGMetadata& Metadata,
				const TConstVoxelArrayView<PCGMetadataEntryKey>& EntryKeys)
			{
				const FVoxelNameBuffer& NameBuffer = Buffer->AsChecked<FVoxelNameBuffer>();
				TVoxelArray<FString> Values;
				FVoxelUtilities::SetNum(Values, NumPoints);
				for (int32 Index = 0; Index < NumPoints; Index++)
				{
					Values[Index] = FName(NameBuffer[Index]).ToString();
				}
				Internal::AddAttribute<FString>(Metadata, EntryKeys, NumPoints, AttributeName, Values);
			};
		}

		return [AttributeName, Buffer, NumPoints](
			UPCGMetadata& Metadata,
			const TConstVoxelArrayView<PCGMetadataEntryKey>& EntryKeys)
		{
			const FVoxelNameBuffer& NameBuffer = Buffer->AsChecked<FVoxelNameBuffer>();
			TVoxelArray<FName> Values;
			FVoxelUtilities::SetNumFast(Values, NumPoints);
			for (int32 Index = 0; Index < NumPoints; Index++)
			{
				Values[Index] = FName(NameBuffer[Index]);
			}
			Internal::AddAttribute<FName>(Metadata, EntryKeys, NumPoints, AttributeName, Values);
		};
	}

	// Object pin types
	if (const TSharedPtr<const FVoxelObjectPinType> ObjectPinType = FVoxelObjectPinType::StructToPinType().FindRef(Buffer->GetInnerType().GetStruct()))
	{
		return [AttributeName, Buffer, ObjectPinType, NumPoints](
			UPCGMetadata& Metadata,
			const TConstVoxelArrayView<PCGMetadataEntryKey>& EntryKeys)
		{
			checkVoxelSlow(IsInGameThread());

			TVoxelArray<FSoftObjectPath> Values;
			FVoxelUtilities::SetNum(Values, NumPoints);
			for (int32 Index = 0; Index < NumPoints; Index++)
			{
				Values[Index] = FSoftObjectPath(
					ObjectPinType->GetWeakObject(Buffer->GetGeneric(Index).GetStructView()).Resolve());
			}

			Internal::AddAttribute<FSoftObjectPath>(Metadata, EntryKeys, NumPoints, AttributeName, Values);
		};
	}

	VOXEL_MESSAGE(Error, "PGLCallVoxelGraph: Unsupported buffer type for attribute {0}: {1}", AttributeName, Buffer->GetBufferType().ToString());
	return {};
#endif
}

} // namespace PGLVoxelMarshalling
