// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AvfMediaVideoSampler.h"
#include "AvfMediaPrivate.h"

#include "Containers/ResourceArray.h"
#include "MediaSamples.h"
#include "Misc/ScopeLock.h"

#if WITH_ENGINE
#include "RenderingThread.h"
#include "RHI.h"
#include "MediaShaders.h"
#include "StaticBoundShaderState.h"
#include "Misc/ScopeLock.h"
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"
#include "Class.h"
#endif

#include "AvfMediaTextureSample.h"


/**
 * Passes a CV*TextureRef or CVPixelBufferRef through to the RHI to wrap in an RHI texture without traversing system memory.
 */
class FAvfTexture2DResourceWrapper
	: public FResourceBulkDataInterface
{
public:

	FAvfTexture2DResourceWrapper(CFTypeRef InImageBuffer)
		: ImageBuffer(InImageBuffer)
	{
		check(ImageBuffer);
		CFRetain(ImageBuffer);
	}

	virtual ~FAvfTexture2DResourceWrapper()
	{
		CFRelease(ImageBuffer);
		ImageBuffer = nullptr;
	}

public:

	//~ FResourceBulkDataInterface interface

	virtual void Discard() override
	{
		delete this;
	}

	virtual const void* GetResourceBulkData() const override
	{
		return ImageBuffer;
	}
	
	virtual uint32 GetResourceBulkDataSize() const override
	{
		return ImageBuffer ? ~0u : 0;
	}

	virtual EBulkDataType GetResourceType() const override
	{
		return EBulkDataType::MediaTexture;
	}
			
	CFTypeRef ImageBuffer;
};


/**
 * Allows for direct GPU mem allocation for texture resource from a CVImageBufferRef's system memory backing store.
 */
class FAvfTexture2DResourceMem
	: public FResourceBulkDataInterface
{
public:
	FAvfTexture2DResourceMem(CVImageBufferRef InImageBuffer)
	: ImageBuffer(InImageBuffer)
	{
		check(ImageBuffer);
		CFRetain(ImageBuffer);
	}
	
	/**
	 * @return ptr to the resource memory which has been preallocated
	 */
	virtual const void* GetResourceBulkData() const override
	{
		CVPixelBufferLockBaseAddress(ImageBuffer, kCVPixelBufferLock_ReadOnly);
		return CVPixelBufferGetBaseAddress(ImageBuffer);
	}
	
	/**
	 * @return size of resource memory
	 */
	virtual uint32 GetResourceBulkDataSize() const override
	{
		int32 Pitch = CVPixelBufferGetBytesPerRow(ImageBuffer);
		int32 Height = CVPixelBufferGetHeight(ImageBuffer);
		uint32 Size = (Pitch * Height);

		return Size;
	}
	
	/**
	 * Free memory after it has been used to initialize RHI resource
	 */
	virtual void Discard() override
	{
		CVPixelBufferUnlockBaseAddress(ImageBuffer, kCVPixelBufferLock_ReadOnly);
		delete this;
	}
	
	virtual ~FAvfTexture2DResourceMem()
	{
		CFRelease(ImageBuffer);
		ImageBuffer = nullptr;
	}
	
	CVImageBufferRef ImageBuffer;
};


/* FAvfMediaVideoSampler structors
 *****************************************************************************/

FAvfMediaVideoSampler::FAvfMediaVideoSampler(FMediaSamples& InSamples)
	: Output(nil)
	, Samples(InSamples)
	, VideoSamplePool(new FAvfMediaTextureSamplePool)
	, FrameRate(0.0f)
#if WITH_ENGINE && COREVIDEO_SUPPORTS_METAL
	, MetalTextureCache(nullptr)
#endif
{ }


FAvfMediaVideoSampler::~FAvfMediaVideoSampler()
{
	[Output release];

	delete VideoSamplePool;
	VideoSamplePool = nullptr;

#if WITH_ENGINE && COREVIDEO_SUPPORTS_METAL
	if (MetalTextureCache)
	{
		CFRelease(MetalTextureCache);
		MetalTextureCache = nullptr;
	}
#endif
}


