// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CEF/CEFBrowserHandler.h"
#include "HAL/PlatformApplicationMisc.h"

#if WITH_CEF3

#include "WebBrowserModule.h"
#include "CEFBrowserClosureTask.h"
#include "IWebBrowserSingleton.h"
#include "WebBrowserSingleton.h"
#include "CEFBrowserPopupFeatures.h"
#include "CEFWebBrowserWindow.h"
#include "CEFBrowserByteResource.h"
#include "SlateApplication.h"
#include "ThreadingBase.h"


#define LOCTEXT_NAMESPACE "WebBrowserHandler"


// Used to force returning custom content instead of performing a request.
const FString CustomContentMethod(TEXT("X-GET-CUSTOM-CONTENT"));

FCEFBrowserHandler::FCEFBrowserHandler(bool InUseTransparency)
: bUseTransparency(InUseTransparency)
{ }

void FCEFBrowserHandler::OnTitleChange(CefRefPtr<CefBrowser> Browser, const CefString& Title)
{
	TSharedPtr<FCEFWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();

	if (BrowserWindow.IsValid())
	{
		BrowserWindow->SetTitle(Title);
	}
}

void FCEFBrowserHandler::OnAddressChange(CefRefPtr<CefBrowser> Browser, CefRefPtr<CefFrame> Frame, const CefString& Url)
{
	if (Frame->IsMain())
	{
		TSharedPtr<FCEFWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();

		if (BrowserWindow.IsValid())
		{
			BrowserWindow->SetUrl(Url);
		}
	}
}

bool FCEFBrowserHandler::OnTooltip(CefRefPtr<CefBrowser> Browser, CefString& Text)
{
	TSharedPtr<FCEFWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();
	if (BrowserWindow.IsValid())
	{
		BrowserWindow->SetToolTip(Text);
	}

	return false;
}

void FCEFBrowserHandler::OnAfterCreated(CefRefPtr<CefBrowser> Browser)
{
	if(Browser->IsPopup())
	{
		TSharedPtr<FCEFWebBrowserWindow> BrowserWindowParent = ParentHandler.get() ? ParentHandler->BrowserWindowPtr.Pin() : nullptr;
		if(BrowserWindowParent.IsValid() && ParentHandler->OnCreateWindow().IsBound())
		{
			TSharedPtr<FWebBrowserWindowInfo> NewBrowserWindowInfo = MakeShareable(new FWebBrowserWindowInfo(Browser, this));
			TSharedPtr<IWebBrowserWindow> NewBrowserWindow = IWebBrowserModule::Get().GetSingleton()->CreateBrowserWindow(
				BrowserWindowParent,
				NewBrowserWindowInfo
				);

			{
				// @todo: At the moment we need to downcast since the handler does not support using the interface.
				TSharedPtr<FCEFWebBrowserWindow> HandlerSpecificBrowserWindow = StaticCastSharedPtr<FCEFWebBrowserWindow>(NewBrowserWindow);
				BrowserWindowPtr = HandlerSpecificBrowserWindow;
			}

			// Request a UI window for the browser.  If it is not created we do some cleanup.
			bool bUIWindowCreated = ParentHandler->OnCreateWindow().Execute(TWeakPtr<IWebBrowserWindow>(NewBrowserWindow), TWeakPtr<IWebBrowserPopupFeatures>(BrowserPopupFeatures));
			if(!bUIWindowCreated)
			{
				NewBrowserWindow->CloseBrowser(true);
			}
			else
			{
				checkf(!NewBrowserWindow.IsUnique(), TEXT("Handler indicated that new window UI was created, but failed to save the new WebBrowserWindow instance."));
			}
		}
		else
		{
			Browser->GetHost()->CloseBrowser(true);
		}
	}
}

