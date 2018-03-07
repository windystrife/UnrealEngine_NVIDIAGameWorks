// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnrealClient.h: Interface definition for platform specific client code.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "Input/PopupMethodReply.h"
#include "Widgets/SWidget.h"
#include "UObject/GCObject.h"
#include "RHI.h"
#include "RenderResource.h"
#include "HitProxies.h"

class FCanvas;
class FViewport;
class FViewportClient;

/**
 * A render target.
 */
class FRenderTarget
{
public:

	// Destructor
	virtual ~FRenderTarget(){}

	/**
	* Accessor for the surface RHI when setting this render target
	* @return render target surface RHI resource
	*/
	ENGINE_API virtual const FTexture2DRHIRef& GetRenderTargetTexture() const;
	ENGINE_API virtual FUnorderedAccessViewRHIRef GetRenderTargetUAV() const;

	// Properties.
	virtual FIntPoint GetSizeXY() const = 0;

	/**
	* @return display gamma expected for rendering to this render target
	*/
	ENGINE_API virtual float GetDisplayGamma() const;

	/**
	* Handles freezing/unfreezing of rendering
	*/
	virtual void ProcessToggleFreezeCommand() {};

	/**
	 * Returns if there is a command to toggle freezerendering
	 */
	virtual bool HasToggleFreezeCommand() { return false; };

	/**
	* Reads the viewport's displayed pixels into a preallocated color buffer.
	* @param OutImageData - RGBA8 values will be stored in this buffer
	* @param InRect - source rect of the image to capture
	* @return True if the read succeeded.
	*/
	ENGINE_API bool ReadPixels(TArray< FColor >& OutImageData,FReadSurfaceDataFlags InFlags = FReadSurfaceDataFlags(RCM_UNorm, CubeFace_MAX), FIntRect InRect = FIntRect(0, 0, 0, 0) );

	/**
	* Reads the viewport's displayed pixels into a preallocated color buffer.
	* @param OutImageBytes - RGBA8 values will be stored in this buffer.  Buffer must be preallocated with the correct size!
	* @param InSrcRect InSrcRect not specified means the whole rect
	* @return True if the read succeeded.
	*/
	ENGINE_API bool ReadPixelsPtr(FColor* OutImageBytes, FReadSurfaceDataFlags InFlags = FReadSurfaceDataFlags(RCM_UNorm, CubeFace_MAX), FIntRect InSrcRect = FIntRect(0, 0, 0, 0));

	/**
	 * Reads the viewport's displayed pixels into the given color buffer.
	 * @param OutputBuffer - RGBA16F values will be stored in this buffer
	 * @param CubeFace - optional cube face for when reading from a cube render target
	 * @return True if the read succeeded.
	 */
	ENGINE_API bool ReadFloat16Pixels(TArray<FFloat16Color>& OutputBuffer,ECubeFace CubeFace=CubeFace_PosX);

	/**
	 * Reads the viewport's displayed pixels into the given color buffer.
	 * @param OutputBuffer - Linear color array to store the value
	 * @param CubeFace - optional cube face for when reading from a cube render target
	 * @return True if the read succeeded.
	 */
	ENGINE_API bool ReadLinearColorPixels(TArray<FLinearColor>& OutputBuffer, FReadSurfaceDataFlags InFlags = FReadSurfaceDataFlags(RCM_MinMax, CubeFace_MAX), FIntRect InRect = FIntRect(0, 0, 0, 0));

	ENGINE_API bool ReadLinearColorPixelsPtr(FLinearColor* OutImageBytes, FReadSurfaceDataFlags InFlags = FReadSurfaceDataFlags(RCM_MinMax, CubeFace_MAX), FIntRect InRect = FIntRect(0, 0, 0, 0));

protected:

	FTexture2DRHIRef RenderTargetTextureRHI;

	/**
	 * Reads the viewport's displayed pixels into a preallocated color buffer.
	 * @param OutImageBytes - RGBA16F values will be stored in this buffer.  Buffer must be preallocated with the correct size!
	 * @param CubeFace - optional cube face for when reading from a cube render target
	 * @return True if the read succeeded.
	 */
	bool ReadFloat16Pixels(FFloat16Color* OutImageBytes,ECubeFace CubeFace=CubeFace_PosX);
};


/**
* An interface to the platform-specific implementation of a UI frame for a viewport.
*/
class FViewportFrame
{
public:

	virtual class FViewport* GetViewport() = 0;
	virtual void ResizeFrame(uint32 NewSizeX,uint32 NewSizeY,EWindowMode::Type NewWindowMode) = 0;

	DEPRECATED(4.13, "The version of FViewportFrame::ResizeFrame that takes a position is deprecated (the position was never used). Please use the version that doesn't take a position instead.")
	void ResizeFrame(uint32 NewSizeX, uint32 NewSizeY, EWindowMode::Type NewWindowMode, int32, int32)
	{
		ResizeFrame(NewSizeX, NewSizeY, NewWindowMode);
	}
};

/**
* The maximum size that the hit proxy kernel is allowed to be set to
*/
#define MAX_HITPROXYSIZE 200

struct ENGINE_API FScreenshotRequest
{
	/**
	 * Requests a new screenshot.  Screenshot can be read from memory by subscribing
	 * to the ViewPort's OnScreenshotCaptured delegate.
	 *
	 * @param bInShowUI				Whether or not to show Slate UI
	 */
	static void RequestScreenshot(bool bInShowUI);

