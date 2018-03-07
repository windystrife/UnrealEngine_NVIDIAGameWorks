// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rendering/SlateRenderer.h"

class FCEFWebBrowserWindow;
class IWebBrowserCookieManager;
class IWebBrowserWindow;
class IWebBrowserSchemeHandlerFactory;
struct FWebBrowserWindowInfo;

class IWebBrowserWindowFactory
{
public:

	virtual TSharedPtr<IWebBrowserWindow> Create(
		TSharedPtr<FCEFWebBrowserWindow>& BrowserWindowParent,
		TSharedPtr<FWebBrowserWindowInfo>& BrowserWindowInfo) = 0;

	virtual TSharedPtr<IWebBrowserWindow> Create(
		void* OSWindowHandle,
		FString InitialURL,
		bool bUseTransparency,
		bool bThumbMouseButtonNavigation,
		TOptional<FString> ContentsToLoad = TOptional<FString>(),
		bool ShowErrorMessage = true,
		FColor BackgroundColor = FColor(255, 255, 255, 255)) = 0;
};

struct WEBBROWSER_API FBrowserContextSettings
{
	FBrowserContextSettings(const FString& InId)
		: Id(InId)
		, AcceptLanguageList()
		, CookieStorageLocation()
		, bPersistSessionCookies(false)
		, bIgnoreCertificateErrors(false)
	{ }

	FString Id;
	FString AcceptLanguageList;
	FString CookieStorageLocation;
	bool bPersistSessionCookies;
	bool bIgnoreCertificateErrors;
};


struct WEBBROWSER_API FCreateBrowserWindowSettings
{

	FCreateBrowserWindowSettings()
		: OSWindowHandle(nullptr)
		, InitialURL()
		, bUseTransparency(false)
		, bThumbMouseButtonNavigation(false)
		, ContentsToLoad()
		, bShowErrorMessage(true)
		, BackgroundColor(FColor(255, 255, 255, 255))
		, BrowserFrameRate(24)
		, Context()
	{ }

	void* OSWindowHandle;
	FString InitialURL;
	bool bUseTransparency;
	bool bThumbMouseButtonNavigation;
	TOptional<FString> ContentsToLoad;
	bool bShowErrorMessage;
	FColor BackgroundColor;
	int BrowserFrameRate;
	TOptional<FBrowserContextSettings> Context;
};

/**
 * A singleton class that takes care of general web browser tasks
 */
class WEBBROWSER_API IWebBrowserSingleton
{
public:
	/**
	 * Virtual Destructor
	 */
	virtual ~IWebBrowserSingleton() {};

	/** @return A factory object that can be used to construct additional WebBrowserWindows on demand. */
	virtual TSharedRef<IWebBrowserWindowFactory> GetWebBrowserWindowFactory() const = 0;


	/**
	 * Create a new web browser window
	 *
	 * @param BrowserWindowParent The parent browser window
	 * @param BrowserWindowInfo Info for setting up the new browser window
	 * @return New Web Browser Window Interface (may be null if not supported)
	 */
	virtual TSharedPtr<IWebBrowserWindow> CreateBrowserWindow(
		TSharedPtr<FCEFWebBrowserWindow>& BrowserWindowParent,
		TSharedPtr<FWebBrowserWindowInfo>& BrowserWindowInfo
		) = 0;

	/**
	 * Create a new web browser window
	 *
	 * @param OSWindowHandle Handle of OS Window that the browser will be displayed in (can be null)
	 * @param InitialURL URL that the browser should initially navigate to
	 * @param bUseTransparency Whether to allow transparent rendering of pages
	 * @param ContentsToLoad Optional string to load as a web page
	 * @param ShowErrorMessage Whether to show an error message in case of loading errors.
	 * @param BackgroundColor Opaque background color used before a document is loaded and when no document color is specified.
	 * @param BrowserFrameRate The framerate of the browser in FPS
	 * @return New Web Browser Window Interface (may be null if not supported)
	 */
	DEPRECATED(4.11, "Please use the new overload that takes a settings struct.")
	virtual TSharedPtr<IWebBrowserWindow> CreateBrowserWindow(
		void* OSWindowHandle,
		FString InitialURL,
		bool bUseTransparency,
		bool bThumbMouseButtonNavigation,
		TOptional<FString> ContentsToLoad = TOptional<FString>(),
		bool ShowErrorMessage = true,
		FColor BackgroundColor = FColor(255, 255, 255, 255),
		int BrowserFrameRate = 24 ) = 0;

