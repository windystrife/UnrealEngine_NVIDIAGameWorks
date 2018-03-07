// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Layout/Geometry.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Input/PopupMethodReply.h"
#include "Widgets/SWidget.h"
#include "Rendering/RenderingCommon.h"
#include "Textures/SlateShaderResource.h"
#include "UnrealClient.h"

class FCanvas;
class FDebugCanvasDrawer;
class FSlateRenderer;
class FSlateWindowElementList;
class SViewport;
class SWindow;

/** Called in FSceneViewport::ResizeFrame after ResizeViewport*/
DECLARE_DELEGATE_OneParam( FOnSceneViewportResize, FVector2D );

class SViewport;

/**
 * A viewport for use with Slate SViewport widgets.
 */
class ENGINE_API FSceneViewport : public FViewportFrame, public FViewport, public ISlateViewport, public IViewportRenderTargetProvider
{
public:
	FSceneViewport( FViewportClient* InViewportClient, TSharedPtr<SViewport> InViewportWidget );
	~FSceneViewport();

	virtual void* GetWindow() override { return NULL; }

	/** FViewport interface */
	virtual void MoveWindow(int32 NewPosX, int32 NewPosY, int32 NewSizeX, int32 NewSizeY) override {}
	virtual bool HasMouseCapture() const override;
	virtual bool HasFocus() const override;
	virtual bool IsForegroundWindow() const override;
	virtual void CaptureMouse( bool bCapture ) override;
	virtual void LockMouseToViewport( bool bLock ) override;
	virtual void ShowCursor( bool bVisible ) override;
	virtual void SetPreCaptureMousePosFromSlateCursor() override;
	virtual bool IsCursorVisible() const override { return bIsCursorVisible; }
	virtual void ShowSoftwareCursor( bool bVisible ) override { bIsSoftwareCursorVisible = bVisible; }
	virtual void SetSoftwareCursorPosition( FVector2D Position ) override { SoftwareCursorPosition = Position; }
	virtual bool IsSoftwareCursorVisible() const override { return bIsSoftwareCursorVisible; }
	virtual FVector2D GetSoftwareCursorPosition() const override { return SoftwareCursorPosition; }
	virtual FCanvas* GetDebugCanvas() override;
	virtual float GetDisplayGamma() const override;

	/** Gets the proper RenderTarget based on the current thread*/
	virtual const FTexture2DRHIRef& GetRenderTargetTexture() const;

	virtual void SetRenderTargetTextureRenderThread(FTexture2DRHIRef& RT);

	/**
	 * Captures or uncaptures the joystick
	 *
	 * @param Capture	true if we should capture, false if we should uncapture
	 */
	virtual bool SetUserFocus(bool bFocus) override;

	/**
	 * Returns the state of the provided key. 
	 *
	 * @param Key	The name of the key to check
	 *
	 * @return true if the key is pressed, false otherwise
	 */
	virtual bool KeyState(FKey Key) const override;

	/**
	 * @return The current X position of the mouse (in local space, relative to the viewports geometry)                 
	 */
	virtual int32 GetMouseX() const override;

	/**
	 * @return The current Y position of the mouse (in local space, relative to the viewports geometry)                 
	 */
	virtual int32 GetMouseY() const override;

	/**
	 * Sets MousePosition to the current mouse position 
	 *
	 * @param MousePosition	Populated with the current mouse position     
	 * @param bLocalPosition Indicates whether the mouse position returned should be in local or absolute space
	 */
	virtual void GetMousePos( FIntPoint& MousePosition, const bool bLocalPosition = true) override;

	/**
	 * Not implemented                   
	 */
	virtual void SetMouse( int32 X, int32 Y ) override;

	/**
	 * Additional input processing that happens every frame                   
	 */
	virtual void ProcessInput( float DeltaTime ) override;
	
	virtual FVector2D VirtualDesktopPixelToViewport(FIntPoint VirtualDesktopPointPx) const override;
	virtual FIntPoint ViewportToVirtualDesktopPixel(FVector2D ViewportCoordinate) const override;

	/**
	 * Called when the viewport should be invalidated and redrawn                   
	 */
	virtual void InvalidateDisplay() override;

	/**
	 * Invalidates the viewport's cached hit proxies at the end of the frame.
	 */
	virtual void DeferInvalidateHitProxy() override;

	/** FViewportFrame interface */
	virtual FViewport* GetViewport() override { return this; }
	virtual FViewportFrame* GetViewportFrame() override { return this; }

	/** @return The viewport widget being used */
	TWeakPtr<SViewport> GetViewportWidget() const { return ViewportWidget; }

