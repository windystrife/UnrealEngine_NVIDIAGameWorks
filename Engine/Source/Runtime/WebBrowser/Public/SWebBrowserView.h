// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/PopupMethodReply.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Framework/SlateDelegates.h"
#include "Framework/Application/IMenu.h"
#include "Widgets/SViewport.h"
#include "IWebBrowserSingleton.h"

class FWebBrowserViewport;
class IWebBrowserAdapter;
class IWebBrowserDialog;
class IWebBrowserPopupFeatures;
class IWebBrowserWindow;
struct FWebNavigationRequest;
enum class EWebBrowserDialogEventResponse;
enum class EWebBrowserDocumentState;

DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnBeforePopupDelegate, FString, FString);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnCreateWindowDelegate, const TWeakPtr<IWebBrowserWindow>&, const TWeakPtr<IWebBrowserPopupFeatures>&);
DECLARE_DELEGATE_RetVal_OneParam(bool, FOnCloseWindowDelegate, const TWeakPtr<IWebBrowserWindow>&);

#if WITH_CEF3
typedef SViewport SWebBrowserWidget;
#else
typedef SWidget SWebBrowserWidget;
#endif

class WEBBROWSER_API SWebBrowserView
	: public SCompoundWidget
{
public:
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnBeforeBrowse, const FString& /*Url*/, const FWebNavigationRequest& /*Request*/)
	DECLARE_DELEGATE_RetVal_ThreeParams(bool, FOnLoadUrl, const FString& /*Method*/, const FString& /*Url*/, FString& /* Response */)
	DECLARE_DELEGATE_RetVal_OneParam(EWebBrowserDialogEventResponse, FOnShowDialog, const TWeakPtr<IWebBrowserDialog>&);
	DECLARE_DELEGATE_RetVal(bool, FOnSuppressContextMenu);

	SLATE_BEGIN_ARGS(SWebBrowserView)
		: _InitialURL(TEXT("https://www.google.com"))
		, _ShowErrorMessage(true)
		, _SupportsTransparency(false)
		, _SupportsThumbMouseButtonNavigation(false)
		, _BackgroundColor(255,255,255,255)
		, _PopupMenuMethod(TOptional<EPopupMethod>())
		, _ContextSettings()
		, _ViewportSize(FVector2D::ZeroVector)
	{ }

		/** A reference to the parent window. */
		SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)

		/** URL that the browser will initially navigate to. */
		SLATE_ARGUMENT(FString, InitialURL)

		/** Optional string to load contents as a web page. */
		SLATE_ARGUMENT(TOptional<FString>, ContentsToLoad)

		/** Whether to show an error message in case of loading errors. */
		SLATE_ARGUMENT(bool, ShowErrorMessage)

		/** Should this browser window support transparency. */
		SLATE_ARGUMENT(bool, SupportsTransparency)

		/** Whether to allow forward and back navigation via the mouse thumb buttons. */
		SLATE_ARGUMENT(bool, SupportsThumbMouseButtonNavigation)

		/** Opaque background color used before a document is loaded and when no document color is specified. */
		SLATE_ARGUMENT(FColor, BackgroundColor)

		/** Override the popup menu method used for popup menus. If not set, parent widgets will be queried instead. */
		SLATE_ARGUMENT(TOptional<EPopupMethod>, PopupMenuMethod)

		/** Override the default global context settings for this specific window. If not set, the global default will be used. */
		SLATE_ARGUMENT(TOptional<FBrowserContextSettings>, ContextSettings)

		/** Desired size of the web browser viewport. */
		SLATE_ATTRIBUTE(FVector2D, ViewportSize);

		/** Called when document loading completed. */
		SLATE_EVENT(FSimpleDelegate, OnLoadCompleted)

		/** Called when document loading failed. */
		SLATE_EVENT(FSimpleDelegate, OnLoadError)

		/** Called when document loading started. */
		SLATE_EVENT(FSimpleDelegate, OnLoadStarted)

		/** Called when document title changed. */
		SLATE_EVENT(FOnTextChanged, OnTitleChanged)

		/** Called when the Url changes. */
		SLATE_EVENT(FOnTextChanged, OnUrlChanged)

		/** Called before a popup window happens */
		SLATE_EVENT(FOnBeforePopupDelegate, OnBeforePopup)

		/** Called when the browser requests the creation of a new window */
		SLATE_EVENT(FOnCreateWindowDelegate, OnCreateWindow)

		/** Called when a browser window close event is detected */
		SLATE_EVENT(FOnCloseWindowDelegate, OnCloseWindow)

		/** Called before browser navigation. */
		SLATE_EVENT(FOnBeforeBrowse, OnBeforeNavigation)

		/** Called to allow bypassing page content on load. */
		SLATE_EVENT(FOnLoadUrl, OnLoadUrl)

		/** Called when the browser needs to show a dialog to the user. */
		SLATE_EVENT(FOnShowDialog, OnShowDialog)

		/** Called to dismiss any dialogs shown via OnShowDialog. */
		SLATE_EVENT(FSimpleDelegate, OnDismissAllDialogs)

		SLATE_EVENT(FOnSuppressContextMenu, OnSuppressContextMenu);

	SLATE_END_ARGS()


	/** Default constructor. */
	SWebBrowserView();

	~SWebBrowserView();

	virtual bool SupportsKeyboardFocus() const override {return true;}

	/**
	 * Construct the widget.
	 *
	 * @param InArgs  Declaration from which to construct the widget.
	 */
	void Construct(const FArguments& InArgs, const TSharedPtr<IWebBrowserWindow>& InWebBrowserWindow = nullptr);

	/**
	 * Load the specified URL.
	 *
	 * @param NewURL New URL to load.
	 */
	void LoadURL(FString NewURL);

	/**
	* Load a string as data to create a web page.
	*
	* @param Contents String to load.
	* @param DummyURL Dummy URL for the page.
	*/
	void LoadString(FString Contents, FString DummyURL);

	/** Reload the current page. */
	void Reload();

	/** Stop loading the page. */
	void StopLoad();

	/** Get the current title of the web page. */
	FText GetTitleText() const;

	/**
	 * Gets the currently loaded URL.
	 *
	 * @return The URL, or empty string if no document is loaded.
	 */
	FString GetUrl() const;

	/**
	 * Gets the URL that appears in the address bar, this may not be the URL that is currently loaded in the frame.
	 *
	 * @return The address bar URL.
	 */
	FText GetAddressBarUrlText() const;

	/** Whether the document finished loading. */
	bool IsLoaded() const;

	/** Whether the document is currently being loaded. */
	bool IsLoading() const;

	/** Whether the browser widget is done initializing. */
	bool IsInitialized() const;

	/** Execute javascript on the current window */
	void ExecuteJavascript(const FString& ScriptText);

	/**
	 * Gets the source of the main frame as raw HTML.
	 *
	 * This method has to be called asynchronously by passing a callback function, which will be called at a later point when the
	 * result is ready.
	 * @param	Callback	A callable that takes a single string reference for handling the result.
	 */
	void GetSource(TFunction<void (const FString&)> Callback) const ;

	/**
	 * Expose a UObject instance to the browser runtime.
	 * Properties and Functions will be accessible from JavaScript side.
	 * As all communication with the rendering procesis asynchronous, return values (both for properties and function results) are wrapped into JS Future objects.
	 *
	 * @param Name The name of the object. The object will show up as window.ue4.{Name} on the javascript side. If there is an existing object of the same name, this object will replace it. If bIsPermanent is false and there is an existing permanent binding, the permanent binding will be restored when the temporary one is removed.
	 * @param Object The object instance.
	 * @param bIsPermanent If true, the object will be visible to all pages loaded through this browser widget, otherwise, it will be deleted when navigating away from the current page. Non-permanent bindings should be registered from inside an OnLoadStarted event handler in order to be available before JS code starts loading.
	 */
	void BindUObject(const FString& Name, UObject* Object, bool bIsPermanent = true);

	/**
	 * Remove an existing script binding registered by BindUObject.
	 *
	 * @param Name The name of the object to remove.
	 * @param Object The object will only be removed if it is the same object as the one passed in.
	 * @param bIsPermanent Must match the bIsPermanent argument passed to BindUObject.
	 */
	void UnbindUObject(const FString& Name, UObject* Object, bool bIsPermanent = true);

	void BindAdapter(const TSharedRef<IWebBrowserAdapter>& Adapter);

	void UnbindAdapter(const TSharedRef<IWebBrowserAdapter>& Adapter);

	void BindInputMethodSystem(ITextInputMethodSystem* TextInputMethodSystem);

	void UnbindInputMethodSystem();

	/** Returns true if the browser can navigate backwards. */
	bool CanGoBack() const;

	/** Navigate backwards. */
	void GoBack();

	/** Returns true if the browser can navigate forwards. */
	bool CanGoForward() const;

	/** Navigate forwards. */
	void GoForward();

