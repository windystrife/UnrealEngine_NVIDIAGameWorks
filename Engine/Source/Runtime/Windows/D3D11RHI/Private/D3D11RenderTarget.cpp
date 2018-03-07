// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11RenderTarget.cpp: D3D render target implementation.
=============================================================================*/

#include "D3D11RHIPrivate.h"
#include "BatchedElements.h"
#include "ScreenRendering.h"
#include "RHIStaticStates.h"
#include "ResolveShader.h"
#include "PipelineStateCache.h"

static inline DXGI_FORMAT ConvertTypelessToUnorm(DXGI_FORMAT Format)
{
	// required to prevent 
	// D3D11: ERROR: ID3D11DeviceContext::ResolveSubresource: The Format (0x1b, R8G8B8A8_TYPELESS) is never able to resolve multisampled resources. [ RESOURCE_MANIPULATION ERROR #294: DEVICE_RESOLVESUBRESOURCE_FORMAT_INVALID ]
	// D3D11: **BREAK** enabled for the previous D3D11 message, which was: [ RESOURCE_MANIPULATION ERROR #294: DEVICE_RESOLVESUBRESOURCE_FORMAT_INVALID ]
	switch (Format)
	{
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
			return DXGI_FORMAT_R8G8B8A8_UNORM;

		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
			return DXGI_FORMAT_B8G8R8A8_UNORM;

		default:
			return Format;
	}
}

static FResolveRect GetDefaultRect(const FResolveRect& Rect,uint32 DefaultWidth,uint32 DefaultHeight)
{
	if (Rect.X1 >= 0 && Rect.X2 >= 0 && Rect.Y1 >= 0 && Rect.Y2 >= 0)
	{
		return Rect;
	}
	else
	{
		return FResolveRect(0,0,DefaultWidth,DefaultHeight);
	}
}

