// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// Module includes
#include "OnlineSharingFacebook.h"
#include "OnlineSubsystemFacebookPrivate.h"
#include "OnlineIdentityFacebook.h"

#if WITH_FACEBOOK

#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#include "Async/TaskGraphInterfaces.h"

FOnlineSharingFacebook::FOnlineSharingFacebook(FOnlineSubsystemFacebook* InSubsystem)
	: FOnlineSharingFacebookCommon(InSubsystem)
{
	FOnFacebookRequestPermissionsOpCompleteDelegate RequestPermissionsOpDelegate = FOnFacebookRequestPermissionsOpCompleteDelegate::CreateRaw(this, &FOnlineSharingFacebook::OnPermissionsOpComplete);
	OnFBRequestPermissionsOpCompleteHandle = AddOnFacebookRequestPermissionsOpCompleteDelegate_Handle(RequestPermissionsOpDelegate);
}

FOnlineSharingFacebook::~FOnlineSharingFacebook()
{
}

bool FOnlineSharingFacebook::RequestNewReadPermissions(int32 LocalUserNum, EOnlineSharingCategory NewPermissions)
{
	ensure((NewPermissions & ~EOnlineSharingCategory::ReadPermissionMask) == EOnlineSharingCategory::None);

	bool bTriggeredRequest = false;
	bool bPendingOp = PermissionsOpCompletionDelegate.IsBound();
	if (!bPendingOp)
	{
		IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
		if (IdentityInt.IsValid() && IdentityInt->GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn)
		{
			// Here we iterate over each category, adding each individual permission linked with it in the ::SetupPermissionMaps
			TArray<FSharingPermission> PermissionsNeeded;
			const bool bHasPermission = CurrentPermissions.HasPermission(NewPermissions, PermissionsNeeded);
			if (!bHasPermission)
			{
				PermissionsOpCompletionDelegate = FOnPermissionsOpComplete::CreateLambda([this, LocalUserNum](EFacebookLoginResponse InResponseCode, const FString& InAccessToken)
				{
					UE_LOG(LogOnline, Display, TEXT("RequestNewReadPermissions : %s"), ToString(InResponseCode));
					if (InResponseCode == EFacebookLoginResponse::RESPONSE_OK)
					{
						FOnRequestCurrentPermissionsComplete PermsDelegate = FOnRequestCurrentPermissionsComplete::CreateLambda([this](int32 InLocalUserNum, bool bWasSuccessful, const TArray<FSharingPermission>& Permissions)
						{
							TriggerOnRequestNewReadPermissionsCompleteDelegates(InLocalUserNum, bWasSuccessful);
						}); 
						RequestCurrentPermissions(LocalUserNum, PermsDelegate);
					}
					else
					{
						TriggerOnRequestNewReadPermissionsCompleteDelegates(LocalUserNum, false);
					}
				});

				extern bool AndroidThunkCpp_Facebook_RequestReadPermissions(const TArray<FSharingPermission>&);
				bTriggeredRequest = AndroidThunkCpp_Facebook_RequestReadPermissions(PermissionsNeeded);
				if (!ensure(bTriggeredRequest))
				{
					// Only if JEnv is wrong
					OnPermissionsOpComplete(EFacebookLoginResponse::RESPONSE_ERROR, TEXT(""));
				}
			}
			else
			{
				// All permissions were already granted, no need to reauthorize
				TriggerOnRequestNewReadPermissionsCompleteDelegates(LocalUserNum, true);
			}
		}
		else
		{
			// If we weren't logged into Facebook we cannot do this action
			TriggerOnRequestNewReadPermissionsCompleteDelegates(LocalUserNum, false);
		}
	}
	else
	{
		UE_LOG_ONLINE(Verbose, TEXT("Operation already in progress"));
		TriggerOnRequestNewReadPermissionsCompleteDelegates(LocalUserNum, false);
	}

	return bTriggeredRequest;
}

