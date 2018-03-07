// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IOSPlatformWebBrowser.h"

#if PLATFORM_IOS
#include "IOSView.h"
#include "IOSAppDelegate.h"
#include "Widgets/SLeafWidget.h"

#import <UIKit/UIKit.h>

@implementation IOSWebViewWrapper

#if !PLATFORM_TVOS
@synthesize WebView;
#endif
@synthesize NextURL;
@synthesize NextContent;

-(void)create:(TSharedPtr<SIOSWebBrowserWidget>)InWebBrowserWidget useTransparency:(bool)InUseTransparency;
{
	WebBrowserWidget = InWebBrowserWidget;
	NextURL = nil;
	NextContent = nil;
	bNeedsAddToView = true;

#if !PLATFORM_TVOS
	dispatch_async(dispatch_get_main_queue(), ^
	{
		WebView = [[UIWebView alloc]initWithFrame:CGRectMake(1, 1, 100, 100)];
		WebView.delegate = self;

		if (InUseTransparency)
		{
			[self.WebView setOpaque:NO];
			[self.WebView setBackgroundColor:[UIColor clearColor]];
		}
		else
		{
			[self.WebView setOpaque:YES];
		}
	});
#endif
}

-(void)close;
{
#if !PLATFORM_TVOS
	WebView.delegate = nil;
	dispatch_async(dispatch_get_main_queue(), ^
	{
		[self.WebView removeFromSuperview];
		WebView = nil;
	});
#endif
}

-(void)updateframe:(CGRect)InFrame;
{
	self.DesiredFrame = InFrame;

#if !PLATFORM_TVOS
	dispatch_async(dispatch_get_main_queue(), ^
	{
		if(WebView != nil)
		{
			WebView.frame = self.DesiredFrame;
			if(bNeedsAddToView)
			{
				bNeedsAddToView = false;
				[[IOSAppDelegate GetDelegate].IOSView addSubview:WebView];
			}
			else
			{
				if(NextContent != nil)
				{
					// Load web content from string
					[self.WebView loadHTMLString:NextContent baseURL:NextURL];
					NextContent = nil;
					NextURL = nil;
				}
				else
				if(NextURL != nil)
				{
					// Load web content from URL
					NSURLRequest *nsrequest = [NSURLRequest requestWithURL:NextURL];
					[self.WebView loadRequest : nsrequest];
					NextURL = nil;
				}
			}
		}
	});
#endif
}

-(void)executejavascript:(NSString*)InJavaScript
{
#if !PLATFORM_TVOS
	dispatch_async(dispatch_get_main_queue(), ^
	{
		[self.WebView stringByEvaluatingJavaScriptFromString:InJavaScript];
	});
#endif
}

-(void)loadurl:(NSURL*)InURL;
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		self.NextURL = InURL;
	});
}

-(void)loadstring:(NSString*)InString dummyurl:(NSURL*)InURL;
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		self.NextContent = InString;
		self.NextURL = InURL;
	});
}

@end

class SIOSWebBrowserWidget : public SLeafWidget
{
	SLATE_BEGIN_ARGS(SIOSWebBrowserWidget)
		: _InitialURL("about:blank")
		, _UseTransparency(false)
	{ }
	
		SLATE_ARGUMENT(FString, InitialURL);
		SLATE_ARGUMENT(bool, UseTransparency);
	
	SLATE_END_ARGS()

	SIOSWebBrowserWidget()
	: WebViewWrapper(nil)
	{}

	void Construct(const FArguments& Args)
	{
		WebViewWrapper = [IOSWebViewWrapper alloc];
		[WebViewWrapper create: TSharedPtr<SIOSWebBrowserWidget>(this) useTransparency:Args._UseTransparency];
		LoadURL(Args._InitialURL);
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		if(WebViewWrapper != nil)
		{
			UIView* View = [IOSAppDelegate GetDelegate].IOSView;
			CGFloat contentScaleFactor = View.contentScaleFactor;
			FVector2D Position = AllottedGeometry.GetAccumulatedRenderTransform().GetTranslation() / contentScaleFactor;
			FVector2D Size = TransformVector(AllottedGeometry.GetAccumulatedRenderTransform(), AllottedGeometry.GetLocalSize()) / contentScaleFactor;
			CGRect NewFrame;
			NewFrame.origin.x = FMath::RoundToInt(Position.X);
			NewFrame.origin.y = FMath::RoundToInt(Position.Y);
			NewFrame.size.width = FMath::RoundToInt(Size.X);
			NewFrame.size.height = FMath::RoundToInt(Size.Y);

			[WebViewWrapper updateframe:NewFrame];
		}

		return LayerId;
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(640, 480);
	}

	void LoadURL(const FString& InNewURL)
	{
		if( WebViewWrapper != nil)
		{
			[WebViewWrapper loadurl:[NSURL URLWithString:[NSString stringWithUTF8String:TCHAR_TO_UTF8(*InNewURL)]]];
		}
	}