bool FCEFBrowserHandler::DoClose(CefRefPtr<CefBrowser> Browser)
{
	TSharedPtr<FCEFWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();
	if(BrowserWindow.IsValid())
	{
		BrowserWindow->OnBrowserClosing();
	}
#if PLATFORM_WINDOWS
	// If we have a window handle, we're rendering directly to the screen and not off-screen
	HWND NativeWindowHandle = Browser->GetHost()->GetWindowHandle();
	if (NativeWindowHandle != nullptr)
	{
		HWND ParentWindow = ::GetParent(NativeWindowHandle);

		if (ParentWindow)
		{
			HWND FocusHandle = ::GetFocus();
			if (FocusHandle && (FocusHandle == NativeWindowHandle || ::IsChild(NativeWindowHandle, FocusHandle)))
			{
				// Set focus to the parent window, otherwise keyboard and mouse wheel input will become wonky
				::SetFocus(ParentWindow);
			}
			// CEF will send a WM_CLOSE to the parent window and potentially exit the application if we don't do this
			::SetParent(NativeWindowHandle, nullptr);
		}
	}
#endif
	return false;
}

void FCEFBrowserHandler::OnBeforeClose(CefRefPtr<CefBrowser> Browser)
{
	TSharedPtr<FCEFWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();
	if (BrowserWindow.IsValid())
	{
		BrowserWindow->OnBrowserClosed();
	}

}

bool FCEFBrowserHandler::OnBeforePopup( CefRefPtr<CefBrowser> Browser,
	CefRefPtr<CefFrame> Frame,
	const CefString& TargetUrl,
	const CefString& TargetFrameName,
	const CefPopupFeatures& PopupFeatures,
	CefWindowInfo& OutWindowInfo,
	CefRefPtr<CefClient>& OutClient,
	CefBrowserSettings& OutSettings,
	bool* OutNoJavascriptAccess )
{
	FString URL = TargetUrl.ToWString().c_str();
	FString FrameName = TargetFrameName.ToWString().c_str();

	/* If OnBeforePopup() is not bound, we allow creating new windows as long as OnCreateWindow() is bound.
	   The BeforePopup delegate is always executed even if OnCreateWindow is not bound to anything .
	  */
	if((OnBeforePopup().IsBound() && OnBeforePopup().Execute(URL, FrameName)) || !OnCreateWindow().IsBound())
	{
		return true;
	}
	else
	{
		TSharedPtr<FCEFBrowserPopupFeatures> NewBrowserPopupFeatures = MakeShareable(new FCEFBrowserPopupFeatures(PopupFeatures));

		bool shouldUseTransparency = URL.Contains(TEXT("chrome-devtools")) ? false : bUseTransparency;

		cef_color_t Alpha = shouldUseTransparency ? 0 : CefColorGetA(OutSettings.background_color);
		cef_color_t R = CefColorGetR(OutSettings.background_color);
		cef_color_t G = CefColorGetG(OutSettings.background_color);
		cef_color_t B = CefColorGetB(OutSettings.background_color);
		OutSettings.background_color = CefColorSetARGB(Alpha, R, G, B);

		CefRefPtr<FCEFBrowserHandler> NewHandler(new FCEFBrowserHandler(shouldUseTransparency));
		NewHandler->ParentHandler = this;
		NewHandler->SetPopupFeatures(NewBrowserPopupFeatures);
		OutClient = NewHandler;

		// Always use off screen rendering so we can integrate with our windows
#if PLATFORM_LINUX
		OutWindowInfo.SetAsWindowless(kNullWindowHandle, shouldUseTransparency);
#else
		OutWindowInfo.SetAsWindowless(kNullWindowHandle);
#endif

		// We need to rely on CEF to create our window so we set the WindowInfo, BrowserSettings, Client, and then return false
		return false;
	}
}

bool FCEFBrowserHandler::OnCertificateError(CefRefPtr<CefBrowser> Browser,
	cef_errorcode_t CertError,
	const CefString &RequestUrl,
	CefRefPtr<CefSSLInfo> SslInfo,
	CefRefPtr<CefRequestCallback> Callback)
{
	// Forward the cert error to the normal load error handler
	CefString ErrorText = "Certificate error";
	OnLoadError(Browser, Browser->GetMainFrame(), CertError, ErrorText, RequestUrl);
	return false;
}