private:

	void SetupParentWindowHandlers();

	/** Callback for document loading state changes. */
	void HandleBrowserWindowDocumentStateChanged(EWebBrowserDocumentState NewState);

	/** Callback to tell slate we want to update the contents of the web view based on changes inside the view. */
	void HandleBrowserWindowNeedsRedraw();

	/** Callback for document title changes. */
	void HandleTitleChanged(FString NewTitle);

	/** Callback for loaded url changes. */
	void HandleUrlChanged(FString NewUrl);

	/** Callback for showing browser tool tips. */
	void HandleToolTip(FString ToolTipText);

	/**
	 * A delegate that is executed prior to browser navigation.
	 *
	 * @return true if the navigation was handled an no further action should be taken by the browser, false if the browser should handle.
	 */
	bool HandleBeforeNavigation(const FString& Url, const FWebNavigationRequest& Request);

	bool HandleLoadUrl(const FString& Method, const FString& Url, FString& OutResponse);

	/**
	 * A delegate that is executed when the browser requests window creation.
	 *
	 * @return true if if the window request was handled, false if the browser requesting the new window should be closed.
	 */
	bool HandleCreateWindow(const TWeakPtr<IWebBrowserWindow>& NewBrowserWindow, const TWeakPtr<IWebBrowserPopupFeatures>& PopupFeatures);

	/**
	 * A delegate that is executed when closing the browser window.
	 *
	 * @return true if if the window close was handled, false otherwise.
	 */
	bool HandleCloseWindow(const TWeakPtr<IWebBrowserWindow>& BrowserWindow);

	/** Callback for showing dialogs to the user */
	EWebBrowserDialogEventResponse HandleShowDialog(const TWeakPtr<IWebBrowserDialog>& DialogParams);

	/** Callback for dismissing any dialogs previously shown  */
	void HandleDismissAllDialogs();

	/** Callback for popup window permission */
	bool HandleBeforePopup(FString URL, FString Target);

	/** Callback for showing a popup menu */
	void HandleShowPopup(const FIntRect& PopupSize);

	/** Callback for hiding the popup menu */
	void HandleDismissPopup();

	/** Callback from the popup menu notifiying it has been dismissed */
	void HandleMenuDismissed(TSharedRef<IMenu>);

	virtual FPopupMethodReply OnQueryPopupMethod() const override
	{
		return PopupMenuMethod.IsSet()
			? FPopupMethodReply::UseMethod(PopupMenuMethod.GetValue())
			: FPopupMethodReply::Unhandled();
	}

	void HandleWindowDeactivated();
	void HandleWindowActivated();

