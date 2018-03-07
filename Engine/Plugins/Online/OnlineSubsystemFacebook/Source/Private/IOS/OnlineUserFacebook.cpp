// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// Module includes
#include "OnlineUserFacebook.h"
#include "OnlineSubsystemFacebookPrivate.h"
#include "OnlineIdentityFacebook.h"

#import <FBSDKCoreKit/FBSDKCoreKit.h>

// FOnlineUserFacebook

FOnlineUserFacebook::FOnlineUserFacebook(FOnlineSubsystemFacebook* InSubsystem)
	: FOnlineUserFacebookCommon(InSubsystem)
{
}

FOnlineUserFacebook::~FOnlineUserFacebook()
{

}

bool FOnlineUserFacebook::QueryUserInfo(int32 LocalUserNum, const TArray<TSharedRef<const FUniqueNetId> >& UserIds)
{
	bool bTriggeredRequest = false;
	
	IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
	if(UserIds.Num() > 0 && IdentityInt->GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn)
	{
		bTriggeredRequest = true;
		CachedUsers.Empty();

		dispatch_async(dispatch_get_main_queue(),^ 
			{
				// We only ever have a single user in the facebook OSS.
				FString ErrorStr;
				bool bGatheredUserInfo = false;
				if([FBSDKAccessToken currentAccessToken])
				{
					CachedUsers.Empty();
					
					bool bValidUserRequested = false;
					const FString UserName([FBSDKProfile currentProfile].userID);
					for(auto& NextUser : UserIds)
					{
						if(NextUser->ToString() == UserName)
						{
							bValidUserRequested = true;
							break;
						}
					}
					if(bValidUserRequested)
					{
						const FString RealName([FBSDKProfile currentProfile].name);
						
						TSharedRef<FOnlineUserInfoFacebook> FBUserInfo = MakeShareable(new FOnlineUserInfoFacebook(UserIds[0]->ToString()));
						FBUserInfo->AccountData.Add(TEXT("name"), RealName);
						FBUserInfo->AccountData.Add(TEXT("username"), UserName);
						
						UE_LOG(LogOnline, Display, TEXT("User Found: u:%s r:%s"), *UserName, *RealName);
						CachedUsers.Add( FBUserInfo );
						bGatheredUserInfo = true;
					}
					else
					{
						ErrorStr = TEXT("No user ids matched those of the single facebook user.");
						UE_LOG(LogOnline, Display, TEXT("Failed to gather user information: %s"), *ErrorStr);
					}

				}
				else
				{
					ErrorStr = TEXT("No valid login.");
					UE_LOG(LogOnline, Display, TEXT("Failed to gather user information: %s"), *ErrorStr);
				}
				
				TriggerOnQueryUserInfoCompleteDelegates(LocalUserNum, bGatheredUserInfo, UserIds, *ErrorStr);

				
/*
				// A full list of all attainable data is here:
				// https://developers.facebook.com/docs/reference/fql/user/

				// Create a query with all the desired user ids and the data we wish to query
				NSMutableString *fqlQuery = 
					[NSMutableString stringWithFormat:@"SELECT name, username FROM user WHERE uid IN ("];

				for( int32 Idx = 0; Idx < UserIds.Num(); Idx++ )
				{
					[fqlQuery appendString:[NSString stringWithFString:UserIds[Idx]->ToString()]];
					
					// Append a comma or close the parenthesis
					if( Idx < UserIds.Num()-1 )
					{
						[fqlQuery appendString:@","];
					}
					else
					{
						[fqlQuery appendString:@")"];
					}
				}

				UE_LOG(LogOnline, Display, TEXT("RunningFQL Query: %s"), *FString(fqlQuery));

				// Kick off the FB Request
				[[[FBSDKGraphRequest alloc]
					initWithGraphPath:@"/fql"
					parameters:[NSDictionary dictionaryWithObjectsAndKeys: fqlQuery, @"q", nil]
					HTTPMethod:@"GET"]
					startWithCompletionHandler:^(FBSDKGraphRequestConnection *connection, id result, NSError *error)
					{
						FString ErrorStr;
						if( error )
						{
							ErrorStr = FString::Printf( TEXT("%s"), *FString([error localizedDescription]) );
						}
						else
						{
							NSArray* UserList = [[NSArray alloc] initWithArray:[result objectForKey:@"data"]];

							// Clear our previously cached users before we repopulate the cache.
							CachedUsers.Empty();

							for( int32 UserIdx = 0; UserIdx < [UserList count]; UserIdx++ )
							{	
								NSDictionary* User = UserList[ UserIdx ];

								const FString UserName([User objectForKey:@"username"]);
								const FString RealName([User objectForKey:@"name"]);

								TSharedRef<FOnlineUserInfoFacebook> FBUserInfo = MakeShareable(new FOnlineUserInfoFacebook(UserIds[UserIdx]->ToString()));
								FBUserInfo->AccountData.Add(TEXT("name"), RealName);
								FBUserInfo->AccountData.Add(TEXT("username"), UserName);
								CachedUsers.Add( FBUserInfo );
							}
						}

						TriggerOnQueryUserInfoCompleteDelegates(LocalUserNum, error == nil, UserIds, ErrorStr);
					}
				];
 */
			}
		);
	}
	else
	{
		TriggerOnQueryUserInfoCompleteDelegates(LocalUserNum, false, UserIds, UserIds.Num() > 0 ? TEXT("Not logged in.") : TEXT("No users requested."));
	}
	
	return bTriggeredRequest;
}

