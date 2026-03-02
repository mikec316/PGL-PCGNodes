// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PGLFoliageDistributor.h"

#include "PCGContext.h"
#include "PCGNodes/PGLDelegationHelper.h"

#define LOCTEXT_NAMESPACE "PGLFoliageDistributor"
#include "DataTypes/PVMeshData.h"
#include "DataTypes/PVFoliageMeshData.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "Facades/PVAttributesNames.h"
#include "Helpers/PCGHelpers.h"
#include "UObject/UnrealType.h"


// =============================================================================
//  UPGLFoliageDistributorSettings
// =============================================================================

UPGLFoliageDistributorSettings::UPGLFoliageDistributorSettings()
{
	// Initialise ramp curves to match PVE's default linear curves.
	// Without this, default-constructed FPVFloatRamp members contain
	// empty FRichCurves that evaluate to 0 instead of the expected
	// linear ramp, which can alter distribution behaviour when
	// ramp-effect weights are non-zero.
	if (PCGHelpers::IsNewObjectAndNotDefault(this))
	{
		FRichCurve* RichCurve = InstanceSpacingRamp.GetRichCurve();
		RichCurve->Reset();
		FKeyHandle H0 = RichCurve->AddKey(0.0f, 0.0f);
		FKeyHandle H1 = RichCurve->AddKey(1.0f, 1.0f);
		RichCurve->SetKeyInterpMode(H0, RCIM_Linear);
		RichCurve->SetKeyInterpMode(H1, RCIM_Linear);

		RichCurve = ScaleRamp.GetRichCurve();
		RichCurve->Reset();
		FKeyHandle H2 = RichCurve->AddKey(0.0f, 0.0f);
		FKeyHandle H3 = RichCurve->AddKey(1.0f, 1.0f);
		RichCurve->SetKeyInterpMode(H2, RCIM_Linear);
		RichCurve->SetKeyInterpMode(H3, RCIM_Linear);

		RichCurve = AxilAngleRamp.GetRichCurve();
		RichCurve->Reset();
		FKeyHandle H4 = RichCurve->AddKey(0.0f, 1.0f);
		FKeyHandle H5 = RichCurve->AddKey(1.0f, 0.5f);
		RichCurve->SetKeyInterpMode(H4, RCIM_Linear);
		RichCurve->SetKeyInterpMode(H5, RCIM_Linear);
	}
}

#if WITH_EDITOR
FText UPGLFoliageDistributorSettings::GetDefaultNodeTitle() const
{
	return NSLOCTEXT("PGLFoliageDistributor", "NodeTitle", "PGL Foliage Distributor");
}

FText UPGLFoliageDistributorSettings::GetNodeTooltipText() const
{
	return NSLOCTEXT("PGLFoliageDistributor", "Tooltip",
		"Distributes foliage instances onto PVE mesh data.\n"
		"Delegates to PVE's Foliage Distributor at runtime and propagates Frame Mesh data.");
}
#endif

FPCGDataTypeIdentifier UPGLFoliageDistributorSettings::GetInputPinTypeIdentifier() const
{
	return FPCGDataTypeIdentifier{ FPVDataTypeInfoMesh::AsId() };
}

FPCGDataTypeIdentifier UPGLFoliageDistributorSettings::GetOutputPinTypeIdentifier() const
{
	return FPCGDataTypeIdentifier{ FPVDataTypeInfoFoliageMesh::AsId() };
}

FPCGElementPtr UPGLFoliageDistributorSettings::CreateElement() const
{
	return MakeShared<FPGLFoliageDistributorElement>();
}


// =============================================================================
//  FPGLFoliageDistributorElement
// =============================================================================

