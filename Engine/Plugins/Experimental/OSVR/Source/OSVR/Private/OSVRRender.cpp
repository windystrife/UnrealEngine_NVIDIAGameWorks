//
// Copyright 2014, 2015 Razer Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "CoreMinimal.h"
#include "OSVRPrivate.h"
#include "OSVRHMD.h"

// Must put path from Engine/Source to these includes since we are an out-of-tree module.
#include "Runtime/Renderer/Private/RendererPrivate.h"
#include "Runtime/Renderer/Private/ScenePrivate.h"
#include "Runtime/Renderer/Private/PostProcess/PostProcessHMD.h"
#include "Runtime/Engine/Public/ScreenRendering.h"
#include "PipelineStateCache.h"

void FOSVRHMD::DrawDistortionMesh_RenderThread(FRenderingCompositePassContext& Context, const FIntPoint& TextureSize)
{
	// shouldn't be called with a custom present
	check(0);
}

// Based off of the SteamVR Unreal Plugin implementation.
void FOSVRHMD::RenderTexture_RenderThread(FRHICommandListImmediate& rhiCmdList, FTexture2DRHIParamRef backBuffer, FTexture2DRHIParamRef srcTexture, FVector2D WindowSize) const
{
	check(IsInRenderingThread());
	const uint32 viewportWidth = backBuffer->GetSizeX();
	const uint32 viewportHeight = backBuffer->GetSizeY();

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	SetRenderTarget(rhiCmdList, backBuffer, FTextureRHIRef());
	rhiCmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	rhiCmdList.SetViewport(0, 0, 0, viewportWidth, viewportHeight, 1.0f);

	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	const auto featureLevel = GMaxRHIFeatureLevel;
	auto shaderMap = GetGlobalShaderMap(featureLevel);

	TShaderMapRef<FScreenVS> vertexShader(shaderMap);
	TShaderMapRef<FScreenPS> pixelShader(shaderMap);

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = RendererModule->GetFilterVertexDeclaration().VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*vertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*pixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(rhiCmdList, GraphicsPSOInit);

	pixelShader->SetParameters(rhiCmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), srcTexture);

	RendererModule->DrawRectangle(
		rhiCmdList,
		0, 0, // X, Y
		viewportWidth, viewportHeight, // SizeX, SizeY
		0.0f, 0.0f, // U, V
		1.0f, 1.0f, // SizeU, SizeV
		FIntPoint(viewportWidth, viewportHeight), // TargetSize
		FIntPoint(1, 1), // TextureSize
		*vertexShader,
		EDRF_Default);
}

void FOSVRHMD::GetEyeRenderParams_RenderThread(const struct FRenderingCompositePassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const
{
	if (Context.View.StereoPass == eSSP_LEFT_EYE)
	{
		EyeToSrcUVOffsetValue.X = 0.0f;
		EyeToSrcUVOffsetValue.Y = 0.0f;

		EyeToSrcUVScaleValue.X = 0.5f;
		EyeToSrcUVScaleValue.Y = 1.0f;
	}
	else
	{
		EyeToSrcUVOffsetValue.X = 0.5f;
		EyeToSrcUVOffsetValue.Y = 0.0f;

		EyeToSrcUVScaleValue.X = 0.5f;
		EyeToSrcUVScaleValue.Y = 1.0f;
	}
}

void FOSVRHMD::BeginRendering_RenderThread(const FTransform& NewRelativeTransform, FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily)
{
	check(IsInRenderingThread());
	FHeadMountedDisplayBase::BeginRendering_RenderThread(NewRelativeTransform, RHICmdList, ViewFamily);
	if (mCustomPresent && !mCustomPresent->IsInitialized())
	{
		mCustomPresent->Initialize();
	}
}

void FOSVRHMD::CalculateRenderTargetSize(const FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY)
{
	check(IsInGameThread());

	if (!IsStereoEnabled())
	{
		return;
	}

	float screenScale = GetScreenScale();
	if (mCustomPresent)
	{
		if (!mCustomPresent->IsInitialized() && IsInRenderingThread() && !mCustomPresent->Initialize())
		{
			mCustomPresent = nullptr;
		}
		if (mCustomPresent && mCustomPresent->IsInitialized())
		{
			mCustomPresent->CalculateRenderTargetSize(InOutSizeX, InOutSizeY, screenScale);
		}
	}
	else
	{
		auto leftEye = HMDDescription.GetDisplaySize(OSVRHMDDescription::LEFT_EYE);
		auto rightEye = HMDDescription.GetDisplaySize(OSVRHMDDescription::RIGHT_EYE);
		InOutSizeX = leftEye.X + rightEye.X;
		InOutSizeY = leftEye.Y;
		InOutSizeX = int(float(InOutSizeX) * screenScale);
		InOutSizeY = int(float(InOutSizeY) * screenScale);
	}
}

void FOSVRHMD::UpdateViewportRHIBridge(bool /* bUseSeparateRenderTarget */, const class FViewport& InViewport, FRHIViewport* const ViewportRHI)
{
	check(IsInGameThread());

	if (mCustomPresent && mCustomPresent->IsInitialized())
	{
		if (!mCustomPresent->UpdateViewport(InViewport, ViewportRHI))
		{
			delete mCustomPresent;
			mCustomPresent = nullptr;
		}
	}
	
	if (!mCustomPresent)
	{
		ViewportRHI->SetCustomPresent(nullptr);
	}
}

bool FOSVRHMD::AllocateRenderTargetTexture(uint32 index, uint32 sizeX, uint32 sizeY, uint8 format, uint32 numMips, uint32 flags, uint32 targetableTextureFlags, FTexture2DRHIRef& outTargetableTexture, FTexture2DRHIRef& outShaderResourceTexture, uint32 numSamples)
{
	check(index == 0);
	if (mCustomPresent && mCustomPresent->IsInitialized())
	{
		return mCustomPresent->AllocateRenderTargetTexture(index, sizeX, sizeY, format, numMips, flags, targetableTextureFlags, outTargetableTexture, outShaderResourceTexture, numSamples);
	}
	return false;
}
