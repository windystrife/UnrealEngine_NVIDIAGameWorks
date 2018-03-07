// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AndroidWebBrowserWidget.h"
#include "AndroidWebBrowserWindow.h"
#include "AndroidWebBrowserDialog.h"
#include "AndroidJSScripting.h"
#include "AndroidApplication.h"
#include "AndroidWindow.h"
#include "AndroidJava.h"
#include "Async.h"
#include "ScopeLock.h"

// For UrlDecode
#include "Http.h"

#include <jni.h>

FCriticalSection SAndroidWebBrowserWidget::WebControlsCS;
TMap<jlong, TWeakPtr<SAndroidWebBrowserWidget>> SAndroidWebBrowserWidget::AllWebControls;

TSharedPtr<SAndroidWebBrowserWidget> SAndroidWebBrowserWidget::GetWidgetPtr(JNIEnv* JEnv, jobject Jobj)
{
	FScopeLock L(&WebControlsCS);

	jclass Class = JEnv->GetObjectClass(Jobj);
	jmethodID JMethod = JEnv->GetMethodID(Class, "GetNativePtr", "()J");
	check(JMethod != nullptr);

	int64 ObjAddr = JEnv->CallLongMethod(Jobj, JMethod);

	TWeakPtr<SAndroidWebBrowserWidget> WebControl = AllWebControls.FindRef(ObjAddr);
	return (WebControl.IsValid()) ? WebControl.Pin() : TSharedPtr<SAndroidWebBrowserWidget>();
}

SAndroidWebBrowserWidget::~SAndroidWebBrowserWidget()
{
	FScopeLock L(&WebControlsCS);
	AllWebControls.Remove(reinterpret_cast<jlong>(this));
}

void SAndroidWebBrowserWidget::Construct(const FArguments& Args)
{
	{
		FScopeLock L(&WebControlsCS);
		AllWebControls.Add(reinterpret_cast<jlong>(this), StaticCastSharedRef<SAndroidWebBrowserWidget>(AsShared()));
	}

	WebBrowserWindowPtr = Args._WebBrowserWindow;
	HistorySize = 0;
	HistoryPosition = 0;

	JWebView.Emplace("com/epicgames/ue4/WebViewControl", "(JZZ)V", reinterpret_cast<jlong>(this), !(UE_BUILD_SHIPPING || UE_BUILD_TEST), Args._UseTransparency);
	JWebView_Update = JWebView->GetClassMethod("Update", "(IIII)V");
	JWebView_ExecuteJavascript = JWebView->GetClassMethod("ExecuteJavascript", "(Ljava/lang/String;)V");
	JWebView_LoadURL = JWebView->GetClassMethod("LoadURL", "(Ljava/lang/String;)V");
	JWebView_LoadString = JWebView->GetClassMethod("LoadString", "(Ljava/lang/String;Ljava/lang/String;)V");
	JWebView_StopLoad = JWebView->GetClassMethod("StopLoad", "()V");
	JWebView_Reload = JWebView->GetClassMethod("Reload", "()V");
	JWebView_Close = JWebView->GetClassMethod("Close", "()V");
	JWebView_GoBackOrForward = JWebView->GetClassMethod("GoBackOrForward", "(I)V");
	JWebView->CallMethod<void>(JWebView_LoadURL.GetValue(), FJavaClassObject::GetJString(Args._InitialURL));
}

