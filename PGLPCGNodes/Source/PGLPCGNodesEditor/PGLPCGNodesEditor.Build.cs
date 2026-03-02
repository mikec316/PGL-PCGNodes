// Copyright by Procgen Labs Ltd. All Rights Reserved.

using UnrealBuildTool;

public class PGLPCGNodesEditor : ModuleRules
{
	public PGLPCGNodesEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"StaticMeshDescription",
				"MeshDescription",
				"Engine",
				"PCG",
				"Foliage",
				"Landscape",
				"ZoneGraph",
				"UnrealEd",
                "Voxel",
				"VoxelCore",
				"VoxelGraph",
				"EditorScriptingUtilities",
				"ScriptableEditorWidgets",
				"InteractiveToolsFramework",
				"ModelingComponents",
				"ScriptableToolsFramework",
				"EditorScriptableToolsFramework",
				"PropertyEditor",      // For IDetailCustomization
				"InputCore"            // For input handling
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"Voronoi",
				"Foliage",
				"PCG",
				"ZoneGraph",
				"PGLPCGNodes",
				"PCGEditor",            // FPCGEditor, UPCGEditorGraphSchema, SPCGEditorViewport
				"AssetDefinition",      // UAssetDefinitionDefault
				"AssetTools",           // UFactory infrastructure
				"ToolMenus",            // Toolbar registration
				"GraphEditor",          // SGraphEditor
				"AdvancedPreviewScene", // Viewport preview scene
				"RenderCore",           // FPrimitiveSceneProxy for PGLLineBatchComponent
				"ProceduralVegetation", // UPVData, UPVBaseSettings, PVE facades
				"AssetRegistry",        // PCGSaveTextureToResourceSettings (migrated from runtime)
				"GeometryCore",         // FDynamicMesh3, FDynamicMeshAttributeSet
				"GeometryFramework",    // UDynamicMesh
				"GeometryScriptingCore", // CopyMeshToStaticMesh, CopyMeshToSkeletalMesh
				"MeshConversion",       // FMeshDescriptionToDynamicMesh
				"PlanarCut",            // ConvertGeometryCollectionToDynamicMesh
				"GeometryCollectionEngine", // FGeometryCollection
				"Chaos",                // FManagedArrayCollection, physics assets
				"SkeletalMeshDescription" // Skeletal mesh export support
				// ... add private dependencies that you statically link with here ...
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