	/**
	 * Requests a new screenshot with a specific filename
	 *
	 * @param InFilename			The filename to use
	 * @param bInShowUI				Whether or not to show Slate UI
	 * @param bAddFilenameSuffix	Whether an auto-generated unique suffix should be added to the supplied filename
	 */
	static void RequestScreenshot(const FString& InFilename, bool bInShowUI, bool bAddFilenameSuffix);

	/**
	 * Resets a screenshot request
	 */
	static void Reset();

	/**
	 * @return The filename of the next screenshot
	 */
	static const FString& GetFilename() { return Filename; }

	/**
	 * @return True if a screenshot is requested
	 */
	static bool IsScreenshotRequested() { return bIsScreenshotRequested; }

	/**
	 * @return True if UI should be shown in the screenshot
	 */
	static bool ShouldShowUI() { return bShowUI; }

	/**
	 * Creates a new screenshot filename from the passed in filename template
	 */
	static void CreateViewportScreenShotFilename( FString& InOutFilename );

	/**
	 * Access a temporary color array for storing the pixel colors for the highres screenshot mask
	 */
	static TArray<FColor>* GetHighresScreenshotMaskColorArray();

private:
	static bool bIsScreenshotRequested;
	static FString NextScreenshotName;
	static FString Filename;
	static bool bShowUI;
	static TArray<FColor> HighresScreenshotMaskColorArray;
};

/** Data needed to display perframe stat tracking when STAT UNIT is enabled */
struct FStatUnitData
{
	/** Unit frame times filtered with a simple running average */
	float RenderThreadTime;
	float GameThreadTime;
	float GPUFrameTime;
	float FrameTime;

	/** Raw equivalents of the above variables */
	float RawRenderThreadTime;
	float RawGameThreadTime;
	float RawGPUFrameTime;
	float RawFrameTime;

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	float VxgiWorldSpaceTime;
	float VxgiScreenSpaceTime;
	float RawVxgiWorldSpaceTime;
	float RawVxgiScreenSpaceTime;
#endif
	// NVCHANGE_END: Add VXGI

	/** Time that has transpired since the last draw call */
	double LastTime;

#if !UE_BUILD_SHIPPING
	static const int32 NumberOfSamples = 200;

	int32 CurrentIndex;
	TArray<float> RenderThreadTimes;
	TArray<float> GameThreadTimes;
	TArray<float> GPUFrameTimes;
	TArray<float> FrameTimes;
#endif //!UE_BUILD_SHIPPING

	FStatUnitData()
		: RenderThreadTime(0.0f)
		, GameThreadTime(0.0f)
		, GPUFrameTime(0.0f)
		, FrameTime(0.0f)
		, RawRenderThreadTime(0.0f)
		, RawGameThreadTime(0.0f)
		, RawGPUFrameTime(0.0f)
		, RawFrameTime(0.0f)
		, LastTime(0.0)
		// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
		, VxgiWorldSpaceTime(0.0f)
		, VxgiScreenSpaceTime(0.0f)
		, RawVxgiWorldSpaceTime(0.0f)
		, RawVxgiScreenSpaceTime(0.0f)
#endif
		// NVCHANGE_END: Add VXGI
	{
#if !UE_BUILD_SHIPPING
		CurrentIndex = 0;
		RenderThreadTimes.AddZeroed(NumberOfSamples);
		GameThreadTimes.AddZeroed(NumberOfSamples);
		GPUFrameTimes.AddZeroed(NumberOfSamples);
		FrameTimes.AddZeroed(NumberOfSamples);
#endif //!UE_BUILD_SHIPPING
	}

	/** Render function to display the stat */
	int32 DrawStat(FViewport* InViewport, FCanvas* InCanvas, int32 InX, int32 InY);
};

/** Data needed to display perframe stat tracking when STAT HITCHES is enabled */
struct FStatHitchesData
{
	double LastTime;

	static const int32 NumHitches = 20;
	TArray<float> Hitches;
	TArray<double> When;
	int32 OverwriteIndex;
	int32 Count;

	FStatHitchesData()
		: LastTime(0.0)
		, OverwriteIndex(0)
		, Count(0)
	{
		Hitches.AddZeroed(NumHitches);
		When.AddZeroed(NumHitches);
	}

	/** Render function to display the stat */
	int32 DrawStat(FViewport* InViewport, FCanvas* InCanvas, int32 InX, int32 InY);
};

/**
 * Encapsulates the I/O of a viewport.
 * The viewport display is implemented using the platform independent RHI.
 */
