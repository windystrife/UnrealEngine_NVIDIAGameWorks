// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CrashReportClientConfig.h"
#include "Logging/LogMacros.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/App.h"
#include "CrashReportClientApp.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"

FCrashReportClientConfig::FCrashReportClientConfig()
	: DiagnosticsFilename( TEXT( "Diagnostics.txt" ) )
	, SectionName( TEXT( "CrashReportClient" ) )
{
	const bool bUnattended =
#if CRASH_REPORT_UNATTENDED_ONLY
		true;
#else
		FApp::IsUnattended();
#endif // CRASH_REPORT_UNATTENDED_ONLY

	if (!GConfig->GetString(*SectionName, TEXT("CrashReportClientVersion"), CrashReportClientVersion, GEngineIni))
	{
		CrashReportClientVersion = TEXT("0.0.0");
	}
	UE_LOG(CrashReportClientLog, Log, TEXT("CrashReportClientVersion=%s"), *CrashReportClientVersion);

	if (!GConfig->GetString( *SectionName, TEXT( "CrashReportReceiverIP" ), CrashReportReceiverIP, GEngineIni ))
	{
		// Use the default value (blank/disabled)
		CrashReportReceiverIP = TEXT("");
	}
	if (CrashReportReceiverIP.IsEmpty())
	{
		UE_LOG(CrashReportClientLog, Log, TEXT("CrashReportReceiver disabled"));
	}
	else
	{
		UE_LOG(CrashReportClientLog, Log, TEXT("CrashReportReceiverIP: %s"), *CrashReportReceiverIP);
	}

	if (!GConfig->GetString(*SectionName, TEXT("DataRouterUrl"), DataRouterUrl, GEngineIni))
	{
		// Use the default value.
		DataRouterUrl = TEXT("");
	}
	if (DataRouterUrl.IsEmpty())
	{
		UE_LOG(CrashReportClientLog, Log, TEXT("DataRouter disabled"));
	}
	else
	{
		UE_LOG(CrashReportClientLog, Log, TEXT("DataRouterUrl: %s"), *DataRouterUrl);
	}

	if (!GConfig->GetBool( TEXT( "CrashReportClient" ), TEXT( "bAllowToBeContacted" ), bAllowToBeContacted, GEngineIni ))
	{
		// Default to true when unattended when config is missing. This is mostly for dedicated servers that do not have config files for CRC.
		if (bUnattended)
		{
			bAllowToBeContacted = true;
		}
	}

	if (!GConfig->GetBool( TEXT( "CrashReportClient" ), TEXT( "bSendLogFile" ), bSendLogFile, GEngineIni ))
	{
		// Default to true when unattended when config is missing. This is mostly for dedicated servers that do not have config files for CRC.
		if (bUnattended)
		{
			bSendLogFile = true;
		}
	}

	if (!GConfig->GetInt(TEXT("CrashReportClient"), TEXT("UserCommentSizeLimit"), UserCommentSizeLimit, GEngineIni))
	{
		UserCommentSizeLimit = 4000;
	}

	FConfigFile EmptyConfigFile;
	SetProjectConfigOverrides(EmptyConfigFile);

	ReadFullCrashDumpConfigurations();
}

void FCrashReportClientConfig::SetAllowToBeContacted( bool bNewValue )
{
	bAllowToBeContacted = bNewValue;
	GConfig->SetBool( *SectionName, TEXT( "bAllowToBeContacted" ), bAllowToBeContacted, GEngineIni );
}

void FCrashReportClientConfig::SetSendLogFile( bool bNewValue )
{
	bSendLogFile = bNewValue;
	GConfig->SetBool( *SectionName, TEXT( "bSendLogFile" ), bSendLogFile, GEngineIni );
}

void FCrashReportClientConfig::SetProjectConfigOverrides(const FConfigFile& InConfigFile)
{
	const FConfigSection* Section = InConfigFile.Find(FGenericCrashContext::ConfigSectionName);

	// Default to false (show the option) when config is missing.
	bHideLogFilesOption = false;

	// Default to true (Allow the user to close without sending) when config is missing.
	bIsAllowedToCloseWithoutSending = true;

	// Try to read values from override config file
	if (Section != nullptr)
	{
		const FConfigValue* HideLogFilesOptionValue = Section->Find(TEXT("bHideLogFilesOption"));
		if (HideLogFilesOptionValue != nullptr)
		{
			bHideLogFilesOption = FCString::ToBool(*HideLogFilesOptionValue->GetValue());
		}

		const FConfigValue* IsAllowedToCloseWithoutSendingValue = Section->Find(TEXT("bIsAllowedToCloseWithoutSending"));
		if (IsAllowedToCloseWithoutSendingValue != nullptr)
		{
			bIsAllowedToCloseWithoutSending = FCString::ToBool(*IsAllowedToCloseWithoutSendingValue->GetValue());
		}
	}
}

const FString FCrashReportClientConfig::GetFullCrashDumpLocationForBranch( const FString& BranchName ) const
{
	for (const auto& It : FullCrashDumpConfigurations)
	{
		const bool bExactMatch = It.bExactMatch;
		const FString IterBranch = It.BranchName.Replace(TEXT("+"), TEXT("/"));
		if (bExactMatch && BranchName == IterBranch)
		{
			return It.Location;
		}
		else if (!bExactMatch && BranchName.Contains(IterBranch))
		{
			return It.Location;
		}
	}

	return TEXT( "" );
}

FString FCrashReportClientConfig::GetKey( const FString& KeyName )
{
	FString Result;
	if (!GConfig->GetString( *SectionName, *KeyName, Result, GEngineIni ))
	{
		return TEXT( "" );
	}
	return Result;
}

void FCrashReportClientConfig::ReadFullCrashDumpConfigurations()
{
	for (int32 NumEntries = 0;; ++NumEntries)
	{
		FString Branch = GetKey( FString::Printf( TEXT( "FullCrashDump_%i_Branch" ), NumEntries ) );
		if (Branch.IsEmpty())
		{
			break;
		}

		const FString NetworkLocation = GetKey( FString::Printf( TEXT( "FullCrashDump_%i_Location" ), NumEntries ) );
		const bool bExactMatch = !Branch.EndsWith( TEXT( "*" ) );
		Branch.ReplaceInline( TEXT( "*" ), TEXT( "" ) );

		FullCrashDumpConfigurations.Add( FFullCrashDumpEntry( Branch, NetworkLocation, bExactMatch ) );

		UE_LOG( CrashReportClientLog, Log, TEXT( "FullCrashDump: %s, NetworkLocation: %s, bExactMatch:%i" ), *Branch, *NetworkLocation, bExactMatch );
	}
}
