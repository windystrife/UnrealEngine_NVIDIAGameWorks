// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


// Module includes
#include "OnlineSharingFacebook.h"
#include "OnlineSubsystemFacebookPrivate.h"

#include "IOS/IOSAsyncTask.h"
#include "ImageCore.h"

#import <FBSDKCoreKit/FBSDKCoreKit.h>
#import <FBSDKShareKit/FBSDKShareKit.h>
#import <FBSDKLoginKit/FBSDKLoginKit.h>

FOnlineSharingFacebook::FOnlineSharingFacebook(FOnlineSubsystemFacebook* InSubsystem)
	: FOnlineSharingFacebookCommon(InSubsystem)
{
}

FOnlineSharingFacebook::~FOnlineSharingFacebook()
{
}

bool FOnlineSharingFacebook::RequestNewReadPermissions(int32 LocalUserNum, EOnlineSharingCategory NewPermissions)
{
	bool bTriggeredRequest = false;

	ensure((NewPermissions & ~EOnlineSharingCategory::ReadPermissionMask) == EOnlineSharingCategory::None);

	IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
	if (IdentityInt.IsValid() && IdentityInt->GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn)
	{
		bTriggeredRequest = true;

		dispatch_async(dispatch_get_main_queue(),^ 
			{
				// Fill in an nsarray with the permissions which match those that the user has set,
				// Here we iterate over each category, adding each individual permission linked with it in the ::SetupPermissionMaps
				NSMutableArray* PermissionsRequested = nil;

				TArray<FSharingPermission> PermissionsNeeded;
				const bool bHasPermission = CurrentPermissions.HasPermission(NewPermissions, PermissionsNeeded);
				if (!bHasPermission)
				{
					PermissionsRequested = [[NSMutableArray alloc] init];
					for (const FSharingPermission& Permission : PermissionsNeeded)
					{
						[PermissionsRequested addObject:[NSString stringWithFString:Permission.Name]];
					}
				}

				if (PermissionsRequested)
                {
					FBSDKLoginManager *loginManager = [[FBSDKLoginManager alloc] init];
					[loginManager logInWithReadPermissions:PermissionsRequested
										fromViewController:nil
										handler: ^(FBSDKLoginManagerLoginResult* result, NSError* error)
						{
							UE_LOG(LogOnline, Display, TEXT("logInWithReadPermissions : Success - %d"), error == nil);
							[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
							{
								if (error == nil)
								{
									FOnRequestCurrentPermissionsComplete PermsDelegate = FOnRequestCurrentPermissionsComplete::CreateRaw(this, &FOnlineSharingFacebook::OnRequestCurrentReadPermissionsComplete);
									RequestCurrentPermissions(LocalUserNum, PermsDelegate);
								}
								else
								{
									TriggerOnRequestNewReadPermissionsCompleteDelegates(LocalUserNum, false);
								}
								return true;
							}];
						}
					];
                }
                else
                {
                    // All permissions were already granted, no need to reauthorize
                    TriggerOnRequestNewReadPermissionsCompleteDelegates(LocalUserNum, true);
                }
			}
		);
	}
	else
	{
		// If we weren't logged into Facebook we cannot do this action
		TriggerOnRequestNewReadPermissionsCompleteDelegates(LocalUserNum, false);
	}

	// We did kick off a request
	return bTriggeredRequest;
}

void FOnlineSharingFacebook::OnRequestCurrentReadPermissionsComplete(int32 LocalUserNum, bool bWasSuccessful, const TArray<FSharingPermission>& Permissions)
{
	TriggerOnRequestNewReadPermissionsCompleteDelegates(LocalUserNum, bWasSuccessful);
}