bool FOnlineSharingFacebook::RequestNewPublishPermissions(int32 LocalUserNum, EOnlineSharingCategory NewPermissions, EOnlineStatusUpdatePrivacy Privacy)
{
	ensure((NewPermissions & ~EOnlineSharingCategory::PublishPermissionMask) == EOnlineSharingCategory::None);

	bool bTriggeredRequest = false;
	bool bPendingOp = PermissionsOpCompletionDelegate.IsBound();
	if (!bPendingOp)
	{
		IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
		if (IdentityInt.IsValid() && IdentityInt->GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn)
		{
			// Here we iterate over each category, adding each individual permission linked with it in the ::SetupPermissionMaps
			TArray<FSharingPermission> PermissionsNeeded;
			const bool bHasPermission = CurrentPermissions.HasPermission(NewPermissions, PermissionsNeeded);
			if (!bHasPermission)
			{
				if (false)
				{
					switch (Privacy)
					{
						case EOnlineStatusUpdatePrivacy::OnlyMe:
							break;
						case EOnlineStatusUpdatePrivacy::OnlyFriends:
							break;
						case EOnlineStatusUpdatePrivacy::Everyone:
							break;
					}
				}

				PermissionsOpCompletionDelegate = FOnPermissionsOpComplete::CreateLambda([this, LocalUserNum](EFacebookLoginResponse InResponseCode, const FString& InAccessToken)
				{
					UE_LOG(LogOnline, Display, TEXT("RequestNewPublishPermissions : %s"), ToString(InResponseCode));
					if (InResponseCode == EFacebookLoginResponse::RESPONSE_OK)
					{
						FOnRequestCurrentPermissionsComplete PermsDelegate = FOnRequestCurrentPermissionsComplete::CreateLambda([this](int32 InLocalUserNum, bool bWasSuccessful, const TArray<FSharingPermission>& Permissions)
						{
							TriggerOnRequestNewPublishPermissionsCompleteDelegates(InLocalUserNum, bWasSuccessful);
						});
						RequestCurrentPermissions(LocalUserNum, PermsDelegate);
					}
					else
					{
						TriggerOnRequestNewPublishPermissionsCompleteDelegates(LocalUserNum, false);
					}
				});

				extern bool AndroidThunkCpp_Facebook_RequestPublishPermissions(const TArray<FSharingPermission>&);
				bTriggeredRequest = AndroidThunkCpp_Facebook_RequestPublishPermissions(PermissionsNeeded);
				if (!ensure(bTriggeredRequest))
				{
					// Only if JEnv is wrong
					OnPermissionsOpComplete(EFacebookLoginResponse::RESPONSE_ERROR, TEXT(""));
				}
			}
			else
			{
				// All permissions were already granted, no need to reauthorize
				TriggerOnRequestNewPublishPermissionsCompleteDelegates(LocalUserNum, true);
			}
		}
		else
		{
			// If we weren't logged into Facebook we cannot do this action
			TriggerOnRequestNewPublishPermissionsCompleteDelegates(LocalUserNum, false);
		}
	}
	else
	{
		UE_LOG_ONLINE(Verbose, TEXT("Operation already in progress"));
		TriggerOnRequestNewPublishPermissionsCompleteDelegates(LocalUserNum, false);
	}

	return bTriggeredRequest;
}

void FOnlineSharingFacebook::OnPermissionsOpComplete(EFacebookLoginResponse InResponseCode, const FString& InAccessToken)
{
	UE_LOG_ONLINE(Verbose, TEXT("OnPermissionsOpComplete %s %s"), ToString(InResponseCode), *InAccessToken);
	ensure(PermissionsOpCompletionDelegate.IsBound());
	PermissionsOpCompletionDelegate.ExecuteIfBound(InResponseCode, InAccessToken);
	PermissionsOpCompletionDelegate.Unbind();
}

#define CHECK_JNI_METHOD(Id) checkf(Id != nullptr, TEXT("Failed to find " #Id));

bool AndroidThunkCpp_Facebook_RequestReadPermissions(const TArray<FSharingPermission>& InNewPermissions)
{
	UE_LOG_ONLINE(Verbose, TEXT("AndroidThunkCpp_Facebook_RequestReadPermissions"));
	bool bSuccess = false;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		const bool bIsOptional = false;
		static jmethodID FacebookRequestReadMethod = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Facebook_RequestReadPermissions", "([Ljava/lang/String;)V", bIsOptional);
		CHECK_JNI_METHOD(FacebookRequestReadMethod);

		// Convert scope array into java fields
		jobjectArray PermsIDArray = (jobjectArray)Env->NewObjectArray(InNewPermissions.Num(), FJavaWrapper::JavaStringClass, nullptr);
		for (uint32 Param = 0; Param < InNewPermissions.Num(); Param++)
		{
			jstring StringValue = Env->NewStringUTF(TCHAR_TO_UTF8(*InNewPermissions[Param].Name));
			Env->SetObjectArrayElement(PermsIDArray, Param, StringValue);
			Env->DeleteLocalRef(StringValue);
		}

		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FacebookRequestReadMethod, PermsIDArray);

		// clean up references
		Env->DeleteLocalRef(PermsIDArray);
		bSuccess = true;
	}

	return bSuccess;
}

