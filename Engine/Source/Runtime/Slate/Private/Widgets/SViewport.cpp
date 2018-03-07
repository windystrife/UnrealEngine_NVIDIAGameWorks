// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SViewport.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateColorBrush.h"
#include "Application/SlateApplicationBase.h"
#include "Framework/Application/SlateApplication.h"

DECLARE_CYCLE_STAT(TEXT("Game UI Tick"), STAT_ViewportTickTime, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("Game UI Paint"), STAT_ViewportPaintTime, STATGROUP_Slate);


/* SViewport constructors
 *****************************************************************************/

SViewport::SViewport()
	: bRenderDirectlyToWindow(false)
	, bEnableGammaCorrection(true)
	, bReverseGammaCorrection(false)
	, bEnableStereoRendering(false)
{ }


/* SViewport interface
 *****************************************************************************/

void SViewport::Construct( const FArguments& InArgs )
{
	ShowDisabledEffect = InArgs._ShowEffectWhenDisabled;
	bRenderDirectlyToWindow = InArgs._RenderDirectlyToWindow;
	bEnableGammaCorrection = InArgs._EnableGammaCorrection;
	bReverseGammaCorrection = InArgs._ReverseGammaCorrection;
	bEnableBlending = InArgs._EnableBlending;
	bEnableStereoRendering = InArgs._EnableStereoRendering;
	bIgnoreTextureAlpha = InArgs._IgnoreTextureAlpha;
	bPreMultipliedAlpha = InArgs._PreMultipliedAlpha;
	ViewportInterface = InArgs._ViewportInterface;
	ViewportSize = InArgs._ViewportSize;

	this->ChildSlot
	[
		InArgs._Content.Widget
	];
}

void SViewport::SetActive(bool bActive)
{
	if (bActive && !ActiveTimerHandle.IsValid())
	{
		ActiveTimerHandle = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SViewport::EnsureTick));
	}
	else if (!bActive && ActiveTimerHandle.IsValid())
	{
		UnRegisterActiveTimer(ActiveTimerHandle.Pin().ToSharedRef());
	}
}

EActiveTimerReturnType SViewport::EnsureTick(double InCurrentTime, float InDeltaTime)
{
	return EActiveTimerReturnType::Continue;
}

int32 SViewport::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	SCOPED_NAMED_EVENT(SViewport_OnPaint, FColor::Purple);
	SCOPE_CYCLE_COUNTER(STAT_ViewportPaintTime);

	bool bEnabled = ShouldBeEnabled( bParentEnabled );
	bool bShowDisabledEffect = ShowDisabledEffect.Get();
	ESlateDrawEffect DrawEffects = bShowDisabledEffect && !bEnabled ? ESlateDrawEffect::DisabledEffect : ESlateDrawEffect::None;

	int32 LastHitTestIndex = Args.GetLastHitTestIndex();


	// Viewport texture alpha channels are often in an indeterminate state, even after the resolve,
	// so we'll tell the shader to not use the alpha channel when blending
	if( bIgnoreTextureAlpha )
	{
		DrawEffects |= ESlateDrawEffect::IgnoreTextureAlpha;
	}

	// Should we perform gamma correction?
	if( !bEnableGammaCorrection )
	{
		DrawEffects |= ESlateDrawEffect::NoGamma;
	}

	// Should we reverse gamma correction
	if (bReverseGammaCorrection)
	{
		DrawEffects |= ESlateDrawEffect::ReverseGamma;
	}

	// Show we enable blending?
	if( !bEnableBlending )
	{
		DrawEffects |= ESlateDrawEffect::NoBlending;
	}
	// Should we use pre-multiplied alpha?
	else if( bPreMultipliedAlpha )
	{
		DrawEffects |= ESlateDrawEffect::PreMultipliedAlpha;
	}

	TSharedPtr<ISlateViewport> ViewportInterfacePin = ViewportInterface.Pin();

	// Tell the interface that we are drawing.
	if (ViewportInterfacePin.IsValid())
	{
		ViewportInterfacePin->OnDrawViewport( AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled );
	}

	// Only draw a quad if not rendering directly to the backbuffer
	if( !ShouldRenderDirectly() )
	{
		if( ViewportInterfacePin.IsValid() && ViewportInterfacePin->GetViewportRenderTargetTexture() != nullptr )
		{
			FSlateDrawElement::MakeViewport( OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), ViewportInterfacePin, DrawEffects, InWidgetStyle.GetColorAndOpacityTint() );
		}
		else
		{
			// Viewport isn't ready yet, so just draw a black box
			static FSlateColorBrush BlackBrush( FColor::Black );
			FSlateDrawElement::MakeBox( OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), &BlackBrush, DrawEffects, BlackBrush.GetTint( InWidgetStyle ) );
		}
	}

	int32 Layer = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bEnabled );

	if( ViewportInterfacePin.IsValid() && ViewportInterfacePin->IsSoftwareCursorVisible() )
	{
		const FVector2D CursorPosScreenSpace = FSlateApplication::Get().GetCursorPos();		
		// @todo Slate: why are we calling OnCursorQuery in here?
		FCursorReply Reply = ViewportInterfacePin->OnCursorQuery( AllottedGeometry,
			FPointerEvent(
				FSlateApplicationBase::CursorPointerIndex,
				CursorPosScreenSpace,
				CursorPosScreenSpace,
				FVector2D::ZeroVector,
				TSet<FKey>(),
				FModifierKeysState() )
		 );
		EMouseCursor::Type CursorType = Reply.GetCursorType();

		const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("SoftwareCursor_Grab"));
		if( CursorType == EMouseCursor::CardinalCross )
		{
			Brush = FCoreStyle::Get().GetBrush(TEXT("SoftwareCursor_CardinalCross"));
		}

		FVector2D CursorPositionLocalSpace = ViewportInterfacePin->GetSoftwareCursorPosition() / AllottedGeometry.Scale;

		LayerId++;
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry( CursorPositionLocalSpace - ( Brush->ImageSize / 2 ), Brush->ImageSize ),
			Brush
		);
	}

	// If there are any custom hit testable widgets in the 3D world we need to register their custom hit test path here.
	if ( CustomHitTestPath.IsValid() )
	{
		Args.InsertCustomHitTestPath(CustomHitTestPath.ToSharedRef(), LastHitTestIndex);
	}

	return Layer;
}

