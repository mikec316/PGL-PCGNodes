// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "PGLPCGNodes/Public/PGLPCGAssetExporter.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodePGLPCGAssetExporter() {}

// Begin Cross Module References
PCG_API UClass* Z_Construct_UClass_UPCGAssetExporter();
PGLPCGNODES_API UClass* Z_Construct_UClass_UPGLPCGAssetExporter();
PGLPCGNODES_API UClass* Z_Construct_UClass_UPGLPCGAssetExporter_NoRegister();
UPackage* Z_Construct_UPackage__Script_PGLPCGNodes();
// End Cross Module References

// Begin Class UPGLPCGAssetExporter
void UPGLPCGAssetExporter::StaticRegisterNativesUPGLPCGAssetExporter()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(UPGLPCGAssetExporter);
UClass* Z_Construct_UClass_UPGLPCGAssetExporter_NoRegister()
{
	return UPGLPCGAssetExporter::StaticClass();
}
struct Z_Construct_UClass_UPGLPCGAssetExporter_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "IncludePath", "PGLPCGAssetExporter.h" },
		{ "ModuleRelativePath", "Public/PGLPCGAssetExporter.h" },
	};
#endif // WITH_METADATA
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UPGLPCGAssetExporter>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
UObject* (*const Z_Construct_UClass_UPGLPCGAssetExporter_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UPCGAssetExporter,
	(UObject* (*)())Z_Construct_UPackage__Script_PGLPCGNodes,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UPGLPCGAssetExporter_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UPGLPCGAssetExporter_Statics::ClassParams = {
	&UPGLPCGAssetExporter::StaticClass,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	nullptr,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	0,
	0,
	0x000000A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UPGLPCGAssetExporter_Statics::Class_MetaDataParams), Z_Construct_UClass_UPGLPCGAssetExporter_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_UPGLPCGAssetExporter()
{
	if (!Z_Registration_Info_UClass_UPGLPCGAssetExporter.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UPGLPCGAssetExporter.OuterSingleton, Z_Construct_UClass_UPGLPCGAssetExporter_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UPGLPCGAssetExporter.OuterSingleton;
}
template<> PGLPCGNODES_API UClass* StaticClass<UPGLPCGAssetExporter>()
{
	return UPGLPCGAssetExporter::StaticClass();
}
UPGLPCGAssetExporter::UPGLPCGAssetExporter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR(UPGLPCGAssetExporter);
UPGLPCGAssetExporter::~UPGLPCGAssetExporter() {}
// End Class UPGLPCGAssetExporter

// Begin Registration
struct Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLPCGAssetExporter_h_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UPGLPCGAssetExporter, UPGLPCGAssetExporter::StaticClass, TEXT("UPGLPCGAssetExporter"), &Z_Registration_Info_UClass_UPGLPCGAssetExporter, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UPGLPCGAssetExporter), 3903457052U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLPCGAssetExporter_h_1369575240(TEXT("/Script/PGLPCGNodes"),
	Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLPCGAssetExporter_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLPCGAssetExporter_h_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