int32 SAndroidWebBrowserWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// Calculate UIScale, which can vary frame-to-frame thanks to device rotation
	// UI Scale is calculated relative to vertical axis of 1280x720 / 720x1280
	float UIScale;
	FPlatformRect ScreenRect = FAndroidWindow::GetScreenRect();
	int32_t ScreenWidth, ScreenHeight;
	FAndroidWindow::CalculateSurfaceSize(FAndroidWindow::GetHardwareWindow(), ScreenWidth, ScreenHeight);
	if (ScreenWidth > ScreenHeight)
	{
		UIScale = (float)ScreenHeight / (ScreenRect.Bottom - ScreenRect.Top);
	}
	else
	{
		UIScale = (float)ScreenHeight / (ScreenRect.Bottom - ScreenRect.Top);
	}

	FVector2D Position = AllottedGeometry.GetAccumulatedRenderTransform().GetTranslation() * UIScale;
	FVector2D Size = TransformVector(AllottedGeometry.GetAccumulatedRenderTransform(), AllottedGeometry.GetLocalSize()) * UIScale;

	// Convert position to integer coordinates
	FIntPoint IntPos(FMath::RoundToInt(Position.X), FMath::RoundToInt(Position.Y));
	// Convert size to integer taking the rounding of position into account to avoid double round-down or double round-up causing a noticeable error.
	FIntPoint IntSize = FIntPoint(FMath::RoundToInt(Position.X + Size.X), FMath::RoundToInt(Size.Y + Position.Y)) - IntPos;

	JWebView->CallMethod<void>(JWebView_Update.GetValue(), IntPos.X, IntPos.Y, IntSize.X, IntSize.Y);

	return LayerId;
}

FVector2D SAndroidWebBrowserWidget::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(640, 480);
}

void SAndroidWebBrowserWidget::ExecuteJavascript(const FString& Script)
{
	JWebView->CallMethod<void>(JWebView_ExecuteJavascript.GetValue(), FJavaClassObject::GetJString(Script));
}

void SAndroidWebBrowserWidget::LoadURL(const FString& NewURL)
{
	JWebView->CallMethod<void>(JWebView_LoadURL.GetValue(), FJavaClassObject::GetJString(NewURL));
}

void SAndroidWebBrowserWidget::LoadString(const FString& Contents, const FString& BaseUrl)
{
	JWebView->CallMethod<void>(JWebView_LoadString.GetValue(), FJavaClassObject::GetJString(Contents), FJavaClassObject::GetJString(BaseUrl));
}

void SAndroidWebBrowserWidget::StopLoad()
{
	JWebView->CallMethod<void>(JWebView_StopLoad.GetValue());
}

void SAndroidWebBrowserWidget::Reload()
{
	JWebView->CallMethod<void>(JWebView_Reload.GetValue());
}

void SAndroidWebBrowserWidget::Close()
{
	JWebView->CallMethod<void>(JWebView_Close.GetValue());
	WebBrowserWindowPtr.Reset();
}

void SAndroidWebBrowserWidget::GoBack()
{
	JWebView->CallMethod<void>(JWebView_GoBackOrForward.GetValue(), -1);
}

void SAndroidWebBrowserWidget::GoForward()
{
	JWebView->CallMethod<void>(JWebView_GoBackOrForward.GetValue(), 1);
}

bool SAndroidWebBrowserWidget::CanGoBack()
{
	return HistoryPosition > 1;
}

bool SAndroidWebBrowserWidget::CanGoForward()
{
	return HistoryPosition < HistorySize-1;
}

