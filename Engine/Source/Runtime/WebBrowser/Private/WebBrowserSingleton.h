// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "IWebBrowserSingleton.h"

#if WITH_CEF3
#if PLATFORM_WINDOWS
	#include "WindowsHWrapper.h"
	#include "AllowWindowsPlatformTypes.h"
	#include "AllowWindowsPlatformAtomics.h"
#endif
#pragma push_macro("OVERRIDE")
#undef OVERRIDE // cef headers provide their own OVERRIDE macro
THIRD_PARTY_INCLUDES_START
#include "include/internal/cef_ptr.h"
#include "include/cef_request_context.h"
THIRD_PARTY_INCLUDES_END
#pragma pop_macro("OVERRIDE")
#if PLATFORM_WINDOWS
	#include "HideWindowsPlatformAtomics.h"
	#include "HideWindowsPlatformTypes.h"
#endif
#include "CEF/CEFSchemeHandler.h"
class CefListValue;
class FCEFBrowserApp;
class FCEFWebBrowserWindow;
#endif

class IWebBrowserCookieManager;
class IWebBrowserWindow;
struct FWebBrowserWindowInfo;
struct FWebBrowserInitSettings;

PRAGMA_DISABLE_DEPRECATION_WARNINGS

/**
 * Implementation of singleton class that takes care of general web browser tasks
 */
class FWebBrowserSingleton
	: public IWebBrowserSingleton
	, public FTickerObjectBase
{
public:

	/** Constructor. */
	FWebBrowserSingleton(const FWebBrowserInitSettings& WebBrowserInitSettings);

	/** Virtual destructor. */
	virtual ~FWebBrowserSingleton();

	/**
	* Gets the Current Locale Code in the format CEF expects
	*
	* @return Locale code as either "xx" or "xx-YY"
	*/
	static FString GetCurrentLocaleCode();

	virtual FString ApplicationCacheDir() const override;

public:

	// IWebBrowserSingleton Interface

	virtual TSharedRef<IWebBrowserWindowFactory> GetWebBrowserWindowFactory() const override;

	TSharedPtr<IWebBrowserWindow> CreateBrowserWindow(
		TSharedPtr<FCEFWebBrowserWindow>& BrowserWindowParent,
		TSharedPtr<FWebBrowserWindowInfo>& BrowserWindowInfo) override;

	TSharedPtr<IWebBrowserWindow> CreateBrowserWindow(
		void* OSWindowHandle,
		FString InitialURL,
		bool bUseTransparency,
		bool bThumbMouseButtonNavigation,
		TOptional<FString> ContentsToLoad = TOptional<FString>(),
		bool ShowErrorMessage = true,
		FColor BackgroundColor = FColor(255, 255, 255, 255),
		int BrowserFrameRate = 24 ) override;

	TSharedPtr<IWebBrowserWindow> CreateBrowserWindow(const FCreateBrowserWindowSettings& Settings) override;

	virtual void DeleteBrowserCookies(FString URL = TEXT(""), FString CookieName = TEXT(""), TFunction<void(int)> Completed = nullptr) override;

	virtual TSharedPtr<IWebBrowserCookieManager> GetCookieManager() const override
	{
		return DefaultCookieManager;
	}

	virtual TSharedPtr<IWebBrowserCookieManager> GetCookieManager(TOptional<FString> ContextId) const override;

	virtual bool RegisterContext(const FBrowserContextSettings& Settings) override;

	virtual bool UnregisterContext(const FString& ContextId) override;

	virtual bool RegisterSchemeHandlerFactory(FString Scheme, FString Domain, IWebBrowserSchemeHandlerFactory* WebBrowserSchemeHandlerFactory) override;

	virtual bool UnregisterSchemeHandlerFactory(IWebBrowserSchemeHandlerFactory* WebBrowserSchemeHandlerFactory) override;

	virtual bool IsDevToolsShortcutEnabled() override
	{
		return bDevToolsShortcutEnabled;
	}

	virtual void SetDevToolsShortcutEnabled(bool Value) override
	{
		bDevToolsShortcutEnabled = Value;
	}

	virtual void SetJSBindingToLoweringEnabled(bool bEnabled) override
	{
		bJSBindingsToLoweringEnabled = bEnabled;
	}

public:

	// FTickerObjectBase Interface

	virtual bool Tick(float DeltaTime) override;

private:

	TSharedPtr<IWebBrowserCookieManager> DefaultCookieManager;

#if WITH_CEF3
	/** When new render processes are created, send all permanent variable bindings to them. */
	void HandleRenderProcessCreated(CefRefPtr<CefListValue> ExtraInfo);
	/** Pointer to the CEF App implementation */
	CefRefPtr<FCEFBrowserApp>			CEFBrowserApp;
	/** List of currently existing browser windows */
	TArray<TWeakPtr<FCEFWebBrowserWindow>>	WindowInterfaces;
	/** Critical section for thread safe modification of WindowInterfaces array. */
	FCriticalSection WindowInterfacesCS;

	TMap<FString, CefRefPtr<CefRequestContext>> RequestContexts;
	FCefSchemeHandlerFactories SchemeHandlerFactories;
#endif

	TSharedRef<IWebBrowserWindowFactory> WebBrowserWindowFactory;

	bool bDevToolsShortcutEnabled;

	bool bJSBindingsToLoweringEnabled;

};

PRAGMA_ENABLE_DEPRECATION_WARNINGS

#if WITH_CEF3

class CefCookieManager;

class FCefWebBrowserCookieManagerFactory
{
public:
	static TSharedRef<IWebBrowserCookieManager> Create(
		const CefRefPtr<CefCookieManager>& CookieManager);
};

#endif