	/** Called before BeginRenderFrame is enqueued */
	virtual void EnqueueBeginRenderFrame() override;

	/** Called when a frame starts to render */
	virtual void BeginRenderFrame(FRHICommandListImmediate& RHICmdList) override;

	/** 
	 * Called when a frame is done rendering
	 *
	 * @param bPresent	Not used in Slate viewports
	 * @param bLockToVsync	Not used in Slate viewports
	 */
	virtual void EndRenderFrame(FRHICommandListImmediate& RHICmdList, bool bPresent, bool bLockToVsync) override;

	/**
	 * Ticks the viewport
	 */
	virtual void Tick( const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime ) override;

	/**
	 * Performs a resize when in swapping viewports while viewing the play world.
	 *
	 * @param OtherViewport	The previously active viewport
	 */
	void OnPlayWorldViewportSwapped( const FSceneViewport& OtherViewport );

	/**
	 * Swaps the active stats with another viewports
	 *
	 * @param OtherViewport	The previously active viewport
	 */
	void SwapStatCommands(const FSceneViewport& OtherViewport);

	/**
	 * Indicate that the viewport should be block for vsync.
	 */
	virtual void SetRequiresVsync(bool bShouldVsync) override { bRequiresVsync = bShouldVsync; }

	/**
	 * Returns true if the viewport should be vsynced.
	 */
	virtual bool RequiresVsync() const override { return bRequiresVsync; }

	/**
	 * Called to resize the actual window where this viewport resides
	 *
	 * @param NewSizeX		The new width of the viewport
	 * @param NewSizeY		The new height of the viewport
	 * @param NewWindowMode	 What window mode should the viewport be resized to
	 */
	virtual void ResizeFrame(uint32 NewSizeX,uint32 NewSizeY,EWindowMode::Type NewWindowMode) override;

	DEPRECATED(4.13, "The version of FSceneViewport::ResizeFrame that takes a position is deprecated (the position was never used). Please use the version that doesn't take a position instead.")
	void ResizeFrame(uint32 NewSizeX, uint32 NewSizeY, EWindowMode::Type NewWindowMode, int32, int32)
	{
		ResizeFrame(NewSizeX, NewSizeY, NewWindowMode);
	}

	/**
	 *	Sets the Viewport resize delegate.
	 */
	void SetOnSceneViewportResizeDel(FOnSceneViewportResize InOnSceneViewportResize) 
	{ 
		OnSceneViewportResizeDel = InOnSceneViewportResize; 
	}

	/** 
	* Sets whether a PIE viewport takes mouse control on startup.
	* @param bGetsMouseControl Takes control if true, or not if false. 
	*/
	void SetPlayInEditorGetsMouseControl(const bool bGetsMouseControl)
	{
		bShouldCaptureMouseOnActivate = bGetsMouseControl;
	}

	void SetPlayInEditorIsSimulate(const bool bIsSimulate)
	{
		bPlayInEditorIsSimulate = bIsSimulate;
	}
	bool GetPlayInEditorIsSimulate() const
	{
		return bPlayInEditorIsSimulate;
	}

	/** Updates the viewport RHI with a new size and fullscreen flag */
	virtual void UpdateViewportRHI(bool bDestroyed, uint32 NewSizeX, uint32 NewSizeY, EWindowMode::Type NewWindowMode, EPixelFormat PreferredPixelFormat) override;