class FViewport : public FRenderTarget, protected FRenderResource
{
public:
	/** delegate type for viewport resize events ( Params: FViewport* Viewport, uint32 ) */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnViewportResized, FViewport*, uint32);
	/** Send when a viewport is resized */
	ENGINE_API static FOnViewportResized ViewportResizedEvent;

	// Constructor.
	ENGINE_API FViewport(FViewportClient* InViewportClient);
	// Destructor
	ENGINE_API virtual ~FViewport();

	//~ Begin FViewport Interface.
	virtual void* GetWindow() = 0;
	virtual void MoveWindow(int32 NewPosX, int32 NewPosY, int32 NewSizeX, int32 NewSizeY) = 0;

	virtual void Destroy() = 0;

	// New MouseCapture/MouseLock API
	virtual bool HasMouseCapture() const				{ return true; }
	virtual bool HasFocus() const					{ return true; }
	virtual bool IsForegroundWindow() const			{ return true; }
	virtual void CaptureMouse( bool bCapture )		{ }
	virtual void LockMouseToViewport( bool bLock )	{ }
	virtual void ShowCursor( bool bVisible)			{ }
	virtual bool UpdateMouseCursor(bool bSetCursor)	{ return true; }

	virtual void ShowSoftwareCursor( bool bVisible )		{ }
	virtual void SetSoftwareCursorPosition( FVector2D Position ) { }
	virtual bool IsSoftwareCursorVisible() const { return false; }

	/**
	 * Returns true if the mouse cursor is currently visible
	 *
	 * @return True if the mouse cursor is currently visible, otherwise false.
	 */
	virtual bool IsCursorVisible() const { return true; }

	virtual bool SetUserFocus(bool bFocus) = 0;
	virtual bool KeyState(FKey Key) const = 0;
	virtual int32 GetMouseX() const = 0;
	virtual int32 GetMouseY() const = 0;
	virtual void GetMousePos(FIntPoint& MousePosition, const bool bLocalPosition = true) = 0;
	virtual float GetTabletPressure() { return 0.f; }
	virtual bool IsPenActive() { return false; }
	virtual void SetMouse(int32 x, int32 y) = 0;
	virtual bool IsFullscreen()	const { return WindowMode == EWindowMode::Fullscreen || WindowMode == EWindowMode::WindowedFullscreen; }
	virtual EWindowMode::Type GetWindowMode()	const { return WindowMode; }
	virtual void ProcessInput( float DeltaTime ) = 0;

	/**
	 * Transforms a virtual desktop pixel (the origin is in the primary screen's top left corner) to the local space of this viewport
	 *
	 * @param VirtualDesktopPointPx   Coordinate on the virtual desktop in pixel units. Desktop is considered virtual because multiple monitors are supported.
	 *
	 * @return The transformed pixel. It is normalized to the range [0, 1]
	 */
	virtual FVector2D VirtualDesktopPixelToViewport(FIntPoint VirtualDesktopPointPx) const = 0;

	/**
	 * Transforms a coordinate in the local space of this viewport into a virtual desktop pixel.
	 *
	 * @param ViewportCoordiante    Normalized coordniate in the rate [0..1]; (0,0) is upper left and (1,1) is lower right.
	 *
	 * @return the transformed coordinate. It is in virtual desktop pixels.
	 */
	virtual FIntPoint ViewportToVirtualDesktopPixel(FVector2D ViewportCoordinate) const = 0;

	/**
	 * @return A canvas that can be used while this viewport is being drawn to render debug elements on top of everything else
	 */
	virtual FCanvas* GetDebugCanvas() { return NULL; };

	/**
	 * Indicate that the viewport should be block for vsync.
	 */
	virtual void SetRequiresVsync(bool bShouldVsync) {}

	/**
	 * Sets PreCapture coordinates from the current position of the slate cursor.
	 */
	virtual void SetPreCaptureMousePosFromSlateCursor() {}

	/**
	 *	Starts a new rendering frame. Called from the game thread thread.
	 */
	ENGINE_API virtual void	EnqueueBeginRenderFrame();

	/**
	 *	Starts a new rendering frame. Called from the rendering thread.
	 */
	ENGINE_API virtual void	BeginRenderFrame(FRHICommandListImmediate& RHICmdList);

	/**
	 *	Ends a rendering frame. Called from the rendering thread.
	 *	@param bPresent		Whether the frame should be presented to the screen
	 *	@param bLockToVsync	Whether the GPU should block until VSYNC before presenting
	 */
	ENGINE_API virtual void	EndRenderFrame(FRHICommandListImmediate& RHICmdList, bool bPresent, bool bLockToVsync);

	/**
	 * @return whether or not this Controller has a keyboard available to be used
	 **/
	virtual bool IsKeyboardAvailable( int32 ControllerID ) const { return true; }

	/**
	 * @return whether or not this Controller has a mouse available to be used
	 **/
	virtual bool IsMouseAvailable( int32 ControllerID ) const { return true; }


	/**
	 * @return aspect ratio that this viewport should be rendered at
	 */
	virtual float GetDesiredAspectRatio() const
	{
		FIntPoint Size = GetSizeXY();
		return (float)Size.X / (float)Size.Y;
	}

	/**
	 * Invalidates the viewport's displayed pixels.
	 */
	virtual void InvalidateDisplay() = 0;

	/**
	 * Updates the viewport's displayed pixels with the results of calling ViewportClient->Draw.
	 *
	 * @param	bShouldPresent	Whether we want this frame to be presented
	 */
	ENGINE_API void Draw( bool bShouldPresent = true );

	/**
	 * Invalidates the viewport's cached hit proxies at the end of the frame.
	 */
	ENGINE_API virtual void DeferInvalidateHitProxy();

	/**
	 * Invalidates cached hit proxies
	 */
	ENGINE_API void InvalidateHitProxy();

	/**
	 * Invalidates cached hit proxies and the display.
	 */
	ENGINE_API void Invalidate();

	ENGINE_API const TArray<FColor>& GetRawHitProxyData(FIntRect InRect);

	/**
	 * Copies the hit proxies from an area of the screen into a buffer.
	 * InRect must be entirely within the viewport's client area.
	 * If the hit proxies are not cached, this will call ViewportClient->Draw with a hit-testing canvas.
	 */
	ENGINE_API void GetHitProxyMap(FIntRect InRect, TArray<HHitProxy*>& OutMap);

	/**
	 * Returns the dominant hit proxy at a given point.  If X,Y are outside the client area of the viewport, returns NULL.
	 * Caution is required as calling Invalidate after this will free the returned HHitProxy.
	 */
	ENGINE_API HHitProxy* GetHitProxy(int32 X,int32 Y);

	/**
	 * Retrieves the interface to the viewport's frame, if it has one.
	 * @return The viewport's frame interface.
	 */
	virtual FViewportFrame* GetViewportFrame() = 0;

	/**
	 * Calculates the view inside the viewport when the aspect ratio is locked.
	 * Used for creating cinematic bars.
	 * @param Aspect [in] ratio to lock to
	 * @param ViewRect [in] unconstrained view rectangle
	 * @return	constrained view rectangle
	 */
	ENGINE_API FIntRect CalculateViewExtents(float AspectRatio, const FIntRect& ViewRect);

	/**
	 *	Sets a viewport client if one wasn't provided at construction time.
	 *	@param InViewportClient	- The viewport client to set.
	 **/
	ENGINE_API virtual void SetViewportClient( FViewportClient* InViewportClient );

	//~ Begin FRenderTarget Interface.
	virtual FIntPoint GetSizeXY() const override { return FIntPoint(SizeX, SizeY); }

	// Accessors.
	FViewportClient* GetClient() const { return ViewportClient; }

	/**
	 * Globally enables/disables rendering
	 *
	 * @param bIsEnabled true if drawing should occur
	 * @param PresentAndStopMovieDelay Number of frames to delay before enabling bPresent in RHIEndDrawingViewport, and before stopping the movie
	 */
	ENGINE_API static void SetGameRenderingEnabled(bool bIsEnabled, int32 PresentAndStopMovieDelay=0);

	/**
	 * Returns whether rendering is globally enabled or disabled.
	 * @return	true if rendering is globally enabled, otherwise false.
	 **/
	ENGINE_API static bool IsGameRenderingEnabled() { return bIsGameRenderingEnabled; }

	/**
	 * Handles freezing/unfreezing of rendering
	 */
	ENGINE_API virtual void ProcessToggleFreezeCommand() override;

	/**
	 * Returns if there is a command to freeze
	 */
	ENGINE_API virtual bool HasToggleFreezeCommand() override;

	/**
	* Accessors for RHI resources
	*/
	const FViewportRHIRef& GetViewportRHI() const { return ViewportRHI; }

	/**
	 * Update the render target surface RHI to the current back buffer
	 */
	void UpdateRenderTargetSurfaceRHIToCurrentBackBuffer();

	/**
	 * First chance for viewports to render custom stats text
	 * @param InCanvas - Canvas for rendering
	 * @param InX - Starting X for drawing
	 * @param InY - Starting Y for drawing
	 * @return - Y for next stat drawing
	 */
	virtual int32 DrawStatsHUD (FCanvas* InCanvas, const int32 InX, const int32 InY)
	{
		return InY;
	}

	/**
	 * Sets the initial size of this viewport.  Will do nothing if the viewport has already been sized
	 *
	 * @param InitialSizeXY	The initial size of the viewport
	 */
	ENGINE_API void SetInitialSize( FIntPoint InitialSizeXY );

	/** Returns true if the viewport is for play in editor */
	bool IsPlayInEditorViewport() const
	{
		return bIsPlayInEditorViewport;
	}

	/** Sets this viewport as a play in editor viewport */
	void SetPlayInEditorViewport( bool bInPlayInEditorViewport )
	{
		bIsPlayInEditorViewport = bInPlayInEditorViewport;
	}

	/** Returns true if this is an FSlateSceneViewport */
	bool IsSlateViewport() const { return bIsSlateViewport; }

	/** The current version of the running instance */
	FString AppVersionString;

	/** Trigger a high res screenshot. Returns true if the screenshot can be taken, and false if it can't. The screenshot
      * can fail if the requested multiplier makes the screen too big for the GPU to cope with
  	 **/
	ENGINE_API bool TakeHighResScreenShot();

	/** Should return true, if stereo rendering is allowed in this viewport */
	virtual bool IsStereoRenderingAllowed() const { return false; }

	/** Returns dimensions of RenderTarget texture. Can be called on a game thread. */
	virtual FIntPoint GetRenderTargetTextureSizeXY() const { return GetSizeXY(); }

	/** Causes this viewport to flush rendering commands once it has been drawn */
	void IncrementFlushOnDraw() { ++FlushOnDrawCount; }

	/** Decrements a previously incremented count that caused this viewport to flush rendering commands when it was drawn */
	void DecrementFlushOnDraw() { check(FlushOnDrawCount); --FlushOnDrawCount; }