/* FAvfMediaVideoSampler interface
 *****************************************************************************/

void FAvfMediaVideoSampler::SetOutput(AVPlayerItemVideoOutput* InOutput, float InFrameRate)
{
	check(IsInRenderingThread());

	FScopeLock Lock(&CriticalSection);

	[InOutput retain];
	[Output release];
	Output = InOutput;

	FrameRate = InFrameRate;
}


void FAvfMediaVideoSampler::Tick()
{
	check(IsInRenderingThread());

	FScopeLock Lock(&CriticalSection);

	if (Output == nil)
	{
		return;
	}

	CMTime OutputItemTime = [Output itemTimeForHostTime:CACurrentMediaTime()];

	if (![Output hasNewPixelBufferForItemTime : OutputItemTime])
	{
		return;
	}

	CVPixelBufferRef Frame = [Output copyPixelBufferForItemTime:OutputItemTime itemTimeForDisplay:nullptr];

	if (!Frame)
	{
		return;
	}

	const FTimespan SampleDuration = FTimespan::FromSeconds(FrameRate);
	const FTimespan SampleTime = FTimespan::FromSeconds(CMTimeGetSeconds(OutputItemTime));

	const int32 FrameHeight = CVPixelBufferGetHeight(Frame);
	const int32 FrameWidth = CVPixelBufferGetWidth(Frame);
	const int32 FrameStride = CVPixelBufferGetBytesPerRow(Frame);

	const FIntPoint Dim = FIntPoint(FrameStride / 4, FrameHeight);
	const FIntPoint OutputDim = FIntPoint(FrameWidth, FrameHeight);

	auto VideoSample = VideoSamplePool->AcquireShared();

#if WITH_ENGINE
	TRefCountPtr<FRHITexture2D> ShaderResource;
#if COREVIDEO_SUPPORTS_METAL
	// On iOS/tvOS we use the Metal texture cache
	if (IsMetalPlatform(GMaxRHIShaderPlatform))
	{
		if (!MetalTextureCache)
		{
			id<MTLDevice> Device = (id<MTLDevice>)GDynamicRHI->RHIGetNativeDevice();
			check(Device);
			
			CVReturn Return = CVMetalTextureCacheCreate(kCFAllocatorDefault, nullptr, Device, nullptr, &MetalTextureCache);
			check(Return == kCVReturnSuccess);
		}
		check(MetalTextureCache);
		
		if (CVPixelBufferIsPlanar(Frame))
		{
			check(IsMetalPlatform(GMaxRHIShaderPlatform));
			
			uint32 TexCreateFlags = TexCreate_Dynamic | TexCreate_NoTiling;
			
			int32 YWidth = CVPixelBufferGetWidthOfPlane(Frame, 0);
			int32 YHeight = CVPixelBufferGetHeightOfPlane(Frame, 0);
			
			CVMetalTextureRef YTextureRef = nullptr;
			CVReturn Result = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault, MetalTextureCache, Frame, nullptr, MTLPixelFormatR8Unorm, YWidth, YHeight, 0, &YTextureRef);
			check(Result == kCVReturnSuccess);
			check(YTextureRef);
			
			int32 UVWidth = CVPixelBufferGetWidthOfPlane(Frame, 1);
			int32 UVHeight = CVPixelBufferGetHeightOfPlane(Frame, 1);
			
			CVMetalTextureRef UVTextureRef = nullptr;
			Result = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault, MetalTextureCache, Frame, nullptr, MTLPixelFormatRG8Unorm, UVWidth, UVHeight, 1, &UVTextureRef);
			check(Result == kCVReturnSuccess);
			check(UVTextureRef);
			
			// Metal can upload directly from an IOSurface to a 2D texture, so we can just wrap it.
			FRHIResourceCreateInfo YCreateInfo;
			YCreateInfo.BulkData = new FAvfTexture2DResourceWrapper(YTextureRef);
			YCreateInfo.ResourceArray = nullptr;
			
			FRHIResourceCreateInfo UVCreateInfo;
			UVCreateInfo.BulkData = new FAvfTexture2DResourceWrapper(UVTextureRef);
			UVCreateInfo.ResourceArray = nullptr;
			
			TRefCountPtr<FRHITexture2D> YTex = RHICreateTexture2D(YWidth, YHeight, PF_G8, 1, 1, TexCreateFlags | TexCreate_ShaderResource, YCreateInfo);
			TRefCountPtr<FRHITexture2D> UVTex = RHICreateTexture2D(UVWidth, UVHeight, PF_R8G8, 1, 1, TexCreateFlags | TexCreate_ShaderResource, UVCreateInfo);
			
			FRHIResourceCreateInfo Info;
			ShaderResource = RHICreateTexture2D(YWidth, YHeight, PF_B8G8R8A8, 1, 1, TexCreateFlags | TexCreate_ShaderResource | TexCreate_RenderTargetable, Info);
			
			// render video frame into sink texture
			FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
			{
				// configure media shaders
				auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
				TShaderMapRef<FMediaShadersVS> VertexShader(ShaderMap);
				TShaderMapRef<FYCbCrConvertPS> PixelShader(ShaderMap);
				
				SetRenderTarget(RHICmdList, ShaderResource, nullptr, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthNop_StencilNop);
				
				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
				
				GraphicsPSOInit.BlendState = TStaticBlendStateWriteMask<CW_RGBA, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE>::GetRHI();
				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GMediaVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
				
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
				
				PixelShader->SetParameters(RHICmdList, YTex, UVTex, MediaShaders::YuvToSrgbPs4, true);
				
				// draw full-size quad
				FMediaElementVertex Vertices[4];
				Vertices[0].Position.Set(-1.0f,  1.0f, 1.0f, 1.0f); // Top Left
				Vertices[1].Position.Set( 1.0f,  1.0f, 1.0f, 1.0f); // Top Right
				Vertices[2].Position.Set(-1.0f, -1.0f, 1.0f, 1.0f); // Bottom Left
				Vertices[3].Position.Set( 1.0f, -1.0f, 1.0f, 1.0f); // Bottom Right
				
				const float ULeft   = 0.0f;
				const float URight  = 1.0f;
				const float VTop    = 0.0f;
				const float VBottom = 1.0f;
				
				Vertices[0].TextureCoordinate.Set(ULeft, VTop);
				Vertices[1].TextureCoordinate.Set(URight, VTop);
				Vertices[2].TextureCoordinate.Set(ULeft, VBottom);
				Vertices[3].TextureCoordinate.Set(URight, VBottom);
				
				RHICmdList.SetViewport(0, 0, 0.0f, YWidth, YHeight, 1.0f);

				DrawPrimitiveUP(RHICmdList, PT_TriangleStrip, 2, Vertices, sizeof(Vertices[0]));
				
				RHICmdList.CopyToResolveTarget(ShaderResource, ShaderResource, true, FResolveParams());
			}
			
			CFRelease(YTextureRef);
			CFRelease(UVTextureRef);
		}
		else
		{
			int32 Width = CVPixelBufferGetWidth(Frame);
			int32 Height = CVPixelBufferGetHeight(Frame);
			
			CVMetalTextureRef TextureRef = nullptr;
			CVReturn Result = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault, MetalTextureCache, Frame, nullptr, MTLPixelFormatBGRA8Unorm_sRGB, Width, Height, 0, &TextureRef);
			check(Result == kCVReturnSuccess);
			check(TextureRef);
			
			FRHIResourceCreateInfo CreateInfo;
			CreateInfo.BulkData = new FAvfTexture2DResourceWrapper(TextureRef);
			CreateInfo.ResourceArray = nullptr;
			
			uint32 TexCreateFlags = TexCreate_SRGB;
			TexCreateFlags |= TexCreate_Dynamic | TexCreate_NoTiling;
			
			ShaderResource = RHICreateTexture2D(Width, Height, PF_B8G8R8A8, 1, 1, TexCreateFlags | TexCreate_ShaderResource, CreateInfo);
			
			CFRelease(TextureRef);
		}
	}
	else // Ran out of time to implement efficient OpenGLES texture upload - its running out of memory.
	/*{
	 if (!OpenGLTextureCache)
	 {
	 EAGLContext* Context = [EAGLContext currentContext];
	 check(Context);
	 
	 CVReturn Return = CVOpenGLESTextureCacheCreate(kCFAllocatorDefault, nullptr, Context, nullptr, &OpenGLTextureCache);
	 check(Return == kCVReturnSuccess);
	 }
	 check(OpenGLTextureCache);
	 
	 int32 Width = CVPixelBufferGetWidth(Frame);
	 int32 Height = CVPixelBufferGetHeight(Frame);
	 
	 CVOpenGLESTextureRef TextureRef = nullptr;
	 CVReturn Result = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault, OpenGLTextureCache, Frame, nullptr, GL_TEXTURE_2D, GL_RGBA, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, 0, &TextureRef);
	 check(Result == kCVReturnSuccess);
	 check(TextureRef);
	 
	 FRHIResourceCreateInfo CreateInfo;
	 CreateInfo.BulkData = new FAvfTexture2DResourceWrapper(TextureRef);
	 CreateInfo.ResourceArray = nullptr;
	 
	 uint32 TexCreateFlags = 0;
	 TexCreateFlags |= TexCreate_Dynamic | TexCreate_NoTiling;
	 
	 TRefCountPtr<FRHITexture2D> RenderTarget;
	 TRefCountPtr<FRHITexture2D> ShaderResource;
	 
	 RHICreateTargetableShaderResource2D(Width,
	 Height,
	 PF_R8G8B8A8,
	 1,
	 TexCreateFlags,
	 TexCreate_RenderTargetable,
	 false,
	 CreateInfo,
	 RenderTarget,
	 ShaderResource
	 );
	 
	 VideoSink->UpdateTextureSinkResource(RenderTarget, ShaderResource);
	 CFRelease(TextureRef);
	 }*/