	virtual TSharedPtr<IWebBrowserWindow> CreateBrowserWindow(const FCreateBrowserWindowSettings& Settings) = 0;

	/**
	 * Delete all browser cookies.
	 *
	 * Removes all matching cookies. Leave both URL and CookieName blank to delete the entire cookie database.
	 * The actual deletion will be scheduled on the browser IO thread, so the operation may not have completed when the function returns.
	 *
	 * @param URL The base URL to match when searching for cookies to remove. Use blank to match all URLs.
	 * @param CookieName The name of the cookie to delete. If left unspecified, all cookies will be removed.
	 * @param Completed A callback function that will be invoked asynchronously on the game thread when the deletion is complete passing in the number of deleted objects.
	 */
	DEPRECATED(4.11, "Please use the CookieManager instead via GetCookieManager().")
	virtual void DeleteBrowserCookies(FString URL = TEXT(""), FString CookieName = TEXT(""), TFunction<void(int)> Completed = nullptr) = 0;

	virtual TSharedPtr<class IWebBrowserCookieManager> GetCookieManager() const = 0;

	virtual TSharedPtr<class IWebBrowserCookieManager> GetCookieManager(TOptional<FString> ContextId) const = 0;

	virtual bool RegisterContext(const FBrowserContextSettings& Settings) = 0;

	virtual bool UnregisterContext(const FString& ContextId) = 0;

	// @return the application cache dir where the cookies are stored
	virtual FString ApplicationCacheDir() const = 0;
	/**
	 * Registers a custom scheme handler factory, for a given scheme and domain. The domain is ignored if the scheme is not a browser built in scheme
	 * and all requests will go through this factory.
	 * @param Scheme                            The scheme name to handle.
	 * @param Domain                            The domain name to handle.
	 * @param WebBrowserSchemeHandlerFactory    The factory implementation for creating request handlers for this scheme.
	 */
	virtual bool RegisterSchemeHandlerFactory(FString Scheme, FString Domain, IWebBrowserSchemeHandlerFactory* WebBrowserSchemeHandlerFactory) = 0;

	/**
	 * Unregister a custom scheme handler factory. The factory may still be used by existing open browser windows, but will no longer be provided for new ones.
	 * @param WebBrowserSchemeHandlerFactory    The factory implementation to remove.
	 */
	virtual bool UnregisterSchemeHandlerFactory(IWebBrowserSchemeHandlerFactory* WebBrowserSchemeHandlerFactory) = 0;

	/**
	 * Enable or disable CTRL/CMD-SHIFT-I shortcut to show the Chromium Dev tools window.
	 * The value defaults to true on debug builds, otherwise false.
	 *
	 * The relevant handlers for spawning new browser windows have to be set up correctly in addition to this flag being true before anything is shown.
	 *
	 * @param Value a boolean value to enable or disable the keyboard shortcut.
	 */
	virtual void SetDevToolsShortcutEnabled(bool Value) = 0;


	/**
	 * Returns wether the CTRL/CMD-SHIFT-I shortcut to show the Chromium Dev tools window is enabled.
	 *
	 * The relevant handlers for spawning new browser windows have to be set up correctly in addition to this flag being true before anything is shown.
	 *
	 * @return a boolean value indicating whether the keyboard shortcut is enabled or not.
	 */
	virtual bool IsDevToolsShortcutEnabled() = 0;


	/**
	 * Enable or disable to-lowering of JavaScript object member bindings.
	 *
	 * Due to how JavaScript to UObject bridges require the use of FNames for building up the JS API objects, it is possible for case-sensitivity issues
	 * to develop if an FName has been previously created with differing case to your function or property names. To-lowering the member names allows
	 * a guaranteed casing for the web page's JS to reference.
	 *
	 * Default behavior is enabled, so that all JS side objects have only lowercase members.
	 *
	 * @param bEnabled a boolean value to enable or disable the to-lowering.
	 */
	virtual void SetJSBindingToLoweringEnabled(bool bEnabled) = 0;

};