jbyteArray SAndroidWebBrowserWidget::HandleShouldInterceptRequest(jstring JUrl)
{
	JNIEnv*	JEnv = FAndroidApplication::GetJavaEnv();

	const char* JUrlChars = JEnv->GetStringUTFChars(JUrl, 0);
	FString Url = UTF8_TO_TCHAR(JUrlChars);
	JEnv->ReleaseStringUTFChars(JUrl, JUrlChars);

	FString Response;
	bool bOverrideResponse = false;
	int32 Position = Url.Find(*FAndroidJSScripting::JSMessageTag, ESearchCase::CaseSensitive);
	if (Position >= 0)
	{
		AsyncTask(ENamedThreads::GameThread, [Url, Position, this]()
		{
			if (WebBrowserWindowPtr.IsValid())
			{
				TSharedPtr<FAndroidWebBrowserWindow> BrowserWindow = WebBrowserWindowPtr.Pin();
				if (BrowserWindow.IsValid())
				{
					FString Origin = Url.Left(Position);
					FString Message = Url.RightChop(Position + FAndroidJSScripting::JSMessageTag.Len());

					TArray<FString> Params;
					Message.ParseIntoArray(Params, TEXT("/"), false);
					if (Params.Num() > 0)
					{
						for (int I = 0; I < Params.Num(); I++)
						{
							Params[I] = FPlatformHttp::UrlDecode(Params[I]);
						}

						FString Command = Params[0];
						Params.RemoveAt(0, 1);
						BrowserWindow->OnJsMessageReceived(Command, Params, Origin);
					}
					else
					{
						GLog->Logf(ELogVerbosity::Error, TEXT("Invalid message from browser view: %s"), *Message);
					}
				}
			}
		});
		bOverrideResponse = true;
	}
	else
	{
		if (WebBrowserWindowPtr.IsValid())
		{
			TSharedPtr<FAndroidWebBrowserWindow> BrowserWindow = WebBrowserWindowPtr.Pin();
			if (BrowserWindow.IsValid() && BrowserWindow->OnLoadUrl().IsBound())
			{
				FString Method = TEXT(""); // We don't support passing anything but the requested URL
				bOverrideResponse = BrowserWindow->OnLoadUrl().Execute(Method, Url, Response);
			}
		}
	}

	if ( bOverrideResponse )
	{
		FTCHARToUTF8 Converter(*Response);
		jbyteArray Buffer = JEnv->NewByteArray(Converter.Length());
		JEnv->SetByteArrayRegion(Buffer, 0, Converter.Length(), reinterpret_cast<const jbyte *>(Converter.Get()));
		return Buffer;
	}
	return nullptr;
}

bool SAndroidWebBrowserWidget::HandleShouldOverrideUrlLoading(jstring JUrl)
{
	JNIEnv*	JEnv = FAndroidApplication::GetJavaEnv();

	const char* JUrlChars = JEnv->GetStringUTFChars(JUrl, 0);
	FString Url = UTF8_TO_TCHAR(JUrlChars);
	JEnv->ReleaseStringUTFChars(JUrl, JUrlChars);
	bool Retval = false;

	if (WebBrowserWindowPtr.IsValid())
	{
		TSharedPtr<FAndroidWebBrowserWindow> BrowserWindow = WebBrowserWindowPtr.Pin();
		if (BrowserWindow.IsValid())
		{
			if (BrowserWindow->OnBeforeBrowse().IsBound())
			{
				FWebNavigationRequest RequestDetails;
				RequestDetails.bIsRedirect = false;
				RequestDetails.bIsMainFrame = true; // shouldOverrideUrlLoading is only called on the main frame

				Retval = BrowserWindow->OnBeforeBrowse().Execute(Url, RequestDetails);
			}
		}
	}
	return Retval;
}

bool SAndroidWebBrowserWidget::HandleJsDialog(TSharedPtr<IWebBrowserDialog>& Dialog)
{
	bool Retval = false;
	if (WebBrowserWindowPtr.IsValid())
	{
		TSharedPtr<FAndroidWebBrowserWindow> BrowserWindow = WebBrowserWindowPtr.Pin();
		if (BrowserWindow.IsValid() && BrowserWindow->OnShowDialog().IsBound())
		{
			EWebBrowserDialogEventResponse EventResponse = BrowserWindow->OnShowDialog().Execute(TWeakPtr<IWebBrowserDialog>(Dialog));
			switch (EventResponse)
			{
			case EWebBrowserDialogEventResponse::Handled:
				Retval = true;
				break;
			case EWebBrowserDialogEventResponse::Continue:
				Dialog->Continue(true, (Dialog->GetType() == EWebBrowserDialogType::Prompt) ? Dialog->GetDefaultPrompt() : FText::GetEmpty());
				Retval = true;
				break;
			case EWebBrowserDialogEventResponse::Ignore:
				Dialog->Continue(false);
				Retval = true;
				break;
			case EWebBrowserDialogEventResponse::Unhandled:
			default:
				Retval = false;
				break;
			}
		}
	}
	return Retval;
}

