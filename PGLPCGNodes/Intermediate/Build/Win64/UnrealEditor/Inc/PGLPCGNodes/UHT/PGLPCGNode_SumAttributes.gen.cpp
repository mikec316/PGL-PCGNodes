// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "PGLPCGNodes/Public/PGLPCGNode_SumAttributes.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodePGLPCGNode_SumAttributes() {}

// Begin Cross Module References
PCG_API UClass* Z_Construct_UClass_UPCGSettings();
PGLPCGNODES_API UClass* Z_Construct_UClass_UPGLSumAttributesNodeSettings();
PGLPCGNODES_API UClass* Z_Construct_UClass_UPGLSumAttributesNodeSettings_NoRegister();
UPackage* Z_Construct_UPackage__Script_PGLPCGNodes();
// End Cross Module References

// Begin Class UPGLSumAttributesNodeSettings
void UPGLSumAttributesNodeSettings::StaticRegisterNativesUPGLSumAttributesNodeSettings()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(UPGLSumAttributesNodeSettings);
UClass* Z_Construct_UClass_UPGLSumAttributesNodeSettings_NoRegister()
{
	return UPGLSumAttributesNodeSettings::StaticClass();
}
struct Z_Construct_UClass_UPGLSumAttributesNodeSettings_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "ClassGroupNames", "Procedural" },
		{ "IncludePath", "PGLPCGNode_SumAttributes.h" },
		{ "ModuleRelativePath", "Public/PGLPCGNode_SumAttributes.h" },
		{ "ObjectInitializerConstructorDeclared", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_OutputAttribute_MetaData[] = {
		{ "Category", "Settings" },
		{ "ModuleRelativePath", "Public/PGLPCGNode_SumAttributes.h" },
		{ "PCG_Overridable", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_AttributeList_MetaData[] = {
		{ "Category", "Settings" },
		{ "ModuleRelativePath", "Public/PGLPCGNode_SumAttributes.h" },
		{ "PCG_Overridable", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_OutputAttribute;
	static const UECodeGen_Private::FStrPropertyParams NewProp_AttributeList;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UPGLSumAttributesNodeSettings>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UPGLSumAttributesNodeSettings_Statics::NewProp_OutputAttribute = { "OutputAttribute", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UPGLSumAttributesNodeSettings, OutputAttribute), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_OutputAttribute_MetaData), NewProp_OutputAttribute_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UPGLSumAttributesNodeSettings_Statics::NewProp_AttributeList = { "AttributeList", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UPGLSumAttributesNodeSettings, AttributeList), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_AttributeList_MetaData), NewProp_AttributeList_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UPGLSumAttributesNodeSettings_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPGLSumAttributesNodeSettings_Statics::NewProp_OutputAttribute,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPGLSumAttributesNodeSettings_Statics::NewProp_AttributeList,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UPGLSumAttributesNodeSettings_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_UPGLSumAttributesNodeSettings_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UPCGSettings,
	(UObject* (*)())Z_Construct_UPackage__Script_PGLPCGNodes,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UPGLSumAttributesNodeSettings_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UPGLSumAttributesNodeSettings_Statics::ClassParams = {
	&UPGLSumAttributesNodeSettings::StaticClass,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_UPGLSumAttributesNodeSettings_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_UPGLSumAttributesNodeSettings_Statics::PropPointers),
	0,
	0x000000A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UPGLSumAttributesNodeSettings_Statics::Class_MetaDataParams), Z_Construct_UClass_UPGLSumAttributesNodeSettings_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_UPGLSumAttributesNodeSettings()
{
	if (!Z_Registration_Info_UClass_UPGLSumAttributesNodeSettings.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UPGLSumAttributesNodeSettings.OuterSingleton, Z_Construct_UClass_UPGLSumAttributesNodeSettings_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UPGLSumAttributesNodeSettings.OuterSingleton;
}
template<> PGLPCGNODES_API UClass* StaticClass<UPGLSumAttributesNodeSettings>()
{
	return UPGLSumAttributesNodeSettings::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(UPGLSumAttributesNodeSettings);
UPGLSumAttributesNodeSettings::~UPGLSumAttributesNodeSettings() {}
// End Class UPGLSumAttributesNodeSettings

// Begin Registration
struct Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLPCGNode_SumAttributes_h_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UPGLSumAttributesNodeSettings, UPGLSumAttributesNodeSettings::StaticClass, TEXT("UPGLSumAttributesNodeSettings"), &Z_Registration_Info_UClass_UPGLSumAttributesNodeSettings, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UPGLSumAttributesNodeSettings), 2696083373U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLPCGNode_SumAttributes_h_2647343651(TEXT("/Script/PGLPCGNodes"),
	Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLPCGNode_SumAttributes_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLPCGNode_SumAttributes_h_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
