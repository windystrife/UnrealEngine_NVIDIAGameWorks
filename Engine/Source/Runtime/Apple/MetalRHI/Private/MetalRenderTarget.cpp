// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalRenderTarget.cpp: Metal render target implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "ScreenRendering.h"
#include "MetalProfiler.h"
#include "MetalCommandBuffer.h"
#include "ResolveShader.h"
#include "PipelineStateCache.h"

static FResolveRect GetDefaultRect(const FResolveRect& Rect, uint32 DefaultWidth, uint32 DefaultHeight)
{
	if (Rect.X1 >= 0 && Rect.X2 >= 0 && Rect.Y1 >= 0 && Rect.Y2 >= 0)
	{
		return Rect;
	}
	else
	{
		return FResolveRect(0, 0, DefaultWidth, DefaultHeight);
	}
}

int32 GMetalUseTexGetBytes = 1;
static FAutoConsoleVariableRef CVarMetalUseTexGetBytes(
								TEXT("rhi.Metal.UseTexGetBytes"),
								GMetalUseTexGetBytes,
								TEXT("If true prefer using -[MTLTexture getBytes:...] to retreive texture data, creating a temporary shared/managed texture to copy from private texture storage when required, rather than using a temporary MTLBuffer. This works around data alignment bugs on some GPU vendor's drivers and may be more appropriate on iOS. (Default: True)"),
								ECVF_RenderThreadSafe
								);

