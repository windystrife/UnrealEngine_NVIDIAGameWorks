// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineIdentityGoogle.h"
#include "OnlineSubsystemGooglePrivate.h"

#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#include "Misc/ConfigCacheIni.h"
#include "Async/TaskGraphInterfaces.h"

FOnlineIdentityGoogle::FOnlineIdentityGoogle(FOnlineSubsystemGoogle* InSubsystem)
	: FOnlineIdentityGoogleCommon(InSubsystem)
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineIdentityGoogle::FOnlineIdentityGoogle()"));

	// Setup permission scope fields
	GConfig->GetArray(TEXT("OnlineSubsystemGoogle.OnlineIdentityGoogle"), TEXT("ScopeFields"), ScopeFields, GEngineIni);
	// always required login access fields
	ScopeFields.AddUnique(TEXT(GOOGLE_PERM_PUBLIC_PROFILE));

	FOnGoogleLoginCompleteDelegate LoginDelegate = FOnGoogleLoginCompleteDelegate::CreateRaw(this, &FOnlineIdentityGoogle::OnLoginComplete);
	OnGoogleLoginCompleteHandle = AddOnGoogleLoginCompleteDelegate_Handle(LoginDelegate);

	FOnGoogleLogoutCompleteDelegate LogoutDelegate = FOnGoogleLogoutCompleteDelegate::CreateRaw(this, &FOnlineIdentityGoogle::OnLogoutComplete);
	OnGoogleLogoutCompleteHandle = AddOnGoogleLogoutCompleteDelegate_Handle(LogoutDelegate);
}

bool FOnlineIdentityGoogle::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	UE_LOG_ONLINE(Verbose, TEXT("FOnlineIdentityGoogle::Login"));
	bool bTriggeredLogin = false;
	bool bPendingOp = LoginCompletionDelegate.IsBound() || LogoutCompletionDelegate.IsBound();
	if (!bPendingOp)
	{
		ELoginStatus::Type LoginStatus = GetLoginStatus(LocalUserNum);
		if (LoginStatus == ELoginStatus::NotLoggedIn)
		{
			LoginCompletionDelegate = FOnInternalLoginComplete::CreateLambda(
				[this, LocalUserNum](EGoogleLoginResponse InResponseCode, const FString& InAccessToken)
			{
				UE_LOG_ONLINE(Verbose, TEXT("FOnInternalLoginComplete %s %s"), ToString(InResponseCode), *InAccessToken);
				if (InResponseCode == EGoogleLoginResponse::RESPONSE_OK)
				{
					FString ErrorStr;

					TSharedPtr<FJsonObject> JsonJavaUserData;
					TSharedRef< TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(InAccessToken);

					if (FJsonSerializer::Deserialize(JsonReader, JsonJavaUserData) &&
						JsonJavaUserData.IsValid())
					{
						const TSharedPtr<FJsonObject>* AuthData = nullptr;
						if (JsonJavaUserData->TryGetObjectField(TEXT("auth_data"), AuthData))
						{
							const TSharedPtr<FJsonObject>* UserProfile = nullptr;
							if (JsonJavaUserData->TryGetObjectField(TEXT("user_data"), UserProfile))
							{
								FAuthTokenGoogle AuthToken;
								if (AuthToken.Parse(*AuthData))
								{
									TSharedRef<FUserOnlineAccountGoogle> User = MakeShared<FUserOnlineAccountGoogle>();
									if (User->Parse(AuthToken, *UserProfile))
									{
										// update/add cached entry for user
										UserAccounts.Add(User->GetUserId()->ToString(), User);
										// keep track of user ids for local users
										UserIds.Add(LocalUserNum, User->GetUserId());
									}
									else
									{
										ErrorStr = FString::Printf(TEXT("Error parsing user profile. payload=%s"), *InAccessToken);
									}
								}
								else
								{
									ErrorStr = FString::Printf(TEXT("Error parsing auth token. payload=%s"), *InAccessToken);
								}
							}
							else
							{
								ErrorStr = FString::Printf(TEXT("user_data field missing. payload=%s"), *InAccessToken);
							}
						}
						else
						{
							ErrorStr = FString::Printf(TEXT("auth_data field missing. payload=%s"), *InAccessToken);
						}
					}
					else
					{
						ErrorStr = FString::Printf(TEXT("Failed to deserialize java data. payload=%s"), *InAccessToken);
					}

					OnLoginAttemptComplete(LocalUserNum, ErrorStr);
				}
				else
				{
					FString ErrorStr;
					if (InResponseCode == EGoogleLoginResponse::RESPONSE_CANCELED)
					{
						ErrorStr = GOOGLE_AUTH_CANCELED;
					}
					else
					{
						ErrorStr = FString::Printf(TEXT("Login failure %s"), ToString(InResponseCode));
					}
					OnLoginAttemptComplete(LocalUserNum, ErrorStr);
				}
			});

			extern bool AndroidThunkCpp_Google_Login(const TArray<FString>&);
			bTriggeredLogin = AndroidThunkCpp_Google_Login(ScopeFields);
			if (!ensure(bTriggeredLogin))
			{
				// Only if JEnv is wrong
				UE_LOG_ONLINE(Verbose, TEXT("FOnlineIdentityGoogle::Login AndroidThunkCpp_Google_Login failed to trigger"));
				OnLoginComplete(EGoogleLoginResponse::RESPONSE_ERROR, TEXT(""));
			}
		}
		else
		{
			TriggerOnLoginCompleteDelegates(LocalUserNum, true, *GetUniquePlayerId(LocalUserNum), TEXT("Already logged in"));
		}
	}
	else
	{
		UE_LOG_ONLINE(Verbose, TEXT("FOnlineIdentityGoogle::Login Operation already in progress!"));
		FString ErrorStr = FString::Printf(TEXT("Operation already in progress"));
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, GetEmptyUniqueId(), ErrorStr);
	}

	return bTriggeredLogin;
}