bool FOnlineSharingFacebook::RequestNewPublishPermissions(int32 LocalUserNum, EOnlineSharingCategory NewPermissions, EOnlineStatusUpdatePrivacy Privacy)
{
	bool bTriggeredRequest = false;

	ensure((NewPermissions & ~EOnlineSharingCategory::PublishPermissionMask) == EOnlineSharingCategory::None);
	
	IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
	if (IdentityInt.IsValid() && IdentityInt->GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn)
	{
		bTriggeredRequest = true;

		dispatch_async(dispatch_get_main_queue(),^ 
			{
				// Fill in an nsarray with the permissions which match those that the user has set,
				// Here we iterate over each category, adding each individual permission linked with it in the ::SetupPermissionMaps

				NSMutableArray* PermissionsRequested = nil;

				TArray<FSharingPermission> PermissionsNeeded;
				const bool bHasPermission = CurrentPermissions.HasPermission(NewPermissions, PermissionsNeeded);
				if (!bHasPermission)
				{
					PermissionsRequested = [[NSMutableArray alloc] init];
					for (const FSharingPermission& Permission : PermissionsNeeded)
					{
						[PermissionsRequested addObject:[NSString stringWithFString:Permission.Name]];
					}
				}

				if (PermissionsRequested)
				{
					FBSDKLoginManager *loginManager = [[FBSDKLoginManager alloc] init];
                    FBSDKDefaultAudience DefaultAudience = FBSDKDefaultAudienceOnlyMe;
                    switch (Privacy)
                    {
                        case EOnlineStatusUpdatePrivacy::OnlyMe:
                            DefaultAudience = FBSDKDefaultAudienceOnlyMe;
                            break;
                        case EOnlineStatusUpdatePrivacy::OnlyFriends:
                            DefaultAudience = FBSDKDefaultAudienceFriends;
                            break;
                        case EOnlineStatusUpdatePrivacy::Everyone:
                            DefaultAudience = FBSDKDefaultAudienceEveryone;
                            break;
                    }

					[loginManager logInWithPublishPermissions:PermissionsRequested
										fromViewController:nil
										handler: ^(FBSDKLoginManagerLoginResult* result, NSError* error)
						{
							UE_LOG(LogOnline, Display, TEXT("logInWithPublishPermissions : Success - %d"), error == nil);
							[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
							{
								if (error == nil)
								{
									FOnRequestCurrentPermissionsComplete PermsDelegate = FOnRequestCurrentPermissionsComplete::CreateRaw(this, &FOnlineSharingFacebook::OnRequestCurrentPublishPermissionsComplete);
									RequestCurrentPermissions(LocalUserNum, PermsDelegate);
								}
								else
								{
									TriggerOnRequestNewPublishPermissionsCompleteDelegates(LocalUserNum, false);
								}
								return true;
							}];
						}
					 ];
                }
                else
                {
                    // All permissions were already granted, no need to reauthorize
                    TriggerOnRequestNewPublishPermissionsCompleteDelegates(LocalUserNum, true);
                }
			}
		);
	}
	else
	{
		// If we weren't logged into Facebook we cannot do this action
		TriggerOnRequestNewPublishPermissionsCompleteDelegates(LocalUserNum, false);
	}

	// We did kick off a request
	return bTriggeredRequest;
}

void FOnlineSharingFacebook::OnRequestCurrentPublishPermissionsComplete(int32 LocalUserNum, bool bWasSuccessful, const TArray<FSharingPermission>& Permissions)
{
	TriggerOnRequestNewPublishPermissionsCompleteDelegates(LocalUserNum, bWasSuccessful);
}

