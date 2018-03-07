// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHIDefinitions.h"
#include "hlslcc.h"

#if PLATFORM_WINDOWS
#include "WindowsHWrapper.h"
#endif

struct FShaderCompilerInput;
struct FShaderCompilerOutput;

enum GLSLVersion 
{
	GLSL_150,
	GLSL_430,
	GLSL_ES2,
	GLSL_ES2_WEBGL,
	GLSL_150_ES2,	// ES2 Emulation
	GLSL_150_ES2_NOUB,	// ES2 Emulation with NoUBs
	GLSL_150_ES3_1,	// ES3.1 Emulation
	GLSL_ES2_IOS,
	GLSL_310_ES_EXT,
	GLSL_ES3_1_ANDROID,
	GLSL_SWITCH,
	GLSL_SWITCH_FORWARD,

	GLSL_MAX
};

class SHADERFORMATOPENGL_API FOpenGLFrontend
{
public:
	virtual ~FOpenGLFrontend()
	{

	}

	void CompileShader(const struct FShaderCompilerInput& Input, struct FShaderCompilerOutput& Output, const class FString& WorkingDirectory, GLSLVersion Version);

protected:
	// does the given version support SSO? Allows subclasses to device based on it's own format
	virtual bool SupportsSeparateShaderObjects(GLSLVersion Version);

	// if true, the shader output map will contain true names (i.e. ColorModifier) instead of helper names for runtime binding (i.e. pb_5)
	virtual bool OutputTrueParameterNames()
	{
		return false;
	}

	virtual bool IsSM5(GLSLVersion Version)
	{
		return Version == GLSL_430 || Version == GLSL_310_ES_EXT;
	}

	// what is the max number of samplers the shader platform can use?
	virtual uint32 GetMaxSamplers(GLSLVersion Version);

	virtual uint32 CalculateCrossCompilerFlags(GLSLVersion Version, bool bCompileES2With310, bool bUseFullPrecisionInPS);

	// set up compilation information like defines and HlslCompileTarget
	virtual void SetupPerVersionCompilationEnvironment(GLSLVersion Version, class FShaderCompilerDefinitions& AdditionalDefines, EHlslCompileTarget& HlslCompilerTarget);

	virtual void ConvertOpenGLVersionFromGLSLVersion(GLSLVersion InVersion, int& OutMajorVersion, int& OutMinorVersion);

	// create the compiling backend
	virtual struct FGlslCodeBackend* CreateBackend(GLSLVersion Version, uint32 CCFlags, EHlslCompileTarget HlslCompilerTarget);

	// create the language spec
	virtual class FGlslLanguageSpec* CreateLanguageSpec(GLSLVersion Version);


	// Allow a subclass to perform additional work on the cross compiled source code
	virtual bool PostProcessShaderSource(GLSLVersion Version, EShaderFrequency Frequency, const ANSICHAR* ShaderSource,
		uint32 SourceLen, class FShaderParameterMap& ParameterMap, TMap<FString, FString>& BindingNameMap, TArray<struct FShaderCompilerError>& Errors,
		const FShaderCompilerInput& ShaderInput)
	{
		return true;
	}

	// allow subclass to write out different output, returning true if it did write everything it needed
	virtual bool OptionalSerializeOutputAndReturnIfSerialized(FArchive& Ar)
	{
		return false;
	}


	void BuildShaderOutput(FShaderCompilerOutput& ShaderOutput, const FShaderCompilerInput& ShaderInput, const ANSICHAR* InShaderSource, int32 SourceLen, GLSLVersion Version);
	void PrecompileShader(FShaderCompilerOutput& ShaderOutput, const FShaderCompilerInput& ShaderInput, const ANSICHAR* ShaderSource, GLSLVersion Version, EHlslShaderFrequency Frequency);
	void PrecompileGLSLES2(FShaderCompilerOutput& ShaderOutput, const FShaderCompilerInput& ShaderInput, const ANSICHAR* ShaderSource, EHlslShaderFrequency Frequency);
};
