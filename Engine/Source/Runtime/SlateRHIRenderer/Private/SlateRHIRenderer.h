// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Textures/SlateShaderResource.h"
#include "Rendering/DrawElements.h"
#include "RHI.h"
#include "RenderResource.h"
#include "SlateRHIResourceManager.h"
#include "UnrealClient.h"
#include "Rendering/SlateRenderer.h"
#include "Rendering/SlateDrawBuffer.h"
#include "Slate/SlateTextures.h"

class FSlateElementBatcher;
class FSlateRHIRenderingPolicy;
class ISlateStyle;
class SWindow;
struct Rect;

template<typename TCmd> struct FRHICommand;

// Number of draw buffers that can be active at any given time
const uint32 NumDrawBuffers = 3;

// Enable to visualize overdraw in Slate
#define WITH_SLATE_VISUALIZERS !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#if WITH_SLATE_VISUALIZERS
extern TAutoConsoleVariable<int32> CVarShowSlateOverdraw;
extern TAutoConsoleVariable<int32> CVarShowSlateBatching;
#endif

class FSlateBackBuffer : public FRenderTarget
{
public:
	FSlateBackBuffer(FTexture2DRHIRef InRenderTargetTexture, FIntPoint InSizeXY)
		: SizeXY(InSizeXY)
	{
		RenderTargetTextureRHI = InRenderTargetTexture;
	}
	virtual FIntPoint GetSizeXY() const override { return SizeXY; }
private:
	FIntPoint SizeXY;
};

/** A Slate rendering implementation for Unreal engine */
class FSlateRHIRenderer : public FSlateRenderer
{
private:

	/** An RHI Representation of a viewport with cached width and height for detecting resizes */
	struct FViewportInfo : public FRenderResource
	{
		/** The projection matrix used in the viewport */
		FMatrix ProjectionMatrix;	
		/** The viewport rendering handle */
		FViewportRHIRef ViewportRHI;
		/** The depth buffer texture if any */
		FTexture2DRHIRef DepthStencil;

		// Buffers used in HDR compositing
		/** sRGB UI render target */
		FTexture2DRHIRef UITargetRT;
		/** HDR source data */
		FTexture2DRHIRef HDRSourceRT;
		/** sRGB UI render target */
		FTexture2DRHIRef UITargetSRV;
		/** HDR source data */
		FTexture2DRHIRef HDRSourceSRV;

		/** Color-space LUT for HDR UI composition. */
		FTexture3DRHIRef ColorSpaceLUTRT;
		FTexture3DRHIRef ColorSpaceLUTSRV;
		int32 ColorSpaceLUTOutputDevice;
		int32 ColorSpaceLUTOutputGamut;
		
		//FTexture2DRHIRef RenderTargetTexture;
		/** The OS Window handle (for recreating the viewport) */
		void* OSWindow;
		/** The actual width of the viewport */
		uint32 Width;
		/** The actual height of the viewport */
		uint32 Height;
		/** The desired width of the viewport */
		uint32 DesiredWidth;
		/** The desired height of the viewport */
		uint32 DesiredHeight;
		/** Whether or not the viewport requires a stencil test */
		bool bRequiresStencilTest;
		/** Whether or not the viewport is in fullscreen */
		bool bFullscreen;
		/** The desired pixel format for this viewport */
		EPixelFormat PixelFormat;
		/** The desired SDR pixel format for this viewport */
		EPixelFormat SDRPixelFormat;
		/** Color gamut for output to HDR display */
		int32 HDRColorGamut;
		/** Device format for output to HDR display */
		int32 HDROutputDevice;

		IViewportRenderTargetProvider* RTProvider;
	
		/** FRenderResource interface */
		virtual void InitRHI() override;
		virtual void ReleaseRHI() override;

		FViewportInfo()
			:	ColorSpaceLUTOutputDevice(0),
				ColorSpaceLUTOutputGamut(0),
				OSWindow(NULL), 
				Width(0),
				Height(0),
				DesiredWidth(0),
				DesiredHeight(0),
				bRequiresStencilTest(false),
				bFullscreen(false),
				PixelFormat(EPixelFormat::PF_Unknown),
				SDRPixelFormat(EPixelFormat::PF_Unknown),
				RTProvider(nullptr)
		{

		}