bool FOnlineSharingFacebook::ShareStatusUpdate(int32 LocalUserNum, const FOnlineStatusUpdate& StatusUpdate)
{
	bool bTriggeredRequest = false;

	IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
	if (IdentityInt.IsValid() && IdentityInt->GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn)
	{
		bTriggeredRequest = true;

		dispatch_async(dispatch_get_main_queue(),^ 
			{
				// We need to determine where we are posting to.
				NSString* GraphPath;

				// The contents of the post
				NSMutableDictionary* Params = [[NSMutableDictionary alloc] init];

				NSString* ConvertedMessage = [NSString stringWithFString:StatusUpdate.Message];
				[Params setObject:ConvertedMessage forKey:@"message"];

				// Setup the image if one was added to the post
				UIImage* SharingImage = nil;
				if( StatusUpdate.Image != nil )
				{
					// We are posting this as a photo.
					GraphPath = @"me/photos";

					// Convert our FImage to a UIImage
					int32 Width = StatusUpdate.Image->SizeX;
					int32 Height = StatusUpdate.Image->SizeY;

					CGColorSpaceRef colorSpace=CGColorSpaceCreateDeviceRGB();
					CGContextRef bitmapContext=CGBitmapContextCreate(StatusUpdate.Image->RawData.GetData(), Width, Height, 8, 4*Width, colorSpace,  kCGImageAlphaNoneSkipLast);
					CFRelease(colorSpace);

					CGImageRef cgImage=CGBitmapContextCreateImage(bitmapContext);
					CGContextRelease(bitmapContext);

					SharingImage = [UIImage imageWithCGImage:cgImage];
					CGImageRelease(cgImage);

					[Params setObject:SharingImage forKey:@"picture"];
				}
				else
				{
					// if no image, we are posting this to the news feed.
					GraphPath = @"me/feed";
				}

				// get the formatted friends tags
				NSString* TaggedFriendIds = @"";

				for(int32 FriendIdx = 0; FriendIdx < StatusUpdate.TaggedFriends.Num(); FriendIdx++)
				{
					NSString* FriendId = [NSString stringWithFString:StatusUpdate.TaggedFriends[FriendIdx]->ToString()];
								
					TaggedFriendIds = [TaggedFriendIds stringByAppendingFormat:@"%@%@", 
						FriendId,
						(FriendIdx < StatusUpdate.TaggedFriends.Num() - 1) ? @"," : @""
					];
				}

				if( ![TaggedFriendIds isEqual:@""])
				{
					[Params setObject:TaggedFriendIds forKey:@"tags"];
				}

				// kick off a request to post the status
				[[[FBSDKGraphRequest alloc]
					initWithGraphPath:GraphPath
					parameters:Params
					HTTPMethod:@"POST"]
					startWithCompletionHandler:^(FBSDKGraphRequestConnection *connection, id result, NSError *error)
					{
						UE_LOG(LogOnline, Display, TEXT("startWithGraphPath : Success - %d"), error == nil);
						TriggerOnSharePostCompleteDelegates( LocalUserNum, error == nil );
					}
				];
			}
		);
	}
	else
	{
		// If we weren't logged into Facebook we cannot do this action
		TriggerOnSharePostCompleteDelegates(LocalUserNum, false);
	}

	return bTriggeredRequest;
}

bool FOnlineSharingFacebook::ReadNewsFeed(int32 LocalUserNum, int32 NumPostsToRead)
{
	bool bTriggeredRequest = false;

	IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
	if (IdentityInt.IsValid() && IdentityInt->GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn)
	{
		bTriggeredRequest = true;
		
		dispatch_async(dispatch_get_main_queue(),^ 
			{
				// The current read permissions for this OSS.
				NSString *fqlQuery = 
					[NSString stringWithFormat:@"SELECT post_id, created_time, type, attachment \
												FROM stream WHERE filter_key in (SELECT filter_key \
												FROM stream_filter WHERE uid=me() AND type='newsfeed')AND is_hidden = 0 \
												LIMIT %d", NumPostsToRead];
				
				[[[FBSDKGraphRequest alloc]
					initWithGraphPath:@"/fql"
					parameters:[NSDictionary dictionaryWithObjectsAndKeys: fqlQuery, @"q", nil]
					HTTPMethod:@"GET"]
					startWithCompletionHandler:^(FBSDKGraphRequestConnection *connection, id result, NSError *error)
					{
						if( error )
						{
							UE_LOG(LogOnline, Display, TEXT("FOnlineSharingFacebook::ReadStatusFeed - error[%s]"), *FString([error localizedDescription]));
						}

						TriggerOnReadNewsFeedCompleteDelegates(LocalUserNum, error==nil);
					}
				];
			}
		);
	}
	else
	{
		// If we weren't logged into Facebook we cannot do this action
		TriggerOnReadNewsFeedCompleteDelegates(LocalUserNum, false);
	}

	return bTriggeredRequest;
}

