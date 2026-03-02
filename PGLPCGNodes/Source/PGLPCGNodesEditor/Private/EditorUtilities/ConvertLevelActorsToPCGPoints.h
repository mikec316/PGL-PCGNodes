// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Sculpt/Height/VoxelSculptHeight.h"  // For AVoxelSculptHeight
#include "Sculpt/Height/VoxelSculptHeightAsset.h"
#include "ConvertLevelActorsToPCGPoints.generated.h"

// Forward declarations
class UPCGPointData;
class AActor;
struct FPCGTaggedData;

UCLASS()
class UConvertLevelActorsToPCGPoints : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "PCG|Utilities", meta = (WorldContext = "WorldContextObject"))
    static TArray<FPCGTaggedData> ConvertLevelActorsToPCGPoints(UObject* WorldContextObject);

    UFUNCTION(BlueprintCallable, Category = "Voxel|Sculpt")
    static void SetSculptAssetFromObject(AVoxelSculptHeight* SculptActor, UObject* AssetObject);
};