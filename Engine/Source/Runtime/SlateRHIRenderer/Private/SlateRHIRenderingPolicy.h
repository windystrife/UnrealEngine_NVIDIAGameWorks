// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/ShaderResourceManager.h"
#include "Rendering/DrawElements.h"
#include "Rendering/RenderingPolicy.h"
#include "Layout/Clipping.h"
#include "SlateElementIndexBuffer.h"
#include "SlateElementVertexBuffer.h"
#include "SlateRHIResourceManager.h"
#include "Shader.h"
#include "Engine/TextureLODSettings.h"

class FSlateFontServices;
class FSlateRHIResourceManager;
class FSlatePostProcessor;
class ILayoutCache;

struct FSlateRenderingOptions
{
	FMatrix ViewProjectionMatrix;
	FVector2D ViewOffset;
	bool bAllowSwitchVerticalAxis;
	bool bWireFrame;

	FSlateRenderingOptions(const FMatrix& InViewProjectionMatrix)
		: ViewProjectionMatrix(InViewProjectionMatrix)
		, ViewOffset(0, 0)
		, bAllowSwitchVerticalAxis(true)
		, bWireFrame(false)
	{
	}
};

class FSlateRHIRenderingPolicy : public FSlateRenderingPolicy
{
public:
	FSlateRHIRenderingPolicy(TSharedRef<FSlateFontServices> InSlateFontServices, TSharedRef<FSlateRHIResourceManager> InResourceManager, TOptional<int32> InitialBufferSize = TOptional<int32>());

	void UpdateVertexAndIndexBuffers(FRHICommandListImmediate& RHICmdList, FSlateBatchData& BatchData);
	void UpdateVertexAndIndexBuffers(FRHICommandListImmediate& RHICmdList, FSlateBatchData& BatchData, const TSharedRef<FSlateRenderDataHandle, ESPMode::ThreadSafe>& RenderHandle);

	void ReleaseCachingResourcesFor(FRHICommandListImmediate& RHICmdList, const ILayoutCache* Cacher);

	void DrawElements(FRHICommandListImmediate& RHICmdList, class FSlateBackBuffer& BackBuffer, FTexture2DRHIRef& ColorTarget, FTexture2DRHIRef& DepthStencilTarget, const TArray<FSlateRenderBatch>& RenderBatches, const TArray<FSlateClippingState> RenderClipStates, const FSlateRenderingOptions& Options);

	virtual TSharedRef<FSlateShaderResourceManager> GetResourceManager() const override { return ResourceManager; }
	virtual bool IsVertexColorInLinearSpace() const override { return false; }
	
	void InitResources();
	void ReleaseResources();

	void BeginDrawingWindows();
	void EndDrawingWindows();

	void SetUseGammaCorrection( bool bInUseGammaCorrection ) { bGammaCorrect = bInUseGammaCorrection; }

	virtual void AddSceneAt(FSceneInterface* Scene, int32 Index) override;
	virtual void ClearScenes() override;

	virtual void FlushGeneratedResources();

protected:
	void UpdateVertexAndIndexBuffers(FRHICommandListImmediate& RHICmdList, FSlateBatchData& BatchData, TSlateElementVertexBuffer<FSlateVertex>& VertexBuffer, FSlateElementIndexBuffer& IndexBuffer);

private:
	ETextureSamplerFilter GetSamplerFilter(const TArray<FTextureLODGroup>& TextureLODGroups, const UTexture* Texture) const;

	/**
	 * Returns the pixel shader that should be used for the specified ShaderType and DrawEffects
	 * 
	 * @param ShaderType	The shader type being used
	 * @param DrawEffects	Draw effects being used
	 * @return The pixel shader for use with the shader type and draw effects
	 */
	class FSlateElementPS* GetTexturePixelShader( TShaderMap<FGlobalShaderType>* ShaderMap, ESlateShader::Type ShaderType, ESlateDrawEffect DrawEffects );
	class FSlateMaterialShaderPS* GetMaterialPixelShader( const class FMaterial* Material, ESlateShader::Type ShaderType, ESlateDrawEffect DrawEffects );
	class FSlateMaterialShaderVS* GetMaterialVertexShader( const class FMaterial* Material, bool bUseInstancing );

	/** @return The RHI primitive type from the Slate primitive type */
	EPrimitiveType GetRHIPrimitiveType(ESlateDrawPrimitive::Type SlateType);

private:
	/** Buffers used for rendering */
	TSlateElementVertexBuffer<FSlateVertex> VertexBuffers;
	FSlateElementIndexBuffer IndexBuffers;

	/** Handles post process effects for slate */
	TSharedRef<FSlatePostProcessor> PostProcessor;

	TSharedRef<FSlateRHIResourceManager> ResourceManager;

	bool bGammaCorrect;

	TOptional<int32> InitialBufferSizeOverride;
};