void FCEFBrowserHandler::OnLoadError(CefRefPtr<CefBrowser> Browser,
	CefRefPtr<CefFrame> Frame,
	CefLoadHandler::ErrorCode InErrorCode,
	const CefString& ErrorText,
	const CefString& FailedUrl)
{
	// notify browser window
	if (Frame->IsMain())
	{
		TSharedPtr<FCEFWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();

		if (BrowserWindow.IsValid())
		{
			BrowserWindow->NotifyDocumentError(InErrorCode, ErrorText, FailedUrl);
		}
	}
}

#if PLATFORM_LINUX
void FCEFBrowserHandler::OnLoadStart(CefRefPtr<CefBrowser> Browser, CefRefPtr<CefFrame> Frame)
{
}
#else
void FCEFBrowserHandler::OnLoadStart(CefRefPtr<CefBrowser> Browser, CefRefPtr<CefFrame> Frame, TransitionType CefTransitionType)
{
}
#endif


void FCEFBrowserHandler::OnLoadingStateChange(CefRefPtr<CefBrowser> Browser, bool bIsLoading, bool bCanGoBack, bool bCanGoForward)
{
	TSharedPtr<FCEFWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();

	if (BrowserWindow.IsValid())
	{
		BrowserWindow->NotifyDocumentLoadingStateChange(bIsLoading);
	}
}

bool FCEFBrowserHandler::GetRootScreenRect(CefRefPtr<CefBrowser> Browser, CefRect& Rect)
{
	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetDisplayMetrics(DisplayMetrics);
	Rect.width = DisplayMetrics.PrimaryDisplayWidth;
	Rect.height = DisplayMetrics.PrimaryDisplayHeight;
	return true;
}

bool FCEFBrowserHandler::GetViewRect(CefRefPtr<CefBrowser> Browser, CefRect& Rect)
{
	TSharedPtr<FCEFWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();

	if (BrowserWindow.IsValid())
	{
		return BrowserWindow->GetViewRect(Rect);
	}
	else
	{
		return false;
	}
}

void FCEFBrowserHandler::OnPaint(CefRefPtr<CefBrowser> Browser,
	PaintElementType Type,
	const RectList& DirtyRects,
	const void* Buffer,
	int Width, int Height)
{
	TSharedPtr<FCEFWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();

	if (BrowserWindow.IsValid())
	{
		BrowserWindow->OnPaint(Type, DirtyRects, Buffer, Width, Height);
	}
}

void FCEFBrowserHandler::OnCursorChange(CefRefPtr<CefBrowser> Browser, CefCursorHandle Cursor, CefRenderHandler::CursorType Type, const CefCursorInfo& CustomCursorInfo)
{
	TSharedPtr<FCEFWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();

	if (BrowserWindow.IsValid())
	{
		BrowserWindow->OnCursorChange(Cursor, Type, CustomCursorInfo);
	}

}

void FCEFBrowserHandler::OnPopupShow(CefRefPtr<CefBrowser> Browser, bool bShow)
{
	TSharedPtr<FCEFWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();

	if (BrowserWindow.IsValid())
	{
		BrowserWindow->ShowPopupMenu(bShow);
	}

}

void FCEFBrowserHandler::OnPopupSize(CefRefPtr<CefBrowser> Browser, const CefRect& Rect)
{
	TSharedPtr<FCEFWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();

	if (BrowserWindow.IsValid())
	{
		BrowserWindow->SetPopupMenuPosition(Rect);
	}
}

bool FCEFBrowserHandler::GetScreenInfo(CefRefPtr<CefBrowser> Browser, CefScreenInfo& ScreenInfo)
{
	TSharedPtr<FWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();

	if (BrowserWindow.IsValid() && BrowserWindow->GetParentWindow().IsValid())
	{
		ScreenInfo.device_scale_factor = BrowserWindow->GetParentWindow()->GetNativeWindow()->GetDPIScaleFactor();
	}
	else
	{
		FDisplayMetrics DisplayMetrics;
		FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);
		ScreenInfo.device_scale_factor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(DisplayMetrics.PrimaryDisplayWorkAreaRect.Left, DisplayMetrics.PrimaryDisplayWorkAreaRect.Top);
	}
	return true;
}


