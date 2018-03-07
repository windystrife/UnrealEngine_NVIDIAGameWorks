// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_IOS

#include "IWebBrowserWindow.h"
#include "Widgets/SWindow.h"
#import <UIKit/UIKit.h>
#import <UIKit/UIWebView.h>

class SIOSWebBrowserWidget;
class SWebBrowserView;

/**
* Wrapper to contain the UIWebView and implement its delegate functions
*/
#if !PLATFORM_TVOS
@interface IOSWebViewWrapper : NSObject <UIWebViewDelegate>
#else
@interface IOSWebViewWrapper : NSObject
#endif
{
	TSharedPtr<SIOSWebBrowserWidget> WebBrowserWidget;
	bool bNeedsAddToView;
}
#if !PLATFORM_TVOS
@property(strong) UIWebView* WebView;
#endif
@property(copy) NSURL* NextURL;
@property(copy) NSString* NextContent;
@property CGRect DesiredFrame;

-(void)create:(TSharedPtr<SIOSWebBrowserWidget>)InWebBrowserWidget useTransparency:(bool)InUseTransparency;
-(void)close;
-(void)updateframe:(CGRect)InFrame;
-(void)loadstring:(NSString*)InString dummyurl:(NSURL*)InURL;
-(void)loadurl:(NSURL*)InURL;
-(void)executejavascript:(NSString*)InJavaScript;
@end

/**
* Implementation of interface for dealing with a Web Browser window.
*/
class FWebBrowserWindow
	: public IWebBrowserWindow
	, public TSharedFromThis<FWebBrowserWindow>
{
	// The WebBrowserSingleton should be the only one creating instances of this class
	friend class FWebBrowserSingleton;
	// CreateWidget should only be called by the WebBrowserView
	friend class SWebBrowserView;

private:
	/**
	* Creates and initializes a new instance.
	*
	* @param InUrl The Initial URL that will be loaded.
	* @param InContentsToLoad Optional string to load as a web page.
	* @param InShowErrorMessage Whether to show an error message in case of loading errors.
	*/
	FWebBrowserWindow(FString InUrl, TOptional<FString> InContentsToLoad, bool ShowErrorMessage, bool bThumbMouseButtonNavigation, bool bUseTransparency);

	/**
	 * Create the SWidget for this WebBrowserWindow
	 */
	TSharedRef<SWidget> CreateWidget();

public:
	/** Virtual Destructor. */
	virtual ~FWebBrowserWindow();

public:

	// IWebBrowserWindow Interface

	virtual void LoadURL(FString NewURL) override;
	virtual void LoadString(FString Contents, FString DummyURL) override;
	virtual void SetViewportSize(FIntPoint WindowSize, FIntPoint WindowPos) override;
	virtual FSlateShaderResource* GetTexture(bool bIsPopup = false) override;
	virtual bool IsValid() const override;
	virtual bool IsInitialized() const override;
	virtual bool IsClosing() const override;
	virtual EWebBrowserDocumentState GetDocumentLoadingState() const override;
	virtual FString GetTitle() const override;
	virtual FString GetUrl() const override;
	virtual bool OnKeyDown(const FKeyEvent& InKeyEvent) override;
	virtual bool OnKeyUp(const FKeyEvent& InKeyEvent) override;
	virtual bool OnKeyChar(const FCharacterEvent& InCharacterEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup) override;
	virtual void OnFocus(bool SetFocus, bool bIsPopup) override;
	virtual void OnCaptureLost() override;
	virtual bool CanGoBack() const override;
	virtual void GoBack() override;
	virtual bool CanGoForward() const override;
	virtual void GoForward() override;
	virtual bool IsLoading() const override;
	virtual void Reload() override;
	virtual void StopLoad() override;
	virtual void ExecuteJavascript(const FString& Script) override;
	virtual void CloseBrowser(bool bForce) override;
	virtual void BindUObject(const FString& Name, UObject* Object, bool bIsPermanent = true) override;
	virtual void UnbindUObject(const FString& Name, UObject* Object = nullptr, bool bIsPermanent = true) override;
	virtual void GetSource(TFunction<void (const FString&)> Callback) const;
	virtual int GetLoadError() override;
	virtual void SetIsDisabled(bool bValue) override;
	virtual TSharedPtr<SWindow> GetParentWindow() const override
	{
		return ParentWindow;
	}

	virtual void SetParentWindow(TSharedPtr<SWindow> Window) override
	{
		ParentWindow = Window;
	}

	// TODO: None of these events are actually called

	DECLARE_DERIVED_EVENT(FWebBrowserWindow, IWebBrowserWindow::FOnDocumentStateChanged, FOnDocumentStateChanged);
	virtual FOnDocumentStateChanged& OnDocumentStateChanged() override
	{
		return DocumentStateChangedEvent;
	}

	DECLARE_DERIVED_EVENT(FWebBrowserWindow, IWebBrowserWindow::FOnTitleChanged, FOnTitleChanged);
	virtual FOnTitleChanged& OnTitleChanged() override
	{
		return TitleChangedEvent;
	}

	DECLARE_DERIVED_EVENT(FWebBrowserWindow, IWebBrowserWindow::FOnUrlChanged, FOnUrlChanged);
	virtual FOnUrlChanged& OnUrlChanged() override
	{
		return UrlChangedEvent;
	}

	DECLARE_DERIVED_EVENT(FWebBrowserWindow, IWebBrowserWindow::FOnToolTip, FOnToolTip);
	virtual FOnToolTip& OnToolTip() override
	{
		return ToolTipEvent;
	}

	DECLARE_DERIVED_EVENT(FWebBrowserWindow, IWebBrowserWindow::FOnNeedsRedraw, FOnNeedsRedraw);
	virtual FOnNeedsRedraw& OnNeedsRedraw() override
	{
		return NeedsRedrawEvent;
	}

	virtual FOnBeforeBrowse& OnBeforeBrowse() override
	{
		return BeforeBrowseDelegate;
	}

	virtual FOnLoadUrl& OnLoadUrl() override
	{
		return LoadUrlDelegate;
	}

	virtual FOnCreateWindow& OnCreateWindow() override
	{
		return CreateWindowDelegate;
	}

	virtual FOnCloseWindow& OnCloseWindow() override
	{
		return CloseWindowDelegate;
	}

	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) override
	{
		//return Cursor == EMouseCursor::Default ? FCursorReply::Unhandled() : FCursorReply::Cursor(Cursor);
		return FCursorReply::Unhandled();
	}

	virtual FOnBeforePopupDelegate& OnBeforePopup() override
	{
		return BeforePopupDelegate;
	}

	DECLARE_DERIVED_EVENT(FWebBrowserWindow, IWebBrowserWindow::FOnShowPopup, FOnShowPopup);
	virtual FOnShowPopup& OnShowPopup() override
	{
		return ShowPopupEvent;
	}

	DECLARE_DERIVED_EVENT(FWebBrowserWindow, IWebBrowserWindow::FOnDismissPopup, FOnDismissPopup);
	virtual FOnDismissPopup& OnDismissPopup() override
	{
		return DismissPopupEvent;
	}

	virtual FOnShowDialog& OnShowDialog() override
	{
		return ShowDialogDelegate;
	}

	virtual FOnDismissAllDialogs& OnDismissAllDialogs() override
	{
		return DismissAllDialogsDelegate;
	}

	virtual FOnSuppressContextMenu& OnSuppressContextMenu() override
	{
		return SuppressContextMenuDelgate;
	}


