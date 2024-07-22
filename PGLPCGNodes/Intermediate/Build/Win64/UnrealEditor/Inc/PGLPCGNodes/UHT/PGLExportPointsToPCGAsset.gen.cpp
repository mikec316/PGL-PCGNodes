// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "PGLPCGNodes/Public/PGLExportPointsToPCGAsset.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodePGLExportPointsToPCGAsset() {}

// Begin Cross Module References
PCG_API UClass* Z_Construct_UClass_UPCGSettings();
PGLPCGNODES_API UClass* Z_Construct_UClass_UPGLExportPointsToPCGAssetSettings();
PGLPCGNODES_API UClass* Z_Construct_UClass_UPGLExportPointsToPCGAssetSettings_NoRegister();
UPackage* Z_Construct_UPackage__Script_PGLPCGNodes();
// End Cross Module References

// Begin Class UPGLExportPointsToPCGAssetSettings
void UPGLExportPointsToPCGAssetSettings::StaticRegisterNativesUPGLExportPointsToPCGAssetSettings()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(UPGLExportPointsToPCGAssetSettings);
UClass* Z_Construct_UClass_UPGLExportPointsToPCGAssetSettings_NoRegister()
{
	return UPGLExportPointsToPCGAssetSettings::StaticClass();
}
struct Z_Construct_UClass_UPGLExportPointsToPCGAssetSettings_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "ClassGroupNames", "Procedural" },
		{ "IncludePath", "PGLExportPointsToPCGAsset.h" },
		{ "ModuleRelativePath", "Public/PGLExportPointsToPCGAsset.h" },
		{ "ObjectInitializerConstructorDeclared", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_AssetName_MetaData[] = {
		{ "Category", "Settings" },
		{ "ModuleRelativePath", "Public/PGLExportPointsToPCGAsset.h" },
		{ "PCG_Overridable", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_AssetPath_MetaData[] = {
		{ "Category", "Settings" },
		{ "ModuleRelativePath", "Public/PGLExportPointsToPCGAsset.h" },
		{ "PCG_Overridable", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_AssetName;
	static const UECodeGen_Private::FStrPropertyParams NewProp_AssetPath;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UPGLExportPointsToPCGAssetSettings>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UPGLExportPointsToPCGAssetSettings_Statics::NewProp_AssetName = { "AssetName", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UPGLExportPointsToPCGAssetSettings, AssetName), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_AssetName_MetaData), NewProp_AssetName_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UPGLExportPointsToPCGAssetSettings_Statics::NewProp_AssetPath = { "AssetPath", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UPGLExportPointsToPCGAssetSettings, AssetPath), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_AssetPath_MetaData), NewProp_AssetPath_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UPGLExportPointsToPCGAssetSettings_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPGLExportPointsToPCGAssetSettings_Statics::NewProp_AssetName,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPGLExportPointsToPCGAssetSettings_Statics::NewProp_AssetPath,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UPGLExportPointsToPCGAssetSettings_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_UPGLExportPointsToPCGAssetSettings_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UPCGSettings,
	(UObject* (*)())Z_Construct_UPackage__Script_PGLPCGNodes,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UPGLExportPointsToPCGAssetSettings_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UPGLExportPointsToPCGAssetSettings_Statics::ClassParams = {
	&UPGLExportPointsToPCGAssetSettings::StaticClass,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_UPGLExportPointsToPCGAssetSettings_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_UPGLExportPointsToPCGAssetSettings_Statics::PropPointers),
	0,
	0x000000A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UPGLExportPointsToPCGAssetSettings_Statics::Class_MetaDataParams), Z_Construct_UClass_UPGLExportPointsToPCGAssetSettings_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_UPGLExportPointsToPCGAssetSettings()
{
	if (!Z_Registration_Info_UClass_UPGLExportPointsToPCGAssetSettings.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UPGLExportPointsToPCGAssetSettings.OuterSingleton, Z_Construct_UClass_UPGLExportPointsToPCGAssetSettings_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UPGLExportPointsToPCGAssetSettings.OuterSingleton;
}
template<> PGLPCGNODES_API UClass* StaticClass<UPGLExportPointsToPCGAssetSettings>()
{
	return UPGLExportPointsToPCGAssetSettings::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(UPGLExportPointsToPCGAssetSettings);
UPGLExportPointsToPCGAssetSettings::~UPGLExportPointsToPCGAssetSettings() {}
// End Class UPGLExportPointsToPCGAssetSettings

// Begin Registration
<<<<<<< Updated upstream
struct Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLExportPointsToPCGAsset_h_Statics
=======
struct Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Desktop_MikeCurrentProject_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLExportPointsToPCGAsset_h_Statics
>>>>>>> Stashed changes
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UPGLExportPointsToPCGAssetSettings, UPGLExportPointsToPCGAssetSettings::StaticClass, TEXT("UPGLExportPointsToPCGAssetSettings"), &Z_Registration_Info_UClass_UPGLExportPointsToPCGAssetSettings, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UPGLExportPointsToPCGAssetSettings), 2026762454U) },
	};
};
<<<<<<< Updated upstream
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLExportPointsToPCGAsset_h_2474228816(TEXT("/Script/PGLPCGNodes"),
	Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLExportPointsToPCGAsset_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLExportPointsToPCGAsset_h_Statics::ClassInfo),
=======
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Desktop_MikeCurrentProject_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLExportPointsToPCGAsset_h_2474228816(TEXT("/Script/PGLPCGNodes"),
	Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Desktop_MikeCurrentProject_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLExportPointsToPCGAsset_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Desktop_MikeCurrentProject_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLExportPointsToPCGAsset_h_Statics::ClassInfo),
>>>>>>> Stashed changes
	nullptr, 0,
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