#if !PLATFORM_LINUX
void FCEFBrowserHandler::OnImeCompositionRangeChanged(
	CefRefPtr<CefBrowser> Browser,
	const CefRange& SelectionRange,
	const CefRenderHandler::RectList& CharacterBounds)
{
	TSharedPtr<FCEFWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();
	if (BrowserWindow.IsValid())
	{
		BrowserWindow->OnImeCompositionRangeChanged(Browser, SelectionRange, CharacterBounds);
	}
}
#endif

CefRequestHandler::ReturnValue FCEFBrowserHandler::OnBeforeResourceLoad(CefRefPtr<CefBrowser> Browser, CefRefPtr<CefFrame> Frame, CefRefPtr<CefRequest> Request, CefRefPtr<CefRequestCallback> Callback)
{
	// Current thread is IO thread. We need to invoke BrowserWindow->GetResourceContent on the UI (aka Game) thread:
	CefPostTask(TID_UI, new FCEFBrowserClosureTask(this, [=]()
	{
		const FString LanguageHeaderText(TEXT("Accept-Language"));
		const FString LocaleCode = FWebBrowserSingleton::GetCurrentLocaleCode();
		CefRequest::HeaderMap HeaderMap;
		Request->GetHeaderMap(HeaderMap);
		auto LanguageHeader = HeaderMap.find(*LanguageHeaderText);
		if (LanguageHeader != HeaderMap.end())
		{
			(*LanguageHeader).second = *LocaleCode;
		}
		else
		{
			HeaderMap.insert(std::pair<CefString, CefString>(*LanguageHeaderText, *LocaleCode));
		}

		TSharedPtr<FCEFWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();

		if (BrowserWindow.IsValid())
		{
			TOptional<FString> Contents = BrowserWindow->GetResourceContent(Frame, Request);
			if(Contents.IsSet())
			{
				Contents.GetValue().ReplaceInline(TEXT("\n"), TEXT(""), ESearchCase::CaseSensitive);
				Contents.GetValue().ReplaceInline(TEXT("\r"), TEXT(""), ESearchCase::CaseSensitive);

				// pass the text we'd like to come back as a response to the request post data
				CefRefPtr<CefPostData> PostData = CefPostData::Create();
				CefRefPtr<CefPostDataElement> Element = CefPostDataElement::Create();
				FTCHARToUTF8 UTF8String(*Contents.GetValue());
				Element->SetToBytes(UTF8String.Length(), UTF8String.Get());
				PostData->AddElement(Element);
				Request->SetPostData(PostData);

				// Set a custom request header, so we know the mime type if it was specified as a hash on the dummy URL
				std::string Url = Request->GetURL().ToString();
				std::string::size_type HashPos = Url.find_last_of('#');
				if (HashPos != std::string::npos)
				{
					std::string MimeType = Url.substr(HashPos + 1);
					HeaderMap.insert(std::pair<CefString, CefString>(TEXT("Content-Type"), MimeType));
				}

				// Change http method to tell GetResourceHandler to return the content
				Request->SetMethod(*CustomContentMethod);
			}
		}

		Request->SetHeaderMap(HeaderMap);

		Callback->Continue(true);
	}));

	// Tell CEF that we're handling this asynchronously.
	return RV_CONTINUE_ASYNC;
}

void FCEFBrowserHandler::OnRenderProcessTerminated(CefRefPtr<CefBrowser> Browser, TerminationStatus Status)
{
	TSharedPtr<FCEFWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();
	if (BrowserWindow.IsValid())
	{
		BrowserWindow->OnRenderProcessTerminated(Status);
	}
}

bool FCEFBrowserHandler::OnBeforeBrowse(CefRefPtr<CefBrowser> Browser,
	CefRefPtr<CefFrame> Frame,
	CefRefPtr<CefRequest> Request,
	bool IsRedirect)
{
	// Current thread: UI thread
	TSharedPtr<FCEFWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();
	if (BrowserWindow.IsValid())
	{
		if(BrowserWindow->OnBeforeBrowse(Browser, Frame, Request, IsRedirect))
		{
			return true;
		}
	}

	return false;
}

