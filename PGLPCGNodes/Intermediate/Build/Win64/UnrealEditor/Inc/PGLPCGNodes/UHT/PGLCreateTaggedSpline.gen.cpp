// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "PGLPCGNodes/Public/PGLCreateTaggedSpline.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodePGLCreateTaggedSpline() {}

// Begin Cross Module References
ENGINE_API UClass* Z_Construct_UClass_AActor_NoRegister();
PCG_API UClass* Z_Construct_UClass_UPCGSettings();
PCG_API UEnum* Z_Construct_UEnum_PCG_EPCGAttachOptions();
PGLPCGNODES_API UClass* Z_Construct_UClass_UPGLCreateTaggedSplineSettings();
PGLPCGNODES_API UClass* Z_Construct_UClass_UPGLCreateTaggedSplineSettings_NoRegister();
PGLPCGNODES_API UEnum* Z_Construct_UEnum_PGLPCGNodes_EPGLCreateTaggedSplineMode();
UPackage* Z_Construct_UPackage__Script_PGLPCGNodes();
// End Cross Module References

// Begin Enum EPGLCreateTaggedSplineMode
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EPGLCreateTaggedSplineMode;
static UEnum* EPGLCreateTaggedSplineMode_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EPGLCreateTaggedSplineMode.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EPGLCreateTaggedSplineMode.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_PGLPCGNodes_EPGLCreateTaggedSplineMode, (UObject*)Z_Construct_UPackage__Script_PGLPCGNodes(), TEXT("EPGLCreateTaggedSplineMode"));
	}
	return Z_Registration_Info_UEnum_EPGLCreateTaggedSplineMode.OuterSingleton;
}
template<> PGLPCGNODES_API UEnum* StaticEnum<EPGLCreateTaggedSplineMode>()
{
	return EPGLCreateTaggedSplineMode_StaticEnum();
}
struct Z_Construct_UEnum_PGLPCGNodes_EPGLCreateTaggedSplineMode_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "CreateComponent.Name", "EPGLCreateTaggedSplineMode::CreateComponent" },
		{ "CreateDataOnly.Name", "EPGLCreateTaggedSplineMode::CreateDataOnly" },
		{ "CreateNewActor.Hidden", "" },
		{ "CreateNewActor.Name", "EPGLCreateTaggedSplineMode::CreateNewActor" },
		{ "ModuleRelativePath", "Public/PGLCreateTaggedSpline.h" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EPGLCreateTaggedSplineMode::CreateDataOnly", (int64)EPGLCreateTaggedSplineMode::CreateDataOnly },
		{ "EPGLCreateTaggedSplineMode::CreateComponent", (int64)EPGLCreateTaggedSplineMode::CreateComponent },
		{ "EPGLCreateTaggedSplineMode::CreateNewActor", (int64)EPGLCreateTaggedSplineMode::CreateNewActor },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
};
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_PGLPCGNodes_EPGLCreateTaggedSplineMode_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_PGLPCGNodes,
	nullptr,
	"EPGLCreateTaggedSplineMode",
	"EPGLCreateTaggedSplineMode",
	Z_Construct_UEnum_PGLPCGNodes_EPGLCreateTaggedSplineMode_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_PGLPCGNodes_EPGLCreateTaggedSplineMode_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_PGLPCGNodes_EPGLCreateTaggedSplineMode_Statics::Enum_MetaDataParams), Z_Construct_UEnum_PGLPCGNodes_EPGLCreateTaggedSplineMode_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_PGLPCGNodes_EPGLCreateTaggedSplineMode()
{
	if (!Z_Registration_Info_UEnum_EPGLCreateTaggedSplineMode.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EPGLCreateTaggedSplineMode.InnerSingleton, Z_Construct_UEnum_PGLPCGNodes_EPGLCreateTaggedSplineMode_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EPGLCreateTaggedSplineMode.InnerSingleton;
}
// End Enum EPGLCreateTaggedSplineMode

// Begin Class UPGLCreateTaggedSplineSettings
void UPGLCreateTaggedSplineSettings::StaticRegisterNativesUPGLCreateTaggedSplineSettings()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(UPGLCreateTaggedSplineSettings);
UClass* Z_Construct_UClass_UPGLCreateTaggedSplineSettings_NoRegister()
{
	return UPGLCreateTaggedSplineSettings::StaticClass();
}
struct Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "ClassGroupNames", "Procedural" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** PCG node that creates a spline presentation from the input points data, with optional tangents */" },
#endif
		{ "IncludePath", "PGLCreateTaggedSpline.h" },
		{ "ModuleRelativePath", "Public/PGLCreateTaggedSpline.h" },
		{ "ObjectInitializerConstructorDeclared", "" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "PCG node that creates a spline presentation from the input points data, with optional tangents" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Mode_MetaData[] = {
		{ "Category", "Settings" },
		{ "ModuleRelativePath", "Public/PGLCreateTaggedSpline.h" },
		{ "PCG_Overridable", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bClosedLoop_MetaData[] = {
		{ "Category", "Settings" },
		{ "ModuleRelativePath", "Public/PGLCreateTaggedSpline.h" },
		{ "PCG_Overridable", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bLinear_MetaData[] = {
		{ "Category", "Settings" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Controls whether the segment between control points is a curve (when false) or a straight line (when true).\n" },
#endif
		{ "ModuleRelativePath", "Public/PGLCreateTaggedSpline.h" },
		{ "PCG_Overridable", "" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Controls whether the segment between control points is a curve (when false) or a straight line (when true)." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bApplyCustomTangents_MetaData[] = {
		{ "Category", "Settings" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** Allow to specify custom tangents for each point, as an attribute. Can't be set if the spline is linear. */" },
#endif
		{ "EditCondition", "!bLinear" },
		{ "ModuleRelativePath", "Public/PGLCreateTaggedSpline.h" },
		{ "PCG_Overridable", "" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Allow to specify custom tangents for each point, as an attribute. Can't be set if the spline is linear." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ArriveTangentAttribute_MetaData[] = {
		{ "Category", "Settings" },
		{ "EditCondition", "!bLinear && bApplyCustomTangents" },
		{ "ModuleRelativePath", "Public/PGLCreateTaggedSpline.h" },
		{ "PCG_Overridable", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_LeaveTangentAttribute_MetaData[] = {
		{ "Category", "Settings" },
		{ "EditCondition", "!bLinear && bApplyCustomTangents" },
		{ "ModuleRelativePath", "Public/PGLCreateTaggedSpline.h" },
		{ "PCG_Overridable", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_TargetActor_MetaData[] = {
		{ "ModuleRelativePath", "Public/PGLCreateTaggedSpline.h" },
		{ "PCG_Overridable", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_AttachOptions_MetaData[] = {
		{ "Category", "Settings" },
		{ "EditCondition", "Mode==EPGLCreateTaggedSplineMode::CreateNewActor" },
		{ "EditConditionHides", "" },
		{ "ModuleRelativePath", "Public/PGLCreateTaggedSpline.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_PostProcessFunctionNames_MetaData[] = {
		{ "Category", "Settings" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** Specify a list of functions to be called on the target actor after spline creation. Functions need to be parameter-less and with \"CallInEditor\" flag enabled. */" },
#endif
		{ "ModuleRelativePath", "Public/PGLCreateTaggedSpline.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Specify a list of functions to be called on the target actor after spline creation. Functions need to be parameter-less and with \"CallInEditor\" flag enabled." },
#endif
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FBytePropertyParams NewProp_Mode_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_Mode;
	static void NewProp_bClosedLoop_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_bClosedLoop;
	static void NewProp_bLinear_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_bLinear;
	static void NewProp_bApplyCustomTangents_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_bApplyCustomTangents;
	static const UECodeGen_Private::FNamePropertyParams NewProp_ArriveTangentAttribute;
	static const UECodeGen_Private::FNamePropertyParams NewProp_LeaveTangentAttribute;
	static const UECodeGen_Private::FSoftObjectPropertyParams NewProp_TargetActor;
	static const UECodeGen_Private::FUInt32PropertyParams NewProp_AttachOptions_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_AttachOptions;
	static const UECodeGen_Private::FNamePropertyParams NewProp_PostProcessFunctionNames_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_PostProcessFunctionNames;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UPGLCreateTaggedSplineSettings>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FBytePropertyParams Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_Mode_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_Mode = { "Mode", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UPGLCreateTaggedSplineSettings, Mode), Z_Construct_UEnum_PGLPCGNodes_EPGLCreateTaggedSplineMode, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Mode_MetaData), NewProp_Mode_MetaData) }; // 1334781496
void Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_bClosedLoop_SetBit(void* Obj)
{
	((UPGLCreateTaggedSplineSettings*)Obj)->bClosedLoop = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_bClosedLoop = { "bClosedLoop", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UPGLCreateTaggedSplineSettings), &Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_bClosedLoop_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bClosedLoop_MetaData), NewProp_bClosedLoop_MetaData) };
void Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_bLinear_SetBit(void* Obj)
{
	((UPGLCreateTaggedSplineSettings*)Obj)->bLinear = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_bLinear = { "bLinear", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UPGLCreateTaggedSplineSettings), &Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_bLinear_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bLinear_MetaData), NewProp_bLinear_MetaData) };
void Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_bApplyCustomTangents_SetBit(void* Obj)
{
	((UPGLCreateTaggedSplineSettings*)Obj)->bApplyCustomTangents = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_bApplyCustomTangents = { "bApplyCustomTangents", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UPGLCreateTaggedSplineSettings), &Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_bApplyCustomTangents_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bApplyCustomTangents_MetaData), NewProp_bApplyCustomTangents_MetaData) };
const UECodeGen_Private::FNamePropertyParams Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_ArriveTangentAttribute = { "ArriveTangentAttribute", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Name, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UPGLCreateTaggedSplineSettings, ArriveTangentAttribute), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ArriveTangentAttribute_MetaData), NewProp_ArriveTangentAttribute_MetaData) };
const UECodeGen_Private::FNamePropertyParams Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_LeaveTangentAttribute = { "LeaveTangentAttribute", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Name, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UPGLCreateTaggedSplineSettings, LeaveTangentAttribute), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_LeaveTangentAttribute_MetaData), NewProp_LeaveTangentAttribute_MetaData) };
const UECodeGen_Private::FSoftObjectPropertyParams Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_TargetActor = { "TargetActor", nullptr, (EPropertyFlags)0x0014000000000000, UECodeGen_Private::EPropertyGenFlags::SoftObject, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UPGLCreateTaggedSplineSettings, TargetActor), Z_Construct_UClass_AActor_NoRegister, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_TargetActor_MetaData), NewProp_TargetActor_MetaData) };
const UECodeGen_Private::FUInt32PropertyParams Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_AttachOptions_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::UInt32, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_AttachOptions = { "AttachOptions", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UPGLCreateTaggedSplineSettings, AttachOptions), Z_Construct_UEnum_PCG_EPCGAttachOptions, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_AttachOptions_MetaData), NewProp_AttachOptions_MetaData) }; // 3528313874
const UECodeGen_Private::FNamePropertyParams Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_PostProcessFunctionNames_Inner = { "PostProcessFunctionNames", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Name, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_PostProcessFunctionNames = { "PostProcessFunctionNames", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UPGLCreateTaggedSplineSettings, PostProcessFunctionNames), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_PostProcessFunctionNames_MetaData), NewProp_PostProcessFunctionNames_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_Mode_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_Mode,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_bClosedLoop,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_bLinear,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_bApplyCustomTangents,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_ArriveTangentAttribute,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_LeaveTangentAttribute,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_TargetActor,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_AttachOptions_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_AttachOptions,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_PostProcessFunctionNames_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::NewProp_PostProcessFunctionNames,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UPCGSettings,
	(UObject* (*)())Z_Construct_UPackage__Script_PGLPCGNodes,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::ClassParams = {
	&UPGLCreateTaggedSplineSettings::StaticClass,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::PropPointers),
	0,
	0x000800A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::Class_MetaDataParams), Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_UPGLCreateTaggedSplineSettings()
{
	if (!Z_Registration_Info_UClass_UPGLCreateTaggedSplineSettings.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UPGLCreateTaggedSplineSettings.OuterSingleton, Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UPGLCreateTaggedSplineSettings.OuterSingleton;
}
template<> PGLPCGNODES_API UClass* StaticClass<UPGLCreateTaggedSplineSettings>()
{
	return UPGLCreateTaggedSplineSettings::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(UPGLCreateTaggedSplineSettings);
UPGLCreateTaggedSplineSettings::~UPGLCreateTaggedSplineSettings() {}
// End Class UPGLCreateTaggedSplineSettings

// Begin Registration
struct Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLCreateTaggedSpline_h_Statics
{
	static constexpr FEnumRegisterCompiledInInfo EnumInfo[] = {
		{ EPGLCreateTaggedSplineMode_StaticEnum, TEXT("EPGLCreateTaggedSplineMode"), &Z_Registration_Info_UEnum_EPGLCreateTaggedSplineMode, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 1334781496U) },
	};
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UPGLCreateTaggedSplineSettings, UPGLCreateTaggedSplineSettings::StaticClass, TEXT("UPGLCreateTaggedSplineSettings"), &Z_Registration_Info_UClass_UPGLCreateTaggedSplineSettings, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UPGLCreateTaggedSplineSettings), 66408957U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLCreateTaggedSpline_h_2660690625(TEXT("/Script/PGLPCGNodes"),
	Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLCreateTaggedSpline_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLCreateTaggedSpline_h_Statics::ClassInfo),
	nullptr, 0,
	Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLCreateTaggedSpline_h_Statics::EnumInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLCreateTaggedSpline_h_Statics::EnumInfo));
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