protected:

	/** The viewport's client. */
	FViewportClient* ViewportClient;

	/**
	 * Updates the viewport RHI with the current viewport state.
	 * @param bDestroyed - True if the viewport has been destroyed.
	 */
	ENGINE_API virtual void UpdateViewportRHI(bool bDestroyed, uint32 NewSizeX, uint32 NewSizeY, EWindowMode::Type NewWindowMode, EPixelFormat PreferredPixelFormat);

	/**
	 * Take a high-resolution screenshot and save to disk.
	 */
	void HighResScreenshot();

protected:

	/** A map from 2D coordinates to cached hit proxies. */
	class FHitProxyMap : public FHitProxyConsumer, public FRenderTarget, public FGCObject
	{
	public:

		/** Constructor */
		FHitProxyMap();

		/** Destructor */
		ENGINE_API virtual ~FHitProxyMap();

		/** Initializes the hit proxy map with the given dimensions. */
		void Init(uint32 NewSizeX,uint32 NewSizeY);

		/** Releases the hit proxy resources. */
		void Release();

		/** Invalidates the cached hit proxy map. */
		void Invalidate();

		//~ Begin FHitProxyConsumer Interface.
		virtual void AddHitProxy(HHitProxy* HitProxy) override;

		//~ Begin FRenderTarget Interface.
		virtual FIntPoint GetSizeXY() const override { return FIntPoint(SizeX, SizeY); }

		/** FGCObject interface */
		virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

		const FTexture2DRHIRef& GetHitProxyTexture(void) const		{ return HitProxyTexture; }
		const FTexture2DRHIRef& GetHitProxyCPUTexture(void) const		{ return HitProxyCPUTexture; }

	private:

		/** The width of the hit proxy map. */
		uint32 SizeX;

		/** The height of the hit proxy map. */
		uint32 SizeY;

		/** References to the hit proxies cached by the hit proxy map. */
		TArray<TRefCountPtr<HHitProxy> > HitProxies;

		FTexture2DRHIRef HitProxyTexture;
		FTexture2DRHIRef HitProxyCPUTexture;
	};

	/** The viewport's hit proxy map. */
	FHitProxyMap HitProxyMap;

	/** Cached hit proxy data. */
	TArray<FColor> CachedHitProxyData;

	/** The RHI viewport. */
	FViewportRHIRef ViewportRHI;

	/** The width of the viewport. */
	uint32 SizeX;

	/** The height of the viewport. */
	uint32 SizeY;

	/** The size of the region to check hit proxies */
	uint32 HitProxySize;

	/** What is the current window mode. */
	EWindowMode::Type WindowMode;

	/** True if the viewport client requires hit proxy storage. */
	uint32 bRequiresHitProxyStorage : 1;

	/** True if the hit proxy buffer buffer has up to date hit proxies for this viewport. */
	uint32 bHitProxiesCached : 1;

	/** If a toggle freeze request has been made */
	uint32 bHasRequestedToggleFreeze : 1;

	/** if true  this viewport is for play in editor */
	uint32 bIsPlayInEditorViewport : 1;

	/** If true this viewport is an FSlateSceneViewport */
	uint32 bIsSlateViewport : 1;

	/** The number of pending calls to IncrementFlushOnDraw. Non-zero implies this viewport will flush rendering commands when it is drawn. */
	uint32 FlushOnDrawCount;

	/** true if we should draw game viewports (has no effect on Editor viewports) */
	ENGINE_API static bool bIsGameRenderingEnabled;

	/** Delay in frames to disable present (but still render scene) and stopping of a movie. This is useful to keep playing a movie while driver caches things on the first frame, which can be slow. */
	static int32 PresentAndStopMovieDelay;

	/** Triggers the taking of a high res screen shot for this viewport. */
	bool bTakeHighResScreenShot;
	//~ Begin FRenderResource Interface.
	ENGINE_API virtual void InitDynamicRHI() override;
	ENGINE_API virtual void ReleaseDynamicRHI() override;
	ENGINE_API virtual void InitRHI() override;
	ENGINE_API virtual void ReleaseRHI() override;
};

