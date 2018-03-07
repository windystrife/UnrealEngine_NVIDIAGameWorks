// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PerforceSourceControlSettings.h"
#include "Misc/ScopeLock.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "SPerforceSourceControlSettings.h"
#include "PerforceSourceControlModule.h"

namespace PerforceSettingsConstants
{

/** The section of the ini file we load our settings from */
static const FString SettingsSection = TEXT("PerforceSourceControl.PerforceSourceControlSettings");

}


const FString& FPerforceSourceControlSettings::GetPort() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return ConnectionInfo.Port;
}

void FPerforceSourceControlSettings::SetPort(const FString& InString)
{
	FScopeLock ScopeLock(&CriticalSection);
	ConnectionInfo.Port = InString;
}

const FString& FPerforceSourceControlSettings::GetUserName() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return ConnectionInfo.UserName;
}

void FPerforceSourceControlSettings::SetUserName(const FString& InString)
{
	FScopeLock ScopeLock(&CriticalSection);
	ConnectionInfo.UserName = InString;
}

const FString& FPerforceSourceControlSettings::GetWorkspace() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return ConnectionInfo.Workspace;
}

void FPerforceSourceControlSettings::SetWorkspace(const FString& InString)
{
	FScopeLock ScopeLock(&CriticalSection);
	ConnectionInfo.Workspace = InString;
}

const FString& FPerforceSourceControlSettings::GetHostOverride() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return ConnectionInfo.HostOverride;
}

void FPerforceSourceControlSettings::SetHostOverride(const FString& InString)
{
	FScopeLock ScopeLock(&CriticalSection);
	ConnectionInfo.HostOverride = InString;
}

const FString& FPerforceSourceControlSettings::GetChangelistNumber() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return ConnectionInfo.ChangelistNumber;
}

void FPerforceSourceControlSettings::SetChangelistNumber(const FString& InString)
{
	FScopeLock ScopeLock(&CriticalSection);
	ConnectionInfo.ChangelistNumber = InString;
}

void FPerforceSourceControlSettings::LoadSettings()
{
	FScopeLock ScopeLock(&CriticalSection);
	const FString& IniFile = SourceControlHelpers::GetSettingsIni();

	if(!GConfig->GetString(*PerforceSettingsConstants::SettingsSection, TEXT("Port"), ConnectionInfo.Port, IniFile))
	{
		// backwards compatibility - previously we mis-specified the Port as 'Host'
		GConfig->GetString(*PerforceSettingsConstants::SettingsSection, TEXT("Host"), ConnectionInfo.Port, IniFile);
	}
	GConfig->GetString(*PerforceSettingsConstants::SettingsSection, TEXT("UserName"), ConnectionInfo.UserName, IniFile);
	GConfig->GetString(*PerforceSettingsConstants::SettingsSection, TEXT("Workspace"), ConnectionInfo.Workspace, IniFile);
	GConfig->GetString(*PerforceSettingsConstants::SettingsSection, TEXT("HostOverride"), ConnectionInfo.HostOverride, IniFile);
}

void FPerforceSourceControlSettings::SaveSettings() const
{
	FScopeLock ScopeLock(&CriticalSection);
	const FString& IniFile = SourceControlHelpers::GetSettingsIni();
	GConfig->SetString(*PerforceSettingsConstants::SettingsSection, TEXT("Port"), *ConnectionInfo.Port, IniFile);
	GConfig->SetString(*PerforceSettingsConstants::SettingsSection, TEXT("UserName"), *ConnectionInfo.UserName, IniFile);
	GConfig->SetString(*PerforceSettingsConstants::SettingsSection, TEXT("Workspace"), *ConnectionInfo.Workspace, IniFile);
	GConfig->SetString(*PerforceSettingsConstants::SettingsSection, TEXT("HostOverride"), *ConnectionInfo.HostOverride, IniFile);
}

FPerforceConnectionInfo FPerforceSourceControlSettings::GetConnectionInfo() const
{
	check(IsInGameThread());
	FPerforceConnectionInfo OutConnectionInfo = ConnectionInfo;
	
	// password needs to be gotten straight from the input UI, its not stored anywhere else
	if(SPerforceSourceControlSettings::GetPassword().Len() > 0)
	{
		OutConnectionInfo.Password = SPerforceSourceControlSettings::GetPassword();
	}

	// Ticket is stored in the provider
	FPerforceSourceControlModule& PerforceSourceControl = FModuleManager::GetModuleChecked<FPerforceSourceControlModule>( "PerforceSourceControl" );
	FPerforceSourceControlProvider& Provider = PerforceSourceControl.GetProvider();
	OutConnectionInfo.Ticket = Provider.GetTicket();

	return OutConnectionInfo;
}
