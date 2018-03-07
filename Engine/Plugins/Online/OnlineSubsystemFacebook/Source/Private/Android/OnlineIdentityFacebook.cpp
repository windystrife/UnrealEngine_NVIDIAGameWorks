// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineIdentityFacebook.h"
#include "OnlineSubsystemFacebookPrivate.h"

#if WITH_FACEBOOK

#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#include "Misc/ConfigCacheIni.h"
#include "Async/TaskGraphInterfaces.h"

FOnlineIdentityFacebook::FOnlineIdentityFacebook(FOnlineSubsystemFacebook* InSubsystem)
	: FOnlineIdentityFacebookCommon(InSubsystem)
{
	// Setup permission scope fields
	GConfig->GetArray(TEXT("OnlineSubsystemFacebook.OnlineIdentityFacebook"), TEXT("ScopeFields"), ScopeFields, GEngineIni);
	// always required login access fields
	ScopeFields.AddUnique(TEXT(PERM_PUBLIC_PROFILE));

	FOnFacebookLoginCompleteDelegate LoginDelegate = FOnFacebookLoginCompleteDelegate::CreateRaw(this, &FOnlineIdentityFacebook::OnLoginComplete);
	OnFBLoginCompleteHandle = AddOnFacebookLoginCompleteDelegate_Handle(LoginDelegate);

	FOnFacebookLogoutCompleteDelegate LogoutDelegate = FOnFacebookLogoutCompleteDelegate::CreateRaw(this, &FOnlineIdentityFacebook::OnLogoutComplete);
	OnFBLogoutCompleteHandle = AddOnFacebookLogoutCompleteDelegate_Handle(LogoutDelegate);
}

bool FOnlineIdentityFacebook::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	bool bTriggeredLogin = false;
	bool bPendingOp = LoginCompletionDelegate.IsBound() || LogoutCompletionDelegate.IsBound();
	if (!bPendingOp)
	{
		ELoginStatus::Type LoginStatus = GetLoginStatus(LocalUserNum);
		if (LoginStatus == ELoginStatus::NotLoggedIn)
		{
			LoginCompletionDelegate = FOnInternalLoginComplete::CreateLambda(
				[this, LocalUserNum](EFacebookLoginResponse InResponseCode, const FString& InAccessToken)
			{
				UE_LOG_ONLINE(Verbose, TEXT("FOnInternalLoginComplete %s %s"), ToString(InResponseCode), *InAccessToken);
				if (InResponseCode == EFacebookLoginResponse::RESPONSE_OK &&
					!InAccessToken.IsEmpty())
				{
					Login(LocalUserNum, InAccessToken);
				}
				else
				{
					FString ErrorStr;
					if (InResponseCode == EFacebookLoginResponse::RESPONSE_CANCELED)
					{
						ErrorStr = FB_AUTH_CANCELED;
					}
					else
					{
						ErrorStr = FString::Printf(TEXT("Login failure %s"), ToString(InResponseCode));
					}
					OnLoginAttemptComplete(LocalUserNum, ErrorStr);
				}
			});

			extern bool AndroidThunkCpp_Facebook_Login(const TArray<FString>&);
			bTriggeredLogin = AndroidThunkCpp_Facebook_Login(ScopeFields);
			if (!ensure(bTriggeredLogin))
			{
				// Only if JEnv is wrong
				OnLoginComplete(EFacebookLoginResponse::RESPONSE_ERROR, TEXT(""));
			}
		}
		else
		{
			TriggerOnLoginCompleteDelegates(LocalUserNum, true, *GetUniquePlayerId(LocalUserNum), TEXT("Already logged in"));
		}
	}
	else
	{
		FString ErrorStr = FString::Printf(TEXT("Operation already in progress"));
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, GetEmptyUniqueId(), ErrorStr);
	}

	return bTriggeredLogin;
}