// Shortcuts for checking the state of both left&right variations of control keys.
extern ENGINE_API bool IsCtrlDown(FViewport* Viewport);
extern ENGINE_API bool IsShiftDown(FViewport* Viewport);
extern ENGINE_API bool IsAltDown(FViewport* Viewport);

extern ENGINE_API bool GetViewportScreenShot(FViewport* Viewport, TArray<FColor>& Bitmap, const FIntRect& ViewRect = FIntRect());
extern ENGINE_API bool GetHighResScreenShotInput(const TCHAR* Cmd, FOutputDevice& Ar, uint32& OutXRes, uint32& OutYRes, float& OutResMult, FIntRect& OutCaptureRegion, bool& OutShouldEnableMask, bool& OutDumpBufferVisualizationTargets, bool& OutCaptureHDR, FString& OutFilenameOverride);

/**
 * An abstract interface to a viewport's client.
 * The viewport's client processes input received by the viewport, and draws the viewport.
 */
class FViewportClient
{
public:
	/** The different types of sound stat flags */
	struct ESoundShowFlags
	{
		enum Type
		{
			Disabled = 0x00,
			Debug = 0x01,
			Sort_Distance = 0x02,
			Sort_Class = 0x04,
			Sort_Name = 0x08,
			Sort_WavesNum = 0x10,
			Sort_Disabled = 0x20,
			Long_Names = 0x40,
		};
	};

	virtual ~FViewportClient(){}
	virtual void Precache() {}
	virtual void RedrawRequested(FViewport* Viewport) { Viewport->Draw(); }
	virtual void RequestInvalidateHitProxy(FViewport* Viewport) { Viewport->InvalidateHitProxy(); }
	virtual void Draw(FViewport* Viewport,FCanvas* Canvas) {}
	virtual void ProcessScreenShots(FViewport* Viewport) {}
	virtual UWorld* GetWorld() const { return NULL; }
	virtual struct FEngineShowFlags* GetEngineShowFlags() { return NULL; }

