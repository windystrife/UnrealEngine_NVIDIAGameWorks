// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NameTypes.h"
#include "OnlineSubsystemPackage.h"

// These are Xbox One specific setting to be used in FOnlineSessionSettings as keys; technically this can be put in OnlineSubsystemSettings.h

/** Custom settings to be associated with Session (value is FString)*/
#define SETTING_CUSTOM FName(TEXT("CUSTOM"))
/** Session is marked as using party enabled session flag (value is bool)*/
#define SETTING_PARTY_ENABLED_SESSION FName(TEXT("PARTYENABLEDSESSION"))
/** Hopper name to search within (value is FString) */
#define SETTING_MATCHING_HOPPER FName(TEXT("MATCHHOPPER"))
/** Timeout for search (value is float seconds) */
#define SETTING_MATCHING_TIMEOUT FName(TEXT("MATCHTIMEOUT"))
/** Match attributes (value is FString) */
#define SETTING_MATCHING_ATTRIBUTES	FName(TEXT("MATCHATTRIBUTES"))
/** Match attributes (value is FString) */
#define SETTING_MATCH_MEMBERS_JSON FName(TEXT("MATCHMEMBERS"))
/** Session member constant custom json (value is FString) - This is to be used with FString::Printf, populated with user xuid */
#define SETTING_SESSION_MEMBER_CONSTANT_CUSTOM_JSON_XUID FName(TEXT("SESSIONMEMBERCONSTANTCUSTOMJSON%s"))
/** Set self as host (value is int32)*/
#define SETTING_MAX_RESULT FName(TEXT("MAXRESULT"))
/** Set self as host (value is int32)*/
#define SETTING_CONTRACT_VERSION_FILTER FName(TEXT("CONTRACTVERSIONFILTER"))
/** Set self as host (value is int32)*/
#define SETTING_FIND_PRIVATE_SESSIONS FName(TEXT("FINDPRIVATESESSIONS"))
/** Set self as host (value is int32)*/
#define SETTING_FIND_RESERVED_SESSIONS FName(TEXT("FINDRESERVEDSESSIONS"))
/** Set self as host (value is int32)*/
#define SETTING_FIND_INACTIVE_SESSIONS FName(TEXT("FINDINACTIVESESSIONS"))
/** Set self as host (value is int32)*/
#define SETTING_MULTIPLAYER_VISIBILITY FName(TEXT("MULTIPLAYERVISIBILITY"))
/** Session template name, must match a session template name from the service config. */
#define SETTING_SESSION_TEMPLATE_NAME FName(TEXT("SESSIONTEMPLATENAME"))
/** Session change number from shoulder tapped version */
#define SETTING_CHANGE_NUMBER FName(TEXT("CHANGENUMBER"))
/** Session can attempt arbiter migration */
#define SETTING_ALLOW_ARBITER_MIGRATION FName(TEXT("ALLOWARBITERMIGRATION"))
/** Session preservation type for matchmaking */
#define SETTING_MATCHING_PRESERVE_SESSION FName(TEXT("PRESERVESESSIONALWAYS"))
/** Matchmade game session URI for join in progress/invites */
#define  SETTING_GAME_SESSION_URI FName(TEXT("GAMESESSIONURI"))
/** Session member group identifier (value is FString) - This is to be used with FString::Printf, populated with user xuid - Field required with Team Based matchmaking*/
#define SETTING_GROUP_NAME FName(TEXT("USERGROUPNAME%s"))
/** Information to join a third-party auxiliary session (value is FString) */
#define SETTING_CUSTOM_JOIN_INFO FName(TEXT("CUSTOMJOININFO"))

// These are PS4 specific settings to be used in FOnlineSessionSettings as keys

/** Enables host migration for PS4 sessions, which is handled on Sony's servers. This means that all clients can update the session, we do not get callbacks when we become the host or a chance to set the host, it is picked automatically. */
#define SETTING_HOST_MIGRATION FName(TEXT("HOSTMIGRATION"))