void SAndroidWebBrowserWidget::HandleReceivedTitle(jstring JTitle)
{
	JNIEnv*	JEnv = FAndroidApplication::GetJavaEnv();

	const char* JTitleChars = JEnv->GetStringUTFChars(JTitle, 0);
	FString Title = UTF8_TO_TCHAR(JTitleChars);
	JEnv->ReleaseStringUTFChars(JTitle, JTitleChars);

	if (WebBrowserWindowPtr.IsValid())
	{
		TSharedPtr<FAndroidWebBrowserWindow> BrowserWindow = WebBrowserWindowPtr.Pin();
		if (BrowserWindow.IsValid())
		{
			BrowserWindow->SetTitle(Title);
		}
	}
}

void SAndroidWebBrowserWidget::HandlePageLoad(jstring JUrl, bool bIsLoading, int InHistorySize, int InHistoryPosition)
{
	HistorySize = InHistorySize;
	HistoryPosition = InHistoryPosition;

	JNIEnv*	JEnv = FAndroidApplication::GetJavaEnv();

	const char* JUrlChars = JEnv->GetStringUTFChars(JUrl, 0);
	FString Url = UTF8_TO_TCHAR(JUrlChars);
	JEnv->ReleaseStringUTFChars(JUrl, JUrlChars);

	if (WebBrowserWindowPtr.IsValid())
	{
		TSharedPtr<FAndroidWebBrowserWindow> BrowserWindow = WebBrowserWindowPtr.Pin();
		if (BrowserWindow.IsValid())
		{
			BrowserWindow->NotifyDocumentLoadingStateChange(Url, bIsLoading);
		}
	}
}

void SAndroidWebBrowserWidget::HandleReceivedError(jint ErrorCode, jstring /* ignore */, jstring JUrl)
{
	JNIEnv*	JEnv = FAndroidApplication::GetJavaEnv();

	const char* JUrlChars = JEnv->GetStringUTFChars(JUrl, 0);
	FString Url = UTF8_TO_TCHAR(JUrlChars);
	JEnv->ReleaseStringUTFChars(JUrl, JUrlChars);

	if (WebBrowserWindowPtr.IsValid())
	{
		TSharedPtr<FAndroidWebBrowserWindow> BrowserWindow = WebBrowserWindowPtr.Pin();
		if (BrowserWindow.IsValid())
		{
			BrowserWindow->NotifyDocumentError(Url, ErrorCode);
		}
	}
}

// Native method implementations:

JNI_METHOD jbyteArray Java_com_epicgames_ue4_WebViewControl_00024ViewClient_shouldInterceptRequestImpl(JNIEnv* JEnv, jobject Client, jstring JUrl)
{
	TSharedPtr<SAndroidWebBrowserWidget> Widget = SAndroidWebBrowserWidget::GetWidgetPtr(JEnv, Client);
	if (Widget.IsValid())
	{
		return Widget->HandleShouldInterceptRequest(JUrl);
	}
	else
	{
		return nullptr;
	}
}

JNI_METHOD jboolean Java_com_epicgames_ue4_WebViewControl_00024ViewClient_shouldOverrideUrlLoading(JNIEnv* JEnv, jobject Client, jobject /* ignore */, jstring JUrl)
{
	TSharedPtr<SAndroidWebBrowserWidget> Widget = SAndroidWebBrowserWidget::GetWidgetPtr(JEnv, Client);
	if (Widget.IsValid())
	{
		return Widget->HandleShouldOverrideUrlLoading(JUrl);
	}
	else
	{
		return false;
	}
}