CefRefPtr<CefResourceHandler> FCEFBrowserHandler::GetResourceHandler( CefRefPtr<CefBrowser> Browser, CefRefPtr< CefFrame > Frame, CefRefPtr< CefRequest > Request )
{

	if (Request->GetMethod() == *CustomContentMethod)
	{
		// Content override header will be set by OnBeforeResourceLoad before passing the request on to this.
		if (Request->GetPostData() && Request->GetPostData()->GetElementCount() > 0)
		{
			// get the mime type from Content-Type header (default to text/html to support old behavior)
			FString MimeType = TEXT("text/html"); // default if not specified
			CefRequest::HeaderMap HeaderMap;
			Request->GetHeaderMap(HeaderMap);
			auto ContentOverride = HeaderMap.find(TEXT("Content-Type"));
			if (ContentOverride != HeaderMap.end())
			{
				MimeType = ContentOverride->second.ToWString().c_str();
			}

			// reply with the post data
			CefPostData::ElementVector Elements;
			Request->GetPostData()->GetElements(Elements);
			return new FCEFBrowserByteResource(Elements[0], MimeType);
		}
	}
	return nullptr;
}

void FCEFBrowserHandler::SetBrowserWindow(TSharedPtr<FCEFWebBrowserWindow> InBrowserWindow)
{
	BrowserWindowPtr = InBrowserWindow;
}

bool FCEFBrowserHandler::OnProcessMessageReceived(CefRefPtr<CefBrowser> Browser,
	CefProcessId SourceProcess,
	CefRefPtr<CefProcessMessage> Message)
{
	bool Retval = false;
	TSharedPtr<FCEFWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();
	if (BrowserWindow.IsValid())
	{
		Retval = BrowserWindow->OnProcessMessageReceived(Browser, SourceProcess, Message);
	}
	return Retval;
}

bool FCEFBrowserHandler::ShowDevTools(const CefRefPtr<CefBrowser>& Browser)
{
	CefPoint Point;
	CefString TargetUrl = "chrome-devtools://devtools/devtools.html";
	CefString TargetFrameName = "devtools";
	CefPopupFeatures PopupFeatures;
	CefWindowInfo WindowInfo;
	CefRefPtr<CefClient> NewClient;
	CefBrowserSettings BrowserSettings;
	bool NoJavascriptAccess = false;

	PopupFeatures.xSet = false;
	PopupFeatures.ySet = false;
	PopupFeatures.heightSet = false;
	PopupFeatures.widthSet = false;
	PopupFeatures.locationBarVisible = false;
	PopupFeatures.menuBarVisible = false;
	PopupFeatures.toolBarVisible  = false;
	PopupFeatures.statusBarVisible  = false;
	PopupFeatures.resizable = true;

	// Set max framerate to maximum supported.
	BrowserSettings.windowless_frame_rate = 60;
	// Disable plugins
	BrowserSettings.plugins = STATE_DISABLED;
	// Dev Tools look best with a white background color
	BrowserSettings.background_color = CefColorSetARGB(255, 255, 255, 255);

	// OnBeforePopup already takes care of all the details required to ask the host application to create a new browser window.
	bool bSuppressWindowCreation = OnBeforePopup(Browser, Browser->GetFocusedFrame(), TargetUrl, TargetFrameName, PopupFeatures, WindowInfo, NewClient, BrowserSettings, &NoJavascriptAccess);

	if(! bSuppressWindowCreation)
	{
		Browser->GetHost()->ShowDevTools(WindowInfo, NewClient, BrowserSettings, Point);
	}
	return !bSuppressWindowCreation;
}

