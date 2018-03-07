// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
ShaderComplexityRendering.h: Declarations used for the shader complexity viewmode.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "DebugViewModeRendering.h"

class FPrimitiveSceneProxy;
struct FMeshBatchElement;
struct FMeshDrawingRenderState;

template <bool bQuadComplexity>
class TComplexityAccumulatePS : public FGlobalShader, public IDebugViewModePSInterface
{
	DECLARE_SHADER_TYPE(TComplexityAccumulatePS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return AllowDebugViewPS(bQuadComplexity ? DVSM_QuadComplexity : DVSM_ShaderComplexity, Platform);
	}

	TComplexityAccumulatePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		NormalizedComplexity.Bind(Initializer.ParameterMap,TEXT("NormalizedComplexity"));
		ShowQuadOverdraw.Bind(Initializer.ParameterMap,TEXT("bShowQuadOverdraw"));
		QuadBufferUAV.Bind(Initializer.ParameterMap,TEXT("RWQuadBuffer"));
	}

	TComplexityAccumulatePS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << NormalizedComplexity;
		Ar << ShowQuadOverdraw;
		Ar << QuadBufferUAV;
		return bShaderHasOutdatedParameters;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("OUTPUT_QUAD_OVERDRAW"), AllowDebugViewPS(DVSM_QuadComplexity, Platform));
	}

	virtual void SetParameters(
		FRHICommandList& RHICmdList, 
		const FShader* OriginalVS, 
		const FShader* OriginalPS, 
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FMaterial& Material,
		const FSceneView& View
		) override;

	virtual void SetMesh(
		FRHICommandList& RHICmdList, 
		const FVertexFactory* VertexFactory,
		const FSceneView& View,
		const FPrimitiveSceneProxy* Proxy,
		int32 VisualizeLODIndex,
		const FMeshBatchElement& BatchElement, 
		const FDrawingPolicyRenderState& DrawRenderState
		) override {}

	virtual void SetMesh(FRHICommandList& RHICmdList, const FSceneView& View) override {}

	virtual FShader* GetShader() override { return static_cast<FShader*>(this); }

private:

	FShaderParameter NormalizedComplexity;
	FShaderParameter ShowQuadOverdraw;
	FShaderResourceParameter QuadBufferUAV;
};

typedef TComplexityAccumulatePS<false> TShaderComplexityAccumulatePS;
typedef TComplexityAccumulatePS<true> TQuadComplexityAccumulatePS;

