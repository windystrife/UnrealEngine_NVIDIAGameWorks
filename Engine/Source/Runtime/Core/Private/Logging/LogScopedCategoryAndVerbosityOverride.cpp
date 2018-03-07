// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Logging/LogScopedCategoryAndVerbosityOverride.h"
#include "HAL/PlatformTLS.h"

FLogScopedCategoryAndVerbosityOverride::FLogScopedCategoryAndVerbosityOverride(FName Category, ELogVerbosity::Type Verbosity)
{
	FOverride* TLS = GetTLSCurrent();
	Backup = *TLS;
	*TLS = FOverride(Category, Verbosity);
}

FLogScopedCategoryAndVerbosityOverride::~FLogScopedCategoryAndVerbosityOverride()
{
	FOverride* TLS = GetTLSCurrent();
	*TLS = Backup;
}

static uint32 OverrrideTLSID = FPlatformTLS::AllocTlsSlot();
FLogScopedCategoryAndVerbosityOverride::FOverride* FLogScopedCategoryAndVerbosityOverride::GetTLSCurrent()
{
	FOverride* TLS = (FOverride*)FPlatformTLS::GetTlsValue(OverrrideTLSID);
	if (!TLS)
	{
		TLS = new FOverride;
		FPlatformTLS::SetTlsValue(OverrrideTLSID, TLS);
	}	
	return TLS;
}

