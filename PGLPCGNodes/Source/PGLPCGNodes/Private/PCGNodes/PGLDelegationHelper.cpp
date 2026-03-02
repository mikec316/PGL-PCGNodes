// Copyright by Procgen Labs Ltd. All Rights Reserved.

#include "PCGNodes/PGLDelegationHelper.h"

#include "PCGContext.h"
#include "PCGElement.h"
#include "PCGNode.h"
#include "PCGSettings.h"
#include "DataTypes/PVData.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "UObject/FieldIterator.h"
#include "UObject/UnrealType.h"

// =============================================================================
//  PGLDelegation
// =============================================================================

void PGLDelegation::CopyMatchingStructProperties(
	const void* SrcData, const UStruct* SrcStruct,
	void* DstData,       const UStruct* DstStruct)
{
	for (TFieldIterator<FProperty> It(SrcStruct); It; ++It)
	{
		FProperty* SrcProp = *It;
		FProperty* DstProp = DstStruct->FindPropertyByName(SrcProp->GetFName());
		if (DstProp && SrcProp->SameType(DstProp))
		{
			const void* SrcAddr = SrcProp->ContainerPtrToValuePtr<void>(SrcData);
			void* DstAddr       = DstProp->ContainerPtrToValuePtr<void>(DstData);
			SrcProp->CopyCompleteValue(DstAddr, SrcAddr);
		}
	}
}

void PGLDelegation::CopyMatchingObjectProperties(
	const UObject* Src, UObject* Dst,
	const UClass* SrcDeclaredClass)
{
	const UClass* DstClass = Dst->GetClass();

	for (TFieldIterator<FProperty> It(SrcDeclaredClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		FProperty* SrcProp = *It;
		FProperty* DstProp = DstClass->FindPropertyByName(SrcProp->GetFName());
		if (DstProp && SrcProp->SameType(DstProp))
		{
			const void* SrcAddr = SrcProp->ContainerPtrToValuePtr<void>(Src);
			void* DstAddr       = DstProp->ContainerPtrToValuePtr<void>(Dst);
			SrcProp->CopyCompleteValue(DstAddr, SrcAddr);
		}
	}
}

void PGLDelegation::CaptureFrameMeshFromInput(
	const FPCGDataCollection& InputData,
	FString& OutPath, FTransform& OutTransform)
{
	OutPath = FString();
	OutTransform = FTransform::Identity;

	static const FName DetailsGroup(TEXT("Details"));
	static const FName FrameMeshPathName(TEXT("FrameMeshPath"));
	static const FName FrameMeshTransformName(TEXT("FrameMeshTransform"));

	const TArray<FPCGTaggedData>& Inputs =
		InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	for (const FPCGTaggedData& Tagged : Inputs)
	{
		const UPVData* PVData = Cast<UPVData>(Tagged.Data);
		if (!PVData) continue;

		const FManagedArrayCollection& Col = PVData->GetCollection();
		const TManagedArray<FString>* PathAttr =
			Col.FindAttribute<FString>(FrameMeshPathName, DetailsGroup);
		if (PathAttr && PathAttr->Num() > 0 && !(*PathAttr)[0].IsEmpty())
		{
			OutPath = (*PathAttr)[0];
			const TManagedArray<FTransform>* XfAttr =
				Col.FindAttribute<FTransform>(FrameMeshTransformName, DetailsGroup);
			if (XfAttr && XfAttr->Num() > 0)
			{
				OutTransform = (*XfAttr)[0];
			}
			break;
		}
	}
}

void PGLDelegation::EnsureFrameMeshInOutput(
	FPCGDataCollection& OutputData,
	const FString& Path, const FTransform& Transform)
{
	if (Path.IsEmpty()) return;

	static const FName DetailsGroup(TEXT("Details"));
	static const FName FrameMeshPathName(TEXT("FrameMeshPath"));
	static const FName FrameMeshTransformName(TEXT("FrameMeshTransform"));

	for (FPCGTaggedData& Output : OutputData.TaggedData)
	{
		// Output.Data is TObjectPtr<const UPCGData>; const_cast is safe here
		// because we own this output collection and need mutable access to
		// inject FrameMesh attributes into UPVData::GetCollection().
		UPVData* PVData = const_cast<UPVData*>(Cast<UPVData>(Output.Data));
		if (!PVData) continue;

		FManagedArrayCollection& Col = PVData->GetCollection();

		TManagedArray<FString>& FMP =
			Col.AddAttribute<FString>(FrameMeshPathName, DetailsGroup);
		if (FMP.Num() > 0) FMP[0] = Path;

		TManagedArray<FTransform>& FMT =
			Col.AddAttribute<FTransform>(FrameMeshTransformName, DetailsGroup);
		if (FMT.Num() > 0) FMT[0] = Transform;
	}
}

bool PGLDelegation::DelegateToPVEElement(
	FPCGContext* CallerContext,
	UPCGSettings* PVESettings,
	FPCGDataCollection& OutOutput)
{
	if (!PVESettings)
	{
		return false;
	}

	// 1. Get the PVE element from the settings (public accessor)
	FPCGElementPtr PVEElement = PVESettings->GetElement();
	if (!PVEElement)
	{
		return false;
	}

	// 2. Create a temporary node with the PVE settings
	UPCGNode* TempNode = NewObject<UPCGNode>(
		GetTransientPackage(), NAME_None, RF_Transient);
	TempNode->SetSettingsInterface(PVESettings, /*bUpdatePins=*/false);

	// 3. Initialize a sub-context using PCG's public API
	FPCGInitializeElementParams Params(
		&CallerContext->InputData,
		CallerContext->ExecutionSource,
		TempNode);

	FPCGContext* SubContext = PVEElement->Initialize(Params);
	if (!SubContext)
	{
		return false;
	}

	// 3b. Set up async state — matches PCGTestsCommon::InitializeTestContext pattern.
	// Initialize() leaves NumAvailableTasks=0; Execute() asserts it's non-zero.
	SubContext->InitializeSettings();
	SubContext->AsyncState.NumAvailableTasks = 1;

	// 4. Drive execution until done (PVE elements are synchronous)
	constexpr int32 MaxIterations = 100;
	for (int32 i = 0; i < MaxIterations; ++i)
	{
		if (PVEElement->Execute(SubContext))
		{
			break;
		}
	}

	// 5. Extract output
	OutOutput = MoveTemp(SubContext->OutputData);

	// 6. Cleanup
	FPCGContext::Release(SubContext);

	return true;
}