void SViewport::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	SCOPE_CYCLE_COUNTER(STAT_ViewportTickTime);

	if ( ViewportInterface.IsValid() )
	{
		ViewportInterface.Pin()->Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	}
}


/* SWidget interface
 *****************************************************************************/

FCursorReply SViewport::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	return ViewportInterface.IsValid() ? ViewportInterface.Pin()->OnCursorQuery(MyGeometry, CursorEvent) : FCursorReply::Unhandled();
}

TOptional<TSharedRef<SWidget>> SViewport::OnMapCursor(const FCursorReply& CursorReply) const
{
	return ViewportInterface.IsValid() ? ViewportInterface.Pin()->OnMapCursor(CursorReply) : TOptional<TSharedRef<SWidget>>();
}

FReply SViewport::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return ViewportInterface.IsValid() ? ViewportInterface.Pin()->OnMouseButtonDown(MyGeometry, MouseEvent) : FReply::Unhandled();
}

FReply SViewport::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return ViewportInterface.IsValid() ? ViewportInterface.Pin()->OnMouseButtonUp(MyGeometry, MouseEvent) : FReply::Unhandled();
}

void SViewport::OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);

	if (ViewportInterface.IsValid())
	{
		ViewportInterface.Pin()->OnMouseEnter(MyGeometry, MouseEvent);
	}
}

void SViewport::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	SCompoundWidget::OnMouseLeave(MouseEvent);

	if (ViewportInterface.IsValid())
	{
		ViewportInterface.Pin()->OnMouseLeave(MouseEvent);
	}
}

FReply SViewport::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return ViewportInterface.IsValid() ? ViewportInterface.Pin()->OnMouseMove(MyGeometry, MouseEvent) : FReply::Unhandled();
}

FReply SViewport::OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return ViewportInterface.IsValid() ? ViewportInterface.Pin()->OnMouseWheel(MyGeometry, MouseEvent) : FReply::Unhandled();
}

FReply SViewport::OnMouseButtonDoubleClick( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return ViewportInterface.IsValid() ? ViewportInterface.Pin()->OnMouseButtonDoubleClick(MyGeometry, MouseEvent) : FReply::Unhandled();
}

FReply SViewport::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& KeyEvent )
{
	return ViewportInterface.IsValid() ? ViewportInterface.Pin()->OnKeyDown(MyGeometry, KeyEvent) : FReply::Unhandled();
}

FReply SViewport::OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& KeyEvent )
{
	return ViewportInterface.IsValid() ? ViewportInterface.Pin()->OnKeyUp(MyGeometry, KeyEvent) : FReply::Unhandled();
}

FReply SViewport::OnAnalogValueChanged( const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent )
{
	return ViewportInterface.IsValid() ? ViewportInterface.Pin()->OnAnalogValueChanged(MyGeometry, InAnalogInputEvent) : FReply::Unhandled();
}

FReply SViewport::OnKeyChar( const FGeometry& MyGeometry, const FCharacterEvent& CharacterEvent )
{
	return ViewportInterface.IsValid() ? ViewportInterface.Pin()->OnKeyChar(MyGeometry, CharacterEvent) : FReply::Unhandled();
}

FReply SViewport::OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent )
{
	return ViewportInterface.IsValid() ? ViewportInterface.Pin()->OnFocusReceived(InFocusEvent) : FReply::Unhandled();
}

