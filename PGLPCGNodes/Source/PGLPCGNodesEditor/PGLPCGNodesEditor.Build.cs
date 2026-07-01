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
				System.IO.Path.Combine(GetModuleDirectory("VoxelGraph"), "Private"), // FVoxelGraphCompiler internals
				System.IO.Path.Combine(GetModuleDirectory("PCG"), "Internal"),       // IPCGNodeSourceTextProvider (needed by PCGComputeSource.h)
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
				"VoxelCoreEditor",
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
				"PGLExtended",          // For Sculpting/PGLVoxelBlueprintLibrary.h used by PGLPCGScriptableTool
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
				"SkeletalMeshDescription", // Skeletal mesh export support
				"ComputeFramework",       // UComputeSource base class for PCG Compute
				"ContentBrowser"          // UContentBrowserAssetContextMenuContext for VoxelToHLSL action
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
