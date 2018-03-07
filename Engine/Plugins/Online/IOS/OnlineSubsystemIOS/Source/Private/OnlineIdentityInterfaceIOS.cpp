// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemIOSPrivatePCH.h"
#include "OnlineError.h"
#import "OnlineAppStoreUtils.h"

FOnlineIdentityIOS::FOnlineIdentityIOS()
	: UniqueNetId(nullptr)
{
}

FOnlineIdentityIOS::FOnlineIdentityIOS(FOnlineSubsystemIOS* InSubsystem)
	: UniqueNetId(nullptr)
	, Subsystem(InSubsystem)
{
}

TSharedPtr<FUniqueNetIdString> FOnlineIdentityIOS::GetLocalPlayerUniqueId() const
{
	return UniqueNetId;
}

void FOnlineIdentityIOS::SetLocalPlayerUniqueId(const TSharedPtr<FUniqueNetIdString>& UniqueId)
{
	UniqueNetId = UniqueId;
}

TSharedPtr<FUserOnlineAccount> FOnlineIdentityIOS::GetUserAccount(const FUniqueNetId& UserId) const
{
	// not implemented
	TSharedPtr<FUserOnlineAccount> Result;
	return Result;
}

TArray<TSharedPtr<FUserOnlineAccount> > FOnlineIdentityIOS::GetAllUserAccounts() const
{
	// not implemented
	TArray<TSharedPtr<FUserOnlineAccount> > Result;
	return Result;
}

bool FOnlineIdentityIOS::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	bool bStartedLogin = false;

	// Since the iOS login code may show a UI, ShowLoginUI is a better fit here. Also, note that the ConnectToService blueprint
	// node that calls Login is deprecated (there's a new ShowExternalLoginUI node meant to replace it).
	UE_LOG(LogOnline, Warning, TEXT("Using the IOnlineIdentity::Login function on iOS is not recommended. Please use IOnlineExternalUI::ShowLoginUI instead."));

	// Was the login handled by Game Center
	if( GetLocalGameCenterUser() && 
		GetLocalGameCenterUser().isAuthenticated )
	{
        // Now logged in
		bStartedLogin = true;
        
		const FString PlayerId(GetLocalGameCenterUser().playerID);
		UniqueNetId = MakeShareable( new FUniqueNetIdString( PlayerId ) );
		TriggerOnLoginCompleteDelegates(LocalUserNum, true, *UniqueNetId, TEXT(""));
        
        UE_LOG(LogOnline, Log, TEXT("The user %s has logged into Game Center"), *PlayerId);
	}
	else if([IOSAppDelegate GetDelegate].OSVersion >= 6.0f)
	{
		// Trigger the login event on the main thread.
		bStartedLogin = true;
		dispatch_async(dispatch_get_main_queue(), ^
		{
			[GetLocalGameCenterUser() setAuthenticateHandler:(^(UIViewController* viewcontroller, NSError *error)
			{
				bool bWasSuccessful = false;

				// The login process has completed.
				if (viewcontroller == nil)
				{
					FString ErrorMessage;

					if (GetLocalGameCenterUser().isAuthenticated == YES)
					{
						/* Perform additional tasks for the authenticated player here */
						const FString PlayerId(GetLocalGameCenterUser().playerID);
						UniqueNetId = MakeShareable(new FUniqueNetIdString(PlayerId));

						bWasSuccessful = true;
						UE_LOG(LogOnline, Log, TEXT("The user %s has logged into Game Center"), *PlayerId);
					}
					else
					{
						ErrorMessage = TEXT("The user could not be authenticated by Game Center");
						UE_LOG(LogOnline, Log, TEXT("%s"), *ErrorMessage);
					}

					if (error)
					{
						NSString *errstr = [error localizedDescription];
						UE_LOG(LogOnline, Warning, TEXT("Game Center login has failed. %s]"), *FString(errstr));
					}

					// Report back to the game thread whether this succeeded.
					[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
					{
						TSharedPtr<FUniqueNetIdString> UniqueIdForUser = UniqueNetId.IsValid() ? UniqueNetId : MakeShareable(new FUniqueNetIdString());
						TriggerOnLoginCompleteDelegates(LocalUserNum, bWasSuccessful, *UniqueIdForUser, *ErrorMessage);

						return true;
					}];
				}
				else
				{
					// Game Center has provided a view controller for us to login, we present it.
					[[IOSAppDelegate GetDelegate].IOSController 
						presentViewController:viewcontroller animated:YES completion:nil];
				}
			})];
		});
	}
	else
	{
		// User is not currently logged into game center
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, FUniqueNetIdString(), TEXT("IOS version is not compatible with the game center implementation"));
	}
	
	return bStartedLogin;
}