template<typename TPixelShader>
void FD3D11DynamicRHI::ResolveTextureUsingShader(
	FRHICommandList_RecursiveHazardous& RHICmdList,
	FD3D11Texture2D* SourceTexture,
	FD3D11Texture2D* DestTexture,
	ID3D11RenderTargetView* DestTextureRTV,
	ID3D11DepthStencilView* DestTextureDSV,
	const D3D11_TEXTURE2D_DESC& ResolveTargetDesc,
	const FResolveRect& SourceRect,
	const FResolveRect& DestRect,
	FD3D11DeviceContext* Direct3DDeviceContext, 
	typename TPixelShader::FParameter PixelShaderParameter
	)
{
	// Save the current viewport so that it can be restored
	D3D11_VIEWPORT SavedViewport;
	uint32 NumSavedViewports = 1;
	StateCache.GetViewports(&NumSavedViewports,&SavedViewport);

	RHICmdList.Flush(); // always call flush when using a command list in RHI implementations before doing anything else. This is super hazardous.
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	// No alpha blending, no depth tests or writes, no stencil tests or writes, no backface culling.
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();

	// Make sure the destination is not bound as a shader resource.
	if (DestTexture)
	{
		ConditionalClearShaderResource(DestTexture);
	}

	// Determine if the entire destination surface is being resolved to.
	// If the entire surface is being resolved to, then it means we can clear it and signal the driver that it can discard
	// the surface's previous contents, which breaks dependencies between frames when using alternate-frame SLI.
	const bool bClearDestTexture =
			DestRect.X1 == 0
		&&	DestRect.Y1 == 0
		&&	DestRect.X2 == ResolveTargetDesc.Width
		&&	DestRect.Y2 == ResolveTargetDesc.Height;
	
	//we may change rendertargets and depth state behind the RHI's back here.
	//save off this original state to restore it.
	FExclusiveDepthStencil OriginalDSVAccessType = CurrentDSVAccessType;
	TRefCountPtr<FD3D11TextureBase> OriginalDepthTexture = CurrentDepthTexture;

	if(ResolveTargetDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
	{
		// Clear the destination texture.
		if(bClearDestTexture)
		{
			GPUProfilingData.RegisterGPUWork(0);

			Direct3DDeviceContext->ClearDepthStencilView(DestTextureDSV,D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,0,0);
		}

		//hack this to  pass validation in SetDepthStencil state since we are directly changing targets with a call to OMSetRenderTargets later.
		CurrentDSVAccessType = FExclusiveDepthStencil::DepthWrite_StencilWrite;
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true,CF_Always>::GetRHI();

		// Write to the dest texture as a depth-stencil target.
		ID3D11RenderTargetView* NullRTV = NULL;
		Direct3DDeviceContext->OMSetRenderTargets(1,&NullRTV,DestTextureDSV);
	}
	else
	{
		// Clear the destination texture.
		if(bClearDestTexture)
		{
			GPUProfilingData.RegisterGPUWork(0);

			FLinearColor ClearColor(0,0,0,0);
			Direct3DDeviceContext->ClearRenderTargetView(DestTextureRTV,(float*)&ClearColor);
		}

		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false,CF_Always>::GetRHI();

		// Write to the dest surface as a render target.
		Direct3DDeviceContext->OMSetRenderTargets(1,&DestTextureRTV,NULL);
	}

	RHICmdList.SetViewport(0.0f, 0.0f, 0.0f, ResolveTargetDesc.Width, ResolveTargetDesc.Height, 1.0f  );


	// Generate the vertices used to copy from the source surface to the destination surface.
	const float MinU = SourceRect.X1;
	const float MinV = SourceRect.Y1;
	const float MaxU = SourceRect.X2;
	const float MaxV = SourceRect.Y2;
	const float MinX = -1.f + DestRect.X1 / ((float)ResolveTargetDesc.Width * 0.5f);		
	const float MinY = +1.f - DestRect.Y1 / ((float)ResolveTargetDesc.Height * 0.5f);
	const float MaxX = -1.f + DestRect.X2 / ((float)ResolveTargetDesc.Width * 0.5f);		
	const float MaxY = +1.f - DestRect.Y2 / ((float)ResolveTargetDesc.Height * 0.5f);

	// Set the vertex and pixel shader
	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FResolveVS> ResolveVertexShader(ShaderMap);
	TShaderMapRef<TPixelShader> ResolvePixelShader(ShaderMap);

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*ResolveVertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*ResolvePixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

	CurrentDepthTexture = DestTexture;
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
	RHICmdList.SetBlendFactor(FLinearColor::White);

	ResolvePixelShader->SetParameters(RHICmdList, PixelShaderParameter);
	RHICmdList.Flush(); // always call flush when using a command list in RHI implementations before doing anything else. This is super hazardous.

	// Set the source texture.
	const uint32 TextureIndex = ResolvePixelShader->UnresolvedSurface.GetBaseIndex();

	if (SourceTexture)
	{
		SetShaderResourceView<SF_Pixel>(SourceTexture, SourceTexture->GetShaderResourceView(), TextureIndex, SourceTexture->GetName());
	}

	// Generate the vertices used
	FScreenVertex Vertices[4];

	Vertices[0].Position.X = MaxX;
	Vertices[0].Position.Y = MinY;
	Vertices[0].UV.X       = MaxU;
	Vertices[0].UV.Y       = MinV;

	Vertices[1].Position.X = MaxX;
	Vertices[1].Position.Y = MaxY;
	Vertices[1].UV.X       = MaxU;
	Vertices[1].UV.Y       = MaxV;

	Vertices[2].Position.X = MinX;
	Vertices[2].Position.Y = MinY;
	Vertices[2].UV.X       = MinU;
	Vertices[2].UV.Y       = MinV;

	Vertices[3].Position.X = MinX;
	Vertices[3].Position.Y = MaxY;
	Vertices[3].UV.X       = MinU;
	Vertices[3].UV.Y       = MaxV;

	DrawPrimitiveUP(RHICmdList, PT_TriangleStrip, 2, Vertices, sizeof(Vertices[0]));
	RHICmdList.Flush(); // always call flush when using a command list in RHI implementations before doing anything else. This is super hazardous.

	if (SourceTexture)
	{
		ConditionalClearShaderResource(SourceTexture);
	}

	// Reset saved render targets
	CommitRenderTargetsAndUAVs();

	// Reset saved viewport
	RHISetMultipleViewports(1,(FViewportBounds*)&SavedViewport);

	//reset DSVAccess.
	CurrentDSVAccessType = OriginalDSVAccessType;
	CurrentDepthTexture = OriginalDepthTexture;
}

/**
* Copies the contents of the given surface to its resolve target texture.
* @param SourceSurface - surface with a resolve texture to copy to
* @param bKeepOriginalSurface - true if the original surface will still be used after this function so must remain valid
* @param ResolveParams - optional resolve params
*/
void FD3D11DynamicRHI::RHICopyToResolveTarget(FTextureRHIParamRef SourceTextureRHI, FTextureRHIParamRef DestTextureRHI, bool bKeepOriginalSurface, const FResolveParams& ResolveParams)
{
	if (!SourceTextureRHI || !DestTextureRHI)
	{
		// no need to do anything (sliently ignored)
		return;
	}

	RHITransitionResources(EResourceTransitionAccess::EReadable, &SourceTextureRHI, 1);

	FRHICommandList_RecursiveHazardous RHICmdList(this);
	

	FD3D11Texture2D* SourceTexture2D = static_cast<FD3D11Texture2D*>(SourceTextureRHI->GetTexture2D());
	FD3D11Texture2D* DestTexture2D = static_cast<FD3D11Texture2D*>(DestTextureRHI->GetTexture2D());

	FD3D11TextureCube* SourceTextureCube = static_cast<FD3D11TextureCube*>(SourceTextureRHI->GetTextureCube());
	FD3D11TextureCube* DestTextureCube = static_cast<FD3D11TextureCube*>(DestTextureRHI->GetTextureCube());

	FD3D11Texture3D* SourceTexture3D = static_cast<FD3D11Texture3D*>(SourceTextureRHI->GetTexture3D());
	FD3D11Texture3D* DestTexture3D = static_cast<FD3D11Texture3D*>(DestTextureRHI->GetTexture3D());
		
	if(SourceTexture2D && DestTexture2D)
	{
		check(!SourceTextureCube && !DestTextureCube);
		if(SourceTexture2D != DestTexture2D)
		{
			GPUProfilingData.RegisterGPUWork();
		
			if(FeatureLevel == D3D_FEATURE_LEVEL_11_0 
				&& DestTexture2D->GetDepthStencilView(FExclusiveDepthStencil::DepthWrite_StencilWrite)
				&& SourceTextureRHI->IsMultisampled()
				&& !DestTextureRHI->IsMultisampled())
			{
				D3D11_TEXTURE2D_DESC ResolveTargetDesc;
				
				DestTexture2D->GetResource()->GetDesc(&ResolveTargetDesc);

				ResolveTextureUsingShader<FResolveDepthPS>(
					RHICmdList,
					SourceTexture2D,
					DestTexture2D,
					DestTexture2D->GetRenderTargetView(0, -1),
					DestTexture2D->GetDepthStencilView(FExclusiveDepthStencil::DepthWrite_StencilWrite),
					ResolveTargetDesc,
					GetDefaultRect(ResolveParams.Rect,DestTexture2D->GetSizeX(),DestTexture2D->GetSizeY()),
					GetDefaultRect(ResolveParams.Rect,DestTexture2D->GetSizeX(),DestTexture2D->GetSizeY()),
					Direct3DDeviceIMContext,
					FDummyResolveParameter()
					);
			}
			else if(FeatureLevel == D3D_FEATURE_LEVEL_10_0 
				&& DestTexture2D->GetDepthStencilView(FExclusiveDepthStencil::DepthWrite_StencilWrite))
			{
				D3D11_TEXTURE2D_DESC ResolveTargetDesc;

				DestTexture2D->GetResource()->GetDesc(&ResolveTargetDesc);

				ResolveTextureUsingShader<FResolveDepthNonMSPS>(
					RHICmdList,
					SourceTexture2D,
					DestTexture2D,
					NULL,
					DestTexture2D->GetDepthStencilView(FExclusiveDepthStencil::DepthWrite_StencilWrite),
					ResolveTargetDesc,
					GetDefaultRect(ResolveParams.Rect,DestTexture2D->GetSizeX(),DestTexture2D->GetSizeY()),
					GetDefaultRect(ResolveParams.Rect,DestTexture2D->GetSizeX(),DestTexture2D->GetSizeY()),
					Direct3DDeviceIMContext,
					FDummyResolveParameter()
					);
			}
			else
			{
				DXGI_FORMAT SrcFmt = (DXGI_FORMAT)GPixelFormats[SourceTextureRHI->GetFormat()].PlatformFormat;
				DXGI_FORMAT DstFmt = (DXGI_FORMAT)GPixelFormats[DestTexture2D->GetFormat()].PlatformFormat;
				
				DXGI_FORMAT Fmt = ConvertTypelessToUnorm((DXGI_FORMAT)GPixelFormats[DestTexture2D->GetFormat()].PlatformFormat);

				// Determine whether a MSAA resolve is needed, or just a copy.
				if(SourceTextureRHI->IsMultisampled() && !DestTexture2D->IsMultisampled())
				{
					Direct3DDeviceIMContext->ResolveSubresource(
						DestTexture2D->GetResource(),
						0,
						SourceTexture2D->GetResource(),
						0,
						Fmt
						);
				}
				else
				{
					if(ResolveParams.Rect.IsValid())
					{
						D3D11_BOX SrcBox;

						SrcBox.left = ResolveParams.Rect.X1;
						SrcBox.top = ResolveParams.Rect.Y1;
						SrcBox.front = 0;
						SrcBox.right = ResolveParams.Rect.X2;
						SrcBox.bottom = ResolveParams.Rect.Y2;
						SrcBox.back = 1;

						Direct3DDeviceIMContext->CopySubresourceRegion(DestTexture2D->GetResource(), 0, ResolveParams.Rect.X1, ResolveParams.Rect.Y1, 0, SourceTexture2D->GetResource(), 0, &SrcBox);
					}
					else
					{
						Direct3DDeviceIMContext->CopyResource(DestTexture2D->GetResource(), SourceTexture2D->GetResource());
					}
				}
			}
		}
	}
	else if(SourceTextureCube && DestTextureCube)
	{
		check(!SourceTexture2D && !DestTexture2D);			

		if(SourceTextureCube != DestTextureCube)
		{
			GPUProfilingData.RegisterGPUWork();

			// Determine the cubemap face being resolved.
			const uint32 D3DFace = GetD3D11CubeFace(ResolveParams.CubeFace);
			const uint32 SourceSubresource = D3D11CalcSubresource(ResolveParams.MipIndex, ResolveParams.SourceArrayIndex * 6 + D3DFace, SourceTextureCube->GetNumMips());
			const uint32 DestSubresource = D3D11CalcSubresource(ResolveParams.MipIndex, ResolveParams.DestArrayIndex * 6 + D3DFace, DestTextureCube->GetNumMips());

			// Determine whether a MSAA resolve is needed, or just a copy.
			if(SourceTextureRHI->IsMultisampled() && !DestTextureCube->IsMultisampled())
			{
				Direct3DDeviceIMContext->ResolveSubresource(
					DestTextureCube->GetResource(),
					DestSubresource,
					SourceTextureCube->GetResource(),
					SourceSubresource,
					(DXGI_FORMAT)GPixelFormats[DestTextureCube->GetFormat()].PlatformFormat
					);
			}
			else
			{
				if (ResolveParams.Rect.IsValid())
				{
					D3D11_BOX SrcBox;

					SrcBox.left = ResolveParams.Rect.X1;
					SrcBox.top = ResolveParams.Rect.Y1;
					SrcBox.front = 0;
					SrcBox.right = ResolveParams.Rect.X2;
					SrcBox.bottom = ResolveParams.Rect.Y2;
					SrcBox.back = 1;

					Direct3DDeviceIMContext->CopySubresourceRegion(DestTextureCube->GetResource(), DestSubresource, 0, 0, 0, SourceTextureCube->GetResource(), SourceSubresource, &SrcBox);
				}
				else
				{
					Direct3DDeviceIMContext->CopySubresourceRegion(DestTextureCube->GetResource(), DestSubresource, 0, 0, 0, SourceTextureCube->GetResource(), SourceSubresource, NULL);
				}
			}
		}
	}
	else if(SourceTexture2D && DestTextureCube)
	{
		// If source is 2D and Dest is a cube then copy the 2D texture to the specified cube face.
		// Determine the cubemap face being resolved.
		const uint32 D3DFace = GetD3D11CubeFace(ResolveParams.CubeFace);
		const uint32 Subresource = D3D11CalcSubresource(0, D3DFace, 1);
		Direct3DDeviceIMContext->CopySubresourceRegion(DestTextureCube->GetResource(), Subresource, 0, 0, 0, SourceTexture2D->GetResource(), 0, NULL);
	}
	else if (SourceTexture3D && DestTexture3D)
	{
		// bit of a hack.  no one resolves slice by slice and 0 is the default value.  assume for the moment they are resolving the whole texture.
		check(ResolveParams.SourceArrayIndex == 0);
		check(SourceTexture3D == DestTexture3D);
	}
}

/**
* Helper for storing IEEE 32 bit float components
*/
struct FFloatIEEE
{
	union
	{
		struct
		{
			uint32	Mantissa : 23, Exponent : 8, Sign : 1;
		} Components;

		float	Float;
	};
};

/**
* Helper for storing 16 bit float components
*/
struct FD3DFloat16
{
	union
	{
		struct
		{
			uint16	Mantissa : 10, Exponent : 5, Sign : 1;
		} Components;

		uint16	Encoded;
	};

	/**
	* @return full 32 bit float from the 16 bit value
	*/
	operator float()
	{
		FFloatIEEE	Result;

		Result.Components.Sign = Components.Sign;
		Result.Components.Exponent = Components.Exponent - 15 + 127; // Stored exponents are biased by half their range.
		Result.Components.Mantissa = FMath::Min<uint32>(FMath::FloorToInt((float)Components.Mantissa / 1024.0f * 8388608.0f),(1 << 23) - 1);

		return Result.Float;
	}
};

/**
* Helper for storing DXGI_FORMAT_R11G11B10_FLOAT components
*/
struct FD3DFloatR11G11B10
{
	// http://msdn.microsoft.com/En-US/library/bb173059(v=VS.85).aspx
	uint32 R_Mantissa : 6;
	uint32 R_Exponent : 5;
	uint32 G_Mantissa : 6;
	uint32 G_Exponent : 5;
	uint32 B_Mantissa : 5;
	uint32 B_Exponent : 5;

	/**
	* @return decompress into three 32 bit float
	*/
	operator FLinearColor()
	{
		FFloatIEEE	Result[3];

		Result[0].Components.Sign = 0;
		Result[0].Components.Exponent = R_Exponent - 15 + 127;
		Result[0].Components.Mantissa = FMath::Min<uint32>(FMath::FloorToInt((float)R_Mantissa / 32.0f * 8388608.0f),(1 << 23) - 1);
		Result[1].Components.Sign = 0;
		Result[1].Components.Exponent = G_Exponent - 15 + 127;
		Result[1].Components.Mantissa = FMath::Min<uint32>(FMath::FloorToInt((float)G_Mantissa / 64.0f * 8388608.0f),(1 << 23) - 1);
		Result[2].Components.Sign = 0;
		Result[2].Components.Exponent = B_Exponent - 15 + 127;
		Result[2].Components.Mantissa = FMath::Min<uint32>(FMath::FloorToInt((float)B_Mantissa / 64.0f * 8388608.0f),(1 << 23) - 1);

		return FLinearColor(Result[0].Float, Result[1].Float, Result[2].Float);
	}
};

// Only supports the formats that are supported by ConvertRAWSurfaceDataToFColor()
static uint32 ComputeBytesPerPixel(DXGI_FORMAT Format)
{
	uint32 BytesPerPixel = 0;

	switch(Format)
	{
		case DXGI_FORMAT_R16_TYPELESS:
			BytesPerPixel = 2;
			break;
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_R24G8_TYPELESS:
		// NVCHANGE_BEGIN: Add VXGI
		case DXGI_FORMAT_R10G10B10A2_TYPELESS:
		// NVCHANGE_END: Add VXGI
		case DXGI_FORMAT_R10G10B10A2_UNORM:
		case DXGI_FORMAT_R11G11B10_FLOAT:
		case DXGI_FORMAT_R16G16_UNORM:
		case DXGI_FORMAT_R32_UINT:
			BytesPerPixel = 4;
			break;
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_UNORM:
			BytesPerPixel = 8;
			break;
#if DEPTH_32_BIT_CONVERSION
		// Changing Depth Buffers to 32 bit on Dingo as D24S8 is actually implemented as a 32 bit buffer in the hardware
		case DXGI_FORMAT_R32G8X24_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
#endif
			BytesPerPixel = 5;
			break;
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
			BytesPerPixel = 16;
			break;
		case DXGI_FORMAT_R8_TYPELESS:
		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_R8_UINT:
		case DXGI_FORMAT_R8_SNORM:
		case DXGI_FORMAT_R8_SINT:
			BytesPerPixel = 1;
			break;
	}

	// format not supported yet
	check(BytesPerPixel);

	return BytesPerPixel;
}

TRefCountPtr<ID3D11Texture2D> FD3D11DynamicRHI::GetStagingTexture(FTextureRHIParamRef TextureRHI,FIntRect InRect, FIntRect& StagingRectOUT, FReadSurfaceDataFlags InFlags)
{
	FD3D11TextureBase* Texture = GetD3D11TextureFromRHITexture(TextureRHI);
	D3D11_TEXTURE2D_DESC SourceDesc; 
	((ID3D11Texture2D*)Texture->GetResource())->GetDesc(&SourceDesc);// check for 3D textures?
	
	bool bRequiresTempStagingTexture = SourceDesc.Usage != D3D11_USAGE_STAGING; 
	if(bRequiresTempStagingTexture == false)
	{
		// Returning the same texture is considerably faster than creating and copying to
		// a new staging texture as we do not have to wait for the GPU pipeline to catch up
		// to the staging texture preparation work.
		StagingRectOUT = InRect;
		return ((ID3D11Texture2D*)Texture->GetResource());
	}

	// a temporary staging texture is needed.
	int32 SizeX = InRect.Width();
	int32 SizeY = InRect.Height();
	// Read back the surface data in the defined rect
	D3D11_BOX Rect;
	Rect.left = InRect.Min.X;
	Rect.top = InRect.Min.Y;
	Rect.right = InRect.Max.X;
	Rect.bottom = InRect.Max.Y;
	Rect.back = 1;
	Rect.front = 0;

	// create a temp 2d texture to copy render target to
	D3D11_TEXTURE2D_DESC Desc;
	ZeroMemory( &Desc, sizeof( D3D11_TEXTURE2D_DESC ) );
	Desc.Width = SizeX;
	Desc.Height = SizeY;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = SourceDesc.Format;
	Desc.SampleDesc.Count = 1;
	Desc.SampleDesc.Quality = 0;
	Desc.Usage = D3D11_USAGE_STAGING;
	Desc.BindFlags = 0;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	Desc.MiscFlags = 0;
	TRefCountPtr<ID3D11Texture2D> TempTexture2D;
	VERIFYD3D11RESULT_EX(Direct3DDevice->CreateTexture2D(&Desc,NULL,TempTexture2D.GetInitReference()), Direct3DDevice);

	// Staging rectangle is now the whole surface.
	StagingRectOUT.Min = FIntPoint::ZeroValue;
	StagingRectOUT.Max = FIntPoint(SizeX,SizeY);

	// Copy the data to a staging resource.
	uint32 Subresource = 0;
	if( SourceDesc.MiscFlags == D3D11_RESOURCE_MISC_TEXTURECUBE )
	{
		uint32 D3DFace = GetD3D11CubeFace(InFlags.GetCubeFace());
		Subresource = D3D11CalcSubresource(0,D3DFace,1);
	}

	D3D11_BOX* RectPtr = NULL; // API prefers NULL for entire texture.
	if(Rect.left != 0 || Rect.top != 0 || Rect.right != SourceDesc.Width || Rect.bottom != SourceDesc.Height)
	{
		// ..Sub rectangle required, use the D3D11_BOX.
		RectPtr = &Rect;
	}

	Direct3DDeviceIMContext->CopySubresourceRegion(TempTexture2D,0,0,0,0,Texture->GetResource(),Subresource,RectPtr);

	return TempTexture2D;
}

void FD3D11DynamicRHI::ReadSurfaceDataNoMSAARaw(FTextureRHIParamRef TextureRHI,FIntRect InRect,TArray<uint8>& OutData, FReadSurfaceDataFlags InFlags)
{
	FD3D11TextureBase* Texture = GetD3D11TextureFromRHITexture(TextureRHI);

	const uint32 SizeX = InRect.Width();
	const uint32 SizeY = InRect.Height();

	// Check the format of the surface
	D3D11_TEXTURE2D_DESC TextureDesc;
	((ID3D11Texture2D*)Texture->GetResource())->GetDesc(&TextureDesc);
	
	uint32 BytesPerPixel = ComputeBytesPerPixel(TextureDesc.Format);
	
	// Allocate the output buffer.
	OutData.Empty();
	OutData.AddUninitialized(SizeX * SizeY * BytesPerPixel);

	FIntRect StagingRect;
	TRefCountPtr<ID3D11Texture2D> TempTexture2D = GetStagingTexture(TextureRHI, InRect, StagingRect, InFlags);

	// Lock the staging resource.
	D3D11_MAPPED_SUBRESOURCE LockedRect;
	VERIFYD3D11RESULT_EX(Direct3DDeviceIMContext->Map(TempTexture2D,0,D3D11_MAP_READ,0,&LockedRect), Direct3DDevice);

	uint32 BytesPerLine = BytesPerPixel * InRect.Width();
	uint8* DestPtr = OutData.GetData();
	uint8* SrcPtr = (uint8*)LockedRect.pData + StagingRect.Min.X * BytesPerPixel +  StagingRect.Min.Y * LockedRect.RowPitch;
	for(uint32 Y = 0; Y < SizeY; Y++)
	{
		memcpy(DestPtr, SrcPtr, BytesPerLine);
		DestPtr += BytesPerLine;
		SrcPtr += LockedRect.RowPitch;
	}

	Direct3DDeviceIMContext->Unmap(TempTexture2D,0);
}


/** Helper for accessing R10G10B10A2 colors. */
struct FD3DR10G10B10A2
{
	uint32 R : 10;
	uint32 G : 10;
	uint32 B : 10;
	uint32 A : 2;
};

struct FD3DR32G8
{
	uint32 R : 32;
	uint32 G : 8;
};

struct FD3DR24G8
{
	uint32 R : 24;
	uint32 G : 8;
};


/** Helper for accessing R16G16 colors. */
struct FD3DRG16
{
	uint16 R;
	uint16 G;
};

/** Helper for accessing R16G16B16A16 colors. */
struct FD3DRGBA16
{
	uint16 R;
	uint16 G;
	uint16 B;
	uint16 A;
};

// todo: this should be available for all RHI
static void ConvertRAWSurfaceDataToFColor(DXGI_FORMAT Format, uint32 Width, uint32 Height, uint8 *In, uint32 SrcPitch, FColor* Out, FReadSurfaceDataFlags InFlags)
{
	bool bLinearToGamma = InFlags.GetLinearToGamma();

	if(Format == DXGI_FORMAT_R16_TYPELESS)
	{
		// e.g. shadow maps
		for(uint32 Y = 0; Y < Height; Y++)
		{
			uint16* SrcPtr = (uint16*)(In + Y * SrcPitch);
			FColor* DestPtr = Out + Y * Width;

			for(uint32 X = 0; X < Width; X++)
			{
				uint16 Value16 = *SrcPtr;
				float Value = Value16 / (float)(0xffff);

				*DestPtr = FLinearColor(Value, Value, Value).Quantize();
				++SrcPtr;
				++DestPtr;
			}
		}
	}
	else if(Format == DXGI_FORMAT_R8G8B8A8_TYPELESS || Format == DXGI_FORMAT_R8G8B8A8_UNORM || Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
	{
		// Read the data out of the buffer, converting it from ABGR to ARGB.
		for(uint32 Y = 0; Y < Height; Y++)
		{
			FColor* SrcPtr = (FColor*)(In + Y * SrcPitch);
			FColor* DestPtr = Out + Y * Width;
			for(uint32 X = 0; X < Width; X++)
			{
				*DestPtr = FColor(SrcPtr->B,SrcPtr->G,SrcPtr->R,SrcPtr->A);
				++SrcPtr;
				++DestPtr;
			}
		}
	}
	else if(Format == DXGI_FORMAT_B8G8R8A8_TYPELESS || Format == DXGI_FORMAT_B8G8R8A8_UNORM || Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
	{
		for(uint32 Y = 0; Y < Height; Y++)
		{
			FColor* SrcPtr = (FColor*)(In + Y * SrcPitch);
			FColor* DestPtr = Out + Y * Width;

			// Need to copy row wise since the Pitch might not match the Width.
			FMemory::Memcpy(DestPtr, SrcPtr, sizeof(FColor) * Width);
		}
	}
	// NVCHANGE_BEGIN: Add VXGI
	else if(Format == DXGI_FORMAT_R10G10B10A2_TYPELESS || Format == DXGI_FORMAT_R10G10B10A2_UNORM)
	// NVCHANGE_ADD: Add VXGI
	{
		// Read the data out of the buffer, converting it from R10G10B10A2 to FColor.
		for(uint32 Y = 0; Y < Height; Y++)
		{
			FD3DR10G10B10A2* SrcPtr = (FD3DR10G10B10A2*)(In + Y * SrcPitch);
			FColor* DestPtr = Out + Y * Width;
			for(uint32 X = 0; X < Width; X++)
			{
				*DestPtr = FLinearColor(
					(float)SrcPtr->R / 1023.0f,
					(float)SrcPtr->G / 1023.0f,
					(float)SrcPtr->B / 1023.0f,
					(float)SrcPtr->A / 3.0f
					).Quantize();
				++SrcPtr;
				++DestPtr;
			}
		}
	}
	else if(Format == DXGI_FORMAT_R16G16B16A16_FLOAT)
	{
		FPlane	MinValue(0.0f,0.0f,0.0f,0.0f),
			MaxValue(1.0f,1.0f,1.0f,1.0f);

		check(sizeof(FD3DFloat16)==sizeof(uint16));

		for(uint32 Y = 0; Y < Height; Y++)
		{
			FD3DFloat16* SrcPtr = (FD3DFloat16*)(In + Y * SrcPitch);

			for(uint32 X = 0; X < Width; X++)
			{
				MinValue.X = FMath::Min<float>(SrcPtr[0],MinValue.X);
				MinValue.Y = FMath::Min<float>(SrcPtr[1],MinValue.Y);
				MinValue.Z = FMath::Min<float>(SrcPtr[2],MinValue.Z);
				MinValue.W = FMath::Min<float>(SrcPtr[3],MinValue.W);
				MaxValue.X = FMath::Max<float>(SrcPtr[0],MaxValue.X);
				MaxValue.Y = FMath::Max<float>(SrcPtr[1],MaxValue.Y);
				MaxValue.Z = FMath::Max<float>(SrcPtr[2],MaxValue.Z);
				MaxValue.W = FMath::Max<float>(SrcPtr[3],MaxValue.W);
				SrcPtr += 4;
			}
		}

		for(uint32 Y = 0; Y < Height; Y++)
		{
			FD3DFloat16* SrcPtr = (FD3DFloat16*)(In + Y * SrcPitch);
			FColor* DestPtr = Out + Y * Width;

			for(uint32 X = 0; X < Width; X++)
			{
				FColor NormalizedColor =
					FLinearColor(
					(SrcPtr[0] - MinValue.X) / (MaxValue.X - MinValue.X),
					(SrcPtr[1] - MinValue.Y) / (MaxValue.Y - MinValue.Y),
					(SrcPtr[2] - MinValue.Z) / (MaxValue.Z - MinValue.Z),
					(SrcPtr[3] - MinValue.W) / (MaxValue.W - MinValue.W)
					).ToFColor(bLinearToGamma);
				FMemory::Memcpy(DestPtr++,&NormalizedColor,sizeof(FColor));
				SrcPtr += 4;
			}
		}
	}
	else if (Format == DXGI_FORMAT_R11G11B10_FLOAT)
	{
		check(sizeof(FD3DFloatR11G11B10) == sizeof(uint32));

		for(uint32 Y = 0; Y < Height; Y++)
		{
			FD3DFloatR11G11B10* SrcPtr = (FD3DFloatR11G11B10*)(In + Y * SrcPitch);
			FColor* DestPtr = Out + Y * Width;

			for(uint32 X = 0; X < Width; X++)
			{
				FLinearColor Value = *SrcPtr;

				FColor NormalizedColor = Value.ToFColor(bLinearToGamma);
				FMemory::Memcpy(DestPtr++, &NormalizedColor, sizeof(FColor));
				++SrcPtr;
			}
		}
	}
	else if (Format == DXGI_FORMAT_R32G32B32A32_FLOAT)
	{
		FPlane MinValue(0.0f,0.0f,0.0f,0.0f);
		FPlane MaxValue(1.0f,1.0f,1.0f,1.0f);

		for(uint32 Y = 0; Y < Height; Y++)
		{
			float* SrcPtr = (float*)(In + Y * SrcPitch);

			for(uint32 X = 0; X < Width; X++)
			{
				MinValue.X = FMath::Min<float>(SrcPtr[0],MinValue.X);
				MinValue.Y = FMath::Min<float>(SrcPtr[1],MinValue.Y);
				MinValue.Z = FMath::Min<float>(SrcPtr[2],MinValue.Z);
				MinValue.W = FMath::Min<float>(SrcPtr[3],MinValue.W);
				MaxValue.X = FMath::Max<float>(SrcPtr[0],MaxValue.X);
				MaxValue.Y = FMath::Max<float>(SrcPtr[1],MaxValue.Y);
				MaxValue.Z = FMath::Max<float>(SrcPtr[2],MaxValue.Z);
				MaxValue.W = FMath::Max<float>(SrcPtr[3],MaxValue.W);
				SrcPtr += 4;
			}
		}
		
		for(uint32 Y = 0; Y < Height; Y++)
		{
			float* SrcPtr = (float*)In;
			FColor* DestPtr = Out + Y * Width;

			for(uint32 X = 0; X < Width; X++)
			{
				FColor NormalizedColor =
					FLinearColor(
					(SrcPtr[0] - MinValue.X) / (MaxValue.X - MinValue.X),
					(SrcPtr[1] - MinValue.Y) / (MaxValue.Y - MinValue.Y),
					(SrcPtr[2] - MinValue.Z) / (MaxValue.Z - MinValue.Z),
					(SrcPtr[3] - MinValue.W) / (MaxValue.W - MinValue.W)
					).ToFColor(bLinearToGamma);
				FMemory::Memcpy(DestPtr++,&NormalizedColor,sizeof(FColor));
				SrcPtr += 4;
			}
		}
	}
	else if (Format == DXGI_FORMAT_R24G8_TYPELESS)
	{
		// Depth stencil
		for(uint32 Y = 0; Y < Height; Y++)
		{
			uint32* SrcPtr = (uint32 *)In;
			FColor* DestPtr = Out + Y * Width;
			
			for(uint32 X = 0; X < Width; X++)
			{
				FColor NormalizedColor;
				if (InFlags.GetOutputStencil())
				{
					uint8 DeviceStencil = (*SrcPtr & 0xFF000000) >> 24;
					NormalizedColor = FColor(DeviceStencil, DeviceStencil, DeviceStencil, 0xFF);
				}
				else
				{
					float DeviceZ = (*SrcPtr & 0xffffff) / (float)(1<<24);
					float LinearValue = FMath::Min(InFlags.ComputeNormalizedDepth(DeviceZ), 1.0f);
					NormalizedColor = FLinearColor(LinearValue, LinearValue, LinearValue, 0).ToFColor(bLinearToGamma);
				}
								
				FMemory::Memcpy(DestPtr++, &NormalizedColor, sizeof(FColor));
				++SrcPtr;
			}
		}
	}
#if DEPTH_32_BIT_CONVERSION
	// Changing Depth Buffers to 32 bit on Dingo as D24S8 is actually implemented as a 32 bit buffer in the hardware
	else if (Format == DXGI_FORMAT_R32G8X24_TYPELESS )
	{
		// Depth stencil
		for(uint32 Y = 0; Y < Height; Y++)
		{
			float* SrcPtr = (float *)(In + Y * SrcPitch);
			FColor* DestPtr = Out + Y * Width;

			for(uint32 X = 0; X < Width; X++)
			{
				float DeviceZ = (*SrcPtr);

				float LinearValue = FMath::Min(InFlags.ComputeNormalizedDepth(DeviceZ), 1.0f);

				FColor NormalizedColor = FLinearColor(LinearValue, LinearValue, LinearValue, 0).ToFColor(bLinearToGamma);
				FMemory::Memcpy(DestPtr++, &NormalizedColor, sizeof(FColor));
				SrcPtr+=1; // todo: copies only depth, need to check how this format is read
				UE_LOG(LogD3D11RHI, Warning, TEXT("CPU read of R32G8X24 is not tested and may not function."));
			}
		}
	}
#endif
	else if(Format == DXGI_FORMAT_R16G16B16A16_UNORM)
	{
		// Read the data out of the buffer, converting it to FColor.
		for(uint32 Y = 0; Y < Height; Y++)
		{
			FD3DRGBA16* SrcPtr = (FD3DRGBA16*)(In + Y * SrcPitch);
			FColor* DestPtr = Out + Y * Width;
			for(uint32 X = 0; X < Width; X++)
			{
				*DestPtr = FLinearColor(
					(float)SrcPtr->R / 65535.0f,
					(float)SrcPtr->G / 65535.0f,
					(float)SrcPtr->B / 65535.0f,
					(float)SrcPtr->A / 65535.0f
					).Quantize();
				++SrcPtr;
				++DestPtr;
			}
		}
	}
	else if(Format == DXGI_FORMAT_R16G16_UNORM)
	{
		// Read the data out of the buffer, converting it to FColor.
		for(uint32 Y = 0; Y < Height; Y++)
		{
			FD3DRG16* SrcPtr = (FD3DRG16*)(In + Y * SrcPitch);
			FColor* DestPtr = Out + Y * Width;
			for(uint32 X = 0; X < Width; X++)
			{
				*DestPtr = FLinearColor(
					(float)SrcPtr->R / 65535.0f,
					(float)SrcPtr->G / 65535.0f,
					0).Quantize();
				++SrcPtr;
				++DestPtr;
			}
		}
	}
	else
	{
		// not supported yet
		check(0);
	}
}

void FD3D11DynamicRHI::RHIReadSurfaceData(FTextureRHIParamRef TextureRHI,FIntRect InRect,TArray<FColor>& OutData, FReadSurfaceDataFlags InFlags)
{
	if (!ensure(TextureRHI))
	{
		OutData.Empty();
		OutData.AddZeroed(InRect.Width() * InRect.Height());
		return;
	}

	TArray<uint8> OutDataRaw;

	FD3D11TextureBase* Texture = GetD3D11TextureFromRHITexture(TextureRHI);

	// Check the format of the surface
	D3D11_TEXTURE2D_DESC TextureDesc;

	((ID3D11Texture2D*)Texture->GetResource())->GetDesc(&TextureDesc);

	check(TextureDesc.SampleDesc.Count >= 1);

	if(TextureDesc.SampleDesc.Count == 1)
	{
		ReadSurfaceDataNoMSAARaw(TextureRHI, InRect, OutDataRaw, InFlags);
	}
	else
	{
		FRHICommandList_RecursiveHazardous RHICmdList(this);
		ReadSurfaceDataMSAARaw(RHICmdList, TextureRHI, InRect, OutDataRaw, InFlags);
	}

	const uint32 SizeX = InRect.Width() * TextureDesc.SampleDesc.Count;
	const uint32 SizeY = InRect.Height();

	// Allocate the output buffer.
	OutData.Empty();
	OutData.AddUninitialized(SizeX * SizeY);

	uint32 BytesPerPixel = ComputeBytesPerPixel(TextureDesc.Format);
	uint32 SrcPitch = SizeX * BytesPerPixel;

	ConvertRAWSurfaceDataToFColor(TextureDesc.Format, SizeX, SizeY, OutDataRaw.GetData(), SrcPitch, OutData.GetData(), InFlags);
}

void FD3D11DynamicRHI::ReadSurfaceDataMSAARaw(FRHICommandList_RecursiveHazardous& RHICmdList, FTextureRHIParamRef TextureRHI,FIntRect InRect,TArray<uint8>& OutData, FReadSurfaceDataFlags InFlags)
{
	FD3D11TextureBase* Texture = GetD3D11TextureFromRHITexture(TextureRHI);

	const uint32 SizeX = InRect.Width();
	const uint32 SizeY = InRect.Height();
	
	// Check the format of the surface
	D3D11_TEXTURE2D_DESC TextureDesc;
	((ID3D11Texture2D*)Texture->GetResource())->GetDesc(&TextureDesc);

	uint32 BytesPerPixel = ComputeBytesPerPixel(TextureDesc.Format);

	const uint32 NumSamples = TextureDesc.SampleDesc.Count;

	// Read back the surface data from the define rect
	D3D11_BOX	Rect;
	Rect.left	= InRect.Min.X;
	Rect.top	= InRect.Min.Y;
	Rect.right	= InRect.Max.X;
	Rect.bottom	= InRect.Max.Y;
	Rect.back = 1;
	Rect.front = 0;

	// Create a non-MSAA render target to resolve individual samples of the source surface to.
	D3D11_TEXTURE2D_DESC NonMSAADesc;
	ZeroMemory( &NonMSAADesc, sizeof( D3D11_TEXTURE2D_DESC ) );
	NonMSAADesc.Width = SizeX;
	NonMSAADesc.Height = SizeY;
	NonMSAADesc.MipLevels = 1;
	NonMSAADesc.ArraySize = 1;
	NonMSAADesc.Format = TextureDesc.Format;
	NonMSAADesc.SampleDesc.Count = 1;
	NonMSAADesc.SampleDesc.Quality = 0;
	NonMSAADesc.Usage = D3D11_USAGE_DEFAULT;
	NonMSAADesc.BindFlags = D3D11_BIND_RENDER_TARGET;
	NonMSAADesc.CPUAccessFlags = 0;
	NonMSAADesc.MiscFlags = 0;
	TRefCountPtr<ID3D11Texture2D> NonMSAATexture2D;
	VERIFYD3D11RESULT_EX(Direct3DDevice->CreateTexture2D(&NonMSAADesc,NULL,NonMSAATexture2D.GetInitReference()), Direct3DDevice);

	TRefCountPtr<ID3D11RenderTargetView> NonMSAARTV;
	D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
	FMemory::Memset(&RTVDesc,0,sizeof(RTVDesc));

	// typeless is not supported, similar code might be needed for other typeless formats
	RTVDesc.Format = ConvertTypelessToUnorm(NonMSAADesc.Format);

	RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	RTVDesc.Texture2D.MipSlice = 0;
	VERIFYD3D11RESULT_EX(Direct3DDevice->CreateRenderTargetView(NonMSAATexture2D,&RTVDesc,NonMSAARTV.GetInitReference()), Direct3DDevice);

	// Create a CPU-accessible staging texture to copy the resolved sample data to.
	TRefCountPtr<ID3D11Texture2D> StagingTexture2D;
	D3D11_TEXTURE2D_DESC StagingDesc;
	ZeroMemory( &StagingDesc, sizeof( D3D11_TEXTURE2D_DESC ) );
	StagingDesc.Width = SizeX;
	StagingDesc.Height = SizeY;
	StagingDesc.MipLevels = 1;
	StagingDesc.ArraySize = 1;
	StagingDesc.Format = TextureDesc.Format;
	StagingDesc.SampleDesc.Count = 1;
	StagingDesc.SampleDesc.Quality = 0;
	StagingDesc.Usage = D3D11_USAGE_STAGING;
	StagingDesc.BindFlags = 0;
	StagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	StagingDesc.MiscFlags = 0;
	VERIFYD3D11RESULT_EX(Direct3DDevice->CreateTexture2D(&StagingDesc,NULL,StagingTexture2D.GetInitReference()), Direct3DDevice);

	// Determine the subresource index for cubemaps.
	uint32 Subresource = 0;
	if( TextureDesc.MiscFlags == D3D11_RESOURCE_MISC_TEXTURECUBE )
	{
		uint32 D3DFace = GetD3D11CubeFace(InFlags.GetCubeFace());
		Subresource = D3D11CalcSubresource(0,D3DFace,1);
	}
	
	// Allocate the output buffer.
	OutData.Empty();
	OutData.AddUninitialized(SizeX * SizeY * NumSamples * BytesPerPixel);

	// Can be optimized by doing all subsamples into a large enough rendertarget in one pass (multiple draw calls)
	for(uint32 SampleIndex = 0;SampleIndex < NumSamples;++SampleIndex)
	{
		// Resolve the sample to the non-MSAA render target.
		ResolveTextureUsingShader<FResolveSingleSamplePS>(
			RHICmdList,
			(FD3D11Texture2D*)TextureRHI->GetTexture2D(),
			NULL,
			NonMSAARTV,
			NULL,
			NonMSAADesc,
			FResolveRect(InRect.Min.X, InRect.Min.Y, InRect.Max.X, InRect.Max.Y),
			FResolveRect(0,0,SizeX,SizeY),
			Direct3DDeviceIMContext,
			SampleIndex
			);

		// Copy the resolved sample data to the staging texture.
		Direct3DDeviceIMContext->CopySubresourceRegion(StagingTexture2D,0,0,0,0,NonMSAATexture2D,Subresource,&Rect);

		// Lock the staging texture.
		D3D11_MAPPED_SUBRESOURCE LockedRect;
		VERIFYD3D11RESULT_EX(Direct3DDeviceIMContext->Map(StagingTexture2D,0,D3D11_MAP_READ,0,&LockedRect), Direct3DDevice);

		// Read the data out of the buffer, could be optimized
		for(int32 Y = InRect.Min.Y; Y < InRect.Max.Y; Y++)
		{
			uint8* SrcPtr = (uint8*)LockedRect.pData + (Y - InRect.Min.Y) * LockedRect.RowPitch + InRect.Min.X * BytesPerPixel;
			uint8* DestPtr = &OutData[(Y - InRect.Min.Y) * SizeX * NumSamples * BytesPerPixel + SampleIndex * BytesPerPixel];

			for(int32 X = InRect.Min.X; X < InRect.Max.X; X++)
			{
				for(uint32 i = 0; i < BytesPerPixel; ++i)
				{
					*DestPtr++ = *SrcPtr++;
				}

				DestPtr += (NumSamples - 1) * BytesPerPixel;
			}
		}

		Direct3DDeviceIMContext->Unmap(StagingTexture2D,0);
	}
}

void FD3D11DynamicRHI::RHIMapStagingSurface(FTextureRHIParamRef TextureRHI,void*& OutData,int32& OutWidth,int32& OutHeight)
{
	ID3D11Texture2D* Texture = (ID3D11Texture2D*)(GetD3D11TextureFromRHITexture(TextureRHI)->GetResource());
	
	D3D11_TEXTURE2D_DESC TextureDesc;
	Texture->GetDesc(&TextureDesc);
	uint32 BytesPerPixel = ComputeBytesPerPixel(TextureDesc.Format);

	D3D11_MAPPED_SUBRESOURCE LockedRect;
	VERIFYD3D11RESULT_EX(Direct3DDeviceIMContext->Map(Texture,0,D3D11_MAP_READ,0,&LockedRect), Direct3DDevice);

	OutData = LockedRect.pData;
	OutWidth = LockedRect.RowPitch / BytesPerPixel;
	OutHeight = LockedRect.DepthPitch / LockedRect.RowPitch;

	check(OutData);
}

void FD3D11DynamicRHI::RHIUnmapStagingSurface(FTextureRHIParamRef TextureRHI)
{
	ID3D11Texture2D* Texture = (ID3D11Texture2D*)(GetD3D11TextureFromRHITexture(TextureRHI)->GetResource());

	Direct3DDeviceIMContext->Unmap(Texture,0);
}

void FD3D11DynamicRHI::RHIReadSurfaceFloatData(FTextureRHIParamRef TextureRHI,FIntRect InRect,TArray<FFloat16Color>& OutData,ECubeFace CubeFace,int32 ArrayIndex,int32 MipIndex)
{
	FD3D11TextureBase* Texture = GetD3D11TextureFromRHITexture(TextureRHI);

	uint32 SizeX = InRect.Width();
	uint32 SizeY = InRect.Height();

	// Check the format of the surface
	D3D11_TEXTURE2D_DESC TextureDesc;
	((ID3D11Texture2D*)Texture->GetResource())->GetDesc(&TextureDesc);

	check(TextureDesc.Format == GPixelFormats[PF_FloatRGBA].PlatformFormat);

	// Allocate the output buffer.
	OutData.Empty(SizeX * SizeY);

	// Read back the surface data from defined rect
	D3D11_BOX	Rect;
	Rect.left	= InRect.Min.X;
	Rect.top	= InRect.Min.Y;
	Rect.right	= InRect.Max.X;
	Rect.bottom	= InRect.Max.Y;
	Rect.back = 1;
	Rect.front = 0;

	// create a temp 2d texture to copy render target to
	D3D11_TEXTURE2D_DESC Desc;
	ZeroMemory( &Desc, sizeof( D3D11_TEXTURE2D_DESC ) );
	Desc.Width = SizeX;
	Desc.Height = SizeY;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = TextureDesc.Format;
	Desc.SampleDesc.Count = 1;
	Desc.SampleDesc.Quality = 0;
	Desc.Usage = D3D11_USAGE_STAGING;
	Desc.BindFlags = 0;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	Desc.MiscFlags = 0;
	TRefCountPtr<ID3D11Texture2D> TempTexture2D;
	VERIFYD3D11RESULT_EX(Direct3DDevice->CreateTexture2D(&Desc,NULL,TempTexture2D.GetInitReference()), Direct3DDevice);

	// Copy the data to a staging resource.
	uint32 Subresource = 0;
	if( TextureDesc.MiscFlags == D3D11_RESOURCE_MISC_TEXTURECUBE )
	{
		uint32 D3DFace = GetD3D11CubeFace(CubeFace);
		Subresource = D3D11CalcSubresource(MipIndex, ArrayIndex * 6 + D3DFace, TextureDesc.MipLevels);
	}
	Direct3DDeviceIMContext->CopySubresourceRegion(TempTexture2D,0,0,0,0,Texture->GetResource(),Subresource,&Rect);

	// Lock the staging resource.
	D3D11_MAPPED_SUBRESOURCE LockedRect;
	VERIFYD3D11RESULT_EX(Direct3DDeviceIMContext->Map(TempTexture2D,0,D3D11_MAP_READ,0,&LockedRect), Direct3DDevice);

	// Presize the array
	int32 TotalCount = SizeX * SizeY;
	if (TotalCount >= OutData.Num())
	{
		OutData.AddZeroed(TotalCount);
	}

	for(int32 Y = InRect.Min.Y; Y < InRect.Max.Y; Y++)
	{
		FFloat16Color* SrcPtr = (FFloat16Color*)((uint8*)LockedRect.pData + (Y - InRect.Min.Y) * LockedRect.RowPitch);
		int32 Index = (Y - InRect.Min.Y) * SizeX;
		check(Index + ((int32)SizeX - 1) < OutData.Num());
		FFloat16Color* DestColor = &OutData[Index];
		FFloat16* DestPtr = (FFloat16*)(DestColor);
		FMemory::Memcpy(DestPtr,SrcPtr,SizeX * sizeof(FFloat16) * 4);
	}

	Direct3DDeviceIMContext->Unmap(TempTexture2D,0);
}

static void ConvertRAWSurfaceDataToFLinearColor(EPixelFormat Format, uint32 Width, uint32 Height, uint8 *In, uint32 SrcPitch, FLinearColor* Out, FReadSurfaceDataFlags InFlags)
{
	if (Format == PF_R16F || Format == PF_R16F_FILTER)
	{
		// e.g. shadow maps
		for (uint32 Y = 0; Y < Height; Y++)
		{
			uint16* SrcPtr = (uint16*)(In + Y * SrcPitch);
			FLinearColor* DestPtr = Out + Y * Width;

			for (uint32 X = 0; X < Width; X++)
			{
				uint16 Value16 = *SrcPtr;
				float Value = Value16 / (float)(0xffff);

				*DestPtr = FLinearColor(Value, Value, Value);
				++SrcPtr;
				++DestPtr;
			}
		}
	}
	else if (Format == PF_R8G8B8A8)
	{
		// Read the data out of the buffer, converting it from ABGR to ARGB.
		for (uint32 Y = 0; Y < Height; Y++)
		{
			FColor* SrcPtr = (FColor*)(In + Y * SrcPitch);
			FLinearColor* DestPtr = Out + Y * Width;
			for (uint32 X = 0; X < Width; X++)
			{
				FColor sRGBColor = FColor(SrcPtr->B, SrcPtr->G, SrcPtr->R, SrcPtr->A);
				*DestPtr = FLinearColor(sRGBColor);
				++SrcPtr;
				++DestPtr;
			}
		}
	}
	else if (Format == PF_B8G8R8A8)
	{
		for (uint32 Y = 0; Y < Height; Y++)
		{
			FColor* SrcPtr = (FColor*)(In + Y * SrcPitch);
			FLinearColor* DestPtr = Out + Y * Width;
			for (uint32 X = 0; X < Width; X++)
			{
				FColor sRGBColor = FColor(SrcPtr->R, SrcPtr->G, SrcPtr->B, SrcPtr->A);
				*DestPtr = FLinearColor(sRGBColor);
				++SrcPtr;
				++DestPtr;
			}
		}
	}
	else if (Format == PF_A2B10G10R10)
	{
		// Read the data out of the buffer, converting it from R10G10B10A2 to FLinearColor.
		for (uint32 Y = 0; Y < Height; Y++)
		{
			FD3DR10G10B10A2* SrcPtr = (FD3DR10G10B10A2*)(In + Y * SrcPitch);
			FLinearColor* DestPtr = Out + Y * Width;
			for (uint32 X = 0; X < Width; X++)
			{
				*DestPtr = FLinearColor(
					(float)SrcPtr->R / 1023.0f,
					(float)SrcPtr->G / 1023.0f,
					(float)SrcPtr->B / 1023.0f,
					(float)SrcPtr->A / 3.0f
					);
				++SrcPtr;
				++DestPtr;
			}
		}
	}
	else if (Format == PF_FloatRGBA)
	{
		if (InFlags.GetCompressionMode() == RCM_MinMax)
		{
			for (uint32 Y = 0; Y < Height; Y++)
			{
				FD3DFloat16* SrcPtr = (FD3DFloat16*)(In + Y * SrcPitch);
				FLinearColor* DestPtr = Out + Y * Width;

				for (uint32 X = 0; X < Width; X++)
				{
					*DestPtr = FLinearColor((float)SrcPtr[0], (float)SrcPtr[1], (float)SrcPtr[2], (float)SrcPtr[3]);
					++DestPtr;
					SrcPtr += 4;
				}
			}
		}
		else
		{
			FPlane	MinValue(0.0f, 0.0f, 0.0f, 0.0f);
			FPlane	MaxValue(1.0f, 1.0f, 1.0f, 1.0f);

			check(sizeof(FD3DFloat16) == sizeof(uint16));

			for (uint32 Y = 0; Y < Height; Y++)
			{
				FD3DFloat16* SrcPtr = (FD3DFloat16*)(In + Y * SrcPitch);

				for (uint32 X = 0; X < Width; X++)
				{
					MinValue.X = FMath::Min<float>(SrcPtr[0], MinValue.X);
					MinValue.Y = FMath::Min<float>(SrcPtr[1], MinValue.Y);
					MinValue.Z = FMath::Min<float>(SrcPtr[2], MinValue.Z);
					MinValue.W = FMath::Min<float>(SrcPtr[3], MinValue.W);
					MaxValue.X = FMath::Max<float>(SrcPtr[0], MaxValue.X);
					MaxValue.Y = FMath::Max<float>(SrcPtr[1], MaxValue.Y);
					MaxValue.Z = FMath::Max<float>(SrcPtr[2], MaxValue.Z);
					MaxValue.W = FMath::Max<float>(SrcPtr[3], MaxValue.W);
					SrcPtr += 4;
				}
			}

			for (uint32 Y = 0; Y < Height; Y++)
			{
				FD3DFloat16* SrcPtr = (FD3DFloat16*)(In + Y * SrcPitch);
				FLinearColor* DestPtr = Out + Y * Width;

				for (uint32 X = 0; X < Width; X++)
				{
					*DestPtr = FLinearColor(
						(SrcPtr[0] - MinValue.X) / (MaxValue.X - MinValue.X),
						(SrcPtr[1] - MinValue.Y) / (MaxValue.Y - MinValue.Y),
						(SrcPtr[2] - MinValue.Z) / (MaxValue.Z - MinValue.Z),
						(SrcPtr[3] - MinValue.W) / (MaxValue.W - MinValue.W)
						);
					++DestPtr;
					SrcPtr += 4;
				}
			}
		}
	}
	else if (Format == PF_FloatRGB || Format == PF_FloatR11G11B10)
	{
		check(sizeof(FD3DFloatR11G11B10) == sizeof(uint32));

		for (uint32 Y = 0; Y < Height; Y++)
		{
			FD3DFloatR11G11B10* SrcPtr = (FD3DFloatR11G11B10*)(In + Y * SrcPitch);
			FLinearColor* DestPtr = Out + Y * Width;

			for (uint32 X = 0; X < Width; X++)
			{
				*DestPtr = *SrcPtr;
				++DestPtr;
				++SrcPtr;
			}
		}
	}
	else if (Format == PF_A32B32G32R32F)
	{
		if (InFlags.GetCompressionMode() == RCM_MinMax)
		{
			// Copy data directly, respecting existing min-max values
			FLinearColor* SrcPtr = (FLinearColor*)In;
			FLinearColor* DestPtr = (FLinearColor*)Out;
			const int32 ImageSize = sizeof(FLinearColor) * Height * Width;

			FMemory::Memcpy(DestPtr, SrcPtr, ImageSize);
		}
		else
		{
			// Normalize data
			FPlane MinValue(0.0f, 0.0f, 0.0f, 0.0f);
			FPlane MaxValue(1.0f, 1.0f, 1.0f, 1.0f);

			for (uint32 Y = 0; Y < Height; Y++)
			{
				float* SrcPtr = (float*)(In + Y * SrcPitch);

				for (uint32 X = 0; X < Width; X++)
				{
					MinValue.X = FMath::Min<float>(SrcPtr[0], MinValue.X);
					MinValue.Y = FMath::Min<float>(SrcPtr[1], MinValue.Y);
					MinValue.Z = FMath::Min<float>(SrcPtr[2], MinValue.Z);
					MinValue.W = FMath::Min<float>(SrcPtr[3], MinValue.W);
					MaxValue.X = FMath::Max<float>(SrcPtr[0], MaxValue.X);
					MaxValue.Y = FMath::Max<float>(SrcPtr[1], MaxValue.Y);
					MaxValue.Z = FMath::Max<float>(SrcPtr[2], MaxValue.Z);
					MaxValue.W = FMath::Max<float>(SrcPtr[3], MaxValue.W);
					SrcPtr += 4;
				} 
			}

			float* SrcPtr = (float*)In;

			for (uint32 Y = 0; Y < Height; Y++)
			{
				FLinearColor* DestPtr = Out + Y * Width;

				for (uint32 X = 0; X < Width; X++)
				{
					*DestPtr = FLinearColor(
							(SrcPtr[0] - MinValue.X) / (MaxValue.X - MinValue.X),
							(SrcPtr[1] - MinValue.Y) / (MaxValue.Y - MinValue.Y),
							(SrcPtr[2] - MinValue.Z) / (MaxValue.Z - MinValue.Z),
							(SrcPtr[3] - MinValue.W) / (MaxValue.W - MinValue.W)
							);
					++DestPtr;
					SrcPtr += 4;
				}
			}
		}
	}
	else if (Format == PF_DepthStencil || Format == PF_D24)
	{
		// Depth stencil
		for (uint32 Y = 0; Y < Height; Y++)
		{
			uint32* SrcPtr = (uint32 *)In;
			FLinearColor* DestPtr = Out + Y * Width;

			for (uint32 X = 0; X < Width; X++)
			{
				float DeviceStencil = 0.0f;
				DeviceStencil = (float)((*SrcPtr & 0xFF000000) >> 24)/255.0f;
				float DeviceZ = (*SrcPtr & 0xffffff) / (float)(1 << 24);
				float LinearValue = FMath::Min(InFlags.ComputeNormalizedDepth(DeviceZ), 1.0f);
				*DestPtr = FLinearColor(LinearValue, DeviceStencil, 0.0f, 0.0f);
				++DestPtr;
				++SrcPtr;
			}
		}
	}
#if DEPTH_32_BIT_CONVERSION
	// Changing Depth Buffers to 32 bit on Dingo as D24S8 is actually implemented as a 32 bit buffer in the hardware
	else if (Format == PF_DepthStencil)
	{
		// Depth stencil
		for (uint32 Y = 0; Y < Height; Y++)
		{
			uint8* SrcStart = (uint8 *)(In + Y * SrcPitch);
			FLinearColor* DestPtr = Out + Y * Width;

			for (uint32 X = 0; X < Width; X++)
			{
				float DeviceZ = *((float *)(SrcStart));
				float LinearValue = FMath::Min(InFlags.ComputeNormalizedDepth(DeviceZ), 1.0f);
				float DeviceStencil = (float)(*(SrcStart+4)) / 255.0f;
				*DestPtr = FLinearColor(LinearValue, DeviceStencil, 0.0f, 0.0f);
				SrcStart += 8; //64 bit format with the last 24 bit ignore
			}
		}
	}
#endif
	else if (Format == PF_A16B16G16R16)
	{
		// Read the data out of the buffer, converting it to FLinearColor.
		for (uint32 Y = 0; Y < Height; Y++)
		{
			FD3DRGBA16* SrcPtr = (FD3DRGBA16*)(In + Y * SrcPitch);
			FLinearColor* DestPtr = Out + Y * Width;
			for (uint32 X = 0; X < Width; X++)
			{
				*DestPtr = FLinearColor(
					(float)SrcPtr->R / 65535.0f,
					(float)SrcPtr->G / 65535.0f,
					(float)SrcPtr->B / 65535.0f,
					(float)SrcPtr->A / 65535.0f
					);
				++SrcPtr;
				++DestPtr;
			}
		}
	}
	else if (Format == PF_G16R16)
	{
		// Read the data out of the buffer, converting it to FLinearColor.
		for (uint32 Y = 0; Y < Height; Y++)
		{
			FD3DRG16* SrcPtr = (FD3DRG16*)(In + Y * SrcPitch);
			FLinearColor* DestPtr = Out + Y * Width;
			for (uint32 X = 0; X < Width; X++)
			{
				*DestPtr = FLinearColor(
					(float)SrcPtr->R / 65535.0f,
					(float)SrcPtr->G / 65535.0f,
					0);
				++SrcPtr;
				++DestPtr;
			}
		}
	}
	else
	{
		// not supported yet
		check(0);
	}
}

void FD3D11DynamicRHI::RHIReadSurfaceData(FTextureRHIParamRef TextureRHI, FIntRect InRect, TArray<FLinearColor>& OutData, FReadSurfaceDataFlags InFlags)
{
	TArray<uint8> OutDataRaw;

	FD3D11TextureBase* Texture = GetD3D11TextureFromRHITexture(TextureRHI);

	// Check the format of the surface
	D3D11_TEXTURE2D_DESC TextureDesc;

	((ID3D11Texture2D*)Texture->GetResource())->GetDesc(&TextureDesc);

	check(TextureDesc.SampleDesc.Count >= 1);

	if (TextureDesc.SampleDesc.Count == 1)
	{
		ReadSurfaceDataNoMSAARaw(TextureRHI, InRect, OutDataRaw, InFlags);
	}
	else
	{
		FRHICommandList_RecursiveHazardous RHICmdList(this);
		ReadSurfaceDataMSAARaw(RHICmdList, TextureRHI, InRect, OutDataRaw, InFlags);
	}

	const uint32 SizeX = InRect.Width() * TextureDesc.SampleDesc.Count;
	const uint32 SizeY = InRect.Height();

	// Allocate the output buffer.
	OutData.Empty();
	OutData.AddUninitialized(SizeX * SizeY);

	uint32 BytesPerPixel = ComputeBytesPerPixel(TextureDesc.Format);
	uint32 SrcPitch = SizeX * BytesPerPixel;
	EPixelFormat Format = TextureRHI->GetFormat();
	if (Format != PF_Unknown)
	{
		ConvertRAWSurfaceDataToFLinearColor(Format, SizeX, SizeY, OutDataRaw.GetData(), SrcPitch, OutData.GetData(), InFlags);
	}
}

void FD3D11DynamicRHI::RHIRead3DSurfaceFloatData(FTextureRHIParamRef TextureRHI,FIntRect InRect,FIntPoint ZMinMax,TArray<FFloat16Color>& OutData)
{
	FD3D11TextureBase* Texture = GetD3D11TextureFromRHITexture(TextureRHI);

	uint32 SizeX = InRect.Width();
	uint32 SizeY = InRect.Height();
	uint32 SizeZ = ZMinMax.Y - ZMinMax.X;

	// Check the format of the surface
	D3D11_TEXTURE3D_DESC TextureDesc;
	((ID3D11Texture3D*)Texture->GetResource())->GetDesc(&TextureDesc);

	check(TextureDesc.Format == GPixelFormats[PF_FloatRGBA].PlatformFormat);

	// Allocate the output buffer.
	OutData.Empty(SizeX * SizeY * SizeZ * sizeof(FFloat16Color));

	// Read back the surface data from defined rect
	D3D11_BOX	Rect;
	Rect.left	= InRect.Min.X;
	Rect.top	= InRect.Min.Y;
	Rect.right	= InRect.Max.X;
	Rect.bottom	= InRect.Max.Y;
	Rect.back = ZMinMax.Y;
	Rect.front = ZMinMax.X;

	// create a temp 2d texture to copy render target to
	D3D11_TEXTURE3D_DESC Desc;
	ZeroMemory( &Desc, sizeof( D3D11_TEXTURE3D_DESC ) );
	Desc.Width = SizeX;
	Desc.Height = SizeY;
	Desc.Depth = SizeZ;
	Desc.MipLevels = 1;
	Desc.Format = TextureDesc.Format;
	Desc.Usage = D3D11_USAGE_STAGING;
	Desc.BindFlags = 0;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	Desc.MiscFlags = 0;
	TRefCountPtr<ID3D11Texture3D> TempTexture3D;
	VERIFYD3D11RESULT_EX(Direct3DDevice->CreateTexture3D(&Desc,NULL,TempTexture3D.GetInitReference()), Direct3DDevice);

	// Copy the data to a staging resource.
	uint32 Subresource = 0;
	Direct3DDeviceIMContext->CopySubresourceRegion(TempTexture3D,0,0,0,0,Texture->GetResource(),Subresource,&Rect);

	// Lock the staging resource.
	D3D11_MAPPED_SUBRESOURCE LockedRect;
	VERIFYD3D11RESULT_EX(Direct3DDeviceIMContext->Map(TempTexture3D,0,D3D11_MAP_READ,0,&LockedRect), Direct3DDevice);

	// Presize the array
	int32 TotalCount = SizeX * SizeY * SizeZ;
	if (TotalCount >= OutData.Num())
	{
		OutData.AddZeroed(TotalCount);
	}

	// Read the data out of the buffer, converting it from ABGR to ARGB.
	for (int32 Z = ZMinMax.X; Z < ZMinMax.Y; ++Z)
	{
		for(int32 Y = InRect.Min.Y; Y < InRect.Max.Y; ++Y)
		{
			FFloat16Color* SrcPtr = (FFloat16Color*)((uint8*)LockedRect.pData + (Y - InRect.Min.Y) * LockedRect.RowPitch + (Z - ZMinMax.X) * LockedRect.DepthPitch);
			int32 Index = (Y - InRect.Min.Y) * SizeX + (Z - ZMinMax.X) * SizeX * SizeY;
			check(Index < OutData.Num());
			FFloat16Color* DestColor = &OutData[Index];
			FFloat16* DestPtr = (FFloat16*)(DestColor);
			FMemory::Memcpy(DestPtr,SrcPtr,SizeX * sizeof(FFloat16) * 4);
		}
	}

	Direct3DDeviceIMContext->Unmap(TempTexture3D,0);
}