void FMetalRHICommandContext::RHICopyToResolveTarget(FTextureRHIParamRef SourceTextureRHI, FTextureRHIParamRef DestTextureRHI, bool bKeepOriginalSurface, const FResolveParams& ResolveParams)
{
	@autoreleasepool {
	if (!SourceTextureRHI || !DestTextureRHI)
	{
		// nothing to do if one of the textures is null!
		return;
	}
	if(SourceTextureRHI != DestTextureRHI)
	{
		FMetalSurface* Source = GetMetalSurfaceFromRHITexture(SourceTextureRHI);
		FMetalSurface* Destination = GetMetalSurfaceFromRHITexture(DestTextureRHI);
		
		switch (Source->Type)
		{
			case RRT_Texture2D:
				break;
			case RRT_TextureCube:
				check(Source->SizeZ == 6); // Arrays might not work yet.
				break;
			default:
				check(false); // Only Tex2D & Cube are tested to work so far!
				break;
		}
		switch (Destination->Type)
		{
			case RRT_Texture2D:
				break;
			case RRT_TextureCube:
				check(Destination->SizeZ == 6); // Arrays might not work yet.
				break;
			default:
				check(false); // Only Tex2D & Cube are tested to work so far!
				break;
		}
		
		MTLOrigin Origin = MTLOriginMake(0, 0, 0);
		MTLSize Size = MTLSizeMake(0, 0, 1);
		if (ResolveParams.Rect.IsValid())
		{
			// Partial copy
			Origin.x = ResolveParams.Rect.X1;
			Origin.y = ResolveParams.Rect.Y1;
			Size.width = ResolveParams.Rect.X2 - ResolveParams.Rect.X1;
			Size.height = ResolveParams.Rect.Y2 - ResolveParams.Rect.Y1;
		}
		else
		{
			// Whole of source copy
			Origin.x = 0;
			Origin.y = 0;
			
			Size.width = FMath::Max<uint32>(1, Source->SizeX >> ResolveParams.MipIndex);
			Size.height = FMath::Max<uint32>(1, Source->SizeY >> ResolveParams.MipIndex);
		}
		
		const bool bSrcCubemap  = Source->bIsCubemap;
		const bool bDestCubemap = Destination->bIsCubemap;
		
		uint32 DestIndex = ResolveParams.DestArrayIndex * (bDestCubemap ? 6 : 1) + (bDestCubemap ? uint32(ResolveParams.CubeFace) : 0);
		uint32 SrcIndex  = ResolveParams.SourceArrayIndex * (bSrcCubemap ? 6 : 1) + (bSrcCubemap ? uint32(ResolveParams.CubeFace) : 0);
		
		if(Profiler)
		{
			Profiler->RegisterGPUWork();
		}

		//if we're trying to resolve an MSAA Target we must perform a manual resolve when the HW resolve can't do the proper resolve
        //or when we are on Mac where we are supported the 'separate MSAA and Resolve target' path.
		const bool bMSAASource = Source->MSAATexture;
        const bool bMSAADest = Destination->MSAATexture;
        const bool bDepthStencil = Source->PixelFormat == PF_DepthStencil;
		if (bMSAASource && !bMSAADest)
		{
            //This path is a layering violation (using high level constructs to implement an RHI call).
            //It needs to be re-implementing as raw Metal and a built-in shader before going back to main.
			const bool bSupportsMSAADepthResolve = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesMSAADepthResolve);
            if (bDepthStencil && !bSupportsMSAADepthResolve)
            {
                FRHICommandList_RecursiveHazardous RHICmdList(this);

                FResolveRect ResolveRect = ResolveParams.Rect;

                SetRenderTargets(RHICmdList, 0, nullptr, DestTextureRHI, ESimpleRenderTargetMode::EClearColorExistingDepth, FExclusiveDepthStencil::DepthWrite_StencilWrite, true);

                FGraphicsPipelineStateInitializer GraphicsPSOInit;
                RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

                // No alpha blending, no depth tests or writes, no stencil tests or writes, no backface culling.
                GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
                GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();

                GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_Always>::GetRHI();

                const uint32 SourceWidth = Source->SizeX;
                const uint32 SourceHeight = Source->SizeY;

                const uint32 TargetWidth = Destination->SizeX;
                const uint32 TargetHeight = Destination->SizeY;

                RHICmdList.SetViewport(0.0f, 0.0f, 0.0f, TargetWidth, TargetHeight, 1.0f);

                FResolveRect SourceRect = GetDefaultRect(ResolveParams.Rect, SourceWidth, SourceHeight);
                FResolveRect DestRect = GetDefaultRect(ResolveParams.Rect, TargetWidth, TargetHeight);

                // Generate the vertices used to copy from the source surface to the destination surface.
                const float MinU = SourceRect.X1;
                const float MinV = SourceRect.Y1;
                const float MaxU = SourceRect.X2;
                const float MaxV = SourceRect.Y2;
                const float MinX = -1.f + DestRect.X1 / ((float)TargetWidth * 0.5f);
                const float MinY = +1.f - DestRect.Y1 / ((float)TargetHeight * 0.5f);
                const float MaxX = -1.f + DestRect.X2 / ((float)TargetWidth * 0.5f);
                const float MaxY = +1.f - DestRect.Y2 / ((float)TargetHeight * 0.5f);

                // Set the vertex and pixel shader
                auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
                TShaderMapRef<FResolveVS> ResolveVertexShader(ShaderMap);
                TShaderMapRef<FResolveDepthPS> ResolvePixelShader(ShaderMap);

                GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
                GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*ResolveVertexShader);
                GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*ResolvePixelShader);
                GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

                SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
                RHICmdList.SetBlendFactor(FLinearColor::White);

                ResolvePixelShader->SetParameters(RHICmdList, FDummyResolveParameter());

                // Set the source texture.
                const uint32 TextureIndex = ResolvePixelShader->UnresolvedSurface.GetBaseIndex();
                if (SourceTextureRHI)
                {
                    RHICmdList.SetShaderTexture(ResolvePixelShader->GetPixelShader(), TextureIndex, SourceTextureRHI);
                }

                // Generate the vertices used
                FScreenVertex Vertices[4];

                Vertices[0].Position.X = MaxX;
                Vertices[0].Position.Y = MinY;
                Vertices[0].UV.X = MaxU;
                Vertices[0].UV.Y = MinV;

                Vertices[1].Position.X = MaxX;
                Vertices[1].Position.Y = MaxY;
                Vertices[1].UV.X = MaxU;
                Vertices[1].UV.Y = MaxV;

                Vertices[2].Position.X = MinX;
                Vertices[2].Position.Y = MinY;
                Vertices[2].UV.X = MinU;
                Vertices[2].UV.Y = MinV;

                Vertices[3].Position.X = MinX;
                Vertices[3].Position.Y = MaxY;
                Vertices[3].UV.X = MinU;
                Vertices[3].UV.Y = MaxV;

                DrawPrimitiveUP(RHICmdList, PT_TriangleStrip, 2, Vertices, sizeof(Vertices[0]));

                RHICmdList.Flush();
            }
            else
            {
                //MacOS MSAA will do extra work here until we have renderpasses.  As-is it will be copying from our 'auto' resolved target to the high level's destination target.  We would prefer to resolve directly to the high level's destination target.
                Context->CopyFromTextureToTexture(Source->MSAAResolveTexture, SrcIndex, ResolveParams.MipIndex, Origin, Size, Destination->Texture, DestIndex, ResolveParams.MipIndex, Origin);
            }
		}
		else
		{
			Context->CopyFromTextureToTexture(Source->Texture, SrcIndex, ResolveParams.MipIndex, Origin, Size, Destination->Texture, DestIndex, ResolveParams.MipIndex, Origin);
		}
	}
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
 * Helper for storing DXGI_FORMAT_R11G11B10_FLOAT components
 */
