// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AndroidAdjust.h"
#include "ModuleManager.h"
#include "AndroidAdjustPrivatePCH.h"
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#include "Paths.h"
#include "ConfigCacheIni.h"

DEFINE_LOG_CATEGORY_STATIC(LogAnalytics, Display, All);

IMPLEMENT_MODULE( FAnalyticsAndroidAdjust, AndroidAdjust )

TSharedPtr<IAnalyticsProvider> FAnalyticsProviderAdjust::Provider;

void FAnalyticsAndroidAdjust::StartupModule()
{
}

void FAnalyticsAndroidAdjust::ShutdownModule()
{
	FAnalyticsProviderAdjust::Destroy();
}

// Android JNI to call Adjust UPL injected methods
void AndroidThunkCpp_Adjust_SetEnabled(bool bEnable)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Adjust_SetEnabled", "(Z)V", false);
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method, bEnable);
	}
}

void AndroidThunkCpp_Adjust_SetOfflineMode(bool bOffline)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Adjust_SetOfflineMode", "(Z)V", false);
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method, bOffline);
	}
}

void AndroidThunkCpp_Adjust_SetPushToken(const FString& Token)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, " AndroidThunkJava_Adjust_SetPushToken", "(Ljava/lang/String;)V", false);
		jstring TokenJava = Env->NewStringUTF(TCHAR_TO_UTF8(*Token));
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method, TokenJava);
		Env->DeleteLocalRef(TokenJava);
	}
}

void AndroidThunkCpp_Adjust_AddSessionPartnerParameter(const FString& Key, const FString& Value)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Adjust_AddSessionPartnerParameter", "(Ljava/lang/String;Ljava/lang/String;)V", false);
		jstring KeyJava = Env->NewStringUTF(TCHAR_TO_UTF8(*Key));
		jstring ValueJava = Env->NewStringUTF(TCHAR_TO_UTF8(*Value));
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method, KeyJava, ValueJava);
		Env->DeleteLocalRef(KeyJava);
		Env->DeleteLocalRef(ValueJava);
	}
}

void AndroidThunkCpp_Adjust_RemoveSessionPartnerParameter(const FString& Key)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Adjust_RemoveSessionPartnerParameter", "(Ljava/lang/String;)V", false);
		jstring KeyJava = Env->NewStringUTF(TCHAR_TO_UTF8(*Key));
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method, KeyJava);
		Env->DeleteLocalRef(KeyJava);
	}
}

void AndroidThunkCpp_Adjust_ResetSessionPartnerParameters()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Adjust_ResetSessionPartnerParameters", "()V", false);
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method);
	}
}

void AndroidThunkCpp_Adjust_Event_AddCallbackParameter(const FString& Key, const FString& Value)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Adjust_Event_AddCallbackParameter", "(Ljava/lang/String;Ljava/lang/String;)V", false);
		jstring KeyJava = Env->NewStringUTF(TCHAR_TO_UTF8(*Key));
		jstring ValueJava = Env->NewStringUTF(TCHAR_TO_UTF8(*Value));
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method, KeyJava, ValueJava);
		Env->DeleteLocalRef(KeyJava);
		Env->DeleteLocalRef(ValueJava);
	}
}

void AndroidThunkCpp_Adjust_Event_RemoveCallbackParameter(const FString& Key)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Adjust_Event_RemoveCallbackParameter", "(Ljava/lang/String;)V", false);
		jstring KeyJava = Env->NewStringUTF(TCHAR_TO_UTF8(*Key));
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method, KeyJava);
		Env->DeleteLocalRef(KeyJava);
	}
}

void AndroidThunkCpp_Adjust_Event_ResetCallbackParameters()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Adjust_Event_ResetCallbackParameters", "()V", false);
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method);
	}
}

void AndroidThunkCpp_Adjust_Event_AddPartnerParameter(const FString& Key, const FString& Value)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Adjust_Event_AddPartnerParameter", "(Ljava/lang/String;Ljava/lang/String;)V", false);
		jstring KeyJava = Env->NewStringUTF(TCHAR_TO_UTF8(*Key));
		jstring ValueJava = Env->NewStringUTF(TCHAR_TO_UTF8(*Value));
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method, KeyJava, ValueJava);
		Env->DeleteLocalRef(KeyJava);
		Env->DeleteLocalRef(ValueJava);
	}
}

void AndroidThunkCpp_Adjust_Event_RemovePartnerParameter(const FString& Key)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Adjust_Event_RemovePartnerParameter", "(Ljava/lang/String;)V", false);
		jstring KeyJava = Env->NewStringUTF(TCHAR_TO_UTF8(*Key));
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method, KeyJava);
		Env->DeleteLocalRef(KeyJava);
	}
}