bool FCEFBrowserHandler::OnKeyEvent(CefRefPtr<CefBrowser> Browser,
	const CefKeyEvent& Event,
	CefEventHandle OsEvent)
{
	// Show dev tools on CMD/CTRL+SHIFT+I
	if( (Event.type == KEYEVENT_RAWKEYDOWN || Event.type == KEYEVENT_KEYDOWN) &&
#if PLATFORM_MAC
		(Event.modifiers == (EVENTFLAG_COMMAND_DOWN | EVENTFLAG_SHIFT_DOWN)) &&
#else
		(Event.modifiers == (EVENTFLAG_CONTROL_DOWN | EVENTFLAG_SHIFT_DOWN)) &&
#endif
		(Event.unmodified_character == 'i' || Event.unmodified_character == 'I') &&
		IWebBrowserModule::Get().GetSingleton()->IsDevToolsShortcutEnabled()
	  )
	{
		return ShowDevTools(Browser);
	}

#if PLATFORM_MAC
	// We need to handle standard Copy/Paste/etc... shortcuts on OS X
	if( (Event.type == KEYEVENT_RAWKEYDOWN || Event.type == KEYEVENT_KEYDOWN) &&
		(Event.modifiers & EVENTFLAG_COMMAND_DOWN) != 0 &&
		(Event.modifiers & EVENTFLAG_CONTROL_DOWN) == 0 &&
		(Event.modifiers & EVENTFLAG_ALT_DOWN) == 0 &&
		( (Event.modifiers & EVENTFLAG_SHIFT_DOWN) == 0 || Event.unmodified_character == 'z' )
	  )
	{
		CefRefPtr<CefFrame> Frame = Browser->GetFocusedFrame();
		if (Frame)
		{
			switch (Event.unmodified_character)
			{
				case 'a':
					Frame->SelectAll();
					return true;
				case 'c':
					Frame->Copy();
					return true;
				case 'v':
					Frame->Paste();
					return true;
				case 'x':
					Frame->Cut();
					return true;
				case 'z':
					if( (Event.modifiers & EVENTFLAG_SHIFT_DOWN) == 0 )
					{
						Frame->Undo();
					}
					else
					{
						Frame->Redo();
					}
					return true;
			}
		}
	}
#endif
	TSharedPtr<FCEFWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();
	if (BrowserWindow.IsValid())
	{
		return BrowserWindow->OnUnhandledKeyEvent(Event);
	}

	return false;
}

#if PLATFORM_LINUX
bool FCEFBrowserHandler::OnJSDialog(CefRefPtr<CefBrowser> Browser, const CefString& OriginUrl, const CefString& AcceptLang, JSDialogType DialogType, const CefString& MessageText, const CefString& DefaultPromptText, CefRefPtr<CefJSDialogCallback> Callback, bool& OutSuppressMessage)
#else
bool FCEFBrowserHandler::OnJSDialog(CefRefPtr<CefBrowser> Browser, const CefString& OriginUrl, JSDialogType DialogType, const CefString& MessageText, const CefString& DefaultPromptText, CefRefPtr<CefJSDialogCallback> Callback, bool& OutSuppressMessage)
#endif
{
	bool Retval = false;
	TSharedPtr<FCEFWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();
	if (BrowserWindow.IsValid())
	{
		Retval = BrowserWindow->OnJSDialog(DialogType, MessageText, DefaultPromptText, Callback, OutSuppressMessage);
	}
	return Retval;
}

bool FCEFBrowserHandler::OnBeforeUnloadDialog(CefRefPtr<CefBrowser> Browser, const CefString& MessageText, bool IsReload, CefRefPtr<CefJSDialogCallback> Callback)
{
	bool Retval = false;
	TSharedPtr<FCEFWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();
	if (BrowserWindow.IsValid())
	{
		Retval = BrowserWindow->OnBeforeUnloadDialog(MessageText, IsReload, Callback);
	}
	return Retval;
}

void FCEFBrowserHandler::OnResetDialogState(CefRefPtr<CefBrowser> Browser)
{
	TSharedPtr<FCEFWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();
	if (BrowserWindow.IsValid())
	{
		BrowserWindow->OnResetDialogState();
	}
}


void FCEFBrowserHandler::OnBeforeContextMenu(CefRefPtr<CefBrowser> Browser, CefRefPtr<CefFrame> Frame, CefRefPtr<CefContextMenuParams> Params, CefRefPtr<CefMenuModel> Model)
{
	TSharedPtr<FCEFWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();
	if ( BrowserWindow.IsValid() && BrowserWindow->OnSuppressContextMenu().IsBound() && BrowserWindow->OnSuppressContextMenu().Execute() )
	{
		Model->Clear();
	}
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_CEF