struct FMetalFloatR11G11B10
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

/** Helper for accessing R10G10B10A2 colors. */
struct FMetalR10G10B10A2
{
	uint32 R : 10;
	uint32 G : 10;
	uint32 B : 10;
	uint32 A : 2;
};

/** Helper for accessing R16G16 colors. */
struct FMetalRG16
{
	uint16 R;
	uint16 G;
};

/** Helper for accessing R16G16B16A16 colors. */
struct FMetalRGBA16
{
	uint16 R;
	uint16 G;
	uint16 B;
	uint16 A;
};

// todo: this should be available for all RHI
static void ConvertSurfaceDataToFColor(EPixelFormat Format, uint32 Width, uint32 Height, uint8 *In, uint32 SrcPitch, FColor* Out, FReadSurfaceDataFlags InFlags)
{
	bool bLinearToGamma = InFlags.GetLinearToGamma();
	
	if(Format == PF_G16 || Format == PF_R16_UINT || Format == PF_R16_SINT)
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
	else if(Format == PF_R8G8B8A8)
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
	else if(Format == PF_B8G8R8A8)
	{
		for(uint32 Y = 0; Y < Height; Y++)
		{
			FColor* SrcPtr = (FColor*)(In + Y * SrcPitch);
			FColor* DestPtr = Out + Y * Width;
			
			// Need to copy row wise since the Pitch might not match the Width.
			FMemory::Memcpy(DestPtr, SrcPtr, sizeof(FColor) * Width);
		}
	}
	else if(Format == PF_A2B10G10R10)
	{
		// Read the data out of the buffer, converting it from R10G10B10A2 to FColor.
		for(uint32 Y = 0; Y < Height; Y++)
		{
			FMetalR10G10B10A2* SrcPtr = (FMetalR10G10B10A2*)(In + Y * SrcPitch);
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
	else if(Format == PF_FloatRGBA)
	{
		FPlane	MinValue(0.0f,0.0f,0.0f,0.0f),
		MaxValue(1.0f,1.0f,1.0f,1.0f);
		
		check(sizeof(FFloat16)==sizeof(uint16));
		
		for(uint32 Y = 0; Y < Height; Y++)
		{
			FFloat16* SrcPtr = (FFloat16*)(In + Y * SrcPitch);
			
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
			FFloat16* SrcPtr = (FFloat16*)(In + Y * SrcPitch);
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
	else if (Format == PF_FloatR11G11B10)
	{
		check(sizeof(FMetalFloatR11G11B10) == sizeof(uint32));
		
		for(uint32 Y = 0; Y < Height; Y++)
		{
			FMetalFloatR11G11B10* SrcPtr = (FMetalFloatR11G11B10*)(In + Y * SrcPitch);
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
	else if (Format == PF_A32B32G32R32F)
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
			float* SrcPtr = (float*)(In + Y * SrcPitch);
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
	else if(Format == PF_A16B16G16R16)
	{
		// Read the data out of the buffer, converting it to FColor.
		for(uint32 Y = 0; Y < Height; Y++)
		{
			FMetalRGBA16* SrcPtr = (FMetalRGBA16*)(In + Y * SrcPitch);
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
	else if(Format == PF_G16R16)
	{
		// Read the data out of the buffer, converting it to FColor.
		for(uint32 Y = 0; Y < Height; Y++)
		{
			FMetalRG16* SrcPtr = (FMetalRG16*)(In + Y * SrcPitch);
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
	else if(Format == PF_DepthStencil)
	{
		// Depth
		if(!InFlags.GetOutputStencil())
		{
			bool const bDepth32 = (GPixelFormats[Format].PlatformFormat == MTLPixelFormatDepth32Float_Stencil8);
			
			for (uint32 Y = 0; Y < Height; Y++)
			{
				uint32* SrcPtr = (uint32*)(In + Y * SrcPitch);
				FColor* DestPtr = Out + Y * Width;
				
				for (uint32 X = 0; X < Width; X++)
				{
					float DeviceZ = bDepth32 ? *SrcPtr : (*SrcPtr & 0xffffff) / (float)(1 << 24);
					float LinearValue = FMath::Min(InFlags.ComputeNormalizedDepth(DeviceZ), 1.0f);
					FColor NormalizedColor = FLinearColor(LinearValue, LinearValue, LinearValue, 0).ToFColor(bLinearToGamma);
					
					FMemory::Memcpy(DestPtr++, &NormalizedColor, sizeof(FColor));
					++SrcPtr;
				}
			}
		}
		// Stencil
		else
		{
			// Depth stencil
			for (uint32 Y = 0; Y < Height; Y++)
			{
				uint8* SrcPtr = (uint8*)(In + Y * SrcPitch);
				FColor* DestPtr = Out + Y * Width;
				
				for (uint32 X = 0; X < Width; X++)
				{
					uint8 DeviceStencil = *SrcPtr;
					FColor NormalizedColor = FColor(DeviceStencil, DeviceStencil, DeviceStencil, 0xFF);
					
					FMemory::Memcpy(DestPtr++, &NormalizedColor, sizeof(FColor));
					++SrcPtr;
				}
			}
		}
	}
	else
	{
		// not supported yet
		NOT_SUPPORTED("RHIReadSurfaceData Format");
	}
}

void FMetalDynamicRHI::RHIReadSurfaceData(FTextureRHIParamRef TextureRHI, FIntRect Rect, TArray<FColor>& OutData, FReadSurfaceDataFlags InFlags)
{
	@autoreleasepool {
	if (!ensure(TextureRHI))
	{
		OutData.Empty();
		OutData.AddZeroed(Rect.Width() * Rect.Height());
		return;
	}

	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(TextureRHI);

	// allocate output space
	const uint32 SizeX = Rect.Width();
	const uint32 SizeY = Rect.Height();
	OutData.Empty();
	OutData.AddUninitialized(SizeX * SizeY);
	
	FColor* OutDataPtr = OutData.GetData();
	MTLRegion Region = MTLRegionMake2D(Rect.Min.X, Rect.Min.Y, SizeX, SizeY);
    
    id<MTLTexture> Texture = Surface->Texture;
    if(!Texture && (Surface->Flags & TexCreate_Presentable))
    {
        Texture = Surface->GetDrawableTexture();
    }
    if(!Texture)
    {
        UE_LOG(LogRHI, Error, TEXT("Trying to read from an uninitialised texture."));
        return;
    }

	if (GMetalUseTexGetBytes && Surface->PixelFormat != PF_DepthStencil && Surface->PixelFormat != PF_ShadowDepth)
	{
		SCOPE_CYCLE_COUNTER(STAT_MetalTexturePageOffTime);
		
		id<MTLTexture> TempTexture = nil;
		if (ImmediateContext.Context->GetCommandQueue().SupportsFeature(EMetalFeaturesResourceOptions) &&  Texture.storageMode == MTLStorageModePrivate)
		{
#if PLATFORM_MAC
			MTLStorageMode StorageMode = MTLStorageModeManaged;
#else
			MTLStorageMode StorageMode = MTLStorageModeShared;
#endif
			MTLPixelFormat MetalFormat = (MTLPixelFormat)GPixelFormats[Surface->PixelFormat].PlatformFormat;
			MTLTextureDescriptor* Desc = [[MTLTextureDescriptor new] autorelease];
			Desc.textureType = Texture.textureType;
			Desc.pixelFormat = Texture.pixelFormat;
			Desc.width = SizeX;
			Desc.height = SizeY;
			Desc.depth = 1;
			Desc.mipmapLevelCount = Texture.mipmapLevelCount;
			Desc.sampleCount = Texture.sampleCount;
			Desc.arrayLength = Texture.arrayLength;
			
			MTLResourceOptions GeneralResourceOption = GetMetalDeviceContext().GetCommandQueue().GetCompatibleResourceOptions(((Texture.cpuCacheMode << MTLResourceCPUCacheModeShift) | (StorageMode << MTLResourceStorageModeShift) | MTLResourceHazardTrackingModeUntracked));
			Desc.resourceOptions = GeneralResourceOption;
			
			Desc.cpuCacheMode = Texture.cpuCacheMode;
			Desc.storageMode = StorageMode;
			Desc.usage = Texture.usage;
			
			TempTexture = [GetMetalDeviceContext().GetDevice() newTextureWithDescriptor:Desc];
			
			ImmediateContext.Context->CopyFromTextureToTexture(Texture, 0, 0, Region.origin, Region.size, TempTexture, 0, 0, MTLOriginMake(0, 0, 0));
			
			Texture = TempTexture;
			Region = MTLRegionMake2D(0, 0, SizeX, SizeY);
		}
#if PLATFORM_MAC
		if(Texture.storageMode == MTLStorageModeManaged)
		{
			// Synchronise the texture with the CPU
			ImmediateContext.Context->SynchronizeTexture(Texture, 0, 0);
		}
#endif

		//kick the current command buffer.
		ImmediateContext.Context->SubmitCommandBufferAndWait();
		
		const uint32 Stride = GPixelFormats[Surface->PixelFormat].BlockBytes * SizeX;
		const uint32 BytesPerImage = Stride * SizeY;

		TArray<uint8> Data;
		Data.AddUninitialized(BytesPerImage);
		
		[Texture getBytes:Data.GetData() bytesPerRow:Stride bytesPerImage:BytesPerImage fromRegion:Region mipmapLevel:0 slice:0];
		
		ConvertSurfaceDataToFColor(Surface->PixelFormat, SizeX, SizeY, (uint8*)Data.GetData(), Stride, OutDataPtr, InFlags);
		
		if (TempTexture)
		{
			SafeReleaseMetalResource(TempTexture);
		}
	}
	else
	{
		uint32 BytesPerPixel = (Surface->PixelFormat != PF_DepthStencil || !InFlags.GetOutputStencil()) ? GPixelFormats[Surface->PixelFormat].BlockBytes : 1;
		const uint32 Stride = BytesPerPixel * SizeX;
		const uint32 Alignment = PLATFORM_MAC ? 1u : 64u; // Mac permits natural row alignment (tightly-packed) but iOS does not.
		const uint32 AlignedStride = ((Stride - 1) & ~(Alignment - 1)) + Alignment;
		const uint32 BytesPerImage = AlignedStride * SizeY;
		id<MTLBuffer> Buffer = ((FMetalDeviceContext*)ImmediateContext.Context)->CreatePooledBuffer(FMetalPooledBufferArgs(ImmediateContext.Context->GetDevice(), BytesPerImage, MTLStorageModeShared));
		{
			// Synchronise the texture with the CPU
			SCOPE_CYCLE_COUNTER(STAT_MetalTexturePageOffTime);
			
			if (Surface->PixelFormat != PF_DepthStencil)
			{
				ImmediateContext.Context->CopyFromTextureToBuffer(Texture, 0, 0, Region.origin, Region.size, Buffer, 0, AlignedStride, BytesPerImage, MTLBlitOptionNone);
			}
			else if (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesDepthStencilBlitOptions))
			{
				if (!InFlags.GetOutputStencil())
				{
					ImmediateContext.Context->CopyFromTextureToBuffer(Texture, 0, 0, Region.origin, Region.size, Buffer, 0, AlignedStride, BytesPerImage, MTLBlitOptionDepthFromDepthStencil);
				}
				else
				{
					ImmediateContext.Context->CopyFromTextureToBuffer(Texture, 0, 0, Region.origin, Region.size, Buffer, 0, AlignedStride, BytesPerImage, MTLBlitOptionStencilFromDepthStencil);
				}
			}
			else
			{
				// not supported yet
				NOT_SUPPORTED("RHIReadSurfaceData Format");
			}
			
			//kick the current command buffer.
			ImmediateContext.Context->SubmitCommandBufferAndWait();
			
			ConvertSurfaceDataToFColor(Surface->PixelFormat, SizeX, SizeY, (uint8*)[Buffer contents], AlignedStride, OutDataPtr, InFlags);
		}
		((FMetalDeviceContext*)ImmediateContext.Context)->ReleasePooledBuffer(Buffer);
	}
	}
}

void FMetalDynamicRHI::RHIMapStagingSurface(FTextureRHIParamRef TextureRHI,void*& OutData,int32& OutWidth,int32& OutHeight)
{
	@autoreleasepool {
    FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(TextureRHI);
    FMetalTexture2D* Texture = (FMetalTexture2D*)TextureRHI->GetTexture2D();
    
    uint32 Stride = 0;
    OutWidth = Texture->GetSizeX();
    OutHeight = Texture->GetSizeY();
    OutData = Surface->Lock(0, 0, RLM_ReadOnly, Stride);
	}
}

void FMetalDynamicRHI::RHIUnmapStagingSurface(FTextureRHIParamRef TextureRHI)
{
	@autoreleasepool {
    FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(TextureRHI);
	
    Surface->Unlock(0, 0);
	}
}

void FMetalDynamicRHI::RHIReadSurfaceFloatData(FTextureRHIParamRef TextureRHI, FIntRect Rect, TArray<FFloat16Color>& OutData, ECubeFace CubeFace,int32 ArrayIndex,int32 MipIndex)
{
	@autoreleasepool {
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(TextureRHI);
	
    id<MTLTexture> Texture = Surface->Texture;
    if(!Texture && (Surface->Flags & TexCreate_Presentable))
    {
		Texture = Surface->GetDrawableTexture();
    }
    if(!Texture)
    {
        UE_LOG(LogRHI, Error, TEXT("Trying to read from an uninitialised texture."));
        return;
    }
    
	// verify the input image format (but don't crash)
	if (Surface->PixelFormat != PF_FloatRGBA)
	{
		UE_LOG(LogRHI, Log, TEXT("Trying to read non-FloatRGBA surface."));
	}

	if (TextureRHI->GetTextureCube())
	{
		// adjust index to account for cubemaps as texture arrays
		ArrayIndex *= CubeFace_MAX;
		ArrayIndex += GetMetalCubeFace(CubeFace);
	}
	
	// allocate output space
	const uint32 SizeX = Rect.Width();
	const uint32 SizeY = Rect.Height();
	OutData.Empty();
	OutData.AddUninitialized(SizeX * SizeY);
	
	MTLRegion Region = MTLRegionMake2D(Rect.Min.X, Rect.Min.Y, SizeX, SizeY);
	
	// function wants details about the destination, not the source
	const uint32 Stride = GPixelFormats[Surface->PixelFormat].BlockBytes * SizeX;
	const uint32 Alignment = PLATFORM_MAC ? 1u : 64u; // Mac permits natural row alignment (tightly-packed) but iOS does not.
	const uint32 AlignedStride = ((Stride - 1) & ~(Alignment - 1)) + Alignment;
	const uint32 BytesPerImage = AlignedStride  * SizeY;
	int32 FloatBGRADataSize = BytesPerImage;
	id<MTLBuffer> Buffer = ((FMetalDeviceContext*)ImmediateContext.Context)->CreatePooledBuffer(FMetalPooledBufferArgs(ImmediateContext.Context->GetDevice(), FloatBGRADataSize, MTLStorageModeShared));
	{
		// Synchronise the texture with the CPU
		SCOPE_CYCLE_COUNTER(STAT_MetalTexturePageOffTime);
		
		ImmediateContext.Context->CopyFromTextureToBuffer(Texture, ArrayIndex, MipIndex, Region.origin, Region.size, Buffer, 0, AlignedStride, BytesPerImage, MTLBlitOptionNone);
		
		//kick the current command buffer.
		ImmediateContext.Context->SubmitCommandBufferAndWait();
	}
	
	uint8* DataPtr = (uint8*)[Buffer contents];
	FFloat16Color* OutDataPtr = OutData.GetData();
	if (Alignment > 1u)
	{
		for (uint32 Row = 0; Row < SizeY; Row++)
		{
			FFloat16Color* FloatBGRAData = (FFloat16Color*)DataPtr;
			FMemory::Memcpy(OutDataPtr, FloatBGRAData, Stride);
			DataPtr += AlignedStride;
			OutDataPtr += SizeX;
		}
	}
	else
	{
		FFloat16Color* FloatBGRAData = (FFloat16Color*)DataPtr;
		FMemory::Memcpy(OutDataPtr, FloatBGRAData, FloatBGRADataSize);
	}
	
	((FMetalDeviceContext*)ImmediateContext.Context)->ReleasePooledBuffer(Buffer);
	}
}

void FMetalDynamicRHI::RHIRead3DSurfaceFloatData(FTextureRHIParamRef TextureRHI,FIntRect InRect,FIntPoint ZMinMax,TArray<FFloat16Color>& OutData)
{
	@autoreleasepool {
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(TextureRHI);
	
	id<MTLTexture> Texture = Surface->Texture;
	if(!Texture)
	{
		UE_LOG(LogRHI, Error, TEXT("Trying to read from an uninitialised texture."));
		return;
	}
	
	// verify the input image format (but don't crash)
	if (Surface->PixelFormat != PF_FloatRGBA)
	{
		UE_LOG(LogRHI, Log, TEXT("Trying to read non-FloatRGBA surface."));
	}
	
	// allocate output space
	const uint32 SizeX = InRect.Width();
	const uint32 SizeY = InRect.Height();
	const uint32 SizeZ = ZMinMax.Y - ZMinMax.X;
	OutData.Empty();
	OutData.AddUninitialized(SizeX * SizeY * SizeZ);
	
	MTLRegion Region = MTLRegionMake3D(InRect.Min.X, InRect.Min.Y, ZMinMax.X, SizeX, SizeY, SizeZ);
	
	// function wants details about the destination, not the source
	const uint32 Stride = GPixelFormats[Surface->PixelFormat].BlockBytes * SizeX;
	const uint32 Alignment = PLATFORM_MAC ? 1u : 64u; // Mac permits natural row alignment (tightly-packed) but iOS does not.
	const uint32 AlignedStride = ((Stride - 1) & ~(Alignment - 1)) + Alignment;
	const uint32 BytesPerImage = AlignedStride  * SizeY;
	int32 FloatBGRADataSize = BytesPerImage * SizeZ;
	id<MTLBuffer> Buffer = ((FMetalDeviceContext*)ImmediateContext.Context)->CreatePooledBuffer(FMetalPooledBufferArgs(ImmediateContext.Context->GetDevice(), FloatBGRADataSize, MTLStorageModeShared));
	{
		// Synchronise the texture with the CPU
		SCOPE_CYCLE_COUNTER(STAT_MetalTexturePageOffTime);
		
		ImmediateContext.Context->CopyFromTextureToBuffer(Texture, 0, 0, Region.origin, Region.size, Buffer, 0, AlignedStride, BytesPerImage, MTLBlitOptionNone);
		
		//kick the current command buffer.
		ImmediateContext.Context->SubmitCommandBufferAndWait();
	}
	
	uint8* DataPtr = (uint8*)[Buffer contents];
	FFloat16Color* OutDataPtr = OutData.GetData();
	if (Alignment > 1u)
	{
		for (uint32 Image = 0; Image < SizeZ; Image++)
		{
			for (uint32 Row = 0; Row < SizeY; Row++)
			{
				FFloat16Color* FloatBGRAData = (FFloat16Color*)DataPtr;
				FMemory::Memcpy(OutDataPtr, FloatBGRAData, Stride);
				DataPtr += AlignedStride;
				OutDataPtr += SizeX;
			}
		}
	}
	else
	{
		FFloat16Color* FloatBGRAData = (FFloat16Color*)DataPtr;
		FMemory::Memcpy(OutDataPtr, FloatBGRAData, FloatBGRADataSize);
	}
	
	((FMetalDeviceContext*)ImmediateContext.Context)->ReleasePooledBuffer(Buffer);
	}
}