public:

private:

	TSharedPtr<SIOSWebBrowserWidget> BrowserWidget;

	/** Current Url of this window. */
	FString CurrentUrl;

	/** Optional text to load as a web page. */
	TOptional<FString> ContentsToLoad;
	
	/** Whether to enable background transparency */
	bool bUseTransparency;

	// TODO: None of these events are actually called

	/** Delegate for broadcasting load state changes. */
	FOnDocumentStateChanged DocumentStateChangedEvent;

	/** Delegate for broadcasting title changes. */
	FOnTitleChanged TitleChangedEvent;

	/** Delegate for broadcasting address changes. */
	FOnUrlChanged UrlChangedEvent;

	/** Delegate for broadcasting when the browser wants to show a tool tip. */
	FOnToolTip ToolTipEvent;

	/** Delegate for notifying that the window needs refreshing. */
	FOnNeedsRedraw NeedsRedrawEvent;

	/** Delegate that is executed prior to browser navigation. */
	FOnBeforeBrowse BeforeBrowseDelegate;

	/** Delegate for overriding Url contents. */
	FOnLoadUrl LoadUrlDelegate;

	/** Delegate for notifying that a popup window is attempting to open. */
	FOnBeforePopupDelegate BeforePopupDelegate;

	/** Delegate for handling requests to create new windows. */
	FOnCreateWindow CreateWindowDelegate;

	/** Delegate for handling requests to close new windows that were created. */
	FOnCloseWindow CloseWindowDelegate;

	/** Delegate for handling requests to show the popup menu. */
	FOnShowPopup ShowPopupEvent;

	/** Delegate for handling requests to dismiss the current popup menu. */
	FOnDismissPopup DismissPopupEvent;

	/** Delegate for showing dialogs. */
	FOnShowDialog ShowDialogDelegate;

	/** Delegate for dismissing all dialogs. */
	FOnDismissAllDialogs DismissAllDialogsDelegate;

	/** Delegate for suppressing context menu */
	FOnSuppressContextMenu SuppressContextMenuDelgate;

	TSharedPtr<SWindow> ParentWindow;

};

#endif