		~FViewportInfo()
		{
			DepthStencil.SafeRelease();
			UITargetRT.SafeRelease();
			HDRSourceRT.SafeRelease();
			UITargetSRV.SafeRelease();
			HDRSourceSRV.SafeRelease();
			ColorSpaceLUTRT.SafeRelease();
			ColorSpaceLUTSRV.SafeRelease();
		}

		void ConditionallyUpdateDepthBuffer(bool bInRequiresStencilTest, uint32 Width, uint32 Height);
		void RecreateDepthBuffer_RenderThread();

		FTexture2DRHIRef GetRenderTargetTexture() const
		{
			if (RTProvider)
			{
				FSlateShaderResource* RenderTargetTexture = RTProvider->GetViewportRenderTargetTexture();
				if( RenderTargetTexture )
				{
					FSlateRenderTargetRHI* RHITarget = (FSlateRenderTargetRHI*)RenderTargetTexture;
					return RHITarget->GetTypedResource();
				}
			}
			return nullptr;
		}

	private:		
	};

public:
	FSlateRHIRenderer( TSharedRef<FSlateFontServices> InSlateFontServices, TSharedRef<FSlateRHIResourceManager> InResourceManager );
	~FSlateRHIRenderer();

	/**
	 * Creates a projection matrix for use when rendering an SWindow
	 *
	 * @param Width 	The width of the window
	 * @param Height	The height of the window
	 * @return The created projection matrix
	 */
	static FMatrix CreateProjectionMatrix( uint32 Width, uint32 Height );

	/** FSlateRenderer interface */
	virtual bool Initialize() override;
	virtual void Destroy() override;
	virtual FSlateDrawBuffer& GetDrawBuffer() override;
	virtual void OnWindowDestroyed( const TSharedRef<SWindow>& InWindow ) override;
	virtual void RequestResize( const TSharedPtr<SWindow>& Window, uint32 NewWidth, uint32 NewHeight ) override;
	virtual void CreateViewport( const TSharedRef<SWindow> Window ) override;
	virtual void UpdateFullscreenState( const TSharedRef<SWindow> Window, uint32 OverrideResX, uint32 OverrideResY ) override;
	virtual void RestoreSystemResolution(const TSharedRef<SWindow> InWindow) override;
	virtual void DrawWindows( FSlateDrawBuffer& InWindowDrawBuffer ) override;
	virtual void FlushCommands() const override;
	virtual void Sync() const override;
	virtual void ReleaseDynamicResource( const FSlateBrush& InBrush ) override;
	virtual void RemoveDynamicBrushResource( TSharedPtr<FSlateDynamicImageBrush> BrushToRemove ) override;
	virtual FIntPoint GenerateDynamicImageResource(const FName InTextureName) override;
	virtual bool GenerateDynamicImageResource( FName ResourceName, uint32 Width, uint32 Height, const TArray< uint8 >& Bytes ) override;
	virtual bool GenerateDynamicImageResource( FName ResourceName, FSlateTextureDataRef TextureData ) override;
	virtual FSlateResourceHandle GetResourceHandle( const FSlateBrush& Brush ) override;
	virtual void* GetViewportResource( const SWindow& Window ) override;
	virtual void SetColorVisionDeficiencyType( uint32 Type ) override;
	virtual FSlateUpdatableTexture* CreateUpdatableTexture(uint32 Width, uint32 Height) override;
	virtual void ReleaseUpdatableTexture(FSlateUpdatableTexture* Texture) override;
	virtual ISlateAtlasProvider* GetTextureAtlasProvider() override;
	virtual void ReleaseAccessedResources(bool bImmediatelyFlush) override;
	virtual TSharedRef<FSlateRenderDataHandle, ESPMode::ThreadSafe> CacheElementRenderData(const ILayoutCache* Cacher, FSlateWindowElementList& ElementList) override;
	virtual void ReleaseCachingResourcesFor(const ILayoutCache* Cacher) override;
	virtual int32 RegisterCurrentScene(FSceneInterface* Scene) override;
	virtual int32 GetCurrentSceneIndex() const override;
	virtual void ClearScenes() override;

