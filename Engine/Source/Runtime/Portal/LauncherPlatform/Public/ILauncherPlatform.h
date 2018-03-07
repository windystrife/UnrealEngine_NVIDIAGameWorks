// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

class FOpenLauncherOptions
{
public:
	FOpenLauncherOptions()
		: bInstall(false)
		, bSilent(true)
		, LauncherRelativeUrl()
	{
	}

	FOpenLauncherOptions(FString InLauncherRelativeUrl)
		: bInstall(false)
		, bSilent(true)
		, LauncherRelativeUrl(InLauncherRelativeUrl)
	{
		if ( !LauncherRelativeUrl.IsEmpty() )
		{
			bSilent = false;
		}
	}

	FOpenLauncherOptions(bool DoInstall, FString InLauncherRelativeUrl)
		: bInstall(DoInstall)
		, bSilent(true)
		, LauncherRelativeUrl(InLauncherRelativeUrl)
	{
		if ( !LauncherRelativeUrl.IsEmpty() || bInstall )
		{
			bSilent = false;
		}
	}

	FString GetLauncherUriRequest() const
	{
		FString LauncherUriRequest;
		if ( LauncherRelativeUrl.IsEmpty() )
		{
			LauncherUriRequest = TEXT("com.epicgames.launcher:");
		}
		else
		{
			LauncherUriRequest = FString::Printf(TEXT("com.epicgames.launcher://%s"), *LauncherRelativeUrl);
		}

		// Append silent query string arg.
		if ( bSilent )
		{
			if ( LauncherUriRequest.Contains("?") )
			{
				LauncherUriRequest += TEXT("&silent=true");
			}
			else
			{
				LauncherUriRequest += TEXT("?silent=true");
			}
		}

		return LauncherUriRequest;
	}

public:

	bool bInstall;
	bool bSilent;
	FString LauncherRelativeUrl;
};


class ILauncherPlatform
{
public:
	/** Virtual destructor */
	virtual ~ILauncherPlatform() {}

	/**
	 * Determines whether the launcher can be opened.
	 *
	 * @param Install					Whether to include the possibility of installing the launcher in the check.
	 * @return true if the launcher can be opened (or installed).
	 */
	virtual bool CanOpenLauncher(bool Install) = 0;

	/**
	 * Opens the marketplace user interface.
	 *
	 * @param Install					Whether to install the marketplace if it is missing.
	 * @param LauncherRelativeUrl		A url relative to the launcher which you'd like the launcher to navigate to. Empty defaults to the UE homepage
	 * @param CommandLineParams			Optional command to open the launcher with if it is not already open
	 * @return true if the marketplace was opened, false if it is not installed or could not be installed/opened.
	 */
	virtual bool OpenLauncher(const FOpenLauncherOptions& Options) = 0;
};