	/** ISlateViewport interface */
	virtual FSlateShaderResource* GetViewportRenderTargetTexture() const override;
	virtual void OnDrawViewport( const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) override;
	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) override;
	virtual TOptional<TSharedRef<SWidget>> OnMapCursor(const FCursorReply& CursorReply) override;
	virtual FReply OnMouseButtonDown( const FGeometry& InGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& InGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnMouseLeave( const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& InGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseWheel( const FGeometry& InGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent ) override;
	virtual FReply OnTouchStarted( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent ) override;
	virtual FReply OnTouchMoved( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent ) override;
	virtual FReply OnTouchEnded( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent ) override;
	virtual FReply OnTouchGesture( const FGeometry& MyGeometry, const FPointerEvent& InGestureEvent ) override;
	virtual FReply OnMotionDetected( const FGeometry& MyGeometry, const FMotionEvent& InMotionEvent ) override;
	virtual FPopupMethodReply OnQueryPopupMethod() const override;
	virtual bool HandleNavigation(const uint32 InUserIndex, TSharedPtr<SWidget> InDestination) override;
	virtual TOptional<bool> OnQueryShowFocus(const EFocusCause InFocusCause) const override;
	virtual void OnFinishedPointerInput() override;
	virtual FReply OnKeyDown( const FGeometry& InGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual FReply OnKeyUp( const FGeometry& InGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual FReply OnAnalogValueChanged( const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent ) override;
	virtual FReply OnKeyChar( const FGeometry& InGeometry, const FCharacterEvent& InCharacterEvent ) override;
	virtual FReply OnFocusReceived( const FFocusEvent& InFocusEvent ) override;
	virtual void OnFocusLost( const FFocusEvent& InFocusEvent ) override;
	virtual void OnViewportClosed() override;
	virtual FReply OnRequestWindowClose() override;
	virtual TWeakPtr<SWidget> GetWidget() override;
	virtual FReply OnViewportActivated(const FWindowActivateEvent& InActivateEvent) override;
	virtual void OnViewportDeactivated(const FWindowActivateEvent& InActivateEvent) override;
	virtual FIntPoint GetSize() const override { return GetSizeXY(); }
	
	void SetViewportSize(uint32 NewSizeX,uint32 NewSizeY);
	TSharedPtr<SWindow> FindWindow();

	/** Should return true, if stereo rendering is allowed in this viewport */
	virtual bool IsStereoRenderingAllowed() const override;

	/** Returns dimensions of RenderTarget texture. Can be called on a game thread. */
	virtual FIntPoint GetRenderTargetTextureSizeXY() const { return (RTTSize.X != 0) ? RTTSize : GetSizeXY(); }

	virtual FSlateShaderResource* GetViewportRenderTargetTexture() override;

	/** Get the cached viewport geometry. */
	const FGeometry& GetCachedGeometry() const { return CachedGeometry; }


	/** Set an optional display gamma to use for this viewport */
	void SetGammaOverride(const float InGammaOverride)
	{
		ViewportGammaOverride = InGammaOverride;
	};

private:
	/**
	 * Called when this viewport is destroyed
	 */
	void Destroy() override;

	// FRenderResource interface.
	virtual void InitDynamicRHI() override;
	virtual void ReleaseDynamicRHI() override;

	// @todo UE4 DLL: Without these functions we get unresolved linker errors with FRenderResource
	virtual void InitRHI() override {}
	virtual void ReleaseRHI() override {}
	virtual void InitResource() override { FViewport::InitResource(); }
	virtual void ReleaseResource() override { FViewport::ReleaseResource(); }
	virtual FString GetFriendlyName() const override { return FString(TEXT("FSlateSceneViewport"));}

	/**
	 * Called from Slate when the viewport should be resized
	 *
	 * @param NewSizeX		 The new width of the viewport
	 * @param NewSizeY		 The new height of the viewport
	 * @param NewWindowMode	 What window mode should the viewport be resized to
	 */
	virtual void ResizeViewport( uint32 NewSizeX,uint32 NewSizeY,EWindowMode::Type NewWindowMode );

	DEPRECATED(4.13, "The version of FSceneViewport::ResizeViewport that takes a position is deprecated (the position was never used). Please use the version that doesn't take a position instead.")
	void ResizeViewport(uint32 NewSizeX, uint32 NewSizeY, EWindowMode::Type NewWindowMode, int32, int32)
	{
		ResizeViewport(NewSizeX, NewSizeY, NewWindowMode);
	}

	/**
	 * Called from slate when input is finished for this frame, and we should process any accumulated mouse data.
	 */
	void ProcessAccumulatedPointerInput();

	/**
	 * Updates the cached mouse position from a mouse event
	 *
	 * @param InGeometry	The geometry of the viewport to convert to local space
	 * @param InMouseEvent	The mouse event containing the position of the mouse in absolute space
	 */
	void UpdateCachedMousePos( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent );

	/**
	 * Updates the cached viewport geometry
	 *
	 * @param InGeometry	The geometry of the viewport to convert to local space
	 * @param InMouseEvent	The mouse event containing the position of the mouse in absolute space
	 */
	void UpdateCachedGeometry( const FGeometry& InGeometry );

	/**
	 * Updates the KeyStateMap via the modifier keys from a mouse event.
	 * This ensures that the key state is correct after focus changes.
	 *
	 * @param InMouseEvent	The mouse event containing the current state of modifier keys.
	 */
	void UpdateModifierKeys( const FPointerEvent& InMouseEvent );

	/**
	 * Calls InputKey on the ViewportClient via the modifier keys.
	 * This ensures that the key state is correct just prior to focus change
	 *
	 * @param InKeysState	The key state containing the states of the modifier keys
	 */
	void ApplyModifierKeys( const FModifierKeysState& InKeysState );

	/** Utility function to create an FReply that properly gets Focus and capture based on the settings*/
	FReply AcquireFocusAndCapture(FIntPoint MousePosition);

	/** Utility function to figure out if we are currently a game viewport */
	bool IsCurrentlyGameViewport();

	void WindowRenderTargetUpdate(FSlateRenderer* Renderer, SWindow* Window);

	/** @return Returns true if we should always render to a separate render target (rather than rendering directly to the
	    viewport backbuffer, taking into account any temporary requirements of head-mounted displays */
	bool UseSeparateRenderTarget() const
	{
		return bUseSeparateRenderTarget || bForceSeparateRenderTarget;
	}

	/**
	 * Called right before a slate window is destroyed so we can free up the backbuffer resource before the window backing it is destroyed
	 */
	void OnWindowBackBufferResourceDestroyed(void* Backbuffer);

	/** 
	 * Called right before a backbuffer is resized. If this viewport is using this backbuffer 
	 * it will release its resource here
	 */
	void OnPreResizeWindowBackbuffer(void* Backbuffer);

	/** 
	 * Called right after a backbuffer is resized. This viewport will reaquire its backbuffer handle if needed
	 */
	void OnPostResizeWindowBackbuffer(void* Backbuffer);

private:
	/** An intermediate reply state that is reset whenever an input event is generated */
	FReply CurrentReplyState;
	/** A mapping of key names to their pressed state */
	TMap<FKey,bool> KeyStateMap;
	/** The last known mouse position in local space, -1, -1 if unknown */
	FIntPoint CachedMousePos;
	/** The last known geometry info */
	FGeometry CachedGeometry;
	/** Mouse position before the latest capture */
	FIntPoint PreCaptureMousePos;
	/**	The current position of the software cursor */
	FVector2D SoftwareCursorPosition;
	/**	Whether the software cursor should be drawn in the viewport */
	bool bIsSoftwareCursorVisible;	
	/** Draws the debug canvas in Slate */
	TSharedPtr<class FDebugCanvasDrawer, ESPMode::ThreadSafe> DebugCanvasDrawer;
	/** The Slate viewport widget where this viewport is drawn */
	TWeakPtr<SViewport> ViewportWidget;
	/** The number of input samples in X since input was was last processed */
	int32 NumMouseSamplesX;
	/** The number of input samples in Y since input was was last processed */
	int32 NumMouseSamplesY;
	/** The current mouse delta */
	FIntPoint MouseDelta;
	/** true if the cursor is currently visible */
	bool bIsCursorVisible;
	/** true if we had Capture when deactivated */
	bool bShouldCaptureMouseOnActivate;
	/** true if this viewport requires vsync. */
	bool bRequiresVsync;
	/** true if this viewport renders to a separate render target.  false to render directly to the windows back buffer */
	bool bUseSeparateRenderTarget;
	/** True if we should force use of a separate render target because the HMD needs it. */
	bool bForceSeparateRenderTarget;
	/** Whether or not we are currently resizing */
	bool bIsResizing;
	/** Delegate that is fired off in ResizeFrame after ResizeViewport */
	FOnSceneViewportResize OnSceneViewportResizeDel;
	/** Whether the PIE viewport is currently in simulate in editor mode */
	bool bPlayInEditorIsSimulate;
	/** Whether or not the cursor is hidden when the viewport captures the mouse */
	bool bCursorHiddenDueToCapture;
	/** Position the cursor was at when we hid it due to capture, so we can put it back afterwards */
	FIntPoint MousePosBeforeHiddenDueToCapture;
	/** Dimensions of RenderTarget texture. */
	FIntPoint RTTSize;

	/** Reprojection on some HMD RHI's requires ViewportTargets to be buffered */
	/** The render target used by Slate to draw the viewport.  Can be null if this viewport renders directly to the backbuffer */
	TArray<class FSlateRenderTargetRHI*> BufferedSlateHandles;
	TArray<FTexture2DRHIRef> BufferedRenderTargetsRHI;
	TArray<FTexture2DRHIRef> BufferedShaderResourceTexturesRHI;

	FTexture2DRHIRef RenderTargetTextureRenderThreadRHI;
	class FSlateRenderTargetRHI* RenderThreadSlateTexture;

	int32 NumBufferedFrames;
	int32 CurrentBufferedTargetIndex;
	int32 NextBufferedTargetIndex;

	/** Tracks the number of touches currently active on the viewport */
	int32 NumTouches;

	/** The optional gamma value to use for this viewport */
	TOptional<float> ViewportGammaOverride;
};
