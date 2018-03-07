// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "ShadowRendering.h"

/*=============================================================================
	SceneOcclusion.h
=============================================================================*/

/**
* A vertex shader for rendering a texture on a simple element.
*/
class FOcclusionQueryVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FOcclusionQueryVS,Global);
public:
	static bool ShouldCache(EShaderPlatform Platform) { return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4); }

	FOcclusionQueryVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
			StencilingGeometryParameters.Bind(Initializer.ParameterMap);
	}

	FOcclusionQueryVS() {}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	void SetParametersWithBoundingSphere(FRHICommandList& RHICmdList, const FSceneView& View, const FSphere& BoundingSphere)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetVertexShader(), View.ViewUniformBuffer);

		FVector4 StencilingSpherePosAndScale;
		StencilingGeometry::GStencilSphereVertexBuffer.CalcTransform(StencilingSpherePosAndScale, BoundingSphere, View.ViewMatrices.GetPreViewTranslation());
		StencilingGeometryParameters.Set(RHICmdList, this, StencilingSpherePosAndScale);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetVertexShader(),View.ViewUniformBuffer);

		// Don't transform if rendering frustum
		StencilingGeometryParameters.Set(RHICmdList, this, FVector4(0,0,0,1));
	}

	//~ Begin FShader Interface
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << StencilingGeometryParameters;
		return bShaderHasOutdatedParameters;
	}
	//~ Begin  End FShader Interface 

private:
	FStencilingGeometryShaderParameters StencilingGeometryParameters;
};