	void LoadString(const FString& InContents, const FString& InDummyURL)
	{
		if( WebViewWrapper != nil)
		{
			[WebViewWrapper loadstring:[NSString stringWithUTF8String:TCHAR_TO_UTF8(*InContents)] dummyurl:[NSURL URLWithString:[NSString stringWithUTF8String:TCHAR_TO_UTF8(*InDummyURL)]]];
		}
	}
	
	void ExecuteJavascript(const FString& Script)
	{
		if (WebViewWrapper != nil)
		{
			[WebViewWrapper executejavascript:[NSString stringWithUTF8String:TCHAR_TO_UTF8(*Script)]];
		}
	}

	void Close()
	{
		if( WebViewWrapper != nil)
		{
			[WebViewWrapper close];
			WebViewWrapper = nil;
		}
	}

	~SIOSWebBrowserWidget()
	{
		Close();
	}
	
protected:
	mutable __strong IOSWebViewWrapper* WebViewWrapper;
};


FWebBrowserWindow::FWebBrowserWindow(FString InUrl, TOptional<FString> InContentsToLoad, bool InShowErrorMessage, bool InThumbMouseButtonNavigation, bool InUseTransparency)
	: CurrentUrl(MoveTemp(InUrl))
	, ContentsToLoad(MoveTemp(InContentsToLoad))
	, bUseTransparency(InUseTransparency)
{
}

FWebBrowserWindow::~FWebBrowserWindow()
{
	CloseBrowser(true);
}

void FWebBrowserWindow::LoadURL(FString NewURL)
{
	BrowserWidget->LoadURL(NewURL);
}

void FWebBrowserWindow::LoadString(FString Contents, FString DummyURL)
{
	BrowserWidget->LoadString(Contents, DummyURL);
}

TSharedRef<SWidget> FWebBrowserWindow::CreateWidget()
{
	TSharedRef<SIOSWebBrowserWidget> BrowserWidgetRef =
		SNew(SIOSWebBrowserWidget)
		.UseTransparency(bUseTransparency)
		.InitialURL(CurrentUrl);

	BrowserWidget = BrowserWidgetRef;
	return BrowserWidgetRef;
}

void FWebBrowserWindow::SetViewportSize(FIntPoint WindowSize, FIntPoint WindowPos)
{
}

FSlateShaderResource* FWebBrowserWindow::GetTexture(bool bIsPopup /*= false*/)
{
	return nullptr;
}

bool FWebBrowserWindow::IsValid() const
{
	return false;
}

bool FWebBrowserWindow::IsInitialized() const
{
	return true;
}

bool FWebBrowserWindow::IsClosing() const
{
	return false;
}

EWebBrowserDocumentState FWebBrowserWindow::GetDocumentLoadingState() const
{
	return EWebBrowserDocumentState::Loading;
}

FString FWebBrowserWindow::GetTitle() const
{
	return "";
}

FString FWebBrowserWindow::GetUrl() const
{
	return "";
}

bool FWebBrowserWindow::OnKeyDown(const FKeyEvent& InKeyEvent)
{
	return false;
}

bool FWebBrowserWindow::OnKeyUp(const FKeyEvent& InKeyEvent)
{
	return false;
}

bool FWebBrowserWindow::OnKeyChar(const FCharacterEvent& InCharacterEvent)
{
	return false;
}

FReply FWebBrowserWindow::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

FReply FWebBrowserWindow::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

FReply FWebBrowserWindow::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

FReply FWebBrowserWindow::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

void FWebBrowserWindow::OnMouseLeave(const FPointerEvent& MouseEvent)
{
}

FReply FWebBrowserWindow::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

void FWebBrowserWindow::OnFocus(bool SetFocus, bool bIsPopup)
{
}

void FWebBrowserWindow::OnCaptureLost()
{
}

bool FWebBrowserWindow::CanGoBack() const
{
	return false;
}

void FWebBrowserWindow::GoBack()
{
}

bool FWebBrowserWindow::CanGoForward() const
{
	return false;
}

void FWebBrowserWindow::GoForward()
{
}

bool FWebBrowserWindow::IsLoading() const
{
	return false;
}

void FWebBrowserWindow::Reload()
{
}

void FWebBrowserWindow::StopLoad()
{
}

void FWebBrowserWindow::GetSource(TFunction<void (const FString&)> Callback) const
{
	Callback(FString());
}

int FWebBrowserWindow::GetLoadError()
{
	return 0;
}

void FWebBrowserWindow::SetIsDisabled(bool bValue)
{
}


void FWebBrowserWindow::ExecuteJavascript(const FString& Script)
{
	BrowserWidget->ExecuteJavascript(Script);
}

void FWebBrowserWindow::CloseBrowser(bool bForce)
{
	BrowserWidget->Close();
}

void FWebBrowserWindow::BindUObject(const FString& Name, UObject* Object, bool bIsPermanent /*= true*/)
{
}

void FWebBrowserWindow::UnbindUObject(const FString& Name, UObject* Object /*= nullptr*/, bool bIsPermanent /*= true*/)
{
}

#endif