#endif	//COREVIDEO_SUPPORTS_METAL // On Mac we have to use IOSurfaceRef for backward compatibility - unless we update MIN_REQUIRED_VERSION to 10.11 we link against an older version of CoreVideo that doesn't support Metal.
	{
		FRHIResourceCreateInfo CreateInfo;
		if (IsMetalPlatform(GMaxRHIShaderPlatform))
		{
			// Metal can upload directly from an IOSurface to a 2D texture, so we can just wrap it.
			CreateInfo.BulkData = new FAvfTexture2DResourceWrapper(Frame);
		}
		else
		{
			// OpenGL on Mac uploads as a TEXTURE_RECTANGLE for which we have no code, so upload via system memory.
			CreateInfo.BulkData = new FAvfTexture2DResourceMem(Frame);
		}
		CreateInfo.ResourceArray = nullptr;
		
		int32 Width = CVPixelBufferGetWidth(Frame);
		int32 Height = CVPixelBufferGetHeight(Frame);
		
		uint32 TexCreateFlags = TexCreate_SRGB;
		TexCreateFlags |= TexCreate_Dynamic | TexCreate_NoTiling;

		ShaderResource = RHICreateTexture2D(Width, Height, PF_B8G8R8A8, 1, 1, TexCreateFlags | TexCreate_ShaderResource, CreateInfo);
	}
	
	if(ShaderResource.IsValid())
	{
		if (VideoSample->Initialize(ShaderResource, Dim, OutputDim, SampleTime, SampleDuration))
		{
			Samples.AddVideo(VideoSample);
		}
	}
	
#else //WITH_ENGINE
	
	if(CVPixelBufferLockBaseAddress(Frame, kCVPixelBufferLock_ReadOnly) == kCVReturnSuccess)
	{
		uint8* VideoData = (uint8*)CVPixelBufferGetBaseAddress(Frame);
		
		if (VideoSample->Initialize(VideoData, Dim, OutputDim, FrameStride, SampleTime, SampleDuration))
		{
			Samples.AddVideo(VideoSample);
		}
		
		
		CVPixelBufferUnlockBaseAddress(Frame, kCVPixelBufferLock_ReadOnly);
	}
	
#endif //WITH_ENGINE
	
	CVPixelBufferRelease(Frame);
}
