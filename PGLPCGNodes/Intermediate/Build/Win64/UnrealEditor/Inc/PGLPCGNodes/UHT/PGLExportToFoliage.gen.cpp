// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "PGLPCGNodes/Public/PGLExportToFoliage.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodePGLExportToFoliage() {}

// Begin Cross Module References
PCG_API UClass* Z_Construct_UClass_UPCGSettings();
PGLPCGNODES_API UClass* Z_Construct_UClass_UPGLExportToFoliageSettings();
PGLPCGNODES_API UClass* Z_Construct_UClass_UPGLExportToFoliageSettings_NoRegister();
UPackage* Z_Construct_UPackage__Script_PGLPCGNodes();
// End Cross Module References

// Begin Class UPGLExportToFoliageSettings
void UPGLExportToFoliageSettings::StaticRegisterNativesUPGLExportToFoliageSettings()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(UPGLExportToFoliageSettings);
UClass* Z_Construct_UClass_UPGLExportToFoliageSettings_NoRegister()
{
	return UPGLExportToFoliageSettings::StaticClass();
}
struct Z_Construct_UClass_UPGLExportToFoliageSettings_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "ClassGroupNames", "Procedural" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** PCG node that processes point data and adds foliage instances */" },
#endif
		{ "IncludePath", "PGLExportToFoliage.h" },
		{ "ModuleRelativePath", "Public/PGLExportToFoliage.h" },
		{ "ObjectInitializerConstructorDeclared", "" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "PCG node that processes point data and adds foliage instances" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_FoliageTypeAttributeName_MetaData[] = {
		{ "Category", "Settings" },
		{ "ModuleRelativePath", "Public/PGLExportToFoliage.h" },
		{ "PCG_Overridable", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_FoliageTypeAttributeName;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UPGLExportToFoliageSettings>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UPGLExportToFoliageSettings_Statics::NewProp_FoliageTypeAttributeName = { "FoliageTypeAttributeName", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UPGLExportToFoliageSettings, FoliageTypeAttributeName), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_FoliageTypeAttributeName_MetaData), NewProp_FoliageTypeAttributeName_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UPGLExportToFoliageSettings_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPGLExportToFoliageSettings_Statics::NewProp_FoliageTypeAttributeName,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UPGLExportToFoliageSettings_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_UPGLExportToFoliageSettings_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UPCGSettings,
	(UObject* (*)())Z_Construct_UPackage__Script_PGLPCGNodes,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UPGLExportToFoliageSettings_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UPGLExportToFoliageSettings_Statics::ClassParams = {
	&UPGLExportToFoliageSettings::StaticClass,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_UPGLExportToFoliageSettings_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_UPGLExportToFoliageSettings_Statics::PropPointers),
	0,
	0x000800A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UPGLExportToFoliageSettings_Statics::Class_MetaDataParams), Z_Construct_UClass_UPGLExportToFoliageSettings_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_UPGLExportToFoliageSettings()
{
	if (!Z_Registration_Info_UClass_UPGLExportToFoliageSettings.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UPGLExportToFoliageSettings.OuterSingleton, Z_Construct_UClass_UPGLExportToFoliageSettings_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UPGLExportToFoliageSettings.OuterSingleton;
}
template<> PGLPCGNODES_API UClass* StaticClass<UPGLExportToFoliageSettings>()
{
	return UPGLExportToFoliageSettings::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(UPGLExportToFoliageSettings);
UPGLExportToFoliageSettings::~UPGLExportToFoliageSettings() {}
// End Class UPGLExportToFoliageSettings

// Begin Registration
struct Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PCGExExampleProject_main_Plugins_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLExportToFoliage_h_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UPGLExportToFoliageSettings, UPGLExportToFoliageSettings::StaticClass, TEXT("UPGLExportToFoliageSettings"), &Z_Registration_Info_UClass_UPGLExportToFoliageSettings, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UPGLExportToFoliageSettings), 3196078624U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PCGExExampleProject_main_Plugins_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLExportToFoliage_h_249101939(TEXT("/Script/PGLPCGNodes"),
	Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PCGExExampleProject_main_Plugins_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLExportToFoliage_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PCGExExampleProject_main_Plugins_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLExportToFoliage_h_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
