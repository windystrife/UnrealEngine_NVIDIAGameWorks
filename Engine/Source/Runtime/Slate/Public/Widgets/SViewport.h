// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Input/NavigationReply.h"
#include "Input/PopupMethodReply.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Rendering/RenderingCommon.h"
#include "Widgets/SWindow.h"
#include "HittestGrid.h"

class FActiveTimerHandle;
class FPaintArgs;
class FSlateWindowElementList;

class SLATE_API SViewport
	: public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS( SViewport )
		: _Content()
		, _ShowEffectWhenDisabled(true)
		, _RenderDirectlyToWindow(false)
		, _EnableGammaCorrection(true)
		, _ReverseGammaCorrection(false)
		, _EnableBlending(false)
		, _EnableStereoRendering(false)
		, _PreMultipliedAlpha(true)
		, _IgnoreTextureAlpha(true)
		, _ViewportSize(FVector2D(320.0f, 240.0f))
	{
	}

		SLATE_DEFAULT_SLOT( FArguments, Content )

		/** Whether or not to show the disabled effect when this viewport is disabled */
		SLATE_ATTRIBUTE( bool, ShowEffectWhenDisabled )
		
		/** 
		 * Whether or not to render directly to the window's backbuffer or an offscreen render target that is applied to the window later 
		 * Rendering to an offscreen target is the most common option in the editor where there may be many frames which this viewport's interface may wish to not re-render but use a cached buffer instead
		 * Rendering directly to the backbuffer is the most common option in the game where you want to update each frame without the cost of writing to an intermediate target first.
		 */
		SLATE_ARGUMENT( bool, RenderDirectlyToWindow )

		/** Whether or not to enable gamma correction. Doesn't apply when rendering directly to a backbuffer. */
		SLATE_ARGUMENT( bool, EnableGammaCorrection )

		/** Whether or not to reverse the gamma correction done to the texture in this viewport.  Ignores the bEnableGammaCorrection setting */
		SLATE_ARGUMENT( bool, ReverseGammaCorrection )

		/** Allow this viewport to blend with its background. */
		SLATE_ARGUMENT( bool, EnableBlending )

		/** Whether or not to enable stereo rendering. */
		SLATE_ARGUMENT(bool, EnableStereoRendering )

		/** True if the viewport texture has pre-multiplied alpha */
		SLATE_ARGUMENT( bool, PreMultipliedAlpha )

		/**
		 * If true, the viewport's texture alpha is ignored when performing blending.  In this case only the viewport tint opacity is used
		 * If false, the texture alpha is used during blending
		 */
		SLATE_ARGUMENT( bool, IgnoreTextureAlpha )

		/** The interface to be used by this viewport for rendering and I/O. */
		SLATE_ARGUMENT(TSharedPtr<ISlateViewport>, ViewportInterface)
		
		/** Size of the viewport widget. */
		SLATE_ATTRIBUTE(FVector2D, ViewportSize);

	SLATE_END_ARGS()

	/** Default constructor. */
	SViewport();

	/**
	 * Construct the widget.
	 *
	 * @param InArgs  Declaration from which to construct the widget.
	 */
	void Construct(const FArguments& InArgs);

public:

	/** SViewport wants keyboard focus */
	virtual bool SupportsKeyboardFocus() const override { return true; }

	/** 
	 * Computes the ideal size necessary to display this widget.
	 *
	 * @return The desired width and height.
	 */
	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return ViewportSize.Get();
	}

	/**
	 * Sets the interface to be used by this viewport for rendering and I/O
	 *
	 * @param InViewportInterface The interface to use
	 */
	void SetViewportInterface( TSharedRef<ISlateViewport> InViewportInterface )
	{
		ViewportInterface = InViewportInterface;
	}

	/**
	 * Sets the interface to be used by this viewport for rendering and I/O
	 *
	 * @param InViewportInterface The interface to use
	 */
	TWeakPtr<ISlateViewport> GetViewportInterface()
	{
		return ViewportInterface;
	}

	/**
	 * Sets the content for this widget
	 *
	 * @param InContent	The new content (can be null)
	 */
	void SetContent( TSharedPtr<SWidget> InContent );

	void SetCustomHitTestPath( TSharedPtr<ICustomHitTestPath> CustomHitTestPath );

	TSharedPtr<ICustomHitTestPath> GetCustomHitTestPath();

	const TSharedPtr<SWidget> GetContent() const { return ChildSlot.GetWidget(); }

	/**
	 * A delegate called when the viewports top level window is being closed
	 *
	 * @param InWindowBeingClosed	The window that is about to be closed
	 */
	void OnWindowClosed( const TSharedRef<SWindow>& InWindowBeingClosed );

	/**
	 * A delegate called when the viewports top level window is deactivated
	 */
	FReply OnViewportActivated(const FWindowActivateEvent& InActivateEvent);

	/**
	 * A delegate called when the viewports top level window is deactivated
	 */
	void OnViewportDeactivated(const FWindowActivateEvent& InActivateEvent);

	/** @return Whether or not this viewport renders directly to the backbuffer */
	bool ShouldRenderDirectly() const { return bRenderDirectlyToWindow; }

	/** @return Whether or not this viewport supports stereo rendering */
	bool IsStereoRenderingAllowed() const { return bEnableStereoRendering; }

	/**
	 * Sets whether this viewport can render directly to the back buffer.  Advanced use only
	 * 
	 * @param	bInRenderDirectlyToWindow	Whether we should be able to render to the back buffer
	 */
	void SetRenderDirectlyToWindow( const bool bInRenderDirectlyToWindow )
	{
		bRenderDirectlyToWindow = bInRenderDirectlyToWindow;
	}

	/**
	 * Sets whether stereo rendering is allowed for this viewport.  Advanced use only
	 * 
	 * @param	bInEnableStereoRendering	Whether stereo rendering should be allowed for this viewport
	 */
	void EnableStereoRendering( const bool bInEnableStereoRendering )
	{
		bEnableStereoRendering = bInEnableStereoRendering;
	}

	/** 
	 * Sets whether this viewport is active. 
	 * While active, a persistent Active Timer is registered and a Slate tick/paint pass is guaranteed every frame.
	 * @param bActive Whether to set the viewport as active
	 */
	void SetActive(bool bActive);

	DEPRECATED(4.11, "SetWidgetToFocusOnActivate is no longer needed, remove this call.")
	void SetWidgetToFocusOnActivate(TSharedPtr<SWidget> WidgetToFocus) { }