bool FOnlineIdentityIOS::Logout(int32 LocalUserNum)
{
	TriggerOnLogoutCompleteDelegates(LocalUserNum, false);
	return true;
}

bool FOnlineIdentityIOS::AutoLogin(int32 LocalUserNum)
{
	return Login( LocalUserNum, FOnlineAccountCredentials() );
}

ELoginStatus::Type FOnlineIdentityIOS::GetLoginStatus(int32 LocalUserNum) const
{
	ELoginStatus::Type LoginStatus = ELoginStatus::NotLoggedIn;

	if(LocalUserNum < MAX_LOCAL_PLAYERS && GetLocalGameCenterUser() != NULL && GetLocalGameCenterUser().isAuthenticated == YES)
	{
		LoginStatus = ELoginStatus::LoggedIn;
	}

	return LoginStatus;
}

ELoginStatus::Type FOnlineIdentityIOS::GetLoginStatus(const FUniqueNetId& UserId) const 
{
	ELoginStatus::Type LoginStatus = ELoginStatus::NotLoggedIn;

	if(GetLocalGameCenterUser() != NULL && GetLocalGameCenterUser().isAuthenticated == YES)
	{
		LoginStatus = ELoginStatus::LoggedIn;
	}

	return LoginStatus;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityIOS::GetUniquePlayerId(int32 LocalUserNum) const
{
	return UniqueNetId;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityIOS::CreateUniquePlayerId(uint8* Bytes, int32 Size)
{
	if( Bytes && Size == sizeof(uint64) )
	{
		int32 StrLen = FCString::Strlen((TCHAR*)Bytes);
		if (StrLen > 0)
		{
			FString StrId((TCHAR*)Bytes);
			return MakeShareable(new FUniqueNetIdString(StrId));
		}
	}
    
	return NULL;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityIOS::CreateUniquePlayerId(const FString& Str)
{
	return MakeShareable(new FUniqueNetIdString(Str));
}

FString FOnlineIdentityIOS::GetPlayerNickname(int32 LocalUserNum) const
{
	if (LocalUserNum < MAX_LOCAL_PLAYERS && GetLocalGameCenterUser() != NULL)
	{
		NSString* PersonaName = [GetLocalGameCenterUser() alias];
		
        if (PersonaName != nil)
        {
            return FString(PersonaName);
        }
	}

	return FString();
}

FString FOnlineIdentityIOS::GetPlayerNickname(const FUniqueNetId& UserId) const 
{
	if (GetLocalGameCenterUser() != NULL)
	{
		NSString* PersonaName = [GetLocalGameCenterUser() alias];
		
        if (PersonaName != nil)
        {
            return FString(PersonaName);
        }
	}

	return FString();
}

FString FOnlineIdentityIOS::GetAuthToken(int32 LocalUserNum) const
{
	FString ResultToken;
	return ResultToken;
}

void FOnlineIdentityIOS::RevokeAuthToken(const FUniqueNetId& UserId, const FOnRevokeAuthTokenCompleteDelegate& Delegate)
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineIdentityIOS::RevokeAuthToken not implemented"));
	TSharedRef<const FUniqueNetId> UserIdRef(UserId.AsShared());
	Subsystem->ExecuteNextTick([UserIdRef, Delegate]()
	{
		Delegate.ExecuteIfBound(*UserIdRef, FOnlineError(FString(TEXT("RevokeAuthToken not implemented"))));
	});
}

void FOnlineIdentityIOS::GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate)
{
	if (Privilege == EUserPrivileges::CanPlayOnline)
	{
		TSharedRef<const FUniqueNetId> SharedUserId = UserId.AsShared();
		FOnQueryAppBundleIdResponse completionDelegate = FOnQueryAppBundleIdResponse::CreateLambda([this, SharedUserId, Privilege, Delegate](NSDictionary* ResponseDict)
		{
			UE_LOG(LogOnline, Log, TEXT("GetUserPrivilege Complete"));
																									   
			uint32 Result = (uint32)EPrivilegeResults::GenericFailure;
			if (ResponseDict != nil && [ResponseDict[@"resultCount"] integerValue] == 1)
			{
				// Get local bundle information
				NSDictionary* infoDictionary = [[NSBundle mainBundle] infoDictionary];
				FString localAppId = FString(infoDictionary[@"CFBundleIdentifier"]);
				FString localVersionString = FString(infoDictionary[@"CFBundleShortVersionString"]);
			    UE_LOG(LogOnline, Log, TEXT("Local: %s %s"), *localAppId, *localVersionString);

				// Get remote bundle information
				FString remoteAppId = FString([[[ResponseDict objectForKey:@"results"] objectAtIndex:0] objectForKey:@"bundleId"]);
				FString remoteVersionString = FString([[[ResponseDict objectForKey:@"results"] objectAtIndex:0] objectForKey:@"version"]);
				UE_LOG(LogOnline, Log, TEXT("Remote: %s %s"), *remoteAppId, *remoteVersionString);

				if (localAppId == remoteAppId)
				{
					TArray<FString> LocalVersionParts;
					localVersionString.ParseIntoArray(LocalVersionParts, TEXT("."));

					TArray<FString> RemoteVersionParts;
					remoteVersionString.ParseIntoArray(RemoteVersionParts, TEXT("."));

					if (LocalVersionParts.Num() >= 2 &&
						RemoteVersionParts.Num() >= 2)
					{
						Result = (uint32)EPrivilegeResults::NoFailures;

						if (LocalVersionParts[0] != RemoteVersionParts[0] ||
							LocalVersionParts[1] != RemoteVersionParts[1])
						{
							UE_LOG(LogOnline, Log, TEXT("Needs Update"));
							Result = (uint32)EPrivilegeResults::RequiredPatchAvailable;
						}
						else
						{
							const FString LocalHotfixVersion = LocalVersionParts.IsValidIndex(2) ? LocalVersionParts[2] : TEXT("0");
							const FString RemoteHotfixVersion = RemoteVersionParts.IsValidIndex(2) ? RemoteVersionParts[2] : TEXT("0");

							if (LocalHotfixVersion != RemoteHotfixVersion)
							{
								UE_LOG(LogOnline, Log, TEXT("Needs Update"));
								Result = (uint32)EPrivilegeResults::RequiredPatchAvailable;
							}
							else
							{
								UE_LOG(LogOnline, Log, TEXT("Does NOT Need Update"));
							}
						}
					}
				}
				else
				{
					UE_LOG(LogOnline, Log, TEXT("BundleId does not match local bundleId"));
				}
			}
			else
			{
				UE_LOG(LogOnline, Log, TEXT("GetUserPrivilege invalid response"));
			}

			Subsystem->ExecuteNextTick([Delegate, SharedUserId, Privilege, Result]()
			{
				Delegate.ExecuteIfBound(*SharedUserId, Privilege, Result);
			});
		});
		
		FAppStoreUtils* AppStoreUtils = Subsystem->GetAppStoreUtils();
		if (AppStoreUtils)
		{
			[AppStoreUtils queryAppBundleId: completionDelegate];
		}
	 }
	 else
	 {
		  Delegate.ExecuteIfBound(UserId, Privilege, (uint32)EPrivilegeResults::NoFailures);
	 }
}

FPlatformUserId FOnlineIdentityIOS::GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& InUniqueNetId) const
{
	for (int i = 0; i < MAX_LOCAL_PLAYERS; ++i)
	{
		auto CurrentUniqueId = GetUniquePlayerId(i);
		if (CurrentUniqueId.IsValid() && (*CurrentUniqueId == InUniqueNetId))
		{
			return i;
		}
	}

	return PLATFORMUSERID_NONE;
}

FString FOnlineIdentityIOS::GetAuthType() const
{
	return TEXT("");
}
