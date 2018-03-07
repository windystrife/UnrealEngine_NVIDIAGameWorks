// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PerforceConnectionInfo.h"

class FPerforceSourceControlSettings
{
public:
	/** Get the Perforce port */
	const FString& GetPort() const;

	/** Set the Perforce port */
	void SetPort(const FString& InString);

	/** Get the Perforce username */
	const FString& GetUserName() const;

	/** Set the Perforce username */
	void SetUserName(const FString& InString);

	/** Get the Perforce workspace */
	const FString& GetWorkspace() const;

	/** Set the Perforce workspace */
	void SetWorkspace(const FString& InString);

	/** Get the Perforce host override */
	const FString& GetHostOverride() const;

	/** Set the Perforce host override */
	void SetHostOverride(const FString& InString);

	/** Get the perforce cl we should use for this run (useful in commandlets) returns empty string if there is no cl*/
	const FString& GetChangelistNumber() const;

	void SetChangelistNumber(const FString& InString);

	/** Load settings from ini file */
	void LoadSettings();

	/** Save settings to ini file */
	void SaveSettings() const;

	/** Get the credentials we use to access the server - only call on the game thread */
	FPerforceConnectionInfo GetConnectionInfo() const;

private:
	/** A critical section for settings access */
	mutable FCriticalSection CriticalSection;

	/** The credentials we use to access the server */
	FPerforceConnectionInfo ConnectionInfo;
};