JNI_METHOD void Java_com_epicgames_ue4_WebViewControl_00024ViewClient_onPageLoad(JNIEnv* JEnv, jobject Client, jstring JUrl, jboolean bIsLoading, jint HistorySize, jint HistoryPosition)
{
	TSharedPtr<SAndroidWebBrowserWidget> Widget = SAndroidWebBrowserWidget::GetWidgetPtr(JEnv, Client);
	if (Widget.IsValid())
	{
		Widget->HandlePageLoad(JUrl, bIsLoading, HistorySize, HistoryPosition);
	}
}

JNI_METHOD void Java_com_epicgames_ue4_WebViewControl_00024ViewClient_onReceivedError(JNIEnv* JEnv, jobject Client, jobject /* ignore */, jint ErrorCode, jstring Description, jstring JUrl)
{
	TSharedPtr<SAndroidWebBrowserWidget> Widget = SAndroidWebBrowserWidget::GetWidgetPtr(JEnv, Client);
	if (Widget.IsValid())
	{
		Widget->HandleReceivedError(ErrorCode, Description, JUrl);
	}
}

JNI_METHOD jboolean Java_com_epicgames_ue4_WebViewControl_00024ChromeClient_onJsAlert(JNIEnv* JEnv, jobject Client, jobject /* ignore */, jstring JUrl, jstring Message, jobject Result)
{
	TSharedPtr<SAndroidWebBrowserWidget> Widget = SAndroidWebBrowserWidget::GetWidgetPtr(JEnv, Client);
	if (Widget.IsValid())
	{
		return Widget->HandleJsDialog(EWebBrowserDialogType::Alert, JUrl, Message, Result);
	}
	else
	{
		return false;
	}
}

JNI_METHOD jboolean Java_com_epicgames_ue4_WebViewControl_00024ChromeClient_onJsBeforeUnload(JNIEnv* JEnv, jobject Client, jobject /* ignore */, jstring JUrl, jstring Message, jobject Result)
{
	TSharedPtr<SAndroidWebBrowserWidget> Widget = SAndroidWebBrowserWidget::GetWidgetPtr(JEnv, Client);
	if (Widget.IsValid())
	{
		return Widget->HandleJsDialog(EWebBrowserDialogType::Unload, JUrl, Message, Result);
	}
	else
	{
		return false;
	}
}

JNI_METHOD jboolean Java_com_epicgames_ue4_WebViewControl_00024ChromeClient_onJsConfirm(JNIEnv* JEnv, jobject Client, jobject /* ignore */, jstring JUrl, jstring Message, jobject Result)
{
	TSharedPtr<SAndroidWebBrowserWidget> Widget = SAndroidWebBrowserWidget::GetWidgetPtr(JEnv, Client);
	if (Widget.IsValid())
	{
		return Widget->HandleJsDialog(EWebBrowserDialogType::Confirm, JUrl, Message, Result);
	}
	else
	{
		return false;
	}
}

JNI_METHOD jboolean Java_com_epicgames_ue4_WebViewControl_00024ChromeClient_onJsPrompt(JNIEnv* JEnv, jobject Client, jobject /* ignore */, jstring JUrl, jstring Message, jstring DefaultValue, jobject Result)
{
	TSharedPtr<SAndroidWebBrowserWidget> Widget = SAndroidWebBrowserWidget::GetWidgetPtr(JEnv, Client);
	if (Widget.IsValid())
	{
		return Widget->HandleJsPrompt(JUrl, Message, DefaultValue, Result);
	}
	else
	{
		return false;
	}
}

JNI_METHOD void Java_com_epicgames_ue4_WebViewControl_00024ChromeClient_onReceivedTitle(JNIEnv* JEnv, jobject Client, jobject /* ignore */, jstring Title)
{
	TSharedPtr<SAndroidWebBrowserWidget> Widget = SAndroidWebBrowserWidget::GetWidgetPtr(JEnv, Client);
	if (Widget.IsValid())
	{
		Widget->HandleReceivedTitle(Title);
	}
}