	/**
	 * Check a key event received by the viewport.
	 * If the viewport client uses the event, it should return true to consume it.
	 * @param	Viewport - The viewport which the key event is from.
	 * @param	ControllerId - The controller which the key event is from.
	 * @param	Key - The name of the key which an event occured for.
	 * @param	Event - The type of event which occured.
	 * @param	AmountDepressed - For analog keys, the depression percent.
	 * @param	bGamepad - input came from gamepad (ie xbox controller)
	 * @return	True to consume the key event, false to pass it on.
	 */
	virtual bool InputKey(FViewport* Viewport,int32 ControllerId,FKey Key,EInputEvent Event,float AmountDepressed = 1.f,bool bGamepad=false) { return false; }

	/**
	 * Check an axis movement received by the viewport.
	 * If the viewport client uses the movement, it should return true to consume it.
	 * @param	Viewport - The viewport which the axis movement is from.
	 * @param	ControllerId - The controller which the axis movement is from.
	 * @param	Key - The name of the axis which moved.
	 * @param	Delta - The axis movement delta.
	 * @param	DeltaTime - The time since the last axis update.
	 * @param	NumSamples - The number of device samples that contributed to this Delta, useful for things like smoothing
	 * @param	bGamepad - input came from gamepad (ie xbox controller)
	 * @return	True to consume the axis movement, false to pass it on.
	 */
	virtual bool InputAxis(FViewport* Viewport,int32 ControllerId,FKey Key,float Delta,float DeltaTime,int32 NumSamples=1,bool bGamepad=false) { return false; }

	/**
	 * Check a character input received by the viewport.
	 * If the viewport client uses the character, it should return true to consume it.
	 * @param	Viewport - The viewport which the axis movement is from.
	 * @param	ControllerId - The controller which the axis movement is from.
	 * @param	Character - The character.
	 * @return	True to consume the character, false to pass it on.
	 */
	virtual bool InputChar(FViewport* Viewport,int32 ControllerId,TCHAR Character) { return false; }

	/**
	 * Check a key event received by the viewport.
	 * If the viewport client uses the event, it should return true to consume it.
	 * @param	Viewport - The viewport which the event is from.
	 * @param	ControllerId - The controller which the key event is from.
	 * @param	Handle - Identifier unique to this touch event
	 * @param	Type - What kind of touch event this is (see ETouchType)
	 * @param	TouchLocation - Screen position of the touch
	 * @param	DeviceTimestamp - Timestamp of the event
	 * @param	TouchpadIndex - For devices with multiple touchpads, this is the index of which one
	 * @return	True to consume the key event, false to pass it on.
	 */
	virtual bool InputTouch(FViewport* Viewport, int32 ControllerId, uint32 Handle, ETouchType::Type Type, const FVector2D& TouchLocation, FDateTime DeviceTimestamp, uint32 TouchpadIndex) { return false; }

	/**
	 * Check a gesture event received by the viewport.
	 * If the viewport client uses the event, it should return true to consume it.
	 * @param	Viewport - The viewport which the event is from.
	 * @param	GestureType - @todo desc
	 * @param	GestureDelta - @todo desc
	 * @return	True to consume the gesture event, false to pass it on.
	 */
	virtual bool InputGesture(FViewport* Viewport, EGestureEvent GestureType, const FVector2D& GestureDelta, bool bIsDirectionInvertedFromDevice) { return false; }

	/**
	 * Each frame, the input system will update the motion data.
	 *
	 * @param Viewport - The viewport which the key event is from.
	 * @param ControllerId - The controller which the key event is from.
	 * @param Tilt			The current orientation of the device
	 * @param RotationRate	How fast the tilt is changing
	 * @param Gravity		Describes the current gravity of the device
	 * @param Acceleration  Describes the acceleration of the device
	 * @return	True to consume the motion event, false to pass it on.
	 */
	virtual bool InputMotion(FViewport* Viewport, int32 ControllerId, const FVector& Tilt, const FVector& RotationRate, const FVector& Gravity, const FVector& Acceleration) { return false; }

	virtual void SetIsSimulateInEditorViewport(bool bInIsSimulateInEditorViewport) { };

	virtual bool WantsPollingMouseMovement(void) const { return true; }

	virtual void MouseEnter( FViewport* Viewport,int32 x, int32 y ) {}

	virtual void MouseLeave( FViewport* Viewport ) {}

	virtual void MouseMove(FViewport* Viewport,int32 X,int32 Y) {}

	/**
	 * Called when the mouse is moved while a window input capture is in effect
	 *
	 * @param	InViewport	Viewport that captured the mouse input
	 * @param	InMouseX	New mouse cursor X coordinate
	 * @param	InMouseY	New mouse cursor Y coordinate
	 */
	virtual void CapturedMouseMove( FViewport* InViewport, int32 InMouseX, int32 InMouseY ) { }

	/**
	 * Retrieves the cursor that should be displayed by the OS
	 *
	 * @param	Viewport	the viewport that contains the cursor
	 * @param	X			the x position of the cursor
	 * @param	Y			the Y position of the cursor
	 *
	 * @return	the cursor that the OS should display
	 */
	virtual EMouseCursor::Type GetCursor(FViewport* Viewport,int32 X,int32 Y) { return EMouseCursor::Default; }

