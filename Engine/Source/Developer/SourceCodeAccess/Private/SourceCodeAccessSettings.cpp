// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SourceCodeAccessSettings.h"
#include "Misc/ConfigCacheIni.h"

USourceCodeAccessSettings::USourceCodeAccessSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if PLATFORM_WINDOWS
	PreferredAccessor = TEXT("VisualStudioSourceCodeAccessor");
#elif PLATFORM_MAC
	PreferredAccessor = TEXT("XCodeSourceCodeAccessor");
#elif PLATFORM_LINUX
	GConfig->GetString(TEXT("/Script/SourceCodeAccess.SourceCodeAccessSettings"), TEXT("PreferredAccessor"), PreferredAccessor, GEngineIni);
	UE_LOG(LogHAL, Log, TEXT("Linux SourceCodeAccessSettings: %s"), *PreferredAccessor);
#endif
}
