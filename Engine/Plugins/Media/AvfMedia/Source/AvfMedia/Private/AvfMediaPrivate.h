// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"


/** Log category for the AvfMedia module. */
DECLARE_LOG_CATEGORY_EXTERN(LogAvfMedia, Log, All);


#if PLATFORM_MAC
	#define AVF_GAME_THREAD_BLOCK 
	#define AVF_GAME_THREAD_RETURN 
	#define AVF_GAME_THREAD_CALL(Block)	GameThreadCall(Block, @[ NSDefaultRunLoopMode ], false)
#else
	#define AVF_GAME_THREAD_BLOCK bool(void)
	#define AVF_GAME_THREAD_RETURN return true
	#define AVF_GAME_THREAD_CALL(Block) \
			[FIOSAsyncTask CreateTaskWithBlock: \
				Block	\
			]
#endif