	/**
	 * Called to map a cursor reply to an actual widget to render.
	 *
	 * @return	the widget that should be rendered for this cursor, return TOptional<TSharedRef<SWidget>>() if no mapping.
	 */
	virtual TOptional<TSharedRef<SWidget>> MapCursor(FViewport* Viewport, const FCursorReply& CursorReply) { return TOptional<TSharedRef<SWidget>>(); }

	/**
	 * Called to determine if we should render the focus brush.
	 *
	 * @param InFocusCause	The cause of focus
	 */
	virtual TOptional<bool> QueryShowFocus(const EFocusCause InFocusCause) const { return TOptional<bool>(); }

	virtual void LostFocus(FViewport* Viewport) {}
	virtual void ReceivedFocus(FViewport* Viewport) {}
	virtual bool IsFocused(FViewport* Viewport) { return true; }

	virtual void Activated(FViewport* Viewport, const FWindowActivateEvent& InActivateEvent) {}
	virtual void Deactivated(FViewport* Viewport, const FWindowActivateEvent& InActivateEvent) {}

	/**
	 * Called when the top level window associated with the viewport has been requested to close.
	 * At this point, the viewport has not been closed and the operation may be canceled.
	 * This may not called from PIE, Editor Windows, on consoles, or before the game ends
	 * from other methods.
	 * This is only when the platform specific window is closed.
	 *
	 * @return True if the viewport may be closed, false otherwise.
	 */
	virtual bool WindowCloseRequested() { return true; }

	virtual void CloseRequested(FViewport* Viewport) {}

	virtual bool RequiresHitProxyStorage() { return true; }

	/**
	 * Determines whether this viewport client should receive calls to InputAxis() if the game's window is not currently capturing the mouse.
	 * Used by the UI system to easily receive calls to InputAxis while the viewport's mouse capture is disabled.
	 */
	virtual bool RequiresUncapturedAxisInput() const { return false; }

	/**
	* Determine if the viewport client is going to need any keyboard input
	* @return true if keyboard input is needed
	*/
	virtual bool RequiresKeyboardInput() const { return true; }

	/**
	 * Returns true if this viewport is orthogonal.
	 * If hit proxies are ever used in-game, this will need to be
	 * overridden correctly in GameViewportClient.
	 */
	virtual bool IsOrtho() const { return false; }

	/**
	 * Returns true if this viewport is excluding non-game elements from its display
	 */
	virtual bool IsInGameView() const { return false; }

	/**
	 * Sets GWorld to the appropriate world for this client
	 *
	 * @return the previous GWorld
	 */
	virtual class UWorld* ConditionalSetWorld() { return NULL; }

	/**
	 * Restores GWorld to InWorld
	 *
	 * @param InWorld	The world to restore
	 */
	virtual void ConditionalRestoreWorld( class UWorld* InWorld ) {}

	/**
	 * Allow viewport client to override the current capture region
	 *
	 * @param OutCaptureRegion    Ref to rectangle where we will write the overridden region
	 * @return true if capture region has been overridden, false otherwise
	 */
	virtual bool OverrideHighResScreenshotCaptureRegion(FIntRect& OutCaptureRegion) { return false; }

	/**
	 * Get a ptr to the stat unit data for this viewport
	 */
	virtual FStatUnitData* GetStatUnitData() const { return NULL; }

	/**
	* Get a ptr to the stat unit data for this viewport
	*/
	virtual FStatHitchesData* GetStatHitchesData() const { return NULL; }

	/**
	 * Get a ptr to the enabled stats list
	 */
	virtual const TArray<FString>* GetEnabledStats() const { return NULL; }

	/**
	 * Sets all the stats that should be enabled for the viewport
	 */
	virtual void SetEnabledStats(const TArray<FString>& InEnabledStats) {}

	/**
	 * Check whether a specific stat is enabled for this viewport
	 */
	virtual bool IsStatEnabled(const FString& InName) const { return false; }

	/**
	* Sets whether stats should be visible for the viewport
	*/
	virtual void SetShowStats(bool bWantStats) { }

	/**
	 * Get the sound stat flags enabled for this viewport
	 */
	virtual ESoundShowFlags::Type GetSoundShowFlags() const { return ESoundShowFlags::Disabled; }

	/**
	 * Set the sound stat flags enabled for this viewport
	 */
	virtual void SetSoundShowFlags(const ESoundShowFlags::Type InSoundShowFlags) {}

	/**
	 * Check whether we should ignore input.
	 */
	virtual bool IgnoreInput() { return false; }

	/**
	 * Gets the mouse capture behavior when the viewport is clicked
	 */
	virtual EMouseCaptureMode CaptureMouseOnClick() { return EMouseCaptureMode::CapturePermanently; }

	/**
	 * Gets whether or not the viewport captures the Mouse on launch of the application
	 * Technically this controls capture on the first window activate, so in situations
	 * where the application is launched but isn't activated the effect is delayed until
	 * activation.
	 */
	virtual bool CaptureMouseOnLaunch() { return true; }

	/**
	 * Gets whether or not the cursor is locked to the viewport when the viewport captures the mouse
	 */
	virtual bool LockDuringCapture() { return true; }