void FOnlineIdentityFacebook::Login(int32 LocalUserNum, const FString& AccessToken)
{
	FOnProfileRequestComplete CompletionDelegate = FOnProfileRequestComplete::CreateLambda([this](int32 LocalUserNumFromRequest, bool bWasProfileRequestSuccessful, const FString& ErrorStr)
	{
		FOnRequestCurrentPermissionsComplete NextCompletionDelegate = FOnRequestCurrentPermissionsComplete::CreateLambda([this](int32 LocalUserNumFromPerms, bool bWerePermsSuccessful, const TArray<FSharingPermission>& Permissions)
		{
			OnRequestCurrentPermissionsComplete(LocalUserNumFromPerms, bWerePermsSuccessful, Permissions);
		});

		if (bWasProfileRequestSuccessful)
		{
			RequestCurrentPermissions(LocalUserNumFromRequest, NextCompletionDelegate);
		}
		else
		{
			OnLoginAttemptComplete(LocalUserNumFromRequest, ErrorStr);
		}
	});

	ProfileRequest(LocalUserNum, AccessToken, ProfileFields, CompletionDelegate);
}

void FOnlineIdentityFacebook::OnRequestCurrentPermissionsComplete(int32 LocalUserNum, bool bWasSuccessful, const TArray<FSharingPermission>& NewPermissions)
{
	FString ErrorStr;
	if (!bWasSuccessful)
	{
		ErrorStr = TEXT("Failure to request current sharing permissions");
	}

	OnLoginAttemptComplete(LocalUserNum, ErrorStr);
}

void FOnlineIdentityFacebook::OnLoginAttemptComplete(int32 LocalUserNum, const FString& ErrorStr)
{
	const FString ErrorStrCopy(ErrorStr);

	if (GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn)
	{
		extern FString AndroidThunkCpp_Facebook_GetAccessToken();
		UE_LOG(LogOnline, Display, TEXT("Facebook login was successful %s"), *AndroidThunkCpp_Facebook_GetAccessToken());
		TSharedPtr<const FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
		check(UserId.IsValid());
		
		FacebookSubsystem->ExecuteNextTick([this, UserId, LocalUserNum, ErrorStrCopy]()
		{
			TriggerOnLoginCompleteDelegates(LocalUserNum, true, *UserId, ErrorStrCopy);
			TriggerOnLoginStatusChangedDelegates(LocalUserNum, ELoginStatus::NotLoggedIn, ELoginStatus::LoggedIn, *UserId);
		});
	}
	else
	{
		LogoutCompletionDelegate = FOnInternalLogoutComplete::CreateLambda(
			[this, LocalUserNum, ErrorStrCopy](EFacebookLoginResponse InResponseCode)
		{
			UE_LOG_ONLINE(Warning, TEXT("Facebook login failed: %s"), *ErrorStrCopy);

			TSharedPtr<const FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
			if (UserId.IsValid())
			{
				// remove cached user account
				UserAccounts.Remove(UserId->ToString());
			}
			else
			{
				UserId = GetEmptyUniqueId().AsShared();
			}
			// remove cached user id
			UserIds.Remove(LocalUserNum);

			TriggerOnLoginCompleteDelegates(LocalUserNum, false, *UserId, ErrorStrCopy);
		});

		// Clean up anything left behind from cached access tokens
		extern bool AndroidThunkCpp_Facebook_Logout();
		if (!ensure(AndroidThunkCpp_Facebook_Logout()))
		{
			// Only if JEnv is wrong
			OnLogoutComplete(EFacebookLoginResponse::RESPONSE_ERROR);
		}
	}
}