bool FPGLFoliageDistributorElement::ExecuteInternal(FPCGContext* Context) const
{
	const UPGLFoliageDistributorSettings* Settings =
		Context->GetInputSettings<UPGLFoliageDistributorSettings>();
	if (!Settings)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingSettings", "PGL Foliage Distributor: missing settings."));
		return true;
	}

	// --- Capture FrameMesh from input ---
	FString CapturedFrameMeshPath;
	FTransform CapturedFrameMeshTransform;
	PGLDelegation::CaptureFrameMeshFromInput(
		Context->InputData, CapturedFrameMeshPath, CapturedFrameMeshTransform);

	// --- Find PVE Foliage Distributor settings class ---
	static UClass* PVEClass = FindObject<UClass>(
		nullptr, TEXT("/Script/ProceduralVegetation.PVFoliageDistributorSettings"));
	if (!PVEClass)
	{
		PCGE_LOG(Error, GraphAndLog,
			LOCTEXT("PVEClassNotFound", "PGL Foliage Distributor: PVE FoliageDistributorSettings class not found. Is ProceduralVegetation plugin enabled?"));
		return true;
	}

	// --- Create PVE settings instance and copy matching properties ---
	UPCGSettings* PVESettings = NewObject<UPCGSettings>(
		GetTransientPackage(), PVEClass, NAME_None, RF_Transient);

	// Foliage Distributor has flat properties (not nested in a struct),
	// so we copy by matching property names on the UObject directly.
	PGLDelegation::CopyMatchingObjectProperties(
		Settings, PVESettings, UPGLFoliageDistributorSettings::StaticClass());

	// --- Diagnostic: dump input state before delegation ---
	{
		const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
		UE_LOG(LogTemp, Warning, TEXT("[PGL FoliageDistributor] Input count: %d"), Inputs.Num());
		for (int32 i = 0; i < Inputs.Num(); ++i)
		{
			const UPCGData* Data = Inputs[i].Data;
			UE_LOG(LogTemp, Warning, TEXT("  [%d] Type: %s"), i, Data ? *Data->GetClass()->GetName() : TEXT("null"));

			if (const UPVMeshData* MeshData = Cast<UPVMeshData>(Data))
			{
				FManagedArrayCollection Collection = MeshData->GetCollection();
				TArray<FName> GroupNames = Collection.GroupNames();
				UE_LOG(LogTemp, Warning, TEXT("  [%d] Groups (%d):"), i, GroupNames.Num());
				for (const FName& GroupName : GroupNames)
				{
					UE_LOG(LogTemp, Warning, TEXT("    Group '%s': %d elements"),
						*GroupName.ToString(), Collection.NumElements(GroupName));
				}

				// Dump key per-branch inner-array sizes for Points group
				const FName PointGroup = PV::GroupNames::PointGroup;
				const FName BranchGroup = PV::GroupNames::BranchGroup;
				if (Collection.HasGroup(BranchGroup) && Collection.HasGroup(PointGroup))
				{
					int32 NumBranches = Collection.NumElements(BranchGroup);
					int32 NumPoints = Collection.NumElements(PointGroup);
					UE_LOG(LogTemp, Warning, TEXT("  [%d] Branches=%d, Points=%d"), i, NumBranches, NumPoints);

					// Check BranchPoints (TArray<int32> per branch — index ranges into PointGroup)
					if (Collection.HasAttribute(PV::AttributeNames::BranchPoints, BranchGroup))
					{
						const TManagedArray<TArray<int32>>& BranchPts =
							Collection.GetAttribute<TArray<int32>>(PV::AttributeNames::BranchPoints, BranchGroup);
						for (int32 b = 0; b < FMath::Min(NumBranches, 5); ++b)
						{
							UE_LOG(LogTemp, Warning, TEXT("    Branch[%d] BranchPoints count: %d"), b, BranchPts[b].Num());
							for (int32 p = 0; p < FMath::Min(BranchPts[b].Num(), 3); ++p)
							{
								UE_LOG(LogTemp, Warning, TEXT("      BranchPoints[%d][%d] = %d (NumPoints=%d)"),
									b, p, BranchPts[b][p], NumPoints);
							}
						}
					}

					// Check budHormoneLevels — TArray<float> per point
					const FName BudHormone = FName(TEXT("budHormoneLevels"));
					if (Collection.HasAttribute(BudHormone, PointGroup))
					{
						const TManagedArray<TArray<float>>& Hormones =
							Collection.GetAttribute<TArray<float>>(BudHormone, PointGroup);
						for (int32 pt = 0; pt < FMath::Min(NumPoints, 5); ++pt)
						{
							UE_LOG(LogTemp, Warning, TEXT("    Point[%d] budHormoneLevels count: %d"), pt, Hormones[pt].Num());
						}
					}

					// Check budDevelopment — TArray<int32> per point
					const FName BudDev = FName(TEXT("budDevelopment"));
					if (Collection.HasAttribute(BudDev, PointGroup))
					{
						const TManagedArray<TArray<int32>>& BudDevArr =
							Collection.GetAttribute<TArray<int32>>(BudDev, PointGroup);
						for (int32 pt = 0; pt < FMath::Min(NumPoints, 5); ++pt)
						{
							UE_LOG(LogTemp, Warning, TEXT("    Point[%d] budDevelopment count: %d"), pt, BudDevArr[pt].Num());
						}
					}
				}
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("[PGL FoliageDistributor] Settings: OverrideDistribution=%d, InstanceSpacing=%.3f, EthyleneThreshold=%.3f"),
			Settings->OverrideDistribution, Settings->InstanceSpacing, Settings->EthyleneThreshold);
	}

	// --- Delegate to PVE element ---
	FPCGDataCollection DelegatedOutput;
	if (!PGLDelegation::DelegateToPVEElement(Context, PVESettings, DelegatedOutput))
	{
		PCGE_LOG(Error, GraphAndLog,
			LOCTEXT("DelegationFailed", "PGL Foliage Distributor: delegation to PVE element failed."));
		return true;
	}

	// --- Set output and propagate FrameMesh ---
	Context->OutputData = MoveTemp(DelegatedOutput);
	PGLDelegation::EnsureFrameMeshInOutput(
		Context->OutputData, CapturedFrameMeshPath, CapturedFrameMeshTransform);

	return true;
}

#undef LOCTEXT_NAMESPACE
