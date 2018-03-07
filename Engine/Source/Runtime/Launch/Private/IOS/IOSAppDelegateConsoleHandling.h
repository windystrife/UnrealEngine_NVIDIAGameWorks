// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IOSAppDelegate.h"

@interface IOSAppDelegate (ConsoleHandling)

#if !UE_BUILD_SHIPPING && !PLATFORM_TVOS
/**
 * Shows the console input dialog
 */
- (void)ShowConsole;

/**
 * Shows an alert with up to 3 buttons. A delegate callback will later set AlertResponse property
 */
- (void)ShowAlert:(NSMutableArray*)StringArray;

/**
 * Handles processing of an input console command
 */
- (void)HandleConsoleCommand:(NSString*)ConsoleCommand;
#endif


@end