void AndroidThunkCpp_Adjust_Event_ResetPartnerParameters()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Adjust_Event_ResetPartnerParameters", "()V", false);
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method);
	}
}

void AndroidThunkCpp_Adjust_SendEvent(const FString& Token)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Adjust_SendEvent", "(Ljava/lang/String;)V", false);
		jstring TokenJava = Env->NewStringUTF(TCHAR_TO_UTF8(*Token));
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method, TokenJava);
		Env->DeleteLocalRef(TokenJava);
	}
}

void AndroidThunkCpp_Adjust_SendRevenueEvent(const FString& Token, const FString& OrderId, double Amount, const FString& Currency)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Adjust_SendRevenueEvent", "(Ljava/lang/String;Ljava/lang/String;DLjava/lang/String;)V", false);
		jstring TokenJava = Env->NewStringUTF(TCHAR_TO_UTF8(*Token));
		jstring OrderIdJava = Env->NewStringUTF(TCHAR_TO_UTF8(*OrderId));
		jstring CurrencyJava = Env->NewStringUTF(TCHAR_TO_UTF8(*Currency));
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method, TokenJava, OrderIdJava, Amount, CurrencyJava);
		Env->DeleteLocalRef(TokenJava);
		Env->DeleteLocalRef(OrderIdJava);
		Env->DeleteLocalRef(CurrencyJava);
	}
}

// End Android JNI to call Adjust UPL injected methods

