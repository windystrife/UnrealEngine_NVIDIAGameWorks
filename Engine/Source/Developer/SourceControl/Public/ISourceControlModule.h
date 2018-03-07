// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class ISourceControlProvider;

SOURCECONTROL_API DECLARE_LOG_CATEGORY_EXTERN(LogSourceControl, Log, All);

/** Delegate called when the source control login window is closed. Parameter determines if source control is enabled or not */
DECLARE_DELEGATE_OneParam( FSourceControlLoginClosed, bool );

/** Delegate called when the active source control provider is changed */
DECLARE_MULTICAST_DELEGATE_TwoParams( FSourceControlProviderChanged, ISourceControlProvider& /*OldProvider*/, ISourceControlProvider& /*NewProvider*/ );

/**
 * The modality of the login window.
 */
namespace ELoginWindowMode
{
	enum Type
	{
		Modal,
		Modeless
	};
};


/**
 * Login window startup behavior
 */
namespace EOnLoginWindowStartup
{
	enum Type
	{
		ResetProviderToNone,
		PreserveProvider
	};
};


/**
 * Interface for talking to source control providers
 */
class ISourceControlModule : public IModuleInterface
{
public:

	/**
	 * Returns a list of the registered source control providers
	 */
	virtual void GetProviderNames(TArray<FName>& OutProviderNames) = 0;

	/**
	 * Tick the source control module.
	 * This is responsible for dispatching batched/queued status requests & for calling ISourceControlProvider::Tick()
	 */
	virtual void Tick() = 0;

	/**
	 * Queues a file to have its source control status updated in the background.
	 * @param	InPackages	The packages to queue.
	 */
	virtual void QueueStatusUpdate(const TArray<UPackage*>& InPackages) = 0;

	/**
	 * Queues a file to have its source control status updated in the background.
	 * @param	InFilenames	The files to queue.
	 */
	virtual void QueueStatusUpdate(const TArray<FString>& InFilenames) = 0;

	/**
	 * Queues a file to have its source control status updated in the background.
	 * @param	InPackage	The package to queue.
	 */
	virtual void QueueStatusUpdate(UPackage* InPackage) = 0;

	/**
	 * Queues a file to have its source control status updated in the background.
	 * @param	InFilename	The file to queue.
	 */
	virtual void QueueStatusUpdate(const FString& InFilename) = 0;

	/**
	 * Check whether source control is enabled.
	 */
	virtual bool IsEnabled() const = 0;

	/**
	 * Get the source control provider that is currently in use.
	 */
	virtual ISourceControlProvider& GetProvider() const = 0;

	/**
	 * Set the current source control provider to the one specified here by name.
	 * This will assert if the provider does not exist.
	 * @param	InName	The name of the provider
	 */
	virtual void SetProvider( const FName& InName ) = 0;

	/**
	 * Show the source control login dialog
	 * @param	InOnWindowClosed		Delegate to be called when the login window is closed.
	 * @param	InLoginWindowMode		Whether the dialog should be presented modally or not. Note that this function blocks if the modality is Modal.
	 * @param	InOnLoginWindowStartup	Whether the provider should be set to 'None' on dialog startup
	 */
	virtual void ShowLoginDialog(const FSourceControlLoginClosed& InOnSourceControlLoginClosed, ELoginWindowMode::Type InLoginWindowMode, EOnLoginWindowStartup::Type InOnLoginWindowStartup = EOnLoginWindowStartup::ResetProviderToNone) = 0;

	/**
	 * Get whether we should use global or per-project settings
	 * @return true if we should use global settings
	 */
	virtual bool GetUseGlobalSettings() const = 0;

	/**
	 * Set whether we should use global or per-project settings
	 * @param bIsUseGlobalSettings	Whether we should use global settings
	 */
	virtual void SetUseGlobalSettings(bool bIsUseGlobalSettings) = 0;

	/**
	 * Register a delegate to be called when the source control provider changes
	 */
	virtual FDelegateHandle RegisterProviderChanged(const FSourceControlProviderChanged::FDelegate& SourceControlProviderChanged) = 0;

	/**
	 * Unregister a delegate to be called when the source control provider changes
	 */
	virtual void UnregisterProviderChanged(FDelegateHandle Handle) = 0;

	/**
	 * Gets a reference to the source control module instance.
	 *
	 * @return A reference to the source control module.
	 */
	static inline ISourceControlModule& Get()
	{
		static FName SourceControlModule("SourceControl");
		return FModuleManager::LoadModuleChecked<ISourceControlModule>(SourceControlModule);
	}
};
