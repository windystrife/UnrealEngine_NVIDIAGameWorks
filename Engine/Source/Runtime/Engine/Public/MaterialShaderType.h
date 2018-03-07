// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialShader.h: Material shader definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "Engine/EngineTypes.h"

/** A macro to implement material shaders. */
#define IMPLEMENT_MATERIAL_SHADER_TYPE(TemplatePrefix,ShaderClass,SourceFilename,FunctionName,Frequency) \
	IMPLEMENT_SHADER_TYPE( \
		TemplatePrefix, \
		ShaderClass, \
		SourceFilename, \
		FunctionName, \
		Frequency \
		);

class FMaterial;
class FShaderCommonCompileJob;
class FShaderCompileJob;
class FUniformExpressionSet;
class FVertexFactoryType;

/** Converts an EMaterialShadingModel to a string description. */
extern FString GetShadingModelString(EMaterialShadingModel ShadingModel);

/** Converts an EBlendMode to a string description. */
extern FString GetBlendModeString(EBlendMode BlendMode);

/** Called for every material shader to update the appropriate stats. */
extern void UpdateMaterialShaderCompilingStats(const FMaterial* Material);

/**
 * Dump material stats for a given platform.
 * 
 * @param	Platform	Platform to dump stats for.
 */
extern ENGINE_API void DumpMaterialStats( EShaderPlatform Platform );

/**
 * A shader meta type for material-linked shaders.
 */
class FMaterialShaderType : public FShaderType
{
public:
	struct CompiledShaderInitializerType : FGlobalShaderType::CompiledShaderInitializerType
	{
		const FUniformExpressionSet& UniformExpressionSet;
		const FString DebugDescription;

		CompiledShaderInitializerType(
			FShaderType* InType,
			const FShaderCompilerOutput& CompilerOutput,
			FShaderResource* InResource,
			const FUniformExpressionSet& InUniformExpressionSet,
			const FSHAHash& InMaterialShaderMapHash,
			const FShaderPipelineType* InShaderPipeline,
			FVertexFactoryType* InVertexFactoryType,
			const FString& InDebugDescription
			)
		: FGlobalShaderType::CompiledShaderInitializerType(InType,CompilerOutput,InResource,InMaterialShaderMapHash,InShaderPipeline,InVertexFactoryType)
		, UniformExpressionSet(InUniformExpressionSet)
		, DebugDescription(InDebugDescription)
		{}
	};
	typedef FShader* (*ConstructCompiledType)(const CompiledShaderInitializerType&);
	typedef bool (*ShouldCacheType)(EShaderPlatform,const FMaterial*);
	typedef void (*ModifyCompilationEnvironmentType)(EShaderPlatform, const FMaterial*, FShaderCompilerEnvironment&);

	FMaterialShaderType(
		const TCHAR* InName,
		const TCHAR* InSourceFilename,
		const TCHAR* InFunctionName,
		uint32 InFrequency,
		ConstructSerializedType InConstructSerializedRef,
		ConstructCompiledType InConstructCompiledRef,
		ModifyCompilationEnvironmentType InModifyCompilationEnvironmentRef,
		ShouldCacheType InShouldCacheRef,
		GetStreamOutElementsType InGetStreamOutElementsRef
		):
		FShaderType(EShaderTypeForDynamicCast::Material, InName, InSourceFilename, InFunctionName, InFrequency, InConstructSerializedRef, InGetStreamOutElementsRef),
		ConstructCompiledRef(InConstructCompiledRef),
		ShouldCacheRef(InShouldCacheRef),
		ModifyCompilationEnvironmentRef(InModifyCompilationEnvironmentRef)
	{
		checkf(FPaths::GetExtension(InSourceFilename) == TEXT("usf"),
			TEXT("Incorrect virtual shader path extension for material shader '%s': Only .usf files should be compiled."),
			InSourceFilename);
	}

	/**
	 * Enqueues a compilation for a new shader of this type.
	 * @param Material - The material to link the shader with.
	 */
	class FShaderCompileJob* BeginCompileShader(
		uint32 ShaderMapId,
		const FMaterial* Material,
		FShaderCompilerEnvironment* MaterialEnvironment,
		const FShaderPipelineType* ShaderPipeline,
		EShaderPlatform Platform,
		TArray<FShaderCommonCompileJob*>& NewJobs
		);

	static void BeginCompileShaderPipeline(
		uint32 ShaderMapId,
		EShaderPlatform Platform,
		const FMaterial* Material,
		FShaderCompilerEnvironment* MaterialEnvironment,
		const FShaderPipelineType* ShaderPipeline,
		const TArray<FMaterialShaderType*>& ShaderStages,
		TArray<FShaderCommonCompileJob*>& NewJobs);

	/**
	 * Either creates a new instance of this type or returns an equivalent existing shader.
	 * @param Material - The material to link the shader with.
	 * @param CurrentJob - Compile job that was enqueued by BeginCompileShader.
	 */
	FShader* FinishCompileShader(
		const FUniformExpressionSet& UniformExpressionSet,
		const FSHAHash& MaterialShaderMapHash,
		const FShaderCompileJob& CurrentJob,
		const FShaderPipelineType* ShaderPipeline,
		const FString& InDebugDescription
		);

	/**
	 * Checks if the shader type should be cached for a particular platform and material.
	 * @param Platform - The platform to check.
	 * @param Material - The material to check.
	 * @return True if this shader type should be cached.
	 */
	bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material) const
	{
		return (*ShouldCacheRef)(Platform,Material);
	}

protected:

	/**
	 * Sets up the environment used to compile an instance of this shader type.
	 * @param Platform - Platform to compile for.
	 * @param Environment - The shader compile environment that the function modifies.
	 */
	void SetupCompileEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& Environment)
	{
		// Allow the shader type to modify its compile environment.
		(*ModifyCompilationEnvironmentRef)(Platform, Material, Environment);
	}

private:
	ConstructCompiledType ConstructCompiledRef;
	ShouldCacheType ShouldCacheRef;
	ModifyCompilationEnvironmentType ModifyCompilationEnvironmentRef;
};