JNI_METHOD void Java_com_epicgames_ue4_FacebookLogin_nativeRequestReadPermissionsComplete(JNIEnv* jenv, jobject thiz, jsize responseCode, jstring accessToken)
{
	EFacebookLoginResponse LoginResponse = (EFacebookLoginResponse)responseCode;

	const char* charsAccessToken = jenv->GetStringUTFChars(accessToken, 0);
	FString AccessToken = FString(UTF8_TO_TCHAR(charsAccessToken));
	jenv->ReleaseStringUTFChars(accessToken, charsAccessToken);

	UE_LOG_ONLINE(VeryVerbose, TEXT("nativeRequestReadPermissionsComplete Response: %d Token: %s"), LoginResponse, *AccessToken);

	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.ProcessFacebookReadPermissions"), STAT_FSimpleDelegateGraphTask_ProcessFacebookReadPermissions, STATGROUP_TaskGraphTasks);
	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]()
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Facebook request read permissions completed %s\n"), ToString(LoginResponse));
		if (IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::Get(FACEBOOK_SUBSYSTEM))
		{
			FOnlineSharingFacebookPtr SharingFBInt = StaticCastSharedPtr<FOnlineSharingFacebook>(OnlineSub->GetSharingInterface());
			if (SharingFBInt.IsValid())
			{
				SharingFBInt->TriggerOnFacebookRequestPermissionsOpCompleteDelegates(LoginResponse, AccessToken);
			}
		}
	}),
	GET_STATID(STAT_FSimpleDelegateGraphTask_ProcessFacebookReadPermissions),
	nullptr,
	ENamedThreads::GameThread
	);
}

bool AndroidThunkCpp_Facebook_RequestPublishPermissions(const TArray<FSharingPermission>& InNewPermissions)
{
	UE_LOG_ONLINE(Verbose, TEXT("AndroidThunkCpp_Facebook_RequestPublishPermissions"));
	bool bSuccess = false;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		const bool bIsOptional = false;
		static jmethodID FacebookRequestPublishMethod = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_Facebook_RequestPublishPermissions", "([Ljava/lang/String;)V", bIsOptional);
		CHECK_JNI_METHOD(FacebookRequestPublishMethod);

		// Convert scope array into java fields
		jobjectArray PermsIDArray = (jobjectArray)Env->NewObjectArray(InNewPermissions.Num(), FJavaWrapper::JavaStringClass, nullptr);
		for (uint32 Param = 0; Param < InNewPermissions.Num(); Param++)
		{
			jstring StringValue = Env->NewStringUTF(TCHAR_TO_UTF8(*InNewPermissions[Param].Name));
			Env->SetObjectArrayElement(PermsIDArray, Param, StringValue);
			Env->DeleteLocalRef(StringValue);
		}

		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FacebookRequestPublishMethod, PermsIDArray);

		// clean up references
		Env->DeleteLocalRef(PermsIDArray);
		bSuccess = true;
	}

	return bSuccess;
}

JNI_METHOD void Java_com_epicgames_ue4_FacebookLogin_nativeRequestPublishPermissionsComplete(JNIEnv* jenv, jobject thiz, jsize responseCode, jstring accessToken)
{
	EFacebookLoginResponse LoginResponse = (EFacebookLoginResponse)responseCode;

	const char* charsAccessToken = jenv->GetStringUTFChars(accessToken, 0);
	FString AccessToken = FString(UTF8_TO_TCHAR(charsAccessToken));
	jenv->ReleaseStringUTFChars(accessToken, charsAccessToken);

	UE_LOG_ONLINE(VeryVerbose, TEXT("nativeRequestPublishPermissionsComplete Response: %d Token: %s"), LoginResponse, *AccessToken);

	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.ProcessFacebookPublishPermissions"), STAT_FSimpleDelegateGraphTask_ProcessFacebookPublishPermissions, STATGROUP_TaskGraphTasks);
	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]()
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Facebook request publish permissions completed %s\n"), ToString(LoginResponse));
		if (IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::Get(FACEBOOK_SUBSYSTEM))
		{
			FOnlineSharingFacebookPtr SharingFBInt = StaticCastSharedPtr<FOnlineSharingFacebook>(OnlineSub->GetSharingInterface());
			if (SharingFBInt.IsValid())
			{
				SharingFBInt->TriggerOnFacebookRequestPermissionsOpCompleteDelegates(LoginResponse, AccessToken);
			}
		}
	}),
	GET_STATID(STAT_FSimpleDelegateGraphTask_ProcessFacebookPublishPermissions),
	nullptr,
	ENamedThreads::GameThread
	);
}

#endif // WITH_FACEBOOK
