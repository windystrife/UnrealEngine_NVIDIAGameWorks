// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLShaders.h: OpenGL shader RHI declaration.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "OpenGLDrv.h"

typedef TArray<ANSICHAR> FAnsiCharArray;

enum EOpenGLShaderTargetPlatform
{
	OGLSTP_Unknown,
	OGLSTP_Desktop,
	OGLSTP_Android,
	OGLSTP_HTML5,
	OGLSTP_iOS,
};

/**
  * GL device capabilities for generating GLSL compilable on platform with described capabilities
  */
struct FOpenGLShaderDeviceCapabilities
{
	EOpenGLShaderTargetPlatform TargetPlatform;
	EShaderPlatform MaxRHIShaderPlatform;
	bool bUseES30ShadingLanguage;
	bool bSupportsSeparateShaderObjects;
	bool bSupportsStandardDerivativesExtension;
	bool bSupportsRenderTargetFormat_PF_FloatRGBA;
	bool bSupportsShaderFramebufferFetch;
	bool bRequiresUEShaderFramebufferFetchDef;
	bool bRequiresARMShaderFramebufferFetchDepthStencilUndef;
	bool bRequiresDontEmitPrecisionForTextureSamplers;
	bool bSupportsShaderTextureLod;
	bool bSupportsShaderTextureCubeLod;
	bool bRequiresTextureCubeLodEXTToTextureCubeLodDefine;
	bool bRequiresGLFragCoordVaryingLimitHack;
	GLint MaxVaryingVectors;
	bool bRequiresTexture2DPrecisionHack;
};

/**
  * Gets the GL device capabilities for the current device.
  *
  * @param Capabilities [out] The current platform's capabilities on device for shader compiling
  */
void OPENGLDRV_API GetCurrentOpenGLShaderDeviceCapabilities(FOpenGLShaderDeviceCapabilities& Capabilities);

/**
  * Processes the GLSL output of the shader cross compiler to get GLSL that can be compiled on a
  * platform with the specified capabilities. Works around inconsistencies between OpenGL
  * implementations, including lack of support for certain extensions and drivers with
  * non-conformant behavior.
  *
  * @param GlslCodeOriginal - [in,out] GLSL output from shader cross compiler to be modified.  Process is destructive; pass in a copy if still need original!
  * @param ShaderName - [in] Shader name
  * @param TypeEnum - [in] Type of shader (GL_[VERTEX, FRAGMENT, GEOMETRY, TESS_CONTROL, TESS_EVALUATION]_SHADER)
  * @param Capabilities - [in] GL Device capabilities
  * @param GlslCode - [out] Compilable GLSL
  */
void OPENGLDRV_API GLSLToDeviceCompatibleGLSL(FAnsiCharArray& GlslCodeOriginal, const FString& ShaderName, GLenum TypeEnum, const FOpenGLShaderDeviceCapabilities& Capabilities, FAnsiCharArray& GlslCode);
