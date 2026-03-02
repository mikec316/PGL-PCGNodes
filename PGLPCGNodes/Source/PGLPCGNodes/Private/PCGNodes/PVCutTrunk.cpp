// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PVCutTrunk.h"

#include "DataTypes/PVGrowthData.h"
#include "Facades/PVBranchFacade.h"
#include "Facades/PVFoliageFacade.h"
#include "Facades/PVPlantFacade.h"
#include "Facades/PVPointFacade.h"
#include "Facades/PVTreeFacade.h"         // private PVE header — accessed via PrivateIncludePaths
#include "GeometryCollection/ManagedArrayCollection.h"
#include "PCGContext.h"
#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PVCutTrunkSettings"

#if WITH_EDITOR
FText UPVCutTrunkSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Cut Trunk");
}

FText UPVCutTrunkSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Cuts the trunk to the range [TrunkStart, CutLength] in LengthFromRoot units.\n\n"
		"TrunkStart = 0 (default): keeps the trunk from root up to CutLength — identical\n"
		"  to the original single-cut behaviour.\n"
		"TrunkStart > 0: removes trunk below TrunkStart AND above CutLength, leaving a\n"
		"  floating chunk — useful for chopping / gameplay interactions.\n\n"
		"Branches attached to removed portions are removed with their full sub-trees.\n\n"
		"Press Ctrl + L to lock/unlock node output"
	);
}
#endif

FPCGElementPtr UPVCutTrunkSettings::CreateElement() const
{
	return MakeShared<FPVCutTrunkElement>();
}

bool FPVCutTrunkElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPVCutTrunkElement::Execute);

	check(InContext);

	const UPVCutTrunkSettings* Settings = InContext->GetInputSettings<UPVCutTrunkSettings>();
	check(Settings);

	const float TrunkStart = Settings->TrunkStart;
	const float CutLength  = FMath::Max(Settings->CutLength, TrunkStart); // guard against inverted range

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
		PV::Facades::FPointFacade  PointFacade(OutCollection);
		PV::Facades::FPlantFacade  PlantFacade(OutCollection);
		PV::Facades::FFoliageFacade FoliageFacade(OutCollection);

		if (!PointFacade.IsValid() || !BranchFacade.IsValid() || !PlantFacade.IsValid())
		{
			UPVGrowthData* OutData = FPCGContext::NewObject_AnyThread<UPVGrowthData>(InContext);
			OutData->Initialize(MoveTemp(OutCollection));
			InContext->OutputData.TaggedData.Emplace(OutData);
			continue;
		}

		// Removal masks (sized once, filled per-plant below)
		const int32 TotalPoints   = PointFacade.GetElementCount();
		const int32 TotalBranches = BranchFacade.GetElementCount();
		const int32 TotalFoliage  = FoliageFacade.IsValid() ? FoliageFacade.NumFoliageEntries() : 0;

		TArray<bool> PointsToRemove,  BranchesToRemove, FoliageToRemove;
		PointsToRemove .Init(false, TotalPoints);
		BranchesToRemove.Init(false, TotalBranches);
		FoliageToRemove .Init(false, TotalFoliage);

		// Build a branch-number → branch-index lookup for child traversal
		TMap<int32, int32> BranchNumberToIndex;
		BranchNumberToIndex.Reserve(TotalBranches);
		for (int32 i = 0; i < TotalBranches; ++i)
		{
			BranchNumberToIndex.Add(BranchFacade.GetBranchNumber(i), i);
		}

		// Recursive helper: mark a branch, all its points, foliage, and descendants for removal
		TFunction<void(int32)> MarkBranchSubtree = [&](int32 BranchIdx)
		{
			BranchesToRemove[BranchIdx] = true;

			for (const int32 PointIdx : BranchFacade.GetPoints(BranchIdx))
			{
				PointsToRemove[PointIdx] = true;
			}

			if (FoliageFacade.IsValid())
			{
				for (const int32 FolIdx : FoliageFacade.GetFoliageEntryIdsForBranch(BranchIdx))
				{
					FoliageToRemove[FolIdx] = true;
				}
			}

			for (const int32 ChildBranchNumber : BranchFacade.GetChildren(BranchIdx))
			{
				if (const int32* ChildIdx = BranchNumberToIndex.Find(ChildBranchNumber))
				{
					MarkBranchSubtree(*ChildIdx);
				}
			}
		};

		for (const TMap<int32, int32> PlantMap = PlantFacade.GetPlantNumbersToTrunkIndicesMap();
		     const TPair<int32, int32>& Pair : PlantMap)
		{
			const int32 PlantNumber = Pair.Key;
			const int32 TrunkIndex  = Pair.Value;

			if (TrunkIndex == INDEX_NONE)
			{
				continue;
			}

			const TArray<int32>& TrunkPoints = BranchFacade.GetPoints(TrunkIndex);
			if (TrunkPoints.Num() == 0)
			{
				continue;
			}

			// Helper: returns true if a LengthFromRoot value lies outside [TrunkStart, CutLength]
			auto OutsideRange = [TrunkStart, CutLength](float L) -> bool
			{
				return L < TrunkStart || L > CutLength;
			};

			// --- 1. Mark trunk points outside the kept range ---
			for (const int32 PointIdx : TrunkPoints)
			{
				if (OutsideRange(PointFacade.GetLengthFromRoot(PointIdx)))
				{
					PointsToRemove[PointIdx] = true;
				}
			}

			// --- 2. Mark trunk foliage outside the kept range ---
			if (FoliageFacade.IsValid())
			{
				for (const int32 FolIdx : FoliageFacade.GetFoliageEntryIdsForBranch(TrunkIndex))
				{
					if (OutsideRange(FoliageFacade.GetLengthFromRoot(FolIdx)))
					{
						FoliageToRemove[FolIdx] = true;
					}
				}
			}

			// --- 3. Mark branches whose attachment sits outside the kept range ---
			// The attachment point (first point of each child branch) determines whether the
			// whole sub-tree is inside the kept range.  If not, mark the entire sub-tree.
			for (const int32 ChildBranchNumber : BranchFacade.GetChildren(TrunkIndex))
			{
				const int32* ChildIdxPtr = BranchNumberToIndex.Find(ChildBranchNumber);
				if (!ChildIdxPtr)
				{
					continue;
				}

				const int32 ChildBranchIdx = *ChildIdxPtr;
				const TArray<int32>& BranchPoints = BranchFacade.GetPoints(ChildBranchIdx);
				if (BranchPoints.Num() == 0)
				{
					continue;
				}

				// First point is the attachment — if outside the range, remove the whole sub-tree
				if (OutsideRange(PointFacade.GetLengthFromRoot(BranchPoints[0])))
				{
					MarkBranchSubtree(ChildBranchIdx);
				}
			}
		}

		// --- 4. Remove everything in one re-indexed pass ---
		PV::Facades::FTreeFacade::RemoveEntriesAndReIndexAttributes(
			OutCollection, PointsToRemove, BranchesToRemove, FoliageToRemove);

		UPVGrowthData* OutData = FPCGContext::NewObject_AnyThread<UPVGrowthData>(InContext);
		OutData->Initialize(MoveTemp(OutCollection));
		InContext->OutputData.TaggedData.Emplace(OutData);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
