// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LongGPUTask.h"
#include "OneColorShader.h"
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"

IMPLEMENT_SHADER_TYPE(, FLongGPUTaskPS, TEXT("/Engine/Private/OneColorShader.usf"), TEXT("MainLongGPUTask"), SF_Pixel);

/** Vertex declaration for just one FVector4 position. */
class FVector4VertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;
	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		Elements.Add(FVertexElement(0, 0, VET_Float4, 0, sizeof(FVector4)));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}
	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

TGlobalResource<FVector4VertexDeclaration> GLongGPUTaskVector4VertexDeclaration;

int32 NumMeasuredIterationsToAchieve100ms = 0;

const int32 NumIterationsForMeasurement = 5;

FRenderQueryRHIRef TimeQueryStart;
FRenderQueryRHIRef TimeQueryEnd;

void IssueScalableLongGPUTask(FRHICommandListImmediate& RHICmdList, int32 NumIteration /* = -1 by default */)
{
	FRHIResourceCreateInfo Info;
	FTexture2DRHIRef LongTaskRenderTarget = RHICreateTexture2D(1920, 1080, PF_B8G8R8A8, 1, 1, TexCreate_RenderTargetable, Info);

	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;

	SetRenderTarget(RHICmdList, LongTaskRenderTarget, FTextureRHIRef(), true);
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	TShaderMapRef<TOneColorVS<true>> VertexShader(ShaderMap);
	TShaderMapRef<FLongGPUTaskPS> PixelShader(ShaderMap);

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GLongGPUTaskVector4VertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader->GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader->GetPixelShader();
	GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	FVector4 Vertices[4];
	Vertices[0].Set(-1.0f, 1.0f, 0, 1.0f);
	Vertices[1].Set(1.0f, 1.0f, 0, 1.0f);
	Vertices[2].Set(-1.0f, -1.0f, 0, 1.0f);
	Vertices[3].Set(1.0f, -1.0f, 0, 1.0f);

	if (NumIteration == -1)
	{
		// Use the measured number of iterations to achieve 100ms
		// If the query results are still not available, stall
		if (NumMeasuredIterationsToAchieve100ms == 0)
		{
			if (TimeQueryStart != nullptr && TimeQueryEnd != nullptr) // Not all platforms/drivers support RQT_AbsoluteTime queries
			{
				uint64 StartTime = 0;
				uint64 EndTime = 0;

				// Results are in microseconds
				RHICmdList.GetRenderQueryResult(TimeQueryStart, StartTime, true);
				RHICmdList.GetRenderQueryResult(TimeQueryEnd, EndTime, true);

				NumMeasuredIterationsToAchieve100ms = FMath::Clamp(FMath::FloorToInt(100.0f / ((EndTime - StartTime) / 1000.0f / NumIterationsForMeasurement)), 1, 200);
			}
			else
			{
				NumMeasuredIterationsToAchieve100ms = 5; // Use a constant time on these platforms
			}
		}

		NumIteration = NumMeasuredIterationsToAchieve100ms;
	}

	for (int32 Iteration = 0; Iteration < NumIteration; Iteration++)
	{
		DrawPrimitiveUP(RHICmdList, PT_TriangleStrip, 2, Vertices, sizeof(Vertices[0]));
	}
}

void MeasureLongGPUTaskExecutionTime(FRHICommandListImmediate& RHICmdList)
{
	check(TimeQueryStart == nullptr && TimeQueryEnd == nullptr);

	TimeQueryStart = RHICmdList.CreateRenderQuery(RQT_AbsoluteTime);
	TimeQueryEnd = RHICmdList.CreateRenderQuery(RQT_AbsoluteTime);

	if (TimeQueryStart != nullptr && TimeQueryEnd != nullptr) // Not all platforms/drivers support RQT_AbsoluteTime queries
	{
		RHICmdList.EndRenderQuery(TimeQueryStart);

		IssueScalableLongGPUTask(RHICmdList, NumIterationsForMeasurement);

		RHICmdList.EndRenderQuery(TimeQueryEnd);
	}
}