	/** Draws windows from a FSlateDrawBuffer on the render thread */
	void DrawWindow_RenderThread(FRHICommandListImmediate& RHICmdList, FSlateRHIRenderer::FViewportInfo& ViewportInfo, FSlateWindowElementList& WindowElementList, bool bLockToVsync, bool bClear, FVector2D WindowSize);

	/**
	 * Reloads texture resources from disk                   
	 */
	virtual void ReloadTextureResources() override;

	virtual void LoadStyleResources( const ISlateStyle& Style ) override;

	/** Returns whether shaders that Slate depends on have been compiled. */
	virtual bool AreShadersInitialized() const override;

	/** 
	 * Removes references to FViewportRHI's.  
	 * This has to be done explicitly instead of using the FRenderResource mechanism because FViewportRHI's are managed by the game thread.
	 * This is needed before destroying the RHI device. 
	 */
	virtual void InvalidateAllViewports() override;

	virtual void PrepareToTakeScreenshot(const FIntRect& Rect, TArray<FColor>* OutColorData) override;

	virtual void SetWindowRenderTarget(const SWindow& Window, class IViewportRenderTargetProvider* Provider) override;

private:
	/** Loads all known textures from Slate styles */
	void LoadUsedTextures();

	/**
	 * Resizes the viewport for a window if needed
	 * 
	 * @param ViewportInfo	The viewport to resize
	 * @param Width			The width that we should size to
	 * @param Height		The height that we shoudl size to
	 * @param bFullscreen	If we should be in fullscreen
	 */
	void ConditionalResizeViewport( FViewportInfo* ViewportInfo, uint32 Width, uint32 Height, bool bFullscreen );
	
	/** 
	 * Creates necessary resources to render a window and sends draw commands to the rendering thread
	 *
	 * @param WindowDrawBuffer	The buffer containing elements to draw 
	 */
	void DrawWindows_Private( FSlateDrawBuffer& InWindowDrawBuffer );

	/**
	 * Delete the updateable textures we've marked for delete that have already had their GPU resources released, but may
	 * have already been used on the game thread at the time they were released.
	 */
	void CleanUpdatableTextures();

private:
	/** A mapping of SWindows to their RHI implementation */
	TMap< const SWindow*, FViewportInfo*> WindowToViewportInfo;

	/** View matrix used by all windows */
	FMatrix ViewMatrix;

	/** Keep a pointer around for when we have deferred drawing happening */
	FSlateDrawBuffer* EnqueuedWindowDrawBuffer;

	/** Double buffered draw buffers so that the rendering thread can be rendering windows while the game thread is setting up for next frame */
	FSlateDrawBuffer DrawBuffers[NumDrawBuffers];

	/** The draw buffer which is currently free for use by the game thread */
	uint8 FreeBufferIndex;

	/** Element batcher which renders draw elements */
	TSharedPtr<FSlateElementBatcher> ElementBatcher;

	/** Texture manager for accessing textures on the game thread */
	TSharedPtr<FSlateRHIResourceManager> ResourceManager;

	/** Drawing policy */
	TSharedPtr<FSlateRHIRenderingPolicy> RenderingPolicy;

	TArray< TSharedPtr<FSlateDynamicImageBrush> > DynamicBrushesToRemove[NumDrawBuffers];

	bool bTakingAScreenShot;
	FIntRect ScreenshotRect;
	TArray<FColor>* OutScreenshotData;

	/** These are state management variables for Scenes on the game thread. A similar copy exists on the RHI Rendering Policy for the rendering thread.*/
	TArray<FSceneInterface*> ActiveScenes;
	int32 CurrentSceneIndex;
};

struct FSlateEndDrawingWindowsCommand : public FRHICommand < FSlateEndDrawingWindowsCommand >
{
	FSlateRHIRenderingPolicy& Policy;
	FSlateDrawBuffer* DrawBuffer;

	FSlateEndDrawingWindowsCommand(FSlateRHIRenderingPolicy& InPolicy, FSlateDrawBuffer* InDrawBuffer);

	void Execute(FRHICommandListBase& CmdList);

	static void EndDrawingWindows(FRHICommandListImmediate& RHICmdList, FSlateDrawBuffer* DrawBuffer, FSlateRHIRenderingPolicy& Policy);
};
