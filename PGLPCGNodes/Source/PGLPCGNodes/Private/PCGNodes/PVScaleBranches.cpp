// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PVScaleBranches.h"

#include "DataTypes/PVGrowthData.h"
#include "Facades/PVBranchFacade.h"
#include "Facades/PVFoliageFacade.h"
#include "Facades/PVPlantFacade.h"
#include "Facades/PVPointFacade.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "PCGContext.h"
#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PVScaleBranchesSettings"

#if WITH_EDITOR
FText UPVScaleBranchesSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Scale Branches");
}

FText UPVScaleBranchesSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Scales all non-trunk branches uniformly. Each branch scales from its own attachment point,\n"
		"so branches stay anchored to the trunk while growing or shrinking.\n"
		"Radius, length, and foliage are all multiplied by BranchScale. The trunk is not modified.\n\n"
		"Press Ctrl + L to lock/unlock node output"
	);
}
#endif

FPCGElementPtr UPVScaleBranchesSettings::CreateElement() const
{
	return MakeShared<FPVScaleBranchesElement>();
}

bool FPVScaleBranchesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPVScaleBranchesElement::Execute);

	check(InContext);

	const UPVScaleBranchesSettings* Settings = InContext->GetInputSettings<UPVScaleBranchesSettings>();
	check(Settings);

	const float BranchScale = Settings->BranchScale;
	const bool bScaleFoliage = Settings->bScaleFoliage;

	for (const FPCGTaggedData& Input : InContext->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel))
	{
		const UPVGrowthData* InputData = Cast<UPVGrowthData>(Input.Data);
		if (!InputData)
		{
			PCGLog::InputOutput::LogInvalidInputDataError(InContext);
			return true;
		}

		FManagedArrayCollection OutCollection = InputData->GetCollection();

		PV::Facades::FBranchFacade BranchFacade(OutCollection);
		PV::Facades::FPointFacade PointFacade(OutCollection);
		PV::Facades::FPlantFacade PlantFacade(OutCollection);
		PV::Facades::FFoliageFacade FoliageFacade(OutCollection);

		if (!PointFacade.IsValid() || !BranchFacade.IsValid() || !PlantFacade.IsValid())
		{
			UPVGrowthData* OutData = FPCGContext::NewObject_AnyThread<UPVGrowthData>(InContext);
			OutData->Initialize(MoveTemp(OutCollection));
			InContext->OutputData.TaggedData.Emplace(OutData);
			continue;
		}

		for (const TMap<int32, int32> PlantNumbersToTrunkIDs = PlantFacade.GetPlantNumbersToTrunkIndicesMap();
		     const TPair<int32, int32>& Pair : PlantNumbersToTrunkIDs)
		{
			const int32 PlantNumber = Pair.Key;
			const int32 TrunkIndex = Pair.Value;

			if (TrunkIndex == INDEX_NONE)
			{
				continue;
			}

			for (const int32 BranchIndex : PlantFacade.GetBranchIndices(PlantNumber))
			{
				// Leave the trunk untouched
				if (PlantFacade.IsTrunkIndex(BranchIndex))
				{
					continue;
				}

				const TArray<int32>& BranchPoints = BranchFacade.GetPoints(BranchIndex);
				if (BranchPoints.Num() == 0)
				{
					continue;
				}

				// Pivot on the branch's own first point (its attachment to the parent).
				// This keeps the branch anchored while scaling it outward from that joint.
				const FVector3f AttachmentPos = PointFacade.GetPosition(BranchPoints[0]);

				for (int32 i = 0; i < BranchPoints.Num(); i++)
				{
					const int32 PointIndex = BranchPoints[i];

					// Always scale radius and lengths
					PointFacade.ModifyPointScales()[PointIndex] *= BranchScale;
					PointFacade.ModifyLengthFromSeeds()[PointIndex] *= BranchScale;
					PointFacade.ModifyLengthFromRoots()[PointIndex] *= BranchScale;

					// First point is the attachment — don't move it, only scale its thickness
					if (i == 0)
					{
						continue;
					}

					FVector3f PointPos = PointFacade.GetPosition(PointIndex);
					PointPos -= AttachmentPos;
					PointPos *= BranchScale;
					PointPos += AttachmentPos;
					PointFacade.ModifyPositions()[PointIndex] = PointPos;
				}

				if (bScaleFoliage && FoliageFacade.IsValid())
				{
					for (const int32 FoliageIndex : FoliageFacade.GetFoliageEntryIdsForBranch(BranchIndex))
					{
						PV::Facades::FFoliageEntryData Data = FoliageFacade.GetFoliageEntry(FoliageIndex);

						Data.PivotPoint -= AttachmentPos;
						Data.PivotPoint *= BranchScale;
						Data.PivotPoint += AttachmentPos;

						Data.Scale *= BranchScale;
						Data.LengthFromRoot *= BranchScale;

						FoliageFacade.SetFoliageEntry(FoliageIndex, Data);
					}
				}
			}
		}

		UPVGrowthData* OutData = FPCGContext::NewObject_AnyThread<UPVGrowthData>(InContext);
		OutData->Initialize(MoveTemp(OutCollection));
		InContext->OutputData.TaggedData.Emplace(OutData);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