private:

	/** Interface for dealing with a web browser window. */
	TSharedPtr<IWebBrowserWindow> BrowserWindow;
	/** The slate window that contains this widget. This must be stored weak otherwise we create a circular reference. */
	TWeakPtr<SWindow> SlateParentWindowPtr;
	/** Viewport interface for rendering the web page. */
	TSharedPtr<FWebBrowserViewport> BrowserViewport;
	/** Viewport interface for rendering popup menus. */
	TSharedPtr<FWebBrowserViewport>	MenuViewport;
	/** The implementation dependent widget that renders the browser contents. */
	TSharedPtr<SWebBrowserWidget> BrowserWidget;

	TArray<TSharedRef<IWebBrowserAdapter>> Adapters;

	/**
	 * An interface pointer to a menu object presenting a popup.
	 * Pointer is null when a popup is not visible.
	 */
	TWeakPtr<IMenu> PopupMenuPtr;

	/** Can be set to override the popup menu method used for popup menus. If not set, parent widgets will be queried instead. */
	TOptional<EPopupMethod> PopupMenuMethod;

	/** The url that appears in the address bar which can differ from the url of the loaded page */
	FText AddressBarUrl;

	/** A delegate that is invoked when document loading completed. */
	FSimpleDelegate OnLoadCompleted;

	/** A delegate that is invoked when document loading failed. */
	FSimpleDelegate OnLoadError;

	/** A delegate that is invoked when document loading started. */
	FSimpleDelegate OnLoadStarted;

	/** A delegate that is invoked when document title changed. */
	FOnTextChanged OnTitleChanged;

	/** A delegate that is invoked when document address changed. */
	FOnTextChanged OnUrlChanged;

	/** A delegate that is invoked when the browser attempts to pop up a new window */
	FOnBeforePopupDelegate OnBeforePopup;

	/** A delegate that is invoked when the browser requests a UI window for another browser it spawned */
	FOnCreateWindowDelegate OnCreateWindow;

	/** A delegate that is invoked when a window close event is detected */
	FOnCloseWindowDelegate OnCloseWindow;

	/** A delegate that is invoked prior to browser navigation */
	FOnBeforeBrowse OnBeforeNavigation;

	/** A delegate that is invoked when loading a resource, allowing the application to provide contents directly */
	FOnLoadUrl OnLoadUrl;

	/** A delegate that is invoked when when the browser needs to present a dialog to the user */
	FOnShowDialog OnShowDialog;

	/** A delegate that is invoked when when the browser needs to dismiss all dialogs */
	FSimpleDelegate OnDismissAllDialogs;

	FDelegateHandle SlateParentWindowSetupTickHandle;

	FOnSuppressContextMenu OnSuppressContextMenu;

protected:
	bool HandleSuppressContextMenu();

};