TSharedPtr<IAnalyticsProvider> FAnalyticsAndroidAdjust::CreateAnalyticsProvider(const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const
{
	if (GetConfigValue.IsBound())
	{
		const FString InAppToken = GetConfigValue.Execute(TEXT("AdjustAppToken"), true);
		return FAnalyticsProviderAdjust::Create(InAppToken);
	}
	else
	{
		UE_LOG(LogAnalytics, Warning, TEXT("AndroidAdjust::CreateAnalyticsProvider called with an unbound delegate"));
	}
	return nullptr;
}

// Provider

FAnalyticsProviderAdjust::FAnalyticsProviderAdjust(const FString inAppToken) :
	AppToken(inAppToken)
{
#if WITH_ADJUST
	// NOTE: currently expect these events to have been added!
	// SessionAttributes
	// Item Purchase
	// Currency Purchase
	// Currency Given
	// Error
	// Progress

	// add event attributes from ini
	FString IniName = FString::Printf(TEXT("%sDefaultEngine.ini"), *FPaths::SourceConfigDir());
	EventMap.Empty();
	TArray<FString> EventNames;
	TArray<FString> EventTokens;
	int NameCount = GConfig->GetArray(TEXT("AdjustAnalyticsEventMapping"), TEXT("EventNames"), EventNames, IniName);
	int TokenCount = GConfig->GetArray(TEXT("AdjustAnalyticsEventMapping"), TEXT("EventTokens"), EventTokens, IniName);
	int Count = NameCount <= TokenCount ? NameCount : TokenCount;
	for (int Index = 0; Index < Count; ++Index)
	{
		EventMap.Add(EventNames[Index], EventTokens[Index]);
	}

	#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_ADJUST=0. Are you missing the SDK?"));
#endif
}

FAnalyticsProviderAdjust::~FAnalyticsProviderAdjust()
{
	if (bHasSessionStarted)
	{
		EndSession();
	}
}

bool FAnalyticsProviderAdjust::StartSession(const TArray<FAnalyticsEventAttribute>& Attributes)
{
#if WITH_ADJUST
	const int32 AttrCount = Attributes.Num();

	// add session attributes (this will be on all events)

	for (auto Attr : Attributes)
	{
		AndroidThunkCpp_Adjust_AddSessionPartnerParameter(Attr.AttrName, Attr.ToString());
	}
	RecordEvent(TEXT("SessionAttributes"), Attributes);

	if (!bHasSessionStarted)
	{
		UE_LOG(LogAnalytics, Display, TEXT("AndroidAdjust::StartSession(%d attributes)"), AttrCount);
	}
	else
	{
		UE_LOG(LogAnalytics, Display, TEXT("AndroidAdjust::RestartSession(%d attributes)"), AttrCount);
	}
	bHasSessionStarted = true;
	return bHasSessionStarted;
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_ADJUST=0. Are you missing the SDK?"));
	return false;
#endif
}

void FAnalyticsProviderAdjust::EndSession()
{
#if WITH_ADJUST
	bHasSessionStarted = false;

	UE_LOG(LogAnalytics, Display, TEXT("AndroidAdjust::EndSession"));
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_ADJUST=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderAdjust::FlushEvents()
{
#if WITH_ADJUST
	UE_LOG(LogAnalytics, Display, TEXT("AndroidAdjust::FlushEvents"));
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_ADJUST=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderAdjust::SetUserID(const FString& InUserID)
{
#if WITH_ADJUST
	UserId = InUserID;
	UE_LOG(LogAnalytics, Display, TEXT("AndroidAdjust::SetUserID(%s)"), *UserId);
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_ADJUST=0. Are you missing the SDK?"));
#endif
}

FString FAnalyticsProviderAdjust::GetUserID() const
{
#if WITH_ADJUST
	UE_LOG(LogAnalytics, Display, TEXT("AndroidAdjust::GetUserID - returning cached id '%s'"), *UserId);

	return UserId;
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_ADJUST=0. Are you missing the SDK?"));
	return FString();
#endif
}

FString FAnalyticsProviderAdjust::GetSessionID() const
{
#if WITH_ADJUST
	FString Id = TEXT("unavailable");

	UE_LOG(LogAnalytics, Display, TEXT("AndroidAdjust::GetSessionID - returning the id as '%s'"), *Id);

	return Id;
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_ADJUST=0. Are you missing the SDK?"));
	return FString();
#endif
}

bool FAnalyticsProviderAdjust::SetSessionID(const FString& InSessionID)
{
#if WITH_ADJUST
	// Ignored
	UE_LOG(LogAnalytics, Display, TEXT("AndroidAdjust::SetSessionID - ignoring call"));
	return false;
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_ADJUST=0. Are you missing the SDK?"));
	return false;
#endif
}

void FAnalyticsProviderAdjust::RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
#if WITH_ADJUST
	FString* EventTokenRef = EventMap.Find(EventName);
	if (EventTokenRef != nullptr)
	{
		FString EventToken = *EventTokenRef;

		AndroidThunkCpp_Adjust_Event_ResetCallbackParameters();
		AndroidThunkCpp_Adjust_Event_ResetPartnerParameters();

		const int32 AttrCount = Attributes.Num();
		if (AttrCount > 0)
		{
			// add event attributes
			for (auto Attr : Attributes)
			{
				AndroidThunkCpp_Adjust_Event_AddCallbackParameter(Attr.AttrName, Attr.ToString());
			}
		}
		AndroidThunkCpp_Adjust_SendEvent(EventToken);
		UE_LOG(LogAnalytics, Display, TEXT("AndroidAdjust::RecordEvent('%s', %d attributes) Token=%s"), *EventName, AttrCount, *EventToken);
	}
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_ADJUST=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderAdjust::RecordItemPurchase(const FString& ItemId, const FString& Currency, int PerItemCost, int ItemQuantity)
{
#if WITH_ADJUST
	FString* EventTokenRef = EventMap.Find(TEXT("Item Purchase"));
	if (EventTokenRef != nullptr)
	{
		FString EventToken = *EventTokenRef;

		AndroidThunkCpp_Adjust_Event_ResetPartnerParameters();

		AndroidThunkCpp_Adjust_Event_ResetCallbackParameters();
		AndroidThunkCpp_Adjust_Event_AddCallbackParameter(TEXT("ItemId"), ItemId);
		AndroidThunkCpp_Adjust_Event_AddCallbackParameter(TEXT("Currency"), Currency);
		AndroidThunkCpp_Adjust_Event_AddCallbackParameter(TEXT("PerItemCost"), FString::FromInt(PerItemCost));
		AndroidThunkCpp_Adjust_Event_AddCallbackParameter(TEXT("ItemQuantity"), FString::FromInt(ItemQuantity));
	
		// @TODO: This is probably wrong.. might just want to do a normal event and forget about revenue / order id (note: input is in cents so divide by 100)
		AndroidThunkCpp_Adjust_SendRevenueEvent(EventToken, ItemId, PerItemCost * ItemQuantity * 0.01, Currency);

		UE_LOG(LogAnalytics, Display, TEXT("AndroidAdjust::RecordItemPurchase('%s', '%s', %d, %d) Token=%s"), *ItemId, *Currency, PerItemCost, ItemQuantity, *EventToken);
	}
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_ADJUST=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderAdjust::RecordCurrencyPurchase(const FString& GameCurrencyType, int GameCurrencyAmount, const FString& RealCurrencyType, float RealMoneyCost, const FString& PaymentProvider)
{
#if WITH_ADJUST
	FString* EventTokenRef = EventMap.Find(TEXT("Currency Purchase"));
	if (EventTokenRef != nullptr)
	{
		FString EventToken = *EventTokenRef;

		AndroidThunkCpp_Adjust_Event_ResetPartnerParameters();

		AndroidThunkCpp_Adjust_Event_ResetCallbackParameters();
		AndroidThunkCpp_Adjust_Event_AddCallbackParameter(TEXT("GameCurrencyType"), GameCurrencyType);
		AndroidThunkCpp_Adjust_Event_AddCallbackParameter(TEXT("GameCurrencyAmount"), FString::FromInt(GameCurrencyAmount));
		AndroidThunkCpp_Adjust_Event_AddCallbackParameter(TEXT("RealCurrencyType"), RealCurrencyType);
		AndroidThunkCpp_Adjust_Event_AddCallbackParameter(TEXT("RealMoneyCost"), FString::Printf(TEXT("%.02f"), RealMoneyCost));
		AndroidThunkCpp_Adjust_Event_AddCallbackParameter(TEXT("PaymentProvider"), PaymentProvider);

		// @TODO: This is probably wrong.. might just want to do a normal event and forget about revenue / order id
		AndroidThunkCpp_Adjust_SendRevenueEvent(EventToken, GameCurrencyType, RealMoneyCost, RealCurrencyType);

		UE_LOG(LogAnalytics, Display, TEXT("AndroidAdjust::RecordCurrencyPurchase('%s', %d, '%s', %.02f, %s) Token=%s"), *GameCurrencyType, GameCurrencyAmount, *RealCurrencyType, RealMoneyCost, *PaymentProvider, *EventToken);
	}
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_ADJUST=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderAdjust::RecordCurrencyGiven(const FString& GameCurrencyType, int GameCurrencyAmount)
{
#if WITH_ADJUST
	FString* EventTokenRef = EventMap.Find(TEXT("Currency Given"));
	if (EventTokenRef != nullptr)
	{
		FString EventToken = *EventTokenRef;

		AndroidThunkCpp_Adjust_Event_ResetPartnerParameters();

		AndroidThunkCpp_Adjust_Event_ResetCallbackParameters();
		AndroidThunkCpp_Adjust_Event_AddCallbackParameter(TEXT("GameCurrencyType"), GameCurrencyType);
		AndroidThunkCpp_Adjust_Event_AddCallbackParameter(TEXT("GameCurrencyAmount"), FString::FromInt(GameCurrencyAmount));

		AndroidThunkCpp_Adjust_SendEvent(EventToken);

		UE_LOG(LogAnalytics, Display, TEXT("AndroidAdjust::RecordCurrencyGiven('%s', %d) Token=%s"), *GameCurrencyType, GameCurrencyAmount, *EventToken);
	}
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_ADJUST=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderAdjust::RecordError(const FString& Error, const TArray<FAnalyticsEventAttribute>& EventAttrs)
{
#if WITH_ADJUST
	FString* EventTokenRef = EventMap.Find(TEXT("Error"));
	if (EventTokenRef != nullptr)
	{
		FString EventToken = *EventTokenRef;

		AndroidThunkCpp_Adjust_Event_ResetCallbackParameters();
		AndroidThunkCpp_Adjust_Event_ResetPartnerParameters();

		const int32 AttrCount = EventAttrs.Num();
		if (AttrCount > 0)
		{
			// add event attributes
			for (auto Attr : EventAttrs)
			{
				AndroidThunkCpp_Adjust_Event_AddCallbackParameter(Attr.AttrName, Attr.ToString());
			}
		}

		AndroidThunkCpp_Adjust_SendEvent(EventToken);

		UE_LOG(LogAnalytics, Display, TEXT("AndroidAdjust::RecordError('%s', %d) Token=%s"), *Error, AttrCount, *EventToken);
	}
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_ADJUST=0. Are you missing the SDK?"));
#endif
}

void FAnalyticsProviderAdjust::RecordProgress(const FString& ProgressType, const FString& ProgressHierarchy, const TArray<FAnalyticsEventAttribute>& EventAttrs)
{
#if WITH_ADJUST
	FString* EventTokenRef = EventMap.Find(TEXT("Progress"));
	if (EventTokenRef != nullptr)
	{
		FString EventToken = *EventTokenRef;

		AndroidThunkCpp_Adjust_Event_ResetCallbackParameters();
		AndroidThunkCpp_Adjust_Event_ResetPartnerParameters();

		AndroidThunkCpp_Adjust_Event_AddCallbackParameter(TEXT("ProgressType"), ProgressType);
		AndroidThunkCpp_Adjust_Event_AddCallbackParameter(TEXT("ProgressHierarchy"), ProgressHierarchy);

		const int32 AttrCount = EventAttrs.Num();
		if (AttrCount > 0)
		{
			// add event attributes
			for (auto Attr : EventAttrs)
			{
				AndroidThunkCpp_Adjust_Event_AddCallbackParameter(Attr.AttrName, Attr.ToString());
			}
		}

		AndroidThunkCpp_Adjust_SendEvent(EventToken);

		UE_LOG(LogAnalytics, Display, TEXT("AndroidAdjust::RecordProgress('%s', %s, %d) Token=%s"), *ProgressType, *ProgressHierarchy, AttrCount, *EventToken);
	}
#else
	UE_LOG(LogAnalytics, Warning, TEXT("WITH_ADJUST=0. Are you missing the SDK?"));
#endif
}