bool FOnlineIdentityFacebook::Logout(int32 LocalUserNum)
{
	bool bTriggeredLogout = false;
	bool bPendingOp = LoginCompletionDelegate.IsBound() || LogoutCompletionDelegate.IsBound();
	if (!bPendingOp)
	{
		ELoginStatus::Type LoginStatus = GetLoginStatus(LocalUserNum);
		if (LoginStatus == ELoginStatus::LoggedIn)
		{
			LogoutCompletionDelegate = FOnInternalLogoutComplete::CreateLambda(
				[this, LocalUserNum](EFacebookLoginResponse InResponseCode)
			{
				UE_LOG_ONLINE(Verbose, TEXT("FOnInternalLogoutComplete %s"), ToString(InResponseCode));
				TSharedPtr<const FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
				if (UserId.IsValid())
				{
					// remove cached user account
					UserAccounts.Remove(UserId->ToString());
				}
				else
				{
					UserId = GetEmptyUniqueId().AsShared();
				}
				// remove cached user id
				UserIds.Remove(LocalUserNum);

				FacebookSubsystem->ExecuteNextTick([this, UserId, LocalUserNum]()
				{
					TriggerOnLogoutCompleteDelegates(LocalUserNum, true);
					TriggerOnLoginStatusChangedDelegates(LocalUserNum, ELoginStatus::LoggedIn, ELoginStatus::NotLoggedIn, *UserId);
				});
			});

			extern bool AndroidThunkCpp_Facebook_Logout();
			bTriggeredLogout = AndroidThunkCpp_Facebook_Logout();
			if (!ensure(bTriggeredLogout))
			{
				// Only if JEnv is wrong
				OnLogoutComplete(EFacebookLoginResponse::RESPONSE_ERROR);
			}
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("No logged in user found for LocalUserNum=%d."), LocalUserNum);
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("Operation already in progress"));
	}

	if (!bTriggeredLogout)
	{
		FacebookSubsystem->ExecuteNextTick([this, LocalUserNum]()
		{
			TriggerOnLogoutCompleteDelegates(LocalUserNum, false);
		});
	}

	return bTriggeredLogout;
}

void FOnlineIdentityFacebook::OnLoginComplete(EFacebookLoginResponse InResponseCode, const FString& InAccessToken)
{
	UE_LOG_ONLINE(Verbose, TEXT("OnLoginComplete %s %s"), ToString(InResponseCode), *InAccessToken);
	ensure(LoginCompletionDelegate.IsBound());
	LoginCompletionDelegate.ExecuteIfBound(InResponseCode, InAccessToken);
	LoginCompletionDelegate.Unbind();
}

void FOnlineIdentityFacebook::OnLogoutComplete(EFacebookLoginResponse InResponseCode)
{
	UE_LOG_ONLINE(Verbose, TEXT("OnLogoutComplete %s"), ToString(InResponseCode));
	ensure(LogoutCompletionDelegate.IsBound());
	LogoutCompletionDelegate.ExecuteIfBound(InResponseCode);
	LogoutCompletionDelegate.Unbind();
}

#define CHECK_JNI_METHOD(Id) checkf(Id != nullptr, TEXT("Failed to find " #Id));

FString AndroidThunkCpp_Facebook_GetAccessToken()
{
	UE_LOG_ONLINE(Verbose, TEXT("AndroidThunkCpp_Facebook_GetAccessToken"));

	FString AccessToken;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		const bool bIsOptional = false;
		static jmethodID FacebookGetAccessTokenMethod = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Facebook_GetAccessToken", "()Ljava/lang/String;", bIsOptional);
		CHECK_JNI_METHOD(FacebookGetAccessTokenMethod);

		jstring accessToken = (jstring)FJavaWrapper::CallObjectMethod(Env, FJavaWrapper::GameActivityThis, FacebookGetAccessTokenMethod);

		const char* charsAccessToken = Env->GetStringUTFChars(accessToken, 0);
		AccessToken = FString(UTF8_TO_TCHAR(charsAccessToken));
		Env->ReleaseStringUTFChars(accessToken, charsAccessToken);
		Env->DeleteLocalRef(accessToken);
	}
	
	return AccessToken;
}

