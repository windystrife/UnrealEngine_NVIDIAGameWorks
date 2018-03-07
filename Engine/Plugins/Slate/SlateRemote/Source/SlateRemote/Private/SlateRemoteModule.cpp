// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Shared/SlateRemoteSettings.h"
#include "Server/SlateRemoteServer.h"
#include "SlateRemotePrivate.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"


#define LOCTEXT_NAMESPACE "FSlateRemoteModule"


/**
 * Implements the SlateRemoteModule module.
 */
class FSlateRemoteModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
		if (!SupportsSlateRemote())
		{
			return;
		}

		// register settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "SlateRemote",
				LOCTEXT("SlateRemoteSettingsName", "Slate Remote"),
				LOCTEXT("SlateRemoteSettingsDescription", "Configure the Slate Remote plug-in."),
				GetMutableDefault<USlateRemoteSettings>()
			);

			if (SettingsSection.IsValid())
			{
				SettingsSection->OnModified().BindRaw(this, &FSlateRemoteModule::HandleSettingsSaved);
			}
		}

		// register application events
		FCoreDelegates::ApplicationHasReactivatedDelegate.AddRaw(this, &FSlateRemoteModule::HandleApplicationHasReactivated);
		FCoreDelegates::ApplicationWillDeactivateDelegate.AddRaw(this, &FSlateRemoteModule::HandleApplicationWillDeactivate);

		RestartServices();
	}

	virtual void ShutdownModule() override
	{
		// unregister application events
		FCoreDelegates::ApplicationHasReactivatedDelegate.RemoveAll(this);
		FCoreDelegates::ApplicationWillDeactivateDelegate.RemoveAll(this);

		// unregister settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "SlateRemote");
		}

		// shut down services
		ShutdownRemoteServer();
	}

	virtual bool SupportsDynamicReloading() override
	{
		return true;
	}

protected:

	/** Initializes the Slate Remote server with the current settings. */
	void InitializeRemoteServer()
	{
		ShutdownRemoteServer();

		USlateRemoteSettings* Settings = GetMutableDefault<USlateRemoteSettings>();

		// load settings
		bool ResaveSettings = false;

		FIPv4Endpoint ServerEndpoint;

		if (GIsEditor)
		{
			if (!FIPv4Endpoint::Parse(Settings->EditorServerEndpoint, ServerEndpoint))
			{
				if (!Settings->EditorServerEndpoint.IsEmpty())
				{
					GLog->Logf(TEXT("Warning: Invalid Slate Remote EditorServerEndpoint '%s' - binding to all local network adapters instead"), *Settings->EditorServerEndpoint);
				}

				ServerEndpoint = SLATE_REMOTE_SERVER_DEFAULT_EDITOR_ENDPOINT;
				Settings->EditorServerEndpoint = ServerEndpoint.ToText().ToString();
				ResaveSettings = true;
			}
		}
		else
		{
			if (!FIPv4Endpoint::Parse(Settings->GameServerEndpoint, ServerEndpoint))
			{
				if (!Settings->GameServerEndpoint.IsEmpty())
				{
					GLog->Logf(TEXT("Warning: Invalid Slate Remote GameServerEndpoint '%s' - binding to all local network adapters instead"), *Settings->GameServerEndpoint);
				}

				ServerEndpoint = SLATE_REMOTE_SERVER_DEFAULT_GAME_ENDPOINT;
				Settings->GameServerEndpoint = ServerEndpoint.ToText().ToString();
				ResaveSettings = true;
			}
		}

		if (ResaveSettings)
		{
			Settings->SaveConfig();
		}

		// create server
		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

		if (SocketSubsystem != nullptr)
		{
			RemoteServer = MakeShareable(new FSlateRemoteServer(*SocketSubsystem, ServerEndpoint));
		}
		else
		{
			GLog->Logf(TEXT("Error: SlateRemote: Failed to acquire socket subsystem."));
		}
	}

	/** Restarts the services that this modules provides. */
	void RestartServices()
	{
		const USlateRemoteSettings& Settings = *GetDefault<USlateRemoteSettings>();

		if (Settings.EnableRemoteServer)
		{
			if (!RemoteServer.IsValid())
			{
				InitializeRemoteServer();
			}
		}
		else
		{
			ShutdownRemoteServer();
		}
	}

	/** Shuts down the Slate Remote server. */
	void ShutdownRemoteServer()
	{
		RemoteServer.Reset();
	}

	/**
	 * Checks whether the Slate Remote server is supported.
	 *
	 * @return true if networked transport is supported, false otherwise.
	 */
	bool SupportsSlateRemote() const
	{
		// disallow for commandlets
		if (IsRunningCommandlet())
		{
			return false;
		}

		return true;
	}

private:

	/** Callback for when an has been reactivated (i.e. return from sleep on iOS). */
	void HandleApplicationHasReactivated()
	{
		RestartServices();
	}

	/** Callback for when the application will be deactivated (i.e. sleep on iOS). */
	void HandleApplicationWillDeactivate()
	{
		ShutdownRemoteServer();
	}

	/** Callback for when the settings were saved. */
	bool HandleSettingsSaved()
	{
		RestartServices();

		return true;
	}

private:

	/** Holds the Slate Remote server. */
	TSharedPtr<FSlateRemoteServer> RemoteServer;
};


IMPLEMENT_MODULE(FSlateRemoteModule, SlateRemote);


#undef LOCTEXT_NAMESPACE