	/**
	 * Gets whether or not the cursor should always be locked to the viewport.
	 */
	virtual bool ShouldAlwaysLockMouse() { return false; }

	/**
	 * Gets whether or not the cursor is hidden when the viewport captures the mouse
	 */
	virtual bool HideCursorDuringCapture() { return false; }

	/** 
	 * Should we make new windows for popups or create an overlay in the current window.
	 */
	virtual FPopupMethodReply OnQueryPopupMethod() const { return FPopupMethodReply::Unhandled(); }

	/**
	 * Optionally do custom handling of a navigation. 
	 */
	virtual bool HandleNavigation(const uint32 InUserIndex, TSharedPtr<SWidget> InDestination) { return false; }
};

/** Tracks the viewport client that should process the stat command, can be NULL */
extern ENGINE_API class FCommonViewportClient* GStatProcessingViewportClient;

/**
 * Common functionality for game and editor viewport clients
 */

class FCommonViewportClient : public FViewportClient
{
public:
	FCommonViewportClient()
#if WITH_EDITOR
		: EditorScreenPercentage(100)
		, bShouldUpdateScreenPercentage(true)
#endif
	{}

	virtual ~FCommonViewportClient()
	{
		//make to clean up the global "stat" client when we delete the active one.
		if (GStatProcessingViewportClient == this)
		{
			GStatProcessingViewportClient = NULL;
		}
	}

#if WITH_EDITOR
	/** Tells this viewport to update editor screen percentage when safe */
	ENGINE_API void RequestUpdateEditorScreenPercentage();
	/** @return the current screen percentage to be used for scene rendering in this client */
	ENGINE_API TOptional<float> GetEditorScreenPercentage();
#endif

	ENGINE_API void DrawHighResScreenshotCaptureRegion(FCanvas& Canvas);

protected:
	/** @return the DPI scale of the window that the viewport client is in */
	virtual float GetViewportClientWindowDPIScale() const { return 1.0; }

private:

#if WITH_EDITOR
	/** Screen percentage to apply to the scene view in editor only (to adjust for DPI scale). 
	 * Note: This is still used in game viewports for PIE
	 */
	TOptional<float> EditorScreenPercentage;
	bool bShouldUpdateScreenPercentage;
#endif

};


/**
 * Minimal viewport for assisting with taking screenshots (also used within a plugin)
 * @todo: This should be refactored
 */
class FDummyViewport : public FViewport
{
public:
	ENGINE_API FDummyViewport(FViewportClient* InViewportClient);

	virtual ~FDummyViewport();

	//~ Begin FViewport Interface
	virtual void BeginRenderFrame(FRHICommandListImmediate& RHICmdList) override
	{
		check( IsInRenderingThread() );
		SetRenderTarget(RHICmdList,  RenderTargetTextureRHI,  FTexture2DRHIRef() );
	};

	virtual void EndRenderFrame(FRHICommandListImmediate& RHICmdList, bool bPresent, bool bLockToVsync) override
	{
		check( IsInRenderingThread() );
	}

	virtual void*	GetWindow() override { return 0; }
	virtual void	MoveWindow(int32 NewPosX, int32 NewPosY, int32 NewSizeX, int32 NewSizeY) override {}
	virtual void	Destroy() override {}
	virtual bool SetUserFocus(bool bFocus) override { return false; }
	virtual bool	KeyState(FKey Key) const override { return false; }
	virtual int32	GetMouseX() const override { return 0; }
	virtual int32	GetMouseY() const override { return 0; }
	virtual void	GetMousePos( FIntPoint& MousePosition, const bool bLocalPosition = true) override { MousePosition = FIntPoint(0, 0); }
	virtual void	SetMouse(int32 x, int32 y) override { }
	virtual void	ProcessInput( float DeltaTime ) override { }
	virtual FVector2D VirtualDesktopPixelToViewport(FIntPoint VirtualDesktopPointPx) const override { return FVector2D::ZeroVector; }
	virtual FIntPoint ViewportToVirtualDesktopPixel(FVector2D ViewportCoordinate) const override { return FIntPoint::ZeroValue; }
	virtual void InvalidateDisplay() override { }
	virtual void DeferInvalidateHitProxy() override { }
	virtual FViewportFrame* GetViewportFrame() override { return 0; }
	virtual FCanvas* GetDebugCanvas() override { return DebugCanvas; }
	//~ End FViewport Interface

	//~ Begin FRenderResource Interface
	virtual void InitDynamicRHI() override
	{
		FTexture2DRHIRef ShaderResourceTextureRHI;

		FRHIResourceCreateInfo CreateInfo;
		RHICreateTargetableShaderResource2D( SizeX, SizeY, PF_A2B10G10R10, 1, TexCreate_None, TexCreate_RenderTargetable, false, CreateInfo, RenderTargetTextureRHI, ShaderResourceTextureRHI );
	}

	// @todo UE4 DLL: Without these functions we get unresolved linker errors with FRenderResource
	virtual void InitRHI() override{}
	virtual void ReleaseRHI() override{}
	virtual void InitResource() override{ FViewport::InitResource(); }
	virtual void ReleaseResource() override { FViewport::ReleaseResource(); }
	virtual FString GetFriendlyName() const override { return FString(TEXT("FDummyViewport"));}
	//~ End FRenderResource Interface
private:
	FCanvas* DebugCanvas;
};