void FOnlineIdentityGoogle::OnLoginAttemptComplete(int32 LocalUserNum, const FString& ErrorStr)
{
	const FString ErrorStrCopy(ErrorStr);

	if (GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn)
	{
		UE_LOG(LogOnline, Display, TEXT("Google login was successful."));
		TSharedPtr<const FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
		check(UserId.IsValid());

		GoogleSubsystem->ExecuteNextTick([this, UserId, LocalUserNum, ErrorStrCopy]()
		{
			TriggerOnLoginCompleteDelegates(LocalUserNum, true, *UserId, ErrorStrCopy);
			TriggerOnLoginStatusChangedDelegates(LocalUserNum, ELoginStatus::NotLoggedIn, ELoginStatus::LoggedIn, *UserId);
		});
	}
	else
	{
		LogoutCompletionDelegate = FOnInternalLogoutComplete::CreateLambda(
			[this, LocalUserNum, ErrorStrCopy](EGoogleLoginResponse InResponseCode)
		{
			UE_LOG_ONLINE(Warning, TEXT("Google login failed: %s"), *ErrorStrCopy);

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
		extern bool AndroidThunkCpp_Google_Logout();
		bool bTriggeredLogout = AndroidThunkCpp_Google_Logout();
		if (!ensure(bTriggeredLogout))
		{
			// Only if JEnv is wrong
			UE_LOG_ONLINE(Verbose, TEXT("FOnlineIdentityGoogle::OnLoginAttemptComplete AndroidThunkCpp_Google_Logout failed to trigger"));
			OnLogoutComplete(EGoogleLoginResponse::RESPONSE_ERROR);
		}
	}
}

bool FOnlineIdentityGoogle::Logout(int32 LocalUserNum)
{
	bool bTriggeredLogout = false;
	bool bPendingOp = LoginCompletionDelegate.IsBound() || LogoutCompletionDelegate.IsBound();
	if (!bPendingOp)
	{
		ELoginStatus::Type LoginStatus = GetLoginStatus(LocalUserNum);
		if (LoginStatus == ELoginStatus::LoggedIn)
		{
			LogoutCompletionDelegate = FOnInternalLogoutComplete::CreateLambda(
				[this, LocalUserNum](EGoogleLoginResponse InResponseCode)
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

				GoogleSubsystem->ExecuteNextTick([this, UserId, LocalUserNum]()
				{
					TriggerOnLogoutCompleteDelegates(LocalUserNum, true);
					TriggerOnLoginStatusChangedDelegates(LocalUserNum, ELoginStatus::LoggedIn, ELoginStatus::NotLoggedIn, *UserId);
				});
			});

			extern bool AndroidThunkCpp_Google_Logout();
			bTriggeredLogout = AndroidThunkCpp_Google_Logout();
			if (!ensure(bTriggeredLogout))
			{
				// Only if JEnv is wrong
				UE_LOG_ONLINE(Verbose, TEXT("FOnlineIdentityGoogle::Logout AndroidThunkCpp_Google_Logout failed to trigger"));
				OnLogoutComplete(EGoogleLoginResponse::RESPONSE_ERROR);
			}
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("No logged in user found for LocalUserNum=%d."), LocalUserNum);
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineIdentityGoogle::Logout - Operation already in progress"));
	}

	if (!bTriggeredLogout)
	{
		UE_LOG_ONLINE(Verbose, TEXT("FOnlineIdentityGoogle::Logout didn't trigger logout"));
		GoogleSubsystem->ExecuteNextTick([this, LocalUserNum]()
		{
			TriggerOnLogoutCompleteDelegates(LocalUserNum, false);
		});
	}

	return bTriggeredLogout;
}

void FOnlineIdentityGoogle::OnLoginComplete(EGoogleLoginResponse InResponseCode, const FString& InAccessToken)
{
	UE_LOG_ONLINE(Verbose, TEXT("OnLoginComplete %s %s"), ToString(InResponseCode), *InAccessToken);
	ensure(LoginCompletionDelegate.IsBound());
	LoginCompletionDelegate.ExecuteIfBound(InResponseCode, InAccessToken);
	LoginCompletionDelegate.Unbind();
}

void FOnlineIdentityGoogle::OnLogoutComplete(EGoogleLoginResponse InResponseCode)
{
	UE_LOG_ONLINE(Verbose, TEXT("OnLogoutComplete %s"), ToString(InResponseCode));
	ensure(LogoutCompletionDelegate.IsBound());
	LogoutCompletionDelegate.ExecuteIfBound(InResponseCode);
	LogoutCompletionDelegate.Unbind();
}


#define CHECK_JNI_METHOD(Id) checkf(Id != nullptr, TEXT("Failed to find " #Id));

bool AndroidThunkCpp_Google_Login(const TArray<FString>& InScopeFields)
{
	UE_LOG_ONLINE(Verbose, TEXT("AndroidThunkCpp_Google_Login"));
	bool bSuccess = false;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		const bool bIsOptional = false;
		static jmethodID GoogleLoginMethod = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Google_Login", "([Ljava/lang/String;)I", bIsOptional);
		CHECK_JNI_METHOD(GoogleLoginMethod);
		UE_LOG_ONLINE(Verbose, TEXT("GoogleLoginMethod 0x%08x"), GoogleLoginMethod);

		// Convert scope array into java fields
		jobjectArray ScopeIDArray = (jobjectArray)Env->NewObjectArray(InScopeFields.Num(), FJavaWrapper::JavaStringClass, nullptr);
		for (uint32 Param = 0; Param < InScopeFields.Num(); Param++)
		{
			jstring StringValue = Env->NewStringUTF(TCHAR_TO_UTF8(*InScopeFields[Param]));
			Env->SetObjectArrayElement(ScopeIDArray, Param, StringValue);
			Env->DeleteLocalRef(StringValue);
		}

		int32 ReturnVal = FJavaWrapper::CallIntMethod(Env, FJavaWrapper::GameActivityThis, GoogleLoginMethod, ScopeIDArray);

		// clean up references
		Env->DeleteLocalRef(ScopeIDArray);
		bSuccess = true;
		UE_LOG_ONLINE(Verbose, TEXT("AndroidThunkCpp_Google_Login retval=%d"), ReturnVal);
	}
	else
	{
		UE_LOG_ONLINE(Verbose, TEXT("AndroidThunkCpp_Google_Login JNI error"));
	}

	return bSuccess;
}

extern "C" void Java_com_epicgames_ue4_GoogleLogin_nativeLoginComplete(JNIEnv* jenv, jobject thiz, jsize responseCode, jstring javaData)
{
	EGoogleLoginResponse LoginResponse = (EGoogleLoginResponse)responseCode;

	const char* charsJavaData = jenv->GetStringUTFChars(javaData, 0);
	FString JavaData = FString(UTF8_TO_TCHAR(charsJavaData));
	jenv->ReleaseStringUTFChars(javaData, charsJavaData);

	UE_LOG_ONLINE(Verbose, TEXT("nativeLoginComplete Response: %s Data: %s"), ToString(LoginResponse), *JavaData);

	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.ProcessGoogleLogin"), STAT_FSimpleDelegateGraphTask_ProcessGoogleLogin, STATGROUP_TaskGraphTasks);
	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]()
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Google login completed %s"), ToString(LoginResponse));
			if (IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::Get(GOOGLE_SUBSYSTEM))
			{
				FOnlineIdentityGooglePtr IdentityGoogleInt = StaticCastSharedPtr<FOnlineIdentityGoogle>(OnlineSub->GetIdentityInterface());
				if (IdentityGoogleInt.IsValid())
				{
					IdentityGoogleInt->TriggerOnGoogleLoginCompleteDelegates(LoginResponse, JavaData);
				}
			}
		}),
	GET_STATID(STAT_FSimpleDelegateGraphTask_ProcessGoogleLogin),
	nullptr,
	ENamedThreads::GameThread
	);
}

