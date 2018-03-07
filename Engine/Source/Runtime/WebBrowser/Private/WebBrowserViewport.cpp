// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WebBrowserViewport.h"
#include "Textures/SlateShaderResource.h"
#include "Widgets/SWidget.h"
#include "IWebBrowserWindow.h"
#include "Layout/WidgetPath.h"

#if WITH_CEF3
#include "CEF/CEFWebBrowserWindow.h"
#endif

FIntPoint FWebBrowserViewport::GetSize() const
{
	return (WebBrowserWindow->GetTexture(bIsPopup) != nullptr)
		? FIntPoint(WebBrowserWindow->GetTexture(bIsPopup)->GetWidth(), WebBrowserWindow->GetTexture(bIsPopup)->GetHeight())
		: FIntPoint();
}

FSlateShaderResource* FWebBrowserViewport::GetViewportRenderTargetTexture() const
{
	return WebBrowserWindow->GetTexture(bIsPopup);
}

void FWebBrowserViewport::Tick( const FGeometry& AllottedGeometry, double InCurrentTime, float DeltaTime )
{
	if (!bIsPopup)
	{
		FVector2D AbsoluteSize = AllottedGeometry.GetLocalSize() * AllottedGeometry.Scale;
		WebBrowserWindow->SetViewportSize(AbsoluteSize.IntPoint());

#if WITH_CEF3
		// Forward the AllottedGeometry to the WebBrowserWindow so the IME implementation can use it
		TSharedPtr<FCEFWebBrowserWindow> CefWebBrowserWindow = StaticCastSharedPtr<FCEFWebBrowserWindow>(WebBrowserWindow);
		CefWebBrowserWindow->UpdateCachedGeometry(AllottedGeometry);
#endif
	}
}

bool FWebBrowserViewport::RequiresVsync() const
{
	return false;
}

FCursorReply FWebBrowserViewport::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent )
{
	return WebBrowserWindow->OnCursorQuery(MyGeometry, CursorEvent);
}

FReply FWebBrowserViewport::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Capture mouse on left button down so that you can drag out of the viewport
	FReply Reply = WebBrowserWindow->OnMouseButtonDown(MyGeometry, MouseEvent, bIsPopup);
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const FWidgetPath* Path = MouseEvent.GetEventPath();
		if (Path->IsValid())
		{
			TSharedRef<SWidget> TopWidget = Path->Widgets.Last().Widget;
			return Reply.CaptureMouse(TopWidget);
		}
	}
	return Reply;
}

FReply FWebBrowserViewport::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Release mouse capture when left button released
	FReply Reply = WebBrowserWindow->OnMouseButtonUp(MyGeometry, MouseEvent, bIsPopup);
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return Reply.ReleaseMouseCapture();
	}
	return Reply;
}

void FWebBrowserViewport::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
}

void FWebBrowserViewport::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	WebBrowserWindow->OnMouseLeave(MouseEvent);
}

FReply FWebBrowserViewport::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return WebBrowserWindow->OnMouseMove(MyGeometry, MouseEvent, bIsPopup);
}

FReply FWebBrowserViewport::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return WebBrowserWindow->OnMouseWheel(MyGeometry, MouseEvent, bIsPopup);
}

FReply FWebBrowserViewport::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	FReply Reply = WebBrowserWindow->OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent, bIsPopup);
	return Reply;
}

FReply FWebBrowserViewport::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return WebBrowserWindow->OnKeyDown(InKeyEvent) ? FReply::Handled() : FReply::Unhandled();
}

FReply FWebBrowserViewport::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return WebBrowserWindow->OnKeyUp(InKeyEvent) ? FReply::Handled() : FReply::Unhandled();
}

FReply FWebBrowserViewport::OnKeyChar( const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent )
{
	return WebBrowserWindow->OnKeyChar(InCharacterEvent) ? FReply::Handled() : FReply::Unhandled();
}

FReply FWebBrowserViewport::OnFocusReceived(const FFocusEvent& InFocusEvent)
{
	WebBrowserWindow->OnFocus(true, bIsPopup);
	return FReply::Handled();
}

void FWebBrowserViewport::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	WebBrowserWindow->OnFocus(false, bIsPopup);
}
