// Copyright by Procgen Labs Ltd. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class PGLPCGNodes : ModuleRules
{
	public PGLPCGNodes(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Access ProceduralVegetation private headers (Facades/, Implementations/)
		PrivateIncludePaths.Add(Path.Combine(GetModuleDirectory("ProceduralVegetation"), "Private"));

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"PCG",
				"ProceduralVegetation",
				"Chaos",
				"GeometryCollectionEngine",
				// Public headers reference Voxel types (EVoxelSculptMode, AVoxelSculptVolume,
				// FVoxelMetadataOverrides, FVoxelFuture) — must be public so dependents can compile.
				"Voxel",
				"VoxelCore",
				// PGLHarvestStateFilter.h includes Harvest/PGLHarvestTypes.h (snapshot + key utils),
				// which pulls GameplayTagContainer.h — both must be public so dependents can compile.
				// (PGLExtended was a private dep for the collection types; promoted, not added.)
				"PGLExtended",
				"GameplayTags",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Slate",
				"SlateCore",
				"Foliage",
				"Landscape",
				"StaticMeshDescription",
				"MeshDescription",
				"Voronoi",
				"PCGExtendedToolkit",
			"PCGExCore",
			"PCGExCollections",
			"PCGExProperties",
				"VoxelGraph",
				"VoxelPCG",
				"UMG",
				"InputCore",
				"Water",
			}
			);
	}
}