bool AndroidThunkCpp_Google_Logout()
{
	UE_LOG_ONLINE(Verbose, TEXT("AndroidThunkCpp_Google_Logout"));
	bool bSuccess = false;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		const bool bIsOptional = false;
		static jmethodID GoogleLogoutMethod = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Google_Logout", "()I", bIsOptional);
		CHECK_JNI_METHOD(GoogleLogoutMethod);
		UE_LOG_ONLINE(Verbose, TEXT("GoogleLogoutMethod 0x%08x"), GoogleLogoutMethod);
		int32 ReturnVal = FJavaWrapper::CallIntMethod(Env, FJavaWrapper::GameActivityThis, GoogleLogoutMethod);
		
		bSuccess = true;
		UE_LOG_ONLINE(Verbose, TEXT("AndroidThunkCpp_Google_Logout retval=%d"), ReturnVal);
	}
	else
	{
		UE_LOG_ONLINE(Verbose, TEXT("AndroidThunkCpp_Google_Logout JNI error"));
	}
	return bSuccess;
}

extern "C" void Java_com_epicgames_ue4_GoogleLogin_nativeLogoutComplete(JNIEnv* jenv, jobject thiz, jsize responseCode)
{
	EGoogleLoginResponse LogoutResponse = (EGoogleLoginResponse)responseCode;
	UE_LOG_ONLINE(Verbose, TEXT("nativeLogoutComplete %s"), ToString(LogoutResponse));

	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.ProcessGoogleLogout"), STAT_FSimpleDelegateGraphTask_ProcessGoogleLogout, STATGROUP_TaskGraphTasks);
	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]()
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Google logout completed %s"), ToString(LogoutResponse));
			if (IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::Get(GOOGLE_SUBSYSTEM))
			{
				FOnlineIdentityGooglePtr IdentityGoogleInt = StaticCastSharedPtr<FOnlineIdentityGoogle>(OnlineSub->GetIdentityInterface());
				if (IdentityGoogleInt.IsValid())
				{
					IdentityGoogleInt->TriggerOnGoogleLogoutCompleteDelegates(LogoutResponse);
				}
			}
		}),
	GET_STATID(STAT_FSimpleDelegateGraphTask_ProcessGoogleLogout),
	nullptr,
	ENamedThreads::GameThread
	);
}





