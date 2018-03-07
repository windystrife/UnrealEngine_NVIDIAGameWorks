// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/** FName declaration of IOS subsystem */

#include "CoreMinimal.h"
#include "SocketSubsystem.h"
#include "ModuleManager.h"
#include "IOSAppDelegate.h"
#include "IOS/IOSAsyncTask.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeLock.h"

class FOnlineSubsystemIOS;

#include "OnlineSessionInterfaceIOS.h"
#include "OnlineFriendsInterfaceIOS.h"
#include "OnlineIdentityInterfaceIOS.h"
#include "OnlineLeaderboardsInterfaceIOS.h"
#include "OnlineStoreInterfaceIOS.h"
#include "OnlineStoreIOS.h"
#include "OnlinePurchaseIOS.h"
#include "OnlineAchievementsInterfaceIOS.h"
#include "OnlineExternalUIInterfaceIOS.h"
#include "OnlineTurnBasedInterfaceIOS.h"
#include "OnlineUserCloudInterfaceIOS.h"
#include "OnlineSharedCloudInterfaceIOS.h"

#include "OnlineSubsystemIOS.h"
#include "OnlineSubsystemIOSModule.h"