public:

	// SWidget interface

	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual FReply OnTouchStarted( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent ) override;
	virtual FReply OnTouchMoved( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent ) override;
	virtual FReply OnTouchEnded( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent ) override;
	virtual FReply OnTouchGesture( const FGeometry& MyGeometry, const FPointerEvent& GestureEvent ) override;
	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override;
	virtual TOptional<TSharedRef<SWidget>> OnMapCursor(const FCursorReply& CursorReply) const override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnMouseLeave( const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& KeyEvent ) override;
	virtual FReply OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& KeyEvent ) override;
	virtual FReply OnAnalogValueChanged( const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent ) override;
	virtual FReply OnKeyChar( const FGeometry& MyGeometry, const FCharacterEvent& CharacterEvent ) override;
	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent ) override;
	virtual void OnFocusLost( const FFocusEvent& InFocusEvent ) override;
	virtual FReply OnMotionDetected( const FGeometry& MyGeometry, const FMotionEvent& InMotionEvent ) override;
	virtual TOptional<bool> OnQueryShowFocus( const EFocusCause InFocusCause ) const override;
	virtual FPopupMethodReply OnQueryPopupMethod() const override;
	virtual void OnFinishedPointerInput() override;
	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual TSharedPtr<struct FVirtualPointerPosition> TranslateMouseCoordinateFor3DChild(const TSharedRef<SWidget>& ChildWidget, const FGeometry& MyGeometry, const FVector2D& ScreenSpaceMouseCoordinate, const FVector2D& LastScreenSpaceMouseCoordinate) const override;
	virtual FNavigationReply OnNavigation( const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent ) override;

private:

	FSimpleSlot& Decl_GetContent()
	{
		return ChildSlot;
	}

	// Viewports shouldn't show focus
	virtual const FSlateBrush* GetFocusBrush() const override
	{
		return nullptr;
	}

protected:
	/** Empty active timer meant to ensure a tick/paint pass while this viewport is active */
	EActiveTimerReturnType EnsureTick(double InCurrentTime, float InDeltaTime);

	/** Interface to the rendering and I/O implementation of the viewport. */
	TWeakPtr<ISlateViewport> ViewportInterface;
	
private:

	/** The handle to the active EnsureTick() timer */
	TWeakPtr<FActiveTimerHandle> ActiveTimerHandle;

	/** Whether or not to show the disabled effect when this viewport is disabled. */
	TAttribute<bool> ShowDisabledEffect;

	/** Size of the viewport. */
	TAttribute<FVector2D> ViewportSize;

	TSharedPtr<ICustomHitTestPath> CustomHitTestPath;

	/** Whether or not this viewport renders directly to the window back-buffer. */
	bool bRenderDirectlyToWindow;

	/** Whether or not to apply gamma correction on the render target supplied by the ISlateViewport. */
	bool bEnableGammaCorrection;

	/** Whether or not to reverse the gamma correction done by the texture in this viewport.  Ignores the bEnableGammaCorrection setting */
	bool bReverseGammaCorrection;

	/** Whether or not to blend this viewport with the background. */
	bool bEnableBlending;

	/** Whether or not to enable stereo rendering. */
	bool bEnableStereoRendering;

	/** Whether or not to allow texture alpha to be used in blending calculations. */
	bool bIgnoreTextureAlpha;

	/** True if the viewport texture has pre-multiplied alpha */
	bool bPreMultipliedAlpha;
};
