// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GlobalShader.h: Shader manager definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Templates/ScopedPointer.h"
#include "Shader.h"
#include "UniquePtr.h"
#include "RHIResources.h"

class FShaderCommonCompileJob;
class FShaderCompileJob;

/** Used to identify the global shader map in compile queues. */
extern SHADERCORE_API const int32 GlobalShaderMapId;

/** Class that encapsulates logic to create a DDC key for the global shader map. */
class FGlobalShaderMapId
{
public:

	/** Create a global shader map Id for the given platform. */
	SHADERCORE_API FGlobalShaderMapId(EShaderPlatform Platform);

	/** Append to a string that will be used as a DDC key. */
	SHADERCORE_API void AppendKeyString(FString& KeyString) const;

private:
	/** Shader types that this shader map is dependent on and their stored state. */
	TArray<FShaderTypeDependency> ShaderTypeDependencies;

	/** Shader pipeline types that this shader map is dependent on and their stored state. */
	TArray<FShaderPipelineTypeDependency> ShaderPipelineTypeDependencies;
};

/**
 * A shader meta type for the simplest shaders; shaders which are not material or vertex factory linked.
 * There should only a single instance of each simple shader type.
 */
class FGlobalShaderType : public FShaderType
{
	friend class FGlobalShaderTypeCompiler;
public:

	typedef FShader::CompiledShaderInitializerType CompiledShaderInitializerType;
	typedef FShader* (*ConstructCompiledType)(const CompiledShaderInitializerType&);
	typedef bool (*ShouldCacheType)(EShaderPlatform);
	typedef void (*ModifyCompilationEnvironmentType)(EShaderPlatform, FShaderCompilerEnvironment&);

	FGlobalShaderType(
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
		FShaderType(EShaderTypeForDynamicCast::Global, InName, InSourceFilename, InFunctionName, InFrequency, InConstructSerializedRef, InGetStreamOutElementsRef),
		ConstructCompiledRef(InConstructCompiledRef),
		ShouldCacheRef(InShouldCacheRef),
		ModifyCompilationEnvironmentRef(InModifyCompilationEnvironmentRef)
	{
		checkf(FPaths::GetExtension(InSourceFilename) == TEXT("usf"),
			TEXT("Incorrect virtual shader path extension for global shader '%s': Only .usf files should be "
			     "compiled."),
			InSourceFilename);
	}

	/**
	 * Checks if the shader type should be cached for a particular platform.
	 * @param Platform - The platform to check.
	 * @return True if this shader type should be cached.
	 */
	bool ShouldCache(EShaderPlatform Platform) const
	{
		return (*ShouldCacheRef)(Platform);
	}

	/**
	 * Sets up the environment used to compile an instance of this shader type.
	 * @param Platform - Platform to compile for.
	 * @param Environment - The shader compile environment that the function modifies.
	 */
	void SetupCompileEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& Environment)
	{
		// Allow the shader type to modify its compile environment.
		(*ModifyCompilationEnvironmentRef)(Platform, Environment);
	}

private:
	ConstructCompiledType ConstructCompiledRef;
	ShouldCacheType ShouldCacheRef;
	ModifyCompilationEnvironmentType ModifyCompilationEnvironmentRef;
};

extern SHADERCORE_API TShaderMap<FGlobalShaderType>* GGlobalShaderMap[SP_NumPlatforms];

/**
 * FGlobalShader
 * 
 * Global shaders derive from this class to set their default recompile group as a global one
 */
class FGlobalShader : public FShader
{
	DECLARE_SHADER_TYPE(FGlobalShader,Global);
public:

	FGlobalShader() : FShader() {}

	SHADERCORE_API FGlobalShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer);
	
	template<typename TViewUniformShaderParameters, typename ShaderRHIParamRef, typename TRHICmdList>
	inline void SetParameters(TRHICmdList& RHICmdList, const ShaderRHIParamRef ShaderRHI, const FUniformBufferRHIParamRef ViewUniformBuffer)
	{
		const auto& ViewUniformBufferParameter = static_cast<const FShaderUniformBufferParameter&>(GetUniformBufferParameter<TViewUniformShaderParameters>());
		CheckShaderIsValid();
		SetUniformBufferParameter(RHICmdList, ShaderRHI, ViewUniformBufferParameter, ViewUniformBuffer);
	}

	typedef void (*ModifyCompilationEnvironmentType)(EShaderPlatform, FShaderCompilerEnvironment&);

	// FShader interface.
	SHADERCORE_API static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment) {}
};

/**
 * An internal dummy pixel shader to use when the user calls RHISetPixelShader(NULL).
 */
class FNULLPS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FNULLPS,Global, SHADERCORE_API);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	FNULLPS( )	{ }
	FNULLPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
	}
};

/**
* Container for Backup/RestoreGlobalShaderMap functions.
* Includes shader data from any populated feature levels.
*/
struct FGlobalShaderBackupData
{
	TUniquePtr<TArray<uint8>> FeatureLevelShaderData[ERHIFeatureLevel::Num];
};

/** Backs up all global shaders to memory through serialization, and removes all references to FShaders from the global shader map. */
extern SHADERCORE_API void BackupGlobalShaderMap(FGlobalShaderBackupData& OutGlobalShaderBackup);

/** Recreates shaders in the global shader map from the serialized memory. */
extern SHADERCORE_API void RestoreGlobalShaderMap(const FGlobalShaderBackupData& GlobalShaderData);

/**
 * Accesses the global shader map.  This is a global TShaderMap<FGlobalShaderType> which contains an instance of each global shader type.
 *
 * @param Platform Which platform's global shader map to use
 * @param bRefreshShaderMap If true, the existing global shader map will be tossed first
 * @return A reference to the global shader map.
 */
extern SHADERCORE_API TShaderMap<FGlobalShaderType>* GetGlobalShaderMap(EShaderPlatform Platform);

/**
  * Overload for the above GetGlobalShaderMap which takes a feature level and translates to the appropriate shader platform
  *
  * @param FeatureLevel - Which feature levels shader map to use
  * @param bRefreshShaderMap If true, the existing global shader map will be tossed first
  * @return A reference to the global shader map.
  *
  **/
inline TShaderMap<FGlobalShaderType>* GetGlobalShaderMap(ERHIFeatureLevel::Type FeatureLevel) 
{ 
	return GetGlobalShaderMap(GShaderPlatformForFeatureLevel[FeatureLevel]); 
}

