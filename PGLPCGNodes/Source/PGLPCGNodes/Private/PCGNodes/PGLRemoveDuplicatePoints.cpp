// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PGLRemoveDuplicatePoints.h"

#include "PCGContext.h"
#include "PCGData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Metadata/PCGMetadata.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PGLRemoveDuplicatePoints)

#define LOCTEXT_NAMESPACE "PGLRemoveDuplicatePoints"

// ---------------------------------------------------------------------------
// UPGLRemoveDuplicatePointsSettings
// ---------------------------------------------------------------------------

#if WITH_EDITOR
FText UPGLRemoveDuplicatePointsSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "PGL Remove Duplicate Points");
}

FText UPGLRemoveDuplicatePointsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Takes two point sets ('Source' and 'Remove') and outputs Source points\n"
		"that do NOT have a matching point in Remove within the specified tolerance.\n\n"
		"Useful for removing foliage or placed instances that overlap with another data set.");
}
#endif

TArray<FPCGPinProperties> UPGLRemoveDuplicatePointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace_GetRef(PGLRemoveDuplicatePointsConstants::SourceLabel, EPCGDataType::Point).SetRequiredPin();
	PinProperties.Emplace_GetRef(PGLRemoveDuplicatePointsConstants::RemoveLabel, EPCGDataType::Point).SetRequiredPin();
	return PinProperties;
}

TArray<FPCGPinProperties> UPGLRemoveDuplicatePointsSettings::OutputPinProperties() const
{
	return DefaultPointOutputPinProperties();
}

FPCGElementPtr UPGLRemoveDuplicatePointsSettings::CreateElement() const
{
	return MakeShared<FPGLRemoveDuplicatePointsElement>();
}

// ---------------------------------------------------------------------------
// FPGLRemoveDuplicatePointsElement
// ---------------------------------------------------------------------------

bool FPGLRemoveDuplicatePointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPGLRemoveDuplicatePointsElement::Execute);
	check(InContext);

	const UPGLRemoveDuplicatePointsSettings* Settings = InContext->GetInputSettings<UPGLRemoveDuplicatePointsSettings>();
	check(Settings);

	const double ToleranceSq = Settings->Tolerance * Settings->Tolerance;

	// --- Collect all Remove points into a flat array for spatial lookup ---
	TArray<FVector> RemovePositions;

	for (const FPCGTaggedData& RemoveInput : InContext->InputData.GetInputsByPin(PGLRemoveDuplicatePointsConstants::RemoveLabel))
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(RemoveInput.Data);
		if (!SpatialData) { continue; }

		const UPCGPointData* RemovePointData = SpatialData->ToPointData(InContext);
		if (!RemovePointData) { continue; }

		for (const FPCGPoint& Pt : RemovePointData->GetPoints())
		{
			RemovePositions.Add(Pt.Transform.GetLocation());
		}
	}

	if (RemovePositions.Num() == 0)
	{
		// Nothing to remove — pass through source data
		for (const FPCGTaggedData& SourceInput : InContext->InputData.GetInputsByPin(PGLRemoveDuplicatePointsConstants::SourceLabel))
		{
			InContext->OutputData.TaggedData.Add(SourceInput);
		}
		return true;
	}

	// Build a simple grid-based spatial hash for O(1) average lookup
	const double CellSize = FMath::Max(Settings->Tolerance * 2.0, 1.0);
	const double InvCellSize = 1.0 / CellSize;

	// Hash helper
	auto CellKey = [InvCellSize](const FVector& Pos) -> FIntVector
	{
		return FIntVector(
			FMath::FloorToInt(Pos.X * InvCellSize),
			FMath::FloorToInt(Pos.Y * InvCellSize),
			FMath::FloorToInt(Pos.Z * InvCellSize));
	};

	auto CellHash = [](const FIntVector& Cell) -> uint32
	{
		return HashCombine(HashCombine(GetTypeHash(Cell.X), GetTypeHash(Cell.Y)), GetTypeHash(Cell.Z));
	};

	// Populate spatial hash
	TMultiMap<uint32, int32> SpatialHash; // hash -> index into RemovePositions
	SpatialHash.Reserve(RemovePositions.Num());

	for (int32 i = 0; i < RemovePositions.Num(); ++i)
	{
		SpatialHash.Add(CellHash(CellKey(RemovePositions[i])), i);
	}

	// Lambda to check if a position matches any remove point
	auto HasMatch = [&](const FVector& Pos) -> bool
	{
		const FIntVector BaseCell = CellKey(Pos);

		// Check 3x3x3 neighborhood to handle points near cell boundaries
		for (int32 dx = -1; dx <= 1; ++dx)
		{
			for (int32 dy = -1; dy <= 1; ++dy)
			{
				for (int32 dz = -1; dz <= 1; ++dz)
				{
					const uint32 H = CellHash(FIntVector(BaseCell.X + dx, BaseCell.Y + dy, BaseCell.Z + dz));

					TArray<int32> Indices;
					SpatialHash.MultiFind(H, Indices);

					for (const int32 Idx : Indices)
					{
						if (FVector::DistSquared(Pos, RemovePositions[Idx]) <= ToleranceSq)
						{
							return true;
						}
					}
				}
			}
		}
		return false;
	};

	// --- Filter source points ---
	TArray<FPCGTaggedData>& Outputs = InContext->OutputData.TaggedData;

	for (const FPCGTaggedData& SourceInput : InContext->InputData.GetInputsByPin(PGLRemoveDuplicatePointsConstants::SourceLabel))
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(SourceInput.Data);
		if (!SpatialData) { continue; }

		const UPCGPointData* SourcePointData = SpatialData->ToPointData(InContext);
		if (!SourcePointData) { continue; }

		const TArray<FPCGPoint>& SourcePoints = SourcePointData->GetPoints();
		if (SourcePoints.Num() == 0) { continue; }

		UPCGPointData* OutPointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(InContext);
		OutPointData->InitializeFromData(SourcePointData);
		TArray<FPCGPoint>& OutPoints = OutPointData->GetMutablePoints();
		OutPoints.Reserve(SourcePoints.Num());

		for (const FPCGPoint& Point : SourcePoints)
		{
			if (!HasMatch(Point.Transform.GetLocation()))
			{
				OutPoints.Add(Point);
			}
		}

		FPCGTaggedData& Output = Outputs.Add_GetRef(SourceInput);
		Output.Data = OutPointData;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