bool AndroidThunkCpp_Facebook_Login(const TArray<FString>& InScopeFields)
{
	UE_LOG_ONLINE(Verbose, TEXT("AndroidThunkCpp_Facebook_Login"));
	bool bSuccess = false;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		const bool bIsOptional = false;
		static jmethodID FacebookLoginMethod = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Facebook_Login", "([Ljava/lang/String;)V", bIsOptional);
		CHECK_JNI_METHOD(FacebookLoginMethod);

		// Convert scope array into java fields
		jobjectArray ScopeIDArray = (jobjectArray)Env->NewObjectArray(InScopeFields.Num(), FJavaWrapper::JavaStringClass, nullptr);
		for (uint32 Param = 0; Param < InScopeFields.Num(); Param++)
		{
			jstring StringValue = Env->NewStringUTF(TCHAR_TO_UTF8(*InScopeFields[Param]));
			Env->SetObjectArrayElement(ScopeIDArray, Param, StringValue);
			Env->DeleteLocalRef(StringValue);
		}

		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FacebookLoginMethod, ScopeIDArray);

		// clean up references
		Env->DeleteLocalRef(ScopeIDArray);
		bSuccess = true;
	}

	return bSuccess;
}

JNI_METHOD void Java_com_epicgames_ue4_FacebookLogin_nativeLoginComplete(JNIEnv* jenv, jobject thiz, jsize responseCode, jstring accessToken)
{
	EFacebookLoginResponse LoginResponse = (EFacebookLoginResponse)responseCode;

	const char* charsAccessToken = jenv->GetStringUTFChars(accessToken, 0);
	FString AccessToken = FString(UTF8_TO_TCHAR(charsAccessToken));
	jenv->ReleaseStringUTFChars(accessToken, charsAccessToken);

	UE_LOG_ONLINE(VeryVerbose, TEXT("nativeLoginComplete Response: %d Token: %s"), LoginResponse, *AccessToken);

	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.ProcessFacebookLogin"), STAT_FSimpleDelegateGraphTask_ProcessFacebookLogin, STATGROUP_TaskGraphTasks);
	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]()
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Facebook login completed %s\n"), ToString(LoginResponse));
			if (IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::Get(FACEBOOK_SUBSYSTEM))
			{
				FOnlineIdentityFacebookPtr IdentityFBInt = StaticCastSharedPtr<FOnlineIdentityFacebook>(OnlineSub->GetIdentityInterface());
				if (IdentityFBInt.IsValid())
				{
					IdentityFBInt->TriggerOnFacebookLoginCompleteDelegates(LoginResponse, AccessToken);
				}
			}
		}),
	GET_STATID(STAT_FSimpleDelegateGraphTask_ProcessFacebookLogin),
	nullptr,
	ENamedThreads::GameThread
	);
}

bool AndroidThunkCpp_Facebook_Logout()
{
	UE_LOG_ONLINE(Verbose, TEXT("AndroidThunkCpp_Facebook_Logout"));
	bool bSuccess = false;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		const bool bIsOptional = false;
		static jmethodID FacebookLogoutMethod = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Facebook_Logout", "()V", bIsOptional);
		CHECK_JNI_METHOD(FacebookLogoutMethod);
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FacebookLogoutMethod);
		bSuccess = true;
	}
	
	return bSuccess;
}

JNI_METHOD void Java_com_epicgames_ue4_FacebookLogin_nativeLogoutComplete(JNIEnv* jenv, jobject thiz, jsize responseCode)
{
	EFacebookLoginResponse LogoutResponse = (EFacebookLoginResponse)responseCode;
	UE_LOG_ONLINE(VeryVerbose, TEXT("nativeLogoutComplete %s"), ToString(LogoutResponse));

	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.ProcessFacebookLogout"), STAT_FSimpleDelegateGraphTask_ProcessFacebookLogout, STATGROUP_TaskGraphTasks);
	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]()
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Facebook logout completed %s\n"), ToString(LogoutResponse));
			if (IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::Get(FACEBOOK_SUBSYSTEM))
			{
				FOnlineIdentityFacebookPtr IdentityFBInt = StaticCastSharedPtr<FOnlineIdentityFacebook>(OnlineSub->GetIdentityInterface());
				if (IdentityFBInt.IsValid())
				{
					IdentityFBInt->TriggerOnFacebookLogoutCompleteDelegates(LogoutResponse);
				}
			}
		}),
	GET_STATID(STAT_FSimpleDelegateGraphTask_ProcessFacebookLogout),
	nullptr,
	ENamedThreads::GameThread
	);
}

#endif // WITH_FACEBOOK



