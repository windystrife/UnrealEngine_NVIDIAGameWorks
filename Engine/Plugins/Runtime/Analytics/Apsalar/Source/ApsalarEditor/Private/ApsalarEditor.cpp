// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ApsalarEditor.h"

#include "Modules/ModuleManager.h"
#include "ApsalarSettings.h"
#include "Analytics.h"

IMPLEMENT_MODULE( FApsalarEditorModule, ApsalarEditorModule );

#define LOCTEXT_NAMESPACE "Apsalar"

void FApsalarEditorModule::StartupModule()
{
}

void FApsalarEditorModule::ShutdownModule()
{
}

UApsalarSettings::UApsalarSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SettingsDisplayName = LOCTEXT("SettingsDisplayName", "Apsalar");
	SettingsTooltip = LOCTEXT("SettingsTooltip", "Apsalar configuration settings");
}

void UApsalarSettings::ReadConfigSettings()
{
	ReadConfigStruct(GetReleaseIniSection(), Release);
	ReadConfigStruct(GetTestIniSection(), Test, &Release);
	ReadConfigStruct(GetDebugIniSection(), Debug, &Release);
	ReadConfigStruct(GetDevelopmentIniSection(), Development, &Release);
}

void UApsalarSettings::WriteConfigSettings()
{
	WriteConfigStruct(GetReleaseIniSection(), Release);
	WriteConfigStruct(GetTestIniSection(), Test);
	WriteConfigStruct(GetDebugIniSection(), Debug);
	WriteConfigStruct(GetDevelopmentIniSection(), Development);
}

void UApsalarSettings::ReadConfigStruct(const FString& Section, FApsalarAnalyticsConfigSetting& Dest, FApsalarAnalyticsConfigSetting* Default)
{
	const FString Key = FAnalytics::Get().GetConfigValueFromIni(GetIniName(), Section, TEXT("ApiKey"), true);
	if (Key.Len() == 0 && Default != nullptr)
	{
		Dest.ApiKey = Default->ApiKey;
	}
	else
	{
		Dest.ApiKey = Key;
	}
	const FString Secret = FAnalytics::Get().GetConfigValueFromIni(GetIniName(), Section, TEXT("ApiSecret"), true);
	if (Secret.Len() == 0 && Default != nullptr)
	{
		Dest.ApiSecret = Default->ApiSecret;
	}
	else
	{
		Dest.ApiSecret = Secret;
	}
	const FString SendInterval = FAnalytics::Get().GetConfigValueFromIni(GetIniName(), Section, TEXT("SendInterval"), false);
	if (SendInterval.Len() == 0 && Default != nullptr)
	{
		Dest.SendInterval = Default->SendInterval;
	}
	else
	{
		Dest.SendInterval = FCString::Atoi(*SendInterval);
	}
	const FString MaxBufferSize = FAnalytics::Get().GetConfigValueFromIni(GetIniName(), Section, TEXT("MaxBufferSize"), false);
	if (MaxBufferSize.Len() == 0 && Default != nullptr)
	{
		Dest.MaxBufferSize = Default->MaxBufferSize;
	}
	else
	{
		Dest.MaxBufferSize = FCString::Atoi(*MaxBufferSize);
	}
	const FString ManuallyReportRevenue = FAnalytics::Get().GetConfigValueFromIni(GetIniName(), Section, TEXT("ManuallyReportRevenue"), false);
	if (ManuallyReportRevenue.Len() == 0 && Default != nullptr)
	{
		Dest.ManuallyReportRevenue = Default->ManuallyReportRevenue;
	}
	else
	{
		Dest.ManuallyReportRevenue = ManuallyReportRevenue.Compare(TEXT("true"), ESearchCase::IgnoreCase) == 0;
	}
}

void UApsalarSettings::WriteConfigStruct(const FString& Section, FApsalarAnalyticsConfigSetting& Source)
{
	FAnalytics::Get().WriteConfigValueToIni(GetIniName(), Section, TEXT("ApiKey"), Source.ApiKey);
	FAnalytics::Get().WriteConfigValueToIni(GetIniName(), Section, TEXT("ApiSecret"), Source.ApiSecret);
	FAnalytics::Get().WriteConfigValueToIni(GetIniName(), Section, TEXT("SendInterval"), FString::Printf(TEXT("%d"), Source.SendInterval));
	FAnalytics::Get().WriteConfigValueToIni(GetIniName(), Section, TEXT("MaxBufferSize"), FString::Printf(TEXT("%d"), Source.MaxBufferSize));
	FAnalytics::Get().WriteConfigValueToIni(GetIniName(), Section, TEXT("ManuallyReportRevenue"), Source.ManuallyReportRevenue ? TEXT("true") : TEXT("false"));
}

#undef LOCTEXT_NAMESPACE
