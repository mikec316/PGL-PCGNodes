// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "PGLCreateTaggedSpline.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef PGLPCGNODES_PGLCreateTaggedSpline_generated_h
#error "PGLCreateTaggedSpline.generated.h already included, missing '#pragma once' in PGLCreateTaggedSpline.h"
#endif
#define PGLPCGNODES_PGLCreateTaggedSpline_generated_h

<<<<<<< Updated upstream
#define FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLCreateTaggedSpline_h_23_INCLASS_NO_PURE_DECLS \
=======
#define FID_Users_chapp_OneDrive_Desktop_MikeCurrentProject_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLCreateTaggedSpline_h_23_INCLASS_NO_PURE_DECLS \
>>>>>>> Stashed changes
private: \
	static void StaticRegisterNativesUPGLCreateTaggedSplineSettings(); \
	friend struct Z_Construct_UClass_UPGLCreateTaggedSplineSettings_Statics; \
public: \
	DECLARE_CLASS(UPGLCreateTaggedSplineSettings, UPCGSettings, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/PGLPCGNodes"), PGLPCGNODES_API) \
	DECLARE_SERIALIZER(UPGLCreateTaggedSplineSettings)


<<<<<<< Updated upstream
#define FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLCreateTaggedSpline_h_23_ENHANCED_CONSTRUCTORS \
=======
#define FID_Users_chapp_OneDrive_Desktop_MikeCurrentProject_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLCreateTaggedSpline_h_23_ENHANCED_CONSTRUCTORS \
>>>>>>> Stashed changes
private: \
	/** Private move- and copy-constructors, should never be used */ \
	UPGLCreateTaggedSplineSettings(UPGLCreateTaggedSplineSettings&&); \
	UPGLCreateTaggedSplineSettings(const UPGLCreateTaggedSplineSettings&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(PGLPCGNODES_API, UPGLCreateTaggedSplineSettings); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UPGLCreateTaggedSplineSettings); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UPGLCreateTaggedSplineSettings) \
	PGLPCGNODES_API virtual ~UPGLCreateTaggedSplineSettings();


<<<<<<< Updated upstream
#define FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLCreateTaggedSpline_h_20_PROLOG
#define FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLCreateTaggedSpline_h_23_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLCreateTaggedSpline_h_23_INCLASS_NO_PURE_DECLS \
	FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLCreateTaggedSpline_h_23_ENHANCED_CONSTRUCTORS \
=======
#define FID_Users_chapp_OneDrive_Desktop_MikeCurrentProject_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLCreateTaggedSpline_h_20_PROLOG
#define FID_Users_chapp_OneDrive_Desktop_MikeCurrentProject_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLCreateTaggedSpline_h_23_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_Users_chapp_OneDrive_Desktop_MikeCurrentProject_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLCreateTaggedSpline_h_23_INCLASS_NO_PURE_DECLS \
	FID_Users_chapp_OneDrive_Desktop_MikeCurrentProject_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLCreateTaggedSpline_h_23_ENHANCED_CONSTRUCTORS \
>>>>>>> Stashed changes
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> PGLPCGNODES_API UClass* StaticClass<class UPGLCreateTaggedSplineSettings>();

#undef CURRENT_FILE_ID
<<<<<<< Updated upstream
#define CURRENT_FILE_ID FID_Users_chapp_OneDrive_Documents_Unreal_Projects_PGL_Test1_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLCreateTaggedSpline_h
=======
#define CURRENT_FILE_ID FID_Users_chapp_OneDrive_Desktop_MikeCurrentProject_Plugins_PGL_PCGNodes_PGLPCGNodes_Source_PGLPCGNodes_Public_PGLCreateTaggedSpline_h
>>>>>>> Stashed changes


#define FOREACH_ENUM_EPGLCREATETAGGEDSPLINEMODE(op) \
	op(EPGLCreateTaggedSplineMode::CreateDataOnly) \
	op(EPGLCreateTaggedSplineMode::CreateComponent) \
	op(EPGLCreateTaggedSplineMode::CreateNewActor) 

enum class EPGLCreateTaggedSplineMode : uint8;
template<> struct TIsUEnumClass<EPGLCreateTaggedSplineMode> { enum { Value = true }; };
template<> PGLPCGNODES_API UEnum* StaticEnum<EPGLCreateTaggedSplineMode>();

PRAGMA_ENABLE_DEPRECATION_WARNINGS
