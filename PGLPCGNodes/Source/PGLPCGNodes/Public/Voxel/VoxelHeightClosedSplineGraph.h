// Copyright by Procgen Labs Ltd. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelGraph.h"
#include "VoxelHeightClosedSplineGraph.generated.h"

UCLASS(BlueprintType, meta = (AssetSubMenu = "Graph"))
class PGLPCGNODES_API UVoxelHeightClosedSplineGraph : public UVoxelGraph
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FFactoryInfo GetFactoryInfo() override;
#endif
	virtual UScriptStruct* GetOutputNodeStruct() const override;
};