void SViewport::OnFocusLost( const FFocusEvent& InFocusEvent )
{
	if (ViewportInterface.IsValid())
	{
		ViewportInterface.Pin()->OnFocusLost(InFocusEvent);
	}
}

void SViewport::SetContent( TSharedPtr<SWidget> InContent )
{
	ChildSlot
	[
		InContent.IsValid() ? InContent.ToSharedRef() : (TSharedRef<SWidget>)SNullWidget::NullWidget
	];
}

void SViewport::SetCustomHitTestPath( TSharedPtr<ICustomHitTestPath> InCustomHitTestPath )
{
	CustomHitTestPath = InCustomHitTestPath;
}

TSharedPtr<ICustomHitTestPath> SViewport::GetCustomHitTestPath()
{
	return CustomHitTestPath;
}

void SViewport::OnWindowClosed( const TSharedRef<SWindow>& WindowBeingClosed )
{
	if (ViewportInterface.IsValid())
	{
		ViewportInterface.Pin()->OnViewportClosed();
	}
}

FReply SViewport::OnViewportActivated(const FWindowActivateEvent& InActivateEvent)
{
	return ViewportInterface.IsValid() ? ViewportInterface.Pin()->OnViewportActivated(InActivateEvent) : FReply::Unhandled();
}

void SViewport::OnViewportDeactivated(const FWindowActivateEvent& InActivateEvent)
{
	if (ViewportInterface.IsValid())
	{
		ViewportInterface.Pin()->OnViewportDeactivated(InActivateEvent);
	}
}

FReply SViewport::OnTouchStarted( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent )
{
	return ViewportInterface.IsValid() ? ViewportInterface.Pin()->OnTouchStarted(MyGeometry, InTouchEvent) : FReply::Unhandled();
}

FReply SViewport::OnTouchMoved( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent )
{
	return ViewportInterface.IsValid() ? ViewportInterface.Pin()->OnTouchMoved(MyGeometry, InTouchEvent) : FReply::Unhandled();
}

FReply SViewport::OnTouchEnded( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent )
{
	return ViewportInterface.IsValid() ? ViewportInterface.Pin()->OnTouchEnded(MyGeometry, InTouchEvent) : FReply::Unhandled();
}

FReply SViewport::OnTouchGesture( const FGeometry& MyGeometry, const FPointerEvent& GestureEvent )
{
	return ViewportInterface.IsValid() ? ViewportInterface.Pin()->OnTouchGesture(MyGeometry, GestureEvent) : FReply::Unhandled();
}

FReply SViewport::OnMotionDetected( const FGeometry& MyGeometry, const FMotionEvent& InMotionEvent )
{
	return ViewportInterface.IsValid() ? ViewportInterface.Pin()->OnMotionDetected(MyGeometry, InMotionEvent) : FReply::Unhandled();
}

TOptional<bool> SViewport::OnQueryShowFocus(const EFocusCause InFocusCause) const
{
	return ViewportInterface.IsValid() ? ViewportInterface.Pin()->OnQueryShowFocus(InFocusCause) : TOptional<bool>();
}

FPopupMethodReply SViewport::OnQueryPopupMethod() const
{
	TSharedPtr<ISlateViewport> PinnedInterface = ViewportInterface.Pin();
	if (PinnedInterface.IsValid())
	{
		return PinnedInterface->OnQueryPopupMethod();
	}
	else
	{
		return FPopupMethodReply::UseMethod(EPopupMethod::CreateNewWindow);
	}	
}

void SViewport::OnFinishedPointerInput()
{
	TSharedPtr<ISlateViewport> PinnedInterface = ViewportInterface.Pin();
	if (PinnedInterface.IsValid())
	{
		PinnedInterface->OnFinishedPointerInput();
	}
}

void SViewport::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	SCompoundWidget::OnArrangeChildren(AllottedGeometry, ArrangedChildren);
	if( ArrangedChildren.Allows3DWidgets() && CustomHitTestPath.IsValid() )
	{
		CustomHitTestPath->ArrangeChildren( ArrangedChildren );
	}
}

TSharedPtr<FVirtualPointerPosition> SViewport::TranslateMouseCoordinateFor3DChild(const TSharedRef<SWidget>& ChildWidget, const FGeometry& MyGeometry, const FVector2D& ScreenSpaceMouseCoordinate, const FVector2D& LastScreenSpaceMouseCoordinate) const
{
	if( CustomHitTestPath.IsValid() )
	{
		return CustomHitTestPath->TranslateMouseCoordinateFor3DChild( ChildWidget, MyGeometry, ScreenSpaceMouseCoordinate, LastScreenSpaceMouseCoordinate );
	}

	return nullptr;
}

FNavigationReply SViewport::OnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent)
{
	return ViewportInterface.IsValid() ? ViewportInterface.Pin()->OnNavigation(MyGeometry, InNavigationEvent) : FNavigationReply::Stop();
}
