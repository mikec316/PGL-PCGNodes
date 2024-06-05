// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "PGLPCGNodes/Public/PCGCreateSplineMesh.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodePCGCreateSplineMesh() {}

// Begin Cross Module References
ENGINE_API UClass* Z_Construct_UClass_AActor_NoRegister();
PCG_API UClass* Z_Construct_UClass_UPCGSettings();
PCG_API UEnum* Z_Construct_UEnum_PCG_EPCGAttachOptions();
PGLPCGNODES_API UClass* Z_Construct_UClass_UPCGCreateSplineMeshSettings();
PGLPCGNODES_API UClass* Z_Construct_UClass_UPCGCreateSplineMeshSettings_NoRegister();
UPackage* Z_Construct_UPackage__Script_PGLPCGNodes();
// End Cross Module References

// Begin Class UPCGCreateSplineMeshSettings
void UPCGCreateSplineMeshSettings::StaticRegisterNativesUPCGCreateSplineMeshSettings()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(UPCGCreateSplineMeshSettings);
UClass* Z_Construct_UClass_UPCGCreateSplineMeshSettings_NoRegister()
{
	return UPCGCreateSplineMeshSettings::StaticClass();
}
struct Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "ClassGroupNames", "Procedural" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** PCG node that creates a spline presentation from the input points data, with optional tangents */" },
#endif
		{ "IncludePath", "PCGCreateSplineMesh.h" },
		{ "ModuleRelativePath", "Public/PCGCreateSplineMesh.h" },
		{ "ObjectInitializerConstructorDeclared", "" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "PCG node that creates a spline presentation from the input points data, with optional tangents" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_TargetActor_MetaData[] = {
		{ "ModuleRelativePath", "Public/PCGCreateSplineMesh.h" },
		{ "PCG_Overridable", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_AttachOptions_MetaData[] = {
		{ "Category", "Settings" },
		{ "ModuleRelativePath", "Public/PCGCreateSplineMesh.h" },
		{ "PCG_Overridable", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_PostProcessFunctionNames_MetaData[] = {
		{ "Category", "Settings" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** Specify a list of functions to be called on the target actor after spline creation. Functions need to be parameter-less and with \"CallInEditor\" flag enabled. */" },
#endif
		{ "ModuleRelativePath", "Public/PCGCreateSplineMesh.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Specify a list of functions to be called on the target actor after spline creation. Functions need to be parameter-less and with \"CallInEditor\" flag enabled." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_StartTangentAttributeName_MetaData[] = {
		{ "Category", "Settings" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// New attributes for spline positions and tangents\n" },
#endif
		{ "ModuleRelativePath", "Public/PCGCreateSplineMesh.h" },
		{ "PCG_Overridable", "" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "New attributes for spline positions and tangents" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_StaticMeshAttributeName_MetaData[] = {
		{ "Category", "Settings" },
		{ "ModuleRelativePath", "Public/PCGCreateSplineMesh.h" },
		{ "PCG_Overridable", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_StartScaleAttributeName_MetaData[] = {
		{ "Category", "Settings" },
		{ "ModuleRelativePath", "Public/PCGCreateSplineMesh.h" },
		{ "PCG_Overridable", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_StartRollAttributeName_MetaData[] = {
		{ "Category", "Settings" },
		{ "ModuleRelativePath", "Public/PCGCreateSplineMesh.h" },
		{ "PCG_Overridable", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FSoftObjectPropertyParams NewProp_TargetActor;
	static const UECodeGen_Private::FUInt32PropertyParams NewProp_AttachOptions_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_AttachOptions;
	static const UECodeGen_Private::FNamePropertyParams NewProp_PostProcessFunctionNames_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_PostProcessFunctionNames;
	static const UECodeGen_Private::FNamePropertyParams NewProp_StartTangentAttributeName;
	static const UECodeGen_Private::FNamePropertyParams NewProp_StaticMeshAttributeName;
	static const UECodeGen_Private::FNamePropertyParams NewProp_StartScaleAttributeName;
	static const UECodeGen_Private::FNamePropertyParams NewProp_StartRollAttributeName;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UPCGCreateSplineMeshSettings>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FSoftObjectPropertyParams Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::NewProp_TargetActor = { "TargetActor", nullptr, (EPropertyFlags)0x0014000000000000, UECodeGen_Private::EPropertyGenFlags::SoftObject, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UPCGCreateSplineMeshSettings, TargetActor), Z_Construct_UClass_AActor_NoRegister, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_TargetActor_MetaData), NewProp_TargetActor_MetaData) };
const UECodeGen_Private::FUInt32PropertyParams Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::NewProp_AttachOptions_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::UInt32, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::NewProp_AttachOptions = { "AttachOptions", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UPCGCreateSplineMeshSettings, AttachOptions), Z_Construct_UEnum_PCG_EPCGAttachOptions, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_AttachOptions_MetaData), NewProp_AttachOptions_MetaData) }; // 3528313874
const UECodeGen_Private::FNamePropertyParams Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::NewProp_PostProcessFunctionNames_Inner = { "PostProcessFunctionNames", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Name, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::NewProp_PostProcessFunctionNames = { "PostProcessFunctionNames", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UPCGCreateSplineMeshSettings, PostProcessFunctionNames), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_PostProcessFunctionNames_MetaData), NewProp_PostProcessFunctionNames_MetaData) };
const UECodeGen_Private::FNamePropertyParams Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::NewProp_StartTangentAttributeName = { "StartTangentAttributeName", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Name, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UPCGCreateSplineMeshSettings, StartTangentAttributeName), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_StartTangentAttributeName_MetaData), NewProp_StartTangentAttributeName_MetaData) };
const UECodeGen_Private::FNamePropertyParams Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::NewProp_StaticMeshAttributeName = { "StaticMeshAttributeName", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Name, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UPCGCreateSplineMeshSettings, StaticMeshAttributeName), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_StaticMeshAttributeName_MetaData), NewProp_StaticMeshAttributeName_MetaData) };
const UECodeGen_Private::FNamePropertyParams Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::NewProp_StartScaleAttributeName = { "StartScaleAttributeName", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Name, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UPCGCreateSplineMeshSettings, StartScaleAttributeName), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_StartScaleAttributeName_MetaData), NewProp_StartScaleAttributeName_MetaData) };
const UECodeGen_Private::FNamePropertyParams Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::NewProp_StartRollAttributeName = { "StartRollAttributeName", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Name, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UPCGCreateSplineMeshSettings, StartRollAttributeName), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_StartRollAttributeName_MetaData), NewProp_StartRollAttributeName_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::NewProp_TargetActor,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::NewProp_AttachOptions_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::NewProp_AttachOptions,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::NewProp_PostProcessFunctionNames_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::NewProp_PostProcessFunctionNames,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::NewProp_StartTangentAttributeName,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::NewProp_StaticMeshAttributeName,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::NewProp_StartScaleAttributeName,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::NewProp_StartRollAttributeName,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UPCGSettings,
	(UObject* (*)())Z_Construct_UPackage__Script_PGLPCGNodes,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::ClassParams = {
	&UPCGCreateSplineMeshSettings::StaticClass,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::PropPointers),
	0,
	0x000800A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::Class_MetaDataParams), Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_UPCGCreateSplineMeshSettings()
{
	if (!Z_Registration_Info_UClass_UPCGCreateSplineMeshSettings.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UPCGCreateSplineMeshSettings.OuterSingleton, Z_Construct_UClass_UPCGCreateSplineMeshSettings_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UPCGCreateSplineMeshSettings.OuterSingleton;
}
template<> PGLPCGNODES_API UClass* StaticClass<UPCGCreateSplineMeshSettings>()
{
	return UPCGCreateSplineMeshSettings::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(UPCGCreateSplineMeshSettings);
UPCGCreateSplineMeshSettings::~UPCGCreateSplineMeshSettings() {}
// End Class UPCGCreateSplineMeshSettings

// Begin Registration
struct Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PCGCreateSplineMesh_h_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UPCGCreateSplineMeshSettings, UPCGCreateSplineMeshSettings::StaticClass, TEXT("UPCGCreateSplineMeshSettings"), &Z_Registration_Info_UClass_UPCGCreateSplineMeshSettings, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UPCGCreateSplineMeshSettings), 2383136019U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PCGCreateSplineMesh_h_3903777909(TEXT("/Script/PGLPCGNodes"),
	Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PCGCreateSplineMesh_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PCGCreateSplineMesh_h_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
