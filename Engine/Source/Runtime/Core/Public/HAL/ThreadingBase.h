// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMath.h"
#include "HAL/PlatformAtomics.h"
#include "HAL/MemoryBase.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformMemory.h"
#include "HAL/UnrealMemory.h"
#include "Templates/Function.h"

#include "HAL/PlatformProperties.h"
#include "HAL/PlatformString.h"
#include "HAL/Event.h"
#include "Misc/ScopedEvent.h"
#include "HAL/Runnable.h"
#include "HAL/PlatformTLS.h"
#include "HAL/PlatformAffinity.h"
#include "HAL/RunnableThread.h"
#include "Misc/SingleThreadEvent.h"
#include "Misc/SingleThreadRunnable.h"
#include "HAL/CriticalSection.h"
#include "HAL/ThreadManager.h"
#include "Misc/IQueuedWork.h"
#include "Misc/QueuedThreadPool.h"
#include "HAL/ThreadSafeCounter.h"
#include "HAL/ThreadSafeCounter64.h"
#include "HAL/ThreadSafeBool.h"
#include "Misc/NoopCounter.h"
#include "Misc/ScopeLock.h"
#include "HAL/TlsAutoCleanup.h"
#include "HAL/ThreadSingleton.h"



